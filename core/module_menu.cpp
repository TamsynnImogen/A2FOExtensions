/*
 * File: core/module_menu.cpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Fleet Operations Mods-screen module-selection integration.
 */

#include "module_menu.hpp"

#include "extension_roots.hpp"
#include "hook.hpp"
#include "module_policy.hpp"

#include <commctrl.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
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
constexpr std::uintptr_t kAssignButtonImagesLargeCenterRva = 0x001784c0;
constexpr std::uintptr_t kControlClickRva = 0x0c724c;
constexpr std::uintptr_t kControlSetParentRva = 0x0c55f0;
constexpr std::uintptr_t kControlSetTextRva = 0x0c574c;
constexpr std::uintptr_t kControlBringToFrontRva = 0x0c59ac;
constexpr std::uintptr_t kControlRepaintRva = 0x0c5d30;
constexpr std::uintptr_t kComponentInsertRva = 0x00088274;
constexpr std::uintptr_t kLongStringClearRva = 0x000056b8;
constexpr std::uintptr_t kLongStringFromPcharRva = 0x000058b0;

constexpr std::size_t kFormHandleOffset = 0x1c4;
constexpr std::size_t kGridOffset = 0x370;
constexpr std::size_t kLaunchButtonOffset = 0x39c;
constexpr std::size_t kControlLeftOffset = 0x40;
constexpr std::size_t kControlTopOffset = 0x44;
constexpr std::size_t kControlWidthOffset = 0x48;
constexpr std::size_t kControlHeightOffset = 0x4c;
constexpr std::size_t kModificationFolderOffset = 0x0c;
constexpr std::size_t kModificationTitleOffset = 0x10;

constexpr int kModuleListId = 0x6301;
constexpr int kSaveButtonId = 0x6302;
constexpr int kCancelButtonId = 0x6303;

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
const std::uint8_t kExpectedAssignButtonImagesLargeCenter[] = {
    0x55, 0x8b, 0xec, 0x33, 0xc9, 0x51, 0x51, 0x51};
const std::uint8_t kExpectedControlSetParent[] = {
    0x53, 0x56, 0x8b, 0xf2, 0x8b, 0xd8};
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
InlineHook g_form_show_hook;
InlineHook g_launch_hook;
void* g_modules_button = nullptr;
void* g_modules_button_form = nullptr;
HWND g_modules_button_owner = nullptr;

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
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
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
    modification = nullptr;
    void* grid = field<void*>(form, kGridOffset);
    if (!grid || !readable_range(grid, sizeof(void*))) return false;
    auto** vtable = *reinterpret_cast<void***>(grid);
    if (!vtable || !readable_range(vtable + 0x148 / sizeof(void*),
                                   sizeof(void*))) {
        return false;
    }
    void* get_selected_index = vtable[0x148 / sizeof(void*)];
    const auto raw_index = reinterpret_cast<std::intptr_t>(
        a2fo_call_delphi_one_register(get_selected_index, grid));
    if (raw_index < 0 || raw_index > 100000) return false;
    void* row = a2fo_call_delphi_two_registers(
        at(kNextGridGetRowRva), grid,
        reinterpret_cast<void*>(raw_index));
    if (!row || !readable_range(static_cast<std::uint8_t*>(row) + 8,
                                sizeof(void*))) {
        return false;
    }
    // Fleet Ops stores nullptr in the synthetic first row representing Data;
    // real mod rows carry their TModificationInfo pointer here.
    modification = field<void*>(row, 8);
    return true;
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
    if (!selected_modification(form, selected.modification)) {
        error = "No mod is selected.";
        return false;
    }
    selected.base = selected.modification == nullptr;
    if (selected.base) {
        selected.title = "Fleet Operations";
        selected.root = g_data_root;
        selected.roots.push_back(g_data_root);
    } else {
        selected.folder = delphi_string_field(selected.modification,
                                               kModificationFolderOffset);
        selected.title = delphi_string_field(selected.modification,
                                              kModificationTitleOffset);
        if (selected.title.empty()) selected.title = selected.folder;
        if (!safe_mod_directory_name(selected.folder)) {
            error = "The selected mod has an unsafe or unreadable folder name.";
            return false;
        }
        const ExtensionRootDiscovery discovery = discover_extension_roots(
            g_data_root, "/mod \"" + selected.folder + "\"");
        selected.roots = discovery.roots;
        selected.diagnostics = discovery.diagnostics;
        selected.root = join_path(join_path(g_data_root, "Mods"),
                                  selected.folder);
        if (selected.roots.empty() ||
            _stricmp(selected.roots.back().c_str(), selected.root.c_str()) != 0) {
            error = "The selected mod directory could not be resolved.";
            return false;
        }
    }
    selected.info_path = join_path(selected.root, "info.ini");
    return true;
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
    return state == ModulePolicyState::required ||
           state == ModulePolicyState::rejected ||
           state == ModulePolicyState::conflict ||
           state == ModulePolicyState::missing_active ||
           state == ModulePolicyState::missing_required;
}

