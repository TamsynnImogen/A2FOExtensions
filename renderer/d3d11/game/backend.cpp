#include "contracts.hpp"
#include "../../../rust/game_backend_ffi.h"
#include "../../textures/upload.hpp"
#include "../../shaders/bytecode_cache.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <set>

namespace a2fo::renderer::game {
namespace {
template<class T> void release(T*& p) noexcept { if (p) { p->Release(); p=nullptr; } }
using CreateDeviceAndSwapChain = HRESULT (WINAPI *)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,
    const D3D_FEATURE_LEVEL*,UINT,UINT,const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**,
    ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
constexpr UINT max_dimension=D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
bool owner(HWND hwnd) {
    DWORD process=0;
    const DWORD thread=hwnd && IsWindow(hwnd) ? GetWindowThreadProcessId(hwnd,&process) : 0;
    return thread && thread==GetCurrentThreadId() && process==GetCurrentProcessId();
}
void report(Log log,const char* action,HRESULT hr,UINT w=0,UINT h=0) {
    if (!log) return;
    char message[192];
    std::snprintf(message,sizeof(message),"D3D11 game: %s HRESULT=0x%08lx dimensions=%ux%u",
                  action,static_cast<unsigned long>(hr),w,h);
    log(message);
}
std::shared_ptr<void> load_module() {
    HMODULE module=LoadLibraryA("d3d11.dll");
    return module ? std::shared_ptr<void>(module,[](void* p){FreeLibrary(static_cast<HMODULE>(p));}) : nullptr;
}
DXGI_FORMAT target_format(UINT legacy,bool depth) {
    switch(a2fo_backend_format(legacy,depth)) {
        case 1: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case 2: return DXGI_FORMAT_D16_UNORM;
        case 3: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case 4: return DXGI_FORMAT_D32_FLOAT;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}
bool same_device(ID3D11Device* expected,ID3D11Texture2D* image){
    if(!expected || !image)return false;
    ID3D11Device* actual=nullptr;image->GetDevice(&actual);
    const bool equal=actual==expected;release(actual);return equal;
}
struct MemoryInfo {
    DXGI_QUERY_VIDEO_MEMORY_INFO local{},nonlocal{};
    HRESULT local_status=E_FAIL,nonlocal_status=E_FAIL;
};
HRESULT query_memory(ID3D11Device* device,MemoryInfo& info){
    struct Context {
        ID3D11Device *device;MemoryInfo *info;
        IDXGIDevice *dxgi=nullptr;IDXGIAdapter *adapter=nullptr;
        IDXGIAdapter3 *adapter3=nullptr;std::exception_ptr exception;
    } context{device,&info,nullptr,nullptr,nullptr,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(step){
                case 0:*result=c.device->QueryInterface(__uuidof(IDXGIDevice),
                    reinterpret_cast<void**>(&c.dxgi));break;
                case 1:*result=c.dxgi->GetAdapter(&c.adapter);break;
                case 2:*result=c.adapter->QueryInterface(__uuidof(IDXGIAdapter3),
                    reinterpret_cast<void**>(&c.adapter3));break;
                case 3:c.info->local_status=c.adapter3->QueryVideoMemoryInfo(0,
                    DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&c.info->local);
                    *result=c.info->local_status;break;
                case 4:c.info->nonlocal_status=c.adapter3->QueryVideoMemoryInfo(0,
                    DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,&c.info->nonlocal);
                    *result=c.info->nonlocal_status;break;
                case 5:release(c.adapter3);release(c.adapter);release(c.dxgi);break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_query_memory_workflow(&context,device!=nullptr,
        E_UNEXPECTED,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
void report_memory_segment(Log log,const char* segment,HRESULT hr,const DXGI_QUERY_VIDEO_MEMORY_INFO& info){
    if(!log)return;
    char message[256];
    if(FAILED(hr))std::snprintf(message,sizeof(message),
        "D3D11 game: DXGI adapter process %s memory unknown HRESULT=0x%08lx",
        segment,static_cast<unsigned long>(hr));
    else std::snprintf(message,sizeof(message),
        "D3D11 game: DXGI adapter process %s budget=%llu usage=%llu available_for_reservation=%llu reservation=%llu bytes",
        segment,static_cast<unsigned long long>(info.Budget),
        static_cast<unsigned long long>(info.CurrentUsage),
        static_cast<unsigned long long>(info.AvailableForReservation),
        static_cast<unsigned long long>(info.CurrentReservation));
    log(message);
}
}

struct Target::Impl {
    std::shared_ptr<void> module;
    ID3D11Texture2D* image=nullptr;
    ID3D11RenderTargetView* color=nullptr;
    ID3D11DepthStencilView* depth=nullptr;
    UINT width=0,height=0;
    UINT subresource=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    bool has_stencil=false;
    ~Impl(){release(color);release(depth);release(image);}
};
Target::Target():impl_(std::make_unique<Impl>()){}
Target::~Target()=default;

struct Texture::Impl {
    std::shared_ptr<void> module;
    ID3D11Texture2D* image=nullptr;
    ID3D11ShaderResourceView* view=nullptr;
    UINT legacy=0,levels=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    std::vector<std::shared_ptr<Target>> targets;
    ~Impl(){targets.clear();release(view);release(image);}
};
Texture::Texture():impl_(std::make_unique<Impl>()){}
Texture::~Texture()=default;

struct Backend::Impl {
    std::shared_ptr<void> module;
    std::unique_ptr<DrawPipeline> pipeline;
    ID3D11Device* device=nullptr;
    ID3D11DeviceContext* context=nullptr;
    IDXGISwapChain* swap=nullptr;
    ID3D11VertexShader* clear_vs=nullptr;
    ID3D11PixelShader* clear_ps=nullptr;
    ID3D11Buffer* clear_constants=nullptr;
    ID3D11RasterizerState* clear_raster=nullptr;
    ID3D11BlendState* clear_blend[2]{};
    ID3D11DepthStencilState* clear_depth[4]{};
    std::shared_ptr<Target> back,depth,bound_color,bound_depth;
    HWND hwnd=nullptr;DWORD thread=0;
    UINT width=0,height=0,depth_format=75;
    bool vsync=false,device_lost=false;
    mutable bool memory_unknown_logged=false;
    std::set<HRESULT> draw_errors;
    Log log=nullptr;
    std::string cache;
    ~Impl(){
        pipeline.reset();
        if(context)context->ClearState();
        for(auto*& state:clear_depth)release(state);
        for(auto*& state:clear_blend)release(state);
        release(clear_raster);release(clear_constants);release(clear_ps);release(clear_vs);
        bound_color.reset();bound_depth.reset();back.reset();depth.reset();
        release(swap);release(context);release(device);
    }
    bool on_thread()const{return GetCurrentThreadId()==thread;}
    HRESULT status()const{
        const bool ready=device && context && swap;
        const auto plan=a2fo_backend_status_plan(device!=nullptr,context!=nullptr,
                                                swap!=nullptr,ready&&on_thread(),device_lost);
        if(plan==1)return E_UNEXPECTED;
        if(plan==2)return invalid_call;
        return plan==3?device->GetDeviceRemovedReason():S_OK;
    }
    HRESULT make_back(){
        struct Context {
            Impl *p;
            std::shared_ptr<Target> color,z;
            std::exception_ptr exception;
        } state{this,{},{},{}};
        const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
            auto& c=*static_cast<Context*>(opaque);
            auto *p=c.p;
            try {
                switch(step){
                    case 0:{
                        c.color=std::make_shared<Target>();c.color->impl_->module=p->module;
                        *result=p->swap->GetBuffer(0,__uuidof(ID3D11Texture2D),
                            reinterpret_cast<void**>(&c.color->impl_->image));
                        report(p->log,"GetBuffer",*result,p->width,p->height);
                        if(SUCCEEDED(*result)){
                            D3D11_TEXTURE2D_DESC desc{};c.color->impl_->image->GetDesc(&desc);
                            c.color->impl_->width=desc.Width;c.color->impl_->height=desc.Height;
                            c.color->impl_->format=desc.Format;
                        }
                        break;
                    }
                    case 1:
                        *result=p->device->CreateRenderTargetView(c.color->impl_->image,nullptr,
                                                                  &c.color->impl_->color);
                        report(p->log,"CreateRenderTargetView backbuffer",*result,p->width,p->height);
                        break;
                    case 2:{
                        c.z=std::make_shared<Target>();c.z->impl_->module=p->module;
                        c.z->impl_->width=p->width;c.z->impl_->height=p->height;
                        c.z->impl_->format=target_format(p->depth_format,true);
                        c.z->impl_->has_stencil=p->depth_format==75;
                        D3D11_TEXTURE2D_DESC zd{};zd.Width=p->width;zd.Height=p->height;
                        zd.MipLevels=1;zd.ArraySize=1;zd.Format=c.z->impl_->format;
                        zd.SampleDesc.Count=1;zd.Usage=D3D11_USAGE_DEFAULT;
                        zd.BindFlags=D3D11_BIND_DEPTH_STENCIL;
                        *result=p->device->CreateTexture2D(&zd,nullptr,&c.z->impl_->image);
                        report(p->log,"CreateTexture2D back depth",*result,p->width,p->height);
                        break;
                    }
                    case 3:
                        *result=p->device->CreateDepthStencilView(c.z->impl_->image,nullptr,
                                                                   &c.z->impl_->depth);
                        report(p->log,"CreateDepthStencilView back depth",*result,p->width,p->height);
                        break;
                    case 4:
                        p->back=c.color;p->depth=c.z;p->bound_color=c.color;p->bound_depth=c.z;
                        p->context->OMSetRenderTargets(1,&c.color->impl_->color,
                                                       c.z?c.z->impl_->depth:nullptr);
                        break;
                }
            } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
        };
        bool threw=false;
        const auto result=a2fo_backend_make_back_workflow(&state,depth_format!=0,native,&threw);
        if(threw)std::rethrow_exception(state.exception);
        return result;
    }
    HRESULT initialize_clear(){
        static const char* vs_source=R"hlsl(
float4 VSMain(uint id:SV_VertexID):SV_Position {
    float2 position=id==0?float2(-1,-1):id==1?float2(3,-1):float2(-1,3);
    return float4(position,0,1);
})hlsl";
        static const char* ps_source=R"hlsl(
cbuffer ClearConstants:register(b0) { float4 colour;float depth;float3 padding; };
struct Output {float4 colour:SV_Target0;float depth:SV_Depth;};
Output PSMain() {Output result;result.colour=colour;result.depth=depth;return result;}
)hlsl";
        struct Context {Impl *p;ShaderCompileResult shader;std::exception_ptr exception;};
        Context state{this,{},{}};
        const auto native=[](void *opaque,UINT step,UINT index,std::int32_t *result,bool *threw){
            auto& c=*static_cast<Context*>(opaque);
            auto *p=c.p;
            try {
                switch(step){
                    case 0:
                        *result=compile_shader({vs_source,"VSMain","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS},p->cache,c.shader);
                        if(FAILED(*result)&&p->log)p->log("D3D11 game clear VS: "+c.shader.diagnostics);
                        break;
                    case 1:
                        *result=p->device->CreateVertexShader(c.shader.bytes.data(),c.shader.bytes.size(),nullptr,&p->clear_vs);
                        break;
                    case 2:
                        *result=compile_shader({ps_source,"PSMain","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS},p->cache,c.shader);
                        if(FAILED(*result)&&p->log)p->log("D3D11 game clear PS: "+c.shader.diagnostics);
                        break;
                    case 3:
                        *result=p->device->CreatePixelShader(c.shader.bytes.data(),c.shader.bytes.size(),nullptr,&p->clear_ps);
                        break;
                    case 4:{
                        D3D11_BUFFER_DESC cb{};cb.ByteWidth=32;cb.Usage=D3D11_USAGE_DEFAULT;
                        cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
                        *result=p->device->CreateBuffer(&cb,nullptr,&p->clear_constants);break;
                    }
                    case 5:{
                        D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;
                        raster.CullMode=D3D11_CULL_NONE;raster.ScissorEnable=TRUE;raster.DepthClipEnable=TRUE;
                        *result=p->device->CreateRasterizerState(&raster,&p->clear_raster);break;
                    }
                    case 6:{
                        D3D11_BLEND_DESC blend{};
                        blend.RenderTarget[0].SrcBlend=D3D11_BLEND_ONE;
                        blend.RenderTarget[0].DestBlend=D3D11_BLEND_ZERO;
                        blend.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
                        blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
                        blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
                        blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
                        blend.RenderTarget[0].RenderTargetWriteMask=index?D3D11_COLOR_WRITE_ENABLE_ALL:0;
                        *result=p->device->CreateBlendState(&blend,&p->clear_blend[index]);break;
                    }
                    case 7:{
                        D3D11_DEPTH_STENCIL_DESC ds{};
                        ds.DepthEnable=(index&1)!=0;ds.DepthWriteMask=(index&1)?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
                        ds.DepthFunc=D3D11_COMPARISON_ALWAYS;ds.StencilEnable=(index&2)!=0;
                        ds.StencilReadMask=0xff;ds.StencilWriteMask=0xff;
                        ds.FrontFace.StencilFailOp=D3D11_STENCIL_OP_KEEP;
                        ds.FrontFace.StencilDepthFailOp=D3D11_STENCIL_OP_KEEP;
                        ds.FrontFace.StencilPassOp=D3D11_STENCIL_OP_REPLACE;
                        ds.FrontFace.StencilFunc=D3D11_COMPARISON_ALWAYS;ds.BackFace=ds.FrontFace;
                        *result=p->device->CreateDepthStencilState(&ds,&p->clear_depth[index]);break;
                    }
                    case 8:
                        for(auto*& item:p->clear_depth)release(item);
                        for(auto*& item:p->clear_blend)release(item);
                        release(p->clear_raster);release(p->clear_constants);
                        release(p->clear_ps);release(p->clear_vs);break;
                }
            } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
        };
        bool threw=false;
        const auto result=a2fo_backend_clear_setup_workflow(&state,clear_depth[3]!=nullptr,native,&threw);
        if(threw)std::rethrow_exception(state.exception);
        return result;
    }
    HRESULT clear_areas(const std::vector<RECT>& areas,DWORD flags,DWORD argb,float z,DWORD stencil){
        HRESULT hr=initialize_clear();
        if(FAILED(hr)){report(log,"initialize partial clear shaders/states",hr,width,height);return hr;}
        const float constants[8]={((argb>>16)&255)/255.f,((argb>>8)&255)/255.f,
            (argb&255)/255.f,((argb>>24)&255)/255.f,z,0,0,0};
        context->UpdateSubresource(clear_constants,0,nullptr,constants,0,0);
        ID3D11RenderTargetView* color=bound_color->impl_->color;
        ID3D11DepthStencilView* depth_view=bound_depth?bound_depth->impl_->depth:nullptr;
        context->OMSetRenderTargets(1,&color,depth_view);
        const float blend_factor[4]{};
        context->OMSetBlendState(clear_blend[(flags&1)!=0],blend_factor,0xffffffffu);
        context->OMSetDepthStencilState(clear_depth[((flags&2)!=0)|(((flags&4)!=0)<<1)],stencil&0xff);
        D3D11_VIEWPORT viewport{};viewport.Width=static_cast<float>(bound_color->impl_->width);
        viewport.Height=static_cast<float>(bound_color->impl_->height);viewport.MaxDepth=1;
        context->RSSetViewports(1,&viewport);context->RSSetState(clear_raster);
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11Buffer* no_vertex=nullptr;const UINT zero=0;
        context->IASetVertexBuffers(0,1,&no_vertex,&zero,&zero);
        context->IASetIndexBuffer(nullptr,DXGI_FORMAT_UNKNOWN,0);
        context->VSSetShader(clear_vs,nullptr,0);context->PSSetShader(clear_ps,nullptr,0);
        context->HSSetShader(nullptr,nullptr,0);context->DSSetShader(nullptr,nullptr,0);context->GSSetShader(nullptr,nullptr,0);
        context->PSSetConstantBuffers(0,1,&clear_constants);
        struct Areas {Impl *p;const std::vector<RECT> *areas;std::exception_ptr exception;};
        Areas items{this,&areas,{}};
        const auto draw_area=[](void *opaque,UINT index)->bool {
            auto& c=*static_cast<Areas*>(opaque);
            try {
                const auto& area=(*c.areas)[index];
                const D3D11_RECT scissor{area.left,area.top,area.right,area.bottom};
                c.p->context->RSSetScissorRects(1,&scissor);c.p->context->Draw(3,0);
                return true;
            } catch (...) {c.exception=std::current_exception();return false;}
        };
        a2fo_backend_each(&items,areas.size(),draw_area);
        if(items.exception)std::rethrow_exception(items.exception);
        return S_OK;
    }
};
Backend::Backend():impl_(std::make_unique<Impl>()){}
Backend::~Backend()=default;

HRESULT Backend::initialize(HWND hwnd,UINT width,UINT height,bool windowed,bool vsync,bool debug,
                            const std::string& cache,Log log,UINT depth_format){
    struct Context {
        Backend *backend;
        HWND hwnd;
        UINT width,height,depth_format;
        bool windowed,vsync,debug;
        const std::string *cache;
        Log log;
        std::unique_ptr<Impl> p;
        CreateDeviceAndSwapChain create=nullptr;
        DXGI_SWAP_CHAIN_DESC desc{};
        D3D_FEATURE_LEVEL obtained{};
        std::exception_ptr exception;
    } context{this,hwnd,width,height,depth_format,windowed,vsync,debug,&cache,log,{},nullptr,{},{},{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(step){
                case 0:*result=owner(c.hwnd)?1:0;break;
                case 1:
                    c.p=std::make_unique<Impl>();c.p->hwnd=c.hwnd;
                    c.p->thread=GetCurrentThreadId();c.p->width=c.width;c.p->height=c.height;
                    c.p->depth_format=c.depth_format;c.p->vsync=c.vsync;
                    c.p->log=c.log;c.p->cache=*c.cache;break;
                case 2:
                    c.p->module=load_module();
                    if(!c.p->module){*result=HRESULT_FROM_WIN32(GetLastError());
                        report(c.log,"LoadLibrary d3d11.dll",*result,c.width,c.height);}
                    break;
                case 3:{
                    const FARPROC symbol=GetProcAddress(static_cast<HMODULE>(c.p->module.get()),
                                                        "D3D11CreateDeviceAndSwapChain");
                    static_assert(sizeof(c.create)==sizeof(symbol));
                    std::memcpy(&c.create,&symbol,sizeof(c.create));
                    if(!c.create){*result=HRESULT_FROM_WIN32(GetLastError());
                        report(c.log,"GetProcAddress D3D11CreateDeviceAndSwapChain",*result,c.width,c.height);}
                    break;
                }
                case 4:case 5:{
                    if(step==5){release(c.p->context);release(c.p->device);release(c.p->swap);}
                    c.desc={};c.desc.BufferDesc.Width=c.width;c.desc.BufferDesc.Height=c.height;
                    c.desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;c.desc.SampleDesc.Count=1;
                    c.desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;c.desc.BufferCount=1;
                    c.desc.OutputWindow=c.hwnd;c.desc.Windowed=c.windowed?TRUE:FALSE;
                    c.desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
                    const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
                    *result=c.create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
                        step==4&&c.debug?D3D11_CREATE_DEVICE_DEBUG:0,
                        levels,1,D3D11_SDK_VERSION,&c.desc,&c.p->swap,
                        &c.p->device,&c.obtained,&c.p->context);
                    report(c.log,step==4?"create hardware device/swapchain":
                        "create without debug layer",*result,c.width,c.height);
                    break;
                }
                case 6:
                    if(!c.p->device || !c.p->context || !c.p->swap ||
                       c.obtained<D3D_FEATURE_LEVEL_11_0){
                        report(c.log,"verify feature level 11",E_FAIL,c.width,c.height);
                        *result=E_FAIL;
                    }
                    break;
                case 7:{
                    IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIFactory* factory=nullptr;
                    HRESULT hr=c.p->device->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(&dxgi));
                    if(SUCCEEDED(hr))hr=dxgi->GetAdapter(&adapter);
                    if(SUCCEEDED(hr))hr=adapter->GetParent(__uuidof(IDXGIFactory),reinterpret_cast<void**>(&factory));
                    if(SUCCEEDED(hr))hr=factory->MakeWindowAssociation(c.hwnd,DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_ALT_ENTER);
                    report(c.log,"MakeWindowAssociation",hr,c.width,c.height);
                    release(factory);release(adapter);release(dxgi);
                    *result=hr;break;
                }
                case 8:*result=c.p->make_back();break;
                case 9:
                    c.p->pipeline=std::make_unique<DrawPipeline>();
                    *result=c.p->pipeline->initialize(c.p->device,c.p->context,*c.cache,c.log);
                    report(c.log,"DrawPipeline initialize",*result,c.width,c.height);
                    break;
                case 10:c.backend->impl_=std::move(c.p);break;
                case 11:report(c.log,"validate window/dimensions",E_INVALIDARG,c.width,c.height);break;
                case 12:report(c.log,"validate auto depth format",invalid_call,c.width,c.height);break;
                case 13:*result=c.p->module?1:0;break;
                case 14:*result=c.create?1:0;break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_initialize_workflow(&context,width,height,depth_format,
        max_dimension,debug,invalid_call,E_INVALIDARG,DXGI_ERROR_SDK_COMPONENT_MISSING,
        native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
HRESULT Backend::resize(UINT width,UINT height,bool windowed,UINT depth_format){
    struct Context {
        Impl *p;
        UINT width,height,depth_format,old_depth_format;
        bool windowed;
        std::exception_ptr exception;
    } context{impl_.get(),width,height,depth_format,impl_->depth_format,windowed,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        auto& p=*c.p;
        try {
            switch(step){
                case 0:*result=p.status();break;
                case 1:*result=owner(p.hwnd)?1:0;break;
                case 2:
                    p.context->ClearState();
                    p.bound_color.reset();p.bound_depth.reset();p.back.reset();p.depth.reset();
                    break;
                case 3:
                    *result=p.swap->SetFullscreenState(c.windowed?FALSE:TRUE,nullptr);
                    report(p.log,"SetFullscreenState",*result,c.width,c.height);
                    break;
                case 4:{
                    const HRESULT restore=p.make_back();
                    report(p.log,"restore targets after fullscreen failure",restore,p.width,p.height);
                    break;
                }
                case 5:
                    *result=p.swap->ResizeBuffers(1,c.width,c.height,DXGI_FORMAT_R8G8B8A8_UNORM,0);
                    report(p.log,"ResizeBuffers",*result,c.width,c.height);
                    break;
                case 6:{
                    const HRESULT restore=p.make_back();
                    report(p.log,"restore targets after resize failure",restore,p.width,p.height);
                    break;
                }
                case 7:p.width=c.width;p.height=c.height;p.depth_format=c.depth_format;break;
                case 8:*result=p.make_back();break;
                case 9:{
                    p.depth_format=c.old_depth_format;
                    const HRESULT restore=p.make_back();
                    report(p.log,"restore targets after backbuffer failure",restore,p.width,p.height);
                    break;
                }
                case 10:p.device_lost=true;break;
                case 11:report(p.log,"validate auto depth format for resize",invalid_call,c.width,c.height);break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_resize_workflow(&context,width,height,windowed,depth_format,
        max_dimension,invalid_call,E_INVALIDARG,DXGI_ERROR_DEVICE_REMOVED,
        DXGI_ERROR_DEVICE_RESET,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
HRESULT Backend::present(const RECT* source,const RECT* dest,HWND override_window,const RGNDATA* dirty){
    struct Context {Impl *p;HRESULT present_hr=0;std::exception_ptr exception;} context{impl_.get(),0,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:c.present_hr=c.p->swap->Present(c.p->vsync?1:0,0);*result=c.present_hr;break;
                case 2:report(c.p->log,"Present",c.present_hr,c.p->width,c.p->height);break;
                case 3:
                    c.p->device_lost=true;
                    report(c.p->log,"GetDeviceRemovedReason",c.p->device->GetDeviceRemovedReason(),
                           c.p->width,c.p->height);break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_present_workflow(&context,source!=nullptr,dest!=nullptr,
        override_window&&override_window!=impl_->hwnd,dirty!=nullptr,invalid_call,
        DXGI_ERROR_DEVICE_REMOVED,DXGI_ERROR_DEVICE_RESET,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
HRESULT Backend::cooperative()const{return impl_->status();}
UINT Backend::available_memory()const{
    HRESULT hr=impl_->status();
    MemoryInfo info;
    if(SUCCEEDED(hr))hr=query_memory(impl_->device,info);
    const auto plan=a2fo_backend_memory_workflow(hr,info.local_status,info.nonlocal_status,
        info.local.Budget,info.local.CurrentUsage,info.nonlocal.Budget,info.nonlocal.CurrentUsage,
        impl_->memory_unknown_logged,false);
    if(plan.report_flags&1){
        report(impl_->log,"DXGI adapter process memory unknown",hr,impl_->width,impl_->height);
        impl_->memory_unknown_logged=true;
    }
    if(plan.report_flags&2){
        report(impl_->log,"DXGI adapter process one memory segment unknown",
            FAILED(info.local_status)?info.local_status:info.nonlocal_status,
            impl_->width,impl_->height);
        impl_->memory_unknown_logged=true;
    }
    return plan.available;
}
void Backend::log_memory()const{
    if(!impl_->log)return;
    HRESULT hr=impl_->status();
    MemoryInfo info;
    if(SUCCEEDED(hr))hr=query_memory(impl_->device,info);
    const auto plan=a2fo_backend_memory_workflow(hr,info.local_status,info.nonlocal_status,
        info.local.Budget,info.local.CurrentUsage,info.nonlocal.Budget,info.nonlocal.CurrentUsage,
        impl_->memory_unknown_logged,true);
    if(plan.report_flags&1){
        report(impl_->log,"DXGI adapter process memory unknown",hr,impl_->width,impl_->height);
        impl_->memory_unknown_logged=true;
    }
    if(plan.report_flags&4)report_memory_segment(impl_->log,"local",info.local_status,info.local);
    if(plan.report_flags&8)report_memory_segment(impl_->log,"nonlocal",info.nonlocal_status,info.nonlocal);
    if(plan.report_flags&16)report_memory_segment(impl_->log,"local",info.local_status,info.local);
    if(plan.report_flags&32)report_memory_segment(impl_->log,"nonlocal",info.nonlocal_status,info.nonlocal);
    if(plan.report_flags&2)impl_->memory_unknown_logged=true;
}
ID3D11Device* Backend::device()const{return impl_->device;}
std::shared_ptr<Target> Backend::backbuffer()const{return impl_->back;}
std::shared_ptr<Target> Backend::depthbuffer()const{return impl_->depth;}

HRESULT Backend::create_target(UINT width,UINT height,UINT legacy,bool depth,std::shared_ptr<Target>& out){
    struct Context {
        Impl *p;UINT width,height,legacy;bool depth;
        std::shared_ptr<Target> *out,target;
        std::exception_ptr exception;
    } state{impl_.get(),width,height,legacy,depth,&out,{},{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:{
                    c.target=std::make_shared<Target>();auto& t=*c.target->impl_;
                    t.module=c.p->module;t.width=c.width;t.height=c.height;
                    t.format=target_format(c.legacy,c.depth);
                    t.has_stencil=c.depth && c.legacy==75;
                    D3D11_TEXTURE2D_DESC desc{};desc.Width=c.width;desc.Height=c.height;
                    desc.MipLevels=1;desc.ArraySize=1;desc.Format=t.format;
                    desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
                    desc.BindFlags=c.depth?D3D11_BIND_DEPTH_STENCIL:D3D11_BIND_RENDER_TARGET;
                    *result=c.p->device->CreateTexture2D(&desc,nullptr,&t.image);
                    report(c.p->log,"CreateTexture2D target",*result,c.width,c.height);break;
                }
                case 2:{
                    auto& t=*c.target->impl_;
                    *result=c.depth?c.p->device->CreateDepthStencilView(t.image,nullptr,&t.depth):
                        c.p->device->CreateRenderTargetView(t.image,nullptr,&t.color);
                    report(c.p->log,c.depth?"CreateDepthStencilView":"CreateRenderTargetView",
                           *result,c.width,c.height);break;
                }
                case 3:*c.out=std::move(c.target);break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_create_target_workflow(&state,width,height,legacy,depth,
        max_dimension,invalid_call,native,&threw);
    if(threw)std::rethrow_exception(state.exception);
    return result;
}
HRESULT Backend::bind_target(const std::shared_ptr<Target>& color,const std::shared_ptr<Target>& depth){
    struct Context {
        Impl *p;const std::shared_ptr<Target> *color,*depth;
        std::exception_ptr exception;
    } state{impl_.get(),&color,&depth,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        const auto& color=*c.color;const auto& depth=*c.depth;
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:*result=color->impl_->color && same_device(c.p->device,color->impl_->image)?1:0;break;
                case 2:*result=depth->impl_->depth && same_device(c.p->device,depth->impl_->image) &&
                    depth->impl_->width==color->impl_->width &&
                    depth->impl_->height==color->impl_->height?1:0;break;
                case 3:
                    c.p->bound_color=color;c.p->bound_depth=depth;
                    c.p->context->OMSetRenderTargets(1,&color->impl_->color,
                                                     depth?depth->impl_->depth:nullptr);
                    break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_bind_target_workflow(&state,color!=nullptr,depth!=nullptr,
        invalid_call,native,&threw);
    if(threw)std::rethrow_exception(state.exception);
    return result;
}
HRESULT Backend::copy_target(Target& source,const RECT* rects,UINT count,Target& destination,const POINT* points){
    struct Context {
        Impl *p;Target *source,*destination;
        const RECT *rects;const POINT *points;UINT count;
        UINT source_format=0,destination_format=0;
        std::vector<A2foBackendRegion> regions;
        std::exception_ptr exception;
    } context{impl_.get(),&source,&destination,rects,points,count,0,0,{},{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:*result=c.source->impl_->color && c.destination->impl_->color?1:0;break;
                case 2:*result=same_device(c.p->device,c.source->impl_->image)?1:0;break;
                case 3:
                    *result=same_device(c.p->device,c.destination->impl_->image)?1:0;
                    if(*result){c.source_format=c.source->impl_->format;
                                c.destination_format=c.destination->impl_->format;}
                    break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto admission=a2fo_backend_copy_admission(&context,&context.source_format,
        &context.destination_format,count,rects!=nullptr,invalid_call,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    if(FAILED(admission))return admission;
    const auto& from=*source.impl_;const auto& to=*destination.impl_;
    struct Input {const RECT *rects;const POINT *points;UINT count,width,height;};
    Input input{rects,points,count,from.width,from.height};
    const auto read=[](void *opaque,UINT i,A2foBackendRect *r,A2foBackendPoint *p) {
        const auto &source=*static_cast<Input*>(opaque);
        const RECT rect=source.count?source.rects[i]:RECT{0,0,
            static_cast<LONG>(source.width),static_cast<LONG>(source.height)};
        const POINT point=source.points?source.points[i]:POINT{rect.left,rect.top};
        *r={rect.left,rect.top,rect.right,rect.bottom};
        *p={point.x,point.y};
    };
    const UINT iterations=count?count:1;
    context.regions.resize(iterations);
    if(!a2fo_backend_copy_plan(&input,count,from.width,from.height,to.width,to.height,
        from.image==to.image && from.subresource==to.subresource,read,
        context.regions.data()))return invalid_call;
    const auto dispatch=[](void *opaque,UINT step,UINT index,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        auto& from=*c.source->impl_;auto& to=*c.destination->impl_;
        try {
            switch(step){
                case 0:c.p->context->OMSetRenderTargets(0,nullptr,nullptr);break;
                case 1:{ID3D11ShaderResourceView* no_views[8]{};
                    c.p->context->PSSetShaderResources(0,8,no_views);break;}
                case 2:{const auto& region=c.regions[index];
                    const D3D11_BOX box{region.left,region.top,0,region.right,region.bottom,1};
                    c.p->context->CopySubresourceRegion(to.image,to.subresource,region.x,region.y,0,
                                                         from.image,from.subresource,&box);break;}
                case 3:{
                    ID3D11RenderTargetView* color=c.p->bound_color?c.p->bound_color->impl_->color:nullptr;
                    ID3D11DepthStencilView* depth=c.p->bound_depth?c.p->bound_depth->impl_->depth:nullptr;
                    c.p->context->OMSetRenderTargets(color?1:0,color?&color:nullptr,depth);break;}
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    threw=false;
    const auto result=a2fo_backend_copy_dispatch(&context,iterations,dispatch,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}

HRESULT Backend::snapshot_depth(const std::shared_ptr<Target>& source,std::shared_ptr<Texture>& out){
    const HRESULT status=impl_->status();
    if(FAILED(status))return status;
    if(!source || !source->impl_->depth || !source->impl_->image ||
       !same_device(impl_->device,source->impl_->image))return invalid_call;

    const auto& from=*source->impl_;
    D3D11_TEXTURE2D_DESC desc{};
    from.image->GetDesc(&desc);
    DXGI_FORMAT storage=DXGI_FORMAT_UNKNOWN,view_format=DXGI_FORMAT_UNKNOWN;
    switch(desc.Format){
        case DXGI_FORMAT_D16_UNORM:
            storage=DXGI_FORMAT_R16_TYPELESS;view_format=DXGI_FORMAT_R16_UNORM;break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            storage=DXGI_FORMAT_R24G8_TYPELESS;view_format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;break;
        case DXGI_FORMAT_D32_FLOAT:
            storage=DXGI_FORMAT_R32_TYPELESS;view_format=DXGI_FORMAT_R32_FLOAT;break;
        default:return invalid_call;
    }
    // CopyResource needs identical subresources in a compatible format family.
    if(from.format!=desc.Format || from.subresource!=0 ||
       from.width!=desc.Width || from.height!=desc.Height ||
       desc.MipLevels!=1 || desc.ArraySize!=1 || desc.SampleDesc.Count!=1 ||
       desc.Usage!=D3D11_USAGE_DEFAULT || !(desc.BindFlags&D3D11_BIND_DEPTH_STENCIL) ||
       desc.CPUAccessFlags!=0)return invalid_call;

    auto snapshot=std::make_shared<Texture>();
    auto& t=*snapshot->impl_;
    // Zero upload levels makes update_texture/texture_target reject snapshots.
    t.module=impl_->module;t.levels=0;t.format=storage;
    desc.Format=storage;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags=0;desc.MiscFlags=0;
    HRESULT hr=impl_->device->CreateTexture2D(&desc,nullptr,&t.image);
    if(FAILED(hr)){report(impl_->log,"CreateTexture2D depth snapshot",hr,desc.Width,desc.Height);return hr;}
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format=view_format;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels=1;
    hr=impl_->device->CreateShaderResourceView(t.image,&srv,&t.view);
    if(FAILED(hr)){report(impl_->log,"CreateShaderResourceView depth snapshot",hr,desc.Width,desc.Height);return hr;}

    // CopyResource accepts a bound depth source; leaving OM untouched also
    // preserves any bound output-merger UAVs and their hidden counters.
    impl_->context->CopyResource(t.image,from.image);
    hr=impl_->device->GetDeviceRemovedReason();
    if(FAILED(hr)){report(impl_->log,"CopyResource depth snapshot device status",hr,desc.Width,desc.Height);return hr;}
    out=std::move(snapshot);
    return S_OK;
}

HRESULT Backend::clear(const State& state,UINT count,const RECT* rects,DWORD flags,DWORD argb,float z,DWORD stencil){
    HRESULT hr=impl_->status();if(FAILED(hr))return hr;
    if(!a2fo_backend_clear_admission(flags,count,rects!=nullptr,
        impl_->bound_color!=nullptr,impl_->bound_depth!=nullptr,
        impl_->bound_depth && impl_->bound_depth->impl_->has_stencil))return invalid_call;
    const auto& t=*impl_->bound_color->impl_;
    const auto bounds=a2fo_backend_clear_bounds(state.x,state.y,state.width,state.height,t.width,t.height);
    if(!a2fo_backend_clear_active(bounds,flags))return S_OK;
    struct ClearInput {const RECT *rects;};
    ClearInput input{rects};
    const auto read=[](void *opaque,UINT i,A2foBackendRect *out) {
        const auto &r=static_cast<ClearInput*>(opaque)->rects[i];
        *out={r.left,r.top,r.right,r.bottom};
    };
    std::vector<A2foBackendRect> planned(count?count:1);
    bool valid=false;
    const auto used=a2fo_backend_clear_plan(&input,bounds,count,read,planned.data(),&valid);
    std::vector<RECT> areas;
    areas.reserve(used);
    for(UINT i=0;i<used;++i){const auto &r=planned[i];areas.push_back({r.left,r.top,r.right,r.bottom});}
    if(!valid)return S_OK;
    const A2foBackendRect first{areas[0].left,areas[0].top,areas[0].right,areas[0].bottom};
    const bool whole=a2fo_backend_whole_clear(&first,areas.size(),t.width,t.height);
    struct Dispatch {
        Impl *p;const std::vector<RECT> *areas;
        DWORD flags,argb,stencil;float z;
        std::exception_ptr exception;
    } dispatch{impl_.get(),&areas,flags,argb,stencil,z,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Dispatch*>(opaque);
        try {
            switch(step){
                case 0:*result=c.p->clear_areas(*c.areas,c.flags,c.argb,c.z,c.stencil);break;
                case 1:{
                    const float color[]={((c.argb>>16)&255)/255.f,((c.argb>>8)&255)/255.f,
                        (c.argb&255)/255.f,((c.argb>>24)&255)/255.f};
                    c.p->context->ClearRenderTargetView(c.p->bound_color->impl_->color,color);
                    break;
                }
                case 2:
                    c.p->context->ClearDepthStencilView(c.p->bound_depth->impl_->depth,
                        ((c.flags&2u)?D3D11_CLEAR_DEPTH:0)|((c.flags&4u)?D3D11_CLEAR_STENCIL:0),
                        c.z,static_cast<UINT8>(c.stencil));
                    break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_clear_dispatch_workflow(&dispatch,whole,flags,native,&threw);
    if(threw)std::rethrow_exception(dispatch.exception);
    return result;
}

HRESULT Backend::create_texture(UINT legacy,const std::vector<MipUpload>& mips,std::shared_ptr<Texture>& out){
    HRESULT hr=impl_->status();if(FAILED(hr))return hr;
    if(!a2fo_backend_mip_count_valid(mips.size(),D3D11_REQ_MIP_LEVELS))return invalid_call;
    std::vector<compat::TextureUpload> prepared(mips.size());
    struct MipContext {
        UINT legacy;
        const std::vector<MipUpload> *mips;
        std::vector<compat::TextureUpload> *prepared;
        std::exception_ptr exception;
    } context{legacy,&mips,&prepared,{}};
    const auto prepare=[](void *opaque,UINT index,std::int32_t *result,
                          UINT *format,UINT *width,UINT *height,bool *threw) {
        auto &c=*static_cast<MipContext*>(opaque);
        const auto &m=(*c.mips)[index];
        try {
            *result=compat::prepare_texture_upload(c.legacy,m.width,m.height,
                m.bytes.data(),m.bytes.size(),m.pitch,(*c.prepared)[index]);
            *format=(*c.prepared)[index].format;*width=m.width;*height=m.height;
        } catch (...) {c.exception=std::current_exception();*threw=true;}
    };
    bool threw=false;
    hr=a2fo_backend_prepare_mips(&context,mips.size(),D3D11_REQ_MIP_LEVELS,
                                 prepare,invalid_call,E_OUTOFMEMORY,&threw);
    if(threw)std::rethrow_exception(context.exception);
    if(FAILED(hr))return hr;
    struct TextureContext {
        Impl *p;UINT legacy;
        const std::vector<MipUpload> *mips;
        const std::vector<compat::TextureUpload> *prepared;
        std::shared_ptr<Texture> *out,texture;
        std::exception_ptr exception;
    } texture_context{impl_.get(),legacy,&mips,&prepared,&out,{},{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<TextureContext*>(opaque);
        try {
            switch(step){
                case 0:{
                    c.texture=std::make_shared<Texture>();auto& t=*c.texture->impl_;
                    t.module=c.p->module;t.legacy=c.legacy;
                    t.levels=static_cast<UINT>(c.mips->size());
                    t.format=(*c.prepared)[0].format;t.targets.resize(t.levels);
                    D3D11_TEXTURE2D_DESC desc{};desc.Width=(*c.mips)[0].width;
                    desc.Height=(*c.mips)[0].height;desc.MipLevels=t.levels;desc.ArraySize=1;
                    desc.Format=t.format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
                    desc.BindFlags=a2fo_backend_texture_bind_flags(t.format,
                        D3D11_BIND_SHADER_RESOURCE,D3D11_BIND_RENDER_TARGET);
                    std::vector<D3D11_SUBRESOURCE_DATA> data(c.mips->size());
                    struct DataFill {TextureContext *context;std::vector<D3D11_SUBRESOURCE_DATA> *data;};
                    DataFill fill{&c,&data};
                    const auto each=[](void *opaque,UINT index)->bool {
                        auto& item=*static_cast<DataFill*>(opaque);
                        (*item.data)[index].pSysMem=(*item.context->prepared)[index].bytes.data();
                        (*item.data)[index].SysMemPitch=(*item.context->prepared)[index].row_pitch;
                        return true;
                    };
                    a2fo_backend_each(&fill,c.mips->size(),each);
                    *result=c.p->device->CreateTexture2D(&desc,data.data(),&t.image);
                    report(c.p->log,"CreateTexture2D texture",*result,desc.Width,desc.Height);break;
                }
                case 1:{
                    auto& t=*c.texture->impl_;
                    *result=c.p->device->CreateShaderResourceView(t.image,nullptr,&t.view);
                    report(c.p->log,"CreateShaderResourceView",*result,
                           (*c.mips)[0].width,(*c.mips)[0].height);break;
                }
                case 2:*c.out=std::move(c.texture);break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool texture_threw=false;
    const auto result=a2fo_backend_create_texture_workflow(&texture_context,native,&texture_threw);
    if(texture_threw)std::rethrow_exception(texture_context.exception);
    return result;
}
HRESULT Backend::update_texture(Texture& texture,UINT level,const MipUpload& mip){
    struct Context {
        Impl *p;Texture *texture;UINT level;const MipUpload *mip;
        compat::TextureUpload prepared;
        A2foBackendUpdateMeta meta{};
        std::exception_ptr exception;
    } context{impl_.get(),&texture,level,&mip,{},{},{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        auto& t=*c.texture->impl_;
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:*result=t.image && same_device(c.p->device,t.image) && c.level<t.levels?1:0;break;
                case 2:{
                    D3D11_TEXTURE2D_DESC desc{};t.image->GetDesc(&desc);
                    c.meta.desc_width=desc.Width;c.meta.desc_height=desc.Height;
                    c.meta.expected_format=t.format;break;
                }
                case 3:{
                    *result=compat::prepare_texture_upload(t.legacy,c.meta.upload_width,
                        c.meta.upload_height,c.mip->bytes.data(),
                        c.mip->bytes.size(),c.mip->pitch,c.prepared);
                    c.meta.prepared_format=c.prepared.format;break;
                }
                case 4:
                    c.p->context->UpdateSubresource(t.image,c.level,nullptr,
                        c.prepared.bytes.data(),c.prepared.row_pitch,0);break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_update_texture_workflow(&context,level,mip.width,mip.height,
        &context.meta,invalid_call,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
HRESULT Backend::texture_view(Texture& texture,ID3D11ShaderResourceView** out){
    struct Context {Impl *p;Texture *texture;ID3D11ShaderResourceView **out;
                    std::exception_ptr exception;} context{impl_.get(),&texture,out,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:*result=c.out && c.texture->impl_->view &&
                    same_device(c.p->device,c.texture->impl_->image)?1:0;break;
                case 2:*c.out=c.texture->impl_->view;(*c.out)->AddRef();break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_texture_view_workflow(&context,invalid_call,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
HRESULT Backend::texture_target(Texture& texture,std::shared_ptr<Target>& out,UINT level){
    struct Context {
        Impl *p;Texture *texture;std::shared_ptr<Target> *out,target;
        UINT level;std::exception_ptr exception;
    } context{impl_.get(),&texture,&out,{},level,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        auto& t=*c.texture->impl_;
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:*result=t.image && same_device(c.p->device,t.image) &&
                    t.format==DXGI_FORMAT_R8G8B8A8_UNORM && c.level<t.levels?1:0;break;
                case 2:*result=t.targets[c.level]?1:0;break;
                case 3:{
                    c.target=std::make_shared<Target>();auto& v=*c.target->impl_;
                    v.module=c.p->module;
                    D3D11_TEXTURE2D_DESC desc{};t.image->GetDesc(&desc);
                    v.width=std::max(1u,desc.Width>>c.level);
                    v.height=std::max(1u,desc.Height>>c.level);
                    v.subresource=c.level;v.format=desc.Format;
                    v.image=t.image;v.image->AddRef();
                    D3D11_RENDER_TARGET_VIEW_DESC view{};view.Format=t.format;
                    view.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
                    view.Texture2D.MipSlice=c.level;
                    *result=c.p->device->CreateRenderTargetView(v.image,&view,&v.color);
                    report(c.p->log,"CreateRenderTargetView texture",*result,v.width,v.height);
                    break;
                }
                case 4:t.targets[c.level]=std::move(c.target);break;
                case 5:*c.out=t.targets[c.level];break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_texture_target_workflow(&context,invalid_call,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
HRESULT Backend::draw(const State& state,const Draw& draw,const std::array<Sampling,8>& sampling){
    struct Context {
        Impl *p;const State *state;const Draw *draw;
        const std::array<Sampling,8> *sampling;
        D3D11_VIEWPORT viewport{};HRESULT draw_result=0;
        UINT target_width=0,target_height=0;
        std::exception_ptr exception;
    } context{impl_.get(),&state,&draw,&sampling,{},0,0,0,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:
                    *result=c.p->bound_color && c.p->pipeline?1:0;
                    if(*result){c.target_width=c.p->bound_color->impl_->width;
                                c.target_height=c.p->bound_color->impl_->height;}
                    break;
                case 2:{
                    ID3D11RenderTargetView* color=c.p->bound_color->impl_->color;
                    ID3D11DepthStencilView* depth=c.p->bound_depth?c.p->bound_depth->impl_->depth:nullptr;
                    c.p->context->OMSetRenderTargets(1,&color,depth);
                    c.viewport.TopLeftX=static_cast<float>(c.state->x);
                    c.viewport.TopLeftY=static_cast<float>(c.state->y);
                    c.viewport.Width=static_cast<float>(c.state->width);
                    c.viewport.Height=static_cast<float>(c.state->height);
                    c.viewport.MinDepth=c.state->min_z;c.viewport.MaxDepth=c.state->max_z;
                    break;
                }
                case 3:c.p->context->RSSetViewports(1,&c.viewport);break;
                case 4:c.draw_result=c.p->pipeline->draw(*c.state,*c.draw,*c.sampling);
                    *result=c.draw_result;break;
                case 5:
                    if(c.p->draw_errors.insert(c.draw_result).second)
                        report(c.p->log,"DrawPipeline draw (first occurrence of HRESULT)",
                               c.draw_result,c.p->bound_color->impl_->width,
                               c.p->bound_color->impl_->height);
                    break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_draw_workflow(&context,state.x,state.y,state.width,state.height,
        &context.target_width,&context.target_height,invalid_call,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
HRESULT Backend::read_target(Target& target,std::vector<std::uint8_t>& rgba,UINT& width,UINT& height){
    struct Context {
        Impl *p;Target *target;std::vector<std::uint8_t> *rgba;
        UINT *width,*height;
        D3D11_TEXTURE2D_DESC desc{};
        D3D11_MAPPED_SUBRESOURCE mapped{};
        ID3D11Texture2D* staging=nullptr;
        std::exception_ptr exception;
    } context{impl_.get(),&target,&rgba,&width,&height,{},{},nullptr,{}};
    const auto native=[](void *opaque,UINT step,std::int32_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        auto& t=*c.target->impl_;
        try {
            switch(step){
                case 0:*result=c.p->status();break;
                case 1:*result=t.image && same_device(c.p->device,t.image) &&
                    t.format==DXGI_FORMAT_R8G8B8A8_UNORM?1:0;break;
                case 2:
                    t.image->GetDesc(&c.desc);
                    *result=c.desc.SampleDesc.Count==1?1:0;break;
                case 3:
                    c.desc.Width=t.width;c.desc.Height=t.height;
                    c.desc.MipLevels=1;c.desc.ArraySize=1;c.desc.BindFlags=0;c.desc.MiscFlags=0;
                    c.desc.Usage=D3D11_USAGE_STAGING;c.desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                    *result=c.p->device->CreateTexture2D(&c.desc,nullptr,&c.staging);
                    report(c.p->log,"CreateTexture2D read staging",*result,c.desc.Width,c.desc.Height);
                    break;
                case 4:{
                    c.p->context->OMSetRenderTargets(0,nullptr,nullptr);
                    ID3D11ShaderResourceView* no_views[8]{};
                    c.p->context->PSSetShaderResources(0,8,no_views);
                    const D3D11_BOX box{0,0,0,t.width,t.height,1};
                    c.p->context->CopySubresourceRegion(c.staging,0,0,0,0,t.image,t.subresource,&box);
                    ID3D11RenderTargetView* color=c.p->bound_color?c.p->bound_color->impl_->color:nullptr;
                    ID3D11DepthStencilView* depth=c.p->bound_depth?c.p->bound_depth->impl_->depth:nullptr;
                    c.p->context->OMSetRenderTargets(color?1:0,color?&color:nullptr,depth);
                    break;
                }
                case 5:
                    *result=c.p->context->Map(c.staging,0,D3D11_MAP_READ,0,&c.mapped);
                    report(c.p->log,"Map read staging",*result,c.desc.Width,c.desc.Height);
                    break;
                case 6:{
                    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(c.desc.Width)*c.desc.Height*4);
                    a2fo_backend_copy_rows(bytes.data(),static_cast<const std::uint8_t*>(c.mapped.pData),
                        static_cast<std::size_t>(c.desc.Width)*4,c.mapped.RowPitch,c.desc.Height);
                    c.p->context->Unmap(c.staging,0);*c.rgba=std::move(bytes);
                    *c.width=c.desc.Width;*c.height=c.desc.Height;
                    break;
                }
                case 7:release(c.staging);break;
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;*result=E_OUTOFMEMORY;}
    };
    bool threw=false;
    const auto result=a2fo_backend_read_target_workflow(&context,invalid_call,native,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return result;
}
} // namespace a2fo::renderer::game
