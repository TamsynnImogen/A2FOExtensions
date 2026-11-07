/*
 * File: core/module_menu.cpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Fleet Operations Mods-screen module-selection integration.
 */

#include "module_menu.hpp"

#include "extension_roots.hpp"
#include "../rust/native_hooks_ffi.h"
#include "module_policy.hpp"
#include "builtin_modules.hpp"
#include "../modules/A2FOLogMailer/config.hpp"
#include "../rust/stage8_b_core_ffi.h"
#include "../rust/stage9_module_menu_ffi.h"
#include "../rust/stage10_core_menu_ffi.h"

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <exception>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace a2fo {
namespace {

constexpr std::uintptr_t kModSettingsFormShowRva = 0x1bf89c;
constexpr std::uintptr_t kModSettingsLaunchRva = 0x1bef2c;
constexpr std::uintptr_t kNextGridGetRowRva = 0x19f110;
constexpr std::uintptr_t kJetButtonCreateRva = 0x0dcb20;
constexpr std::uintptr_t kJetButtonClickRva = 0x0dd260;
constexpr std::uintptr_t kJetButtonSetBoundsRva = 0x0dd6ec;
constexpr std::uintptr_t kJvgCheckBoxCreateRva = 0x165e34;
constexpr std::uintptr_t kJvgCheckBoxSetCheckedRva = 0x166d0c;
constexpr std::uintptr_t kAssignJvgCheckBoxImagesRva = 0x1786d0;
constexpr std::uintptr_t kAssignButtonImagesLargeCenterRva = 0x001784c0;
constexpr std::uintptr_t kControlClickRva = 0x0c724c;
constexpr std::uintptr_t kControlSetParentRva = 0x0c55f0;
constexpr std::uintptr_t kControlSetVisibleRva = 0x0c563c;
constexpr std::uintptr_t kControlSetTextRva = 0x0c574c;
constexpr std::uintptr_t kControlBringToFrontRva = 0x0c59ac;
constexpr std::uintptr_t kControlRepaintRva = 0x0c5d30;
constexpr std::uintptr_t kComponentInsertRva = 0x00088274;
constexpr std::uintptr_t kLongStringClearRva = 0x000056b8;
constexpr std::uintptr_t kLongStringFromPcharRva = 0x000058b0;

constexpr std::size_t kFormHandleOffset = 0x1c4;
constexpr std::size_t kComponentOwnerOffset = 0x04;
constexpr std::size_t kControlParentOffset = 0x30;
constexpr std::size_t kGridOffset = 0x370;
constexpr std::size_t kLaunchButtonOffset = 0x39c;
constexpr std::size_t kVisitWebsiteButtonOffset = 0x3a0;
constexpr std::size_t kControlLeftOffset = 0x40;
constexpr std::size_t kControlTopOffset = 0x44;
constexpr std::size_t kControlWidthOffset = 0x48;
constexpr std::size_t kControlHeightOffset = 0x4c;
constexpr std::size_t kModificationFolderOffset = 0x0c;
constexpr std::size_t kModificationTitleOffset = 0x10;
constexpr std::size_t kJvgCheckBoxCheckedOffset = 0x1c8;
constexpr std::size_t kJvgCheckBoxOptionsOffset = 0x1f0;

constexpr int kModuleListId = 0x6301;
constexpr int kSaveButtonId = 0x6302;
constexpr int kCancelButtonId = 0x6303;
constexpr int kCrashConsentId = 0x6304;
constexpr int kReportModeId = 0x6305;

const std::uint8_t kExpectedFormShow[] = {
    0x55, 0x8b, 0xec, 0x33, 0xc9};
const std::uint8_t kExpectedLaunch[] = {
    0x53, 0x56, 0x8b, 0xd8, 0x8b, 0xb3, 0x70, 0x03, 0x00, 0x00};
const std::uint8_t kExpectedJetButtonCreate[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56};
const std::uint8_t kExpectedJetButtonClick[] = {
    0xe8, 0xe7, 0x9f, 0xfe, 0xff, 0xc3};
const std::uint8_t kExpectedJetButtonSetBounds[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8};
const std::uint8_t kExpectedJvgCheckBoxCreate[] = {
    0x53, 0x56, 0x84, 0xd2, 0x74, 0x08};
const std::uint8_t kExpectedJvgCheckBoxSetChecked[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf0};
const std::uint8_t kExpectedAssignJvgCheckBoxImages[] = {
    0xb9, 0xe8, 0x86, 0x97, 0x5a,
    0xba, 0x10, 0x87, 0x97, 0x5a};
const std::uint8_t kExpectedAssignButtonImagesLargeCenter[] = {
    0x55, 0x8b, 0xec, 0x33, 0xc9, 0x51, 0x51, 0x51};
const std::uint8_t kExpectedControlSetParent[] = {
    0x53, 0x56, 0x8b, 0xf2, 0x8b, 0xd8};
const std::uint8_t kExpectedControlSetVisible[] = {
    0x53, 0x56, 0x57, 0x8b, 0xda, 0x8b, 0xf8};
const std::uint8_t kExpectedControlSetText[] = {
    0x55, 0x8b, 0xec, 0x6a, 0x00};
const std::uint8_t kExpectedControlClick[] = {
    0x53, 0x8b, 0xd8, 0x66, 0x83, 0xbb};
const std::uint8_t kExpectedControlBringToFront[] = {
    0x56, 0x8b, 0xf0, 0xb2, 0x01};
const std::uint8_t kExpectedControlRepaint[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8};
const std::uint8_t kExpectedComponentInsert[] = {
    0x53, 0x56, 0x57, 0x8b, 0xda, 0x8b, 0xf8};
const std::uint8_t kExpectedLongStringClear[] = {
    0x8b, 0x10, 0x85, 0xd2, 0x74, 0x1c};
const std::uint8_t kExpectedLongStringFromPchar[] = {
    0x31, 0xc9, 0x85, 0xd2, 0x74, 0x21};

constexpr char kDialogClassName[] = "A2FO.ModuleSelector";

HMODULE g_fleet_ops = nullptr;
std::string g_data_root;
void (*g_log_line)(const std::string&) = nullptr;
A2foRsInlineHook g_form_show_hook{};
A2foRsInlineHook g_launch_hook{};
void* g_modules_button = nullptr;
void* g_modules_button_form = nullptr;
HWND g_modules_button_owner = nullptr;
void* g_shortcut_button = nullptr;
void* g_nointro_checkbox = nullptr;
void* g_noai_checkbox = nullptr;
void* g_flag_checkbox_form = nullptr;
void* g_jvg_checkbox_class = nullptr;
// Fleet Operations repaints over ordinary child HWND controls on this screen.
// When a native TJvgCheckBox VMT cannot be recovered, use genuine windowless
// TJetButton controls as checkbox-like toggles so they participate in FO's VCL
// paint path just like the injected Modules button.
void* g_nointro_toggle_button = nullptr;
void* g_noai_toggle_button = nullptr;
void* g_flag_toggle_form = nullptr;

struct FlagCheckboxState {
    const char* text = nullptr;
    bool checked = false;
};

FlagCheckboxState g_nointro_fallback_state{"nointro", false};
FlagCheckboxState g_noai_fallback_state{"noai", false};
HWND g_nointro_fallback_window = nullptr;
HWND g_noai_fallback_window = nullptr;
HWND g_flag_fallback_parent = nullptr;
bool g_flag_fallback_class_registered = false;
HINSTANCE g_flag_fallback_instance = nullptr;
constexpr char kFlagCheckboxWindowClass[] = "A2FO.ModFlagCheckbox";

volatile LONG g_pending_nointro = 0;
volatile LONG g_pending_noai = 0;

void clear_pending_launch_flags();

using CreateProcessAFunction = BOOL (WINAPI*)(
    LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
    LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
using CreateProcessWFunction = BOOL (WINAPI*)(
    LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
    LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
using ShellExecuteAFunction = HINSTANCE (WINAPI*)(
    HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
using ShellExecuteWFunction = HINSTANCE (WINAPI*)(
    HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
using ShellExecuteExAFunction = BOOL (WINAPI*)(SHELLEXECUTEINFOA*);
using ShellExecuteExWFunction = BOOL (WINAPI*)(SHELLEXECUTEINFOW*);
using WinExecFunction = UINT (WINAPI*)(LPCSTR, UINT);

CreateProcessAFunction g_original_create_process_a = nullptr;
CreateProcessWFunction g_original_create_process_w = nullptr;
ShellExecuteAFunction g_original_shell_execute_a = nullptr;
ShellExecuteWFunction g_original_shell_execute_w = nullptr;
ShellExecuteExAFunction g_original_shell_execute_ex_a = nullptr;
ShellExecuteExWFunction g_original_shell_execute_ex_w = nullptr;
WinExecFunction g_original_win_exec = nullptr;

extern "C" void* a2fo_call_delphi_one_register(
    void* function, void* eax_argument);
extern "C" void* a2fo_call_delphi_two_registers(
    void* function, void* eax_argument, void* edx_argument);
extern "C" void* a2fo_call_delphi_constructor(
    void* function, void* class_reference, void* owner);
extern "C" void a2fo_call_delphi_set_bounds(
    void* function, void* control, int left, int top, int width, int height);
extern "C" void a2fo_mod_settings_form_show_bridge();
extern "C" void a2fo_mod_settings_launch_bridge();
extern "C" void a2fo_module_menu_jet_button_click_bridge();

void log(const std::string& line) {
    if (g_log_line) g_log_line(line);
}

void* at(std::uintptr_t rva) {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_fleet_ops) + rva);
}

std::string join_path(const std::string& left, const std::string& right) {
    switch (a2fo_rs_stage10_core_menu_join_path(
        left.size(), right.size(),
        left.empty() ? 0 : static_cast<std::uint8_t>(left.back()))) {
    case 0: return right;
    case 1: return left;
    case 2: return left + right;
    default: return left + "\\" + right;
    }
}

template <typename T>
T field(void* object, std::size_t offset) {
    if (!object) return T{};
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(object) + offset);
}

bool readable_range(const void* pointer, std::size_t length) {
    if (!pointer || length == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    const auto region_end =
        reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
        information.RegionSize;
    return start <= region_end && length <= region_end - start;
}

std::string delphi_string_field(void* object, std::size_t offset) {
    char* value = field<char*>(object, offset);
    if (!value || !readable_range(value - sizeof(std::int32_t),
                                  sizeof(std::int32_t))) {
        return {};
    }
    const std::int32_t length =
        *reinterpret_cast<const std::int32_t*>(value - sizeof(std::int32_t));
    if (length <= 0 || length > 4096 ||
        !readable_range(value, static_cast<std::size_t>(length))) {
        return {};
    }
    return std::string(value, static_cast<std::size_t>(length));
}

bool selected_modification(void* form, void*& modification) {
    struct Context {void* form;void*& modification;void* grid{};void** vtable{};
        std::intptr_t index{};void* row{};std::exception_ptr exception{};
    } c{form,modification};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 600:c.modification=nullptr;return 0;
            case 601:c.grid=field<void*>(c.form,kGridOffset);return 0;
            case 602:return c.grid!=nullptr;
            case 603:return readable_range(c.grid,sizeof(void*));
            case 604:c.vtable=*reinterpret_cast<void***>(c.grid);return 0;
            case 605:return c.vtable!=nullptr;
            case 606:return readable_range(c.vtable+0x148/sizeof(void*),sizeof(void*));
            case 607:c.index=reinterpret_cast<std::intptr_t>(a2fo_call_delphi_one_register(
                c.vtable[0x148/sizeof(void*)],c.grid));return static_cast<std::uint32_t>(c.index);
            case 608:c.row=a2fo_call_delphi_two_registers(at(kNextGridGetRowRva),c.grid,
                reinterpret_cast<void*>(c.index));return 0;
            case 609:return c.row!=nullptr;
            case 610:return readable_range(static_cast<std::uint8_t*>(c.row)+8,sizeof(void*));
            case 611:c.modification=field<void*>(c.row,8);return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_module_menu_selected_mod(&c,op)!=0;
    if(c.exception)std::rethrow_exception(c.exception);
    return result;
}

struct SelectedMod {
    void* modification = nullptr;
    bool base = false;
    std::string folder;
    std::string title;
    std::string root;
    std::string info_path;
    std::vector<std::string> roots;
    std::vector<std::string> diagnostics;
};

bool resolve_selected_mod(void* form, SelectedMod& selected,
                          std::string& error) {
    struct Context {void* form;SelectedMod& selected;std::string& error;
        std::exception_ptr exception{};
    } c{form,selected,error};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 620:return selected_modification(c.form,c.selected.modification);
            case 621:c.error="No mod is selected.";return 0;
            case 622:c.selected.base=c.selected.modification==nullptr;return c.selected.base;
            case 623:c.selected.title="Fleet Operations";c.selected.root=g_data_root;
                c.selected.roots.push_back(g_data_root);return 0;
            case 624:c.selected.folder=delphi_string_field(c.selected.modification,
                kModificationFolderOffset);return 0;
            case 625:c.selected.title=delphi_string_field(c.selected.modification,
                kModificationTitleOffset);return 0;
            case 626:return c.selected.title.empty();
            case 627:c.selected.title=c.selected.folder;return 0;
            case 628:return safe_mod_directory_name(c.selected.folder);
            case 629:c.error="The selected mod has an unsafe or unreadable folder name.";return 0;
            case 630:{const ExtensionRootDiscovery discovery=discover_extension_roots(
                g_data_root,"/mod \""+c.selected.folder+"\"");
                c.selected.roots=discovery.roots;
                c.selected.diagnostics=discovery.diagnostics;return 0;}
            case 631:c.selected.root=join_path(join_path(g_data_root,"Mods"),
                c.selected.folder);return 0;
            case 632:return c.selected.roots.empty();
            case 633:return _stricmp(c.selected.roots.back().c_str(),
                c.selected.root.c_str())==0;
            case 634:c.error="The selected mod directory could not be resolved.";return 0;
            case 635:c.selected.info_path=join_path(c.selected.root,"info.ini");return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_module_menu_resolve_mod(&c,op)!=0;
    if(c.exception)std::rethrow_exception(c.exception);
    return result;
}

bool checkbox_checked(void* checkbox) {
    return readable_range(checkbox, kJvgCheckBoxCheckedOffset + 1) &&
           field<std::uint8_t>(checkbox, kJvgCheckBoxCheckedOffset) != 0;
}

LRESULT CALLBACK flag_checkbox_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<FlagCheckboxState*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTA*>(lparam);
        state = create
            ? reinterpret_cast<FlagCheckboxState*>(create->lpCreateParams)
            : nullptr;
        SetWindowLongPtrA(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(state));
    }
    switch (message) {
    case WM_LBUTTONUP: {
        if (!state) break;
        RECT client{};
        GetClientRect(window, &client);
        POINT point{static_cast<short>(LOWORD(lparam)),
                    static_cast<short>(HIWORD(lparam))};
        if (PtInRect(&client, point)) {
            state->checked = !state->checked;
            InvalidateRect(window, nullptr, FALSE);
        }
        ReleaseCapture();
        return 0;
    }
    case WM_LBUTTONDOWN:
        SetCapture(window);
        return 0;
    case WM_KEYUP:
        if (state && wparam == VK_SPACE) {
            state->checked = !state->checked;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_GETDLGCODE:
        return DLGC_WANTCHARS | DLGC_WANTARROWS;
    case WM_ERASEBKGND:
        // WS_EX_TRANSPARENT asks Windows/Wine to paint the parent first. Do
        // not cover Fleet Operations' skinned background with a system brush.
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (dc) {
            RECT client{};
            GetClientRect(window, &client);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(235, 245, 255));
            HGDIOBJ previous_font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));

            const int box_size = 12;
            const int box_left = 1;
            const int box_top = std::max(
                1, static_cast<int>((client.bottom - box_size) / 2));
            HPEN outline = CreatePen(PS_SOLID, 1, RGB(75, 160, 245));
            HGDIOBJ previous_pen = SelectObject(dc, outline);
            HGDIOBJ previous_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, box_left, box_top,
                      box_left + box_size, box_top + box_size);
            if (state && state->checked) {
                MoveToEx(dc, box_left + 3, box_top + 6, nullptr);
                LineTo(dc, box_left + 5, box_top + 9);
                LineTo(dc, box_left + 10, box_top + 3);
            }
            SelectObject(dc, previous_brush);
            SelectObject(dc, previous_pen);
            DeleteObject(outline);

            RECT text_rect{box_left + box_size + 5, 0,
                           client.right, client.bottom};
            DrawTextA(dc, state && state->text ? state->text : "",
                      -1, &text_rect,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, previous_font);
        }
        EndPaint(window, &paint);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

bool ensure_flag_checkbox_window_class() {
    if (g_flag_fallback_class_registered) return true;
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&flag_checkbox_window_proc), &module)) {
        module = GetModuleHandleA("A2FOExtensions.dll");
    }
    WNDCLASSEXA window_class{};
    g_flag_fallback_instance = module;
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &flag_checkbox_window_proc;
    window_class.hInstance = g_flag_fallback_instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_HAND);
    window_class.lpszClassName = kFlagCheckboxWindowClass;
    if (!RegisterClassExA(&window_class)) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            log("Module menu: fallback checkbox window class registration "
                "failed (error " + std::to_string(error) + ")");
            return false;
        }
    }
    g_flag_fallback_class_registered = true;
    return true;
}