bool state_checked(ModulePolicyState state) {
    return state == ModulePolicyState::active ||
           state == ModulePolicyState::required;
}

struct ModuleDialogContext {
    HWND owner = nullptr;
    HWND window = nullptr;
    HWND list = nullptr;
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

void populate_module_list(ModuleDialogContext& context) {
    LVCOLUMNA column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<char*>("Module");
    column.cx = 315;
    ListView_InsertColumn(context.list, 0, &column);
    column.pszText = const_cast<char*>("State");
    column.cx = 210;
    column.iSubItem = 1;
    ListView_InsertColumn(context.list, 1, &column);

    for (std::size_t index = 0; index < context.policy.entries.size(); ++index) {
        const ModulePolicyEntry& entry = context.policy.entries[index];
        LVITEMA item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<char*>(entry.name.c_str());
        item.lParam = static_cast<LPARAM>(index);
        ListView_InsertItem(context.list, &item);
        ListView_SetItemText(context.list, static_cast<int>(index), 1,
                             const_cast<char*>(state_text(entry.state)));
        ListView_SetCheckState(context.list, static_cast<int>(index),
                               state_checked(entry.state));
    }
    context.initializing = false;
}

bool save_dialog_selection(ModuleDialogContext& context) {
    std::vector<std::string> active;
    for (std::size_t index = 0; index < context.policy.entries.size(); ++index) {
        const ModulePolicyEntry& entry = context.policy.entries[index];
        if (!entry.installed || state_locked(entry.state) ||
            !ListView_GetCheckState(context.list, static_cast<int>(index))) {
            continue;
        }
        active.push_back(entry.name);
    }
    std::string error;
    if (!save_active_module_selection(context.selected.info_path, active,
                                      error)) {
        MessageBoxA(context.window, error.c_str(), "Modules",
                    MB_OK | MB_ICONERROR);
        return false;
    }
    context.saved = true;
    log("Module menu: saved selection for " + context.selected.title);
    return true;
}

LRESULT handle_list_notification(ModuleDialogContext& context,
                                 NMHDR* header) {
    if (!header || header->hwndFrom != context.list) return 0;
    if (header->code == LVN_ITEMCHANGING && !context.initializing) {
        auto* change = reinterpret_cast<NMLISTVIEW*>(header);
        if (change->iItem >= 0 &&
            static_cast<std::size_t>(change->iItem) <
                context.policy.entries.size()) {
            const UINT old_image = change->uOldState & LVIS_STATEIMAGEMASK;
            const UINT new_image = change->uNewState & LVIS_STATEIMAGEMASK;
            if (old_image != new_image &&
                state_locked(context.policy.entries[change->iItem].state)) {
                return TRUE;
            }
        }
    } else if (header->code == NM_CUSTOMDRAW) {
        auto* custom = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
        if (custom->nmcd.dwDrawStage == CDDS_PREPAINT) {
            return CDRF_NOTIFYITEMDRAW;
        }
        if (custom->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            const std::size_t index = custom->nmcd.dwItemSpec;
            if (index < context.policy.entries.size()) {
                const ModulePolicyState state =
                    context.policy.entries[index].state;
                if (state == ModulePolicyState::conflict ||
                    state == ModulePolicyState::missing_required) {
                    custom->clrText = RGB(255, 105, 105);
                } else if (state == ModulePolicyState::required) {
                    custom->clrText = RGB(145, 205, 255);
                } else if (state_locked(state)) {
                    custom->clrText = RGB(145, 145, 145);
                } else {
                    custom->clrText = RGB(235, 235, 235);
                }
                custom->clrTextBk = RGB(0, 0, 0);
            }
            return CDRF_NEWFONT;
        }
    }
    return 0;
}

LRESULT CALLBACK module_dialog_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam) {
    auto* context = reinterpret_cast<ModuleDialogContext*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(lparam);
        context = static_cast<ModuleDialogContext*>(create->lpCreateParams);
        SetWindowLongPtrA(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(context));
        context->window = window;
    }
    if (!context) return DefWindowProcA(window, message, wparam, lparam);

    switch (message) {
        case WM_CREATE: {
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
                "Changes are written to this mod's info.ini and apply on its next launch.",
                WS_CHILD | WS_VISIBLE, 18, 390, 554, 20,
                window, nullptr, nullptr, nullptr);
            set_control_font(label);
            HWND button = CreateWindowExA(
                0, "BUTTON", "Save",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                376, 423, 94, 26, window,
                reinterpret_cast<HMENU>(kSaveButtonId), nullptr, nullptr);
            set_control_font(button);
            button = CreateWindowExA(
                0, "BUTTON", "Cancel",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                478, 423, 94, 26, window,
                reinterpret_cast<HMENU>(kCancelButtonId), nullptr, nullptr);
            set_control_font(button);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == kSaveButtonId &&
                HIWORD(wparam) == BN_CLICKED) {
                if (save_dialog_selection(*context)) DestroyWindow(window);
                return 0;
            }
            if (LOWORD(wparam) == kCancelButtonId &&
                HIWORD(wparam) == BN_CLICKED) {
                DestroyWindow(window);
                return 0;
            }
            break;
        case WM_NOTIFY:
            return handle_list_notification(
                *context, reinterpret_cast<NMHDR*>(lparam));
        case WM_DRAWITEM:
            draw_dark_button(reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
            return TRUE;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, RGB(235, 235, 235));
            SetBkColor(dc, RGB(8, 8, 8));
            static HBRUSH background = CreateSolidBrush(RGB(8, 8, 8));
            return reinterpret_cast<LRESULT>(background);
        }
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            context->window = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcA(window, message, wparam, lparam);
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
    SelectedMod selected;
    std::string error;
    if (!resolve_selected_mod(form, selected, error)) {
        MessageBoxA(owner, error.c_str(), "Modules", MB_OK | MB_ICONERROR);
        return;
    }
    std::vector<std::string> diagnostics;
    const std::vector<InstalledModule> installed =
        discover_installed_modules(g_data_root, &diagnostics);
    ModuleDialogContext context;
    context.owner = owner;
    context.selected = std::move(selected);
    context.policy = evaluate_module_policy(context.selected.roots, installed);
    context.policy.diagnostics.insert(context.policy.diagnostics.end(),
                                      diagnostics.begin(), diagnostics.end());
    if (!register_dialog_class()) {
        MessageBoxA(owner, "The module-selection window could not be created.",
                    "Modules", MB_OK | MB_ICONERROR);
        return;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    RECT rectangle{0, 0, 592, 470};
    AdjustWindowRectEx(&rectangle, WS_CAPTION | WS_SYSMENU | WS_POPUP,
                       FALSE, WS_EX_DLGMODALFRAME);
    const std::string title = "Modules - " + context.selected.title;
    HWND window = CreateWindowExA(
        WS_EX_DLGMODALFRAME, kDialogClassName, title.c_str(),
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
        owner, nullptr,
        reinterpret_cast<HINSTANCE>(GetModuleHandleA("A2FOExtensions.dll")),
        &context);
    if (!window) {
        MessageBoxA(owner, "The module-selection window could not be created.",
                    "Modules", MB_OK | MB_ICONERROR);
        return;
    }
    centre_window(window, owner);
    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    while (context.window && GetMessageA(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
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
    HWND window = field<HWND>(form, kFormHandleOffset);
    void* launch = field<void*>(form, kLaunchButtonOffset);
    if (!IsWindow(window)) {
        log("Module menu: Mods form was shown without a valid window handle");
        return;
    }
    if (!launch || !readable_range(launch, kControlHeightOffset +
                                               sizeof(std::int32_t))) {
        log("Module menu: Mods form was shown without a readable Launch button");
        return;
    }
    const int launch_left = field<int>(launch, kControlLeftOffset);
    const int launch_top = field<int>(launch, kControlTopOffset);
    const int launch_width = field<int>(launch, kControlWidthOffset);
    const int launch_height = field<int>(launch, kControlHeightOffset);
    if (launch_left < 0 || launch_top < 0 || launch_width <= 0 ||
        launch_height <= 0 || launch_width > 4096 || launch_height > 4096) {
        log("Module menu: ignored invalid Launch button bounds " +
            std::to_string(launch_left) + "," +
            std::to_string(launch_top) + " " +
            std::to_string(launch_width) + "x" +
            std::to_string(launch_height));
        return;
    }

    const int gap = 18;
    const int button_left = std::max(8, launch_left - launch_width - gap);
    if (!g_modules_button || g_modules_button_form != form ||
        !readable_range(g_modules_button, kControlHeightOffset +
                                              sizeof(std::int32_t))) {
        void* jet_button_class = field<void*>(launch, 0);
        if (!jet_button_class || !readable_range(jet_button_class,
                                                 sizeof(void*))) {
            log("Module menu: Launch button class was unreadable");
            return;
        }
        void* button = a2fo_call_delphi_constructor(
            at(kJetButtonCreateRva), jet_button_class, form);
        if (!button || !readable_range(button, kControlHeightOffset +
                                                   sizeof(std::int32_t))) {
            log("Module menu: native TJetButton construction failed");
            return;
        }
        // TJetButton.Create deliberately constructs its TGraphicControl base
        // without an Owner. Register it with the form so normal VCL teardown
        // owns the lifetime, then attach it to the form's visual child list.
        a2fo_call_delphi_two_registers(
            at(kComponentInsertRva), form, button);
        a2fo_call_delphi_two_registers(
            at(kControlSetParentRva), button, form);
        g_modules_button = button;
        g_modules_button_form = form;
    }

    a2fo_call_delphi_set_bounds(
        at(kJetButtonSetBoundsRva), g_modules_button,
        button_left, launch_top, launch_width, launch_height);
    // Match FormShow's setup for each of the four stock bottom-row controls.
    // Style 8 assigns the large centred normal/hover/pressed button images.
    a2fo_call_delphi_two_registers(
        at(kAssignButtonImagesLargeCenterRva), g_modules_button,
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(8)));
    set_control_text(g_modules_button, "Modules");
    a2fo_call_delphi_one_register(
        at(kControlBringToFrontRva), g_modules_button);
    a2fo_call_delphi_one_register(
        at(kControlRepaintRva), g_modules_button);
    g_modules_button_owner = window;
    log("Module menu: native Modules TJetButton active at " +
        std::to_string(button_left) + "," + std::to_string(launch_top) +
        " " + std::to_string(launch_width) + "x" +
        std::to_string(launch_height));
}

}  // namespace

