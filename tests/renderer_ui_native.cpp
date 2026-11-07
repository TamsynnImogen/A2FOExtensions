// Pixel comparisons against the real D3D8 provider, in a private headless host.
// No game entry point, visible window, activation or input automation.
#include "renderer_ui_scene.hpp"
#include "../renderer/textures/upload.hpp"
#include <d3d11.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

using namespace a2fo::renderer;
using renderer_fixture::Scene;
namespace {
void require(bool condition,const char* message) {
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::fflush(stderr);std::exit(1);}
}
void checked(HRESULT hr,const char* message) {
    if(FAILED(hr)){std::fprintf(stderr,"FAIL: %s HRESULT=0x%08lx\n",message,static_cast<unsigned long>(hr));std::exit(1);}
}
template<class T> struct Owned {
    T* p=nullptr;
    ~Owned(){if(p)p->Release();}
    T** out(){return &p;}
    T* operator->(){return p;}
};
template<class T,std::size_t N> void vertices(Scene& s,const std::array<T,N>& v) {
    s.vertices.resize(sizeof(v));std::memcpy(s.vertices.data(),v.data(),sizeof(v));
    s.stride=sizeof(T);s.vertex_count=N;
}
struct ColorVertex {float x,y,z,rhw;DWORD color;};
struct TextureVertex {float x,y,z,rhw;DWORD color;float u,v;};
struct SpecularVertex {float x,y,z,rhw;DWORD color,specular;float u,v;};
static_assert(sizeof(ColorVertex)==20 && sizeof(TextureVertex)==28 && sizeof(SpecularVertex)==32);
Scene solid(DWORD color=0xff20c080) {
    Scene s;s.state.color_op=1;s.state.alpha_op=1;
    const std::array<ColorVertex,4> quad{{{4.5f,6.5f,.4f,1,color},{28.5f,6.5f,.4f,1,color},
        {28.5f,30.5f,.4f,1,color},{4.5f,30.5f,.4f,1,color}}};
    vertices(s,quad);return s;
}
Scene textured() {
    Scene s;s.fvf=324;s.texture_width=s.texture_height=2;s.texture_pitch=8;
    const DWORD pixels[]={0xffe03010,0xff2080c0,0x4010d040,0xff5020d0};
    s.texture.resize(sizeof(pixels));std::memcpy(s.texture.data(),pixels,sizeof(pixels));
    const std::array<TextureVertex,4> quad{{{3.5f,5.5f,.5f,1,0xff80c0ff,0,0},
        {35.5f,5.5f,.5f,1,0xff80c0ff,1,0},{35.5f,37.5f,.5f,1,0xff80c0ff,1,1},
        {3.5f,37.5f,.5f,1,0xff80c0ff,0,1}}};
    vertices(s,quad);return s;
}
void log(const std::string& line){std::printf("%s\n",line.c_str());}
void dump(const std::string& path,const std::vector<std::uint8_t>& rgba) {
    FILE* file=std::fopen(path.c_str(),"wb");if(!file)return;
    std::fprintf(file,"P6\n64 64\n255\n");
    for(std::size_t p=0;p<rgba.size();p+=4)std::fwrite(rgba.data()+p,1,3,file);
    std::fclose(file);
}
struct ThreadRequest {UiRenderer* ui;ID3D11Device* device;ID3D11DeviceContext* context;HRESULT result=E_FAIL;};
DWORD WINAPI wrong_thread_initialize(void* pointer) {
    auto& request=*static_cast<ThreadRequest*>(pointer);
    request.result=request.ui->initialize(request.device,request.context,{});
    return 0;
}
}
int run_fixture(int argc,char** argv) {
    require(argc==2,"private output directory argument");
    const std::string output=argv[1];
    char current[32768]{};require(GetCurrentDirectoryA(sizeof(current),current)!=0,"private current directory");
    const std::string cache=std::string(current)+"\\ui-shader-cache";
    require(CreateDirectoryA(cache.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"private shader cache directory");
    WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
    cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="A2FOUiPixelFixture";
    require(RegisterClassExA(&cls)!=0,"register private window");
    HWND window=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,
        "private renderer comparison",WS_POPUP,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    require(window && !IsWindowVisible(window),"hidden nonactivating reference window");
    HMODULE library=LoadLibraryA("d3d11.dll");require(library!=nullptr,"load D3D11 dynamically");
    using Create=HRESULT (WINAPI*)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
    Create create=nullptr;const auto address=GetProcAddress(library,"D3D11CreateDevice");
    std::memcpy(&create,&address,sizeof(create));require(create!=nullptr,"resolve D3D11CreateDevice");
    Owned<ID3D11Device> device;Owned<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL wanted=D3D_FEATURE_LEVEL_11_0;D3D_FEATURE_LEVEL obtained{};
    const HRESULT created=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&wanted,1,D3D11_SDK_VERSION,device.out(),&obtained,context.out());
    if(FAILED(created)){std::printf("SKIP: D3D11 hardware device HRESULT=0x%08lx\n",static_cast<unsigned long>(created));return 77;}
    require(obtained>=wanted,"SM5 hardware feature level");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=64;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    Owned<ID3D11Texture2D> target,staging,depth;Owned<ID3D11RenderTargetView> rtv;Owned<ID3D11DepthStencilView> dsv;
    checked(device->CreateTexture2D(&desc,nullptr,target.out()),"offscreen target");
    checked(device->CreateRenderTargetView(target.p,nullptr,rtv.out()),"offscreen RTV");
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    checked(device->CreateTexture2D(&desc,nullptr,staging.out()),"readback staging");
    desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    checked(device->CreateTexture2D(&desc,nullptr,depth.out()),"depth target");
    checked(device->CreateDepthStencilView(depth.p,nullptr,dsv.out()),"depth view");
    UiRenderer ui;checked(ui.initialize(device.p,context.p,cache,log),"UI backend initialize");
    checked(ui.set_target(rtv.p,dsv.p,64,64),"UI backend target");
    ThreadRequest thread_request{&ui,device.p,context.p};
    HANDLE thread=CreateThread(nullptr,0,wrong_thread_initialize,&thread_request,0,nullptr);
    require(thread!=nullptr,"wrong-thread fixture");
    require(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"wrong-thread fixture completion");CloseHandle(thread);
    require(FAILED(thread_request.result),"wrong-thread initialize rejects without destroying active owner backend");
    std::vector<std::pair<std::string,Scene>> cases;
    cases.push_back({"diffuse-fan",solid()});
    auto indexed=solid(0xffd04090);const std::uint16_t indices[]={0,1,2,3};
    indexed.indices.resize(sizeof(indices));std::memcpy(indexed.indices.data(),indices,sizeof(indices));
    cases.push_back({"indexed-fan",indexed});
    auto blend=solid(0x8000ff00);blend.state.alpha_blend=true;cases.push_back({"alpha-blend",blend});
    auto texture=textured();cases.push_back({"texture-point",texture});
    auto selected=texture;selected.state.color_op=selected.state.alpha_op=2;
    cases.push_back({"texture-select",selected});
    auto added=texture;added.state.color_op=added.state.alpha_op=7;added.state.alpha_blend=true;
    cases.push_back({"texture-add-blend",added});
    auto luminance=texture;luminance.texture_format=50;luminance.texture_pitch=2;luminance.texture={0,63,128,255};
    cases.push_back({"texture-luminance",luminance});
    auto rgb565=texture;rgb565.texture_format=23;rgb565.texture_pitch=4;
    const std::uint16_t packed565[]={0xf800,0x07e0,0x001f,0x18e3};
    rgb565.texture.resize(sizeof(packed565));std::memcpy(rgb565.texture.data(),packed565,sizeof(packed565));
    cases.push_back({"texture-rgb565",rgb565});
    for(const auto format:std::array<UINT,3>{0x31545844u,0x33545844u,0x35545844u}) {
        auto compressed=texture;compressed.texture_format=format;compressed.texture_width=compressed.texture_height=4;
        compressed.texture_pitch=format==0x31545844u ? 8 : 16;compressed.texture.assign(compressed.texture_pitch,0);
        const UINT color_offset=format==0x31545844u ? 0 : 8;
        compressed.texture[color_offset+1]=0xf8;compressed.texture[color_offset+2]=0x1f;
        if(format==0x33545844u)std::fill_n(compressed.texture.begin(),8,0xff);
        if(format==0x35545844u)compressed.texture[0]=0xff;
        cases.push_back({format==0x31545844u ? "texture-bc1" : format==0x33545844u ? "texture-bc2" : "texture-bc3",compressed});
    }
    texture.state.alpha_test=true;texture.state.alpha_func=5;texture.state.alpha_ref=128;
    cases.push_back({"texture-alpha-test",texture});
    texture.state.alpha_test=false;texture.state.min_filter=texture.state.mag_filter=2;
    cases.push_back({"texture-linear",texture});
    auto clipped=solid();clipped.primitive_count=0;clipped.vertices.clear();
    clipped.viewport={12,9,32,40,0,1};clipped.clear_rectangles={{0,0,20,17},{39,45,64,64}};
    cases.push_back({"viewport-clear-rectangles",clipped});
    auto subviewport=solid();subviewport.viewport={12,9,32,40,0,1};cases.push_back({"viewport-draw",subviewport});
    auto spec=textured();spec.fvf=452;spec.state.specular_enable=true;
    const std::array<SpecularVertex,4> spec_vertices{{{3.5f,5.5f,.5f,1,0xff405080,0x00201008,0,0},
        {35.5f,5.5f,.5f,1,0xff405080,0x00201008,1,0},{35.5f,37.5f,.5f,1,0xff405080,0x00201008,1,1},
        {3.5f,37.5f,.5f,1,0xff405080,0x00201008,0,1}}};
    vertices(spec,spec_vertices);cases.push_back({"texture-specular",spec});
    auto masked=solid();masked.state.color_write_mask=5;cases.push_back({"colour-mask",masked});
    auto culled=solid();culled.state.cull=2;cases.push_back({"cull-clockwise",culled});
    culled.state.cull=3;cases.push_back({"cull-counterclockwise",culled});
    for(const auto& item:cases) {
        const auto& s=item.second;
        std::vector<std::uint8_t> expected,actual(64*64*4);std::string error;
        const HRESULT reference=renderer_fixture::render_reference(window,s,expected,error);
        if(FAILED(reference)){std::fprintf(stderr,"FAIL: %s reference %s hr=0x%08lx\n",item.first.c_str(),error.c_str(),static_cast<unsigned long>(reference));return 1;}
        checked(ui.set_viewport({0,0,64,64,0,1}),"full clear viewport");
        checked(ui.clear_color(s.background),"full D3D8-style Clear");
        context->ClearDepthStencilView(dsv.p,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
        checked(ui.set_viewport(s.viewport),"scene viewport");
        if(!s.clear_rectangles.empty())checked(ui.clear_color(s.clear_color,static_cast<UINT>(s.clear_rectangles.size()),s.clear_rectangles.data()),"viewport-clipped Clear rectangles");
        Owned<ID3D11ShaderResourceView> texture_view;
        if(!s.texture.empty()) {
            compat::TextureUpload upload;
            checked(compat::prepare_texture_upload(s.texture_format,s.texture_width,s.texture_height,s.texture.data(),s.texture.size(),s.texture_pitch,upload),"translate D3D8 texture pixels");
            checked(ui.upload_texture(upload,texture_view.out()),"D3D11 texture upload");
        }
        checked(ui.set_texture(texture_view.p),"bind translated texture");
        if(s.primitive_count) {
            if(s.indices.empty())checked(ui.draw_up(s.primitive,s.primitive_count,s.fvf,s.vertices.data(),s.vertices.size(),s.stride,s.state),item.first.c_str());
            else checked(ui.draw_indexed_up(s.primitive,s.primitive_count,s.fvf,s.vertices.data(),s.vertices.size(),s.vertex_count,s.stride,s.indices.data(),s.indices.size(),s.index_format,s.state),item.first.c_str());
        }
        context->CopyResource(staging.p,target.p);
        D3D11_MAPPED_SUBRESOURCE mapped{};checked(context->Map(staging.p,0,D3D11_MAP_READ,0,&mapped),"read D3D11 pixels");
        for(UINT y=0;y<64;++y)std::memcpy(actual.data()+y*64*4,static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch,64*4);
        context->Unmap(staging.p,0);
        unsigned differences=0,max_difference=0;
        for(UINT p=0;p<64*64;++p)for(UINT channel=0;channel<3;++channel) {
            const auto difference=static_cast<unsigned>(std::abs(int(actual[p*4+channel])-int(expected[p*4+channel])));
            max_difference=std::max(max_difference,difference);
            if(difference>1) {
                if(differences<5)std::fprintf(stderr,"MISMATCH %s (%u,%u) channel=%u legacy=%u modern=%u\n",item.first.c_str(),p%64,p/64,channel,expected[p*4+channel],actual[p*4+channel]);
                ++differences;
            }
        }
        dump(output+"/"+item.first+"-d3d8.ppm",expected);dump(output+"/"+item.first+"-d3d11.ppm",actual);
        std::printf("PIXELS %s differences=%u max_channel_delta=%u\n",item.first.c_str(),differences,max_difference);std::fflush(stdout);
        require(differences==0,"D3D8/D3D11 pixel comparison within one colour level");
    }
    auto invalid=solid();invalid.fvf=274;
    require(FAILED(ui.draw_up(invalid.primitive,invalid.primitive_count,invalid.fvf,invalid.vertices.data(),invalid.vertices.size(),invalid.stride,invalid.state)),"3D FVF explicitly unsupported by UI slice");
    invalid=solid();invalid.state.color_op=24;
    require(FAILED(ui.draw_up(invalid.primitive,invalid.primitive_count,invalid.fvf,invalid.vertices.data(),invalid.vertices.size(),invalid.stride,invalid.state)),"unsupported combiner rejected");
    invalid=solid();
    require(FAILED(ui.draw_up(invalid.primitive,invalid.primitive_count,invalid.fvf,invalid.vertices.data(),invalid.vertices.size(),4096,invalid.state)),"unsupported vertex stride rejected before upload");
    Owned<ID3D11Texture2D> readable_depth;
    Owned<ID3D11DepthStencilView> readable_dsv;
    Owned<ID3D11ShaderResourceView> depth_srv;
    desc.Format=DXGI_FORMAT_R24G8_TYPELESS;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    checked(device->CreateTexture2D(&desc,nullptr,readable_depth.out()),"shader-readable depth texture");
    D3D11_DEPTH_STENCIL_VIEW_DESC depth_view{};depth_view.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_view.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    checked(device->CreateDepthStencilView(readable_depth.p,&depth_view,readable_dsv.out()),"shader-readable depth DSV");
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_view{};srv_view.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srv_view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv_view.Texture2D.MipLevels=1;
    checked(device->CreateShaderResourceView(readable_depth.p,&srv_view,depth_srv.out()),"depth SRV");
    checked(ui.set_target(rtv.p,readable_dsv.p,64,64),"alias test target");
    checked(ui.set_texture(depth_srv.p),"alias test texture");
    invalid=textured();
    require(FAILED(ui.draw_up(invalid.primitive,invalid.primitive_count,invalid.fvf,invalid.vertices.data(),invalid.vertices.size(),invalid.stride,invalid.state)),"depth and texture resource alias rejected");
    ui.shutdown();
    DestroyWindow(window);UnregisterClassA(cls.lpszClassName,cls.hInstance);
    return 0;
}
int main(int argc,char** argv) {
    const int status=run_fixture(argc,argv);
    // Publish success only after every retained COM resource is released.
    if(!status)std::puts("PASS: D3D8 and D3D11 UI pixels");
    return status;
}
