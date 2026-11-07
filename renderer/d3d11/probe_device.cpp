#include "probe_device.hpp"
#include "ui_renderer.hpp"
#include "../../rust/d3d_resources_ffi.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <optional>

namespace a2fo::renderer {
namespace {
using CreateDeviceAndSwapChain = HRESULT (WINAPI *)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*,
    IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

template <typename T> void release(T*& object) noexcept {
    if (object) {object->Release();object=nullptr;}
}
constexpr float diagnostic_colour[4] = {0.08f,0.37f,0.82f,1.0f};
const char* report_name(std::uint32_t id) noexcept {
    switch(id) {
    case A2FO_D3D_REPORT_VALIDATE:return "validate window/dimensions";
    case A2FO_D3D_REPORT_LOAD:return "LoadLibrary d3d11.dll";
    case A2FO_D3D_REPORT_PROC:return "GetProcAddress D3D11CreateDeviceAndSwapChain";
    case A2FO_D3D_REPORT_CREATE_DEBUG:return "create hardware device/swapchain debug";
    case A2FO_D3D_REPORT_CREATE:return "create hardware device/swapchain";
    case A2FO_D3D_REPORT_CREATE_RETRY:return "create hardware device/swapchain without debug";
    case A2FO_D3D_REPORT_VERIFY:return "verify FL11_0 hardware device";
    case A2FO_D3D_REPORT_QUERY:return "QueryInterface IDXGIDevice";
    case A2FO_D3D_REPORT_ADAPTER:return "GetAdapter";
    case A2FO_D3D_REPORT_FACTORY:return "GetParent IDXGIFactory";
    case A2FO_D3D_REPORT_ASSOC:return "MakeWindowAssociation disable window changes/alt-enter";
    case A2FO_D3D_REPORT_BUFFER:return "GetBuffer";
    case A2FO_D3D_REPORT_RTV:return "CreateRenderTargetView";
    case A2FO_D3D_REPORT_DEPTH:return "CreateTexture2D D24S8";
    case A2FO_D3D_REPORT_DSV:return "CreateDepthStencilView";
    case A2FO_D3D_REPORT_BIND:return "bind targets and viewport";
    case A2FO_D3D_REPORT_UNINITIALIZED:return "present while uninitialized";
    case A2FO_D3D_REPORT_PRESENT_COLOUR:return "clear colour/depth and Present(0,0)";
    case A2FO_D3D_REPORT_REMOVED:return "GetDeviceRemovedReason";
    case A2FO_D3D_REPORT_TRIANGLE:return "SM5 transformed-FVF diagnostic triangle";
    case A2FO_D3D_REPORT_PRESENT_TRIANGLE:return "triangle Present(0,0)";
    default:return "unknown probe operation";
    }
}
A2FoD3DProbeParams params(UINT width,UINT height,bool debug) noexcept {
    return {width,height,debug?1u:0u,E_INVALIDARG,E_FAIL,E_UNEXPECTED,
        DXGI_ERROR_SDK_COMPONENT_MISSING,DXGI_ERROR_DEVICE_REMOVED,
        DXGI_ERROR_DEVICE_RESET,D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
        D3D_FEATURE_LEVEL_11_0,DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_D24_UNORM_S8_UINT,DXGI_USAGE_RENDER_TARGET_OUTPUT,
        DXGI_SWAP_EFFECT_DISCARD,D3D11_USAGE_DEFAULT,D3D11_BIND_DEPTH_STENCIL,
        D3D11_CREATE_DEVICE_DEBUG,DXGI_MWA_NO_WINDOW_CHANGES,DXGI_MWA_NO_ALT_ENTER};
}
std::uint32_t ready_bits(ID3D11Device* device,ID3D11DeviceContext* context,
    IDXGISwapChain* swap,ID3D11RenderTargetView* target,
    ID3D11DepthStencilView* depth) noexcept {
    return (device?1u:0u)|(context?2u:0u)|(swap?4u:0u)|
           (target?8u:0u)|(depth?16u:0u);
}
}
ProbeDevice::~ProbeDevice() {shutdown();}
void ProbeDevice::report(const char* operation,HRESULT result) const {
    if(!log_)return;
    char message[192];
    std::snprintf(message,sizeof(message),
        "D3D11 probe: %s HRESULT=0x%08lx dimensions=%ux%u",
        operation,static_cast<unsigned long>(result),width_,height_);
    log_(message);
}
HRESULT ProbeDevice::initialize(HWND window,UINT width,UINT height,
                                bool debug_layer,Log log) {
    struct Context {
        ProbeDevice* self;
        HWND window;
        UINT width,height;
        bool debug_layer;
        Log log;
        CreateDeviceAndSwapChain create=nullptr;
        IDXGIDevice* dxgi=nullptr;
        IDXGIAdapter* adapter=nullptr;
        IDXGIFactory* factory=nullptr;
        ID3D11Texture2D* back_buffer=nullptr;
        std::exception_ptr exception;
        ~Context(){release(factory);release(adapter);release(dxgi);release(back_buffer);}
    } c{this,window,width,height,debug_layer,log,nullptr,nullptr,nullptr,nullptr,
        nullptr,{}};
    auto call=[](void* raw,std::uint32_t operation,const void* argument,
                 A2FoD3DReply* reply) noexcept -> int {
        auto& c=*static_cast<Context*>(raw);
        auto* self=c.self;
        try {
            switch(operation) {
            case A2FO_D3D_PROBE_SHUTDOWN:self->shutdown();break;
            case A2FO_D3D_PROBE_SET_PARAMS:
                self->log_=c.log;self->width_=c.width;self->height_=c.height;break;
            case A2FO_D3D_PROBE_VALIDATE_WINDOW: {
                DWORD process_id=0;
                const DWORD owner=c.window&&IsWindow(c.window)
                    ?GetWindowThreadProcessId(c.window,&process_id):0;
                reply->value=owner;reply->handle=process_id;
                reply->value64=(std::uint64_t{GetCurrentProcessId()}<<32)|GetCurrentThreadId();
                break;
            }
            case A2FO_D3D_PROBE_REPORT: {
                const auto& request=*static_cast<const A2FoD3DProbeRequest*>(argument);
                self->report(report_name(request.id),request.hr);break;
            }
            case A2FO_D3D_PROBE_LOAD_LIBRARY:
                self->d3d11_=LoadLibraryA("d3d11.dll");
                reply->hr=self->d3d11_?S_OK:HRESULT_FROM_WIN32(GetLastError());break;
            case A2FO_D3D_PROBE_GET_PROC: {
                const FARPROC symbol=GetProcAddress(self->d3d11_,"D3D11CreateDeviceAndSwapChain");
                static_assert(sizeof(c.create)==sizeof(symbol));
                std::memcpy(&c.create,&symbol,sizeof(c.create));
                reply->hr=c.create?S_OK:HRESULT_FROM_WIN32(GetLastError());break;
            }
            case A2FO_D3D_PROBE_TICK:reply->value64=GetTickCount64();break;
            case A2FO_D3D_PROBE_CREATE_DEVICE: {
                const auto& plan=*static_cast<const A2FoD3DProbeRequest*>(argument);
                DXGI_SWAP_CHAIN_DESC description{};
                description.BufferDesc.Width=plan.width;
                description.BufferDesc.Height=plan.height;
                description.BufferDesc.Format=static_cast<DXGI_FORMAT>(plan.format);
                description.SampleDesc.Count=plan.sample_count;
                description.BufferUsage=plan.usage;
                description.BufferCount=plan.buffer_count;
                description.OutputWindow=c.window;
                description.Windowed=plan.windowed;
                description.SwapEffect=static_cast<DXGI_SWAP_EFFECT>(plan.swap_effect);
                const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
                D3D_FEATURE_LEVEL obtained{};
                reply->hr=c.create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,plan.flags,
                    levels,1,D3D11_SDK_VERSION,&description,&self->swap_chain_,
                    &self->device_,&obtained,&self->context_);
                reply->value=obtained;break;
            }
            case A2FO_D3D_PROBE_VERIFY_DEVICE:
                reply->value=(self->device_?1u:0u)|(self->context_?2u:0u)|
                    (self->swap_chain_?4u:0u);break;
            case A2FO_D3D_PROBE_RELEASE_CONTEXT:release(self->context_);break;
            case A2FO_D3D_PROBE_RELEASE_DEVICE:release(self->device_);break;
            case A2FO_D3D_PROBE_RELEASE_SWAP:release(self->swap_chain_);break;
            case A2FO_D3D_PROBE_DEBUG_LOG:
                if(self->log_)
                    self->log_("D3D11 probe: debug layer unavailable; retrying hardware without debug layer");
                break;
            case A2FO_D3D_PROBE_ELAPSED_LOG: {
                if(self->log_) {
                    const auto& request=*static_cast<const A2FoD3DProbeRequest*>(argument);
                    char message[160];
                    std::snprintf(message,sizeof(message),
                        "D3D11 probe: hardware creation elapsed=%llu ms HRESULT=0x%08lx dimensions=%ux%u",
                        static_cast<unsigned long long>(request.elapsed_ms),
                        static_cast<unsigned long>(request.hr),self->width_,self->height_);
                    self->log_(message);
                }break;
            }
            case A2FO_D3D_PROBE_QUERY_DXGI:
                reply->hr=self->device_->QueryInterface(__uuidof(IDXGIDevice),
                    reinterpret_cast<void**>(&c.dxgi));break;
            case A2FO_D3D_PROBE_GET_ADAPTER:reply->hr=c.dxgi->GetAdapter(&c.adapter);break;
            case A2FO_D3D_PROBE_GET_FACTORY:
                reply->hr=c.adapter->GetParent(__uuidof(IDXGIFactory),
                    reinterpret_cast<void**>(&c.factory));break;
            case A2FO_D3D_PROBE_MAKE_ASSOC: {
                const auto& request=*static_cast<const A2FoD3DProbeRequest*>(argument);
                reply->hr=c.factory->MakeWindowAssociation(c.window,request.flags);break;
            }
            case A2FO_D3D_PROBE_RELEASE_FACTORY:release(c.factory);break;
            case A2FO_D3D_PROBE_RELEASE_ADAPTER:release(c.adapter);break;
            case A2FO_D3D_PROBE_RELEASE_DXGI:release(c.dxgi);break;
            case A2FO_D3D_PROBE_GET_BUFFER:
                reply->hr=self->swap_chain_->GetBuffer(0,__uuidof(ID3D11Texture2D),
                    reinterpret_cast<void**>(&c.back_buffer));break;
            case A2FO_D3D_PROBE_CREATE_RTV:
                reply->hr=self->device_->CreateRenderTargetView(c.back_buffer,nullptr,
                    &self->render_target_);break;
            case A2FO_D3D_PROBE_RELEASE_BUFFER:release(c.back_buffer);break;
            case A2FO_D3D_PROBE_CREATE_DEPTH: {
                const auto& plan=*static_cast<const A2FoD3DProbeRequest*>(argument);
                D3D11_TEXTURE2D_DESC description{};
                description.Width=plan.width;description.Height=plan.height;
                description.MipLevels=plan.mip_levels;description.ArraySize=plan.array_size;
                description.Format=static_cast<DXGI_FORMAT>(plan.format);
                description.SampleDesc.Count=plan.sample_count;
                description.Usage=static_cast<D3D11_USAGE>(plan.usage);
                description.BindFlags=plan.bind;
                reply->hr=self->device_->CreateTexture2D(&description,nullptr,
                    &self->depth_texture_);break;
            }
            case A2FO_D3D_PROBE_CREATE_DSV:
                reply->hr=self->device_->CreateDepthStencilView(self->depth_texture_,
                    nullptr,&self->depth_view_);break;
            case A2FO_D3D_PROBE_BIND_TARGETS: {
                const auto& plan=*static_cast<const A2FoD3DProbeRequest*>(argument);
                self->context_->OMSetRenderTargets(1,&self->render_target_,self->depth_view_);
                D3D11_VIEWPORT viewport{};
                viewport.Width=plan.viewport_width;viewport.Height=plan.viewport_height;
                viewport.MaxDepth=1.0f;
                self->context_->RSSetViewports(1,&viewport);break;
            }
            default:return 0;
            }
            return 1;
        }catch(...){c.exception=std::current_exception();return 0;}
    };
    const auto config=params(width,height,debug_layer);
    const HRESULT result=a2fo_rs_d3d_probe_initialize(&config,&c,call);
    if(c.exception)std::rethrow_exception(c.exception);
    return result;
}
HRESULT ProbeDevice::present_test_colour() {
    struct Context {ProbeDevice* self;std::exception_ptr exception;} c{this,{}};
    auto call=[](void* raw,std::uint32_t operation,const void* argument,
                 A2FoD3DReply* reply) noexcept -> int {
        auto& c=*static_cast<Context*>(raw);auto* self=c.self;
        try {
            switch(operation) {
            case A2FO_D3D_PROBE_REPORT: {
                const auto& request=*static_cast<const A2FoD3DProbeRequest*>(argument);
                self->report(report_name(request.id),request.hr);break;
            }
            case A2FO_D3D_PROBE_CLEAR_TARGET:
                self->context_->ClearRenderTargetView(self->render_target_,diagnostic_colour);break;
            case A2FO_D3D_PROBE_CLEAR_DEPTH:
                self->context_->ClearDepthStencilView(self->depth_view_,
                    D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.0f,0);break;
            case A2FO_D3D_PROBE_PRESENT:reply->hr=self->swap_chain_->Present(0,0);break;
            case A2FO_D3D_PROBE_REMOVED_REASON:
                reply->hr=self->device_->GetDeviceRemovedReason();break;
            case A2FO_D3D_PROBE_SHUTDOWN:self->shutdown();break;
            default:return 0;
            }
            return 1;
        }catch(...){c.exception=std::current_exception();return 0;}
    };
    const auto config=params(width_,height_,false);
    const HRESULT result=a2fo_rs_d3d_probe_present_colour(&config,
        ready_bits(device_,context_,swap_chain_,render_target_,depth_view_),&c,call);
    if(c.exception)std::rethrow_exception(c.exception);
    return result;
}
HRESULT ProbeDevice::present_test_triangle(const std::string& shader_cache) {
    struct Context {ProbeDevice* self;std::optional<UiRenderer> renderer;
        const std::string& cache;std::exception_ptr exception;
    } c{this,{},shader_cache,{}};
    auto call=[](void* raw,std::uint32_t operation,const void* argument,
                 A2FoD3DReply* reply) noexcept -> int {
        auto& c=*static_cast<Context*>(raw);auto* self=c.self;
        try {
            switch(operation) {
            case A2FO_D3D_PROBE_REPORT: {
                const auto& request=*static_cast<const A2FoD3DProbeRequest*>(argument);
                self->report(report_name(request.id),request.hr);break;
            }
            case A2FO_D3D_PROBE_UI_INIT:
                c.renderer.emplace();
                reply->hr=c.renderer->initialize(self->device_,self->context_,c.cache,self->log_);break;
            case A2FO_D3D_PROBE_UI_TARGET:
                reply->hr=c.renderer->set_target(self->render_target_,self->depth_view_,
                    self->width_,self->height_);break;
            case A2FO_D3D_PROBE_UI_CLEAR: {
                const auto& plan=*static_cast<const A2FoD3DProbeRequest*>(argument);
                reply->hr=c.renderer->clear_color(plan.flags);break;
            }
            case A2FO_D3D_PROBE_UI_DRAW: {
                const auto& plan=*static_cast<const A2FoD3DTriangle*>(argument);
                struct Vertex {float x,y,z,rhw;DWORD color;};
                Vertex vertices[3];
                for(unsigned i=0;i<3;++i)vertices[i]={plan.vertices[i].x,
                    plan.vertices[i].y,plan.vertices[i].z,plan.vertices[i].rhw,
                    plan.vertices[i].color};
                UiDrawState state{};state.color_op=plan.color_op;
                state.alpha_op=plan.alpha_op;
                reply->hr=c.renderer->draw_up(plan.primitive,plan.primitive_count,
                    plan.fvf,vertices,sizeof(vertices),sizeof(Vertex),state);break;
            }
            case A2FO_D3D_PROBE_PRESENT:reply->hr=self->swap_chain_->Present(0,0);break;
            case A2FO_D3D_PROBE_REMOVED_REASON:
                reply->hr=self->device_->GetDeviceRemovedReason();break;
            case A2FO_D3D_PROBE_UI_SHUTDOWN:c.renderer->shutdown();break;
            case A2FO_D3D_PROBE_SHUTDOWN:self->shutdown();break;
            default:return 0;
            }
            return 1;
        }catch(...){c.exception=std::current_exception();return 0;}
    };
    const auto config=params(width_,height_,false);
    const HRESULT result=a2fo_rs_d3d_probe_present_triangle(&config,
        ready_bits(device_,context_,swap_chain_,render_target_,depth_view_),&c,call);
    if(c.exception)std::rethrow_exception(c.exception);
    return result;
}
void ProbeDevice::shutdown() noexcept {
    auto call=[](void* raw,std::uint32_t operation,const void*,
                 A2FoD3DReply*) noexcept -> int {
        auto* self=static_cast<ProbeDevice*>(raw);
        switch(operation) {
        case A2FO_D3D_PROBE_CLEAR_STATE:if(self->context_)self->context_->ClearState();break;
        case A2FO_D3D_PROBE_RELEASE_DSV:release(self->depth_view_);break;
        case A2FO_D3D_PROBE_RELEASE_DEPTH:release(self->depth_texture_);break;
        case A2FO_D3D_PROBE_RELEASE_RTV:release(self->render_target_);break;
        case A2FO_D3D_PROBE_RELEASE_SWAP:release(self->swap_chain_);break;
        case A2FO_D3D_PROBE_RELEASE_CONTEXT:release(self->context_);break;
        case A2FO_D3D_PROBE_RELEASE_DEVICE:release(self->device_);break;
        case A2FO_D3D_PROBE_FREE_LIBRARY:
            if(self->d3d11_){FreeLibrary(self->d3d11_);self->d3d11_=nullptr;}break;
        case A2FO_D3D_PROBE_RESET_MEMBERS:
            self->width_=self->height_=0;self->log_=nullptr;break;
        default:return 0;
        }
        return 1;
    };
    a2fo_rs_d3d_probe_shutdown(this,call);
}
bool ProbeDevice::ready() const noexcept {
    return a2fo_rs_d3d_probe_ready(
        ready_bits(device_,context_,swap_chain_,render_target_,depth_view_))!=0;
}
} // namespace a2fo::renderer
