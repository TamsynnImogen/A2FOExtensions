// Private hardware D3D11 fixture: no game, swap chain, visible window or input.
#include "renderer_bc_blocks.hpp"
#include "../renderer/d3d11/compressed_texture.hpp"
#include "../renderer/d3d11/ui_renderer.hpp"
#include "../renderer/shaders/bc_normals.hpp"
#include "../renderer/shaders/bytecode_cache.hpp"
#include <d3d11.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace a2fo::renderer;
extern "C" HRESULT exercise_compressed_texture8(HWND,ID3D11Device*,
    const a2fo::renderer::textures::CompressedImage&,ID3D11ShaderResourceView**);
using a2fo::renderer::textures::CompressedImage;
using a2fo::renderer::textures::parse_compressed_dds;
namespace {
template<class T> struct Owned {
    T* p=nullptr;
    ~Owned(){if(p)p->Release();}
    T** out(){return &p;}
    T* operator->(){return p;}
};
void require(bool ok,const char* what){if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
void checked(HRESULT hr,const char* what){if(FAILED(hr)){std::fprintf(stderr,"FAIL: %s hr=0x%08lx\n",what,static_cast<unsigned long>(hr));std::exit(1);}}
void checked_upload(HRESULT hr,const std::string& error,const char* what){
    if(FAILED(hr)){std::fprintf(stderr,"FAIL: %s hr=0x%08lx detail=%s\n",what,
                               static_cast<unsigned long>(hr),error.c_str());std::exit(1);}
}
std::vector<std::uint8_t> read_file(const char* path){
    FILE* file=std::fopen(path,"rb");require(file!=nullptr,"open external DDS");
    require(std::fseek(file,0,SEEK_END)==0,"seek external DDS end");
    const long length=std::ftell(file);
    require(length>0 && length<=512L*1024L*1024L,"external DDS size");
    require(std::fseek(file,0,SEEK_SET)==0,"seek external DDS start");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    require(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size(),"read external DDS");
    require(std::fclose(file)==0,"close external DDS");
    return bytes;
}
unsigned difference(unsigned actual,unsigned expected){return unsigned(std::abs(int(actual)-int(expected)));}
float srgb(float value){return value<=0.04045f?value/12.92f:std::pow((value+0.055f)/1.055f,2.4f);}
std::array<float,4> expected(DXGI_FORMAT format,unsigned mip,unsigned x,unsigned y,bool normal) {
    const unsigned block=mip==0?(y/4)*2+x/4:0;
    const bool signed_format=format==DXGI_FORMAT_BC5_SNORM;
    if(format==DXGI_FORMAT_BC7_UNORM || format==DXGI_FORMAT_BC7_UNORM_SRGB) {
        auto c=bc_fixture::bc7_colour(mip,block);
        float r=c.r/255.f,g=c.g/255.f,b=c.b/255.f;
        if(format==DXGI_FORMAT_BC7_UNORM_SRGB){r=srgb(r);g=srgb(g);b=srgb(b);}
        return {r,g,b,c.a/255.f};
    }
    auto c=bc_fixture::bc5_colour(signed_format,mip,block);
    float r=signed_format?c[0]/127.f:c[0]/255.f;
    float g=signed_format?c[1]/127.f:c[1]/255.f;
    if(normal){
        const float nx=signed_format?r:r*2.f-1.f,ny=signed_format?g:g*2.f-1.f;
        const float nz=std::sqrt(std::max(0.f,1.f-nx*nx-ny*ny));
        const float inv=1.f/std::sqrt(nx*nx+ny*ny+nz*nz);
        return {nx*inv*.5f+.5f,ny*inv*.5f+.5f,nz*inv*.5f+.5f,1.f};
    }
    return {signed_format?r*.5f+.5f:r,signed_format?g*.5f+.5f:g,0.f,1.f};
}
const char* format_name(DXGI_FORMAT f){switch(f){
    case DXGI_FORMAT_BC5_UNORM:return "BC5_UNORM";
    case DXGI_FORMAT_BC5_SNORM:return "BC5_SNORM";
    case DXGI_FORMAT_BC7_UNORM:return "BC7_UNORM";
    case DXGI_FORMAT_BC7_UNORM_SRGB:return "BC7_UNORM_SRGB";
    default:return "unexpected";
}}
std::string pixel_source(DXGI_FORMAT format,bool normal){
    std::string src=R"(
struct Varying { float4 position:SV_Position; float2 uv:TEXCOORD0; };
Texture2D image:register(t0); SamplerState point_sampler:register(s0);
cbuffer Mip:register(b0) { uint level; float3 padding; };
)";
    if(normal) src+=a2fo::renderer::shaders::bc5_normal_functions;
    src+=R"(
float4 PSMain(Varying input):SV_Target {
    float4 texel=image.SampleLevel(point_sampler,input.uv,level);
)";
    if(normal) src+=format==DXGI_FORMAT_BC5_SNORM ?
        "return float4(a2fo_decode_bc5_snorm(texel.rg)*.5+.5,1);\n" :
        "return float4(a2fo_decode_bc5_unorm(texel.rg)*.5+.5,1);\n";
    else if(format==DXGI_FORMAT_BC5_SNORM) src+="return float4(texel.rg*.5+.5,0,1);\n";
    else src+="return float4(texel.rgb,texel.a);\n";
    src+="}\n";return src;
}
void verify_pixels(ID3D11DeviceContext* context,ID3D11Texture2D* target,ID3D11Texture2D* staging,
                   DXGI_FORMAT format,unsigned mip,bool normal){
    context->CopyResource(staging,target);
    D3D11_MAPPED_SUBRESOURCE map{};checked(context->Map(staging,0,D3D11_MAP_READ,0,&map),"map readback");
    unsigned max_delta=0,bad=0;
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
        const auto want=expected(format,mip,x,y,normal);
        const auto* pixel=static_cast<const std::uint8_t*>(map.pData)+y*map.RowPitch+x*4;
        for(unsigned channel=0;channel<4;++channel){
            const unsigned delta=difference(pixel[channel],unsigned(std::lround(want[channel]*255.f)));
            max_delta=std::max(max_delta,delta);
            if(delta>1){if(bad<4)std::fprintf(stderr,"MISMATCH %s mip=%u normal=%u (%u,%u) channel=%u got=%u expected=%u\n",
                format_name(format),mip,normal?1:0,x,y,channel,pixel[channel],unsigned(std::lround(want[channel]*255.f)));++bad;}
        }
    }
    context->Unmap(staging,0);
    std::printf("PIXELS format=%s mip=%u mode=%s maxdelta=%u bad=%u encodedbytes=112\n",
                format_name(format),mip,normal?"normal":"raw",max_delta,bad);
    require(bad==0,"all 64 sampled pixels within one UNORM level");
}
struct Vertex {float x,y,z,rhw;std::uint32_t diffuse;float u,v;};
void ui_draw(UiRenderer& ui,ID3D11ShaderResourceView* view,ID3D11DeviceContext* context,
             ID3D11Texture2D* target,ID3D11Texture2D* staging){
    checked(ui.set_texture(view),"bind BC7 through UiRenderer");
    checked(ui.set_viewport({0,0,8,8,0,1}),"UI viewport");
    checked(ui.clear_color(0xff000000),"UI clear");
    const Vertex quad[4]={{-.5f,-.5f,.5f,1,0xffffffff,0,0},{7.5f,-.5f,.5f,1,0xffffffff,1,0},
                          {7.5f,7.5f,.5f,1,0xffffffff,1,1},{-.5f,7.5f,.5f,1,0xffffffff,0,1}};
    UiDrawState state;state.color_op=state.alpha_op=2;state.min_filter=state.mag_filter=1;
    checked(ui.draw_up(6,2,0x144,quad,sizeof(quad),sizeof(Vertex),state),"UI BC7 textured quad");
    verify_pixels(context,target,staging,DXGI_FORMAT_BC7_UNORM,0,false);
    checked(ui.set_texture(nullptr),"unbind UI texture");
}
}
int main(int argc,char** argv){
    require(argc>=2 && argc<=4,
            "usage: renderer_bc_native.exe OUTPUT_DIR [EXTERNAL_DDS [RGBA8_8X8_REFERENCE]]");
    (void)argv[1]; // Reserved by the private validator for captured artifacts.
    CompressedImage external_image;
    std::vector<std::uint8_t> external_reference;
    if(argc>=3){
        const auto bytes=read_file(argv[2]);std::string error;
        checked_upload(parse_compressed_dds(bytes.data(),bytes.size(),external_image,error),error,"parse external DDS");
        require(external_image.format==DXGI_FORMAT_BC5_UNORM ||
                external_image.format==DXGI_FORMAT_BC5_SNORM ||
                external_image.format==DXGI_FORMAT_BC7_UNORM ||
                external_image.format==DXGI_FORMAT_BC7_UNORM_SRGB,"external DDS is supported BC5/BC7");
    }
    if(argc==4){
        external_reference=read_file(argv[3]);
        require(external_reference.size()==8*8*4,"external RGBA8 reference is exactly 256 bytes");
    }
    WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
    cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="A2FOBCPrivateFixture";
    require(RegisterClassExA(&cls)!=0,"register private window class");
    HWND window=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,"BC fixture",WS_POPUP,
                                0,0,8,8,nullptr,nullptr,cls.hInstance,nullptr);
    require(window && !IsWindowVisible(window),"hidden nonactivating window");
    HMODULE library=LoadLibraryA("d3d11.dll");require(library!=nullptr,"load D3D11");
    {
    using Create=HRESULT (WINAPI*)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
    Create create=nullptr;const auto address=GetProcAddress(library,"D3D11CreateDevice");
    std::memcpy(&create,&address,sizeof(create));require(create!=nullptr,"resolve D3D11CreateDevice");
    Owned<ID3D11Device> device;Owned<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};const D3D_FEATURE_LEVEL requested=D3D_FEATURE_LEVEL_11_0;
    const HRESULT hr=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&requested,1,D3D11_SDK_VERSION,
                            device.out(),&level,context.out());
    if(FAILED(hr)){
        std::printf("SKIP: D3D11 hardware device hr=0x%08lx\n",static_cast<unsigned long>(hr));
        if(context.p){context.p->Release();context.p=nullptr;}
        if(device.p){device.p->Release();device.p=nullptr;}
        DestroyWindow(window);UnregisterClassA(cls.lpszClassName,cls.hInstance);
        FreeLibrary(library);return 77;
    }
    require(level>=requested,"SM5 hardware level");
    Owned<IDXGIDevice> dxgi_device;
    checked(device->QueryInterface(__uuidof(IDXGIDevice),
            reinterpret_cast<void**>(dxgi_device.out())),"query DXGI device identity");
    Owned<IDXGIAdapter> adapter;
    checked(dxgi_device->GetAdapter(adapter.out()),"get actual D3D11 adapter");
    DXGI_ADAPTER_DESC adapter_desc{};
    checked(adapter->GetDesc(&adapter_desc),"get actual D3D11 adapter description");
    char adapter_name[512]{};
    require(WideCharToMultiByte(CP_UTF8,0,adapter_desc.Description,-1,
                                adapter_name,sizeof(adapter_name),nullptr,nullptr)>0,
            "encode adapter description");
    std::printf("ADAPTER vendor=0x%04x device=0x%04x description=%s\n",
                adapter_desc.VendorId,adapter_desc.DeviceId,adapter_name);
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=8;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    Owned<ID3D11Texture2D> target,staging;Owned<ID3D11RenderTargetView> rtv;
    checked(device->CreateTexture2D(&desc,nullptr,target.out()),"render target");
    checked(device->CreateRenderTargetView(target.p,nullptr,rtv.out()),"render target view");
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    checked(device->CreateTexture2D(&desc,nullptr,staging.out()),"readback target");
    UiRenderer ui;checked(ui.initialize(device.p,context.p,{}),"UI initialize");
    checked(ui.set_target(rtv.p,nullptr,8,8),"UI render target");
    for(DXGI_FORMAT format:{DXGI_FORMAT_BC5_UNORM,DXGI_FORMAT_BC5_SNORM,
                            DXGI_FORMAT_BC7_UNORM,DXGI_FORMAT_BC7_UNORM_SRGB}){
        UINT support=0;checked(device->CheckFormatSupport(format,&support),"query BC support");
        require((support&(D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))==
                (D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),"typed BC texture sampling supported");
        const auto file=bc_fixture::make_dds(format);
        CompressedImage image;std::string error;
        checked(parse_compressed_dds(file.data(),file.size(),image,error),"parse original BC DDS");
        require(error.empty() && image.bytes.size()==112 && image.mips.size()==4,"encoded full mip chain is 112 bytes");
        Owned<ID3D11ShaderResourceView> view;CompressedTextureInfo info{};
        checked_upload(create_compressed_texture(device.p,image,view.out(),info,error),error,"generic compressed upload");
        require(error.empty() && info.format==format && info.width==8 && info.height==8 &&
                info.mip_levels==4 && info.block_payload_bytes==112 && info.rgba8_reference_bytes==340,
                "compressed upload metadata and storage ratio");
        Owned<ID3D11ShaderResourceView> facade_view;
        checked(exercise_compressed_texture8(window,device.p,image,facade_view.out()),"D3D8 texture facade contract");
        std::swap(view.p,facade_view.p);
        Owned<ID3D11Resource> resource;view->GetResource(resource.out());
        Owned<ID3D11Texture2D> texture;checked(resource->QueryInterface(IID_ID3D11Texture2D,reinterpret_cast<void**>(texture.out())),"SRV is texture2D");
        D3D11_TEXTURE2D_DESC uploaded{};texture->GetDesc(&uploaded);
        require(uploaded.Format==format && uploaded.Usage==D3D11_USAGE_IMMUTABLE && uploaded.MipLevels==4 &&
                uploaded.ArraySize==1 && uploaded.CPUAccessFlags==0,"typed immutable compressed resource");
        D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};view->GetDesc(&view_desc);
        require(view_desc.Format==format && view_desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D &&
                view_desc.Texture2D.MipLevels==4,"typed SRV covers all mips");
        auto forged=image;forged.mips[1].row_pitch=1;
        Owned<ID3D11ShaderResourceView> invalid;CompressedTextureInfo unchanged{};
        unchanged.width=99;unchanged.block_payload_bytes=123;
        require(FAILED(create_compressed_texture(device.p,forged,invalid.out(),unchanged,error)) &&
                !invalid.p && unchanged.width==99 && unchanged.block_payload_bytes==123 && !error.empty(),
                "bad mip pitch rejects atomically");
        if(format==DXGI_FORMAT_BC7_UNORM){
            Owned<ID3D11ShaderResourceView> ui_view;
            checked_upload(ui.upload_compressed_texture(image,ui_view.out(),error),error,"UI compressed upload");
            ui_draw(ui,ui_view.p,context.p,target.p,staging.p);
        }
        D3D11_SAMPLER_DESC sampler_desc{};sampler_desc.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler_desc.AddressU=sampler_desc.AddressV=sampler_desc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MaxLOD=D3D11_FLOAT32_MAX;
        Owned<ID3D11SamplerState> sampler;checked(device->CreateSamplerState(&sampler_desc,sampler.out()),"point clamp sampler");
        const char* vertex_source=R"(