void destroy_fallback_flag_windows() {
    if (g_nointro_fallback_window && IsWindow(g_nointro_fallback_window)) {
        DestroyWindow(g_nointro_fallback_window);
    }
    if (g_noai_fallback_window && IsWindow(g_noai_fallback_window)) {
        DestroyWindow(g_noai_fallback_window);
    }
    g_nointro_fallback_window = nullptr;
    g_noai_fallback_window = nullptr;
    g_flag_fallback_parent = nullptr;
}

bool ensure_fallback_flag_checkboxes(void* form, int row_left, int row_top,
                                     int check_height) {
    const HWND parent = field<HWND>(form, kFormHandleOffset);
    if (!parent || !IsWindow(parent)) {
        log("Module menu: Mods form HWND unavailable for fallback checkboxes");
        return false;
    }
    constexpr int nointro_width = 78;
    constexpr int noai_width = 58;
    constexpr int gap = 6;
    if (g_flag_fallback_parent != parent ||
        !g_nointro_fallback_window || !IsWindow(g_nointro_fallback_window) ||
        !g_noai_fallback_window || !IsWindow(g_noai_fallback_window)) {
        // Preserve the state if Fleet Ops recreates the Mods form.
        if (g_nointro_fallback_window && IsWindow(g_nointro_fallback_window)) {
            g_nointro_fallback_state.checked =
                SendMessageA(g_nointro_fallback_window, BM_GETCHECK, 0, 0) ==
                BST_CHECKED;
        }
        if (g_noai_fallback_window && IsWindow(g_noai_fallback_window)) {
            g_noai_fallback_state.checked =
                SendMessageA(g_noai_fallback_window, BM_GETCHECK, 0, 0) ==
                BST_CHECKED;
        }
        destroy_fallback_flag_windows();

        // Do not depend on Delphi/JVCL RTTI here. A stock Win32 checkbox is a
        // real child HWND, which is reliable under both Windows and Wine.
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                            BS_AUTOCHECKBOX | BS_FLAT;
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(
            GetModuleHandleA("A2FOExtensions.dll"));
        g_nointro_fallback_window = CreateWindowExA(
            0, "BUTTON", "nointro", style,
            row_left, row_top, nointro_width, check_height,
            parent, nullptr, instance, nullptr);
        g_noai_fallback_window = CreateWindowExA(
            0, "BUTTON", "noai", style,
            row_left + nointro_width + gap, row_top, noai_width, check_height,
            parent, nullptr, instance, nullptr);
        if (!g_nointro_fallback_window || !g_noai_fallback_window) {
            const DWORD error = GetLastError();
            destroy_fallback_flag_windows();
            log("Module menu: Win32 checkbox fallback creation failed (error " +
                std::to_string(error) + ")");
            return false;
        }
        SendMessageA(g_nointro_fallback_window, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
        SendMessageA(g_noai_fallback_window, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
        SendMessageA(g_nointro_fallback_window, BM_SETCHECK,
                     g_nointro_fallback_state.checked ? BST_CHECKED : BST_UNCHECKED,
                     0);
        SendMessageA(g_noai_fallback_window, BM_SETCHECK,
                     g_noai_fallback_state.checked ? BST_CHECKED : BST_UNCHECKED,
                     0);
        g_flag_fallback_parent = parent;
    }
    SetWindowPos(g_nointro_fallback_window, HWND_TOP,
                 row_left, row_top, nointro_width, check_height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(g_noai_fallback_window, HWND_TOP,
                 row_left + nointro_width + gap, row_top,
                 noai_width, check_height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RedrawWindow(g_nointro_fallback_window, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
    RedrawWindow(g_noai_fallback_window, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
    log("Module menu: Win32 nointro/noai checkbox fallback v3 active at " +
        std::to_string(row_left) + "," + std::to_string(row_top));
    return true;
}

bool nointro_selected() {
    if (g_nointro_checkbox &&
        readable_range(g_nointro_checkbox, kJvgCheckBoxCheckedOffset + 1)) {
        return checkbox_checked(g_nointro_checkbox);
    }
    if (g_nointro_toggle_button && g_flag_toggle_form) {
        return g_nointro_fallback_state.checked;
    }
    if (g_nointro_fallback_window && IsWindow(g_nointro_fallback_window)) {
        return SendMessageA(g_nointro_fallback_window, BM_GETCHECK, 0, 0) ==
               BST_CHECKED;
    }
    return g_nointro_fallback_state.checked;
}

bool noai_selected() {
    if (g_noai_checkbox &&
        readable_range(g_noai_checkbox, kJvgCheckBoxCheckedOffset + 1)) {
        return checkbox_checked(g_noai_checkbox);
    }
    if (g_noai_toggle_button && g_flag_toggle_form) {
        return g_noai_fallback_state.checked;
    }
    if (g_noai_fallback_window && IsWindow(g_noai_fallback_window)) {
        return SendMessageA(g_noai_fallback_window, BM_GETCHECK, 0, 0) ==
               BST_CHECKED;
    }
    return g_noai_fallback_state.checked;
}

std::string ascii_lower(std::string text) {
    struct Context {
        std::string* text;
        std::uint8_t failed = 0;
        std::exception_ptr exception{};
    } c{&text};
    const auto step = [](void* opaque, std::uint32_t id,
                         std::uint64_t index) noexcept -> std::uint64_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            if (id == 0) {
                const auto character = static_cast<unsigned char>((*c.text)[index]);
                (*c.text)[index] = static_cast<char>(std::tolower(character));
            }
        } catch (...) { c.exception = std::current_exception(); c.failed = 1; }
        return 0;
    };
    a2fo_rs_stage10_core_menu_ascii_lower(text.size(), &c, step, &c.failed);
    if (c.exception) std::rethrow_exception(c.exception);
    return text;
}

bool has_command_switch(const std::string& command, const char* name) {
    const std::string lower = ascii_lower(command);
    const std::string slash = std::string("/") + name;
    const std::string dash = std::string("-") + name;
    const auto is_space=[](std::uint8_t value) noexcept -> std::uint8_t {
        return std::isspace(value)!=0;
    };
    const auto bounded=[&](const std::string& token) {
        return a2fo_rs_stage8_b_module_menu_has_switch(
            reinterpret_cast<const std::uint8_t*>(lower.data()),lower.size(),
            reinterpret_cast<const std::uint8_t*>(token.data()),token.size(),
            is_space)!=0;
    };
    return bounded(slash)||bounded(dash);
}

std::string append_selected_switches(std::string command, bool nointro,
                                     bool noai) {
    struct Context {std::string command;bool nointro,noai;std::exception_ptr exception{};
    } c{std::move(command),nointro,noai};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 500:return c.nointro;
            case 501:return has_command_switch(c.command,"nointro");
            case 502:
                if(!c.command.empty() && !std::isspace(
                    static_cast<unsigned char>(c.command.back())))c.command.push_back(' ');
                c.command+="/nointro";return 0;
            case 503:return c.noai;
            case 504:return has_command_switch(c.command,"noai");
            case 505:
                if(!c.command.empty() && !std::isspace(
                    static_cast<unsigned char>(c.command.back())))c.command.push_back(' ');
                c.command+="/noai";return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_append_switches(&c,op);
    if(c.exception)std::rethrow_exception(c.exception);
    return std::move(c.command);
}

std::wstring widen_ascii(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

std::string pending_switch_suffix() {
    struct Context {
        std::string suffix;
        std::uint8_t failed = 0;
        std::exception_ptr exception{};
    } c;
    const auto step = [](void* opaque, std::uint32_t id,
                         std::uint64_t) noexcept -> std::uint64_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (id) {
            case 0: return InterlockedCompareExchange(&g_pending_nointro, 0, 0) != 0;
            case 1: c.suffix += " /nointro"; return 0;
            case 2: return InterlockedCompareExchange(&g_pending_noai, 0, 0) != 0;
            case 3: c.suffix += " /noai"; return 0;
            default: return 0;
            }
        } catch (...) { c.exception = std::current_exception(); c.failed = 1; }
        return 0;
    };
    a2fo_rs_stage10_core_menu_flow(0, &c, step, &c.failed);
    if (c.exception) std::rethrow_exception(c.exception);
    return std::move(c.suffix);
}

void clear_pending_launch_flags() {
    InterlockedExchange(&g_pending_nointro, 0);
    InterlockedExchange(&g_pending_noai, 0);
}

std::string append_pending_switches(std::string command) {
    const bool nointro = InterlockedCompareExchange(
        &g_pending_nointro, 0, 0) != 0;
    const bool noai = InterlockedCompareExchange(&g_pending_noai, 0, 0) != 0;
    return append_selected_switches(std::move(command), nointro, noai);
}

std::wstring append_pending_switches(std::wstring command) {
    struct Context {
        std::wstring command;
        std::string suffix;
        std::wstring wide;
        std::wstring result;
        std::uint8_t failed = 0;
        std::exception_ptr exception{};
    } c{std::move(command), pending_switch_suffix(), {}, {}, 0, {}};
    const auto step = [](void* opaque, std::uint32_t id,
                         std::uint64_t) noexcept -> std::uint64_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (id) {
            case 0: return c.suffix.empty();
            case 1: c.wide = widen_ascii(c.suffix); return 0;
            case 2: return c.command.empty();
            case 3: return c.command.back() == L' ';
            case 4: return c.wide.empty();
            case 5: return c.wide.front() == L' ';
            case 6: c.command.pop_back(); return 0;
            case 7: c.result = c.command + c.wide; return 0;
            default: return 0;
            }
        } catch (...) { c.exception = std::current_exception(); c.failed = 1; }
        return 0;
    };
    const bool has_suffix = a2fo_rs_stage10_core_menu_flow(1, &c, step, &c.failed) != 0;
    if (c.exception) std::rethrow_exception(c.exception);
    return has_suffix ? std::move(c.result) : std::move(c.command);
}

extern "C" BOOL WINAPI a2fo_mod_launch_create_process_a(
    LPCSTR application, LPSTR command_line,
    LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles,
    DWORD creation_flags, LPVOID environment, LPCSTR current_directory,
    LPSTARTUPINFOA startup_info, LPPROCESS_INFORMATION process_info) {
    struct Context {LPCSTR application;LPSTR command_line;
        LPSECURITY_ATTRIBUTES process_attributes,thread_attributes;
        BOOL inherit_handles;DWORD creation_flags;LPVOID environment;
        LPCSTR current_directory;LPSTARTUPINFOA startup_info;
        LPPROCESS_INFORMATION process_info;std::string command{},original{};
        std::vector<char> buffer{};BOOL result{};std::exception_ptr exception{};
    } c{application,command_line,process_attributes,thread_attributes,
        inherit_handles,creation_flags,environment,current_directory,
        startup_info,process_info};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 901:return pending_switch_suffix().empty();
        case 902:c.result=g_original_create_process_a(c.application,c.command_line,
            c.process_attributes,c.thread_attributes,c.inherit_handles,
            c.creation_flags,c.environment,c.current_directory,c.startup_info,
            c.process_info);return 0;
        case 910:c.command=c.command_line?c.command_line:"";return 0;
        case 911:return c.command.empty();
        case 912:return c.application!=nullptr;
        case 913:c.command="\""+std::string(c.application)+"\"";return 0;
        case 914:c.original=c.command;return 0;
        case 915:c.command=append_pending_switches(std::move(c.command));return 0;
        case 916:log("Module menu: CreateProcessA original command: "+c.original);return 0;
        case 917:log("Module menu: CreateProcessA final command: "+c.command);return 0;
        case 918:c.buffer.assign(c.command.begin(),c.command.end());
            c.buffer.push_back('\0');return 0;
        case 919:c.result=g_original_create_process_a(c.application,c.buffer.data(),
            c.process_attributes,c.thread_attributes,c.inherit_handles,
            c.creation_flags,c.environment,c.current_directory,c.startup_info,
            c.process_info);return 0;
        case 920:return c.result!=FALSE;
        case 921:log("Module menu: launch flags consumed by CreateProcessA");return 0;
        case 922:clear_pending_launch_flags();return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_launch_intercept(&c,op,0);
    if(c.exception)std::rethrow_exception(c.exception);
    return c.result;
}

extern "C" BOOL WINAPI a2fo_mod_launch_create_process_w(
    LPCWSTR application, LPWSTR command_line,
    LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles,
    DWORD creation_flags, LPVOID environment, LPCWSTR current_directory,
    LPSTARTUPINFOW startup_info, LPPROCESS_INFORMATION process_info) {
    struct Context {LPCWSTR application;LPWSTR command_line;
        LPSECURITY_ATTRIBUTES process_attributes,thread_attributes;
        BOOL inherit_handles;DWORD creation_flags;LPVOID environment;
        LPCWSTR current_directory;LPSTARTUPINFOW startup_info;
        LPPROCESS_INFORMATION process_info;std::wstring command{},original{};
        std::vector<wchar_t> buffer{};BOOL result{};std::exception_ptr exception{};
    } c{application,command_line,process_attributes,thread_attributes,
        inherit_handles,creation_flags,environment,current_directory,
        startup_info,process_info};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 901:return pending_switch_suffix().empty();
        case 902:c.result=g_original_create_process_w(c.application,c.command_line,
            c.process_attributes,c.thread_attributes,c.inherit_handles,
            c.creation_flags,c.environment,c.current_directory,c.startup_info,
            c.process_info);return 0;
        case 910:c.command=c.command_line?c.command_line:L"";return 0;
        case 911:return c.command.empty();
        case 912:return c.application!=nullptr;
        case 913:c.command=L"\""+std::wstring(c.application)+L"\"";return 0;
        case 914:c.original=c.command;return 0;
        case 915:c.command=append_pending_switches(std::move(c.command));return 0;
        case 916:log("Module menu: CreateProcessW intercepted pending mod launch");return 0;
        case 918:c.buffer.assign(c.command.begin(),c.command.end());
            c.buffer.push_back(L'\0');return 0;
        case 919:c.result=g_original_create_process_w(c.application,c.buffer.data(),
            c.process_attributes,c.thread_attributes,c.inherit_handles,
            c.creation_flags,c.environment,c.current_directory,c.startup_info,
            c.process_info);return 0;
        case 920:return c.result!=FALSE;
        case 921:log("Module menu: launch flags consumed by CreateProcessW");return 0;
        case 922:clear_pending_launch_flags();return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_launch_intercept(&c,op,1);
    if(c.exception)std::rethrow_exception(c.exception);
    return c.result;
}

extern "C" HINSTANCE WINAPI a2fo_mod_launch_shell_execute_a(
    HWND window, LPCSTR operation, LPCSTR file, LPCSTR parameters,
    LPCSTR directory, INT show_command) {
    struct Context {HWND window;LPCSTR operation,file,parameters,directory;
        INT show_command;std::string original{},updated{};HINSTANCE result{};
        std::exception_ptr exception{};
    } c{window,operation,file,parameters,directory,show_command};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 901:return pending_switch_suffix().empty();
        case 902:c.result=g_original_shell_execute_a(c.window,c.operation,c.file,
            c.parameters,c.directory,c.show_command);return 0;
        case 930:c.original=c.parameters?c.parameters:"";return 0;
        case 931:c.updated=append_pending_switches(c.original);return 0;
        case 932:log("Module menu: ShellExecuteA file: "+std::string(c.file?c.file:"")+
            " original args: "+c.original+" final args: "+c.updated);return 0;
        case 933:c.result=g_original_shell_execute_a(c.window,c.operation,c.file,
            c.updated.c_str(),c.directory,c.show_command);return 0;
        case 934:return reinterpret_cast<INT_PTR>(c.result)>32;
        case 935:log("Module menu: launch flags consumed by ShellExecuteA");return 0;
        case 922:clear_pending_launch_flags();return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_launch_intercept(&c,op,2);
    if(c.exception)std::rethrow_exception(c.exception);
    return c.result;
}

extern "C" HINSTANCE WINAPI a2fo_mod_launch_shell_execute_w(
    HWND window, LPCWSTR operation, LPCWSTR file, LPCWSTR parameters,
    LPCWSTR directory, INT show_command) {
    struct Context {HWND window;LPCWSTR operation,file,parameters,directory;
        INT show_command;std::wstring updated{};HINSTANCE result{};
        std::exception_ptr exception{};
    } c{window,operation,file,parameters,directory,show_command};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 901:return pending_switch_suffix().empty();
        case 902:c.result=g_original_shell_execute_w(c.window,c.operation,c.file,
            c.parameters,c.directory,c.show_command);return 0;
        case 930:c.updated=append_pending_switches(c.parameters?
            std::wstring(c.parameters):std::wstring());return 0;
        case 932:log("Module menu: ShellExecuteW intercepted pending mod launch");return 0;
        case 933:c.result=g_original_shell_execute_w(c.window,c.operation,c.file,
            c.updated.c_str(),c.directory,c.show_command);return 0;
        case 934:return reinterpret_cast<INT_PTR>(c.result)>32;
        case 935:log("Module menu: launch flags consumed by ShellExecuteW");return 0;
        case 922:clear_pending_launch_flags();return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_launch_intercept(&c,op,3);
    if(c.exception)std::rethrow_exception(c.exception);
    return c.result;
}

extern "C" BOOL WINAPI a2fo_mod_launch_shell_execute_ex_a(
    SHELLEXECUTEINFOA* execute_info) {
    struct Context {SHELLEXECUTEINFOA* info;SHELLEXECUTEINFOA copy{};
        std::string original{},updated{};BOOL result{};
        std::exception_ptr exception{};
    } c{execute_info};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 940:return c.info!=nullptr;
        case 901:return pending_switch_suffix().empty();
        case 902:c.result=g_original_shell_execute_ex_a(c.info);return 0;
        case 941:c.copy=*c.info;return 0;
        case 942:c.original=c.copy.lpParameters?c.copy.lpParameters:"";return 0;
        case 943:c.updated=append_pending_switches(c.original);return 0;
        case 944:c.copy.lpParameters=c.updated.c_str();return 0;
        case 945:log("Module menu: ShellExecuteExA file: "+
            std::string(c.copy.lpFile?c.copy.lpFile:"")+
            " original args: "+c.original+" final args: "+c.updated);return 0;
        case 946:c.result=g_original_shell_execute_ex_a(&c.copy);return 0;
        case 947:c.info->hInstApp=c.copy.hInstApp;c.info->hProcess=c.copy.hProcess;return 0;
        case 948:return c.result!=FALSE;
        case 949:log("Module menu: launch flags consumed by ShellExecuteExA");return 0;
        case 922:clear_pending_launch_flags();return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_launch_intercept(&c,op,4);
    if(c.exception)std::rethrow_exception(c.exception);
    return c.result;
}

extern "C" BOOL WINAPI a2fo_mod_launch_shell_execute_ex_w(
    SHELLEXECUTEINFOW* execute_info) {
    struct Context {SHELLEXECUTEINFOW* info;SHELLEXECUTEINFOW copy{};
        std::wstring updated{};BOOL result{};std::exception_ptr exception{};
    } c{execute_info};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 940:return c.info!=nullptr;
        case 901:return pending_switch_suffix().empty();
        case 902:c.result=g_original_shell_execute_ex_w(c.info);return 0;
        case 941:c.copy=*c.info;return 0;
        case 942:c.updated=append_pending_switches(c.copy.lpParameters?
            std::wstring(c.copy.lpParameters):std::wstring());return 0;
        case 944:c.copy.lpParameters=c.updated.c_str();return 0;
        case 945:log("Module menu: ShellExecuteExW intercepted pending mod launch");return 0;
        case 946:c.result=g_original_shell_execute_ex_w(&c.copy);return 0;
        case 947:c.info->hInstApp=c.copy.hInstApp;c.info->hProcess=c.copy.hProcess;return 0;
        case 948:return c.result!=FALSE;
        case 949:log("Module menu: launch flags consumed by ShellExecuteExW");return 0;
        case 922:clear_pending_launch_flags();return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_launch_intercept(&c,op,5);
    if(c.exception)std::rethrow_exception(c.exception);
    return c.result;
}

extern "C" UINT WINAPI a2fo_mod_launch_win_exec(LPCSTR command_line,
                                                   UINT show_command) {
    struct Context {LPCSTR command_line;UINT show_command;
        std::string original{},updated{};UINT result{};
        std::exception_ptr exception{};
    } c{command_line,show_command};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 901:return pending_switch_suffix().empty();
        case 902:c.result=g_original_win_exec(c.command_line,c.show_command);return 0;
        case 950:c.original=c.command_line?c.command_line:"";return 0;
        case 951:c.updated=append_pending_switches(c.original);return 0;
        case 952:log("Module menu: WinExec original command: "+c.original);return 0;
        case 953:log("Module menu: WinExec final command: "+c.updated);return 0;
        case 954:c.result=g_original_win_exec(c.updated.c_str(),c.show_command);return 0;
        case 955:return c.result>31;
        case 956:log("Module menu: launch flags consumed by WinExec");return 0;
        case 922:clear_pending_launch_flags();return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_launch_intercept(&c,op,6);
    if(c.exception)std::rethrow_exception(c.exception);
    return c.result;
}

template <typename Function>
bool patch_import_by_name(HMODULE module, const char* function_name,
                          Function replacement, Function& original) {
    static_assert(std::is_pointer<Function>::value,
                  "import replacement must be a function pointer");
    if (!module || !function_name || !replacement) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (!readable_range(dos, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<std::uint8_t*>(module) + dos->e_lfanew);
    if (!readable_range(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    const IMAGE_DATA_DIRECTORY directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0 || directory.Size == 0) return false;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        reinterpret_cast<std::uint8_t*>(module) + directory.VirtualAddress);
    for (; descriptor->Name != 0; ++descriptor) {
        if (descriptor->OriginalFirstThunk == 0 ||
            descriptor->FirstThunk == 0) {
            continue;
        }
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<std::uint8_t*>(module) +
            descriptor->OriginalFirstThunk);
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<std::uint8_t*>(module) + descriptor->FirstThunk);
        for (; names->u1.AddressOfData != 0; ++names, ++addresses) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto* import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                reinterpret_cast<std::uint8_t*>(module) +
                names->u1.AddressOfData);
            if (!readable_range(import, sizeof(WORD) + 1) ||
                std::strcmp(reinterpret_cast<const char*>(import->Name),
                            function_name) != 0) {
                continue;
            }
            DWORD old_protection = 0;
            if (!VirtualProtect(&addresses->u1.Function,
                                sizeof(addresses->u1.Function),
                                PAGE_READWRITE, &old_protection)) {
                return false;
            }
            original = reinterpret_cast<Function>(addresses->u1.Function);
            addresses->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            DWORD ignored = 0;
            VirtualProtect(&addresses->u1.Function,
                           sizeof(addresses->u1.Function),
                           old_protection, &ignored);
            FlushInstructionCache(GetCurrentProcess(),
                                  &addresses->u1.Function,
                                  sizeof(addresses->u1.Function));
            return true;
        }
    }
    return false;
}

