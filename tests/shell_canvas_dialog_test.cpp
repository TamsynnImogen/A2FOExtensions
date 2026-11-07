// Synthetic USER32 host for nested native-dialog input dispatch. HWNDs are
// opaque tokens; no desktop window or input device is touched.
#include "../modules/A2FOShellDisplay/canvas.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <map>
#include <string>

BOOL WINAPI host_IsWindow(HWND);
BOOL WINAPI host_GetClientRect(HWND, LPRECT);
BOOL WINAPI host_GetWindowRect(HWND, LPRECT);
HWND WINAPI host_GetParent(HWND);
LONG_PTR WINAPI host_GetWindowLongPtrA(HWND, int);
LONG_PTR WINAPI host_SetWindowLongPtrA(HWND, int, LONG_PTR);
int WINAPI host_GetClassNameA(HWND, LPSTR, int);
BOOL WINAPI host_GetModuleHandleExA(DWORD, LPCSTR, HMODULE*);
HMODULE WINAPI host_GetModuleHandleA(LPCSTR);
LRESULT WINAPI host_CallWindowProcA(WNDPROC, HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI host_DefWindowProcA(HWND, UINT, WPARAM, LPARAM);

#undef GetWindowLongPtrA
#undef SetWindowLongPtrA
#define IsWindow host_IsWindow
#define GetClientRect host_GetClientRect
#define GetWindowRect host_GetWindowRect
#define GetParent host_GetParent
#define GetWindowLongPtrA host_GetWindowLongPtrA
#define SetWindowLongPtrA host_SetWindowLongPtrA
#define GetClassNameA host_GetClassNameA
#define GetModuleHandleExA host_GetModuleHandleExA
#define GetModuleHandleA host_GetModuleHandleA
#define CallWindowProcA host_CallWindowProcA
#define DefWindowProcA host_DefWindowProcA
#include "../modules/A2FOShellDisplay/canvas.cpp"
#undef IsWindow
#undef GetClientRect
#undef GetWindowRect
#undef GetParent
#undef GetWindowLongPtrA
#undef SetWindowLongPtrA
#undef GetClassNameA
#undef GetModuleHandleExA
#undef GetModuleHandleA
#undef CallWindowProcA
#undef DefWindowProcA

namespace canvas = a2fo::shell_display::canvas;
namespace {
struct FakeWindow {
    WNDPROC wndproc{};
    WNDPROC dlgproc{};
    RECT client{0, 0, 2560, 1440};
    std::string class_name{"#32770"};
    bool fail_dlg_install{};
};
std::map<HWND, FakeWindow*> g_windows;
HMODULE g_armada_module = reinterpret_cast<HMODULE>(0x11110000);
HMODULE g_fleetops_module = reinterpret_cast<HMODULE>(0x22220000);
HMODULE g_user32_module = reinterpret_cast<HMODULE>(0x33330000);
LPARAM g_outer_parameter{};
LPARAM g_main_outer_parameter{}, g_reentrant_outer_parameter{};
LPARAM g_game_parameter{};
LPARAM g_nested_default_parameter{};
LPARAM g_main_game_parameter{}, g_reentrant_game_parameter{};
int g_game_calls{}, g_outer_calls{}, g_default_calls{};
bool g_nested_default{}, g_reentrant_dispatch{};
LRESULT g_game_result{17};
LRESULT g_last_callback_result{};

FakeWindow* find(HWND window) {
    const auto it = g_windows.find(window);
    return it == g_windows.end() ? nullptr : it->second;
}

LRESULT CALLBACK outer_dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    ++g_outer_calls;
    g_outer_parameter = lparam;
    if (message == WM_LBUTTONDOWN) g_main_outer_parameter = lparam;
    if (message == WM_MOUSEMOVE) g_reentrant_outer_parameter = lparam;
    const auto* fake = find(window);
    const LRESULT handled = fake && fake->dlgproc
        ? fake->dlgproc(window, message, wparam, lparam) : FALSE;
    g_last_callback_result = handled;
    if (handled == FALSE) {
        host_DefWindowProcA(window, message, wparam, lparam);
        return 73;
    }
    return handled;
}

LRESULT CALLBACK original_game_dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    ++g_game_calls;
    g_game_parameter = lparam;
    if (message == WM_LBUTTONDOWN) g_main_game_parameter = lparam;
    if (message == WM_MOUSEMOVE) g_reentrant_game_parameter = lparam;
    if (message == WM_LBUTTONDOWN && g_reentrant_dispatch) {
        g_reentrant_dispatch = false;
        canvas::window_proc(window, WM_MOUSEMOVE, wparam, MAKELPARAM(1282, 722));
        g_reentrant_dispatch = true;
    }
    if (g_nested_default) {
        canvas::default_proc(window, message, 0, lparam);
    }
    return g_game_result;
}

