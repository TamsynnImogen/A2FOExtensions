// Synthetic USER32 host for shell/Options lifecycle; creates no HWNDs.
#include "../modules/A2FOShellDisplay/canvas.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
extern "C" { int g_native_bounds{}; }

BOOL WINAPI host_IsWindow(HWND);
BOOL WINAPI host_IsWindowVisible(HWND);
BOOL WINAPI host_GetClientRect(HWND, LPRECT);
BOOL WINAPI host_GetWindowRect(HWND, LPRECT);
HWND WINAPI host_GetParent(HWND);
LONG_PTR WINAPI host_GetWindowLongPtrA(HWND, int);
LONG_PTR WINAPI host_SetWindowLongPtrA(HWND, int, LONG_PTR);
BOOL WINAPI host_ScreenToClient(HWND, LPPOINT);
BOOL WINAPI host_ClientToScreen(HWND, LPPOINT);
BOOL WINAPI host_SetWindowPos(HWND, HWND, int, int, int, int, UINT);
BOOL WINAPI host_MoveWindow(HWND, int, int, int, int, BOOL);
BOOL WINAPI host_EnumChildWindows(HWND, WNDENUMPROC, LPARAM);
LRESULT WINAPI host_SendMessageA(HWND, UINT, WPARAM, LPARAM);
BOOL WINAPI host_InvalidateRect(HWND, const RECT*, BOOL);
LRESULT WINAPI host_CallWindowProcA(WNDPROC, HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI host_DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
int WINAPI host_GetClassNameA(HWND, LPSTR, int);
BOOL WINAPI host_GetCursorPos(LPPOINT);
HDC WINAPI host_GetDC(HWND);
int WINAPI host_ReleaseDC(HWND,HDC);

#define IsWindow host_IsWindow
#define IsWindowVisible host_IsWindowVisible
#define GetClientRect host_GetClientRect
#define GetWindowRect host_GetWindowRect
#define GetParent host_GetParent
#undef GetWindowLongPtrA
#undef SetWindowLongPtrA
#define GetWindowLongPtrA host_GetWindowLongPtrA
#define SetWindowLongPtrA host_SetWindowLongPtrA
#define ScreenToClient host_ScreenToClient
#define ClientToScreen host_ClientToScreen
#define SetWindowPos host_SetWindowPos
#define MoveWindow host_MoveWindow
#define EnumChildWindows host_EnumChildWindows
#define SendMessageA host_SendMessageA
#define InvalidateRect host_InvalidateRect
#define CallWindowProcA host_CallWindowProcA
#define DefWindowProcA host_DefWindowProcA
#define GetClassNameA host_GetClassNameA
#define GetCursorPos host_GetCursorPos
#define GetDC host_GetDC
#define ReleaseDC host_ReleaseDC
#include "../modules/A2FOShellDisplay/canvas.cpp"
#undef IsWindow
#undef IsWindowVisible
#undef GetClientRect
#undef GetWindowRect
#undef GetParent
#undef GetWindowLongPtrA
#undef SetWindowLongPtrA
#undef ScreenToClient
#undef ClientToScreen
#undef SetWindowPos
#undef MoveWindow
#undef EnumChildWindows
#undef SendMessageA
#undef InvalidateRect
#undef CallWindowProcA
#undef DefWindowProcA
#undef GetClassNameA
#undef GetCursorPos
#undef GetDC
#undef ReleaseDC

namespace shell = a2fo::shell_display::canvas;
namespace {
struct FakeWindow {
    HWND parent{};
    LONG_PTR style{};
    RECT outer{};
    int client_width{}, client_height{};
    WNDPROC proc{};
    void* control{};
    std::string name{"TFakeVCL"};
    WNDPROC dlgproc{};
    LONG_PTR exstyle{};
    bool visible{};
};
std::map<HWND, FakeWindow*> g_host;
int g_invalidations{};
unsigned g_native_registrations{};
LPARAM g_mouse{};
UINT g_last_message{};
POINT g_cursor{};
bool g_cursor_ok = true;
HWND g_checkbox_parent{};
constexpr RECT kCheckbox{100, 120, 400, 180};
bool g_checkbox_checked{};
unsigned g_checkbox_queries{};
POINT g_checkbox_local{};
unsigned g_seed_calls{};
unsigned g_refresh_calls{};
HWND g_registration_window{};
HDC g_registration_dc{};
bool g_registration_saw_seed{};
int g_seed_behavior{};
COLORREF g_seed_color=RGB(255,255,255);
int g_refresh_behavior{};
unsigned g_completion_calls{};
HWND g_completed_window{};
HDC g_completed_logical{};
BOOL g_completed_presented{};
bool g_completion_throws{};
void __cdecl canvas_completed(HWND window,HDC logical,BOOL presented,void*){
    ++g_completion_calls;
    g_completed_window=window;g_completed_logical=logical;
    g_completed_presented=presented;
    if(g_completion_throws)throw 1;
}
BOOL __cdecl refuse_present(HWND,HDC,void*){return FALSE;}
HWND g_native_clear_window{};
HDC g_native_clear_dc{};
constexpr LRESULT kNativeShowResult=77;
BOOL __cdecl seed_canvas(HWND, HDC logical, int width, int height, void*) {
    ++g_seed_calls;
    HBRUSH brush=CreateSolidBrush(g_seed_color);
    if(!brush)return FALSE;
    const RECT bounds{0,0,width,height};
    const BOOL painted=FillRect(logical,&bounds,brush);
    DeleteObject(brush);
    if (g_seed_behavior == 2) throw 1;
    if(!painted)return FALSE;
    return g_seed_behavior == 0;
}
BOOL __cdecl refresh_canvas(HWND, HDC logical, int width, int height, void*) {
    ++g_refresh_calls;
    HBRUSH brush=CreateSolidBrush(g_seed_color);
    if(!brush)return FALSE;
    const RECT bounds{0,0,width,height};
    const BOOL painted=FillRect(logical,&bounds,brush);
    DeleteObject(brush);
    if(g_refresh_behavior==2)throw 1;
    return painted&&g_refresh_behavior==0;
}
bool check(bool okay, const char* label) {
    if (!okay) std::fprintf(stderr, "FAIL: %s\n", label);
    return okay;
}
HWND add(FakeWindow& window) {
    HWND handle = reinterpret_cast<HWND>(&window);
    g_host.emplace(handle, &window);
    return handle;
}
FakeWindow* find(HWND handle) {
    const auto it = g_host.find(handle);
    return it == g_host.end() ? nullptr : it->second;
}
LRESULT CALLBACK vcl_proc(HWND handle, UINT message, WPARAM, LPARAM parameter) {
    auto* window = find(handle);
    if (!window) return 0;
    if(handle==g_native_clear_window && (message==WM_SHOWWINDOW || message==WM_WINDOWPOSCHANGED)){
        PatBlt(g_native_clear_dc,0,0,1920,1060,BLACKNESS);
    }
    if (message == WM_WINDOWPOSCHANGED || message == WM_SIZE) {
        if (window->control) shell::a2fo_shell_bounds_handler(window->control);
    } else if (message == WM_ERASEBKGND) {
        g_last_message=message;
    } else if (message == WM_LBUTTONDOWN) {
        g_mouse = parameter;
        g_last_message = message;
    } else if (message == WM_LBUTTONUP && handle == g_checkbox_parent) {
        POINT cursor{};
        if (shell::checkbox_cursor_pos(&cursor)) {
            ++g_checkbox_queries;
            // Model TJvgCheckBox's TControl.ScreenToClient: its parent has a
            // physical screen origin, while Left/Top remain logical.
            g_checkbox_local = POINT{cursor.x - window->outer.left - kCheckbox.left,
                                     cursor.y - window->outer.top - kCheckbox.top};
            const RECT local{0, 0, kCheckbox.right - kCheckbox.left,
                             kCheckbox.bottom - kCheckbox.top};
            if (PtInRect(&local, g_checkbox_local)) g_checkbox_checked = !g_checkbox_checked;
        }
    }
    return handle==g_native_clear_window&&message==WM_SHOWWINDOW?kNativeShowResult:0;
}
template<std::size_t N> void set_handle(std::array<std::uint8_t, N>& control, HWND handle) {
    static_assert(N >= 0x1b8, "VCL handle field requires a full synthetic control");
    std::memcpy(control.data() + 0x1b4, &handle, sizeof(handle));
}
template<std::size_t N> int field(const std::array<std::uint8_t, N>& control, std::size_t offset) {
    int result{};
    std::memcpy(&result, control.data() + offset, sizeof(result));
    return result;
}
bool checkbox_release(HWND parent, int logical_x, int logical_y, bool expected,
                      const char* label) {
    auto record = shell::lookup(parent);
    auto* host = find(parent);
    if (!record || !host) return check(false, label);
    const shell::Scale scale = shell::local_scale(*record);
    g_cursor = POINT{host->outer.left + scale.x(logical_x),
                     host->outer.top + scale.y(logical_y)};
    const unsigned before = g_checkbox_queries;
    shell::window_proc(parent, WM_LBUTTONUP, 0,
                       MAKELPARAM(static_cast<short>(scale.x(logical_x)),
                                  static_cast<short>(scale.y(logical_y))));
    return check(g_checkbox_queries == before + 1 && g_checkbox_checked == expected &&
                 std::abs(g_checkbox_local.x - (logical_x - kCheckbox.left)) <= 1 &&
                 std::abs(g_checkbox_local.y - (logical_y - kCheckbox.top)) <= 1, label);
}
extern "C" void __cdecl native_register(void*, HWND window, int left, int top, int width, int height) {
    ++g_native_registrations;
    if(window==g_registration_window){
        g_registration_saw_seed=GetPixel(g_registration_dc,800,500)==RGB(255,255,255);
        g_seed_color=RGB(0,128,255);
    }
    // Model the original registration's move through the already-patched
    // USER32 import, after the root has been recorded by our hook.
    shell::move_window(window, left, top, width, height, TRUE);
}
extern "C" __attribute__((naked)) void register_gateway() {
    __asm__ volatile(
        "pushl %ebp\n movl %esp, %ebp\n"
        "pushl %ebx\n pushl %esi\n pushl %edi\n"
        "pushl 8(%ebp)\n pushl 12(%ebp)\n pushl 16(%ebp)\n"
        "pushl %ecx\n pushl %edx\n pushl %eax\n"
        "call _native_register\n addl $24, %esp\n"
        "popl %edi\n popl %esi\n popl %ebx\n leave\n ret $12\n");
}
extern "C" __attribute__((naked)) void bounds_gateway() {
    __asm__ volatile("incl _g_native_bounds\n ret");
}
} // namespace

// USER32 methods used by this case operate only on FakeWindow records.
BOOL WINAPI host_IsWindow(HWND handle) { return find(handle) != nullptr; }
BOOL WINAPI host_IsWindowVisible(HWND handle) {
    auto* window=find(handle);
    return window&&window->visible;
}
BOOL WINAPI host_GetClientRect(HWND handle, LPRECT rect) {
    auto* window = find(handle);
    if (!window || !rect) return FALSE;
    *rect = RECT{0, 0, window->client_width, window->client_height};
    return TRUE;
}
BOOL WINAPI host_GetWindowRect(HWND handle, LPRECT rect) {
    auto* window = find(handle);
    if (!window || !rect) return FALSE;
    *rect = window->outer;
    return TRUE;
}
HWND WINAPI host_GetParent(HWND handle) {
    auto* window = find(handle);
    return window ? window->parent : nullptr;
}
LONG_PTR WINAPI host_GetWindowLongPtrA(HWND handle, int field) {
    auto* window = find(handle);
    if (!window) return 0;
    if(field==DWLP_DLGPROC)return reinterpret_cast<LONG_PTR>(window->dlgproc);
    if (field == GWL_STYLE) return window->style;
    if (field == GWL_EXSTYLE) return window->exstyle;
    return field == GWLP_WNDPROC ? reinterpret_cast<LONG_PTR>(window->proc) : 0;
}
LONG_PTR WINAPI host_SetWindowLongPtrA(HWND handle, int field, LONG_PTR value) {
    auto* window = find(handle);
    if(!window)return 0;
    if(field==DWLP_DLGPROC){auto previous=window->dlgproc;window->dlgproc=reinterpret_cast<WNDPROC>(value);return reinterpret_cast<LONG_PTR>(previous);}
    if(field==GWL_EXSTYLE){const auto previous=window->exstyle;window->exstyle=value;return previous;}
    if(field!=GWLP_WNDPROC)return 0;
    const auto old = reinterpret_cast<LONG_PTR>(window->proc);
    window->proc = reinterpret_cast<WNDPROC>(value);
    return old;
}
BOOL WINAPI host_ScreenToClient(HWND handle, LPPOINT point) {
    auto* window = find(handle);
    if (!window || !point) return FALSE;
    point->x -= window->outer.left; point->y -= window->outer.top;
    return TRUE;
}
BOOL WINAPI host_ClientToScreen(HWND handle, LPPOINT point) {
    auto* window = find(handle);
    if (!window || !point) return FALSE;
    point->x += window->outer.left; point->y += window->outer.top;
    return TRUE;
}
BOOL WINAPI host_SetWindowPos(HWND handle, HWND, int x, int y, int width, int height, UINT flags) {
    auto* window = find(handle);
    if (!window) return FALSE;
    if (flags & SWP_NOMOVE) { x = window->outer.left; y = window->outer.top; }
    if (flags & SWP_NOSIZE) {
        width = window->outer.right - window->outer.left;
        height = window->outer.bottom - window->outer.top;
    }
    window->outer = RECT{x, y, x + width, y + height};
    window->client_width = width; window->client_height = height;
    WINDOWPOS position{handle, nullptr, x, y, width, height, flags};
    if (window->proc) {
        window->proc(handle, WM_WINDOWPOSCHANGED, 0, reinterpret_cast<LPARAM>(&position));
        window->proc(handle, WM_SIZE, 0, MAKELPARAM(width, height));
    }
    return TRUE;
}
BOOL WINAPI host_MoveWindow(HWND handle, int x, int y, int width, int height, BOOL) {
    return host_SetWindowPos(handle, nullptr, x, y, width, height, 0);
}
BOOL WINAPI host_EnumChildWindows(HWND parent, WNDENUMPROC callback, LPARAM data) {
    for (const auto& entry : g_host)
        if (entry.second->parent == parent && (entry.second->style & WS_CHILD))
            if (!callback(entry.first, data)) break;
    return TRUE;
}
LRESULT WINAPI host_SendMessageA(HWND, UINT, WPARAM, LPARAM) { return 0; }
BOOL WINAPI host_InvalidateRect(HWND, const RECT*, BOOL) { ++g_invalidations; return TRUE; }
HDC WINAPI host_GetDC(HWND window){return window==g_registration_window?g_registration_dc:nullptr;}
int WINAPI host_ReleaseDC(HWND window,HDC dc){return window==g_registration_window&&dc==g_registration_dc?1:0;}
LRESULT WINAPI host_CallWindowProcA(WNDPROC procedure, HWND window, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
    return procedure ? procedure(window, message, wparam, lparam) : 0;
}
LRESULT WINAPI host_DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }
int WINAPI host_GetClassNameA(HWND handle, LPSTR output, int capacity) {
    auto* window = find(handle);
    if (!window || !output || capacity <= 0) return 0;
    lstrcpynA(output, window->name.c_str(), capacity);
    return static_cast<int>(std::strlen(output));
}
BOOL WINAPI host_GetCursorPos(LPPOINT point) {
    if (!g_cursor_ok || !point) return FALSE;
    *point = g_cursor;
    return TRUE;
}