int install_mod_launch_import_hooks_for(HMODULE module,
                                        const char* module_name) {
    struct Context {HMODULE module;const char* name;std::exception_ptr exception{};
    } c{module,module_name};
    const auto op=[](void* opaque,std::uint32_t id,std::uint64_t index) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            static const char* names[]{"CreateProcessA","CreateProcessW",
                "ShellExecuteA","ShellExecuteW","ShellExecuteExA",
                "ShellExecuteExW","WinExec"};
            if(id==800)return c.module!=nullptr;
            if(id==802){log(std::string("Module menu: hooked ")+names[index]+
                " in "+c.name);return 0;}
            if(id!=801)return 0;
            switch(index) {
            case 0:return patch_import_by_name(c.module,names[index],
                &a2fo_mod_launch_create_process_a,g_original_create_process_a);
            case 1:return patch_import_by_name(c.module,names[index],
                &a2fo_mod_launch_create_process_w,g_original_create_process_w);
            case 2:return patch_import_by_name(c.module,names[index],
                &a2fo_mod_launch_shell_execute_a,g_original_shell_execute_a);
            case 3:return patch_import_by_name(c.module,names[index],
                &a2fo_mod_launch_shell_execute_w,g_original_shell_execute_w);
            case 4:return patch_import_by_name(c.module,names[index],
                &a2fo_mod_launch_shell_execute_ex_a,g_original_shell_execute_ex_a);
            case 5:return patch_import_by_name(c.module,names[index],
                &a2fo_mod_launch_shell_execute_ex_w,g_original_shell_execute_ex_w);
            case 6:return patch_import_by_name(c.module,names[index],
                &a2fo_mod_launch_win_exec,g_original_win_exec);
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const int hooked=static_cast<int>(a2fo_rs_stage8_b_module_menu_import_for(&c,op));
    if(c.exception)std::rethrow_exception(c.exception);
    return hooked;
}

bool install_mod_launch_import_hooks() {
    struct Context {int fleet{},armada_hooks{};HMODULE armada{};
        std::exception_ptr exception{};} c;
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 810:c.fleet=install_mod_launch_import_hooks_for(g_fleet_ops,
                "FleetOpsHook.dll");return c.fleet;
            case 811:c.armada=GetModuleHandleA(nullptr);return 0;
            case 812:return c.armada!=nullptr;
            case 813:return c.armada!=g_fleet_ops;
            case 814:c.armada_hooks=install_mod_launch_import_hooks_for(c.armada,
                "ArmadaL.exe");return c.armada_hooks;
            case 815:log("Module menu: hooked "+
                std::to_string(c.fleet+c.armada_hooks)+
                " process-launch import(s) for mod command flags (FleetOps="+
                std::to_string(c.fleet)+", ArmadaL="+
                std::to_string(c.armada_hooks)+")");return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_module_menu_import_all(&c,op)!=0;
    if(c.exception)std::rethrow_exception(c.exception);
    return result;
}

const CLSID kShellLinkClsid = {
    0x00021401, 0x0000, 0x0000,
    {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID kShellLinkAIid = {
    0x000214ee, 0x0000, 0x0000,
    {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID kPersistFileIid = {
    0x0000010b, 0x0000, 0x0000,
    {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

using CoInitializeExFunction = HRESULT (WINAPI*)(LPVOID, DWORD);
using CoCreateInstanceFunction = HRESULT (WINAPI*)(
    REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
using CoUninitializeFunction = void (WINAPI*)();
using SHGetFolderPathAFunction = HRESULT (WINAPI*)(
    HWND, int, HANDLE, DWORD, LPSTR);

// Synchronous native storage/COM bridge. Never unwind through Rust, and keep
// full-width FILETIME values distinct from the exception failure byte.
template<class Context, class Step>
std::uint64_t module_menu_policy(std::uint32_t kind, Context& context, Step step) {
    struct Call { Context& context; Step& step; std::exception_ptr error; std::uint8_t failed = 0; };
    Call call{context, step, {}, 0};
    const auto bridge = +[](void* opaque, std::uint32_t op, std::uint64_t arg) -> std::uint64_t {
        auto& c = *static_cast<Call*>(opaque);
        try { return c.step(c.context, op, arg); }
        catch (...) { c.error = std::current_exception(); c.failed = 1; return 0; }
    };
    const auto result = a2fo_rs_stage9_module_menu_flow(kind, bridge, &call, &call.failed);
    if (call.error) std::rethrow_exception(call.error);
    return result;
}

std::wstring path_to_wide(const std::string& path) {
    // Keep a real native local/NRVO return, including short-string storage.
    std::wstring wide;
    struct Context { const std::string& path; std::wstring& wide; int needed = 0; } c{path, wide};
    module_menu_policy(3, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: x.needed = MultiByteToWideChar(CP_ACP, 0, x.path.c_str(), -1, nullptr, 0); return static_cast<std::uint64_t>(static_cast<std::int64_t>(x.needed));
        case 1: x.wide.assign(static_cast<std::size_t>(x.needed), L'\0'); return 0;
        case 2: MultiByteToWideChar(CP_ACP, 0, x.path.c_str(), -1, &x.wide[0], x.needed); return 0;
        case 3: x.wide.resize(static_cast<std::size_t>(x.needed - 1)); return 0;
        default: return 0;
        }
    });
    return wide;
}

std::uint64_t file_time_value(const FILETIME& time) {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

bool update_shortcut_arguments(const std::string& path,
                               const std::string& mod_folder,
                               bool nointro, bool noai,
                               CoCreateInstanceFunction create_instance) {
    struct Context {
        IShellLinkA* link = nullptr; IPersistFile* persist = nullptr;
        const std::wstring* wide = nullptr; std::array<char, 32768> arguments{};
        std::optional<std::string>* updated = nullptr;
    } c;
    const auto step = [&](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: return SUCCEEDED(create_instance(kShellLinkClsid, nullptr, CLSCTX_INPROC_SERVER, kShellLinkAIid, reinterpret_cast<void**>(&x.link)));
        case 1: return x.link != nullptr;
        case 2: return SUCCEEDED(x.link->QueryInterface(kPersistFileIid, reinterpret_cast<void**>(&x.persist)));
        case 3: return x.persist != nullptr;
        case 4: x.link->Release(); return 0;
        case 6: return x.wide->empty();
        case 7: return SUCCEEDED(x.persist->Load(x.wide->c_str(), STGM_READWRITE));
        case 8: x.persist->Release(); return 0;
        case 9: return SUCCEEDED(x.link->GetArguments(x.arguments.data(), static_cast<int>(x.arguments.size())));
        case 10: return _stricmp(active_mod_from_command_line(x.arguments.data()).c_str(), mod_folder.c_str()) == 0;
        case 11: x.updated->emplace(append_selected_switches(x.arguments.data(), nointro, noai)); return 0;
        case 12: return **x.updated != x.arguments.data();
        case 13: return SUCCEEDED(x.link->SetArguments((*x.updated)->c_str()));
        case 14: return SUCCEEDED(x.persist->Save(x.wide->c_str(), TRUE));
        case 15: return nointro;
        case 16: return noai;
        default: return 0;
        }
    };
    if (module_menu_policy(0, c, step) == 0) return false;
    const std::wstring wide_path = path_to_wide(path);
    c.wide = &wide_path;
    std::optional<std::string> updated; // destroyed before wide_path, as originally
    c.updated = &updated;
    return module_menu_policy(4, c, step) != 0;
}

bool update_recent_desktop_shortcut(const SelectedMod& selected,
                                    bool nointro, bool noai,
                                    const FILETIME& started) {
    struct Context {
        HMODULE shell = nullptr, ole = nullptr;
        SHGetFolderPathAFunction folder = nullptr;
        CoInitializeExFunction initialize = nullptr;
        CoCreateInstanceFunction create = nullptr;
        CoUninitializeFunction uninitialize = nullptr;
        std::array<char, MAX_PATH> desktop{}; HRESULT initialized = E_FAIL;
        std::optional<std::string> pattern, path;
        WIN32_FIND_DATAA data{}; HANDLE search = INVALID_HANDLE_VALUE;
    } c;
    return module_menu_policy(1, c, [&](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: return selected.base;
        case 1: return selected.folder.empty();
        case 2: return nointro;
        case 3: return noai;
        case 4: x.shell = LoadLibraryA("shell32.dll"); return x.shell != nullptr;
        case 5: x.ole = LoadLibraryA("ole32.dll"); return x.ole != nullptr;
        case 8: FreeLibrary(x.shell); return 0;
        case 9: FreeLibrary(x.ole); return 0;
        case 10: x.folder = reinterpret_cast<SHGetFolderPathAFunction>(GetProcAddress(x.shell, "SHGetFolderPathA")); return 0;
        case 11: x.initialize = reinterpret_cast<CoInitializeExFunction>(GetProcAddress(x.ole, "CoInitializeEx")); return 0;
        case 12: x.create = reinterpret_cast<CoCreateInstanceFunction>(GetProcAddress(x.ole, "CoCreateInstance")); return 0;
        case 13: x.uninitialize = reinterpret_cast<CoUninitializeFunction>(GetProcAddress(x.ole, "CoUninitialize")); return 0;
        case 14: return x.folder != nullptr;
        case 15: return x.initialize != nullptr;
        case 16: return x.create != nullptr;
        case 17: return x.uninitialize != nullptr;
        case 18: return SUCCEEDED(x.folder(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, x.desktop.data()));
        case 19: x.initialized = x.initialize(nullptr, COINIT_APARTMENTTHREADED); return SUCCEEDED(x.initialized);
        case 20: return x.initialized == RPC_E_CHANGED_MODE;
        case 21: x.pattern.emplace(join_path(x.desktop.data(), "*.lnk")); return 0;
        case 22: x.search = FindFirstFileA(x.pattern->c_str(), &x.data); return x.search != INVALID_HANDLE_VALUE;
        case 23: return file_time_value(started);
        case 24: return (x.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        case 25: return file_time_value(x.data.ftLastWriteTime);
        case 26: x.path.emplace(join_path(x.desktop.data(), x.data.cFileName)); return 0;
        case 27: return update_shortcut_arguments(*x.path, selected.folder, nointro, noai, x.create);
        case 28: x.path.reset(); return 0;
        case 29: return FindNextFileA(x.search, &x.data);
        case 30: FindClose(x.search); return 0;
        case 31: x.uninitialize(); return 0;
        default: return 0;
        }
    }) != 0;
}

void set_control_text(void* control, const char* text);

void set_control_visible(void* control, bool visible) {
    if (!control) return;
    a2fo_call_delphi_two_registers(
        at(kControlSetVisibleRva), control,
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(visible)));
}

bool set_initial_control_bounds(void* control, int left, int top,
                                int width, int height) {
    if (!readable_range(control, kControlHeightOffset + sizeof(int))) {
        return false;
    }
    *reinterpret_cast<int*>(static_cast<std::uint8_t*>(control) +
                            kControlLeftOffset) = left;
    *reinterpret_cast<int*>(static_cast<std::uint8_t*>(control) +
                            kControlTopOffset) = top;
    *reinterpret_cast<int*>(static_cast<std::uint8_t*>(control) +
                            kControlWidthOffset) = width;
    *reinterpret_cast<int*>(static_cast<std::uint8_t*>(control) +
                            kControlHeightOffset) = height;
    return true;
}


void* find_delphi_class_reference(const char* class_name) {
    if (!g_fleet_ops || !class_name || !*class_name) return nullptr;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_fleet_ops);
    if (!readable_range(dos, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const std::uint8_t*>(g_fleet_ops) + dos->e_lfanew);
    if (!readable_range(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }
    const std::size_t image_size = nt->OptionalHeader.SizeOfImage;
    const std::size_t name_length = std::strlen(class_name);
    if (name_length == 0 || name_length > 255) return nullptr;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    const WORD section_count = nt->FileHeader.NumberOfSections;
    if (section_count == 0 || section_count > 96 ||
        !readable_range(sections,
                        static_cast<std::size_t>(section_count) * sizeof(*sections))) {
        return nullptr;
    }
    std::vector<const std::uint8_t*> class_names;
    for (WORD index = 0; index < section_count; ++index) {
        const IMAGE_SECTION_HEADER& section = sections[index];
        const std::size_t size = std::max<std::size_t>(
            section.Misc.VirtualSize, section.SizeOfRawData);
        if (section.VirtualAddress >= image_size || size <= name_length) continue;
        const std::size_t bounded = std::min<std::size_t>(
            size, image_size - section.VirtualAddress);
        const auto* begin = reinterpret_cast<const std::uint8_t*>(g_fleet_ops) +
                            section.VirtualAddress;
        if (!readable_range(begin, bounded)) continue;
        for (std::size_t offset = 0; offset + name_length + 1 <= bounded;
             ++offset) {
            if (begin[offset] == name_length &&
                std::memcmp(begin + offset + 1, class_name, name_length) == 0) {
                class_names.push_back(begin + offset);
            }
        }
    }
    if (class_names.empty()) return nullptr;

    const auto* image_begin = reinterpret_cast<const std::uint8_t*>(g_fleet_ops);
    for (const std::uint8_t* class_string : class_names) {
        const std::uintptr_t class_string_address =
            reinterpret_cast<std::uintptr_t>(class_string);
        for (WORD index = 0; index < section_count; ++index) {
            const IMAGE_SECTION_HEADER& section = sections[index];
            const std::size_t size = std::max<std::size_t>(
                section.Misc.VirtualSize, section.SizeOfRawData);
            if (section.VirtualAddress >= image_size || size < sizeof(void*)) {
                continue;
            }
            const std::size_t bounded = std::min<std::size_t>(
                size, image_size - section.VirtualAddress);
            const auto* begin = image_begin + section.VirtualAddress;
            if (!readable_range(begin, bounded)) continue;
            for (std::size_t offset = 0; offset + sizeof(std::uintptr_t) <= bounded;
                 offset += sizeof(std::uintptr_t)) {
                std::uintptr_t value = 0;
                std::memcpy(&value, begin + offset, sizeof(value));
                if (value != class_string_address) continue;
                const std::uintptr_t class_name_slot =
                    reinterpret_cast<std::uintptr_t>(begin + offset);
                // Across the Delphi 32-bit VMT layouts relevant to Fleet Ops,
                // vmtSelfPtr sits eight pointer slots before vmtClassName and
                // vmtInstanceSize immediately follows vmtClassName. Reading
                // those relationships avoids pinning this lookup to one exact
                // negative-offset table revision.
                if (class_name_slot <
                        reinterpret_cast<std::uintptr_t>(image_begin) + 32 ||
                    !readable_range(reinterpret_cast<const void*>(
                        class_name_slot - 32), 40)) {
                    continue;
                }
                std::uintptr_t vmt = 0;
                std::memcpy(&vmt, reinterpret_cast<const void*>(
                    class_name_slot - 32), sizeof(vmt));
                std::uint32_t instance_size = 0;
                std::memcpy(&instance_size, reinterpret_cast<const void*>(
                    class_name_slot + sizeof(void*)), sizeof(instance_size));
                const std::uintptr_t image_start =
                    reinterpret_cast<std::uintptr_t>(image_begin);
                if (vmt > class_name_slot && vmt - class_name_slot >= 32 &&
                    vmt - class_name_slot <= 96 && vmt < image_start + image_size &&
                    instance_size >= 0x100 && instance_size <= 0x2000 &&
                    readable_range(reinterpret_cast<const void*>(vmt),
                                   sizeof(void*))) {
                    return reinterpret_cast<void*>(vmt);
                }
            }
        }
    }
    return nullptr;
}

bool is_flag_checkbox_for_form(void* control, void* form) {
    return control && form && g_jvg_checkbox_class &&
        readable_range(control, kJvgCheckBoxOptionsOffset + 1) &&
        field<void*>(control, 0) == g_jvg_checkbox_class &&
        field<void*>(control, kComponentOwnerOffset) == form &&
        field<void*>(control, kControlParentOffset) == form;
}

void* create_flag_checkbox(void* form, const char* text,
                           int left, int top, int width, int height) {
    if (!g_jvg_checkbox_class) return nullptr;
    void* control = a2fo_call_delphi_constructor(
        at(kJvgCheckBoxCreateRva), g_jvg_checkbox_class, form);
    if (!control ||
        !readable_range(control, kJvgCheckBoxOptionsOffset + 1) ||
        field<void*>(control, 0) != g_jvg_checkbox_class) {
        return nullptr;
    }
    a2fo_call_delphi_two_registers(at(kComponentInsertRva), form, control);
    if (field<void*>(control, kComponentOwnerOffset) != form ||
        !set_initial_control_bounds(control, left, top, width, height)) {
        return nullptr;
    }
    a2fo_call_delphi_two_registers(at(kControlSetParentRva), control, form);
    if (field<void*>(control, kControlParentOffset) != form) return nullptr;
    a2fo_call_delphi_one_register(at(kAssignJvgCheckBoxImagesRva), control);
    set_control_text(control, text);
    a2fo_call_delphi_two_registers(
        at(kJvgCheckBoxSetCheckedRva), control, nullptr);
    set_control_visible(control, true);
    a2fo_call_delphi_one_register(at(kControlBringToFrontRva), control);
    a2fo_call_delphi_one_register(at(kControlRepaintRva), control);
    return control;
}

void set_flag_toggle_caption(void* button, bool checked, const char* name) {
    if (!button || !name) return;
    const std::string caption = std::string(checked ? "[X] " : "[ ] ") + name;
    set_control_text(button, caption.c_str());
    a2fo_call_delphi_one_register(at(kControlRepaintRva), button);
}

bool valid_flag_toggle_button(void* button, void* form) {
    return button && form && g_modules_button &&
        readable_range(button, kControlHeightOffset + sizeof(std::int32_t)) &&
        readable_range(g_modules_button, sizeof(void*)) &&
        field<void*>(button, 0) == field<void*>(g_modules_button, 0) &&
        field<void*>(button, kComponentOwnerOffset) == form &&
        field<void*>(button, kControlParentOffset) == form;
}

void* create_flag_toggle_button(void* form, int left, int top,
                                int width, int height,
                                const char* name, bool checked) {
    if (!form || !g_modules_button ||
        !readable_range(g_modules_button, sizeof(void*))) {
        return nullptr;
    }
    void* jet_button_class = field<void*>(g_modules_button, 0);
    if (!jet_button_class || !readable_range(jet_button_class, sizeof(void*))) {
        return nullptr;
    }
    void* button = a2fo_call_delphi_constructor(
        at(kJetButtonCreateRva), jet_button_class, form);
    if (!button || !readable_range(
            button, kControlHeightOffset + sizeof(std::int32_t)) ||
        field<void*>(button, 0) != jet_button_class) {
        return nullptr;
    }
    a2fo_call_delphi_two_registers(at(kComponentInsertRva), form, button);
    a2fo_call_delphi_two_registers(at(kControlSetParentRva), button, form);
    if (field<void*>(button, kComponentOwnerOffset) != form ||
        field<void*>(button, kControlParentOffset) != form) {
        return nullptr;
    }
    a2fo_call_delphi_set_bounds(
        at(kJetButtonSetBoundsRva), button, left, top, width, height);
    a2fo_call_delphi_two_registers(
        at(kAssignButtonImagesLargeCenterRva), button,
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(8)));
    set_flag_toggle_caption(button, checked, name);
    a2fo_call_delphi_one_register(at(kControlBringToFrontRva), button);
    a2fo_call_delphi_one_register(at(kControlRepaintRva), button);
    return button;
}

bool ensure_jet_flag_toggles(void* form, int row_left, int row_top,
                             int button_width, int button_height) {
    struct Context {void* form;int left,top,width,height;std::exception_ptr exception{};
    } c{form,row_left,row_top,button_width,button_height};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 340:return c.form!=nullptr;
            case 341:return g_modules_button!=nullptr;
            case 342:return c.width>0;
            case 343:return g_flag_toggle_form==c.form;
            case 344:return valid_flag_toggle_button(g_nointro_toggle_button,c.form);
            case 345:return valid_flag_toggle_button(g_noai_toggle_button,c.form);
            case 346:a2fo_call_delphi_set_bounds(at(kJetButtonSetBoundsRva),
                g_nointro_toggle_button,c.left,c.top,c.width,c.height);return 0;
            case 347:a2fo_call_delphi_set_bounds(at(kJetButtonSetBoundsRva),
                g_noai_toggle_button,c.left+c.width+18,c.top,c.width,c.height);return 0;
            case 348:set_flag_toggle_caption(g_nointro_toggle_button,
                g_nointro_fallback_state.checked,"nointro");return 0;
            case 349:set_flag_toggle_caption(g_noai_toggle_button,
                g_noai_fallback_state.checked,"noai");return 0;
            case 350:set_control_visible(g_nointro_toggle_button,true);return 0;
            case 351:set_control_visible(g_noai_toggle_button,true);return 0;
            case 352:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),
                g_nointro_toggle_button);return 0;
            case 353:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),
                g_noai_toggle_button);return 0;
            case 354:g_flag_toggle_form=c.form;return 0;
            case 355:g_nointro_toggle_button=create_flag_toggle_button(c.form,c.left,c.top,
                c.width,c.height,"nointro",g_nointro_fallback_state.checked);return 0;
            case 356:g_noai_toggle_button=create_flag_toggle_button(c.form,c.left+c.width+18,
                c.top,c.width,c.height,"noai",g_noai_fallback_state.checked);return 0;
            case 357:return g_nointro_toggle_button!=nullptr;
            case 358:return g_noai_toggle_button!=nullptr;
            case 359:return g_nointro_toggle_button!=nullptr;
            case 360:set_control_visible(g_nointro_toggle_button,false);return 0;
            case 361:return g_noai_toggle_button!=nullptr;
            case 362:set_control_visible(g_noai_toggle_button,false);return 0;
            case 363:g_nointro_toggle_button=nullptr;return 0;
            case 364:g_noai_toggle_button=nullptr;return 0;
            case 365:g_flag_toggle_form=nullptr;return 0;
            case 366:destroy_fallback_flag_windows();return 0;
            case 367:log("Module menu: VCL nointro/noai checkbox-style TJetButton fallback active at "+
                std::to_string(c.left)+","+std::to_string(c.top));return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_module_menu_jet_flags(&c,op)!=0;
    if(c.exception)std::rethrow_exception(c.exception);
    return result;
}

void ensure_flag_checkboxes(void* form, int modules_left, int modules_top,
                            int button_width, int button_height) {
    struct Context {void* form;int modules_left,modules_top,button_width,button_height;
        int row_left{},row_top{},toggle_left{},toggle_top{};
        std::exception_ptr exception{};
    } c{form,modules_left,modules_top,button_width,button_height};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 300:a2fo_rs_stage8_b_module_menu_flag_row(c.modules_left,c.modules_top,
                c.button_width,c.button_height,&c.row_left,&c.row_top);return 0;
            case 301:return g_jvg_checkbox_class!=nullptr;
            case 302:g_jvg_checkbox_class=find_delphi_class_reference("TJvgCheckBox");return 0;
            case 303:return g_jvg_checkbox_class!=nullptr;
            case 304:log("Module menu: native TJvgCheckBox class reference discovered");return 0;
            case 305:return g_jvg_checkbox_class!=nullptr;
            case 306:return g_flag_checkbox_form==c.form;
            case 307:return is_flag_checkbox_for_form(g_nointro_checkbox,c.form);
            case 308:return is_flag_checkbox_for_form(g_noai_checkbox,c.form);
            case 309:destroy_fallback_flag_windows();return 0;
            case 310:set_control_visible(g_nointro_checkbox,true);return 0;
            case 311:set_control_visible(g_noai_checkbox,true);return 0;
            case 312:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_nointro_checkbox);return 0;
            case 313:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_noai_checkbox);return 0;
            case 314:g_flag_checkbox_form=c.form;return 0;
            case 315:g_nointro_checkbox=create_flag_checkbox(c.form,"nointro",
                c.row_left,c.row_top,78,18);return 0;
            case 316:g_noai_checkbox=create_flag_checkbox(c.form,"noai",
                c.row_left+84,c.row_top,58,18);return 0;
            case 317:return g_nointro_checkbox!=nullptr;
            case 318:return g_noai_checkbox!=nullptr;
            case 319:destroy_fallback_flag_windows();return 0;
            case 320:if(g_nointro_toggle_button)set_control_visible(g_nointro_toggle_button,false);return 0;
            case 321:if(g_noai_toggle_button)set_control_visible(g_noai_toggle_button,false);return 0;
            case 322:log("Module menu: native nointro/noai checkboxes active");return 0;
            case 324:if(g_nointro_checkbox)set_control_visible(g_nointro_checkbox,false);return 0;
            case 325:if(g_noai_checkbox)set_control_visible(g_noai_checkbox,false);return 0;
            case 326:g_nointro_checkbox=nullptr;return 0;
            case 327:g_noai_checkbox=nullptr;return 0;
            case 328:log("Module menu: native checkbox construction unavailable; using "
                "Win32 checkbox fallback v3");return 0;
            case 329:log("Module menu: TJvgCheckBox class reference unavailable; using "
                "Win32 checkbox fallback v3");return 0;
            case 330:a2fo_rs_stage8_b_module_menu_toggle_row(c.modules_left,c.modules_top,
                c.button_width,c.button_height,&c.toggle_left,&c.toggle_top);return 0;
            case 331:return ensure_jet_flag_toggles(c.form,c.toggle_left,c.toggle_top,
                c.button_width,c.button_height);
            case 332:log("Module menu: nointro/noai VCL fallback toggles active at "+
                std::to_string(c.toggle_left)+","+std::to_string(c.toggle_top)+
                " width="+std::to_string(c.button_width));return 0;
            case 333:log("Module menu: nointro/noai VCL fallback toggles could not be created");return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_flags(&c,op);
    if(c.exception)std::rethrow_exception(c.exception);
}

