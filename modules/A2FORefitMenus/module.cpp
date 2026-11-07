// Refit main-menu presentation adapted to FO4's actual form/actions.
// Upstream MIT attribution: third_party/armada2-refit/.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../../sdk/include/a2fo_supported_fleetops.hpp"
#include "../refit_common/settings.hpp"
#include "native_model.hpp"
#include "interaction.hpp"
#include "status.hpp"
#include "profile.hpp"
#include "skin_native.hpp"
#include "menu_scene.hpp"
#include "scene_settings.hpp"
#include "setup_native.hpp"
#include "loading_native.hpp"
#include "mission_backdrop.hpp"
#include "../../rust/stage10_presentation_completion_ffi.h"
#include "../../rust/stage8_b_refit_ffi.h"
#include <commctrl.h>
#include <windowsx.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <exception>

static_assert(sizeof(void*) == 4, "FO Delphi bindings require i386");
extern "C" void a2fo_refit_menu_show_bridge();
extern "C" void* a2fo_refit_menu_call(void*, void*, void*);
namespace {
using namespace a2fo::refit_menu;
namespace n = a2fo::refit_menu::native;
constexpr char name[] = "A2FORefitMenus";
constexpr UINT_PTR subclass_id = 0xa2f04d01;
constexpr UINT_PTR handover_timer = 0xa2f04d02;
const A2FO_ModuleApi* api = nullptr;
HMODULE fo = nullptr;
std::uint32_t image_size = 0;
A2FO_InlineHook hook{};
std::atomic<void*> next_show{nullptr};
std::atomic<bool> ready{false};
bool initialized = false, installed = false;
UINT action_message = 0, restore_message = 0;
volatile LONG shown_count = 0, action_count = 0, active_count = 0;
std::uint32_t serial = 0;
struct Session {
    HWND window = nullptr;
    void* form = nullptr;
    Snapshot snapshot;
    Artwork artwork;
    Interaction input;
    std::array<n::Visual, 9> visuals{};
    std::uint32_t generation = 0;
    bool restoring = false;
    int pressed_scene = -1;
    int deferred_action = -1;
    ULONGLONG handover_deadline{};
};
std::shared_ptr<Session> session; // Accessed on the native UI thread only.
std::atomic<HWND> active_window{nullptr};
void log(const char* text) { if (api && api->log) api->log(name, text); }
void* at(std::uint32_t rva) { return reinterpret_cast<BYTE*>(fo) + rva; }
struct Reader {
    bool operator()(std::uintptr_t address, void* out, std::size_t size) const {
        SIZE_T count = 0;
        return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size, &count) && count == size;
    }
};
bool capture(Session& s, bool visuals) {
    Reader reader;
    return n::model(reader, reinterpret_cast<std::uintptr_t>(s.form), reinterpret_cast<std::uintptr_t>(fo), image_size,
                    s.snapshot, visuals ? &s.visuals : nullptr);
}
bool known_fleetops() {
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS32 nt{}; Reader read;
    const auto base = reinterpret_cast<std::uintptr_t>(fo);
    if (!read(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 || dos.e_lfanew >= 4096 ||
        !read(base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.OptionalHeader.ImageBase != 0x5a800000 || nt.OptionalHeader.SizeOfImage > UINT32_MAX - base) return false;
    image_size = nt.OptionalHeader.SizeOfImage;
    char path[32768]{};
    const DWORD length = GetModuleFileNameA(fo, path, sizeof(path));
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!length || length >= sizeof(path) || !GetFileAttributesExA(path, GetFileExInfoStandard, &info) ||
        info.nFileSizeHigh || info.nFileSizeLow != 3043840) return false;
    std::uint32_t crc = 0;
    // Existing ShellDisplay profiles plus the reviewed October Roots DLL;
    // that legacy October admission retains its exact complete-file SHA-256.
    const bool known_crc =
        a2fo::supported_armada::file_range_crc32(fo, 0, info.nFileSizeLow, crc) &&
        (crc == 0x07bd1e4f || crc == 0x3ed28fbe || crc == 0x46a0dd96 ||
         (crc == 0x7cd6661e && n::october_roots_file(path)));
    if (!known_crc && !a2fo::supported_fleetops::approved_profile(fo)) return false;
    for (const auto& sig : n::signatures) {
        std::array<BYTE, 64> bytes{};
        if (sig.size > bytes.size() || sig.rva > image_size || sig.size > image_size - sig.rva ||
            !read(base + sig.rva, bytes.data(), sig.size) || std::memcmp(bytes.data(), sig.bytes, sig.size)) return false;
    }
    return true;
}
void set_visible(const n::Visual& visual, bool value) {
    a2fo_refit_menu_call(at(0xc563c), reinterpret_cast<void*>(visual.control), reinterpret_cast<void*>(value ? 1u : 0u));
}
struct RestoreContext { std::shared_ptr<Session> value; std::exception_ptr error; };
std::uint64_t restore_op(void* opaque, std::uint32_t operation) noexcept {
    auto& c = *static_cast<RestoreContext*>(opaque);
    auto& s = c.value;
    try {
        switch (operation) {
        case 0: return s->restoring;
        case 1: s->restoring = true; return 0;
        case 2: KillTimer(s->window, handover_timer); return 0;
        case 3: return s->deferred_action >= 0;
        case 4: s->deferred_action = -1; menu_scene::end_handover(); return 0;
        case 5: a2fo_stage10_presentation_each(s->visuals.begin(),s->visuals.end(),[](const auto& v){set_visible(v,v.visible!=0);}); return 0;
        case 6: return session == s;
        case 7: session.reset(); active_window.store(nullptr, std::memory_order_release);
                InterlockedExchange(&active_count, 0); return 0;
        case 8: InvalidateRect(s->window, nullptr, TRUE); return 0;
        default: return 0;
        }
    } catch (...) { c.error = std::current_exception(); return UINT64_MAX; }
}
void restore(std::shared_ptr<Session> s, bool destroying) {
    RestoreContext c{std::move(s), {}};
    a2fo_rs_stage8_b_refit_restore(destroying, &c, restore_op);
    if (c.error) std::rethrow_exception(c.error);
}
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR);
void detach(std::shared_ptr<Session> s, bool destroying) {
    RemoveWindowSubclass(s->window, &window_proc, subclass_id);
    restore(s, destroying);
}
struct QueueContext {const std::shared_ptr<Session>& value;std::exception_ptr error;};
std::uint64_t queue_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<QueueContext*>(opaque);
    try{
        switch(id){
        case 0:return c.value->input.pending>=0;
        case 1:return PostMessageA(c.value->window,action_message,
            static_cast<WPARAM>(c.value->input.pending),c.value->generation);
        case 2:c.value->input.pending=-1;return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
bool queue(const std::shared_ptr<Session>& s) {
    QueueContext c{s,{}};
    const bool result=a2fo_rs_stage8_b_refit_queue(&c,queue_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
Layout geometry(const Session& s) {
    RECT rect{};
    return GetClientRect(s.window, &rect) ? layout(rect.right, rect.bottom, s.snapshot) : Layout{};
}
struct ActionContext {
    std::shared_ptr<Session> value;
    HWND window{};
    WPARAM wp{};
    LPARAM lp{};
    int row{-1};
    std::exception_ptr error;
};
std::uint64_t action_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<ActionContext*>(opaque);
    auto& s=c.value;
    try{
        switch(id){
        case 0:return c.wp==handover_timer&&s->deferred_action>=0;
        case 1:return menu_scene::handover_ready();
        case 2:return menu_scene::handover_active();
        case 3:return GetTickCount64()<s->handover_deadline;
        case 4:KillTimer(c.window,handover_timer);return 0;
        case 5:c.row=s->deferred_action;s->deferred_action=-1;return 0;
        case 6:menu_scene::end_handover();return 0;
        case 7:return capture(*s,false);
        case 8:detach(s,false);return 0;
        case 9:InterlockedIncrement(&action_count);return 0;
        case 10:a2fo_refit_menu_call(at(n::action_rvas[c.row]),s->form,s->form);return 0;
        case 11:return static_cast<std::uint32_t>(c.lp)==s->generation&&
            static_cast<int>(c.wp)==s->input.pending;
        case 12:return ready.load(std::memory_order_acquire);
        case 13:c.row=s->input.take(s->snapshot);return static_cast<std::uint64_t>(c.row+1);
        case 14:{HWND parent{};Reader reader;return n::field(reader,
            reinterpret_cast<std::uintptr_t>(s->form),n::parent_window_offset,parent)&&
            menu_scene::begin_handover(c.window,parent);}
        case 15:s->deferred_action=c.row;s->handover_deadline=GetTickCount64()+2000;return 0;
        case 16:return SetTimer(c.window,handover_timer,16,nullptr)!=0;
        case 17:s->deferred_action=-1;menu_scene::end_handover();return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
bool timer_action(const std::shared_ptr<Session>& s,HWND window,WPARAM wp) {
    ActionContext c{};c.value=s;c.window=window;c.wp=wp;
    const bool handled=a2fo_rs_stage8_b_refit_timer(&c,action_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return handled;
}
void queued_action(const std::shared_ptr<Session>& s,HWND window,WPARAM wp,LPARAM lp) {
    ActionContext c{};c.value=s;c.window=window;c.wp=wp;c.lp=lp;
    a2fo_rs_stage8_b_refit_action(&c,action_op);
    if(c.error)std::rethrow_exception(c.error);
}
struct PaintContext {
    std::shared_ptr<Session> value;
    HWND window{};
    PAINTSTRUCT ps{};
    HDC dc{};
    RECT rect{};
    const Artwork* live{};
    bool painted{};
    bool ended{};
    std::exception_ptr error;
};
std::uint64_t paint_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<PaintContext*>(opaque);
    try{
        switch(id){
        case 0:c.dc=BeginPaint(c.window,&c.ps);return c.dc!=nullptr;
        case 1:return menu_scene::handover_active();
        case 2:if(c.dc)EndPaint(c.window,&c.ps);c.ended=true;return 0;
        case 3:return c.dc&&GetClientRect(c.window,&c.rect);
        case 4:c.live=menu_scene::frame(c.window);return 0;
        case 5:c.painted=paint(c.dc,c.rect.right,c.rect.bottom,c.value->snapshot,
            c.value->input.hot,c.value->input.pressed,c.live?c.live:&c.value->artwork);
            return c.painted;
        case 6:menu_scene::frame_drawn(c.live,c.dc);return 0;
        case 7:return menu_scene::requested();
        case 8:menu_scene::paint_choices(c.dc,c.rect.right,c.rect.bottom);return 0;
        case 9:detach(c.value,false);return 0;
        case 10:log("Main-menu paint failed; native controls restored");return 0;
        default:return 0;
        }
    }catch(...){
        if(id>=3&&id<=8&&c.dc&&!c.ended){EndPaint(c.window,&c.ps);c.ended=true;}
        c.error=std::current_exception();return UINT64_MAX;
    }
}
void paint_window(const std::shared_ptr<Session>& s,HWND window) {
    PaintContext c{};c.value=s;c.window=window;
    a2fo_rs_stage8_b_refit_paint(&c,paint_op);
    if(c.error)std::rethrow_exception(c.error);
}
struct InputContext {
    std::shared_ptr<Session> value;
    HWND window{};
    LPARAM lp{};
    Layout geometry_value{};
    int x{},y{},scene{-1},old_hot{},old_pressed{};
    std::exception_ptr error;
};
std::uint64_t input_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<InputContext*>(opaque);
    auto& s=c.value;
    try{
        switch(id){
        case 0:c.old_hot=s->input.hot;c.old_pressed=s->input.pressed;return 0;
        case 1:c.geometry_value=geometry(*s);c.x=GET_X_LPARAM(c.lp);c.y=GET_Y_LPARAM(c.lp);return 0;
        case 2:{RECT client{};GetClientRect(c.window,&client);
            c.scene=menu_scene::choice_at(client.right,client.bottom,c.x,c.y);
            return static_cast<std::uint64_t>(c.scene+1);}
        case 3:s->pressed_scene=c.scene;s->input.leave();return 0;
        case 4:InvalidateRect(c.window,nullptr,FALSE);return 0;
        case 5:return s->pressed_scene>=0;
        case 6:return c.scene==s->pressed_scene;
        case 7:menu_scene::choose(c.scene);return 0;
        case 8:s->pressed_scene=-1;return 0;
        case 9:s->input.move(c.geometry_value,s->snapshot,c.x,c.y);return 0;
        case 10:{TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,c.window,0};TrackMouseEvent(&track);return 0;}
        case 11:s->input.down(c.geometry_value,s->snapshot,c.x,c.y);return 0;
        case 12:return s->input.up(c.geometry_value,s->snapshot,c.x,c.y);
        case 13:queue(s);return 0;
        case 14:return c.old_hot!=s->input.hot||c.old_pressed!=s->input.pressed;
        case 15:return (GetKeyState(VK_SHIFT)&0x8000)!=0;
        case 16:s->input.hot=next_enabled(s->snapshot,s->input.hot,-1);s->input.pressed=-1;return 0;
        case 17:s->input.hot=next_enabled(s->snapshot,s->input.hot,1);s->input.pressed=-1;return 0;
        case 18:return s->input.request(s->snapshot,s->input.hot);
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
void mouse_window(const std::shared_ptr<Session>& s,HWND window,UINT message,LPARAM lp) {
    InputContext c{};c.value=s;c.window=window;c.lp=lp;
    const auto kind=message==WM_MOUSEMOVE?0u:message==WM_LBUTTONDOWN?1u:2u;
    a2fo_rs_stage8_b_refit_mouse(kind,&c,input_op);
    if(c.error)std::rethrow_exception(c.error);
}
bool key_window(const std::shared_ptr<Session>& s,HWND window,WPARAM wp,LPARAM lp) {
    InputContext c{};c.value=s;c.window=window;c.lp=lp;
    const bool handled=a2fo_rs_stage8_b_refit_key(static_cast<std::uint32_t>(wp),
        static_cast<std::uint32_t>(lp),&c,input_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return handled;
}
struct DispatchContext {
    std::shared_ptr<Session> value;
    HWND window{};UINT message{};WPARAM wp{};LPARAM lp{};
    LRESULT result{};
    std::exception_ptr error;
};
std::uint64_t dispatch_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<DispatchContext*>(opaque);
    try{
        switch(id){
        case 0:detach(c.value,true);return 0;
        case 1:c.result=DefSubclassProc(c.window,c.message,c.wp,c.lp);return 0;
        case 2:detach(c.value,false);return 0;
        case 3:return timer_action(c.value,c.window,c.wp);
        case 4:queued_action(c.value,c.window,c.wp,c.lp);return 0;
        case 5:paint_window(c.value,c.window);return 0;
        case 6:InvalidateRect(c.window,nullptr,FALSE);return 0;
        case 7:return c.value->deferred_action>=0;
        case 8:mouse_window(c.value,c.window,c.message,c.lp);return 0;
        case 9:c.value->pressed_scene=-1;c.value->input.leave();
               InvalidateRect(c.window,nullptr,FALSE);return 0;
        case 10:return key_window(c.value,c.window,c.wp,c.lp);
        case 11:c.result=DefSubclassProc(c.window,c.message,c.wp,c.lp);return 0;
        case 12:c.result=1;return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    // Keep a strong local reference: a native action can synchronously destroy
    // this form before the callback returns.
    auto s = session;
    if (!s || s.get() != reinterpret_cast<Session*>(ref) || s->window != window)
        return DefSubclassProc(window, message, wp, lp);
    try {
        DispatchContext c{};c.value=s;c.window=window;c.message=message;c.wp=wp;c.lp=lp;
        a2fo_rs_stage8_b_refit_dispatch(message,restore_message,action_message,&c,dispatch_op);
        if(c.error)std::rethrow_exception(c.error);
        return c.result;
    } catch (...) {
        log("Main-menu presentation exception; restoring native controls");
        detach(s, false);
    }
    return DefSubclassProc(window, message, wp, lp);
}
struct AttachContext {
    void* form{};
    std::shared_ptr<Session> value;
    char window_class[64]{};
    std::exception_ptr error;
};
std::uint64_t attach_op(void* opaque, std::uint32_t operation) noexcept {
    auto& c = *static_cast<AttachContext*>(opaque);
    try {
        switch (operation) {
        case 0: return !session && ready.load(std::memory_order_acquire);
        case 1: c.value = std::make_shared<Session>(); c.value->form = c.form; return 0;
        case 2: { Reader reader; return n::field(reader, reinterpret_cast<std::uintptr_t>(c.form),
            n::handle_offset, c.value->window); }
        case 3: return IsWindow(c.value->window);
        case 4: return GetWindowThreadProcessId(c.value->window, nullptr) == GetCurrentThreadId();
        case 5: return GetClassNameA(c.value->window, c.window_class, sizeof(c.window_class)) != 0;
        case 6: return lstrcmpiA(c.window_class, "TMainForm") == 0;
        case 7: return capture(*c.value, true);
        case 8: log("Main form/control contract unavailable; keeping native menu"); return 0;
        case 9: c.value->generation = ++serial;
                if (!c.value->generation) c.value->generation = ++serial;
                return 0;
        case 10: c.value->artwork = skin_native::main_artwork(c.form); return 0;
        case 11: return SetWindowSubclass(c.value->window, &window_proc, subclass_id,
            reinterpret_cast<DWORD_PTR>(c.value.get()));
        case 12: log("Main-menu subclass unavailable; keeping native menu"); return 0;
        case 13: session = c.value;
                 active_window.store(c.value->window, std::memory_order_release); return 0;
        case 14: a2fo_stage10_presentation_each(c.value->visuals.begin(),c.value->visuals.end(),[](const auto& v){set_visible(v,false);}); return 0;
        case 15: InterlockedExchange(&active_count, 1); InterlockedIncrement(&shown_count); return 0;
        case 16: InvalidateRect(c.value->window, nullptr, TRUE); return 0;
        case 17: log(c.value->artwork.pixels.empty() ?
            "Refit main menu attached; artwork unavailable, gradient fallback; native FO actions retained" :
            "Refit main menu attached with mod background artwork; native FO actions retained"); return 0;
        case 18: setup_native::start(); return 0;
        default: return 0;
        }
    } catch (...) { c.error = std::current_exception(); return UINT64_MAX; }
}
void attach(void* form) {
    AttachContext c{};
    c.form = form;
    a2fo_rs_stage8_b_refit_attach(&c, attach_op);
    if (c.error) std::rethrow_exception(c.error);
}
struct InitContext {
    const A2FO_ModuleApi* supplied;
    std::string path;
    bool live_scene = false;
    menu_scene::SceneRenderWidth scene_width{};
    bool animate_loading = false;
    std::exception_ptr error;
};
std::uint64_t init_op(void* opaque, std::uint32_t operation) noexcept {
    auto& c = *static_cast<InitContext*>(opaque);
    try {
        switch (operation) {
        case 0: return c.supplied && c.supplied->api_version == A2FO_MODULE_API_VERSION &&
            A2FO_MODULE_API_HAS(c.supplied, install_inline_hook) && c.supplied->log &&
            c.supplied->root_directory && c.supplied->armada_module &&
            c.supplied->fleetops_module && c.supplied->install_inline_hook;
        case 1: return initialized;
        case 2: initialized = true; api = c.supplied; return 0;
        case 3: {
            const char* root = api->root_directory();
            if (!root || !*root) return 0;
            c.path = std::string(root) + "\\A2FORefitMenus.ini";
            return 1;
        }
        case 4: {
            char enabled[32]{};
            const auto count = GetPrivateProfileStringA("RefitMenus", "Enabled", "0", enabled,
                sizeof(enabled), c.path.c_str());
            const auto requested = a2fo::refit::parse_enabled(enabled);
            return count && count < sizeof(enabled)-1 && requested && *requested;
        }
        case 5: log("Disabled or invalid configuration; no hooks installed"); return 0;
        case 6: fo = static_cast<HMODULE>(api->fleetops_module()); return fo != nullptr;
        case 7: return a2fo::supported_armada::identify(static_cast<HMODULE>(api->armada_module())) ==
            a2fo::supported_armada::Identity::unsupported;
        case 8: return a2fo::supported_armada::identify(static_cast<HMODULE>(api->armada_module())) ==
            a2fo::supported_armada::Identity::canonical;
        case 9: return known_fleetops();
        case 10: log("Unsupported engine/FO identity or changed menu bindings; passive"); return 0;
        case 11: action_message = RegisterWindowMessageA("A2FORefitMenus.Action.v1"); return action_message;
        case 12: restore_message = RegisterWindowMessageA("A2FORefitMenus.Restore.v1"); return restore_message;
        case 13: {
            HMODULE resident = nullptr;
            return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<const char*>(&a2fo_refit_menu_show_bridge), &resident);
        }
        case 14: log("Lifetime/message setup failed; passive"); return 0;
        case 15: return api->install_inline_hook(at(n::show_rva),
            reinterpret_cast<void*>(&a2fo_refit_menu_show_bridge), n::show_prologue.size(),
            n::show_prologue.data(), &hook);
        case 16: log("Checked FormShow hook failed; passive"); return 0;
        case 17: next_show.store(hook.gateway, std::memory_order_release); return 0;
        case 18: {
            char setting[32]{};
            GetPrivateProfileStringA("RefitMenus", "LiveScene", "0", setting, sizeof(setting), c.path.c_str());
            const auto parsed = a2fo::refit::parse_enabled(setting);
            c.live_scene = parsed && *parsed;
            return 0;
        }
        case 19: {
            char setting[64]{};
            const auto count = GetPrivateProfileStringA("RefitMenus", "SceneRenderWidth", "0",
                setting, sizeof(setting), c.path.c_str());
            c.scene_width = menu_scene::parse_scene_render_width(setting);
            if (count >= sizeof(setting)-1) c.scene_width = {0, true};
            return c.scene_width.invalid;
        }
        case 20: log("Invalid SceneRenderWidth; using native menu resolution within safety limits"); return 0;
        case 21: menu_scene::configure(api, fo, c.live_scene, c.scene_width.width); return 0;
        case 22: return mission_backdrop::initialize(api,
            static_cast<HMODULE>(api->armada_module()), fo);
        case 23: log("Mission backdrop observer unavailable; native compatibility fallback retained"); return 0;
        case 24: {
            char setting[32]{};
            GetPrivateProfileStringA("RefitMenus", "InstantAction", "1", setting,
                sizeof(setting), c.path.c_str());
            const auto parsed = a2fo::refit::parse_enabled(setting);
            setup_native::configure(api, parsed && *parsed);
            return 0;
        }
        case 25: {
            char setting[32]{};
            GetPrivateProfileStringA("RefitMenus", "LoadingScreen", "0", setting,
                sizeof(setting), c.path.c_str());
            const auto parsed = a2fo::refit::parse_enabled(setting);
            return parsed && *parsed;
        }
        case 26: c.animate_loading = GetPrivateProfileIntA("RefitMenus", "LoadingAnimation", 0,
            c.path.c_str()) == 1; return 0;
        case 27: return loading_native::initialize(api,
            static_cast<HMODULE>(api->armada_module()), fo, c.animate_loading);
        case 28: log("Refit loading presentation unavailable; original Fleet Ops loading screen retained"); return 0;
        case 29: {
            char mode[256]{};
            std::snprintf(mode, sizeof(mode),
                "Background mode: LiveScene=%u; SceneRenderWidth=%d (0=native); config=%s",
                c.live_scene ? 1u : 0u, c.scene_width.width, c.path.c_str());
            log(mode); return 0;
        }
        case 30: return skin_native::initialize(api, fo);
        case 31: log("Shared menu skin unavailable; checked main-menu presentation retained; other assets stay native"); return 0;
        case 32: installed = true; ready.store(true, std::memory_order_release); return 0;
        case 33: log("Armed: Refit main menu and shared shell artwork/widget skin; native controls/actions retained; restart required"); return 0;
        default: return 0;
        }
    } catch (...) { c.error = std::current_exception(); return UINT64_MAX; }
}
bool initialize(const A2FO_ModuleApi* supplied) {
    InitContext c{};
    c.supplied = supplied;
    const bool result = a2fo_rs_stage8_b_refit_init(&c, init_op) != 0;
    if (c.error) std::rethrow_exception(c.error);
    return result;
}
struct PresentGateContext {HWND window;HDC dc;const char* view;std::exception_ptr error;};
std::uint64_t present_gate_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<PresentGateContext*>(opaque);
    try {
        switch(id){
        case 0:return ready.load(std::memory_order_acquire);
        case 1:return c.dc!=nullptr;
        case 2:return c.window!=nullptr;
        case 3:return GetWindowThreadProcessId(c.window,nullptr)==GetCurrentThreadId();
        case 4:return menu_scene::requested();
        case 5:return menu_scene::select_view(c.view);
        default:return 0;
        }
    } catch(...) {c.error=std::current_exception();return UINT64_MAX;}
}
bool present_gate(HWND window,HDC dc,const char* view){
    PresentGateContext c{window,dc,view,{}};
    const bool result=a2fo_rs_stage8_b_refit_present_gate(&c,present_gate_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
struct PresentContext {
    HWND window{};HDC dc{};int width{},height{};unsigned flags{};
    std::shared_ptr<const Artwork> loading;
    const Artwork* image{};
    BITMAPINFO info{};
    int saved{},drawn{};
    std::exception_ptr error;
};
std::uint64_t present_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<PresentContext*>(opaque);
    try{
        switch(id){
        case 0:return (c.flags&A2FO_MENU_SCENE_DEPARTURE)||menu_scene::departing();
        case 1:c.loading=menu_scene::loading_frame(c.width,c.height);c.image=c.loading.get();return 0;
        case 2:c.image=menu_scene::frame(c.window,true,true);return 0;
        case 3:return c.image!=nullptr;
        case 4:return c.image->width>0;
        case 5:return c.image->height>0;
        case 6:c.info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            c.info.bmiHeader.biWidth=c.image->width;c.info.bmiHeader.biHeight=-c.image->height;
            c.info.bmiHeader.biPlanes=1;c.info.bmiHeader.biBitCount=32;
            c.info.bmiHeader.biCompression=BI_RGB;return 0;
        case 7:c.saved=SaveDC(c.dc);return c.saved!=0;
        case 8:SetStretchBltMode(c.dc,COLORONCOLOR);return 0;
        case 9:c.drawn=StretchDIBits(c.dc,0,0,c.width,c.height,0,0,c.image->width,c.image->height,
            c.image->pixels.data(),&c.info,DIB_RGB_COLORS,SRCCOPY);return 0;
        case 10:RestoreDC(c.dc,c.saved);return 0;
        case 11:return c.drawn!=0&&c.drawn!=static_cast<int>(GDI_ERROR);
        case 12:menu_scene::frame_drawn(c.image,c.dc);return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
bool present_scene(HWND window,HDC dc,int width,int height,unsigned flags){
    PresentContext c{};c.window=window;c.dc=dc;c.width=width;c.height=height;c.flags=flags;
    const bool result=a2fo_rs_stage8_b_refit_present(&c,present_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
struct ShutdownContext {HWND window{};std::exception_ptr error;};
std::uint64_t shutdown_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<ShutdownContext*>(opaque);
    try {
        switch(id){
        case 0:ready.store(false,std::memory_order_release);return 0;
        case 1:loading_native::shutdown();return 0;
        case 2:mission_backdrop::shutdown();return 0;
        case 3:skin_native::shutdown();return 0;
        case 4:setup_native::shutdown();return 0;
        case 5:menu_scene::shutdown();return 0;
        case 6:c.window=active_window.load(std::memory_order_acquire);return c.window!=nullptr;
        case 7:PostMessageA(c.window,restore_message,0,0);return 0;
        case 8:log("Future menu replacement disabled; active form restoration queued on its UI thread; restart removes resident hook");return 0;
        default:return 0;
        }
    } catch(...) {c.error=std::current_exception();return UINT64_MAX;}
}
struct GuardContext {
    const char* view{};
    const A2FO_MenuMissionV1* mission{};
    bool use_mission{};
    std::exception_ptr error;
};
std::uint64_t guard_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<GuardContext*>(opaque);
    try{
        switch(id){
        case 0:return ready.load(std::memory_order_acquire);
        case 1:return c.use_mission?loading_native::set_mission(c.mission):menu_scene::begin_departure(c.view);
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
struct StatusContext {Status* out{};std::exception_ptr error;};
std::uint64_t status_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<StatusContext*>(opaque);
    try{
        switch(id){
        case 0:return c.out!=nullptr;
        case 1:return c.out->struct_size==sizeof(*c.out);
        case 2:*c.out={sizeof(*c.out),1,installed?1u:0u,
            static_cast<std::uint32_t>(InterlockedCompareExchange(&active_count,0,0)),
            static_cast<std::uint32_t>(InterlockedCompareExchange(&shown_count,0,0)),
            static_cast<std::uint32_t>(InterlockedCompareExchange(&action_count,0,0))};return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
struct EndDepartureContext {std::exception_ptr error;};
std::uint64_t end_departure_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<EndDepartureContext*>(opaque);
    try{
        if(id==0){menu_scene::end_departure();return 0;}
        if(id==1){loading_native::clear_mission();return 0;}
        return 0;
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
} // namespace
// Native Delphi guarantees four-byte stack alignment, while our C++ callees
// may expect sixteen. Align at the foreign-ABI entry, not in the gateway.
extern "C" __attribute__((force_align_arg_pointer)) void a2fo_refit_menu_show(void* form, void* sender) {
    void* next = nullptr;
    while (!(next = next_show.load(std::memory_order_acquire))) Sleep(0);
    a2fo_refit_menu_call(next, form, sender);
    try { attach(form); }
    catch (...) { if (session) detach(session, false); log("Main-menu attach failed; native fallback retained"); }
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* supplied) {
    try { return initialize(supplied); } catch (...) { log("Initialization exception; passive"); return true; }
}

extern "C" __declspec(dllexport) BOOL __cdecl A2FORefitMenus_PresentSceneV1(
    HWND window,HDC dc,int width,int height,const char* view,unsigned flags) {
    if(!present_gate(window,dc,view))return FALSE;
    try{
        return present_scene(window,dc,width,height,flags);
    }catch(...){return FALSE;}
}
extern "C" __declspec(dllexport) BOOL __cdecl A2FORefitMenus_BeginDepartureV1(const char* view){
    try{
        GuardContext c{};c.view=view;
        const bool result=a2fo_rs_stage8_b_refit_guarded_call(&c,guard_op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        return result;
    }catch(...){return FALSE;}
}
extern "C" __declspec(dllexport) void __cdecl A2FORefitMenus_EndDepartureV1(){
    EndDepartureContext c{};
    a2fo_rs_stage8_b_refit_end_departure(&c,end_departure_op);
    if(c.error)std::rethrow_exception(c.error);
}
extern "C" __declspec(dllexport) BOOL __cdecl A2FORefitMenus_SetMissionV1(const A2FO_MenuMissionV1* mission){
    try{
        GuardContext c{};c.mission=mission;c.use_mission=true;
        const bool result=a2fo_rs_stage8_b_refit_guarded_call(&c,guard_op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        return result;
    }catch(...){return FALSE;}
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FORefitMenus_QueryStatus(a2fo::refit_menu::Status* out) {
    StatusContext c{};c.out=out;
    const bool result=a2fo_rs_stage8_b_refit_status(&c,status_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    ShutdownContext c{};
    a2fo_rs_stage8_b_refit_shutdown(&c,shutdown_op);
    if(c.error)std::rethrow_exception(c.error);
}