LRESULT CALLBACK unknown_dialog_proc(HWND, UINT, WPARAM, LPARAM lparam) {
    g_game_parameter = lparam;
    return 29;
}

LRESULT CALLBACK builtin_edit_dialog_proc(HWND, UINT, WPARAM, LPARAM lparam) {
    ++g_game_calls;
    g_game_parameter = lparam;
    return 31;
}
}

BOOL WINAPI host_IsWindow(HWND window) { return find(window) != nullptr; }
BOOL WINAPI host_GetClientRect(HWND window, LPRECT rect) {
    const auto* fake = find(window);
    if (!fake || !rect) return FALSE;
    *rect = fake->client;
    return TRUE;
}
BOOL WINAPI host_GetWindowRect(HWND window, LPRECT rect) {
    const auto* fake = find(window);
    if (!fake || !rect) return FALSE;
    *rect = fake->client;
    return TRUE;
}
HWND WINAPI host_GetParent(HWND) { return nullptr; }
LONG_PTR WINAPI host_GetWindowLongPtrA(HWND window, int field) {
    const auto* fake = find(window);
    if (!fake) return 0;
    if (field == GWLP_WNDPROC) return reinterpret_cast<LONG_PTR>(fake->wndproc);
    if (field == DWLP_DLGPROC) return reinterpret_cast<LONG_PTR>(fake->dlgproc);
    return 0;
}
LONG_PTR WINAPI host_SetWindowLongPtrA(HWND window, int field, LONG_PTR value) {
    auto* fake = find(window);
    if (!fake) { SetLastError(ERROR_INVALID_WINDOW_HANDLE); return 0; }
    if (field == DWLP_DLGPROC && fake->fail_dlg_install) {
        SetLastError(ERROR_ACCESS_DENIED);
        return 0;
    }
    LONG_PTR previous{};
    if (field == GWLP_WNDPROC) {
        previous = reinterpret_cast<LONG_PTR>(fake->wndproc);
        fake->wndproc = reinterpret_cast<WNDPROC>(value);
    } else if (field == DWLP_DLGPROC) {
        previous = reinterpret_cast<LONG_PTR>(fake->dlgproc);
        fake->dlgproc = reinterpret_cast<WNDPROC>(value);
    } else {
        SetLastError(ERROR_INVALID_INDEX);
        return 0;
    }
    return previous;
}
int WINAPI host_GetClassNameA(HWND window, LPSTR name, int count) {
    const auto* fake = find(window);
    if (!fake || !name || count <= 0) return 0;
    const int copied = std::min<int>(static_cast<int>(fake->class_name.size()), count - 1);
    std::memcpy(name, fake->class_name.data(), static_cast<std::size_t>(copied));
    name[copied] = '\0';
    return copied;
}
BOOL WINAPI host_GetModuleHandleExA(DWORD, LPCSTR address, HMODULE* module) {
    if (!module) return FALSE;
    const auto procedure = reinterpret_cast<WNDPROC>(const_cast<char*>(address));
    if (procedure == original_game_dialog_proc || procedure == unknown_dialog_proc) {
        *module = procedure == original_game_dialog_proc ? g_armada_module :
            reinterpret_cast<HMODULE>(0x44440000);
        return TRUE;
    }
    if (procedure == outer_dialog_proc || procedure == builtin_edit_dialog_proc) {
        *module = g_user32_module;
        return TRUE;
    }
    return FALSE;
}
HMODULE WINAPI host_GetModuleHandleA(LPCSTR name) {
    return name && lstrcmpiA(name, "user32.dll") == 0 ? g_user32_module : nullptr;
}
LRESULT WINAPI host_CallWindowProcA(WNDPROC procedure, HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam) {
    return procedure ? procedure(window, message, wparam, lparam) : 0;
}
LRESULT WINAPI host_DefWindowProcA(HWND, UINT, WPARAM, LPARAM lparam) {
    ++g_default_calls;
    g_nested_default_parameter = lparam;
    return 0;
}