void* discover_shortcut_button(void* form, void* launch) {
    if (!form || !launch ||
        !readable_range(form, 0x3b4) ||
        !readable_range(launch, kControlHeightOffset + 4)) {
        return nullptr;
    }
    void* launch_class = field<void*>(launch, 0);
    void* visit_website = field<void*>(form, kVisitWebsiteButtonOffset);
    const int launch_left = field<int>(launch, kControlLeftOffset);
    const int launch_top = field<int>(launch, kControlTopOffset);
    const int launch_height = field<int>(launch, kControlHeightOffset);
    void* best = nullptr;
    int best_left = 0x7fffffff;
    // The stock bottom-row controls are adjacent form fields around the known
    // grid/Launch/Visit Website members. Discover the next same-class control
    // geometrically instead of baking another brittle Delphi field offset in.
    for (std::size_t offset = 0x374; offset <= 0x3b0; offset += sizeof(void*)) {
        void* candidate = field<void*>(form, offset);
        if (!candidate || candidate == launch || candidate == visit_website ||
            candidate == g_modules_button ||
            !readable_range(candidate, kControlHeightOffset + 4) ||
            field<void*>(candidate, 0) != launch_class) {
            continue;
        }
        const int left = field<int>(candidate, kControlLeftOffset);
        const int top = field<int>(candidate, kControlTopOffset);
        const int height = field<int>(candidate, kControlHeightOffset);
        if (left > launch_left && left < best_left &&
            std::abs(top - launch_top) <= 4 &&
            std::abs(height - launch_height) <= 4) {
            best = candidate;
            best_left = left;
        }
    }
    return best;
}