int main() {
    using a2fo::shell_display::Mode;
    // This code page is private test memory; AdjustSize is a single RET.
    constexpr std::size_t image_size = shell::kScreenDialogProc + 4096;
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, image_size,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!check(image != nullptr, "synthetic FO image allocated")) return 1;
    image[0xc4c20] = 0xc3;
    shell::g_fleetops = reinterpret_cast<HMODULE>(image);
    shell::g_register_next.store(reinterpret_cast<void*>(&register_gateway));
    shell::g_bounds_next.store(reinterpret_cast<void*>(&bounds_gateway));
    shell::g_ready.store(true);
    const Mode full{1920, 1080, 32};
    shell::active_mode(&full);

    FakeWindow owner{nullptr, static_cast<LONG_PTR>(WS_POPUP), RECT{0,0,1920,1060}, 1920,1060, vcl_proc};
    FakeWindow menu{nullptr, static_cast<LONG_PTR>(WS_POPUP), RECT{0,0,1200,900}, 1200,900, vcl_proc};
    menu.name = "TMainForm";
    menu.exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
    HWND owner_handle = add(owner), menu_handle = add(menu);
    std::array<std::uint8_t, 0x1b8> main_form{};
    set_handle(main_form, owner_handle);
    std::array<std::uint8_t, 0x1b8> menu_control{};
    set_handle(menu_control, menu_handle);
    menu.control = menu_control.data();
    shell::a2fo_shell_register_handler(main_form.data(), menu_handle, 0, 0, 1200, 900);
    if (!(menu.client_width == 1920 && menu.client_height == 1060 &&
          field(menu_control, 0x48) == 1200 && field(menu_control, 0x4c) == 900))
        std::fprintf(stderr, "menu physical=%dx%d logical=%dx%d registered=%d\n",
            menu.client_width, menu.client_height, field(menu_control, 0x48), field(menu_control, 0x4c),
            shell::lookup(menu_handle) ? 1 : 0);
    if (!check(shell::lookup(menu_handle) && shell::lookup(menu_handle)->root_scale.physical_height == 1060 &&
               menu.client_width == 1920 && menu.client_height == 1060 &&
               field(menu_control, 0x48) == 1200 && field(menu_control, 0x4c) == 900 &&
               g_invalidations > 0 && menu.exstyle == (WS_EX_APPWINDOW | WS_EX_TOPMOST),
               "native registration moves main canvas but VCL bounds remain logical")) return 1;
    shell::active_mode(&full);
    if (!check(menu.client_width == 1920 && menu.client_height == 1060 &&
               field(menu_control, 0x48) == 1200 && field(menu_control, 0x4c) == 900,
               "main resize writes logical VCL Width/Height")) return 1;
    g_checkbox_parent = menu_handle;
    if (!checkbox_release(menu_handle, 115, 140, true, "checkbox glyph toggles on") ||
        !checkbox_release(menu_handle, 350, 140, false, "checkbox label toggles off")) return 1;
    const shell::Scale full_scale = shell::local_scale(*shell::lookup(menu_handle));
    const POINT legacy_inside{full_scale.x(80) - kCheckbox.left,
                              full_scale.y(140) - kCheckbox.top};
    const RECT checkbox_local{0, 0, kCheckbox.right - kCheckbox.left,
                              kCheckbox.bottom - kCheckbox.top};
    if (!check(PtInRect(&checkbox_local, legacy_inside) &&
               checkbox_release(menu_handle, 80, 140, false,
                   "physical cursor that falsely hits unscaled box stays outside") &&
               checkbox_release(menu_handle, 420, 140, false, "right outside stays outside"),
               "legacy false positive is excluded")) return 1;

    FakeWindow options{menu_handle, static_cast<LONG_PTR>(WS_POPUP), RECT{0,0,1200,900}, 1200,900, vcl_proc};
    options.name = "TOptionsMenuForm";
    options.exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
    HWND options_handle = add(options);
    RECT client{};
    shell::client_rect(options_handle, &client);
    if (!check(!shell::lookup(options_handle), "owned WS_POPUP awaits explicit registration")) return 1;
    std::array<std::uint8_t, 0x1b8> options_control{};
    set_handle(options_control, options_handle);
    options.control = options_control.data();
    shell::a2fo_shell_register_handler(main_form.data(), options_handle, 0, 0, 1200, 900);
    if (!check(shell::lookup(options_handle) && shell::lookup(options_handle)->root == options_handle &&
               options.client_width == 1920 && options.client_height == 1060 &&
               field(options_control, 0x48) == 1200 && field(options_control, 0x4c) == 900 &&
               options.exstyle == (WS_EX_TOOLWINDOW | WS_EX_TOPMOST),
               "Options registration retains bounds and only changes taskbar styles")) return 1;

    FakeWindow button{options_handle, WS_CHILD, RECT{0,0,200,40}, 200,40, vcl_proc};
    HWND button_handle = add(button);
    std::array<std::uint8_t, 0x1b8> button_control{};
    set_handle(button_control, button_handle);
    button.control = button_control.data();
    shell::client_rect(button_handle, &client);
    if (!check(static_cast<bool>(shell::lookup(button_handle)), "late WS_CHILD adopts into Options root")) return 1;
    // A partial first paint must publish seeded pixels outside its dirty clip.
    BITMAPINFO physical_info{};
    physical_info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    physical_info.bmiHeader.biWidth=1920;
    physical_info.bmiHeader.biHeight=-1060;
    physical_info.bmiHeader.biPlanes=1;
    physical_info.bmiHeader.biBitCount=32;
    physical_info.bmiHeader.biCompression=BI_RGB;
    HDC physical=CreateCompatibleDC(nullptr);
    void* physical_pixels{};
    HBITMAP physical_bitmap=CreateDIBSection(physical,&physical_info,DIB_RGB_COLORS,
                                             &physical_pixels,nullptr,0);
    HGDIOBJ physical_old=physical_bitmap?SelectObject(physical,physical_bitmap):nullptr;
    if(!check(physical&&physical_bitmap&&physical_old&&physical_old!=HGDI_ERROR,
              "seed test physical bitmap"))return 1;
    PAINTSTRUCT partial{};
    partial.rcPaint=RECT{0,0,20,20};
    if(!check(A2FO_SetShellCanvasSeedV1(&seed_canvas,nullptr),"register thread-local seed"))return 1;
    g_last_message=0;
    if(!check(shell::window_proc(options_handle,WM_ERASEBKGND,reinterpret_cast<WPARAM>(physical),0)==1&&
              g_seed_calls==1&&g_last_message==0&&GetPixel(physical,800,500)==RGB(255,255,255),
              "early erase publishes retained pixels before first root paint and skips native black erase"))return 1;
    HDC logical=shell::acquire(options_handle,physical,&partial);
    if(!check(logical!=physical&&g_seed_calls==1&&
              GetPixel(logical,500,400)==RGB(255,255,255),
              "fresh root seeded before partial lease clip"))return 1;
    PatBlt(logical,0,0,10,10,BLACKNESS);
    if(!check(shell::finish(options_handle,logical,true,nullptr,nullptr)&&
              GetPixel(physical,800,500)==RGB(255,255,255)&&
              GetPixel(physical,0,0)==RGB(0,0,0),
              "partial first publish preserves seeded outside pixels"))return 1;
    g_last_message=0;
    shell::window_proc(options_handle,WM_ERASEBKGND,reinterpret_cast<WPARAM>(physical),0);
    if(!check(g_last_message==WM_ERASEBKGND&&g_seed_calls==1,
              "completed root paint restores native erase ownership"))return 1;
    logical=shell::acquire(options_handle,physical,nullptr);
    if(!check(logical!=physical&&g_seed_calls==1&&
              shell::finish(options_handle,logical,false,nullptr,nullptr),
              "cached root does not reseed"))return 1;
    logical=shell::acquire(button_handle,physical,nullptr);
    if(!check(logical!=physical&&g_seed_calls==1&&
              shell::finish(button_handle,logical,false,nullptr,nullptr),
              "child backing canvas does not seed"))return 1;
    if(!check(A2FO_SetShellCanvasCompletionV1(&canvas_completed,nullptr),
              "register UI-thread completion callback"))return 1;
    logical=shell::acquire(options_handle,physical,nullptr);
    HDC released_physical{};
    if(!check(logical!=physical&&shell::finish(options_handle,logical,false,nullptr,&released_physical)&&
              released_physical==physical&&g_completion_calls==1&&
              g_completed_window==options_handle&&g_completed_logical==logical&&g_completed_presented,
              "nonpaint lease reports successful physical presentation once"))return 1;
    if(!check(!shell::finish(options_handle,logical,false,nullptr,nullptr)&&g_completion_calls==1,
              "unmatched finish does not invoke completion"))return 1;
    if(!check(A2FO_SetShellCanvasPresenterV1(options_handle,&refuse_present,nullptr),
              "register refusing presenter"))return 1;
    logical=shell::acquire(options_handle,physical,nullptr);
    options.client_width=0;
    if(!check(logical!=physical&&shell::finish(options_handle,logical,false,nullptr,&released_physical)&&
              released_physical==physical&&g_completion_calls==2&&!g_completed_presented,
              "refused presenter and failed physical fallback report failure"))return 1;
    options.client_width=1920;
    if(!check(A2FO_SetShellCanvasPresenterV1(options_handle,nullptr,nullptr),
              "clear refusing presenter"))return 1;
    logical=shell::acquire(options_handle,physical,&partial);
    g_completion_throws=true;
    if(!check(logical!=physical&&shell::finish(options_handle,logical,true,nullptr,nullptr)&&
              g_completion_calls==3&&g_completed_presented&&
              shell::lookup(options_handle)->native_presented,
              "throwing completion preserves successful native paint state"))return 1;
    g_completion_throws=false;
    if(!check(A2FO_SetShellCanvasCompletionV1(nullptr,reinterpret_cast<void*>(1))&&
              !shell::g_completion&&!shell::g_completion_context,
              "clearing completion also clears context"))return 1;
    for(int behavior: {1,2}) {
        shell::lookup(options_handle)->surface.reset();
        g_seed_behavior=behavior;
        logical=shell::acquire(options_handle,physical,nullptr);
        if(!check(logical!=physical&&g_seed_calls==static_cast<unsigned>(behavior+1)&&
                  GetPixel(logical,500,400)==RGB(0,0,0)&&
                  shell::finish(options_handle,logical,false,nullptr,nullptr)&&
                  GetPixel(physical,800,500)==RGB(0,0,0),
                  "failed or throwing seed falls back to native black canvas"))return 1;
    }
    shell::lookup(options_handle)->native_presented=false;
    shell::lookup(options_handle)->surface.reset();
    g_seed_behavior=1;g_last_message=0;
    const auto seed_attempts=g_seed_calls;
    shell::window_proc(options_handle,WM_ERASEBKGND,reinterpret_cast<WPARAM>(physical),0);
    if(!check(g_last_message==WM_ERASEBKGND&&g_seed_calls==seed_attempts+1,
              "refused early seed forwards the native erase once"))return 1;
    FakeWindow arriving{menu_handle,static_cast<LONG_PTR>(WS_POPUP),RECT{0,0,1200,900},1200,900,vcl_proc};
    arriving.name="TOptionsMenuForm";
    const HWND arriving_handle=add(arriving);
    std::array<std::uint8_t,0x1b8> arriving_control{};
    set_handle(arriving_control,arriving_handle);arriving.control=arriving_control.data();
    g_registration_window=arriving_handle;g_registration_dc=physical;g_registration_saw_seed=false;
    g_seed_behavior=0;PatBlt(physical,0,0,1920,1060,BLACKNESS);
    const auto early_calls=g_seed_calls,registrations=g_native_registrations;
    shell::a2fo_shell_register_handler(main_form.data(),arriving_handle,0,0,1200,900);
    if(!check(g_registration_saw_seed&&g_native_registrations==registrations+1&&g_seed_calls==early_calls+1&&
              g_refresh_calls==0&&GetPixel(physical,1800,1000)==RGB(255,255,255),
              "legacy seed runs once per allocation without a refresh callback"))return 1;
    g_native_clear_window=arriving_handle;g_native_clear_dc=physical;
    const unsigned cached_calls=g_seed_calls;
    if(!check(shell::window_proc(arriving_handle,WM_ERASEBKGND,
                 reinterpret_cast<WPARAM>(physical),0)==1&&
              GetPixel(physical,800,500)==RGB(255,255,255)&&g_seed_calls==cached_calls&&
              g_refresh_calls==0,
              "without optional refresh early erase replays the legacy cache"))return 1;
    if(!check(A2FO_SetShellCanvasRefreshV1(&refresh_canvas,nullptr),
              "register optional thread-local refresh"))return 1;
    unsigned refresh_calls=g_refresh_calls;
    if(!check(shell::window_proc(arriving_handle,WM_ERASEBKGND,
                 reinterpret_cast<WPARAM>(physical),0)==1&&
              GetPixel(physical,800,500)==RGB(0,128,255)&&
              g_seed_calls==cached_calls&&g_refresh_calls==refresh_calls+1,
              "early erase refreshes a valid cached scene without reseeding"))return 1;
    ++refresh_calls;
    PatBlt(physical,0,0,1920,1060,BLACKNESS);
    if(!check(shell::window_proc(arriving_handle,WM_SHOWWINDOW,TRUE,0)==kNativeShowResult&&
              GetPixel(physical,800,500)==RGB(0,0,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls,
              "hidden root forwards show without publishing cached seed"))return 1;
    arriving.visible=true;
    g_seed_color=RGB(255,0,0);
    if(!check(shell::window_proc(arriving_handle,WM_SHOWWINDOW,TRUE,0)==kNativeShowResult&&
              GetPixel(physical,800,500)==RGB(255,0,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls+1,
              "visible show refreshes latest seed after native clear and retains return value"))return 1;
    ++refresh_calls;
    WINDOWPOS shown{arriving_handle,nullptr,0,0,1920,1060,SWP_SHOWWINDOW|SWP_NOSIZE};
    g_seed_color=RGB(128,0,255);
    shell::window_proc(arriving_handle,WM_WINDOWPOSCHANGED,0,reinterpret_cast<LPARAM>(&shown));
    if(!check(GetPixel(physical,800,500)==RGB(128,0,255)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls+1,
              "visible position show refreshes latest seed after native clear"))return 1;
    ++refresh_calls;
    WINDOWPOS resized{arriving_handle,nullptr,0,0,1920,1060,SWP_NOMOVE};
    g_seed_color=RGB(0,255,0);
    shell::window_proc(arriving_handle,WM_WINDOWPOSCHANGED,0,reinterpret_cast<LPARAM>(&resized));
    if(!check(GetPixel(physical,800,500)==RGB(0,255,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls+1,
              "visible position size refreshes latest seed after native clear"))return 1;
    ++refresh_calls;
    g_refresh_behavior=1;g_seed_color=RGB(255,0,0);
    shell::window_proc(arriving_handle,WM_SHOWWINDOW,TRUE,0);
    if(!check(GetPixel(physical,800,500)==RGB(0,255,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls+1,
              "refused refresh preserves the previous valid cache"))return 1;
    ++refresh_calls;
    g_refresh_behavior=2;g_seed_color=RGB(255,255,0);
    shell::window_proc(arriving_handle,WM_WINDOWPOSCHANGED,0,reinterpret_cast<LPARAM>(&resized));
    if(!check(GetPixel(physical,800,500)==RGB(0,255,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls+1,
              "throwing refresh preserves the previous valid cache"))return 1;
    ++refresh_calls;
    g_refresh_behavior=0;
    if(!check(A2FO_SetShellCanvasRefreshV1(nullptr,reinterpret_cast<void*>(1))&&
              !shell::g_refresh&&!shell::g_refresh_context,
              "clearing optional refresh also clears its context"))return 1;
    shell::window_proc(arriving_handle,WM_SHOWWINDOW,TRUE,0);
    if(!check(GetPixel(physical,800,500)==RGB(0,255,0)&&
              g_seed_calls==cached_calls&&g_refresh_calls==refresh_calls,
              "cleared refresh replays the previous valid cache"))return 1;
    A2FO_SetShellCanvasRefreshV1(&refresh_canvas,nullptr);
    WINDOWPOS moved{arriving_handle,nullptr,0,0,1920,1060,SWP_NOSIZE};
    shell::window_proc(arriving_handle,WM_WINDOWPOSCHANGED,0,reinterpret_cast<LPARAM>(&moved));
    if(!check(GetPixel(physical,800,500)==RGB(0,0,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls,
              "move-only position leaves native pixels untouched"))return 1;
    arriving.visible=false;
    shell::window_proc(arriving_handle,WM_SHOWWINDOW,FALSE,0);
    if(!check(GetPixel(physical,800,500)==RGB(0,0,0)&&g_refresh_calls==refresh_calls,
              "hidden show leaves native clear untouched"))return 1;
    arriving.visible=true;
    A2FO_SetShellCanvasSeedV1(nullptr,nullptr);
    shell::window_proc(arriving_handle,WM_SHOWWINDOW,TRUE,0);
    if(!check(GetPixel(physical,800,500)==RGB(0,0,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls,
              "no active seed suppresses both replay and optional refresh"))return 1;
    A2FO_SetShellCanvasSeedV1(&seed_canvas,nullptr);
    g_seed_color=RGB(0,255,255);
    HDC first_paint=shell::acquire(arriving_handle,physical,&partial);
    if(!check(first_paint!=physical&&shell::finish(arriving_handle,first_paint,true,nullptr,nullptr)&&
              shell::lookup(arriving_handle)->native_presented,
              "successful normal root paint closes seed bridge"))return 1;
    shell::window_proc(arriving_handle,WM_SHOWWINDOW,TRUE,0);
    if(!check(GetPixel(physical,800,500)==RGB(0,0,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls,
              "completed root paint ends show bridge"))return 1;
    g_native_clear_window=button_handle;
    shell::window_proc(button_handle,WM_SHOWWINDOW,TRUE,0);
    if(!check(GetPixel(physical,800,500)==RGB(0,0,0)&&g_seed_calls==cached_calls&&
              g_refresh_calls==refresh_calls,
              "child show never publishes root seed"))return 1;
    g_native_clear_window=nullptr;g_native_clear_dc=nullptr;
    shell::window_proc(arriving_handle,WM_NCDESTROY,0,0);
    g_registration_window=nullptr;g_registration_dc=nullptr;
    A2FO_SetShellCanvasSeedV1(nullptr,nullptr);
    A2FO_SetShellCanvasRefreshV1(nullptr,nullptr);
    SelectObject(physical,physical_old);
    DeleteObject(physical_bitmap);
    DeleteDC(physical);
    const int logical_x = (field(options_control, 0x48) - 200) / 2;
    shell::position_window(button_handle, nullptr, logical_x, 300, 200, 40, 0);
    if (!check(logical_x == 500 && shell::lookup(button_handle)->left == 500 &&
               button.outer.left == 800 && button.client_width == 320 &&
               field(button_control, 0x48) == 200,
               "Options OnShow centers button once in logical coordinates")) return 1;
    shell::window_proc(button_handle, WM_LBUTTONDOWN, 0, MAKELPARAM(32, 12));
    if (!check(g_last_message == WM_LBUTTONDOWN &&
               static_cast<short>(LOWORD(g_mouse)) == 20 &&
               static_cast<short>(HIWORD(g_mouse)) == 10,
               "physical button click reaches logical VCL point")) return 1;

    owner.client_width = 1280; owner.client_height = 960;
    const Mode smaller{1280, 960, 32};
    shell::active_mode(&smaller);
    if (!check(menu.client_width == 1280 && menu.client_height == 960 &&
               options.client_width == 1280 && options.client_height == 960 &&
               field(options_control, 0x48) == 1200 && field(options_control, 0x4c) == 900,
               "smaller mode retains authored bounds")) return 1;
    if (!checkbox_release(menu_handle, 115, 140, true, "smaller mode glyph toggles on") ||
        !checkbox_release(menu_handle, 350, 140, false, "smaller mode label toggles off")) return 1;
    shell::window_proc(button_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(options_handle, WM_NCDESTROY, 0, 0);
    if (!check(!shell::lookup(button_handle) && !shell::lookup(options_handle) &&
               static_cast<bool>(shell::lookup(menu_handle)),
               "closing Options removes its UI but preserves main menu")) return 1;
    owner.client_width = 1920; owner.client_height = 1060;
    shell::active_mode(&full);
    shell::active_mode(&full);
    if (!check(menu.client_width == 1920 && menu.client_height == 1060 &&
               options.client_width == 1280 && options.client_height == 960 &&
               field(menu_control, 0x48) == 1200 && field(menu_control, 0x4c) == 900,
               "return to menu and large mode does not compound scale")) return 1;
    menu.outer = RECT{-120, 35, -120 + menu.client_width, 35 + menu.client_height};
    if (!checkbox_release(menu_handle, 115, 140, true, "negative screen origin glyph toggles") ||
        !checkbox_release(menu_handle, 350, 140, false, "negative screen origin label toggles")) return 1;

    // The native dialog bridge is verified against a pinned game image in
    // production. Supply its saved callback token here because this fake host
    // intentionally has no game image to verify or real dialog HWND to patch.
    FakeWindow dialog_owner{nullptr, static_cast<LONG_PTR>(WS_POPUP),
                            RECT{120,80,2040,1160}, 1920,1080, vcl_proc};
    FakeWindow dialog{nullptr, static_cast<LONG_PTR>(WS_POPUP),
                      RECT{120,80,1320,980}, 1200,900, vcl_proc};
    dialog.name = "#32770";
    const HWND dialog_owner_handle = add(dialog_owner), dialog_handle = add(dialog);
    std::array<std::uint8_t, 0x1b8> dialog_form{}, dialog_control{};
    set_handle(dialog_form, dialog_owner_handle);
    set_handle(dialog_control, dialog_handle);
    dialog.control = dialog_control.data();
    shell::a2fo_shell_register_handler(dialog_form.data(), dialog_handle, 120, 80, 1200, 900);
    auto dialog_record = shell::lookup(dialog_handle);
    if (!check(dialog_record && dialog.client_width == 1920 && dialog.client_height == 1080 &&
               dialog.outer.left == 120 && dialog.outer.top == 80,
               "offset dialog registration fits owner client")) return 1;
    dialog_record->dialog_next = vcl_proc;
    dialog.dlgproc=shell::game_dialog_proc;
    shell::move_window(dialog_handle, 120, 80, 1920, 1080, TRUE);
    if (!check(dialog_record->left == 120 && dialog_record->top == 80 &&
               dialog_record->width == 1200 && dialog_record->height == 900 &&
               dialog.client_width == 1920 && dialog.client_height == 1080 &&
               dialog.outer.left == 120 && dialog.outer.top == 80 &&
               field(dialog_control, 0x48) == 1200 && field(dialog_control, 0x4c) == 900,
               "native MoveWindow owner fit does not scale dialog twice")) return 1;
    int canvas_width{},canvas_height{};
    if(!check(dialog_record->surface.get(nullptr,1200,900)&&
              A2FO_CopyShellCanvasV1(dialog_handle,nullptr,0,0,&canvas_width,&canvas_height)&&
              canvas_width==1200&&canvas_height==900,
              "owner fit retains exported logical canvas dimensions"))return 1;
    shell::position_window(dialog_handle, nullptr, 120, 80, 1920, 1080, 0);
    if (!check(dialog_record->width == 1200 && dialog_record->height == 900 &&
               dialog.client_width == 1920 && dialog.client_height == 1080 &&
               field(dialog_control, 0x48) == 1200 && field(dialog_control, 0x4c) == 900,
               "native SetWindowPos owner fit does not scale dialog twice")) return 1;
    shell::position_window(dialog_handle, nullptr, 130, 90, 300, 200, SWP_NOSIZE);
    if (!check(dialog_record->left == 130 && dialog_record->top == 90 &&
               dialog_record->width == 1200 && dialog_record->height == 900 &&
               dialog.outer.left == 136 && dialog.outer.top == 92 &&
               dialog.client_width == 1920 && dialog.client_height == 1080,
               "SWP_NOSIZE remains a logical dialog move")) return 1;
    shell::position_window(dialog_handle,nullptr,999,777,1920,1080,SWP_NOMOVE);
    if(!check(dialog_record->left==130&&dialog_record->top==90&&
              dialog_record->width==1200&&dialog_record->height==900&&
              dialog.outer.left==136&&dialog.outer.top==92,
              "SWP_NOMOVE fit ignores unused coordinates and keeps authored position"))return 1;
    shell::move_window(dialog_handle,130,90,1920,1080,TRUE);
    if(!check(dialog_record->width==1920&&dialog_record->height==1080&&
              dialog.client_width==3072&&dialog.client_height==1296,
              "nonmatching origin retains existing authored resize contract"))return 1;
    shell::move_window(dialog_handle,120,80,1920,1080,TRUE);
    if(!check(dialog_record->width==1200&&dialog_record->height==900&&
              A2FO_CopyShellCanvasV1(dialog_handle,nullptr,0,0,&canvas_width,&canvas_height)&&
              canvas_width==1200&&canvas_height==900,
              "repeated exact owner fit restores registered canvas without compounding"))return 1;
    dialog.dlgproc=vcl_proc;
    shell::move_window(dialog_handle,120,80,1920,1080,TRUE);
    if(!check(dialog_record->width==1920&&dialog_record->height==1080,
              "changed dialog callback disables normalization"))return 1;
    dialog.dlgproc=shell::game_dialog_proc;
    shell::move_window(dialog_handle,120,80,1920,1080,TRUE);
    shell::move_window(dialog_handle, 120, 80, 1000, 700, TRUE);
    if (!check(dialog_record->width == 1000 && dialog_record->height == 700 &&
               dialog.client_width == 1600 && dialog.client_height == 840 &&
               field(dialog_control, 0x48) == 1000 && field(dialog_control, 0x4c) == 700,
               "authored dialog resize still scales")) return 1;

    FakeWindow dialog_child{dialog_handle, WS_CHILD, RECT{120,80,320,120}, 200,40, vcl_proc};
    const HWND dialog_child_handle = add(dialog_child);
    shell::client_rect(dialog_child_handle, &client);
    shell::move_window(dialog_child_handle, 0, 0, 1920, 1080, TRUE);
    if (!check(shell::lookup(dialog_child_handle) &&
               shell::lookup(dialog_child_handle)->width == 1920 &&
               shell::lookup(dialog_child_handle)->height == 1080 &&
               dialog_child.client_width == 3072 && dialog_child.client_height == 1296,
               "child control retains authored resize")) return 1;

    FakeWindow vcl_root{nullptr, static_cast<LONG_PTR>(WS_POPUP),
                        RECT{120,80,1320,980}, 1200,900, vcl_proc};
    const HWND vcl_root_handle = add(vcl_root);
    std::array<std::uint8_t, 0x1b8> vcl_control{};
    set_handle(vcl_control, vcl_root_handle);
    vcl_root.control = vcl_control.data();
    shell::a2fo_shell_register_handler(dialog_form.data(), vcl_root_handle, 120, 80, 1200, 900);
    shell::move_window(vcl_root_handle, 120, 80, 1920, 1080, TRUE);
    if (!check(shell::lookup(vcl_root_handle) && !shell::lookup(vcl_root_handle)->dialog_next &&
               shell::lookup(vcl_root_handle)->width == 1920 &&
               shell::lookup(vcl_root_handle)->height == 1080 &&
               vcl_root.client_width == 3072 && vcl_root.client_height == 1296,
               "ordinary VCL root retains authored resize")) return 1;
    shell::window_proc(dialog_child_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(dialog_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(vcl_root_handle, WM_NCDESTROY, 0, 0);

    FakeWindow pane_owner{nullptr, static_cast<LONG_PTR>(WS_POPUP),
                          RECT{-120,35,1800,1115}, 1920,1080, vcl_proc};
    FakeWindow outer_dialog{nullptr, static_cast<LONG_PTR>(WS_POPUP),
                            RECT{-120,35,1080,935}, 1200,900, vcl_proc};
    outer_dialog.name = "TMainWindowForm";
    outer_dialog.exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
    const HWND pane_owner_handle = add(pane_owner), outer_handle = add(outer_dialog);
    std::array<std::uint8_t, 0x1b8> pane_form{}, outer_control{};
    set_handle(pane_form, pane_owner_handle);
    set_handle(outer_control, outer_handle);
    outer_dialog.control = outer_control.data();
    shell::a2fo_shell_register_handler(pane_form.data(), outer_handle, -120, 35, 1200, 900);
    auto outer_record = shell::lookup(outer_handle);
    if (!check(outer_record && outer_dialog.client_width == 1920 &&
               outer_dialog.client_height == 1080 && outer_dialog.outer.left == -120 &&
               outer_dialog.outer.top == 35 &&
               outer_dialog.exstyle == (WS_EX_APPWINDOW | WS_EX_TOPMOST),
               "negative-origin primary dialog fills owner viewport without style changes")) return 1;

    FakeWindow pane{outer_handle, static_cast<LONG_PTR>(WS_POPUP | DS_CONTROL),
                    RECT{-120,35,806,779}, 926,744, vcl_proc};
    pane.name = "#32770";
    pane.exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
    pane.dlgproc = reinterpret_cast<WNDPROC>(reinterpret_cast<BYTE*>(shell::g_fleetops) +
                                              shell::kScreenDialogProc);
    const HWND pane_handle = add(pane);
    std::array<std::uint8_t, 0x1b8> pane_control{};
    set_handle(pane_control, pane_handle);
    pane.control = pane_control.data();
    const unsigned registrations_before = g_native_registrations;
    shell::a2fo_shell_register_handler(pane_form.data(), pane_handle, 0, 0, 926, 744);
    auto pane_record = shell::lookup(pane_handle);
    if (!check(pane_record && pane_record->root == outer_handle &&
               pane_record->parent == outer_handle && pane_record->owner == pane_owner_handle &&
               pane_record->root_scale.logical_width == 1200 &&
               pane_record->root_scale.logical_height == 900 &&
               pane_record->root_scale.physical_width == 1920 &&
               pane_record->root_scale.physical_height == 1080 &&
               pane_record->width == 926 && pane_record->height == 744 &&
               pane.client_width == 1482 && pane.client_height == 893 &&
               field(pane_control, 0x48) == 926 && field(pane_control, 0x4c) == 744 &&
               g_native_registrations == registrations_before + 1 &&
               pane.exstyle == (WS_EX_TOOLWINDOW | WS_EX_TOPMOST),
               "registered child pane inherits outer canvas and native registration runs once")) return 1;
    shell::position_window(pane_handle, nullptr, 170, 60, 630, 512, 0);
    if (!check(pane_record->left == 170 && pane_record->top == 60 &&
               pane_record->width == 630 && pane_record->height == 512 &&
               pane.outer.left == 344 && pane.outer.top == 65 &&
               pane.client_width == 1008 && pane.client_height == 614 &&
               pane.outer.left >= outer_dialog.outer.left &&
               pane.outer.top >= outer_dialog.outer.top &&
               pane.outer.right <= outer_dialog.outer.right &&
               pane.outer.bottom <= outer_dialog.outer.bottom &&
               field(pane_control, 0x48) == 630 && field(pane_control, 0x4c) == 512,
               "pane's authored position and size scale within outer viewport")) return 1;
    const unsigned repeat_before = g_native_registrations;
    shell::a2fo_shell_register_handler(pane_form.data(), pane_handle, 170, 60, 630, 512);
    if (!check(shell::lookup(pane_handle) == pane_record &&
               pane_record->root == outer_handle && pane_record->width == 630 &&
               pane_record->height == 512 && pane.outer.left == 344 && pane.outer.top == 65 &&
               pane.client_width == 1008 && pane.client_height == 614 &&
               field(pane_control, 0x48) == 630 && field(pane_control, 0x4c) == 512 &&
               g_native_registrations == repeat_before + 1,
               "repeat child registration forwards once without scaling twice")) return 1;

    FakeWindow pane_button{pane_handle, WS_CHILD, RECT{0,0,120,30}, 120,30, vcl_proc};
    const HWND pane_button_handle = add(pane_button);
    shell::client_rect(pane_button_handle, &client);
    shell::position_window(pane_button_handle, nullptr, 40, 50, 120, 30, 0);
    FakeWindow done_button{outer_handle, WS_CHILD, RECT{0,0,80,40}, 80,40, vcl_proc};
    const HWND done_handle = add(done_button);
    shell::client_rect(done_handle, &client);
    shell::position_window(done_handle, nullptr, 1100, 820, 80, 40, 0);
    shell::window_proc(done_handle, WM_LBUTTONDOWN, 0, MAKELPARAM(16, 12));
    if (!check(pane_button.outer.left >= 0 && pane_button.outer.top >= 0 &&
               pane_button.outer.right <= pane.client_width &&
               pane_button.outer.bottom <= pane.client_height &&
               done_button.outer.right <= outer_dialog.client_width &&
               done_button.outer.bottom <= outer_dialog.client_height &&
               g_last_message == WM_LBUTTONDOWN &&
               static_cast<short>(LOWORD(g_mouse)) == 10 &&
               static_cast<short>(HIWORD(g_mouse)) == 10,
               "pane controls and outer Done hit bounds remain in their viewports")) return 1;
    FakeWindow unrelated_popup{outer_handle, static_cast<LONG_PTR>(WS_POPUP | DS_CONTROL),
                               RECT{-120,35,680,635}, 800,600, vcl_proc};
    unrelated_popup.name = "#32770";
    unrelated_popup.dlgproc = vcl_proc;
    unrelated_popup.exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
    const HWND unrelated_handle = add(unrelated_popup);
    shell::a2fo_shell_register_handler(pane_form.data(), unrelated_handle, 0, 0, 800, 600);
    if (!check(shell::lookup(unrelated_handle) &&
               shell::lookup(unrelated_handle)->root == unrelated_handle &&
               unrelated_popup.client_width == 1920 && unrelated_popup.client_height == 1080 &&
               unrelated_popup.exstyle == (WS_EX_APPWINDOW | WS_EX_TOPMOST),
               "unrelated owned dialog callback retains independent root")) return 1;
    FakeWindow registered_child{outer_handle, WS_CHILD, RECT{0,0,800,600},
                                800,600, vcl_proc};
    registered_child.name = "TOptionsMenuForm";
    registered_child.exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
    const HWND registered_child_handle = add(registered_child);
    shell::a2fo_shell_register_handler(pane_form.data(), registered_child_handle,
                                       0, 0, 800, 600);
    if (!check(shell::lookup(registered_child_handle) &&
               shell::lookup(registered_child_handle)->root == outer_handle &&
               registered_child.exstyle == (WS_EX_APPWINDOW | WS_EX_TOPMOST),
               "registered WS_CHILD retains its extended style")) return 1;
    shell::window_proc(registered_child_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(unrelated_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(pane_button_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(done_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(pane_handle, WM_NCDESTROY, 0, 0);
    shell::window_proc(outer_handle, WM_NCDESTROY, 0, 0);

    FakeWindow game{nullptr, static_cast<LONG_PTR>(WS_POPUP), RECT{0,0,900,700}, 900,700, vcl_proc};
    HWND game_handle = add(game);
    std::array<std::uint8_t, 0x1b8> game_control{};
    set_handle(game_control, game_handle);
    game.control = game_control.data();
    std::memset(game_control.data() + 0x40, 0x7a, 16);
    shell::a2fo_shell_bounds_handler(game_control.data());
    shell::position_window(game_handle, nullptr, 15, 25, 900, 700, 0);
    if (!check(g_native_bounds == 3 && field(game_control, 0x48) == 0x7a7a7a7a &&
               game.outer.left == 15 && game.outer.top == 25 &&
               game.client_width == 900 && game.client_height == 700 &&
               !shell::lookup(game_handle), "untracked game control forwards unchanged")) return 1;
    g_cursor = POINT{-42, 71};
    POINT query{};
    if (!check(shell::checkbox_cursor_pos(&query) &&
               query.x == g_cursor.x && query.y == g_cursor.y,
               "cursor query outside dispatch remains physical")) return 1;
    shell::Dispatch untracked{game_handle, WM_LBUTTONUP, 0, nullptr};
    shell::g_dispatch = &untracked;
    query = {};
    if (!check(shell::checkbox_cursor_pos(&query) &&
               query.x == g_cursor.x && query.y == g_cursor.y,
               "untracked dispatch preserves physical cursor")) return 1;
    shell::Dispatch tracked{menu_handle, WM_LBUTTONUP, 0, nullptr};
    shell::g_dispatch = &tracked;
    shell::g_ready.store(false);
    if(!check(!A2FO_SetShellCanvasCompletionV1(&canvas_completed,nullptr)&&!shell::g_completion,
              "inactive canvas refuses completion registration"))return 1;
    query = {};
    if (!check(shell::checkbox_cursor_pos(&query) &&
               query.x == g_cursor.x && query.y == g_cursor.y,
               "inactive canvas preserves physical cursor")) return 1;
    shell::g_ready.store(true);
    g_cursor_ok = false;
    query = POINT{321, 654};
    if (!check(!shell::checkbox_cursor_pos(&query) && query.x == 321 && query.y == 654,
               "native cursor failure preserves result and output")) return 1;
    g_cursor_ok = true;
    shell::g_dispatch = nullptr;
    shell::shutdown();
    VirtualFree(image, 0, MEM_RELEASE);
    std::puts("Shell canvas lifecycle: PASS (synthetic USER32 host, no HWNDs)");
    return 0;
}