extern "C" void a2fo_mod_settings_form_show_hook_cpp(void* form) {
    a2fo_call_delphi_one_register(g_form_show_hook.gateway, form);
    try {
        ensure_modules_button(form);
    } catch (...) {
        log("Module menu: Mods form integration was skipped after an exception");
    }
}

extern "C" void a2fo_mod_settings_launch_hook_cpp(void* form, void* sender) {
    try {
        std::string error;
        if (!policy_allows_launch(form, error)) {
            HWND owner = field<HWND>(form, kFormHandleOffset);
            MessageBoxA(owner, error.c_str(), "Module requirements",
                        MB_OK | MB_ICONERROR);
            return;
        }
    } catch (...) {
        log("Module menu: launch validation failed unexpectedly");
        MessageBoxA(nullptr,
                    "Module requirements could not be validated. Launch was "
                    "cancelled for safety.",
                    "Module requirements", MB_OK | MB_ICONERROR);
        return;
    }
    a2fo_call_delphi_two_registers(g_launch_hook.gateway, form, sender);
}

extern "C" void a2fo_module_menu_jet_button_click_hook_cpp(void* button) {
    if (button == g_modules_button && g_modules_button_form) {
        try {
            show_module_dialog(g_modules_button_owner,
                               g_modules_button_form);
        } catch (...) {
            log("Module menu: native Modules button click failed unexpectedly");
        }
        return;
    }
    // TJetButton.Click is only a call-through to TControl.Click in this
    // binary. Preserve that exact behavior for every native button.
    a2fo_call_delphi_one_register(at(kControlClickRva), button);
}