const char* state_text(ModulePolicyState state) {
    switch (state) {
        case ModulePolicyState::active: return "Active";
        case ModulePolicyState::required: return "Required";
        case ModulePolicyState::rejected: return "Incompatible";
        case ModulePolicyState::conflict: return "Policy conflict";
        case ModulePolicyState::missing_active: return "Selected - missing";
        case ModulePolicyState::missing_required: return "Required - missing";
        case ModulePolicyState::inactive:
        default: return "Inactive";
    }
}

bool state_locked(ModulePolicyState state) {
    return a2fo_rs_stage8_b_module_menu_state_locked(static_cast<std::uint32_t>(state))!=0;
}

bool state_checked(ModulePolicyState state) {
    return a2fo_rs_stage8_b_module_menu_state_checked(static_cast<std::uint32_t>(state))!=0;
}

struct ModuleDialogContext {
    HWND owner = nullptr;
    HWND window = nullptr;
    HWND list = nullptr;
    HWND crash_consent = nullptr;
    HWND report_mode = nullptr;
    bool mailer_installed = false;
    SelectedMod selected;
    ModulePolicy policy;
    bool initializing = true;
    bool saved = false;
};

void set_control_font(HWND control) {
    if (control) {
        SendMessageA(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
}

void draw_dark_button_content(HDC dc, RECT rectangle, const char* caption,
                              bool pressed, bool disabled, bool focused) {
    if (!dc) return;
    const int saved_dc = SaveDC(dc);
    HBRUSH background = CreateSolidBrush(
        pressed ? RGB(20, 45, 80) : RGB(0, 0, 0));
    FillRect(dc, &rectangle, background);
    DeleteObject(background);
    HPEN border = CreatePen(PS_SOLID, 1,
                            disabled ? RGB(80, 80, 80) : RGB(155, 185, 225));
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rectangle.left, rectangle.top,
              rectangle.right, rectangle.bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc,
                 disabled ? RGB(115, 115, 115) : RGB(255, 255, 255));
    DrawTextA(dc, caption ? caption : "", -1, &rectangle,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (focused) {
        InflateRect(&rectangle, -3, -3);
        DrawFocusRect(dc, &rectangle);
    }
    if (saved_dc != 0) RestoreDC(dc, saved_dc);
}

void draw_dark_button(const DRAWITEMSTRUCT* draw) {
    if (!draw) return;
    char caption[128]{};
    GetWindowTextA(draw->hwndItem, caption, sizeof(caption));
    draw_dark_button_content(
        draw->hDC, draw->rcItem, caption,
        (draw->itemState & ODS_SELECTED) != 0,
        (draw->itemState & ODS_DISABLED) != 0,
        (draw->itemState & ODS_FOCUS) != 0);
}

void populate_module_list(ModuleDialogContext& dialog) {
    struct Context {
        ModuleDialogContext& dialog; LVCOLUMNA column{};
        const ModulePolicyEntry* entry = nullptr;
        std::optional<LVITEMA> item; std::optional<std::string> status;
    } c{dialog, {}, nullptr, {}, {}};
    module_menu_policy(2, c, [](Context& x, std::uint32_t op, std::uint64_t index) -> std::uint64_t {
        switch (op) {
        case 0: x.column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM; x.column.pszText = const_cast<char*>("Module"); x.column.cx = 250; return 0;
        case 1: ListView_InsertColumn(x.dialog.list, 0, &x.column); return 0;
        case 2: x.column.pszText = const_cast<char*>("Selection"); x.column.cx = 140; x.column.iSubItem = 1; ListView_InsertColumn(x.dialog.list, 1, &x.column); return 0;
        case 3: x.column.pszText = const_cast<char*>("Current / applies"); x.column.cx = 140; x.column.iSubItem = 2; ListView_InsertColumn(x.dialog.list, 2, &x.column); return 0;
        case 4: return x.dialog.policy.entries.size();
        case 5:
            x.entry = &x.dialog.policy.entries[static_cast<std::size_t>(index)]; x.item.emplace();
            x.item->mask = LVIF_TEXT | LVIF_PARAM; x.item->iItem = static_cast<int>(index); x.item->pszText = const_cast<char*>(x.entry->name.c_str()); x.item->lParam = static_cast<LPARAM>(index);
            ListView_InsertItem(x.dialog.list, &*x.item); return 0;
        case 6: ListView_SetItemText(x.dialog.list, static_cast<int>(index), 1, const_cast<char*>(state_text(x.entry->state))); return 0;
        case 7: x.status.emplace(builtin_module_status(x.entry->name, x.dialog.selected.roots)); return 0;
        case 8: ListView_SetItemText(x.dialog.list, static_cast<int>(index), 2, const_cast<char*>(x.status->c_str())); return 0;
        case 9: ListView_SetCheckState(x.dialog.list, static_cast<int>(index), state_checked(x.entry->state)); return 0;
        case 10: x.status.reset(); x.item.reset(); return 0;
        case 11: x.dialog.initializing = false; return 0;
        default: return 0;
        }
    });
}

bool save_dialog_selection(ModuleDialogContext& context) {
    struct Context {
        ModuleDialogContext& dialog;
        std::wstring reporting_root{};
        DWORD helper_attributes{INVALID_FILE_ATTRIBUTES};
        bool reporting_enabled{};
        bool all_sessions{};
        std::vector<std::string> active{};
        std::string error{};
        std::optional<log_mailer::PreferencesLock> lock{};
        std::wstring previous_reporting{};
        std::optional<log_mailer::Config> previous_config{};
        bool reporting_changed{};
        bool saved{};
        std::vector<InstalledModule> installed{};
        ModulePolicy updated_policy{};
        std::exception_ptr exception{};
    } c{context};
    const auto op=[](void* opaque,std::uint32_t id,std::uint64_t argument) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 200:c.reporting_root=log_mailer::from_ansi(g_data_root);return 0;
            case 201:return c.dialog.crash_consent!=nullptr;
            case 202:c.helper_attributes=GetFileAttributesW(log_mailer::join(
                c.reporting_root,L"modules\\A2FOLogMailerHelper.exe").c_str());return 0;
            case 203:return c.dialog.crash_consent!=nullptr;
            case 204:c.reporting_enabled=SendMessageA(c.dialog.crash_consent,
                BM_GETCHECK,0,0)==BST_CHECKED;return c.reporting_enabled;
            case 205:return c.dialog.report_mode!=nullptr;
            case 206:c.all_sessions=SendMessageA(c.dialog.report_mode,
                CB_GETCURSEL,0,0)==1;return c.all_sessions;
            case 207:return c.dialog.policy.entries.size();
            case 208:return c.dialog.policy.entries[static_cast<std::size_t>(argument)].installed;
            case 209:return state_locked(c.dialog.policy.entries[
                static_cast<std::size_t>(argument)].state);
            case 210:return ListView_GetCheckState(c.dialog.list,static_cast<int>(argument))!=FALSE;
            case 211:c.active.push_back(c.dialog.policy.entries[
                static_cast<std::size_t>(argument)].name);return 0;
            case 220:c.lock.emplace(c.dialog.crash_consent!=nullptr);return 0;
            case 221:c.previous_config.emplace(c.dialog.crash_consent?
                log_mailer::load_config(c.reporting_root):log_mailer::Config{});return 0;
            case 222:c.reporting_changed=c.dialog.crash_consent &&
                (c.reporting_enabled!=c.previous_config->enabled ||
                 c.all_sessions!=c.previous_config->all_sessions);
                return c.reporting_changed;
            case 223:return c.lock->held;
            case 224:c.error="Email settings are busy. Try Save again.";return 0;
            case 225:return c.helper_attributes==INVALID_FILE_ATTRIBUTES ||
                (c.helper_attributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
            case 226:c.error="The log mailer installation is incomplete. Please reinstall its "
                "package, including A2FOLogMailerHelper.exe. No logs have been sent.";return 0;
            case 227:return c.previous_config->ready();
            case 228:c.error="Automatic log emails are unavailable because this installation's "
                "reporting settings are invalid. Please reinstall the log mailer "
                "package or contact its distributor. No logs have been sent.";return 0;
            case 229:return log_mailer::preferences_section(c.reporting_root,
                c.previous_reporting);
            case 230:return log_mailer::save_preferences(c.reporting_root,
                (argument&1u)!=0,(argument&2u)!=0,c.previous_reporting);
            case 231:c.error="Email preferences could not be saved. Module selection has not changed.";return 0;
            case 232:c.saved=save_active_module_selection(c.dialog.selected.info_path,
                c.active,c.error);return c.saved;
            case 233:return log_mailer::write_section(c.reporting_root,c.previous_reporting);
            case 234:c.error+="\nEmail preferences could not be restored; review A2FOLogMailer.ini.";return 0;
            case 235:c.lock.reset();return 0;
            case 236:MessageBoxA(c.dialog.window,c.error.c_str(),"Modules",
                MB_OK|MB_ICONERROR);return 0;
            case 237:c.dialog.saved=true;return 0;
            case 238:log("Module menu: saved selection for "+c.dialog.selected.title);return 0;
            case 239:c.installed=discover_installed_modules(g_data_root);return 0;
            case 240:c.updated_policy=evaluate_module_policy(c.dialog.selected.roots,
                c.installed);return 0;
            case 241:return apply_builtin_module_policy(c.dialog.selected.roots,
                c.updated_policy,c.error);
            case 242:log("Module menu: "+c.error);return 0;
            case 243:MessageBoxA(c.dialog.window,c.error.c_str(),"Modules saved",
                MB_OK|MB_ICONWARNING);return 0;
            case 244:return builtin_modules_match_roots(c.dialog.selected.roots);
            case 245:log("Module menu: built-in feature changes applied to running mod without restart");return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const bool saved=a2fo_rs_stage8_b_module_menu_save(&c,op)!=0;
    if(c.exception)std::rethrow_exception(c.exception);
    return saved;
}

LRESULT handle_list_notification(ModuleDialogContext& context,
                                 NMHDR* header) {
    struct Context {ModuleDialogContext& dialog;NMHDR* header;
        NMLISTVIEW* change{};NMLVCUSTOMDRAW* custom{};
        std::size_t index{};std::exception_ptr exception{};
    } c{context,header};
    const auto op=[](void* opaque,std::uint32_t id,std::uint64_t argument) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 400:return c.header!=nullptr;
            case 401:return c.header->hwndFrom==c.dialog.list;
            case 402:return c.header->code==LVN_ITEMCHANGING;
            case 403:return !c.dialog.initializing;
            case 404:c.change=reinterpret_cast<NMLISTVIEW*>(c.header);
                return static_cast<std::uint32_t>(c.change->iItem);
            case 405:return c.dialog.policy.entries.size();
            case 406:return c.change->uOldState&LVIS_STATEIMAGEMASK;
            case 407:return c.change->uNewState&LVIS_STATEIMAGEMASK;
            case 408:return static_cast<std::uint32_t>(c.dialog.policy.entries[
                static_cast<std::size_t>(c.change->iItem)].state);
            case 409:return TRUE;
            case 410:return c.header->code==NM_CUSTOMDRAW;
            case 411:c.custom=reinterpret_cast<NMLVCUSTOMDRAW*>(c.header);
                return c.custom->nmcd.dwDrawStage==CDDS_PREPAINT;
            case 412:return CDRF_NOTIFYITEMDRAW;
            case 413:return c.custom->nmcd.dwDrawStage==CDDS_ITEMPREPAINT;
            case 414:c.index=c.custom->nmcd.dwItemSpec;return static_cast<std::uint32_t>(c.index);
            case 415:return c.dialog.policy.entries.size();
            case 416:return static_cast<std::uint32_t>(c.dialog.policy.entries[c.index].state);
            case 417:
                switch(argument) {
                case 0:c.custom->clrText=RGB(255,105,105);break;
                case 1:c.custom->clrText=RGB(145,205,255);break;
                case 2:c.custom->clrText=RGB(145,145,145);break;
                default:c.custom->clrText=RGB(235,235,235);break;
                }
                return 0;
            case 418:c.custom->clrTextBk=RGB(0,0,0);return 0;
            case 419:return CDRF_NEWFONT;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_b_module_menu_notification(&c,op);
    if(c.exception)std::rethrow_exception(c.exception);
    return static_cast<LRESULT>(result);
}

void create_module_dialog_controls(ModuleDialogContext* context, HWND window) {
            const std::string heading = "Modules for " + context->selected.title;
            HWND label = CreateWindowExA(
                0, "STATIC", heading.c_str(), WS_CHILD | WS_VISIBLE,
                18, 16, 560, 22, window, nullptr, nullptr, nullptr);
            set_control_font(label);
            label = CreateWindowExA(
                0, "STATIC",
                "Required modules are locked on; incompatible modules are locked off.",
                WS_CHILD | WS_VISIBLE, 18, 42, 560, 20,
                window, nullptr, nullptr, nullptr);
            set_control_font(label);
            context->list = CreateWindowExA(
                WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT |
                    LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                18, 70, 554, 310, window,
                reinterpret_cast<HMENU>(kModuleListId), nullptr, nullptr);
            set_control_font(context->list);
            ListView_SetExtendedListViewStyle(
                context->list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT |
                                   LVS_EX_DOUBLEBUFFER);
            ListView_SetBkColor(context->list, RGB(0, 0, 0));
            ListView_SetTextBkColor(context->list, RGB(0, 0, 0));
            ListView_SetTextColor(context->list, RGB(235, 235, 235));
            populate_module_list(*context);

            label = CreateWindowExA(
                0, "STATIC",
                "Save applies live built-ins to the running mod; other choices apply on launch.",
                WS_CHILD | WS_VISIBLE, 18, 390, 554, 20,
                window, nullptr, nullptr, nullptr);
            set_control_font(label);
            if (context->mailer_installed) {
                context->crash_consent = CreateWindowExA(
                    0, "BUTTON", "Allow automatic log emails (optional, all mods)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    18, 418, 554, 24, window,
                    reinterpret_cast<HMENU>(kCrashConsentId), nullptr, nullptr);
                set_control_font(context->crash_consent);
                const std::wstring root = log_mailer::from_ansi(g_data_root);
                SendMessageA(context->crash_consent, BM_SETCHECK,
                    log_mailer::load_config(root).enabled ? BST_CHECKED : BST_UNCHECKED, 0);
                label = CreateWindowExA(0, "STATIC",
                    "To: aclaydon1@googlemail.com. Includes fresh FO bugreport.txt if present.",
                    WS_CHILD | WS_VISIBLE, 18, 446, 554, 20,
                    window, nullptr, nullptr, nullptr);
                set_control_font(label);
                context->report_mode = CreateWindowExA(
                    0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                        CBS_DROPDOWNLIST | WS_VSCROLL,
                    18, 468, 554, 120, window,
                    reinterpret_cast<HMENU>(kReportModeId), nullptr, nullptr);
                set_control_font(context->report_mode);
                SendMessageA(context->report_mode, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>("Crash reports only - FO crash report and A2FO logs"));
                SendMessageA(context->report_mode, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>("All game logs - send after every session, including normal exits"));
                SendMessageA(context->report_mode, CB_SETCURSEL,
                    log_mailer::load_config(root).all_sessions ? 1 : 0, 0);
                label = CreateWindowExA(0, "STATIC",
                    "Also select A2FOLogMailer above. Email setup is included with the module.",
                    WS_CHILD | WS_VISIBLE, 18, 498, 554, 20,
                    window, nullptr, nullptr, nullptr);
                set_control_font(label);
            }
            HWND button = CreateWindowExA(
                0, "BUTTON", "Save",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                376, context->mailer_installed ? 530 : 423, 94, 26, window,
                reinterpret_cast<HMENU>(kSaveButtonId), nullptr, nullptr);
            set_control_font(button);
            button = CreateWindowExA(
                0, "BUTTON", "Cancel",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                478, context->mailer_installed ? 530 : 423, 94, 26, window,
                reinterpret_cast<HMENU>(kCancelButtonId), nullptr, nullptr);
            set_control_font(button);
 }

LRESULT CALLBACK module_dialog_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam) {
    struct Context {HWND window;UINT message;WPARAM wparam;LPARAM lparam;
        ModuleDialogContext* dialog{};std::exception_ptr exception{};
    } c{window,message,wparam,lparam};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 1000:c.dialog=reinterpret_cast<ModuleDialogContext*>(
            GetWindowLongPtrA(c.window,GWLP_USERDATA));return 0;
        case 1001:return c.message==WM_NCCREATE;
        case 1002:{auto* create=reinterpret_cast<CREATESTRUCTA*>(c.lparam);
            c.dialog=static_cast<ModuleDialogContext*>(create->lpCreateParams);
            SetWindowLongPtrA(c.window,GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(c.dialog));
            c.dialog->window=c.window;return 0;}
        case 1003:return c.dialog!=nullptr;
        case 1004:return static_cast<std::uint32_t>(DefWindowProcA(
            c.window,c.message,c.wparam,c.lparam));
        case 1005:return c.message==WM_CREATE;
        case 1006:create_module_dialog_controls(c.dialog,c.window);return 0;
        case 1007:return c.message==WM_COMMAND;
        case 1008:return LOWORD(c.wparam)==kSaveButtonId;
        case 1009:return HIWORD(c.wparam)==BN_CLICKED;
        case 1010:return save_dialog_selection(*c.dialog);
        case 1011:DestroyWindow(c.window);return 0;
        case 1012:return LOWORD(c.wparam)==kCancelButtonId;
        case 1013:return c.message==WM_NOTIFY;
        case 1014:return static_cast<std::uint32_t>(handle_list_notification(
            *c.dialog,reinterpret_cast<NMHDR*>(c.lparam)));
        case 1015:return c.message==WM_DRAWITEM;
        case 1016:draw_dark_button(reinterpret_cast<DRAWITEMSTRUCT*>(c.lparam));return 0;
        case 1017:return TRUE;
        case 1018:return c.message==WM_CTLCOLORSTATIC;
        case 1019:{HDC dc=reinterpret_cast<HDC>(c.wparam);
            SetTextColor(dc,RGB(235,235,235));
            SetBkColor(dc,RGB(8,8,8));
            static HBRUSH background=CreateSolidBrush(RGB(8,8,8));
            return reinterpret_cast<std::uintptr_t>(background);}
        case 1020:return static_cast<std::uint32_t>(DefWindowProcA(
            c.window,c.message,c.wparam,c.lparam));
        case 1021:return c.message==WM_CLOSE;
        case 1022:return c.message==WM_DESTROY;
        case 1023:c.dialog->window=nullptr;return 0;
        default:return 0;
        }}catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_b_module_menu_dialog_message(&c,op);
    if(c.exception)std::rethrow_exception(c.exception);
    return static_cast<LRESULT>(result);
}