struct Varying { float4 position:SV_Position; float2 uv:TEXCOORD0; };
Varying VSMain(uint id:SV_VertexID){
    Varying o;float2 uv=float2(id==1?2:0,id==2?2:0);
    o.position=float4(uv.x*2-1,1-uv.y*2,0,1);o.uv=uv;return o;
})";
        ShaderCompileResult vertex_code;
        checked(compile_shader({vertex_source,"VSMain","vs_5_0",0},{},vertex_code),"compile fullscreen VS");
        Owned<ID3D11VertexShader> vertex;checked(device->CreateVertexShader(vertex_code.bytes.data(),vertex_code.bytes.size(),nullptr,vertex.out()),"fullscreen VS");
        for(bool normal:{false,true}){
            if(normal && format!=DXGI_FORMAT_BC5_UNORM && format!=DXGI_FORMAT_BC5_SNORM)continue;
            ShaderCompileResult pixel_code;
            checked(compile_shader({pixel_source(format,normal),"PSMain","ps_5_0",0},{},pixel_code),"compile BC sampling PS");
            Owned<ID3D11PixelShader> pixel;checked(device->CreatePixelShader(pixel_code.bytes.data(),pixel_code.bytes.size(),nullptr,pixel.out()),"BC sampling PS");
            for(unsigned mip=0;mip<4;++mip){
                struct alignas(16) MipConstants{unsigned level;unsigned padding[3];} constants{mip,{0,0,0}};
                D3D11_BUFFER_DESC cb_desc{};cb_desc.ByteWidth=sizeof(constants);cb_desc.Usage=D3D11_USAGE_IMMUTABLE;cb_desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
                D3D11_SUBRESOURCE_DATA cb_data{};cb_data.pSysMem=&constants;
                Owned<ID3D11Buffer> cb;checked(device->CreateBuffer(&cb_desc,&cb_data,cb.out()),"mip constant buffer");
                context->OMSetRenderTargets(1,&rtv.p,nullptr);
                D3D11_VIEWPORT viewport{};viewport.Width=viewport.Height=8;viewport.MaxDepth=1;
                context->RSSetViewports(1,&viewport);
                context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context->VSSetShader(vertex.p,nullptr,0);context->PSSetShader(pixel.p,nullptr,0);
                context->PSSetConstantBuffers(0,1,&cb.p);context->PSSetShaderResources(0,1,&view.p);
                context->PSSetSamplers(0,1,&sampler.p);
                const float clear[4]={0,0,0,0};context->ClearRenderTargetView(rtv.p,clear);
                context->Draw(3,0);
                verify_pixels(context.p,target.p,staging.p,format,mip,normal);
                ID3D11ShaderResourceView* null_view=nullptr;context->PSSetShaderResources(0,1,&null_view);
            }
        }
        if(argc>=3 && external_image.format==format){
            Owned<ID3D11ShaderResourceView> external_view;CompressedTextureInfo external_info{};
            std::string external_error;
            checked_upload(create_compressed_texture(device.p,external_image,external_view.out(),
                                                     external_info,external_error),external_error,"upload external DDS");
            require(external_info.format==format && external_info.width==external_image.width &&
                    external_info.height==external_image.height &&
                    external_info.mip_levels==external_image.mips.size() &&
                    external_info.block_payload_bytes==external_image.bytes.size(),
                    "external DDS uploader metadata");
            std::uint64_t rgba_bytes=0;
            for(const auto& mip:external_image.mips)
                rgba_bytes+=std::uint64_t(mip.width)*mip.height*4;
            require(external_info.rgba8_reference_bytes==rgba_bytes,"external DDS reference byte count");
            Owned<ID3D11ShaderResourceView> external_facade_view;
            checked(exercise_compressed_texture8(window,device.p,external_image,external_facade_view.out()),
                    "real DDS D3D8 texture facade contract");
            std::swap(external_view.p,external_facade_view.p);
            Owned<ID3D11Resource> external_resource;external_view->GetResource(external_resource.out());
            Owned<ID3D11Texture2D> external_texture;
            checked(external_resource->QueryInterface(IID_ID3D11Texture2D,
                    reinterpret_cast<void**>(external_texture.out())),"external DDS resource type");
            D3D11_TEXTURE2D_DESC external_desc{};external_texture->GetDesc(&external_desc);
            require(external_desc.Format==format && external_desc.Width==external_image.width &&
                    external_desc.Height==external_image.height &&
                    external_desc.MipLevels==external_image.mips.size() &&
                    external_desc.ArraySize==1 && external_desc.Usage==D3D11_USAGE_IMMUTABLE &&
                    external_desc.CPUAccessFlags==0,"external DDS remains typed and compressed");
            D3D11_SHADER_RESOURCE_VIEW_DESC external_srv_desc{};external_view->GetDesc(&external_srv_desc);
            require(external_srv_desc.Format==format &&
                    external_srv_desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D &&
                    external_srv_desc.Texture2D.MipLevels==external_image.mips.size(),
                    "external DDS SRV covers supplied mip chain");
            ShaderCompileResult raw_code;
            checked(compile_shader({pixel_source(format,false),"PSMain","ps_5_0",0},{},raw_code),
                    "compile external raw sample PS");
            Owned<ID3D11PixelShader> raw_pixel;
            checked(device->CreatePixelShader(raw_code.bytes.data(),raw_code.bytes.size(),nullptr,
                                              raw_pixel.out()),"external raw sample PS");
            struct alignas(16) MipConstants{unsigned level;unsigned padding[3];} constants{0,{0,0,0}};
            D3D11_BUFFER_DESC cb_desc{};cb_desc.ByteWidth=sizeof(constants);
            cb_desc.Usage=D3D11_USAGE_IMMUTABLE;cb_desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            D3D11_SUBRESOURCE_DATA cb_data{};cb_data.pSysMem=&constants;
            Owned<ID3D11Buffer> cb;checked(device->CreateBuffer(&cb_desc,&cb_data,cb.out()),"external mip constant");
            context->OMSetRenderTargets(1,&rtv.p,nullptr);
            D3D11_VIEWPORT viewport{};viewport.Width=viewport.Height=8;viewport.MaxDepth=1;
            context->RSSetViewports(1,&viewport);
            context->IASetInputLayout(nullptr);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->VSSetShader(vertex.p,nullptr,0);context->PSSetShader(raw_pixel.p,nullptr,0);
            context->PSSetConstantBuffers(0,1,&cb.p);
            context->PSSetShaderResources(0,1,&external_view.p);
            context->PSSetSamplers(0,1,&sampler.p);
            const float clear[4]={0,0,0,0};context->ClearRenderTargetView(rtv.p,clear);
            context->Draw(3,0);
            context->CopyResource(staging.p,target.p);
            D3D11_MAPPED_SUBRESOURCE map{};
            checked(context->Map(staging.p,0,D3D11_MAP_READ,0,&map),"map external DDS sample");
            std::uint64_t checksum=1469598103934665603ull;
            unsigned differing_pixels=0;std::array<std::uint8_t,4> first{};
            unsigned max_delta=0,bad_channels=0;
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
                const auto* pixel=static_cast<const std::uint8_t*>(map.pData)+y*map.RowPitch+x*4;
                if(x==0 && y==0)std::copy_n(pixel,4,first.begin());
                else if(!std::equal(pixel,pixel+4,first.begin()))++differing_pixels;
                for(unsigned channel=0;channel<4;++channel){
                    checksum^=pixel[channel];checksum*=1099511628211ull;
                    if(argc==4){
                        const unsigned want=external_reference[(y*8+x)*4+channel];
                        const unsigned delta=difference(pixel[channel],want);
                        max_delta=std::max(max_delta,delta);
                        if(delta>1){
                            if(bad_channels<4)
                                std::fprintf(stderr,"EXTERNAL MISMATCH (%u,%u) channel=%u got=%u expected=%u\n",
                                             x,y,channel,pixel[channel],want);
                            ++bad_channels;
                        }
                    }
                }
            }
            context->Unmap(staging.p,0);
            ID3D11ShaderResourceView* null_view=nullptr;
            context->PSSetShaderResources(0,1,&null_view);
            std::printf("EXTERNAL format=%s width=%u height=%u mips=%u encodedbytes=%llu rgba8bytes=%llu checksum=%016llx differing_pixels=%u mode=%s maxdelta=%u bad=%u\n",
                        format_name(format),external_image.width,external_image.height,
                        external_info.mip_levels,
                        static_cast<unsigned long long>(external_info.block_payload_bytes),
                        static_cast<unsigned long long>(external_info.rgba8_reference_bytes),
                        static_cast<unsigned long long>(checksum),differing_pixels,
                        argc==4?"cpu_reference":"checksum_only",max_delta,bad_channels);
            require(bad_channels==0,"external DDS matches independent CPU RGBA8 reference within one level");
        }
    }
    context->ClearState();context->Flush();ui.shutdown();
    }
    DestroyWindow(window);UnregisterClassA(cls.lpszClassName,cls.hInstance);FreeLibrary(library);
    std::puts("PASS: BC5 BC7 compressed DDS GPU sampling");
}