bool install_module_menu(HMODULE fleet_ops, const std::string& data_root,
                         void (*log_line)(const std::string&)) {
    if (!fleet_ops || data_root.empty()) return false;
    g_fleet_ops = fleet_ops;
    g_data_root = data_root;
    g_log_line = log_line;
    if (std::memcmp(at(kModSettingsFormShowRva), kExpectedFormShow,
                    sizeof(kExpectedFormShow)) != 0 ||
        std::memcmp(at(kModSettingsLaunchRva), kExpectedLaunch,
                    sizeof(kExpectedLaunch)) != 0 ||
        std::memcmp(at(kJetButtonCreateRva), kExpectedJetButtonCreate,
                    sizeof(kExpectedJetButtonCreate)) != 0 ||
        std::memcmp(at(kJetButtonClickRva), kExpectedJetButtonClick,
                    sizeof(kExpectedJetButtonClick)) != 0 ||
        std::memcmp(at(kJetButtonSetBoundsRva), kExpectedJetButtonSetBounds,
                    sizeof(kExpectedJetButtonSetBounds)) != 0 ||
        std::memcmp(at(kAssignButtonImagesLargeCenterRva),
                    kExpectedAssignButtonImagesLargeCenter,
                    sizeof(kExpectedAssignButtonImagesLargeCenter)) != 0 ||
        std::memcmp(at(kControlSetParentRva), kExpectedControlSetParent,
                    sizeof(kExpectedControlSetParent)) != 0 ||
        std::memcmp(at(kControlSetTextRva), kExpectedControlSetText,
                    sizeof(kExpectedControlSetText)) != 0 ||
        std::memcmp(at(kControlClickRva), kExpectedControlClick,
                    sizeof(kExpectedControlClick)) != 0 ||
        std::memcmp(at(kControlBringToFrontRva),
                    kExpectedControlBringToFront,
                    sizeof(kExpectedControlBringToFront)) != 0 ||
        std::memcmp(at(kControlRepaintRva), kExpectedControlRepaint,
                    sizeof(kExpectedControlRepaint)) != 0 ||
        std::memcmp(at(kComponentInsertRva), kExpectedComponentInsert,
                    sizeof(kExpectedComponentInsert)) != 0 ||
        std::memcmp(at(kLongStringClearRva), kExpectedLongStringClear,
                    sizeof(kExpectedLongStringClear)) != 0 ||
        std::memcmp(at(kLongStringFromPcharRva),
                    kExpectedLongStringFromPchar,
                    sizeof(kExpectedLongStringFromPchar)) != 0) {
        log("Module menu: FleetOps Mods-screen signatures did not match");
        return false;
    }
    const bool form_hook = install_inline_hook(
        at(kModSettingsFormShowRva),
        reinterpret_cast<void*>(&a2fo_mod_settings_form_show_bridge),
        sizeof(kExpectedFormShow), kExpectedFormShow, g_form_show_hook);
    const bool launch_hook = install_inline_hook(
        at(kModSettingsLaunchRva),
        reinterpret_cast<void*>(&a2fo_mod_settings_launch_bridge),
        sizeof(kExpectedLaunch), kExpectedLaunch, g_launch_hook);
    const bool click_hook = patch_jump(
        at(kJetButtonClickRva),
        reinterpret_cast<void*>(&a2fo_module_menu_jet_button_click_bridge),
        kExpectedJetButtonClick, sizeof(kExpectedJetButtonClick));
    if (!form_hook || !launch_hook || !click_hook) {
        log("Module menu: FleetOps Mods-screen hooks did not match");
        return false;
    }
    log("Module menu: Mods-screen selector and launch validation enabled");
    return true;
}

}  // namespace a2fo