bool register_dialog_class() {
    static bool attempted = false;
    static bool registered = false;
    if (attempted) return registered;
    attempted = true;
    WNDCLASSEXA window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &module_dialog_proc;
    window_class.hInstance =
        reinterpret_cast<HINSTANCE>(GetModuleHandleA("A2FOExtensions.dll"));
    window_class.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    window_class.hbrBackground = CreateSolidBrush(RGB(8, 8, 8));
    window_class.lpszClassName = kDialogClassName;
    registered = RegisterClassExA(&window_class) != 0 ||
                 GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    return registered;
}

void centre_window(HWND window, HWND owner) {
    RECT window_rect{};
    RECT owner_rect{};
    GetWindowRect(window, &window_rect);
    if (!owner || !GetWindowRect(owner, &owner_rect)) {
        owner_rect.left = 0;
        owner_rect.top = 0;
        owner_rect.right = GetSystemMetrics(SM_CXSCREEN);
        owner_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    const int x = owner_rect.left +
                  ((owner_rect.right - owner_rect.left) - width) / 2;
    const int y = owner_rect.top +
                  ((owner_rect.bottom - owner_rect.top) - height) / 2;
    SetWindowPos(window, HWND_TOP, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);
}

void show_module_dialog(HWND owner, void* form) {
    struct Context {HWND owner;void* form;SelectedMod selected{};std::string error{};
        std::vector<std::string> diagnostics{};
        std::vector<InstalledModule> installed{};
        std::optional<ModuleDialogContext> dialog{};
        RECT rectangle{};std::string title{};HWND window{};MSG message{};
        std::exception_ptr exception{};
    } c{owner,form};
    const auto op=[](void* opaque,std::uint32_t id,std::uint64_t argument) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 700:return resolve_selected_mod(c.form,c.selected,c.error);
            case 701:MessageBoxA(c.owner,c.error.c_str(),"Modules",MB_OK|MB_ICONERROR);return 0;
            case 702:c.installed=discover_installed_modules(g_data_root,&c.diagnostics);return 0;
            case 703:c.dialog.emplace();return 0;
            case 704:c.dialog->owner=c.owner;return 0;
            case 705:c.dialog->selected=std::move(c.selected);return 0;
            case 706:c.dialog->policy=evaluate_module_policy(c.dialog->selected.roots,
                c.installed);return 0;
            case 707:return c.dialog->policy.entries.size();
            case 708:{const auto& entry=c.dialog->policy.entries[
                static_cast<std::size_t>(argument)];
                return entry.key=="a2fologmailer"&&entry.installed;}
            case 709:c.dialog->mailer_installed=true;return 0;
            case 710:c.dialog->policy.diagnostics.insert(
                c.dialog->policy.diagnostics.end(),c.diagnostics.begin(),
                c.diagnostics.end());return 0;
            case 711:return register_dialog_class();
            case 712:MessageBoxA(c.owner,"The module-selection window could not be created.",
                "Modules",MB_OK|MB_ICONERROR);return 0;
            case 713:{INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES};
                InitCommonControlsEx(&controls);return 0;}
            case 714:c.rectangle=RECT{0,0,592,c.dialog->mailer_installed?572:470};
                AdjustWindowRectEx(&c.rectangle,WS_CAPTION|WS_SYSMENU|WS_POPUP,
                    FALSE,WS_EX_DLGMODALFRAME);return 0;
            case 715:c.title="Modules - "+c.dialog->selected.title;return 0;
            case 716:c.window=CreateWindowExA(WS_EX_DLGMODALFRAME,kDialogClassName,
                c.title.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP,
                CW_USEDEFAULT,CW_USEDEFAULT,
                c.rectangle.right-c.rectangle.left,
                c.rectangle.bottom-c.rectangle.top,c.owner,nullptr,
                reinterpret_cast<HINSTANCE>(GetModuleHandleA("A2FOExtensions.dll")),
                &*c.dialog);return 0;
            case 717:return c.window!=nullptr;
            case 718:MessageBoxA(c.owner,"The module-selection window could not be created.",
                "Modules",MB_OK|MB_ICONERROR);return 0;
            case 719:centre_window(c.window,c.owner);return 0;
            case 720:EnableWindow(c.owner,FALSE);return 0;
            case 721:ShowWindow(c.window,SW_SHOW);return 0;
            case 722:UpdateWindow(c.window);return 0;
            case 723:return c.dialog->window!=nullptr;
            case 724:return static_cast<std::uint32_t>(GetMessageA(&c.message,nullptr,0,0));
            case 725:return IsDialogMessageA(c.window,&c.message)!=FALSE;
            case 726:TranslateMessage(&c.message);return 0;
            case 727:DispatchMessageA(&c.message);return 0;
            case 728:EnableWindow(c.owner,TRUE);return 0;
            case 729:SetActiveWindow(c.owner);return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_dialog(&c,op);
    if(c.exception)std::rethrow_exception(c.exception);
}

