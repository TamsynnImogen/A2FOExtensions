#include "../../sdk/include/a2fo_module_provider.hpp"
// Menu-only Refit host. All D3D/provider work belongs to one background thread;
// the native shell only exchanges requests and immutable CPU snapshots.
#include "menu_scene.hpp"
#include "../../rust/stage10_presentation_completion_ffi.h"
#include "../../rust/stage8_b_scene_ffi.h"
#include "scene_budget.hpp"
#include "scene_mailbox.hpp"
#include "scene_settings.hpp"
#include "scene_catalogue.hpp"
#include "scene_handover.hpp"
#include "scene_transition.hpp"
#include "mission_backdrop.hpp"
#include "../../sdk/include/a2fo_shell_canvas.h"
#include <d3d9.h>
#include <commctrl.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace a2fo::refit_menu::menu_scene {
namespace {
const A2FO_ModuleApi* api{};
std::atomic<bool> wanted{false};
std::atomic<DWORD> ui_thread{0};
std::atomic<bool> loading_seen{false};
std::atomic<std::uint64_t> menu_returns{0};
Mailbox exchange;
SceneHandover handover;
std::atomic<HANDLE> wake_event{nullptr}; // Retained until process exit; no loader-lock join.
std::atomic<HANDLE> worker_thread{nullptr}; // Process lifetime; diagnostic wait only outside shutdown.
bool started{}, unavailable{}; // UI thread only.
int selected{};
int configured_width{};
HWND presentation_window{};
std::map<HWND, UINT_PTR> windows;
std::map<HWND, bool> redraw_children;
std::shared_ptr<const Artwork> displayed;
std::uint64_t displayed_revision{};
std::map<HDC,std::shared_ptr<const Artwork>> pending_canvas;
bool canvas_completion_registered{};
constexpr UINT_PTR timer_id = 0xa2f05302;
std::vector<SceneView> views{{"main", "Main"}, {"federation", "Federation"},
    {"tutorials", "Starbase"}, {"klingon", "Klingon"}, {"borg", "Borg"}};
int choice_page{};
constexpr int page_size = 5;
constexpr std::uint64_t loading_lease_ms = 2000;


struct HostInit {
    std::uint32_t size, version;
    void* device;
    const char* resource_root;
    const char* const* roots;
    std::uint32_t root_count;
};
static_assert(sizeof(HostInit) == 24);
using Version = unsigned (__cdecl*)();
using Init = unsigned (__cdecl*)(const HostInit*);
using SetDevice = unsigned (__cdecl*)(void*);
using Draw = unsigned (__cdecl*)(const char*, unsigned, unsigned, unsigned, unsigned);

template<class T> T symbol(HMODULE module, const char* name) {
    const auto raw = GetProcAddress(module, name);
    T result{};
    static_assert(sizeof(result) == sizeof(raw));
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}
void wake() { if (const auto event = wake_event.load(std::memory_order_acquire)) SetEvent(event); }
bool claim_ui() {
    DWORD unowned = 0;
    const DWORD current = GetCurrentThreadId();
    return ui_thread.compare_exchange_strong(unowned, current) || unowned == current;
}
void active(bool value) { if (exchange.active(value)) wake(); }
void drain_logs() {
    auto messages=exchange.logs();
    a2fo_stage10_presentation_each(messages.begin(),messages.end(),[](const auto& message){
        if(api&&api->log)api->log("A2FORefitMenus",message.c_str());
    });
}

struct Context {
    std::string root;
    std::vector<std::string> roots;
    HINSTANCE instance{};
};

constexpr char handover_property[]="A2FORefitMenus.SceneHandover.v1";
constexpr UINT_PTR handover_erase_id=0xa2f05305;
std::uintptr_t handover_generation{}; // UI-thread only; properties vanish on native HWND destruction.
bool handover_window_matches(const SceneHandover::State& state,std::uintptr_t handle){
    const auto window=reinterpret_cast<HWND>(handle);DWORD process{};
    return window&&state.token&&IsWindow(window)&&
        reinterpret_cast<std::uintptr_t>(GetPropA(window,handover_property))==state.token&&
        GetWindowThreadProcessId(window,&process)==state.thread&&process==GetCurrentProcessId()&&
        (handle==state.parent||GetParent(window)==reinterpret_cast<HWND>(state.parent));
}
LRESULT CALLBACK handover_erase_proc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(message==WM_NCDESTROY){
        RemoveWindowSubclass(window,&handover_erase_proc,handover_erase_id);
        return DefSubclassProc(window,message,wp,lp);
    }
    try{
        struct EraseContext {
            HWND window;UINT message;WPARAM wp;
            std::optional<SceneHandover::State> state;ULONGLONG now{};
            std::optional<Mailbox::Snapshot> snapshot;
            std::shared_ptr<const Artwork> image;
            RECT rect{};int saved{},drawn{};
            BITMAPINFO info{};std::exception_ptr error{};
            EraseContext(HWND handle,UINT event,WPARAM value)
                :window(handle),message(event),wp(value){}
        } c(window,message,wp);
        const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
            auto& c=*static_cast<EraseContext*>(opaque);
            try{
                switch(id){
                case 700:return c.message==WM_ERASEBKGND;
                case 701:return c.wp!=0;
                case 702:return wanted.load();
                // Never wait for the worker's GDI draw while holding the handover lease.
                case 703:c.state.emplace();return handover.try_state(*c.state);
                case 704:return GetPropA(c.window,handover_property)!=nullptr;
                case 705:c.now=GetTickCount64();return 0;
                case 706:return c.state->active(c.now);
                case 707:return c.state->parent==reinterpret_cast<std::uintptr_t>(c.window);
                case 708:return handover_window_matches(*c.state,c.state->parent);
                case 709:c.snapshot.emplace(exchange.snapshot());return 0;
                case 710:c.image=transition_image(c.snapshot->image,c.state->overlay,
                    c.now>=c.state->started?c.now-c.state->started:0);return 0;
                case 711:return bool(c.image);
                case 712:return c.image->width>0;
                case 713:return c.image->height>0;
                case 714:return GetClientRect(c.window,&c.rect)!=FALSE;
                case 715:return c.rect.right>0;
                case 716:return c.rect.bottom>0;
                case 717:c.saved=SaveDC(reinterpret_cast<HDC>(c.wp));return c.saved!=0;
                case 718:c.info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                    c.info.bmiHeader.biWidth=c.image->width;
                    c.info.bmiHeader.biHeight=-c.image->height;
                    c.info.bmiHeader.biPlanes=1;c.info.bmiHeader.biBitCount=32;
                    c.info.bmiHeader.biCompression=BI_RGB;
                    SetStretchBltMode(reinterpret_cast<HDC>(c.wp),COLORONCOLOR);
                    c.drawn=StretchDIBits(reinterpret_cast<HDC>(c.wp),0,0,c.rect.right,
                        c.rect.bottom,0,0,c.image->width,c.image->height,
                        c.image->pixels.data(),&c.info,DIB_RGB_COLORS,SRCCOPY);return 0;
                case 719:return RestoreDC(reinterpret_cast<HDC>(c.wp),c.saved)!=FALSE;
                case 720:return c.drawn!=0&&c.drawn!=static_cast<int>(GDI_ERROR);
                default:return 0;
                }
            }catch(...){c.error=std::current_exception();return UINT64_MAX;}
        };
        const bool consumed=a2fo_rs_stage8_b_scene_erase(&c,op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        if(consumed)return 1;
    }catch(...){} // Native erase remains the fallback on allocation failure.
    return DefSubclassProc(window,message,wp,lp);
}
bool draw_handover_pixels(const SceneHandover::State& state,const Artwork& image){
    struct DrawContext {
        const SceneHandover::State& state;const Artwork& image;
        std::uintptr_t handle{};HWND window{};RECT client{};HDC dc{};
        BITMAPINFO info{};int saved{},drawn{};std::exception_ptr error{};
        DrawContext(const SceneHandover::State& value,const Artwork& frame)
            :state(value),image(frame){}
    } c(state,image);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<DrawContext*>(opaque);
        try{
            const auto source=[&](std::uint32_t which){
                return which==0?c.state.destination:which==1?c.state.source:c.state.parent;
            };
            switch(id){
            case 500:return handover_window_matches(c.state,c.state.parent);
            case 501:return c.state.launch;
            case 502:{const auto handle=source(arg);
                const auto window=reinterpret_cast<HWND>(handle);
                return handover_window_matches(c.state,handle)&&
                    IsWindowVisible(window)&&!IsIconic(window);}
            case 503:c.handle=source(arg);return 0;
            case 504:return c.handle!=0;
            case 505:return c.image.width>0;
            case 506:return c.image.height>0;
            case 507:c.window=reinterpret_cast<HWND>(c.handle);
                return GetClientRect(c.window,&c.client)!=FALSE;
            case 508:return c.client.right>=80;
            case 509:return c.client.bottom>=100;
            case 510:c.dc=GetDC(c.window);return c.dc!=nullptr;
            case 511:c.info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                c.info.bmiHeader.biWidth=c.image.width;
                c.info.bmiHeader.biHeight=-c.image.height;
                c.info.bmiHeader.biPlanes=1;c.info.bmiHeader.biBitCount=32;
                c.info.bmiHeader.biCompression=BI_RGB;
                c.saved=SaveDC(c.dc);return c.saved!=0;
            case 512:SetStretchBltMode(c.dc,COLORONCOLOR);
                c.drawn=StretchDIBits(c.dc,0,0,c.client.right,c.client.bottom,0,0,
                    c.image.width,c.image.height,c.image.pixels.data(),
                    &c.info,DIB_RGB_COLORS,SRCCOPY);return 0;
            case 513:return c.drawn!=0&&c.drawn!=static_cast<int>(GDI_ERROR);
            case 514:return GdiFlush()!=FALSE;
            case 515:RestoreDC(c.dc,c.saved);return 0;
            case 516:ReleaseDC(c.window,c.dc);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_scene_draw_handover(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
// Private injection point for the isolated fixture's memory-DC sink. The shipped
// module always uses the validated same-process native-window painter above.
using HandoverDraw=bool(*)(const SceneHandover::State&,const Artwork&);
std::atomic<HandoverDraw> handover_draw{&draw_handover_pixels};
void present_handover(bool initial=false){
    struct PresentContext {
        bool initial;std::optional<Mailbox::Request> request;
        std::optional<Mailbox::Snapshot> snapshot;
        ULONGLONG now{};std::optional<SceneHandover::State> prepared;
        std::shared_ptr<const Artwork> image;std::exception_ptr error{};
        explicit PresentContext(bool value):initial(value){}
    } c(initial);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<PresentContext*>(opaque);
        try{
            switch(id){
            case 300:c.request.emplace(exchange.request());return 0;
            case 301:return c.request->stop;
            case 302:return c.initial;
            case 303:return exchange.awaiting_presentation(*c.request);
            case 304:c.snapshot.emplace(exchange.snapshot());return 0;
            case 305:return bool(c.snapshot->image);
            case 306:c.now=GetTickCount64();return 0;
            case 307:c.prepared.emplace(handover.state());return 0;
            case 308:c.image=transition_image(c.snapshot->image,c.prepared->overlay,
                c.now>=c.prepared->started?c.now-c.prepared->started:0);return 0;
            case 309:return handover.present(c.now,c.snapshot->view,[&](const auto& state){
                return state.token==c.prepared->token&&handover_draw.load()(state,*c.image);});
            case 310:exchange.presented(c.snapshot->image,c.now,100);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_present_handover(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

// Created, used and destroyed only by worker_main. No engine-owned HWND or
// graphics device is borrowed, and no SDK callback runs from this thread.
class Backend {
public:
    struct Timing{DWORD prepare{},draw{},readback{},copy{};} timing;
    explicit Backend(Context config) : config_(std::move(config)) {}
    ~Backend() {
        release_device();
        if (window_) DestroyWindow(window_);
    }
    void reset() { release_device(); }
    bool render(const Mailbox::Request& request, Artwork& output) {
        struct RenderContext {
            Backend& backend;const Mailbox::Request& request;Artwork& output;
            DWORD began{},prepared{},rendered{},read{};
            Draw draw{};IDirect3DSurface9* target{};D3DLOCKED_RECT pixels{};
            std::exception_ptr error{};
            RenderContext(Backend& value,const Mailbox::Request& item,Artwork& frame)
                :backend(value),request(item),output(frame){}
        } c(*this,request,output);
        const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
            auto& c=*static_cast<RenderContext*>(opaque);
            try{
                switch(id){
                case 1200:c.began=GetTickCount();c.backend.timing={};return 0;
                case 1201:return c.backend.load();
                case 1202:return c.backend.device_!=nullptr;
                case 1203:return c.backend.create(c.request.width,c.request.height);
                case 1204:return c.backend.resize_output(c.request.width,c.request.height);
                case 1205:return SUCCEEDED(c.backend.device_->TestCooperativeLevel());
                case 1206:return SUCCEEDED(c.backend.device_->BeginScene());
                case 1207:c.prepared=GetTickCount();
                    c.backend.timing.prepare=c.prepared-c.began;return 0;
                case 1208:return c.backend.set_time_!=nullptr;
                case 1209:c.backend.set_time_(c.request.animation_time);return 0;
                case 1210:return c.request.departure;
                case 1211:return c.request.epoch!=c.backend.warp_epoch_;
                case 1212:return c.backend.reset_warp_!=nullptr;
                case 1213:c.backend.reset_warp_();return 0;
                case 1214:return c.backend.warp_!=nullptr;
                case 1215:c.backend.draw_(c.request.view.c_str(),0,0,
                    c.request.width,c.request.height);return 0;
                case 1216:c.backend.warp_epoch_=c.request.epoch;return 0;
                case 1217:c.draw=arg?c.backend.warp_:c.backend.draw_;return 0;
                case 1218:return c.draw(c.request.view.c_str(),0,0,
                    c.request.width,c.request.height)!=0;
                case 1219:return SUCCEEDED(c.backend.device_->EndScene());
                case 1220:c.rendered=GetTickCount();
                    c.backend.timing.draw=c.rendered-c.prepared;return 0;
                case 1221:return SUCCEEDED(c.backend.device_->GetRenderTarget(0,&c.target))&&
                    c.target!=nullptr;
                case 1222:return SUCCEEDED(c.backend.device_->GetRenderTargetData(
                    c.target,c.backend.readback_));
                case 1223:c.target->Release();return 0;
                case 1224:return SUCCEEDED(c.backend.readback_->LockRect(&c.pixels,nullptr,
                    D3DLOCK_READONLY));
                case 1225:c.read=GetTickCount();
                    c.backend.timing.readback=c.read-c.rendered;return 0;
                case 1226:return c.pixels.pBits!=nullptr;
                case 1227:return c.pixels.Pitch>=c.request.width*4;
                case 1228:c.output.width=c.request.width;c.output.height=c.request.height;
                    c.output.pixels.resize(static_cast<std::size_t>(c.request.width)*
                        c.request.height);return 0;
                case 1229:std::memcpy(c.output.pixels.data()+
                        static_cast<std::size_t>(arg)*c.request.width,
                        static_cast<const BYTE*>(c.pixels.pBits)+
                        static_cast<std::size_t>(arg)*c.pixels.Pitch,
                        c.request.width*4);return 0;
                case 1230:c.backend.readback_->UnlockRect();return 0;
                case 1231:c.backend.timing.copy=GetTickCount()-c.read;return 0;
                case 1232:return static_cast<std::uint32_t>(c.request.height);
                default:return 0;
                }
            }catch(...){c.error=std::current_exception();return UINT64_MAX;}
        };
        const bool result=a2fo_rs_stage8_b_scene_backend_render(&c,op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        return result;
    }
private:
    Context config_;
    HMODULE provider_{}, d3d_runtime_{};
    Init init_{};
    SetDevice set_device_{};
    Draw draw_{}, warp_{};
    using ResetWarp = void (__cdecl*)();
    ResetWarp reset_warp_{};
    using SetTime = unsigned (__cdecl*)(double);
    SetTime set_time_{};
    std::uint64_t warp_epoch_ = UINT64_MAX;
    bool initialized_{};
    HWND window_{};
    IDirect3D9* d3d_{};
    IDirect3DDevice9* device_{};
    IDirect3DSurface9* readback_{};
    IDirect3DSurface9* output_{}, *depth_{};
    D3DFORMAT depth_format_ = D3DFMT_D24S8;
    int width_{}, height_{};
    bool load() {
        struct LoadContext {
            Backend& backend;std::string path;HMODULE module{};
            Version version{};std::exception_ptr error{};
            explicit LoadContext(Backend& value):backend(value){}
        } c(*this);
        const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
            auto& c=*static_cast<LoadContext*>(opaque);
            try{
                switch(id){
                case 900:return c.backend.provider_!=nullptr;
                case 901:c.path=c.backend.config_.root+"\\modules\\refit\\A2FORefitScene.dll";return 0;
                case 902:c.module=LoadLibraryExA(c.path.c_str(),nullptr,
                    LOAD_WITH_ALTERED_SEARCH_PATH);return c.module!=nullptr;
                case 903:c.version=symbol<Version>(c.module,"A2FOScene_ApiVersion");
                    c.backend.init_=symbol<Init>(c.module,"A2FOScene_Initialize");
                    c.backend.set_device_=symbol<SetDevice>(c.module,"A2FOScene_SetDevice");
                    c.backend.draw_=symbol<Draw>(c.module,"Render3D_MenuScene");
                    c.backend.warp_=symbol<Draw>(c.module,"Render3D_MenuWarp");
                    c.backend.reset_warp_=symbol<ResetWarp>(c.module,"A2FOScene_ResetDeparture");
                    c.backend.set_time_=symbol<SetTime>(c.module,"A2FOScene_SetFrameTime");return 0;
                case 904:return c.version!=nullptr;
                case 905:return c.version();
                case 906:return c.backend.init_!=nullptr;
                case 907:return c.backend.set_device_!=nullptr;
                case 908:return c.backend.draw_!=nullptr;
                case 909:{HMODULE pinned{};
                    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
                        GET_MODULE_HANDLE_EX_FLAG_PIN,
                        reinterpret_cast<const char*>(c.version),&pinned)!=FALSE;}
                case 910:c.backend.provider_=c.module;return 0;
                case 911:return c.backend.set_time_!=nullptr;
                case 912:exchange.log(arg?
                    "Refit scene pacing: one frame ahead; presentation-driven animation clock; visible cadence 30 Hz":
                    "Refit scene pacing: one frame ahead; legacy provider clock; visible cadence 30 Hz");
                    return 0;
                default:return 0;
                }
            }catch(...){c.error=std::current_exception();return UINT64_MAX;}
        };
        const bool result=a2fo_rs_stage8_b_scene_backend_load(&c,op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        return result;
    }
    void release_device() {
        if (initialized_ && set_device_) set_device_(nullptr);
        if (device_) {
            for (DWORD i = 0; i < 16; ++i) {
                device_->SetTexture(i, nullptr);
                device_->SetStreamSource(i, nullptr, 0, 0);
            }
            for (DWORD i = 257; i < 261; ++i) device_->SetTexture(i, nullptr);
            device_->SetIndices(nullptr);
            device_->SetVertexShader(nullptr);
            device_->SetPixelShader(nullptr);
            device_->SetVertexDeclaration(nullptr);
            for (DWORD i = 1; i < 4; ++i) device_->SetRenderTarget(i, nullptr);
            device_->SetDepthStencilSurface(nullptr);
            IDirect3DSurface9* back{};
            if (SUCCEEDED(device_->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)) && back) {
                device_->SetRenderTarget(0,back);back->Release();
            }
        }
        if (output_) { output_->Release(); output_ = nullptr; }
        if (depth_) { depth_->Release(); depth_ = nullptr; }
        if (readback_) { readback_->Release(); readback_ = nullptr; }
        if (device_) { device_->Release(); device_ = nullptr; }
        if (d3d_) { d3d_->Release(); d3d_ = nullptr; }
        width_ = height_ = 0;
        warp_epoch_ = UINT64_MAX;
    }
    bool resize_output(int width,int height) {
        struct Surfaces {
            IDirect3DSurface9* color{},*depth{},*readback{};
            ~Surfaces(){if(color)color->Release();if(depth)depth->Release();if(readback)readback->Release();}
        };
        struct ResizeContext {
            Backend& backend;int width,height;Surfaces replacement;
            std::exception_ptr error{};
            ResizeContext(Backend& value,int w,int h):backend(value),width(w),height(h){}
        } c(*this,width,height);
        const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
            auto& c=*static_cast<ResizeContext*>(opaque);
            try{
                switch(id){
                case 1000:return c.backend.output_&&c.backend.width_==c.width&&
                    c.backend.height_==c.height;
                case 1001:return SUCCEEDED(c.backend.device_->CreateRenderTarget(c.width,
                    c.height,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,
                    &c.replacement.color,nullptr));
                case 1002:return SUCCEEDED(c.backend.device_->CreateDepthStencilSurface(c.width,
                    c.height,c.backend.depth_format_,D3DMULTISAMPLE_NONE,0,TRUE,
                    &c.replacement.depth,nullptr));
                case 1003:return SUCCEEDED(c.backend.device_->CreateOffscreenPlainSurface(c.width,
                    c.height,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&c.replacement.readback,nullptr));
                case 1004:return SUCCEEDED(c.backend.device_->SetDepthStencilSurface(nullptr));
                case 1005:return SUCCEEDED(c.backend.device_->SetRenderTarget(0,c.replacement.color));
                case 1006:return SUCCEEDED(c.backend.device_->SetDepthStencilSurface(c.replacement.depth));
                case 1007:{D3DVIEWPORT9 viewport{0,0,static_cast<DWORD>(c.width),
                    static_cast<DWORD>(c.height),0.f,1.f};
                    return SUCCEEDED(c.backend.device_->SetViewport(&viewport));}
                case 1008:std::swap(c.backend.output_,c.replacement.color);
                    std::swap(c.backend.depth_,c.replacement.depth);
                    std::swap(c.backend.readback_,c.replacement.readback);
                    c.backend.width_=c.width;c.backend.height_=c.height;return 0;
                case 1009:{char message[160]{};
                    std::snprintf(message,sizeof(message),
                        "Refit output resized: %dx%d; existing device, textures and shaders retained",
                        c.width,c.height);exchange.log(message);return 0;}
                default:return 0;
                }
            }catch(...){c.error=std::current_exception();return UINT64_MAX;}
        };
        const bool result=a2fo_rs_stage8_b_scene_resize(&c,op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        return result;
    }
    bool create(int width, int height) {
        struct CreateContext {
            Backend& backend;int width,height;
            D3DPRESENT_PARAMETERS parameters{};HRESULT hr{E_FAIL};DWORD chosen{};
            std::exception_ptr error{};
            CreateContext(Backend& value,int w,int h):backend(value),width(w),height(h){}
        } c(*this,width,height);
        const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
            auto& c=*static_cast<CreateContext*>(opaque);
            try{
                switch(id){
                case 1100:c.backend.release_device();return 0;
                case 1101:return c.backend.window_!=nullptr;
                case 1102:{WNDCLASSEXA wc{};wc.cbSize=sizeof(wc);
                    wc.hInstance=c.backend.config_.instance;wc.lpfnWndProc=DefWindowProcA;
                    wc.lpszClassName="A2FORefitScene.Offscreen.v1";
                    return RegisterClassExA(&wc)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS;}
                case 1103:c.backend.window_=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,
                    "A2FORefitScene.Offscreen.v1","",WS_POPUP,0,0,1,1,nullptr,nullptr,
                    c.backend.config_.instance,nullptr);return c.backend.window_!=nullptr;
                case 1104:return c.backend.d3d_runtime_!=nullptr;
                case 1105:c.backend.d3d_runtime_=LoadLibraryA("d3d9.dll");return 0;
                case 1106:{using Create9=IDirect3D9*(WINAPI*)(UINT);
                    const auto create9=c.backend.d3d_runtime_?
                        symbol<Create9>(c.backend.d3d_runtime_,"Direct3DCreate9"):nullptr;
                    c.backend.d3d_=create9?create9(D3D_SDK_VERSION):nullptr;
                    return c.backend.d3d_!=nullptr;}
                case 1107:c.parameters.BackBufferWidth=1;c.parameters.BackBufferHeight=1;
                    c.parameters.BackBufferFormat=D3DFMT_X8R8G8B8;
                    c.parameters.BackBufferCount=1;c.parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
                    c.parameters.hDeviceWindow=c.backend.window_;c.parameters.Windowed=TRUE;
                    c.parameters.EnableAutoDepthStencil=TRUE;
                    c.parameters.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;return 0;
                case 1108:{const DWORD processing=arg<2?D3DCREATE_HARDWARE_VERTEXPROCESSING:
                        D3DCREATE_SOFTWARE_VERTEXPROCESSING;
                    const auto depth=(arg%2)==0?D3DFMT_D24S8:D3DFMT_D16;
                    c.parameters.AutoDepthStencilFormat=depth;
                    constexpr DWORD common=D3DCREATE_FPU_PRESERVE|D3DCREATE_NOWINDOWCHANGES;
                    c.hr=c.backend.d3d_->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,
                        c.backend.window_,common|processing,&c.parameters,&c.backend.device_);
                    if(SUCCEEDED(c.hr)&&c.backend.device_){c.chosen=processing;
                        c.backend.depth_format_=depth;return 1;}return 0;}
                case 1109:return SUCCEEDED(c.hr);
                case 1110:return c.backend.device_!=nullptr;
                case 1111:return c.backend.resize_output(c.width,c.height);
                case 1112:return c.backend.initialized_;
                case 1113:{std::vector<const char*> pointers;
                    for(const auto& root:c.backend.config_.roots)pointers.push_back(root.c_str());
                    const auto resources=c.backend.config_.root+"\\RefitScene";
                    const HostInit host{sizeof(HostInit),1,c.backend.device_,resources.c_str(),
                        pointers.data(),static_cast<unsigned>(pointers.size())};
                    c.backend.initialized_=c.backend.init_(&host)!=0;
                    return c.backend.initialized_;}
                case 1114:return c.backend.set_device_(c.backend.device_)!=0;
                case 1115:{char message[180]{};
                    std::snprintf(message,sizeof(message),
                        "Refit scene worker: %s; retained device/cache; %dx%d; adaptive frame budget",
                        c.chosen==D3DCREATE_HARDWARE_VERTEXPROCESSING?
                            "hardware vertices":"software vertex fallback",c.width,c.height);
                    exchange.log(message);return 0;}
                default:return 0;
                }
            }catch(...){c.error=std::current_exception();return UINT64_MAX;}
        };
        const bool result=a2fo_rs_stage8_b_scene_backend_create(&c,op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        return result;
    }
};

DWORD WINAPI __attribute__((force_align_arg_pointer)) worker_main(void* parameter) {
    std::unique_ptr<Context> context(static_cast<Context*>(parameter));
    try {
        Backend backend(std::move(*context));
        DWORD completed{}, cost{}, failed_at{};
        std::string drawn_view;
        std::uint64_t drawn_epoch = UINT64_MAX;
        unsigned reports{}, failures{};
        for (;;) {
            struct Step {
                Backend& backend;DWORD& completed;
                DWORD& cost;DWORD& failed_at;std::string& drawn_view;
                std::uint64_t& drawn_epoch;unsigned& reports;unsigned& failures;
                std::optional<Mailbox::Request> request;
                Artwork output;DWORD now{},rendered_cost{};
                std::exception_ptr error{};
                Step(Backend& b,DWORD& done,DWORD& spent,DWORD& failed,
                    std::string& view,std::uint64_t& epoch,unsigned& notes,unsigned& retries)
                    :backend(b),completed(done),cost(spent),failed_at(failed),
                     drawn_view(view),drawn_epoch(epoch),reports(notes),failures(retries){}
            } c{backend,completed,cost,failed_at,drawn_view,drawn_epoch,reports,failures};
            const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
                auto& c=*static_cast<Step*>(opaque);
                try{
                    switch(id){
                    case 200:{MSG message{};while(PeekMessageA(&message,nullptr,0,0,PM_REMOVE)){
                        TranslateMessage(&message);DispatchMessageA(&message);}return 0;}
                    case 201:return handover_active();
                    case 202:present_handover();return 0;
                    case 203:c.request.emplace(exchange.request());return 0;
                    case 204:return c.request->stop;
                    case 205:c.now=GetTickCount();return c.now;
                    case 206:return c.request->view!=c.drawn_view||
                        c.request->epoch!=c.drawn_epoch;
                    case 207:return c.request->departure;
                    case 208:return GetTickCount64()<c.request->lease_until;
                    case 209:return c.request->active;
                    case 210:return animation_pause(c.cost);
                    case 211:return retry_pause(c.failures,c.failed_at,c.now);
                    case 212:return exchange.awaiting_presentation(*c.request);
                    case 213:return c.completed;
                    case 214:exchange.loading(arg!=0);return 0;
                    case 215:return c.backend.render(*c.request,c.output);
                    case 216:exchange.loading(false);return 0;
                    case 217:c.failed_at=GetTickCount();return 0;
                    case 218:c.backend.reset();return 0;
                    case 219:return ++c.failures;
                    case 220:exchange.log("Refit scene worker unavailable after 3 attempts; retaining latest frame/artwork fallback");return 0;
                    case 221:c.drawn_view.clear();return 0;
                    case 222:exchange.log("Refit scene worker draw failed; retaining frame and retrying device after at least 1 second");return 0;
                    case 223:c.failures=0;return 0;
                    case 224:c.completed=GetTickCount();c.rendered_cost=c.completed-c.now;return 0;
                    case 225:c.cost=arg?0:c.rendered_cost;return 0;
                    case 226:c.drawn_view=c.request->view;c.drawn_epoch=c.request->epoch;return 0;
                    case 227:exchange.publish(std::move(c.output),*c.request);return 0;
                    case 228:return c.reports++;
                    case 229:{char text[320]{};
                        std::snprintf(text,sizeof(text),
                            "Refit worker timing: view=%s render=%lu ms, scheduled-yield=%u ms, size=%dx%d; prepare=%lu draw=%lu GPU-wait/readback=%lu copy=%lu ms; UI did not wait",
                            c.request->view.c_str(),static_cast<unsigned long>(c.rendered_cost),
                            animation_pause(c.cost),c.request->width,c.request->height,
                            static_cast<unsigned long>(c.backend.timing.prepare),
                            static_cast<unsigned long>(c.backend.timing.draw),
                            static_cast<unsigned long>(c.backend.timing.readback),
                            static_cast<unsigned long>(c.backend.timing.copy));
                        exchange.log(text);return 0;}
                    case 230:{const HANDLE event=wake_event.load(std::memory_order_acquire);
                        MsgWaitForMultipleObjects(1,&event,FALSE,arg,QS_ALLINPUT);return 0;}
                    default:return 0;
                    }
                }catch(...){c.error=std::current_exception();return UINT64_MAX;}
            };
            const bool keep_running=a2fo_rs_stage8_b_scene_worker_step(&c,op)!=0;
            if(c.error)std::rethrow_exception(c.error);
            if(!keep_running)break;
        }
    } catch (...) {
        exchange.loading(false);
        exchange.log("Refit scene worker exception; retaining artwork fallback");
    }
    return 0;
}

struct StartContext {std::unique_ptr<Context> value;std::exception_ptr error;};
std::uint64_t start_op(void* opaque,std::uint32_t id,std::uint32_t arg) noexcept {
    auto& c=*static_cast<StartContext*>(opaque);
    try{
        switch(id){
        case 0:return started;
        case 1:return unavailable;
        case 2:return api&&api->root_directory;
        case 3:c.value=std::make_unique<Context>();return 0;
        case 4:{const auto root=api->root_directory();if(!root||!*root)return 0;c.value->root=root;return 1;}
        case 5:return A2FO_MODULE_API_HAS(api,extension_root)&&api->extension_root_count&&api->extension_root;
        case 6:return api->extension_root_count();
        case 7:{const auto path=api->extension_root(arg);if(path&&*path)c.value->roots.emplace_back(path);return 0;}
        case 8:if(c.value->roots.empty())c.value->roots.push_back(c.value->root);return 0;
        case 9:{HMODULE self{};if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const char*>(&worker_main),&self))return 0;
            c.value->instance=self;return 1;}
        case 10:{const HANDLE event=CreateEventA(nullptr,FALSE,FALSE,nullptr);if(!event)return 0;
            wake_event.store(event,std::memory_order_release);return 1;}
        case 11:{
            // Ownership transition is one native operation: worker_main may
            // adopt the Context immediately after CreateThread returns.
            HANDLE thread=CreateThread(nullptr,0,&worker_main,c.value.get(),0,nullptr);
            if(!thread){unavailable=true;return 0;}
            c.value.release();
            worker_thread.store(thread,std::memory_order_release);
            return 1;
        }
        case 12:started=true;return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
bool start() {
    StartContext c{};
    const bool result=a2fo_rs_stage8_b_scene_start(&c,start_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    struct WindowContext {
        HWND window;UINT message;WPARAM wp;
        decltype(windows)::iterator entry;
        std::exception_ptr error{};
        WindowContext(HWND handle,UINT event,WPARAM value)
            :window(handle),message(event),wp(value){}
    } c(window,message,wp);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<WindowContext*>(opaque);
        try{
            switch(id){
            case 800:return c.message==WM_TIMER;
            case 801:c.entry=windows.find(c.window);return 0;
            case 802:return wanted.load();
            case 803:return c.entry!=windows.end();
            case 804:return c.wp==c.entry->second;
            case 805:return c.window==presentation_window;
            case 806:drain_logs();return 0;
            case 807:return !mission_backdrop::active();
            case 808:return c.window==presentation_window;
            case 809:return IsWindowVisible(c.window)!=FALSE;
            case 810:return !IsIconic(c.window);
            case 811:return IsWindowEnabled(c.window)!=FALSE;
            case 812:active(arg!=0);return 0;
            case 813:return exchange.needs_presentation(displayed_revision);
            case 814:RedrawWindow(c.window,nullptr,nullptr,RDW_INVALIDATE|
                (redraw_children[c.window]?RDW_ALLCHILDREN:RDW_NOCHILDREN));return 0;
            case 815:return c.message==WM_SHOWWINDOW||c.message==WM_ENABLE;
            case 816:return c.wp==0;
            case 817:return c.window==presentation_window;
            case 818:active(false);return 0;
            case 819:return c.message==WM_NCDESTROY;
            case 820:c.entry=windows.find(c.window);return 0;
            case 821:return c.entry!=windows.end();
            case 822:KillTimer(c.window,c.entry->second);windows.erase(c.entry);return 0;
            case 823:redraw_children.erase(c.window);return 0;
            case 824:RemoveWindowSubclass(c.window,&window_proc,timer_id);return 0;
            case 825:return c.window==presentation_window;
            case 826:presentation_window=nullptr;active(false);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool handled=a2fo_rs_stage8_b_scene_window(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    if(handled)return 0;
    // The worker's private device/window survive this form's lifetime.
    return DefSubclassProc(window, message, wp, lp);
}
int page_count() {
    return a2fo_rs_stage8_b_scene_page_count(static_cast<int>(views.size()),page_size);
}
int choice_count() {
    return a2fo_rs_stage8_b_scene_choice_count(static_cast<int>(views.size()),
        choice_page,page_size);
}
Rect choice(int index, int width, int height) {
    const auto geometry = layout(width, height, default_snapshot());
    const int count = choice_count();
    A2foStage8SceneRect result{};
    a2fo_rs_stage8_b_scene_choice_rect(index,width,height,geometry.title.x,
        geometry.title.width,count,&result);
    return {result.x,result.y,result.width,result.height};
}
}

struct ConfigureContext {
    const A2FO_ModuleApi* supplied;
    bool enable;
    int render_width;
    const char* root{};
    std::vector<SceneView> catalogue;
    std::string error_text;
    std::exception_ptr error;
};
std::uint64_t configure_op(void* opaque,std::uint32_t id,std::uint32_t arg) noexcept {
    auto& c=*static_cast<ConfigureContext*>(opaque);
    try{
        switch(id){
        case 0:api=c.supplied;configured_width=c.render_width;wanted=c.enable;return 0;
        case 1:return c.enable&&api&&api->root_directory;
        case 2:c.root=api->root_directory();return c.root!=nullptr;
        case 3:return load_scene_catalogue(std::string(c.root)+"\\RefitScene",c.catalogue,&c.error_text)&&!c.catalogue.empty();
        case 4:views=std::move(c.catalogue);selected=0;return 0;
        case 5:return views.size();
        case 6:return views[arg].id=="main";
        case 7:selected=static_cast<int>(arg);return 0;
        case 8:choice_page=selected/page_size;return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
void configure(const A2FO_ModuleApi* supplied, HMODULE, bool enable, int render_width) {
    ConfigureContext c{};
    c.supplied=supplied;c.enable=enable;c.render_width=render_width;
    a2fo_rs_stage8_b_scene_configure(&c,configure_op);
    if(c.error)std::rethrow_exception(c.error);
}
bool requested() { return wanted; }
bool handover_active(){return handover.state().active(GetTickCount64());}
bool handover_active_for_erase(){
    SceneHandover::State state;
    return !handover.try_state(state)||state.active(GetTickCount64());
}
bool launch_handover_active(){const auto state=handover.state();return state.launch&&state.active(GetTickCount64());}
bool handover_ready(){const auto state=handover.state();return state.active(GetTickCount64())&&state.ready;}
namespace {
bool start_handover(HWND source,HWND parent,bool launch,std::shared_ptr<const Artwork> overlay,bool accepted_loading=false){
    struct HandoverContext {
        HWND source,parent;bool launch,accepted;std::shared_ptr<const Artwork>& overlay;
        DWORD process{},thread{};RECT client{};SceneSize size{};
        HANDLE token{};std::string view,candidate;ULONGLONG now{};
        std::exception_ptr error{};
        HandoverContext(HWND s,HWND p,bool l,bool a,std::shared_ptr<const Artwork>& o)
            :source(s),parent(p),launch(l),accepted(a),overlay(o){}
    } c(source,parent,launch,accepted_loading,overlay);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<HandoverContext*>(opaque);
        try{
            switch(id){
            case 100:return wanted.load();
            case 101:return claim_ui();
            case 102:return bool(exchange.snapshot().image);
            case 103:return c.parent!=nullptr;
            case 104:return IsWindow(c.parent)!=FALSE;
            case 105:return c.accepted;
            case 106:return c.source!=nullptr;
            case 107:return c.source!=c.parent;
            case 108:return IsWindow(c.source)!=FALSE;
            case 109:return GetParent(c.source)==c.parent;
            case 110:if(api&&api->log){char text[192]{};
                std::snprintf(text,sizeof(text),
                    "IA scene handover skipped: native window relation source=%p parent=%p observed=%p",
                    static_cast<void*>(c.source),static_cast<void*>(c.parent),
                    static_cast<void*>(c.source?GetParent(c.source):nullptr));
                api->log("A2FORefitMenus",text);}return 0;
            case 111:c.thread=GetCurrentThreadId();return 0;
            case 112:return GetWindowThreadProcessId(c.source,&c.process)==c.thread&&
                c.process==GetCurrentProcessId();
            case 113:return GetWindowThreadProcessId(c.parent,&c.process)==c.thread&&
                c.process==GetCurrentProcessId();
            case 114:return GetClientRect(c.accepted?c.parent:c.source,&c.client)!=FALSE;
            case 115:c.size=target_size(c.client.right,c.client.bottom,configured_width);
                return c.size.valid;
            case 116:return start();
            case 117:end_handover();return 0;
            case 118:if(!++handover_generation)++handover_generation;
                c.token=reinterpret_cast<HANDLE>(handover_generation);return 0;
            case 119:return SetPropA(c.parent,handover_property,c.token)!=FALSE;
            case 120:return c.source!=nullptr;
            case 121:return SetPropA(c.source,handover_property,c.token)!=FALSE;
            case 122:RemovePropA(c.parent,handover_property);return 0;
            case 123:c.view=c.launch?exchange.request().view:views[selected].id;return 0;
            case 124:c.candidate=c.view+".instantaction";return 0;
            case 125:return c.launch;
            case 126:return (a2fo_stage10_presentation_find(views.begin(),views.end(),
                [&](const auto& v){return v.id==c.candidate;})!=views.end());
            case 127:c.view=c.candidate;return 0;
            case 128:c.now=GetTickCount64();return 0;
            case 129:handover.begin(reinterpret_cast<std::uintptr_t>(c.source),
                reinterpret_cast<std::uintptr_t>(c.parent),c.thread,c.view,c.now,
                handover_generation,c.launch,std::move(c.overlay));return 0;
            case 130:SetWindowSubclass(c.parent,&handover_erase_proc,handover_erase_id,0);return 0;
            case 131:exchange.request(c.view,c.size.width,c.size.height,true,c.launch,
                c.launch?c.now+10000:0,c.launch);return 0;
            case 132:return c.launch;
            case 133:present_handover(true);return 0;
            case 134:wake();return 0;
            case 135:if(api&&api->log)api->log("A2FORefitMenus",c.launch?
                "Launch scene handover started: IA controls fade into the continuing departure":
                "IA scene handover started before native main-menu close");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_scene_handover(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
}
bool begin_handover(HWND source,HWND parent){return start_handover(source,parent,false,{});}
bool begin_launch_handover(HWND source,HWND parent,std::shared_ptr<const Artwork> completed_ui){
    return completed_ui&&start_handover(source,parent,true,std::move(completed_ui));
}
bool begin_accepted_loading_handover(HWND parent,std::shared_ptr<const Artwork> completed_ui){
    return completed_ui&&start_handover(nullptr,parent,true,std::move(completed_ui),true);
}
void handover_destination(HWND window){
    struct DestinationContext {
        HWND window;std::optional<SceneHandover::State> state;
        std::exception_ptr error{};
        explicit DestinationContext(HWND value):window(value){}
    } c(window);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<DestinationContext*>(opaque);
        try{
            switch(id){
            case 400:return claim_ui();
            case 401:return c.window!=nullptr;
            case 402:return GetWindowThreadProcessId(c.window,nullptr)==GetCurrentThreadId();
            case 403:c.state.emplace(handover.state());return 0;
            case 404:return c.state->active(GetTickCount64());
            case 405:return handover_window_matches(*c.state,c.state->parent);
            case 406:return GetParent(c.window)==reinterpret_cast<HWND>(c.state->parent);
            case 407:return SetPropA(c.window,handover_property,
                reinterpret_cast<HANDLE>(c.state->token))!=FALSE;
            case 408:handover.destination(reinterpret_cast<std::uintptr_t>(c.window),
                GetTickCount64());return 0;
            case 409:return c.state->destination!=0;
            case 410:return c.state->destination!=reinterpret_cast<std::uintptr_t>(c.window);
            case 411:return c.state->destination!=c.state->source;
            case 412:return c.state->destination!=c.state->parent;
            case 413:return reinterpret_cast<std::uintptr_t>(GetPropA(
                reinterpret_cast<HWND>(c.state->destination),handover_property))==c.state->token;
            case 414:RemovePropA(reinterpret_cast<HWND>(c.state->destination),
                handover_property);return 0;
            case 415:wake();return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_destination(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
void end_handover(){
    struct EndContext {
        std::optional<SceneHandover::State> old;
        std::exception_ptr error{};
        std::uintptr_t handle(std::uint32_t index) const {
            return index==0?old->source:index==1?old->parent:old->destination;
        }
    } c;
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t index) noexcept -> std::uint64_t {
        auto& c=*static_cast<EndContext*>(opaque);
        try{
            switch(id){
            case 420:c.old.emplace(handover.finish());return 0;
            case 421:return c.old->parent!=0;
            case 422:return GetWindowThreadProcessId(
                reinterpret_cast<HWND>(c.old->parent),nullptr)==GetCurrentThreadId();
            case 423:RemoveWindowSubclass(reinterpret_cast<HWND>(c.old->parent),
                &handover_erase_proc,handover_erase_id);return 0;
            case 424:return c.handle(index)!=0;
            case 425:return c.old->token!=0;
            case 426:return reinterpret_cast<std::uintptr_t>(GetPropA(
                reinterpret_cast<HWND>(c.handle(index)),handover_property))==c.old->token;
            case 427:RemovePropA(reinterpret_cast<HWND>(c.handle(index)),handover_property);
                return 0;
            case 428:return c.old->parent!=0;
            case 429:return api&&api->log;
            case 430:{char text[160]{};std::snprintf(text,sizeof(text),
                "IA scene handover finished: %u background frames over %llu ms",
                c.old->frames,static_cast<unsigned long long>(GetTickCount64()-c.old->started));
                api->log("A2FORefitMenus",text);return 0;}
            case 431:wake();return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_end_handover(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
std::shared_ptr<const Artwork> retained_frame() { return exchange.snapshot().image; }
struct FrameContext {
    HWND window{};
    bool animate_overlay{},repaint_children{},instant_action{};
    char class_name[64]{};
    RECT client{};
    SceneSize size{};
    UINT_PTR timer{};
    std::string view,setup_view;
    const Artwork* result{};
    std::exception_ptr error;
};
std::uint64_t frame_op(void* opaque,std::uint32_t id,std::uint32_t) noexcept {
    auto& c=*static_cast<FrameContext*>(opaque);
    try{
        switch(id){
        case 0:return claim_ui();
        case 9:end_departure();return 0;
        case 10:return wanted.load();
        case 12:{DWORD unowned=0;const DWORD current=GetCurrentThreadId();
            return ui_thread.compare_exchange_strong(unowned,current)||unowned==current;}
        case 13:return start();
        case 21:return c.window!=nullptr;
        case 22:return IsWindow(c.window);
        case 23:return GetWindowThreadProcessId(c.window,nullptr)==GetCurrentThreadId();
        case 24:return launch_handover_active();
        case 25:displayed=exchange.snapshot().image;c.result=displayed.get();return 0;
        case 26:return loading_seen.exchange(false);
        case 27:++menu_returns;return 0;
        case 28:return mission_backdrop::active();
        case 29:end_departure();return 0;
        case 30:active(false);return 0;
        case 31:c.result=mission_backdrop::frame();return 0;
        case 32:mission_backdrop::reset();return 0;
        case 33:drain_logs();return 0;
        case 34:return GetClassNameA(c.window,c.class_name,sizeof(c.class_name))!=0;
        case 35:return (!std::strcmp(c.class_name,"TMainForm")||c.animate_overlay)&&IsWindowEnabled(c.window);
        case 36:return GetClientRect(c.window,&c.client);
        case 37:return c.client.right>=80&&c.client.bottom>=100;
        case 38:c.size=target_size(c.client.right,c.client.bottom,configured_width);return c.size.valid;
        case 39:presentation_window=c.window;return 0;
        case 40:redraw_children[c.window]=c.repaint_children;return 0;
        case 41:return windows.count(c.window);
        case 42:return SetWindowSubclass(c.window,&window_proc,timer_id,0);
        case 43:c.timer=SetTimer(c.window,timer_id,16,nullptr);return c.timer!=0;
        case 44:RemoveWindowSubclass(c.window,&window_proc,timer_id);return 0;
        case 45:windows.emplace(c.window,c.timer);return 0;
        case 46:c.view=handover_active()?exchange.request().view:views[selected].id;
                c.setup_view=c.view+".instantaction";return 0;
        case 47:return !handover_active()&&c.instant_action&&
            !std::strcmp(c.class_name,"#32770")&&
            (a2fo_stage10_presentation_find(views.begin(),views.end(),[&](const auto& candidate){return candidate.id==c.setup_view;})!=views.end());
        case 48:c.view=c.setup_view;return 0;
        case 49:return exchange.request(c.view,c.size.width,c.size.height,true);
        case 50:return c.window==presentation_window;
        case 51:displayed=exchange.snapshot().image;c.result=displayed.get();return 0;
        case 52:wake();return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
const Artwork* frame(HWND window, bool animate_overlay, bool repaint_children, bool instant_action) {
    FrameContext c{};c.window=window;c.animate_overlay=animate_overlay;
    c.repaint_children=repaint_children;c.instant_action=instant_action;
    a2fo_rs_stage8_b_scene_frame(&c,frame_op);
    if(c.error)std::rethrow_exception(c.error);
    return c.result;
}
void frame_presented(const Artwork* image) {
    struct Context {const Artwork* image;std::optional<Mailbox::Snapshot> snapshot;
        std::exception_ptr error{};explicit Context(const Artwork* value):image(value){}} c(image);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 600:c.snapshot.emplace(exchange.snapshot());return 0;
            case 601:return c.image!=nullptr;
            case 602:return c.snapshot->image.get()==c.image;
            case 603:return exchange.presented(c.snapshot->image,GetTickCount64());
            case 604:displayed_revision=c.snapshot->image_revision;return 0;
            case 605:wake();return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_frame_presented(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
namespace {
void __cdecl canvas_completed(HWND,HDC logical,BOOL presented,void*) {
    struct Context {HDC logical;BOOL presented;
        decltype(pending_canvas)::iterator found;
        std::shared_ptr<const Artwork> image;
        std::optional<Mailbox::Snapshot> snapshot;
        std::exception_ptr error{};
        Context(HDC surface,BOOL completed):logical(surface),presented(completed){}
    } c(logical,presented);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 610:c.found=pending_canvas.find(c.logical);
                return c.found!=pending_canvas.end();
            case 611:c.image=std::move(c.found->second);
                pending_canvas.erase(c.found);return 0;
            case 612:c.snapshot.emplace(exchange.snapshot());return 0;
            case 613:return c.presented!=FALSE;
            case 614:return c.snapshot->image==c.image;
            case 615:return exchange.presented(c.image,GetTickCount64());
            case 616:displayed_revision=c.snapshot->image_revision;return 0;
            case 617:wake();return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_canvas_completed(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
}
void frame_drawn(const Artwork* image,HDC destination) {
    struct Context {const Artwork* image;HDC destination;
        std::optional<Mailbox::Snapshot> snapshot;HMODULE shell{};
        A2FO_ShellCanvasWindowV1Fn resolve{};
        std::exception_ptr error{};
        Context(const Artwork* value,HDC dc):image(value),destination(dc){}
    } c(image,destination);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 620:return c.image!=nullptr;
            case 621:c.snapshot.emplace(exchange.snapshot());return 0;
            case 622:return c.snapshot->image.get()==c.image;
            case 623:c.shell=a2fo::startup_module_provider("A2FOShellDisplay.dll",
                {"A2FO_ShellCanvasWindowV1", "A2FO_SetShellCanvasCompletionV1"});
                c.resolve=c.shell?symbol<A2FO_ShellCanvasWindowV1Fn>(
                    c.shell,"A2FO_ShellCanvasWindowV1"):nullptr;return 0;
            case 624:return c.resolve!=nullptr;
            case 625:return c.resolve(c.destination)!=FALSE;
            case 626:return canvas_completion_registered;
            case 627:{const auto set=symbol<A2FO_SetShellCanvasCompletionV1Fn>(
                    c.shell,"A2FO_SetShellCanvasCompletionV1");
                if(set)canvas_completion_registered=set(&canvas_completed,nullptr)!=FALSE;
                return 0;}
            case 628:return canvas_completion_registered;
            case 629:pending_canvas[c.destination]=c.snapshot->image;return 0;
            case 630:frame_presented(c.image);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_frame_drawn(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
struct ViewContext {
    const char* id{};
    int width{},height{};
    bool restart{};
    std::string name;
    std::optional<Mailbox::Request> request;
    SceneSize size{};
    std::shared_ptr<const Artwork> image;
    std::exception_ptr error;
};
std::uint64_t view_op(void* opaque,std::uint32_t id,std::uint32_t arg) noexcept {
    auto& c=*static_cast<ViewContext*>(opaque);
    try{
        switch(id){
        case 0:return claim_ui();
        case 1:return !c.id||!*c.id;
        case 2:return static_cast<unsigned char>(c.id[arg]);
        case 3:c.name.push_back(static_cast<char>(arg));return 0;
        case 4:return views.size();
        case 5:return views[arg].id==c.name;
        case 6:selected=static_cast<int>(arg);choice_page=selected/page_size;return 0;
        case 7:c.request=exchange.request();return 0;
        case 8:return exchange.request(views[selected].id,c.request->width,c.request->height,
            c.request->active,c.request->departure,c.request->lease_until);
        case 9:wake();return 0;
        case 10:return wanted.load();
        case 11:return select_view(c.id);
        case 12:{DWORD unowned=0;const DWORD current=GetCurrentThreadId();
            return ui_thread.compare_exchange_strong(unowned,current)||unowned==current;}
        case 13:return start();
        case 14:c.request=exchange.request();return 0;
        case 15:{const auto& view=c.id&&*c.id?views[selected].id:c.request->view;
            return exchange.request(view,c.request->width,c.request->height,true,true,
                GetTickCount64()+loading_lease_ms,true);}
        case 16:loading_seen=true;return 0;
        case 17:c.size=target_size(c.width,c.height,configured_width);return c.size.valid;
        case 18:return exchange.request(c.request->view,c.size.width,c.size.height,true,true,
            GetTickCount64()+loading_lease_ms,c.restart);
        case 19:drain_logs();return 0;
        case 20:c.image=exchange.snapshot().image;return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
bool select_view(const char* id) {
    ViewContext c{};c.id=id;
    const bool result=a2fo_rs_stage8_b_scene_select(&c,view_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
bool begin_departure(const char* id) {
    ViewContext c{};c.id=id;
    const bool result=a2fo_rs_stage8_b_scene_departure(&c,view_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
void end_departure() { if (exchange.end_departure()) wake(); }
bool departing() { return exchange.request().departure; }
std::uint64_t menu_epoch() { return menu_returns.load(); }
std::shared_ptr<const Artwork> loading_frame(int width, int height, bool restart) {
    ViewContext c{};c.width=width;c.height=height;c.restart=restart;
    a2fo_rs_stage8_b_scene_loading_frame(&c,view_op);
    if(c.error)std::rethrow_exception(c.error);
    return c.image;
}
void loading_presented(const std::shared_ptr<const Artwork>& image) {
    if(exchange.presented(image,GetTickCount64()))wake();
}
struct ChoiceContext {
    int width{},height{},x{},y{};
    std::optional<Mailbox::Request> request;
    std::exception_ptr error;
};
std::uint64_t choice_op(void* opaque,std::uint32_t id,std::uint32_t arg) noexcept {
    auto& c=*static_cast<ChoiceContext*>(opaque);
    try{
        switch(id){
        case 0:return wanted.load();
        case 1:return choice_count();
        case 2:return choice(static_cast<int>(arg),c.width,c.height).contains(c.x,c.y);
        case 3:return views.size();
        case 4:return choice_page;
        case 5:return page_size;
        case 6:return page_count();
        case 7:choice_page=static_cast<int>(arg);return 0;
        case 8:selected=static_cast<int>(arg);return 0;
        case 9:c.request=exchange.request();return 0;
        case 10:return exchange.request(views[selected].id,c.request->width,c.request->height,c.request->active);
        case 11:wake();return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
int choice_at(int width, int height, int x, int y) {
    ChoiceContext c{};c.width=width;c.height=height;c.x=x;c.y=y;
    const int result=a2fo_rs_stage8_b_scene_choice_at(&c,choice_op);
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
void choose(int index) {
    ChoiceContext c{};
    a2fo_rs_stage8_b_scene_choose(index,&c,choice_op);
    if(c.error)std::rethrow_exception(c.error);
}
void paint_choices(HDC dc, int width, int height) {
    struct PaintContext {
        HDC dc;int width,height,saved{},index{},i{};RECT area{};HBRUSH brush{};
        std::exception_ptr error{};
        PaintContext(HDC surface,int w,int h):dc(surface),width(w),height(h){}
    } c(dc,width,height);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<PaintContext*>(opaque);
        try{
            switch(id){
            case 1300:return wanted.load();
            case 1301:c.saved=SaveDC(c.dc);return c.saved!=0;
            case 1302:SelectObject(c.dc,GetStockObject(DEFAULT_GUI_FONT));
                SetBkMode(c.dc,TRANSPARENT);return 0;
            case 1303:return views.size();
            case 1304:return static_cast<std::uint32_t>(choice_page);
            case 1305:return static_cast<std::uint32_t>(choice_count());
            case 1306:return static_cast<std::uint32_t>(selected);
            case 1307:{c.index=static_cast<int>(arg);
                const auto r=choice(c.i,c.width,c.height);
                c.area={r.x,r.y,r.x+r.width,r.y+r.height};return 0;}
            case 1308:c.brush=CreateSolidBrush(arg?RGB(122,76,16):RGB(24,32,42));return 0;
            case 1309:if(c.brush){FillRect(c.dc,&c.area,c.brush);DeleteObject(c.brush);}return 0;
            case 1310:SetTextColor(c.dc,arg?RGB(242,169,59):RGB(228,234,240));return 0;
            case 1311:{const char* label=arg==0?views[c.index].label.c_str():
                    arg==1?"<":">";
                DrawTextA(c.dc,label,-1,&c.area,
                    DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
                ++c.i;return 0;}
            case 1312:return exchange.snapshot().loading;
            case 1313:{RECT loading{std::max(0,c.width-700),c.height-98,c.width-16,
                    c.height-72};
                SetTextColor(c.dc,RGB(228,234,240));
                DrawTextA(c.dc,"Preparing 3D background...",-1,&loading,
                    DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);return 0;}
            case 1314:RestoreDC(c.dc,c.saved);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_paint_choices(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
void shutdown() {
    struct Context {std::exception_ptr error{};} c;
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 640:wanted=false;return 0;
            case 641:exchange.stop();return 0;
            case 642:wake();return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_scene_shutdown(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    // Never wait on a graphics driver or join a worker from a loader callback.
    // The resident worker finishes any in-flight draw and destroys its own device.
}
}