namespace {
HWND token(FakeWindow& fake) { return reinterpret_cast<HWND>(&fake); }
std::shared_ptr<canvas::Window> adopt_dialog(FakeWindow& fake, int width = 1200, int height = 900) {
    const HWND hwnd = token(fake);
    g_windows.emplace(hwnd, &fake);
    auto root = std::make_shared<canvas::Window>();
    root->handle = hwnd; root->root = hwnd; root->owner = hwnd;
    root->width = width; root->height = height;
    root->root_scale = {width, height, fake.client.right, fake.client.bottom};
    return canvas::adopt(hwnd, root, true, 0, 0, width, height);
}
void setup() {
    canvas::g_ready.store(true, std::memory_order_release);
    canvas::g_mode = {2560, 1440, 32};
    canvas::g_armada = g_armada_module;
    canvas::g_fleetops = g_fleetops_module;
}
}

int main() {
    setup();
    FakeWindow supported;
    supported.wndproc = outer_dialog_proc;
    supported.dlgproc = original_game_dialog_proc;
    const HWND hwnd = token(supported);
    auto adopted = adopt_dialog(supported);
    assert(adopted && adopted->dialog_next == original_game_dialog_proc);
    assert(supported.wndproc != outer_dialog_proc && supported.dlgproc != original_game_dialog_proc);
    assert(A2FO_ShellDialogBridgeReadyV1(hwnd));
    const LPARAM physical = MAKELPARAM(1280, 720);
    g_nested_default = true;
    g_reentrant_dispatch = true;
    canvas::Dispatch sentinel{reinterpret_cast<HWND>(0x55550000), WM_NULL, 0, nullptr};
    auto* previous_dispatch = canvas::g_dispatch;
    canvas::g_dispatch = &sentinel;
    const LRESULT result = canvas::window_proc(hwnd, WM_LBUTTONDOWN, 0, physical);
    assert(canvas::g_dispatch == &sentinel);
    canvas::g_dispatch = previous_dispatch;
    g_reentrant_dispatch = false;
    g_nested_default = false;
    assert(result == g_game_result && g_outer_calls == 2 && g_game_calls == 2);
    assert(g_main_outer_parameter == physical && g_reentrant_outer_parameter == MAKELPARAM(1282, 722) &&
           g_main_game_parameter == MAKELPARAM(600, 450) &&
           g_reentrant_game_parameter == MAKELPARAM(601, 451));
    assert(g_default_calls == 2);
    assert(g_nested_default_parameter == physical);

    // A logical callback FALSE delegates to USER32's default path using the
    // original physical message, while a TRUE return remains untouched.
    g_game_result = FALSE;
    assert(canvas::window_proc(hwnd, WM_LBUTTONUP, 0, physical) == 73);
    assert(g_last_callback_result == FALSE && g_outer_parameter == physical &&
           g_game_parameter == MAKELPARAM(600, 450) && g_nested_default_parameter == physical);

    // Disabled scoping reaches the callback with the unmodified physical
    // point and still lets the outer dialog perform its default handling.
    canvas::g_ready.store(false, std::memory_order_release);
    g_game_calls = g_outer_calls = 0;
    assert(canvas::window_proc(hwnd, WM_LBUTTONUP, 0, physical) == 73);
    assert(g_last_callback_result == FALSE && g_outer_parameter == physical &&
           g_game_parameter == physical && g_nested_default_parameter == physical &&
           g_outer_calls == 1 && g_game_calls == 1);
    canvas::g_ready.store(true, std::memory_order_release);
    g_game_result = 17;

    // Built-in Edit dialog callbacks remain untouched and receive physical
    // input, even when their parent HWND is in the active shell scope.
    FakeWindow edit;
    edit.class_name = "Edit";
    edit.wndproc = builtin_edit_dialog_proc;
    auto edit_record = adopt_dialog(edit);
    assert(edit_record && edit_record->dialog_next == nullptr && !edit.dlgproc);
    g_game_calls = 0;
    assert(canvas::window_proc(token(edit), WM_LBUTTONDOWN, 0, physical) == 31 &&
           g_game_parameter == physical && g_game_calls == 1);

    // Unknown callback ownership skips only the dialog bridge.
    FakeWindow foreign;
    foreign.dlgproc = unknown_dialog_proc;
    foreign.wndproc = outer_dialog_proc;
    auto foreign_record = adopt_dialog(foreign);
    assert(foreign_record && foreign_record->dialog_next == nullptr &&
           foreign.dlgproc == unknown_dialog_proc);
    assert(!A2FO_ShellDialogBridgeReadyV1(token(foreign)));
    assert(canvas::window_proc(token(foreign), WM_LBUTTONDOWN, 0, physical) == 29 &&
           g_game_parameter == physical);

    // A failed DLGPROC slot write leaves the verified original callback in
    // place while retaining the normal outer WNDPROC adoption.
    FakeWindow failed;
    failed.wndproc = outer_dialog_proc;
    failed.dlgproc = original_game_dialog_proc;
    failed.fail_dlg_install = true;
    auto failed_record = adopt_dialog(failed);
    assert(failed_record && failed_record->dialog_next == nullptr &&
           failed.dlgproc == original_game_dialog_proc);
    assert(!A2FO_ShellDialogBridgeReadyV1(token(failed)));
    assert(canvas::window_proc(token(failed), WM_LBUTTONDOWN, 0, physical) == 17 &&
           g_game_parameter == physical);

    // NCDESTROY restores the owned callback but does not overwrite a callback
    // another owner installed after adoption.
    const HWND destroyed = token(supported);
    assert(canvas::window_proc(destroyed, WM_NCDESTROY, 0, 0) == g_game_result);
    assert(supported.dlgproc == original_game_dialog_proc && !canvas::lookup(destroyed));
    FakeWindow borrowed;
    borrowed.wndproc = outer_dialog_proc;
    borrowed.dlgproc = original_game_dialog_proc;
    auto borrowed_record = adopt_dialog(borrowed);
    assert(borrowed_record && borrowed_record->dialog_next == original_game_dialog_proc);
    const HWND borrowed_hwnd = token(borrowed);
    assert(A2FO_ShellDialogBridgeReadyV1(borrowed_hwnd));
    borrowed.dlgproc = unknown_dialog_proc;
    assert(!A2FO_ShellDialogBridgeReadyV1(borrowed_hwnd));
    canvas::window_proc(borrowed_hwnd, WM_NCDESTROY, 0, 0);
    assert(borrowed.dlgproc == unknown_dialog_proc && !canvas::lookup(borrowed_hwnd));

    canvas::shutdown();
    std::puts("shell native dialog nested input dispatch passed");
}