bool policy_allows_launch(void* form, std::string& error) {
    SelectedMod selected;
    if (!resolve_selected_mod(form, selected, error)) return false;
    std::vector<std::string> diagnostics;
    const std::vector<InstalledModule> installed =
        discover_installed_modules(g_data_root, &diagnostics);
    const ModulePolicy policy = evaluate_module_policy(selected.roots, installed);
    if (policy.valid) return true;
    error = "This mod cannot be launched because its module requirements are "
            "not satisfied:\r\n\r\n";
    for (const std::string& diagnostic : policy.diagnostics) {
        error += "- " + diagnostic + "\r\n";
    }
    return false;
}

void set_control_text(void* control, const char* text) {
    char* delphi_text = nullptr;
    a2fo_call_delphi_two_registers(
        at(kLongStringFromPcharRva), &delphi_text,
        const_cast<char*>(text));
    a2fo_call_delphi_two_registers(
        at(kControlSetTextRva), control, delphi_text);
    a2fo_call_delphi_one_register(
        at(kLongStringClearRva), &delphi_text);
}

void ensure_modules_button(void* form) {
    struct Context {void* form;HWND window{};void* launch{};
        int left{},top{},width{},height{},button_left{};
        void* button_class{};void* button{};std::exception_ptr error{};
    } c{form};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 30:c.window=field<HWND>(c.form,kFormHandleOffset);return 0;
            case 31:c.launch=field<void*>(c.form,kLaunchButtonOffset);return 0;
            case 32:return IsWindow(c.window)!=FALSE;
            case 33:log("Module menu: Mods form was shown without a valid window handle");return 0;
            case 34:return c.launch!=nullptr;
            case 35:return readable_range(c.launch,kControlHeightOffset+sizeof(std::int32_t));
            case 36:log("Module menu: Mods form was shown without a readable Launch button");return 0;
            case 37:c.left=field<int>(c.launch,kControlLeftOffset);return static_cast<std::uint32_t>(c.left);
            case 38:c.top=field<int>(c.launch,kControlTopOffset);return static_cast<std::uint32_t>(c.top);
            case 39:c.width=field<int>(c.launch,kControlWidthOffset);return static_cast<std::uint32_t>(c.width);
            case 40:c.height=field<int>(c.launch,kControlHeightOffset);return static_cast<std::uint32_t>(c.height);
            case 41:log("Module menu: ignored invalid Launch button bounds "+
                std::to_string(c.left)+","+std::to_string(c.top)+" "+
                std::to_string(c.width)+"x"+std::to_string(c.height));return 0;
            case 42:c.button_left=a2fo_rs_stage8_b_module_menu_button_left(c.left,c.width);return 0;
            case 43:return g_modules_button!=nullptr;
            case 44:return g_modules_button_form==c.form;
            case 45:return readable_range(g_modules_button,kControlHeightOffset+sizeof(std::int32_t));
            case 46:c.button_class=field<void*>(c.launch,0);return 0;
            case 47:return c.button_class!=nullptr;
            case 48:return readable_range(c.button_class,sizeof(void*));
            case 49:log("Module menu: Launch button class was unreadable");return 0;
            case 50:c.button=a2fo_call_delphi_constructor(at(kJetButtonCreateRva),c.button_class,c.form);return 0;
            case 51:return c.button!=nullptr;
            case 52:return readable_range(c.button,kControlHeightOffset+sizeof(std::int32_t));
            case 53:log("Module menu: native TJetButton construction failed");return 0;
            case 54:a2fo_call_delphi_two_registers(at(kComponentInsertRva),c.form,c.button);return 0;
            case 55:a2fo_call_delphi_two_registers(at(kControlSetParentRva),c.button,c.form);return 0;
            case 56:g_modules_button=c.button;g_modules_button_form=c.form;return 0;
            case 57:a2fo_call_delphi_set_bounds(at(kJetButtonSetBoundsRva),g_modules_button,
                c.button_left,c.top,c.width,c.height);return 0;
            case 58:a2fo_call_delphi_two_registers(at(kAssignButtonImagesLargeCenterRva),
                g_modules_button,reinterpret_cast<void*>(static_cast<std::uintptr_t>(8)));return 0;
            case 59:set_control_text(g_modules_button,"Modules");return 0;
            case 60:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_modules_button);return 0;
            case 61:a2fo_call_delphi_one_register(at(kControlRepaintRva),g_modules_button);return 0;
            case 62:g_modules_button_owner=c.window;return 0;
            case 63:g_shortcut_button=discover_shortcut_button(c.form,c.launch);return 0;
            case 64:ensure_flag_checkboxes(c.form,c.button_left,c.top,c.width,c.height);return 0;
            case 66:return g_shortcut_button!=nullptr;
            case 67:log("Module menu: Create Shortcut button was not identified; "
                "shortcut command flags are unavailable");return 0;
            case 68:log("Module menu: native Modules TJetButton active at "+
                std::to_string(c.button_left)+","+std::to_string(c.top)+" "+
                std::to_string(c.width)+"x"+std::to_string(c.height));return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_button(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

}  // namespace

