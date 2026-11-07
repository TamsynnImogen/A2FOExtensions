#include "renderer.hpp"
#include "shaders.hpp"
#include "shadow_plan.hpp"
#include "shadow_target.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>

namespace a2fo::native11 {
namespace {
constexpr HRESULT bad=E_INVALIDARG;
constexpr std::uint32_t max_dim=16384, max_vertices=1000000, max_draws=8192;
constexpr std::uint64_t resource_budget=256ull*1024*1024;
template<class T> void drop(T*& p){if(p){p->Release();p=nullptr;}}
bool finite(float x){return std::isfinite(x);}
bool floats(const float* p,std::size_t n){for(std::size_t i=0;i<n;++i)if(!finite(p[i]))return false;return true;}
bool own(HWND h,DWORD thread){DWORD pid=0;return h&&IsWindow(h)&&GetWindowThreadProcessId(h,&pid)==thread&&
    thread==GetCurrentThreadId()&&pid==GetCurrentProcessId();}
using Create=HRESULT (WINAPI*)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,
    const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
using CreateDevice=HRESULT (WINAPI*)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,
    ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
using Compile=HRESULT (WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
struct alignas(16) Constants {
    float world_to_clip[16],local_to_world[16],normal_to_world[16];
    float tint[4],emission[4],ambient[4];
    float pos_type[8][4],dir_intensity[8][4],colour_start[8][4],range_cone[8][4];
    std::uint32_t light_count,flags,texture_alpha;float alpha_cutoff;
    float uv_rows[8];
    float fog_colour[4],fog_range[4],camera_position[4];
};
static_assert(sizeof(Constants)==848);
static_assert(offsetof(Constants,texture_alpha)==760);
static_assert(offsetof(Constants,alpha_cutoff)==764);
struct alignas(16) ShadowConstants {float world_to_clip[48][16];float settings[4];};
static_assert(sizeof(ShadowConstants)==3088);
// Inverse transpose of the upper-left 3x3, embedded in a row-major 4x4.
bool normal_matrix(const float* m,float* out){
    const double a=m[0],b=m[1],c=m[2],d=m[4],e=m[5],f=m[6],g=m[8],h=m[9],i=m[10];
    const double A=e*i-f*h,B=f*g-d*i,C=d*h-e*g;
    const double det=a*A+b*B+c*C;
    if(!std::isfinite(det)||std::fabs(det)<1e-20)return false;
    const double x[9]={A,(c*h-b*i),(b*f-c*e),B,(a*i-c*g),(c*d-a*f),C,(b*g-a*h),(a*e-b*d)};
    std::fill(out,out+16,0.f);out[15]=1;
    for(int row=0;row<3;++row)for(int col=0;col<3;++col)out[row*4+col]=static_cast<float>(x[col*3+row]/det);
    return floats(out,16);
}
bool mirrored(const float* m){
    return double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])+
        double(m[1])*(double(m[6])*m[8]-double(m[4])*m[10])+
        double(m[2])*(double(m[4])*m[9]-double(m[5])*m[8])<0;
}
}
struct Renderer::Impl {
    struct Mesh {ID3D11Buffer* vertices=nullptr,*indices=nullptr;UINT count=0;std::uint64_t bytes=0;ShadowBounds bounds{};float uv_low[2]{},uv_high[2]{};
        ~Mesh(){drop(vertices);drop(indices);}};
    struct Image {ID3D11Texture2D* image=nullptr;ID3D11ShaderResourceView* view=nullptr;
        TextureAlpha alpha=TextureAlpha::unknown;std::uint64_t bytes=0;
        ~Image(){drop(view);drop(image);}};
    HMODULE d3d=nullptr,compiler=nullptr;Compile compile=nullptr;
    ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;IDXGISwapChain* swap=nullptr;
    IDXGISwapChain* presentation_swap=nullptr;
    HWND presentation_window=nullptr;
    UINT presentation_width=0,presentation_height=0;
    bool presentation_image=false;
    ID3D11Texture2D* back=nullptr,*depth=nullptr,*readback_staging=nullptr;
    std::uint64_t readback_staging_creations=0;
    ID3D11RenderTargetView* rtv=nullptr;ID3D11DepthStencilView* dsv=nullptr;
    ID3D11VertexShader* vs=nullptr;ID3D11PixelShader* ps=nullptr,*shadow_ps=nullptr;ID3D11InputLayout* layout=nullptr;
    ID3D11Buffer* constants=nullptr,*shadow_constants=nullptr;ID3D11SamplerState* sampler=nullptr;
    std::array<ID3D11SamplerState*,3> extra_samplers{};
    ID3D11SamplerState* sampler_for(Sampling mode) const {const auto index=static_cast<unsigned>(mode);return index?extra_samplers[index-1]:sampler;}
    std::unique_ptr<ShadowTarget> shadows;
    ID3D11RasterizerState* cull=nullptr,*mirror_cull=nullptr,*two_sided=nullptr;
    ID3D11BlendState* opaque=nullptr,*alpha=nullptr,*additive=nullptr;
    ID3D11DepthStencilState* depth_write=nullptr,*depth_read=nullptr,*depth_none=nullptr;
    bool colour_valid=false,depth_valid=false;
    std::map<std::uint32_t,std::unique_ptr<Mesh>> meshes;
    std::map<std::uint32_t,std::unique_ptr<Image>> images;
    std::uint64_t mesh_bytes=0,image_bytes=0,epoch=0;
    std::uint32_t last_mesh=0,last_image=0;
    HWND hwnd=nullptr;DWORD thread=0;UINT width=0,height=0;Log log=nullptr;bool offscreen=false;
    ~Impl(){release_all();}
    bool ready()const{return device&&context&&(offscreen||swap)&&rtv&&dsv;}
    bool valid_mesh(std::uint32_t id)const{return meshes.find(id)!=meshes.end();}
    bool valid_image(std::uint32_t id)const{return images.find(id)!=images.end();}
    HRESULT guard()const{return ready()&&(offscreen?thread==GetCurrentThreadId():own(hwnd,thread))?
        device->GetDeviceRemovedReason():E_UNEXPECTED;}
    // Residency/cleanup belong to the device thread, even when swap-chain target
    // recreation failed or its HWND was destroyed. They must not require views.
    HRESULT resource_guard()const{return device&&context&&thread==GetCurrentThreadId()?S_OK:E_UNEXPECTED;}
    void release_targets(){colour_valid=depth_valid=false;drop(dsv);drop(depth);drop(rtv);drop(readback_staging);drop(back);}
    void release_all(){
        presentation_image=false;presentation_window=nullptr;
        presentation_width=presentation_height=0;drop(presentation_swap);
        meshes.clear();images.clear();mesh_bytes=image_bytes=0;
        if(context)context->ClearState();
        drop(depth_none);drop(depth_read);drop(depth_write);drop(additive);drop(alpha);drop(opaque);
        shadows.reset();drop(shadow_constants);drop(shadow_ps);
        drop(two_sided);drop(mirror_cull);drop(cull);
        for(auto& extra:extra_samplers){drop(extra);}drop(sampler);drop(constants);drop(layout);drop(ps);drop(vs);
        release_targets();drop(swap);drop(context);drop(device);offscreen=false;
        if(compiler){FreeLibrary(compiler);compiler=nullptr;}if(d3d){FreeLibrary(d3d);d3d=nullptr;}
    }
    HRESULT targets(){
        HRESULT hr=S_OK;
        if(offscreen){
            D3D11_TEXTURE2D_DESC colour{};colour.Width=width;colour.Height=height;
            colour.MipLevels=colour.ArraySize=1;colour.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
            colour.SampleDesc.Count=1;colour.Usage=D3D11_USAGE_DEFAULT;
            colour.BindFlags=D3D11_BIND_RENDER_TARGET;
            hr=device->CreateTexture2D(&colour,nullptr,&back);
        }else hr=swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&back));
        if(FAILED(hr))return hr;
        hr=device->CreateRenderTargetView(back,nullptr,&rtv);if(FAILED(hr))return hr;
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=1;
        d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;
        d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        hr=device->CreateTexture2D(&d,nullptr,&depth);if(FAILED(hr))return hr;
        return device->CreateDepthStencilView(depth,nullptr,&dsv);
    }
    HRESULT shader(const char* source,const char* profile,ID3DBlob** output){
        ID3DBlob* errors=nullptr;
        HRESULT hr=compile(source,std::strlen(source),nullptr,nullptr,nullptr,"main",profile,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,output,&errors);
        if(FAILED(hr)&&errors&&log)log(static_cast<const char*>(errors->GetBufferPointer()));
        drop(errors);return hr;
    }
    HRESULT setup(){
        ID3DBlob *v=nullptr,*p=nullptr,*s=nullptr;
        HRESULT hr=shader(shaders::vertex,"vs_5_0",&v);
        if(SUCCEEDED(hr))hr=shader(shaders::pixel,"ps_5_0",&p);
        if(SUCCEEDED(hr))hr=device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs);
        if(SUCCEEDED(hr))hr=device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps);
        if(SUCCEEDED(hr))hr=shader(shaders::shadow_pixel,"ps_5_0",&s);
        if(SUCCEEDED(hr))hr=device->CreatePixelShader(s->GetBufferPointer(),s->GetBufferSize(),nullptr,&shadow_ps);
        const D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(SUCCEEDED(hr))hr=device->CreateInputLayout(elements,3,v->GetBufferPointer(),v->GetBufferSize(),&layout);
        drop(v);drop(p);drop(s);if(FAILED(hr))return hr;
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Constants);buffer.Usage=D3D11_USAGE_DEFAULT;
        buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        hr=device->CreateBuffer(&buffer,nullptr,&constants);if(FAILED(hr))return hr;
        buffer.ByteWidth=sizeof(ShadowConstants);
        hr=device->CreateBuffer(&buffer,nullptr,&shadow_constants);if(FAILED(hr))return hr;
        D3D11_SAMPLER_DESC sample{};sample.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        sample.MaxLOD=D3D11_FLOAT32_MAX;
        hr=device->CreateSamplerState(&sample,&sampler);if(FAILED(hr))return hr;
        for(unsigned i=1;i<4;++i){
            sample.Filter=i>=2?D3D11_FILTER_MIN_MAG_MIP_POINT:D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sample.AddressU=sample.AddressV=sample.AddressW=(i&1)?D3D11_TEXTURE_ADDRESS_WRAP:D3D11_TEXTURE_ADDRESS_CLAMP;
            hr=device->CreateSamplerState(&sample,&extra_samplers[i-1]);if(FAILED(hr))return hr;
        }
        D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_BACK;
        rs.DepthClipEnable=TRUE;hr=device->CreateRasterizerState(&rs,&cull);if(FAILED(hr))return hr;
        rs.FrontCounterClockwise=TRUE;hr=device->CreateRasterizerState(&rs,&mirror_cull);if(FAILED(hr))return hr;
        rs.CullMode=D3D11_CULL_NONE;hr=device->CreateRasterizerState(&rs,&two_sided);if(FAILED(hr))return hr;
        D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=TRUE;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
        ds.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        hr=device->CreateDepthStencilState(&ds,&depth_write);if(FAILED(hr))return hr;
        ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
        hr=device->CreateDepthStencilState(&ds,&depth_read);if(FAILED(hr))return hr;
        ds.DepthEnable=FALSE;
        hr=device->CreateDepthStencilState(&ds,&depth_none);if(FAILED(hr))return hr;
        D3D11_BLEND_DESC blend{};blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        hr=device->CreateBlendState(&blend,&opaque);if(FAILED(hr))return hr;
        blend.RenderTarget[0].BlendEnable=TRUE;blend.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
        hr=device->CreateBlendState(&blend,&alpha);if(FAILED(hr))return hr;
        blend.RenderTarget[0].SrcBlend=D3D11_BLEND_ONE;blend.RenderTarget[0].DestBlend=D3D11_BLEND_ONE;
        blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ONE;
        hr=device->CreateBlendState(&blend,&additive);if(FAILED(hr))return hr;
        const std::uint32_t white=0xffffffff;
        ImageView white_view{reinterpret_cast<const std::uint8_t*>(&white),1,1,4,4};
        return upload_image(white_view,0);
    }
    HRESULT upload_image(TextureView input,const TexturePlan& plan,std::uint32_t expected){
        DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
        switch(plan.format){
        case TextureFormat::bgra8:format=DXGI_FORMAT_B8G8R8A8_UNORM;break;
        case TextureFormat::rgba8:format=DXGI_FORMAT_R8G8B8A8_UNORM;break;
        case TextureFormat::bgra8_srgb:format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;break;
        case TextureFormat::rgba8_srgb:format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;break;
        case TextureFormat::bc1:format=DXGI_FORMAT_BC1_UNORM;break;
        case TextureFormat::bc1_srgb:format=DXGI_FORMAT_BC1_UNORM_SRGB;break;
        case TextureFormat::bc2:format=DXGI_FORMAT_BC2_UNORM;break;
        case TextureFormat::bc2_srgb:format=DXGI_FORMAT_BC2_UNORM_SRGB;break;
        case TextureFormat::bc3:format=DXGI_FORMAT_BC3_UNORM;break;
        case TextureFormat::bc3_srgb:format=DXGI_FORMAT_BC3_UNORM_SRGB;break;
        case TextureFormat::bc4:format=DXGI_FORMAT_BC4_UNORM;break;
        case TextureFormat::bc4_snorm:format=DXGI_FORMAT_BC4_SNORM;break;
        case TextureFormat::bc5:format=DXGI_FORMAT_BC5_UNORM;break;
        case TextureFormat::bc5_snorm:format=DXGI_FORMAT_BC5_SNORM;break;
        case TextureFormat::bc7:format=DXGI_FORMAT_BC7_UNORM;break;
        case TextureFormat::bc7_srgb:format=DXGI_FORMAT_BC7_UNORM_SRGB;break;
        }
        UINT support=0;HRESULT hr=device->CheckFormatSupport(format,&support);
        if(FAILED(hr))return hr;
        const UINT required=D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE|
            (plan.mip_count>1?D3D11_FORMAT_SUPPORT_MIP:0);
        if((support&required)!=required)return DXGI_ERROR_UNSUPPORTED;
        std::array<D3D11_SUBRESOURCE_DATA,15> levels{};
        for(UINT i=0;i<plan.mip_count;++i){
            levels[i].pSysMem=input.bytes+input.mips[i].offset;
            levels[i].SysMemPitch=input.mips[i].row_pitch;
            levels[i].SysMemSlicePitch=input.mips[i].slice_pitch;
        }
        auto image=std::make_unique<Image>();image->alpha=plan.alpha;
        D3D11_TEXTURE2D_DESC d{};d.Width=plan.width;d.Height=plan.height;
        d.MipLevels=plan.mip_count;d.ArraySize=1;d.Format=format;
        d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        hr=device->CreateTexture2D(&d,levels.data(),&image->image);if(FAILED(hr))return hr;
        D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=format;
        srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=plan.mip_count;
        hr=device->CreateShaderResourceView(image->image,&srv,&image->view);if(FAILED(hr))return hr;
        if(images.find(expected)!=images.end())return E_UNEXPECTED;
        image->bytes=plan.storage_bytes;images.emplace(expected,std::move(image));image_bytes+=plan.storage_bytes;return S_OK;
    }
    HRESULT upload_image(ImageView input,std::uint32_t expected){
        auto image=std::make_unique<Image>();
        D3D11_TEXTURE2D_DESC d{};d.Width=input.width;d.Height=input.height;
        d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=input.bgra;data.SysMemPitch=input.pitch;
        HRESULT hr=device->CreateTexture2D(&d,&data,&image->image);if(FAILED(hr))return hr;
        hr=device->CreateShaderResourceView(image->image,nullptr,&image->view);if(FAILED(hr))return hr;
        if(images.find(expected)!=images.end())return E_UNEXPECTED;
        image->bytes=std::uint64_t(input.width)*input.height*4;
        images.emplace(expected,std::move(image));image_bytes+=std::uint64_t(input.width)*input.height*4;return S_OK;
    }
};
Renderer::Renderer():p_(std::make_unique<Impl>()){}
Renderer::~Renderer()=default;
HRESULT Renderer::initialize(HWND hwnd,std::uint32_t width,std::uint32_t height,Log log){
    if(p_->device||p_->epoch==UINT64_MAX||!hwnd||!IsWindow(hwnd)||width==0||height==0||width>max_dim||height>max_dim||
       std::uint64_t(width)*height*8>resource_budget)return bad;
    const DWORD thread=GetCurrentThreadId();if(!own(hwnd,thread))return bad;
    p_->hwnd=hwnd;p_->thread=thread;p_->width=width;p_->height=height;p_->log=log;
    p_->d3d=LoadLibraryA("d3d11.dll");if(!p_->d3d){const auto hr=HRESULT_FROM_WIN32(GetLastError());p_->release_all();return hr;}
    p_->compiler=LoadLibraryA("d3dcompiler_47.dll");
    if(!p_->compiler)p_->compiler=LoadLibraryA("d3dcompiler_43.dll");
    if(!p_->compiler){const auto hr=HRESULT_FROM_WIN32(GetLastError());p_->release_all();return hr;}
    const FARPROC compile_symbol=GetProcAddress(p_->compiler,"D3DCompile");
    const FARPROC create_symbol=GetProcAddress(p_->d3d,"D3D11CreateDeviceAndSwapChain");
    static_assert(sizeof(compile_symbol)==sizeof(p_->compile) && sizeof(create_symbol)==sizeof(Create));
    std::memcpy(&p_->compile,&compile_symbol,sizeof(compile_symbol));
    Create create=nullptr;std::memcpy(&create,&create_symbol,sizeof(create_symbol));
    if(!create||!p_->compile){p_->release_all();return E_NOINTERFACE;}
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=width;desc.BufferDesc.Height=height;
    desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=hwnd;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;D3D_FEATURE_LEVEL got{};
    HRESULT hr=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &level,1,D3D11_SDK_VERSION,&desc,&p_->swap,&p_->device,&got,&p_->context);
    if(SUCCEEDED(hr)&&got!=level)hr=E_FAIL;
    if(SUCCEEDED(hr))hr=p_->targets();
    if(SUCCEEDED(hr))hr=p_->setup();
    if(FAILED(hr))p_->release_all();
    else ++p_->epoch;
    return hr;
}
HRESULT Renderer::initialize_offscreen(std::uint32_t width,std::uint32_t height,Log log){
    if(p_->device||p_->epoch==UINT64_MAX||!width||!height||width>max_dim||height>max_dim||
       std::uint64_t(width)*height*8>resource_budget)return bad;
    p_->hwnd=nullptr;p_->thread=GetCurrentThreadId();p_->width=width;p_->height=height;
    p_->log=log;p_->offscreen=true;
    p_->d3d=LoadLibraryA("d3d11.dll");
    if(!p_->d3d){const auto hr=HRESULT_FROM_WIN32(GetLastError());p_->release_all();return hr;}
    p_->compiler=LoadLibraryA("d3dcompiler_47.dll");
    if(!p_->compiler)p_->compiler=LoadLibraryA("d3dcompiler_43.dll");
    if(!p_->compiler){const auto hr=HRESULT_FROM_WIN32(GetLastError());p_->release_all();return hr;}
    const FARPROC compile_symbol=GetProcAddress(p_->compiler,"D3DCompile");
    const FARPROC create_symbol=GetProcAddress(p_->d3d,"D3D11CreateDevice");
    static_assert(sizeof(compile_symbol)==sizeof(p_->compile) && sizeof(create_symbol)==sizeof(CreateDevice));
    std::memcpy(&p_->compile,&compile_symbol,sizeof(compile_symbol));
    CreateDevice create=nullptr;std::memcpy(&create,&create_symbol,sizeof(create_symbol));
    if(!create||!p_->compile){p_->release_all();return E_NOINTERFACE;}
    const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;D3D_FEATURE_LEVEL got{};
    HRESULT hr=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &level,1,D3D11_SDK_VERSION,&p_->device,&got,&p_->context);
    if(SUCCEEDED(hr)&&got!=level)hr=E_FAIL;
    if(SUCCEEDED(hr))hr=p_->targets();
    if(SUCCEEDED(hr))hr=p_->setup();
    if(FAILED(hr))p_->release_all();
    else ++p_->epoch;
    return hr;
}
HRESULT Renderer::upload_mesh(MeshView view,std::uint32_t& handle){
    HRESULT hr=p_->guard();if(FAILED(hr))return hr;
    if(!view.vertices||!view.indices||view.vertex_count<3||view.vertex_count>max_vertices||
       view.index_count<3||view.index_count>max_vertices*3||view.index_count%3)return bad;
    if(p_->meshes.size()>=8192||p_->last_mesh==UINT32_MAX)return E_OUTOFMEMORY;
    const std::uint64_t bytes=std::uint64_t(view.vertex_count)*sizeof(Vertex)+std::uint64_t(view.index_count)*4;
    if(bytes>resource_budget-p_->mesh_bytes-p_->image_bytes)return E_OUTOFMEMORY;
    for(UINT i=0;i<view.vertex_count;++i)if(!floats(view.vertices[i].position,3)||
        !floats(view.vertices[i].normal,3)||!floats(view.vertices[i].uv,2))return bad;
    for(UINT i=0;i<view.index_count;++i)if(view.indices[i]>=view.vertex_count)return bad;
    auto mesh=std::make_unique<Impl::Mesh>();mesh->count=view.index_count;
    for(int axis=0;axis<3;++axis){mesh->bounds.low[axis]=mesh->bounds.high[axis]=view.vertices[0].position[axis];
        for(UINT i=1;i<view.vertex_count;++i){mesh->bounds.low[axis]=std::min(mesh->bounds.low[axis],view.vertices[i].position[axis]);
            mesh->bounds.high[axis]=std::max(mesh->bounds.high[axis],view.vertices[i].position[axis]);}}
    for(unsigned axis=0;axis<2;++axis){mesh->uv_low[axis]=mesh->uv_high[axis]=view.vertices[0].uv[axis];
        for(UINT i=1;i<view.vertex_count;++i){mesh->uv_low[axis]=std::min(mesh->uv_low[axis],view.vertices[i].uv[axis]);
            mesh->uv_high[axis]=std::max(mesh->uv_high[axis],view.vertices[i].uv[axis]);}}
    D3D11_BUFFER_DESC d{};d.ByteWidth=view.vertex_count*sizeof(Vertex);d.Usage=D3D11_USAGE_IMMUTABLE;
    d.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA data{};data.pSysMem=view.vertices;
    hr=p_->device->CreateBuffer(&d,&data,&mesh->vertices);if(FAILED(hr))return hr;
    d.ByteWidth=view.index_count*sizeof(std::uint32_t);d.BindFlags=D3D11_BIND_INDEX_BUFFER;
    data.pSysMem=view.indices;hr=p_->device->CreateBuffer(&d,&data,&mesh->indices);if(FAILED(hr))return hr;
    mesh->bytes=bytes;const auto id=p_->last_mesh+1;
    p_->meshes.emplace(id,std::move(mesh));p_->mesh_bytes+=bytes;p_->last_mesh=id;
    handle=id;return S_OK;
}
HRESULT Renderer::upload_texture(ImageView view,std::uint32_t& handle){
    HRESULT hr=p_->guard();if(FAILED(hr))return hr;
    if(!view.bgra||!view.width||!view.height||view.width>max_dim||view.height>max_dim||
       view.pitch<static_cast<std::uint64_t>(view.width)*4||
       view.byte_count<static_cast<std::uint64_t>(view.pitch)*(view.height-1)+view.width*4)return bad;
    if(p_->images.size()>=4097||p_->last_image==UINT32_MAX)return E_OUTOFMEMORY;
    const std::uint64_t bytes=std::uint64_t(view.width)*view.height*4;
    if(bytes>resource_budget-p_->mesh_bytes-p_->image_bytes)return E_OUTOFMEMORY;
    const auto id=p_->last_image+1;hr=p_->upload_image(view,id);
    if(SUCCEEDED(hr)){p_->last_image=id;handle=id;}
    return hr;
}
HRESULT Renderer::upload_texture(TextureView view,std::uint32_t& handle){
    HRESULT hr=p_->guard();if(FAILED(hr))return hr;
    TexturePlan plan{};
    if(!plan_texture(view,plan))return bad;
    if(p_->images.size()>=4097||p_->last_image==UINT32_MAX)return E_OUTOFMEMORY;
    // Custom alpha has no generic compositing rule. Preserve it in the CPU
    // adapter, and reject it explicitly until a material-specific consumer exists.
    if(plan.alpha==TextureAlpha::custom)return E_NOTIMPL;
    if(plan.storage_bytes>resource_budget-p_->mesh_bytes-p_->image_bytes)return E_OUTOFMEMORY;
    const auto id=p_->last_image+1;hr=p_->upload_image(view,plan,id);
    if(SUCCEEDED(hr)){p_->last_image=id;handle=id;}
    return hr;
}
HRESULT Renderer::release_mesh(std::uint32_t handle){
    HRESULT hr=p_->resource_guard();if(FAILED(hr))return hr;
    if(!p_->valid_mesh(handle))return bad;
    // Drop context references as well as registry ownership. Queued GPU work
    // retains its normal D3D11 lifetime; logical admission bytes are reclaimed.
    ID3D11Buffer* none=nullptr;const UINT zero=0;
    p_->context->IASetVertexBuffers(0,1,&none,&zero,&zero);
    p_->context->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);
    const auto found=p_->meshes.find(handle);p_->mesh_bytes-=found->second->bytes;p_->meshes.erase(found);
    return S_OK;
}
HRESULT Renderer::release_texture(std::uint32_t handle){
    HRESULT hr=p_->resource_guard();if(FAILED(hr))return hr;
    if(!handle||!p_->valid_image(handle))return bad;
    ID3D11ShaderResourceView* none=nullptr;p_->context->PSSetShaderResources(0,1,&none);
    const auto found=p_->images.find(handle);p_->image_bytes-=found->second->bytes;p_->images.erase(found);
    return S_OK;
}
HRESULT Renderer::resource_usage(ResourceUsage& output)const{
    const HRESULT hr=p_->resource_guard();if(FAILED(hr))return hr;
    ResourceUsage value{};value.epoch=p_->epoch;value.mesh_bytes=p_->mesh_bytes;value.texture_bytes=p_->image_bytes;
    for(const auto& mesh:p_->meshes)if(mesh.second)++value.meshes;
    for(const auto& image:p_->images)if(image.second)++value.textures;
    output=value;return S_OK;
}
HRESULT Renderer::render(FrameView frame,bool present){
    HRESULT hr=p_->guard();if(FAILED(hr))return hr;
    p_->presentation_image=false;
    if(present&&p_->offscreen)return E_INVALIDARG;
    if((!frame.clear_colour_target&&!p_->colour_valid)||(!frame.clear_depth_target&&!p_->depth_valid))return bad;
    if(frame.ordering!=DrawOrdering::capture&&frame.ordering!=DrawOrdering::opaque_then_alpha)return bad;
    if(!floats(frame.world_to_clip,16)||!floats(frame.ambient,3)||!floats(frame.clear_colour,4)||
       frame.light_count>8||frame.draw_count>max_draws||
       (frame.light_count&&!frame.lights)||(frame.draw_count&&!frame.draws))return bad;
    if(!floats(frame.viewport,4)||!floats(frame.fog.colour,3)||!floats(frame.fog.camera_position,3)||
       !finite(frame.fog.start)||!finite(frame.fog.end))return bad;
    for(float colour:frame.fog.colour)if(colour<0||colour>1)return bad;
    if(frame.fog.enabled&&(frame.fog.start<0||frame.fog.end<=frame.fog.start))return bad;
    const bool full_view=frame.viewport[0]==0&&frame.viewport[1]==0&&
        frame.viewport[2]==0&&frame.viewport[3]==0;
    if(!full_view&&(frame.viewport[0]<0||frame.viewport[1]<0||
       frame.viewport[2]<1||frame.viewport[3]<1||
       double(frame.viewport[0])+frame.viewport[2]>p_->width||
       double(frame.viewport[1])+frame.viewport[3]>p_->height))return bad;
    for(float v:frame.ambient)if(v<0)return bad;
    for(float v:frame.clear_colour)if(v<0||v>1)return bad;
    for(UINT j=0;j<frame.light_count;++j){const auto& l=frame.lights[j];
        if(l.type!=LightType::directional&&l.type!=LightType::point&&l.type!=LightType::spot)return bad;
        if(!floats(l.position,3)||!floats(l.direction,3)||!floats(l.colour,3)||
           !finite(l.intensity)||!finite(l.falloff_start)||!finite(l.falloff_range)||
           !finite(l.cone_inner_degrees)||!finite(l.cone_outer_degrees)||l.intensity<0||
           (l.type!=LightType::directional&&(l.falloff_start<0||l.falloff_range<=0))||
           (l.type==LightType::spot&&(l.cone_inner_degrees<0||l.cone_outer_degrees<l.cone_inner_degrees||
           l.cone_outer_degrees>=180||l.cone_outer_degrees<=0)))return bad;
        if(l.colour[0]<0||l.colour[1]<0||l.colour[2]<0)return bad;
        for(float colour:l.colour)if(!finite(colour*l.intensity))return bad;
        const float n=l.direction[0]*l.direction[0]+l.direction[1]*l.direction[1]+l.direction[2]*l.direction[2];
        if(l.type!=LightType::point&&(n<1e-12f||!finite(n)))return bad;
    }
    for(UINT j=0;j<frame.draw_count;++j){const auto& d=frame.draws[j];float normal[16];
        if(!p_->valid_mesh(d.mesh)||!p_->valid_image(d.texture)||
           (d.flags&~15u)||((d.flags&alpha_blend)&&(d.flags&additive))||
           !floats(d.local_to_world,16)||!floats(d.colour,4)||!floats(d.emissive,3)||
           d.local_to_world[12]!=0||d.local_to_world[13]!=0||d.local_to_world[14]!=0||
           d.local_to_world[15]!=1||!normal_matrix(d.local_to_world,normal))return bad;
        if(!floats(d.uv_transform,6))return bad;
        const auto& uv_mesh=*p_->meshes.at(d.mesh);
        const double ux=std::max(std::fabs(double(uv_mesh.uv_low[0])),std::fabs(double(uv_mesh.uv_high[0])));
        const double uy=std::max(std::fabs(double(uv_mesh.uv_low[1])),std::fabs(double(uv_mesh.uv_high[1])));
        for(unsigned row=0;row<2;++row)if(std::fabs(double(d.uv_transform[row*3]))*ux+
            std::fabs(double(d.uv_transform[row*3+1]))*uy+std::fabs(double(d.uv_transform[row*3+2]))>
            std::numeric_limits<float>::max())return bad;
        if(static_cast<std::uint32_t>(d.sampler)>3)return bad;
        if(d.depth_mode!=DepthMode::automatic&&d.depth_mode!=DepthMode::read_only&&d.depth_mode!=DepthMode::disabled)return bad;
        if(!finite(d.alpha_cutoff)||d.alpha_cutoff<0||d.alpha_cutoff>1)return bad;
        for(float x:d.colour)if(x<0)return bad;
        if(d.colour[3]>1)return bad;
        for(float x:d.emissive)if(x<0)return bad;
    }
    struct OrderedDraw {UINT index,layer;bool transparent;double depth;};
    std::vector<OrderedDraw> colour_order;
    try {
        colour_order.reserve(frame.draw_count);
        for(UINT j=0;j<frame.draw_count;++j){const auto& draw=frame.draws[j];
            if(draw.shadow_only)continue;
            OrderedDraw item{j,draw.layer,(draw.flags&(alpha_blend|additive))!=0,0};
            if(frame.ordering==DrawOrdering::opaque_then_alpha&&item.transparent){
                const auto& bounds=p_->meshes.at(draw.mesh)->bounds;
                double world[4]{0,0,0,1};
                for(unsigned row=0;row<3;++row){world[row]=draw.local_to_world[row*4+3];
                    for(unsigned col=0;col<3;++col)world[row]+=double(draw.local_to_world[row*4+col])*
                        ((double(bounds.low[col])+bounds.high[col])*.5);}
                double z=0,w=0;
                for(unsigned col=0;col<4;++col){z+=double(frame.world_to_clip[8+col])*world[col];
                    w+=double(frame.world_to_clip[12+col])*world[col];}
                if(!std::isfinite(z)||!std::isfinite(w)||w<=1e-12)return bad;
                item.depth=z/w;if(!std::isfinite(item.depth))return bad;
            }
            colour_order.push_back(item);
        }
        if(frame.ordering==DrawOrdering::opaque_then_alpha)
            std::stable_sort(colour_order.begin(),colour_order.end(),[](const OrderedDraw& a,const OrderedDraw& b){
                if(a.layer!=b.layer)return a.layer<b.layer;
                if(a.transparent!=b.transparent)return !a.transparent;
                return a.transparent&&a.depth>b.depth;
            });
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}
     catch(...){return E_FAIL;}
    const auto& settings=frame.shadows;
    // Binary32 endpoint is exact on x87 too; do not widen a decimal literal.
    if(static_cast<std::uint32_t>(settings.filter)>2)return bad;
    if((settings.light_mask&~((1u<<frame.light_count)-1u))||!finite(settings.depth_bias)||
       settings.depth_bias<0||settings.depth_bias>0x1.99999ap-4f)return bad;
    if(settings.light_mask&&(settings.resolution<128||settings.resolution>2048||
       (settings.resolution&(settings.resolution-1))))return bad;
    std::array<ShadowLightPlan,8> plans{};std::array<UINT,8> shadow_bases{};
    UINT shadow_slices=0;
    ShadowConstants shadow_data{};shadow_data.settings[0]=settings.depth_bias;
    shadow_data.settings[1]=static_cast<float>(settings.resolution);
    shadow_data.settings[2]=static_cast<float>(static_cast<std::uint32_t>(settings.filter));
    if(settings.light_mask&&frame.draw_count){
        ShadowBounds bounds{};bool first=true;
        for(UINT j=0;j<frame.draw_count;++j){const auto& draw=frame.draws[j];
            if(!draw.casts_shadow&&!draw.receives_shadow)continue;
            const auto& box=p_->meshes.at(draw.mesh)->bounds;
            for(int corner=0;corner<8;++corner)for(int axis=0;axis<3;++axis){
                double value=draw.local_to_world[axis*4+3];
                for(int col=0;col<3;++col)value+=double(draw.local_to_world[axis*4+col])*
                    ((corner&(1<<col))?box.high[col]:box.low[col]);
                if(!std::isfinite(value)||std::fabs(value)>std::numeric_limits<float>::max())return bad;
                const auto f=static_cast<float>(value);
                if(first){bounds.low[axis]=bounds.high[axis]=f;}
                else{bounds.low[axis]=std::min(bounds.low[axis],f);bounds.high[axis]=std::max(bounds.high[axis],f);}
                if(axis==2)first=false;
            }
        }
        for(UINT j=0;j<frame.light_count;++j)if(!first&&(settings.light_mask&(1u<<j))){
            if(!plan_shadow_light(frame.lights[j],bounds,plans[j]))return bad;
            shadow_bases[j]=shadow_slices+1;
            for(UINT face=0;face<plans[j].count;++face)
                std::memcpy(shadow_data.world_to_clip[shadow_slices++],plans[j].world_to_clip[face],sizeof(float)*16);
        }
        if(std::uint64_t(settings.resolution)*settings.resolution*shadow_slices*4>64ull*1024*1024)return bad;
    }
    // No shadow resource allocation or draw may clear the previous colour view
    // until validation and the complete required target allocation succeed.
    ID3D11ShaderResourceView* none=nullptr;p_->context->PSSetShaderResources(1,1,&none);
    if(shadow_slices){
        if(!p_->shadows||p_->shadows->resolution()!=settings.resolution||p_->shadows->slices()!=shadow_slices){
            auto target=std::make_unique<ShadowTarget>();hr=target->initialize(p_->device,settings.resolution,shadow_slices);
            if(FAILED(hr))return hr;
            p_->shadows=std::move(target);
        }
        p_->context->IASetInputLayout(p_->layout);
        p_->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        p_->context->VSSetShader(p_->vs,nullptr,0);p_->context->PSSetShader(p_->shadow_ps,nullptr,0);
        p_->context->VSSetConstantBuffers(0,1,&p_->constants);p_->context->PSSetConstantBuffers(0,1,&p_->constants);
        p_->context->PSSetSamplers(0,1,&p_->sampler);
        D3D11_VIEWPORT shadow_viewport{};shadow_viewport.Width=shadow_viewport.Height=static_cast<float>(settings.resolution);
        shadow_viewport.MaxDepth=1;p_->context->RSSetViewports(1,&shadow_viewport);
        p_->context->OMSetDepthStencilState(p_->depth_write,0);
        const float factor[4]{};p_->context->OMSetBlendState(p_->opaque,factor,0xffffffffu);
        for(UINT slice=0;slice<shadow_slices;++slice){
            auto* depth_view=p_->shadows->depth_view(slice);p_->context->OMSetRenderTargets(0,nullptr,depth_view);
            p_->context->ClearDepthStencilView(depth_view,D3D11_CLEAR_DEPTH,1,0);
            for(UINT j=0;j<frame.draw_count;++j){const auto& draw=frame.draws[j];
                if(!draw.casts_shadow||(draw.flags&(alpha_blend|additive)))continue; // generic translucent materials do not cast solid silhouettes
                const auto& mesh=*p_->meshes.at(draw.mesh);const auto& image=*p_->images.at(draw.texture);
                Constants c{};std::memcpy(c.world_to_clip,shadow_data.world_to_clip[slice],sizeof(c.world_to_clip));
                std::memcpy(c.local_to_world,draw.local_to_world,sizeof(c.local_to_world));normal_matrix(draw.local_to_world,c.normal_to_world);
                std::memcpy(c.tint,draw.colour,sizeof(c.tint));c.flags=draw.flags;c.alpha_cutoff=draw.alpha_cutoff;
                for(unsigned row=0;row<2;++row)for(unsigned col=0;col<3;++col)c.uv_rows[row*4+col]=draw.uv_transform[row*3+col];
                c.texture_alpha=static_cast<UINT>(image.alpha);
                p_->context->UpdateSubresource(p_->constants,0,nullptr,&c,0,0);
                const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vertices=mesh.vertices;
                p_->context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);p_->context->IASetIndexBuffer(mesh.indices,DXGI_FORMAT_R32_UINT,0);
                p_->context->PSSetShaderResources(0,1,&image.view);
                auto* selected_sampler=p_->sampler_for(draw.sampler);p_->context->PSSetSamplers(0,1,&selected_sampler);
                p_->context->RSSetState((draw.flags&two_sided)?p_->two_sided:(mirrored(draw.local_to_world)?p_->mirror_cull:p_->cull));
                p_->context->DrawIndexed(mesh.count,0,0);
            }
        }
        hr=p_->device->GetDeviceRemovedReason();if(FAILED(hr))return hr;
    }else p_->shadows.reset();
    ID3D11RenderTargetView* rtv=p_->rtv;p_->context->OMSetRenderTargets(1,&rtv,p_->dsv);
    if(frame.clear_colour_target)p_->context->ClearRenderTargetView(rtv,frame.clear_colour);
    if(frame.clear_depth_target)p_->context->ClearDepthStencilView(p_->dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX=full_view?0:frame.viewport[0];viewport.TopLeftY=full_view?0:frame.viewport[1];
    viewport.Width=full_view?static_cast<float>(p_->width):frame.viewport[2];
    viewport.Height=full_view?static_cast<float>(p_->height):frame.viewport[3];
    viewport.MaxDepth=1;p_->context->RSSetViewports(1,&viewport);
    p_->context->IASetInputLayout(p_->layout);
    p_->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    p_->context->VSSetShader(p_->vs,nullptr,0);p_->context->PSSetShader(p_->ps,nullptr,0);
    p_->context->VSSetConstantBuffers(0,1,&p_->constants);p_->context->PSSetConstantBuffers(0,1,&p_->constants);
    p_->context->PSSetSamplers(0,1,&p_->sampler);
    p_->context->PSSetConstantBuffers(1,1,&p_->shadow_constants);
    p_->context->UpdateSubresource(p_->shadow_constants,0,nullptr,&shadow_data,0,0);
    ID3D11ShaderResourceView* shadow_view=p_->shadows?p_->shadows->view():nullptr;
    p_->context->PSSetShaderResources(1,1,&shadow_view);
    for(const auto& item:colour_order){const auto& d=frame.draws[item.index];
        const auto& mesh=*p_->meshes.at(d.mesh);const auto& image=*p_->images.at(d.texture);
        Constants c{};std::memcpy(c.world_to_clip,frame.world_to_clip,sizeof(c.world_to_clip));
        std::memcpy(c.local_to_world,d.local_to_world,sizeof(c.local_to_world));
        normal_matrix(d.local_to_world,c.normal_to_world);
        std::memcpy(c.tint,d.colour,sizeof(d.colour));
        std::memcpy(c.emission,d.emissive,sizeof(d.emissive));
        std::memcpy(c.ambient,frame.ambient,sizeof(frame.ambient));
        for(unsigned row=0;row<2;++row)for(unsigned col=0;col<3;++col)c.uv_rows[row*4+col]=d.uv_transform[row*3+col];
        std::copy(frame.fog.colour,frame.fog.colour+3,c.fog_colour);
        std::copy(frame.fog.camera_position,frame.fog.camera_position+3,c.camera_position);
        c.fog_range[0]=frame.fog.start;c.fog_range[1]=frame.fog.end;c.fog_range[2]=frame.fog.enabled?1.f:0.f;
        c.light_count=frame.light_count;c.flags=d.flags|(d.receives_shadow?0u:16u)|(d.fog_enabled?0u:32u);c.alpha_cutoff=d.alpha_cutoff;c.texture_alpha=static_cast<UINT>(image.alpha);
        for(UINT k=0;k<frame.light_count;++k){const auto& l=frame.lights[k];
            std::memcpy(c.pos_type[k],l.position,sizeof(l.position));c.pos_type[k][3]=static_cast<float>(static_cast<UINT>(l.type));
            std::memcpy(c.dir_intensity[k],l.direction,sizeof(l.direction));c.dir_intensity[k][3]=l.intensity;
            std::memcpy(c.colour_start[k],l.colour,sizeof(l.colour));c.colour_start[k][3]=l.falloff_start;
            c.range_cone[k][0]=l.falloff_range;c.range_cone[k][1]=l.cone_inner_degrees;
            c.range_cone[k][2]=l.cone_outer_degrees;c.range_cone[k][3]=static_cast<float>(shadow_bases[k]);
        }
        p_->context->UpdateSubresource(p_->constants,0,nullptr,&c,0,0);
        const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vb=mesh.vertices;
        p_->context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        p_->context->IASetIndexBuffer(mesh.indices,DXGI_FORMAT_R32_UINT,0);
        p_->context->PSSetShaderResources(0,1,&image.view);
        auto* selected_sampler=p_->sampler_for(d.sampler);p_->context->PSSetSamplers(0,1,&selected_sampler);
        p_->context->RSSetState((d.flags&two_sided)?p_->two_sided:(mirrored(d.local_to_world)?p_->mirror_cull:p_->cull));
        p_->context->OMSetDepthStencilState(d.depth_mode==DepthMode::disabled?p_->depth_none:
            (d.depth_mode==DepthMode::read_only||(d.flags&(alpha_blend|additive)))?p_->depth_read:p_->depth_write,0);
        const float factor[4]{};p_->context->OMSetBlendState((d.flags&additive)?p_->additive:
            (d.flags&alpha_blend)?p_->alpha:p_->opaque,factor,0xffffffffu);
        p_->context->DrawIndexed(mesh.count,0,0);
    }
    hr=p_->device->GetDeviceRemovedReason();if(FAILED(hr))return hr;
    hr=present?p_->swap->Present(0,0):S_OK;
    if(SUCCEEDED(hr)){p_->colour_valid=true;p_->depth_valid=true;
        p_->presentation_image=p_->offscreen&&p_->presentation_swap&&
            p_->width==p_->presentation_width&&p_->height==p_->presentation_height;}
    return hr;
}
HRESULT Renderer::render_status()const{return p_->guard();}
HRESULT Renderer::readback(std::vector<std::uint8_t>& bgra,std::uint32_t& width,std::uint32_t& height){
    HRESULT hr=p_->guard();if(FAILED(hr))return hr;
    // Allocate before acquiring/mapping a temporary GPU resource.
    std::vector<std::uint8_t> result(static_cast<std::size_t>(p_->width)*p_->height*4);
    D3D11_TEXTURE2D_DESC d{};p_->back->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;
    d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
    if(!p_->readback_staging){
        if(p_->readback_staging_creations==UINT64_MAX)return E_OUTOFMEMORY;
        ID3D11Texture2D* candidate=nullptr;
        hr=p_->device->CreateTexture2D(&d,nullptr,&candidate);if(FAILED(hr))return hr;
        p_->readback_staging=candidate;++p_->readback_staging_creations;
    }
    auto* staging=p_->readback_staging;
    p_->context->CopyResource(staging,p_->back);
    D3D11_MAPPED_SUBRESOURCE map{};hr=p_->context->Map(staging,0,D3D11_MAP_READ,0,&map);
    if(FAILED(hr)){drop(p_->readback_staging);return hr;}
    if(!map.pData||map.RowPitch<std::uint64_t(p_->width)*4){p_->context->Unmap(staging,0);drop(p_->readback_staging);return E_FAIL;}
    for(UINT y=0;y<p_->height;++y)std::memcpy(result.data()+static_cast<std::size_t>(y)*p_->width*4,
        static_cast<const std::uint8_t*>(map.pData)+static_cast<std::size_t>(y)*map.RowPitch,p_->width*4);
    p_->context->Unmap(staging,0);
    bgra=std::move(result);width=p_->width;height=p_->height;return S_OK;
}
HRESULT Renderer::resize(std::uint32_t width,std::uint32_t height){
    // A prior target-creation failure must not make resize permanently
    // unreachable. Device/context/swap and thread remain authoritative.
    if(!p_->device||!p_->context||(!p_->offscreen&&!p_->swap)||
       !(p_->offscreen?p_->thread==GetCurrentThreadId():own(p_->hwnd,p_->thread)))return E_UNEXPECTED;
    p_->presentation_image=false;
    HRESULT hr=p_->device->GetDeviceRemovedReason();if(FAILED(hr))return hr;
    if(!width||!height||width>max_dim||height>max_dim||std::uint64_t(width)*height*8>resource_budget)return bad;
    if(width==p_->width&&height==p_->height&&p_->rtv&&p_->dsv)return S_OK;
    p_->context->OMSetRenderTargets(0,nullptr,nullptr);p_->release_targets();
    if(p_->offscreen){
        p_->width=width;p_->height=height;hr=p_->targets();
        if(FAILED(hr))p_->release_targets();
        return hr;
    }
    hr=p_->swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
    if(FAILED(hr)){
        // DXGI retains the old buffers on failure. Reacquire them when
        // possible, report the original failure, and still permit retry.
        p_->targets();return hr;
    }
    p_->width=width;p_->height=height;hr=p_->targets();
    if(FAILED(hr))p_->release_targets();
    return hr;
}
HRESULT Renderer::configure_presentation(HWND window,std::uint32_t width,std::uint32_t height){
    if(!p_->offscreen||FAILED(p_->resource_guard()))return E_UNEXPECTED;
    p_->presentation_image=false;
    if(!window){
        if(width||height)return bad;
        drop(p_->presentation_swap);p_->presentation_window=nullptr;
        p_->presentation_width=p_->presentation_height=0;return S_OK;
    }
    if(!own(window,p_->thread)||width!=p_->width||height!=p_->height||!width||!height)return bad;
    RECT rect{};
    if(!GetClientRect(window,&rect)||rect.left||rect.top||
       rect.right!=static_cast<LONG>(width)||rect.bottom!=static_cast<LONG>(height))return bad;
    HRESULT hr=p_->device->GetDeviceRemovedReason();if(FAILED(hr))return hr;
    if(p_->presentation_swap&&window==p_->presentation_window){
        if(width!=p_->presentation_width||height!=p_->presentation_height){
            hr=p_->presentation_swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
            if(FAILED(hr))return hr;
        }
        p_->presentation_width=width;p_->presentation_height=height;return S_OK;
    }
    IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIFactory* factory=nullptr;
    IDXGISwapChain* candidate=nullptr;
    hr=p_->device->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(&dxgi));
    if(SUCCEEDED(hr))hr=dxgi->GetAdapter(&adapter);
    if(SUCCEEDED(hr))hr=adapter->GetParent(__uuidof(IDXGIFactory),reinterpret_cast<void**>(&factory));
    if(SUCCEEDED(hr))hr=factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER|DXGI_MWA_NO_WINDOW_CHANGES);
    if(SUCCEEDED(hr)){
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=width;desc.BufferDesc.Height=height;
        desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;
        desc.OutputWindow=window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        hr=factory->CreateSwapChain(p_->device,&desc,&candidate);
    }
    drop(factory);drop(adapter);drop(dxgi);
    if(FAILED(hr)){drop(candidate);return hr;}
    drop(p_->presentation_swap);p_->presentation_swap=candidate;p_->presentation_window=window;
    p_->presentation_width=width;p_->presentation_height=height;return S_OK;
}
HRESULT Renderer::present(std::uint32_t sync_interval){
    const auto hr=p_->guard();if(FAILED(hr))return hr;
    const bool eligible=p_->presentation_image;p_->presentation_image=false;
    if(sync_interval>4)return bad;
    if(!p_->offscreen||!p_->presentation_swap||!eligible||
       !own(p_->presentation_window,p_->thread))return E_UNEXPECTED;
    // Every owner-thread attempt consumes the image, including invalid input,
    // lost windows and driver failures. Foreign threads never mutate it.
    RECT rect{};
    if(!GetClientRect(p_->presentation_window,&rect)||rect.left||rect.top||
       rect.right!=static_cast<LONG>(p_->width)||rect.bottom!=static_cast<LONG>(p_->height)||
       p_->width!=p_->presentation_width||p_->height!=p_->presentation_height)return bad;
    ID3D11Texture2D* target=nullptr;
    auto result=p_->presentation_swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&target));
    if(FAILED(result))return result;
    p_->context->OMSetRenderTargets(0,nullptr,nullptr);
    p_->context->CopyResource(target,p_->back);drop(target);
    result=p_->device->GetDeviceRemovedReason();if(FAILED(result))return result;
    result=p_->presentation_swap->Present(sync_interval,0);
    return result==DXGI_STATUS_OCCLUDED?S_FALSE:result;
}
void Renderer::shutdown(){
    // Monotonic identities and device epoch survive recreation; released slots
    // do not consume the resident resource limits or retain registry storage.
    p_->release_all();
}
} // namespace a2fo::native11