extern "C" void a2fo_mod_settings_form_show_hook_cpp(void* form) {
    clear_pending_launch_flags();
    a2fo_call_delphi_one_register(g_form_show_hook.gateway, form);
    try {
        ensure_modules_button(form);
    } catch (...) {
        log("Module menu: Mods form integration was skipped after an exception");
    }
}

extern "C" void a2fo_mod_settings_launch_hook_cpp(void* form, void* sender) {
    struct Context {void* form;void* sender;std::string error{};
        HWND owner{};bool nointro{},noai{};std::exception_ptr exception{};
    } c{form,sender};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 80:return policy_allows_launch(c.form,c.error);
            case 81:c.owner=field<HWND>(c.form,kFormHandleOffset);return 0;
            case 82:MessageBoxA(c.owner,c.error.c_str(),"Module requirements",
                MB_OK|MB_ICONERROR);return 0;
            case 90:c.nointro=nointro_selected();return c.nointro;
            case 91:c.noai=noai_selected();return c.noai;
            case 92:log(std::string("Module menu: launch command flags requested:")+
                (c.nointro?" /nointro":"")+(c.noai?" /noai":""));return 0;
            case 93:InterlockedExchange(&g_pending_nointro,c.nointro?1:0);return 0;
            case 94:InterlockedExchange(&g_pending_noai,c.noai?1:0);return 0;
            case 95:a2fo_call_delphi_two_registers(g_launch_hook.gateway,c.form,c.sender);return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    try {
        const bool allowed=a2fo_rs_stage8_b_module_menu_launch_admit(&c,op)!=0;
        if(c.exception)std::rethrow_exception(c.exception);
        if(!allowed)return;
    } catch (...) {
        log("Module menu: launch validation failed unexpectedly");
        MessageBoxA(nullptr,
                    "Module requirements could not be validated. Launch was "
                    "cancelled for safety.",
                    "Module requirements", MB_OK | MB_ICONERROR);
        return;
    }
    a2fo_rs_stage8_b_module_menu_launch_commit(&c,op);
    if(c.exception)std::rethrow_exception(c.exception);
    // Fleet Operations may defer the actual process creation until after this
    // event handler returns. Leave the flags armed until the first successful
    // hooked process-launch API consumes them. Reopening the Mods form clears
    // any stale request that never launched.
}

extern "C" void a2fo_module_menu_jet_button_click_hook_cpp(void* button) {
    struct ShortcutState {SelectedMod selected;std::string error;};
    struct Context {void* button;std::optional<ShortcutState> shortcut{};
        bool nointro{},noai{};FILETIME started{};std::exception_ptr exception{};
    } c{button};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 100:return c.button==g_modules_button;
            case 101:return g_modules_button_form!=nullptr;
            case 102:
                try {show_module_dialog(g_modules_button_owner,g_modules_button_form);}
                catch(...){log("Module menu: native Modules button click failed unexpectedly");}
                return 0;
            case 103:return c.button==g_nointro_toggle_button;
            case 104:return g_flag_toggle_form!=nullptr;
            case 105:g_nointro_fallback_state.checked=!g_nointro_fallback_state.checked;return 0;
            case 106:set_flag_toggle_caption(g_nointro_toggle_button,
                g_nointro_fallback_state.checked,"nointro");return 0;
            case 107:log(std::string("Module menu: nointro toggle ")+
                (g_nointro_fallback_state.checked?"enabled":"disabled"));return 0;
            case 108:return c.button==g_noai_toggle_button;
            case 109:return g_flag_toggle_form!=nullptr;
            case 110:g_noai_fallback_state.checked=!g_noai_fallback_state.checked;return 0;
            case 111:set_flag_toggle_caption(g_noai_toggle_button,
                g_noai_fallback_state.checked,"noai");return 0;
            case 112:log(std::string("Module menu: noai toggle ")+
                (g_noai_fallback_state.checked?"enabled":"disabled"));return 0;
            case 113:return c.button==g_shortcut_button;
            case 114:return g_modules_button_form!=nullptr;
            case 115:c.shortcut.emplace();c.nointro=nointro_selected();return 0;
            case 116:c.noai=noai_selected();return 0;
            case 117:
                try {return resolve_selected_mod(g_modules_button_form,
                    c.shortcut->selected,c.shortcut->error);}
                catch(...){log("Module menu: shortcut selection could not be resolved; "
                    "native shortcut creation retained");return 0;}
            case 118:GetSystemTimeAsFileTime(&c.started);return 0;
            case 119:a2fo_call_delphi_one_register(at(kControlClickRva),c.button);return 0;
            case 120:return c.nointro;
            case 121:return c.noai;
            case 122:
                try {return update_recent_desktop_shortcut(c.shortcut->selected,c.nointro,c.noai,
                    c.started)?1u:0u;}
                catch(...){return 2;}
            case 123:
                try {log("Module menu: shortcut command flags updated for "+
                    c.shortcut->selected.folder);}
                catch(...){log("Module menu: shortcut flag update failed unexpectedly; "
                    "native shortcut was left intact");}
                return 0;
            case 124:
                try {log("Module menu: native shortcut was created but its "
                    "command flags could not be updated");}
                catch(...){log("Module menu: shortcut flag update failed unexpectedly; "
                    "native shortcut was left intact");}
                return 0;
            case 125:a2fo_call_delphi_one_register(at(kControlClickRva),c.button);return 0;
            case 126:log("Module menu: shortcut flag update failed unexpectedly; "
                "native shortcut was left intact");return 0;
            default:return 0;
            }
        }catch(...){c.exception=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_module_menu_click(&c,op);
    if(c.exception)std::rethrow_exception(c.exception);
}

bool install_module_menu(HMODULE fleet_ops, const std::string& data_root,
                         void (*log_line)(const std::string&)) {
    struct Context {HMODULE fleet_ops;const std::string& root;
        void (*logger)(const std::string&);std::exception_ptr error{};
    } c{fleet_ops,data_root,log_line};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 0:return c.fleet_ops!=nullptr && !c.root.empty();
            case 1:g_fleet_ops=c.fleet_ops;g_data_root=c.root;g_log_line=c.logger;return 0;
            case 20:log("Module menu: FleetOps Mods-screen signatures did not match");return 0;
            case 21:return a2fo_rs_windows_hook_install(at(kModSettingsFormShowRva),
                reinterpret_cast<void*>(&a2fo_mod_settings_form_show_bridge),
                sizeof(kExpectedFormShow),kExpectedFormShow, &(g_form_show_hook));
            case 22:return a2fo_rs_windows_hook_install(at(kModSettingsLaunchRva),
                reinterpret_cast<void*>(&a2fo_mod_settings_launch_bridge),
                sizeof(kExpectedLaunch),kExpectedLaunch, &(g_launch_hook));
            case 23:return a2fo_rs_windows_patch_jump(at(kJetButtonClickRva),
                reinterpret_cast<void*>(&a2fo_module_menu_jet_button_click_bridge),
                kExpectedJetButtonClick,sizeof(kExpectedJetButtonClick));
            case 24:log("Module menu: FleetOps Mods-screen hooks did not match");return 0;
            case 25:return install_mod_launch_import_hooks();
            case 26:log("Module menu: launch command interception was unavailable; "
                "nointro/noai launch flags are disabled");return 0;
            case 27:log("Module menu: Mods-screen selector, launch validation, and command "
                "flags enabled");return 0;
            default:break;
            }
            constexpr std::uintptr_t addresses[]{kModSettingsFormShowRva,kModSettingsLaunchRva,
                kJetButtonCreateRva,kJetButtonClickRva,kJetButtonSetBoundsRva,
                kJvgCheckBoxCreateRva,kJvgCheckBoxSetCheckedRva,
                kAssignJvgCheckBoxImagesRva,kAssignButtonImagesLargeCenterRva,
                kControlSetParentRva,kControlSetVisibleRva,kControlSetTextRva,
                kControlClickRva,kControlBringToFrontRva,kControlRepaintRva,
                kComponentInsertRva,kLongStringClearRva,kLongStringFromPcharRva};
            const std::uint8_t* expected[]{kExpectedFormShow,kExpectedLaunch,
                kExpectedJetButtonCreate,kExpectedJetButtonClick,kExpectedJetButtonSetBounds,
                kExpectedJvgCheckBoxCreate,kExpectedJvgCheckBoxSetChecked,
                kExpectedAssignJvgCheckBoxImages,kExpectedAssignButtonImagesLargeCenter,
                kExpectedControlSetParent,kExpectedControlSetVisible,kExpectedControlSetText,
                kExpectedControlClick,kExpectedControlBringToFront,kExpectedControlRepaint,
                kExpectedComponentInsert,kExpectedLongStringClear,kExpectedLongStringFromPchar};
            constexpr std::size_t sizes[]{sizeof(kExpectedFormShow),sizeof(kExpectedLaunch),
                sizeof(kExpectedJetButtonCreate),sizeof(kExpectedJetButtonClick),
                sizeof(kExpectedJetButtonSetBounds),sizeof(kExpectedJvgCheckBoxCreate),
                sizeof(kExpectedJvgCheckBoxSetChecked),sizeof(kExpectedAssignJvgCheckBoxImages),
                sizeof(kExpectedAssignButtonImagesLargeCenter),sizeof(kExpectedControlSetParent),
                sizeof(kExpectedControlSetVisible),sizeof(kExpectedControlSetText),
                sizeof(kExpectedControlClick),sizeof(kExpectedControlBringToFront),
                sizeof(kExpectedControlRepaint),sizeof(kExpectedComponentInsert),
                sizeof(kExpectedLongStringClear),sizeof(kExpectedLongStringFromPchar)};
            if(id>=2 && id<=19) {
                const auto index=id-2;
                return std::memcmp(at(addresses[index]),expected[index],sizes[index])==0;
            }
            return 0;
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_module_menu_install(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

}  // namespace a2fo
