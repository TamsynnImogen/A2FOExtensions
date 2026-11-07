/*
 * File: core/renderer_options.cpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Restart-applied renderer and monitor selection on Graphics Options.
 */

#include "renderer_options.hpp"

#include "build_identity.hpp"
#include "game_monitor.hpp"
#include "../rust/native_hooks_ffi.h"
#include "../rust/stage8_b_renderer_options_ffi.h"
#include "../rust/stage9_renderer_options_ffi.h"

#include <windows.h>

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace a2fo {
namespace {

// uGraphicOptions.TGraphicOptionsForm.FormShow in the supported
// FleetOpsHook.dll. The RVA includes the PE .text virtual address.
constexpr std::uintptr_t kGraphicsOptionsFormShowRva = 0x1bd00c;
constexpr std::uintptr_t kCustomLabelCreateRva = 0x09d1e8;
constexpr std::uintptr_t kCustomLabelSetTransparentRva = 0x09db4c;
constexpr std::uintptr_t kStringsAddRva = 0x081b30;
constexpr std::uintptr_t kFontSetColorRva = 0x08e1ec;
// The Graphics form uses TJvHTComboBox, not a plain TCustomComboBox. Calling
// only the inherited VCL constructor leaves the JvEx/JvHT state uninitialised
// and its first SetBounds reaches WM_WINDOWPOSCHANGED with a malformed message.
constexpr std::uintptr_t kJvCustomHTComboBoxCreateRva = 0x16cdec;
constexpr std::uintptr_t kJvgCheckBoxCreateRva = 0x165e34;
constexpr std::uintptr_t kJvgCheckBoxSetCheckedRva = 0x166d0c;
constexpr std::uintptr_t kAssignJvgCheckBoxImagesRva = 0x1786d0;
constexpr std::uintptr_t kCustomComboSetItemIndexRva = 0x09ee24;
constexpr std::uintptr_t kCustomComboBoxSetStyleRva = 0x09fd94;
constexpr std::uintptr_t kCustomComboSelectRva = 0x09f9b4;
constexpr std::uintptr_t kControlSetParentRva = 0x0c55f0;
constexpr std::uintptr_t kControlSetFontRva = 0x0c5810;
constexpr std::uintptr_t kControlSetParentFontRva = 0x0c5848;
// Controls.TWinControl.GetHandle. Unlike reading a field directly, this asks
// VCL to create the native child window if the control is still lazy-created.
constexpr std::uintptr_t kWinControlGetHandleRva = 0x0cd81c;
constexpr std::uintptr_t kControlSetVisibleRva = 0x0c563c;
constexpr std::uintptr_t kControlSetBoundsRva = 0x0c4b80;
constexpr std::uintptr_t kControlSetTextRva = 0x0c574c;
constexpr std::uintptr_t kControlBringToFrontRva = 0x0c59ac;
constexpr std::uintptr_t kControlRepaintRva = 0x0c5d30;
constexpr std::uintptr_t kComponentInsertRva = 0x00088274;
constexpr std::uintptr_t kLongStringClearRva = 0x000056b8;
constexpr std::uintptr_t kLongStringFromPcharRva = 0x000058b0;
// TWinControl.GetHandle returns [self+0x1b4] in both the clean and enlarged
// Roots menus. +0x1c4 is a different form field, not its HWND.
constexpr std::size_t kFormHandleOffset = 0x1b4;
constexpr std::size_t kPrimaryDeviceComboOffset = 0x370;
// uGraphicOptions.TGraphicOptionsForm.BumpMappingCheck. FormShow reads this
// exact field before applying the shared checkbox images and translated
// bump_mapping caption. Using the named form field avoids confusing adjacent
// streamed PNG/image objects for controls while still accepting Fleet Ops'
// runtime checkbox subclass.
constexpr std::size_t kBumpMappingCheckOffset = 0x3a0;
constexpr std::size_t kFirstNativeLabelOffset = 0x3d4;
constexpr std::size_t kComboItemsOffset = 0x284;
constexpr std::size_t kComboOnChangeCodeOffset = 0x268;
constexpr std::size_t kComboOnChangeDataOffset = 0x26c;
constexpr std::size_t kComponentOwnerOffset = 0x04;
constexpr std::size_t kControlParentOffset = 0x30;
constexpr std::size_t kControlLeftOffset = 0x40;
constexpr std::size_t kControlTopOffset = 0x44;
constexpr std::size_t kControlWidthOffset = 0x48;
constexpr std::size_t kControlHeightOffset = 0x4c;
constexpr std::size_t kControlFontOffset = 0x68;
constexpr std::size_t kJvgCheckBoxCheckedOffset = 0x1c8;
constexpr std::size_t kJvgCheckBoxOptionsOffset = 0x1f0;
constexpr int kNativeBumpLeft = 660;
constexpr int kNativeBumpTop = 432;
constexpr int kNativeBumpWidth = 297;
constexpr int kNativeBumpHeight = 17;
constexpr int kEffectRowGap = 8;
constexpr int kEffectWidth = 142;
constexpr int kEffectGap = 8;
constexpr UINT_PTR kMonitorSelectionTimerId = 0xa2f04d4fu;
constexpr UINT kMonitorSelectionTimerIntervalMs = 100u;

const std::uint8_t kExpectedFormShow[] = {
    0x55, 0x8b, 0xec, 0xb9, 0x09, 0x00, 0x00, 0x00};
const std::uint8_t kExpectedCustomLabelCreate[] = {
    0x53, 0x56, 0x84, 0xd2, 0x74, 0x08};
const std::uint8_t kExpectedCustomLabelSetTransparent[] = {
    0x53, 0x56, 0x8b, 0xda, 0x8b, 0xf0};
const std::uint8_t kExpectedStringsAdd[] = {
    0x53, 0x56, 0x57, 0x8b, 0xfa, 0x8b, 0xd8};
const std::uint8_t kExpectedFontSetColor[] = {
    0x56, 0x3b, 0x50, 0x18, 0x74, 0x0c};
const std::uint8_t kExpectedJvCustomHTComboBoxCreate[] = {
    0x53, 0x56, 0x84, 0xd2, 0x74, 0x08};
const std::uint8_t kExpectedJvgCheckBoxCreate[] = {
    0x53, 0x56, 0x84, 0xd2, 0x74, 0x08};
const std::uint8_t kExpectedJvgCheckBoxSetChecked[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf0};
const std::uint8_t kExpectedAssignJvgCheckBoxImages[] = {
    0xb9, 0xe8, 0x86, 0x97, 0x5a,
    0xba, 0x10, 0x87, 0x97, 0x5a};
const std::uint8_t kExpectedCustomComboSetItemIndex[] = {
    0x53, 0x56, 0x8b, 0xf2, 0x8b, 0xd8};
const std::uint8_t kExpectedCustomComboBoxSetStyle[] = {
    0x53, 0x8b, 0xd8, 0x3a, 0x93, 0xbe, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedCustomComboSelect[] = {
    0x53, 0x56, 0x66, 0x83, 0xb8, 0x6a, 0x02, 0x00, 0x00, 0x00};
const std::uint8_t kExpectedControlSetParent[] = {
    0x53, 0x56, 0x8b, 0xf2, 0x8b, 0xd8};
const std::uint8_t kExpectedControlSetFont[] = {
    0x56, 0x8b, 0xf0, 0x8b, 0x46, 0x68};
const std::uint8_t kExpectedControlSetParentFont[] = {
    0x3a, 0x50, 0x59, 0x74, 0x1d};
const std::uint8_t kExpectedWinControlGetHandle[] = {
    0x53, 0x8b, 0xd8, 0x8b, 0xc3, 0xe8, 0xd2, 0xff, 0xff, 0xff,
    0x8b, 0x83, 0xb4, 0x01, 0x00, 0x00, 0x5b, 0xc3};
const std::uint8_t kExpectedControlSetVisible[] = {
    0x53, 0x56, 0x57, 0x8b, 0xda, 0x8b, 0xf8};
const std::uint8_t kExpectedControlSetBounds[] = {
    0x55, 0x8b, 0xec, 0x53, 0x56, 0x57,
    0x8b, 0xf9, 0x8b, 0xf2, 0x8b, 0xd8};
const std::uint8_t kExpectedControlSetText[] = {
    0x55, 0x8b, 0xec, 0x6a, 0x00};
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

enum class RendererBackend {
    system,
    dxvk,
};

HMODULE g_fleet_ops = nullptr;
std::string g_data_root;
void (*g_log_line)(const std::string&) = nullptr;
A2foRsInlineHook g_form_show_hook{};
A2foRsInlineHook g_jvg_checkbox_set_checked_hook{};

HWND g_form_window = nullptr;
void* g_renderer_label = nullptr;
void* g_renderer_combo_control = nullptr;
HWND g_renderer_combo_window = nullptr;
void* g_renderer_control_form = nullptr;
void* g_monitor_label = nullptr;
void* g_monitor_combo_control = nullptr;
HWND g_monitor_combo_window = nullptr;
void* g_monitor_control_form = nullptr;
std::vector<GameMonitorDescriptor> g_game_monitors;
void* g_emissive_checkbox = nullptr;
void* g_specular_checkbox = nullptr;
void* g_effect_checkbox_form = nullptr;
void* g_restart_label = nullptr;
WNDPROC g_original_form_proc = nullptr;
bool g_helper_scheduled = false;
volatile LONG g_emissive_maps_enabled = -1;
volatile LONG g_specular_maps_enabled = -1;
volatile LONG g_syncing_effect_checkboxes = 0;
volatile LONG g_syncing_renderer_combo = 0;
volatile LONG g_syncing_monitor_combo = 0;
int g_observed_monitor_selection = -1;
bool g_monitor_selection_timer_active = false;

extern "C" void* a2fo_call_delphi_one_register(
    void* function, void* eax_argument);
extern "C" void* a2fo_call_delphi_two_registers(
    void* function, void* eax_argument, void* edx_argument);
extern "C" void* a2fo_call_delphi_constructor(
    void* function, void* class_reference, void* owner);
extern "C" void a2fo_call_delphi_set_bounds(
    void* function, void* control, int left, int top, int width, int height);
extern "C" void a2fo_graphics_options_form_show_bridge();
extern "C" void a2fo_jvg_checkbox_set_checked_bridge();
extern "C" void a2fo_renderer_combo_change_bridge();

std::string join_path(const std::string& left, const std::string& right);

void log(const std::string& line) {
    if (g_log_line) g_log_line(line);
}

void append_renderer_audit(const std::string& text) {
    if (g_data_root.empty()) return;
    const std::string path = join_path(g_data_root, "A2FORenderer.log");
    HANDLE file = CreateFileA(
        path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char prefix[96]{};
    std::snprintf(
        prefix, sizeof(prefix),
        "%04u-%02u-%02u %02u:%02u:%02u A2FOExtensions %s: ",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
        now.wSecond, A2FO_BUILD_ID);
    const std::string output = std::string(prefix) + text + "\r\n";
    DWORD written = 0;
    WriteFile(file, output.data(), static_cast<DWORD>(output.size()),
              &written, nullptr);
    CloseHandle(file);
}

void* at(std::uintptr_t rva) {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_fleet_ops) + rva);
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

void set_control_text(void* control, const char* text) {
    if (!control || !text) return;
    char* delphi_text = nullptr;
    a2fo_call_delphi_two_registers(
        at(kLongStringFromPcharRva), &delphi_text,
        const_cast<char*>(text));
    a2fo_call_delphi_two_registers(
        at(kControlSetTextRva), control, delphi_text);
    a2fo_call_delphi_one_register(at(kLongStringClearRva), &delphi_text);
}

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
    // These are newly constructed, unparented controls with no HWND. Seed the
    // standard TControl L/T/W/H fields directly so SetParent creates the child
    // at its final rectangle. Calling this VCL generation's SetBounds on the
    // live TJvHTComboBox produces a synthetic WM_WINDOWPOSCHANGED with a null
    // WINDOWPOS and crashes inside TWinControl.WMWindowPosChanged.
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

void set_combo_index(int index) {
    if (!g_renderer_combo_control) return;
    InterlockedIncrement(&g_syncing_renderer_combo);
    a2fo_call_delphi_two_registers(
        at(kCustomComboSetItemIndexRva), g_renderer_combo_control,
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(index)));
    // Keep the native selection in sync even if VCL still considers the
    // dynamically inserted component to be in its loading phase.
    if (g_renderer_combo_window && IsWindow(g_renderer_combo_window)) {
        SendMessageA(g_renderer_combo_window, CB_SETCURSEL, index, 0);
    }
    InterlockedDecrement(&g_syncing_renderer_combo);
}

std::string join_path(const std::string& left, const std::string& right) {
    struct Context { const std::string& left; const std::string& right; } context{left, right};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uint32_t) noexcept -> std::uint64_t {
        const auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
            case 0: return state.left.empty();
            case 1: return state.right.empty();
            case 2: return state.left.back() == '\\' || state.left.back() == '/';
            default: return 0;
        }
    };
    switch (a2fo_rs_stage9_renderer_join(&context, step)) {
        case 0: return right;
        case 1: return left;
        case 2: return left + right;
        default: return left + "\\" + right;
    }
}

std::string renderer_ini_path() {
    return join_path(g_data_root, "A2FORenderer.ini");
}

bool read_effect_enabled(const char* key, volatile LONG* cached) noexcept {
    struct Context {const char* key;volatile LONG* cached;std::exception_ptr error{};} c{key,cached};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 700:return c.key!=nullptr;
            case 701:return c.cached!=nullptr;
            case 702:return static_cast<std::uint32_t>(InterlockedCompareExchange(c.cached,0,0));
            case 703:return g_data_root.empty();
            case 704:return GetPrivateProfileIntA("Effects",c.key,1,renderer_ini_path().c_str())!=0;
            case 705:InterlockedCompareExchange(c.cached,static_cast<LONG>(arg),-1);return 0;
            case 706:return static_cast<std::uint32_t>(InterlockedCompareExchange(c.cached,0,0));
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_renderer_read_effect(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

bool write_effect_enabled(const char* key, bool enabled) noexcept {
    struct Context {const char* key;bool enabled;std::exception_ptr failure{};}
        context{key,enabled};
    const auto step=[](void* opaque,std::uint32_t operation,
                       std::uint32_t) noexcept -> std::uint64_t {
        auto& state=*static_cast<Context*>(opaque);
        try {
            switch(operation) {
            case 0:return state.key!=nullptr;
            case 1:return !g_data_root.empty();
            case 2:return WritePrivateProfileStringA("Effects",state.key,
                state.enabled?"1":"0",renderer_ini_path().c_str())!=FALSE;
            default:return 0;
            }
        }catch(...){state.failure=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage9_renderer_write_effect(&context,step);
    if(context.failure)std::rethrow_exception(context.failure);
    return result;
}

void set_effect_checkbox_state(void* checkbox, bool enabled) noexcept {
    if (!checkbox || !g_jvg_checkbox_set_checked_hook.gateway) return;
    InterlockedIncrement(&g_syncing_effect_checkboxes);
    a2fo_call_delphi_two_registers(
        at(kJvgCheckBoxSetCheckedRva), checkbox,
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(enabled)));
    InterlockedDecrement(&g_syncing_effect_checkboxes);
}

void save_effect_checkbox(void* checkbox, const char* key, const char* label,
                          volatile LONG* cached) noexcept {
    struct Context {void* checkbox;const char* key;const char* label;volatile LONG* cached;
        bool enabled{};std::exception_ptr error{};} c{checkbox,key,label,cached};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 720:return c.checkbox!=nullptr;
            case 721:return c.key!=nullptr;
            case 722:return c.label!=nullptr;
            case 723:return c.cached!=nullptr;
            case 724:return readable_range(c.checkbox,kJvgCheckBoxCheckedOffset+1);
            case 725:c.enabled=field<std::uint8_t>(c.checkbox,kJvgCheckBoxCheckedOffset)!=0;return 0;
            case 726:return write_effect_enabled(c.key,c.enabled);
            case 727:set_effect_checkbox_state(c.checkbox,read_effect_enabled(c.key,c.cached));return 0;
            case 728:log(std::string("Renderer options: could not save ")+c.label+" map setting");return 0;
            case 730:InterlockedExchange(c.cached,c.enabled?1:0);return 0;
            case 731:log(std::string("Renderer options: ")+c.label+" maps "+
                         (c.enabled?"enabled":"disabled"));return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_save_effect(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

std::string renderer_helper_path() {
    return join_path(g_data_root, "A2FORendererHelper.exe");
}

std::string active_renderer_path() {
    return join_path(g_data_root, "d3d9.dll");
}

std::string dxvk_payload_path() {
    return join_path(g_data_root, "renderers\\dxvk\\d3d9.dll");
}

bool file_exists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return a2fo_rs_stage9_renderer_file_exists(attributes);
}

bool same_file_contents(const std::string& left,
                        const std::string& right) {
    struct Context {
        const std::string& left;
        const std::string& right;
        HANDLE left_file{INVALID_HANDLE_VALUE};
        HANDLE right_file{INVALID_HANDLE_VALUE};
        LARGE_INTEGER left_size{};
        LARGE_INTEGER right_size{};
        std::array<std::uint8_t, 16 * 1024> left_buffer{};
        std::array<std::uint8_t, 16 * 1024> right_buffer{};
        DWORD left_read{};
        DWORD right_read{};
    } context{left, right};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uint32_t) noexcept -> std::uint64_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
            case 0:
                state.left_file = CreateFileA(state.left.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                return state.left_file != INVALID_HANDLE_VALUE;
            case 1:
                state.right_file = CreateFileA(state.right.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                return state.right_file != INVALID_HANDLE_VALUE;
            case 2: CloseHandle(state.left_file); return 0;
            case 3: return GetFileSizeEx(state.left_file, &state.left_size) != FALSE;
            case 4: return GetFileSizeEx(state.right_file, &state.right_size) != FALSE;
            case 5: return state.left_size.QuadPart == state.right_size.QuadPart;
            case 6:
                state.left_read = 0;
                state.right_read = 0;
                return ReadFile(state.left_file, state.left_buffer.data(),
                    state.left_buffer.size(), &state.left_read, nullptr) != FALSE;
            case 7:
                return ReadFile(state.right_file, state.right_buffer.data(),
                    state.right_buffer.size(), &state.right_read, nullptr) != FALSE;
            case 8: return state.left_read == state.right_read;
            case 9: return std::memcmp(state.left_buffer.data(),
                state.right_buffer.data(), state.left_read) == 0;
            case 10: return state.left_read == 0;
            case 11: CloseHandle(state.right_file); return 0;
            default: return 0;
        }
    };
    return a2fo_rs_stage9_renderer_same_file(&context, step);
}

bool ensure_directory(const std::string& path) {
    struct Context {
        const std::string& path;
        DWORD attributes{INVALID_FILE_ATTRIBUTES};
        std::size_t slash{std::string::npos};
        std::exception_ptr failure{};
    } context{path};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uint32_t) noexcept -> std::uint64_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            switch (operation) {
                case 0: return state.path.empty();
                case 1: state.attributes = GetFileAttributesA(state.path.c_str()); return 0;
                case 2: return state.attributes != INVALID_FILE_ATTRIBUTES;
                case 3: return (state.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                case 4:
                    state.slash = state.path.find_last_of("\\/");
                    return state.slash != std::string::npos && state.slash > 0;
                case 5: return ensure_directory(state.path.substr(0, state.slash));
                case 6: return CreateDirectoryA(state.path.c_str(), nullptr) != FALSE;
                case 7: return GetLastError() == ERROR_ALREADY_EXISTS;
                default: return 0;
            }
        } catch (...) {
            state.failure = std::current_exception();
            return UINT64_MAX;
        }
    };
    const bool result = a2fo_rs_stage9_renderer_ensure_directory(&context, step);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
}

RendererBackend backend_from_text(const char* text) {
    struct Context {const char* text;} context{text};
    const auto step=[](void* opaque,std::uint32_t operation,
                       std::uint32_t) noexcept -> std::uint64_t {
        const auto& state=*static_cast<Context*>(opaque);
        if(operation==0)return state.text!=nullptr;
        return _stricmp(state.text,"dxvk")==0;
    };
    return static_cast<RendererBackend>(
        a2fo_rs_stage9_renderer_backend_from_text(&context,step));
}

const char* backend_text(RendererBackend backend) {
    return a2fo_rs_stage9_renderer_backend_label(
        static_cast<std::uint32_t>(backend)) ? "dxvk" : "system";
}

RendererBackend read_backend(const char* key, RendererBackend fallback) {
    char value[32]{};
    GetPrivateProfileStringA("Renderer", key, backend_text(fallback), value,
                             sizeof(value), renderer_ini_path().c_str());
    return backend_from_text(value);
}

RendererBackend requested_backend() {
    struct Context {
        RendererBackend applied{RendererBackend::system};
        std::exception_ptr failure{};
    } context;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uint32_t) noexcept -> std::uint64_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            if (operation == 0) {
                state.applied = read_backend("AppliedBackend", RendererBackend::system);
                return static_cast<std::uint32_t>(state.applied);
            }
            return static_cast<std::uint32_t>(read_backend("Backend", state.applied));
        } catch (...) {
            state.failure = std::current_exception();
            return UINT64_MAX;
        }
    };
    const auto result = a2fo_rs_stage9_renderer_requested(&context, step);
    if (context.failure) std::rethrow_exception(context.failure);
    return static_cast<RendererBackend>(result);
}

RendererBackend applied_backend() {
    // Absence of AppliedBackend means the helper has not confirmed a managed
    // renderer yet. Do not assume that a saved DXVK request was applied.
    return read_backend("AppliedBackend", RendererBackend::system);
}

bool write_backend(RendererBackend backend) {
    return WritePrivateProfileStringA("Renderer", "Backend",
                                      backend_text(backend),
                                      renderer_ini_path().c_str()) != FALSE;
}

void reconcile_applied_backend_with_files() {
    struct Context { std::exception_ptr error{}; } context;
    const auto op = [](void* opaque, std::uint32_t id,
                       std::uint32_t arg) noexcept -> std::uint64_t {
        auto& context = *static_cast<Context*>(opaque);
        try {
            const auto detected = arg ? RendererBackend::dxvk : RendererBackend::system;
            switch (id) {
                case 0: return file_exists(active_renderer_path());
                case 1: return file_exists(dxvk_payload_path());
                case 2: return same_file_contents(active_renderer_path(), dxvk_payload_path());
                case 3: log("Renderer options: Data\\d3d9.dll is not the managed DXVK "
                            "payload; helper state was left unchanged"); return 0;
                case 4: return static_cast<std::uint32_t>(applied_backend());
                case 5: return WritePrivateProfileStringA(
                    "Renderer", "AppliedBackend", backend_text(detected),
                    renderer_ini_path().c_str()) != FALSE;
                case 6: log("Renderer options: could not reconcile AppliedBackend with the "
                            "active renderer file"); return 0;
                case 7: log(std::string("Renderer options: reconciled helper state to active ") +
                            backend_text(detected) + " renderer file"); return 0;
                default: return 0;
            }
        } catch (...) { context.error = std::current_exception(); return UINT64_MAX; }
    };
    a2fo_rs_stage8_b_renderer_reconcile(&context, op);
    if (context.error) std::rethrow_exception(context.error);
}

std::string read_last_error() {
    char value[256]{};
    GetPrivateProfileStringA("Renderer", "LastError", "", value,
                             sizeof(value), renderer_ini_path().c_str());
    return value;
}

bool preserve_active_dxvk_payload() {
    struct Context {std::string directory;std::exception_ptr error{};} c;
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 740:return file_exists(dxvk_payload_path());
            case 741:return file_exists(active_renderer_path());
            case 742:c.directory=join_path(g_data_root,"renderers\\dxvk");return 0;
            case 743:return ensure_directory(c.directory);
            case 744:return CopyFileA(active_renderer_path().c_str(),dxvk_payload_path().c_str(),TRUE)!=FALSE;
            case 745:return GetLastError()==ERROR_FILE_EXISTS;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_renderer_preserve_payload(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

void set_restart_text(const std::string& text) {
    struct Context {const std::string& text;std::exception_ptr failure{};}
        context{text};
    const auto step=[](void* opaque,std::uint32_t operation,
                       std::uint32_t) noexcept -> std::uint64_t {
        auto& state=*static_cast<Context*>(opaque);
        try {
            switch(operation) {
            case 0:set_control_text(g_restart_label,state.text.c_str());return 0;
            case 1:return g_restart_label!=nullptr;
            case 2:a2fo_call_delphi_one_register(at(kControlRepaintRva),
                g_restart_label);return 0;
            default:return 0;
            }
        }catch(...){state.failure=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage9_renderer_restart_text(&context,step);
    if(context.failure)std::rethrow_exception(context.failure);
}

bool schedule_renderer_helper() {
    struct Context {
        std::string helper;
        std::vector<char> mutable_command;
        STARTUPINFOA startup{};
        PROCESS_INFORMATION process{};
        std::exception_ptr error{};
    } context;
    const auto op = [](void* opaque, std::uint32_t id,
                       std::uint32_t) noexcept -> std::uint64_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (id) {
                case 8: return g_helper_scheduled;
                case 9: c.helper = renderer_helper_path(); return 0;
                case 10: return file_exists(c.helper);
                case 11: log("Renderer options: A2FORendererHelper.exe is not installed"); return 0;
                case 12: {
                    std::string command = "\"" + c.helper + "\" --wait-pid " +
                        std::to_string(GetCurrentProcessId()) +
                        " --data-root \"" + g_data_root + "\"";
                    c.mutable_command.assign(command.begin(), command.end());
                    c.mutable_command.push_back('\0');
                    c.startup.cb = sizeof(c.startup);
                    return 0;
                }
                case 13: return CreateProcessA(c.helper.c_str(), c.mutable_command.data(),
                    nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                    g_data_root.c_str(), &c.startup, &c.process) != FALSE;
                case 14: log("Renderer options: could not start renderer helper (error " +
                             std::to_string(GetLastError()) + ")"); return 0;
                case 15: CloseHandle(c.process.hThread); CloseHandle(c.process.hProcess);
                    g_helper_scheduled = true; return 0;
                case 16: log("Renderer options: renderer helper scheduled for game exit"); return 0;
                default: return 0;
            }
        } catch (...) { c.error = std::current_exception(); return UINT64_MAX; }
    };
    const bool result = a2fo_rs_stage8_b_renderer_schedule(&context, op) != 0;
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

void select_renderer(RendererBackend backend) {
    struct Context {
        RendererBackend backend, previous{}, applied{};
        std::exception_ptr error{};
    } context{backend};
    const auto op = [](void* opaque, std::uint32_t id,
                       std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (id) {
                case 17: c.previous = requested_backend();
                    return static_cast<std::uint32_t>(c.previous);
                case 18: c.applied = applied_backend();
                    return static_cast<std::uint32_t>(c.applied);
                case 19: return read_last_error().empty();
                case 20: set_restart_text("Renderer is already selected for the next start."); return 0;
                case 21: return file_exists(dxvk_payload_path());
                case 22: return static_cast<std::uint32_t>(applied_backend());
                case 23: return preserve_active_dxvk_payload();
                case 24: set_combo_index(arg == 1 ? 1 : 0);
                    set_restart_text("DXVK is not installed in Data\\renderers\\dxvk.");
                    log("Renderer options: DXVK selection rejected because its payload "
                        "is missing"); return 0;
                case 25: set_combo_index(arg == 1 ? 1 : 0);
                    set_restart_text("Could not preserve DXVK; renderer was not changed.");
                    log("Renderer options: could not preserve the active DXVK payload"); return 0;
                case 26: return write_backend(arg == 1 ? RendererBackend::dxvk : RendererBackend::system);
                case 27: set_combo_index(arg == 1 ? 1 : 0);
                    set_restart_text("Could not save the renderer setting.");
                    log("Renderer options: A2FORenderer.ini could not be written"); return 0;
                case 28: WritePrivateProfileStringA("Renderer", "LastError", "",
                             renderer_ini_path().c_str()); return 0;
                case 29: return schedule_renderer_helper();
                case 30: set_restart_text(
                    "Saved, but the renderer helper is missing; restart change pending."); return 0;
                case 31: set_restart_text(
                    "Renderer saved. Fully exit and relaunch Fleet Operations to apply it."); return 0;
                case 32: log(std::string("Renderer options: requested backend ") +
                             backend_text(arg == 1 ? RendererBackend::dxvk : RendererBackend::system));
                    return 0;
                default: return 0;
            }
        } catch (...) { c.error = std::current_exception(); return UINT64_MAX; }
    };
    a2fo_rs_stage8_b_renderer_select(&context, op,
        static_cast<std::uint32_t>(backend));
    if (context.error) std::rethrow_exception(context.error);
}

void save_game_monitor_selection(int selected);

void observe_game_monitor_selection(const char* source) {
    struct Context {const char* source;LRESULT selected{};
        std::exception_ptr error{};explicit Context(const char* text):source(text){}} c(source);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 70:return InterlockedCompareExchange(&g_syncing_monitor_combo,0,0)==0;
            case 71:return g_monitor_combo_window!=nullptr;
            case 72:return IsWindow(g_monitor_combo_window)!=FALSE;
            case 73:c.selected=SendMessageA(g_monitor_combo_window,CB_GETCURSEL,0,0);
                return static_cast<std::uint32_t>(c.selected);
            case 74:return g_game_monitors.size();
            case 75:return static_cast<std::uint32_t>(g_observed_monitor_selection);
            case 76:g_observed_monitor_selection=static_cast<int>(c.selected);return 0;
            case 77:log(std::string("Renderer options: Game Monitor selection detected by ")+
                c.source+" (selected="+std::to_string(c.selected)+")");return 0;
            case 78:save_game_monitor_selection(static_cast<int>(c.selected));return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_observe_monitor(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

void ensure_game_monitor_selection_timer() {
    struct Context {std::exception_ptr error{};} c;
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 80:return g_monitor_selection_timer_active;
            case 81:return g_form_window!=nullptr;
            case 82:return IsWindow(g_form_window)!=FALSE;
            case 83:return SetTimer(g_form_window,kMonitorSelectionTimerId,
                kMonitorSelectionTimerIntervalMs,nullptr)!=0;
            case 84:log("Renderer options: Game Monitor selection timer could not be started");
                return 0;
            case 85:g_monitor_selection_timer_active=true;return 0;
            case 86:log("Renderer options: Game Monitor selection timer enabled");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_monitor_timer(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

LRESULT CALLBACK graphics_form_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
    struct Context {
        HWND window;UINT message;WPARAM wparam;LPARAM lparam;
        RendererBackend requested_before{RendererBackend::system};
        WNDPROC original{};LRESULT result{},renderer_selected{},monitor_selected{};
        std::string configured_device;
        std::exception_ptr error{};
        Context(HWND handle,UINT event,WPARAM w,LPARAM l)
            :window(handle),message(event),wparam(w),lparam(l){}
    } c(window,message,wparam,lparam);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 100:case 104:return c.message==WM_COMMAND;
            case 101:return reinterpret_cast<HWND>(c.lparam)==g_renderer_combo_window;
            case 102:case 106:return HIWORD(c.wparam)==CBN_SELCHANGE;
            case 103:return InterlockedCompareExchange(&g_syncing_renderer_combo,0,0)==0;
            case 105:return reinterpret_cast<HWND>(c.lparam)==g_monitor_combo_window;
            case 107:return InterlockedCompareExchange(&g_syncing_monitor_combo,0,0)==0;
            case 108:c.requested_before=requested_backend();return 0;
            case 109:c.original=g_original_form_proc;return 0;
            case 110:c.result=c.original?CallWindowProcA(c.original,c.window,c.message,
                c.wparam,c.lparam):DefWindowProcA(c.window,c.message,c.wparam,c.lparam);
                return 0;
            case 111:return g_renderer_combo_window!=nullptr;
            case 112:return IsWindow(g_renderer_combo_window)!=FALSE;
            case 113:c.renderer_selected=SendMessageA(g_renderer_combo_window,CB_GETCURSEL,0,0);
                return static_cast<std::uint32_t>(c.renderer_selected);
            case 114:return static_cast<std::uint32_t>(requested_backend());
            case 115:return static_cast<std::uint32_t>(c.requested_before);
            case 116:log("Renderer options: native Windows combo notification handled as OnChange fallback");return 0;
            case 117:select_renderer(c.renderer_selected==1?RendererBackend::dxvk:
                RendererBackend::system);return 0;
            case 120:return g_monitor_combo_window!=nullptr;
            case 121:return IsWindow(g_monitor_combo_window)!=FALSE;
            case 122:c.monitor_selected=SendMessageA(g_monitor_combo_window,CB_GETCURSEL,0,0);
                return static_cast<std::uint32_t>(c.monitor_selected);
            case 123:return g_game_monitors.size();
            case 124:return read_configured_game_monitor(g_data_root,c.configured_device);
            case 125:return _stricmp(c.configured_device.c_str(),
                g_game_monitors[c.monitor_selected].device_name.c_str())!=0;
            case 126:g_observed_monitor_selection=static_cast<int>(c.monitor_selected);return 0;
            case 127:log("Renderer options: native Windows Game Monitor combo notification handled as OnChange fallback");return 0;
            case 128:save_game_monitor_selection(static_cast<int>(c.monitor_selected));return 0;
            case 130:return c.message==WM_TIMER;
            case 131:return c.wparam==kMonitorSelectionTimerId;
            case 132:observe_game_monitor_selection("UI timer fallback");return 0;
            case 133:return c.message==WM_NCDESTROY;
            case 134:return c.window==g_form_window;
            case 135:log("Renderer options: Graphics Options window destroyed; retained form-owned VCL controls for handle recreation");return 0;
            case 136:KillTimer(c.window,kMonitorSelectionTimerId);return 0;
            case 137:g_monitor_selection_timer_active=false;
                g_observed_monitor_selection=-1;g_form_window=nullptr;
                g_renderer_combo_window=nullptr;g_monitor_combo_window=nullptr;
                g_original_form_proc=nullptr;return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_window(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return c.result;
}

void* create_vcl_control(void* form, void* exemplar,
                         std::uintptr_t constructor_rva);

bool is_owned_control_for_form(void* control, void* exemplar,
                               void* form) {
    if (!form || !readable_range(
            control, kControlParentOffset + sizeof(void*)) ||
        !readable_range(exemplar, sizeof(void*))) {
        return false;
    }
    return field<void*>(control, 0) == field<void*>(exemplar, 0) &&
        field<void*>(control, kComponentOwnerOffset) == form &&
        field<void*>(control, kControlParentOffset) == form;
}

void* find_native_bump_checkbox(void* form) {
    if (!readable_range(
            form, kBumpMappingCheckOffset + sizeof(void*))) {
        return nullptr;
    }
    void* candidate = field<void*>(form, kBumpMappingCheckOffset);
    if (!readable_range(
            candidate, kJvgCheckBoxOptionsOffset + sizeof(std::uint8_t))) {
        return nullptr;
    }
    // The streamed field identity is authoritative. Windows DPI/display
    // scaling can change its live bounds from the DFM values, so geometry is
    // unsuitable as a class guard here. create_vcl_control still validates
    // the exemplar VMT and the constructed control's resulting VMT.
    return candidate;
}

struct GraphicsOptionsLayout {
    int bump_left = kNativeBumpLeft;
    int bump_top = kNativeBumpTop;
    int checkbox_height = kNativeBumpHeight;

    int x(int reference) const {
        return a2fo_rs_stage8_b_renderer_scaled_coordinate(
            reference,bump_left,kNativeBumpLeft);
    }
    int y(int reference) const {
        return a2fo_rs_stage8_b_renderer_scaled_coordinate(
            reference,bump_top,kNativeBumpTop);
    }
};

GraphicsOptionsLayout graphics_options_layout(void* form) {
    GraphicsOptionsLayout layout;
    if (void* bump = find_native_bump_checkbox(form)) {
        const int left = field<int>(bump, kControlLeftOffset);
        const int top = field<int>(bump, kControlTopOffset);
        const int height = field<int>(bump, kControlHeightOffset);
        // The recovered clean form is 800x600, while Roots enlarges it to
        // 1200x900. Anchor to its native control's logical coordinates so the
        // extension works with either DFM and the shell canvas scales once.
        if (a2fo_rs_stage8_b_renderer_valid_bump_bounds(left,top,height)) {
            layout.bump_left = left;
            layout.bump_top = top;
            layout.checkbox_height = height;
        }
    }
    return layout;
}

void position_effect_checkboxes(void* bump_checkbox) {
    struct Context {void* bump;int left{};int top{};int height{};std::exception_ptr error{};} c{bump_checkbox};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 750:c.left=field<int>(c.bump,kControlLeftOffset);return static_cast<std::uint32_t>(c.left);
            case 751:c.top=field<int>(c.bump,kControlTopOffset);return static_cast<std::uint32_t>(c.top);
            case 752:c.height=field<int>(c.bump,kControlHeightOffset);return static_cast<std::uint32_t>(c.height);
            case 753:log("Renderer options: native Bump Mapping bounds were invalid");return 0;
            case 754:a2fo_call_delphi_set_bounds(at(kControlSetBoundsRva),g_emissive_checkbox,
                         c.left,static_cast<int>(arg),kEffectWidth,c.height);return 0;
            case 755:a2fo_call_delphi_set_bounds(at(kControlSetBoundsRva),g_specular_checkbox,
                         c.left+kEffectWidth+kEffectGap,static_cast<int>(arg),kEffectWidth,c.height);return 0;
            case 756:log("Renderer options: map-effect row positioned below native Bump Mapping "
                         "(bump top="+std::to_string(c.top)+", height="+std::to_string(c.height)+
                         "; emissive top="+std::to_string(field<int>(g_emissive_checkbox,kControlTopOffset))+
                         ", specular top="+std::to_string(field<int>(g_specular_checkbox,kControlTopOffset))+")");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_position_effect(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

void ensure_effect_checkboxes(void* form) {
    struct Context {
        void* form;void* bump{};GraphicsOptionsLayout layout{};
        std::exception_ptr error{};
        explicit Context(void* value):form(value){}
    } c(form);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 200:return c.form!=nullptr;
            case 201:c.bump=find_native_bump_checkbox(c.form);return c.bump!=nullptr;
            case 202:log("Renderer options: native Bump Mapping checkbox was not found");return 0;
            case 203:return g_effect_checkbox_form==c.form;
            case 204:return is_owned_control_for_form(g_emissive_checkbox,c.bump,c.form);
            case 205:return is_owned_control_for_form(g_specular_checkbox,c.bump,c.form);
            case 206:return readable_range(g_emissive_checkbox,kJvgCheckBoxCheckedOffset+1);
            case 207:return readable_range(g_specular_checkbox,kJvgCheckBoxCheckedOffset+1);
            case 208:set_effect_checkbox_state(g_emissive_checkbox,
                read_effect_enabled("EmissiveMaps",&g_emissive_maps_enabled));return 0;
            case 209:set_effect_checkbox_state(g_specular_checkbox,
                read_effect_enabled("SpecularMaps",&g_specular_maps_enabled));return 0;
            case 210:position_effect_checkboxes(c.bump);return 0;
            case 211:set_control_visible(g_emissive_checkbox,true);return 0;
            case 212:set_control_visible(g_specular_checkbox,true);return 0;
            case 213:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_emissive_checkbox);return 0;
            case 214:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_specular_checkbox);return 0;
            case 215:log("Renderer options: reused native map-effect controls after Graphics Options handle recreation");return 0;
            case 217:g_emissive_checkbox=create_vcl_control(c.form,c.bump,kJvgCheckBoxCreateRva);return 0;
            case 218:g_specular_checkbox=create_vcl_control(c.form,c.bump,kJvgCheckBoxCreateRva);return 0;
            case 219:return g_emissive_checkbox!=nullptr;
            case 220:return g_specular_checkbox!=nullptr;
            case 221:g_emissive_checkbox=nullptr;g_specular_checkbox=nullptr;return 0;
            case 222:log("Renderer options: native map-effect checkboxes could not be constructed");return 0;
            case 223:g_effect_checkbox_form=c.form;return 0;
            case 224:c.layout=graphics_options_layout(c.form);return 0;
            case 225:*reinterpret_cast<std::uint8_t*>(static_cast<std::uint8_t*>(
                g_emissive_checkbox)+kJvgCheckBoxOptionsOffset)=
                field<std::uint8_t>(c.bump,kJvgCheckBoxOptionsOffset);return 0;
            case 226:*reinterpret_cast<std::uint8_t*>(static_cast<std::uint8_t*>(
                g_specular_checkbox)+kJvgCheckBoxOptionsOffset)=
                field<std::uint8_t>(c.bump,kJvgCheckBoxOptionsOffset);return 0;
            case 227:return set_initial_control_bounds(g_emissive_checkbox,c.layout.bump_left,
                c.layout.bump_top+c.layout.checkbox_height+kEffectRowGap,
                kEffectWidth,c.layout.checkbox_height);
            case 228:return set_initial_control_bounds(g_specular_checkbox,
                c.layout.bump_left+kEffectWidth+kEffectGap,
                c.layout.bump_top+c.layout.checkbox_height+kEffectRowGap,
                kEffectWidth,c.layout.checkbox_height);
            case 229:log("Renderer options: native map-effect checkbox bounds were unreadable");return 0;
            case 230:a2fo_call_delphi_two_registers(at(kControlSetParentRva),
                g_emissive_checkbox,c.form);return 0;
            case 231:a2fo_call_delphi_two_registers(at(kControlSetParentRva),
                g_specular_checkbox,c.form);return 0;
            case 232:return field<void*>(g_emissive_checkbox,kControlParentOffset)==c.form;
            case 233:return field<void*>(g_specular_checkbox,kControlParentOffset)==c.form;
            case 234:log("Renderer options: native map-effect checkboxes did not accept their visual parent");return 0;
            case 235:a2fo_call_delphi_one_register(at(kAssignJvgCheckBoxImagesRva),
                g_emissive_checkbox);return 0;
            case 236:a2fo_call_delphi_one_register(at(kAssignJvgCheckBoxImagesRva),
                g_specular_checkbox);return 0;
            case 237:set_control_text(g_emissive_checkbox,"Emissive Maps");return 0;
            case 238:set_control_text(g_specular_checkbox,"Specular Maps");return 0;
            case 239:set_effect_checkbox_state(g_emissive_checkbox,
                read_effect_enabled("EmissiveMaps",&g_emissive_maps_enabled));return 0;
            case 240:set_effect_checkbox_state(g_specular_checkbox,
                read_effect_enabled("SpecularMaps",&g_specular_maps_enabled));return 0;
            case 241:position_effect_checkboxes(c.bump);return 0;
            case 242:set_control_visible(g_emissive_checkbox,true);return 0;
            case 243:set_control_visible(g_specular_checkbox,true);return 0;
            case 244:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),
                g_emissive_checkbox);return 0;
            case 245:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),
                g_specular_checkbox);return 0;
            case 246:a2fo_call_delphi_one_register(at(kControlRepaintRva),
                g_emissive_checkbox);return 0;
            case 247:a2fo_call_delphi_one_register(at(kControlRepaintRva),
                g_specular_checkbox);return 0;
            case 248:log("Renderer options: native Emissive Maps and Specular Maps controls added beside Bump Mapping");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_effect_controls(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

bool ensure_graphics_form_subclass(HWND form_window) {
    struct Context {HWND window;LONG_PTR previous{};std::exception_ptr error{};
        explicit Context(HWND value):window(value){}} c(form_window);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 250:return c.window!=nullptr;
            case 251:return IsWindow(c.window)!=FALSE;
            case 252:return c.window==g_form_window;
            case 253:return g_original_form_proc!=nullptr;
            case 254:return g_original_form_proc!=nullptr;
            case 255:log("Renderer options: refused to subclass a second live Graphics Options window");return 0;
            case 256:g_form_window=c.window;return 0;
            case 257:SetLastError(ERROR_SUCCESS);return 0;
            case 258:c.previous=SetWindowLongPtrA(c.window,GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(&graphics_form_window_proc));return 0;
            case 259:return c.previous==0&&GetLastError()!=ERROR_SUCCESS;
            case 260:g_form_window=nullptr;return 0;
            case 261:log("Renderer options: Graphics Options window could not be subclassed");return 0;
            case 262:g_original_form_proc=reinterpret_cast<WNDPROC>(c.previous);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_renderer_subclass(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

void add_combo_item(void* combo_control, const char* text) {
    void* items = field<void*>(combo_control, kComboItemsOffset);
    if (!items || !readable_range(items, sizeof(void*))) return;
    char* delphi_text = nullptr;
    a2fo_call_delphi_two_registers(
        at(kLongStringFromPcharRva), &delphi_text,
        const_cast<char*>(text));
    a2fo_call_delphi_two_registers(at(kStringsAddRva), items, delphi_text);
    a2fo_call_delphi_one_register(at(kLongStringClearRva), &delphi_text);
}

bool install_combo_change_handler(void* combo_control) {
    if (!combo_control ||
        !readable_range(combo_control,
                        kComboOnChangeDataOffset + sizeof(void*))) {
        return false;
    }
    // TNotifyEvent is a Delphi method pointer: Code followed by Data. The
    // dynamically constructed combo has no streamed OnChange handler, so use
    // the control itself as the method instance and receive selection changes
    // through TCustomCombo.Select instead of relying on a raw parent
    // WM_COMMAND notification that TJvHTComboBox consumes internally.
    *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(combo_control) +
        kComboOnChangeCodeOffset) =
        reinterpret_cast<void*>(&a2fo_renderer_combo_change_bridge);
    *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(combo_control) +
        kComboOnChangeDataOffset) = combo_control;
    return true;
}

void* create_vcl_control(void* form, void* exemplar,
                         std::uintptr_t constructor_rva) {
    struct Context {void* form;void* exemplar;std::uintptr_t constructor_rva;
        void* class_reference{};void* control{};std::exception_ptr error{};
    } c{form,exemplar,constructor_rva};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 760:return c.exemplar!=nullptr;
            case 761:return readable_range(c.exemplar,sizeof(void*));
            case 762:c.class_reference=field<void*>(c.exemplar,0);return 0;
            case 763:return c.class_reference!=nullptr;
            case 764:return readable_range(c.class_reference,sizeof(void*));
            case 765:c.control=a2fo_call_delphi_constructor(at(c.constructor_rva),
                         c.class_reference,c.form);return 0;
            case 766:return c.control!=nullptr;
            case 767:return readable_range(c.control,kFormHandleOffset+sizeof(HWND));
            case 768:return field<void*>(c.control,0)==c.class_reference;
            case 769:log("Renderer options: constructed VCL control class did not match its exemplar");return 0;
            case 770:a2fo_call_delphi_two_registers(at(kComponentInsertRva),c.form,c.control);return 0;
            case 771:return field<void*>(c.control,sizeof(void*))==c.form;
            case 772:log("Renderer options: VCL control did not accept form ownership");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool ok=a2fo_rs_stage8_b_renderer_create_control(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return ok?c.control:nullptr;
}

int native_display_selection(void* native_combo) {
    struct Context {void* combo;HWND window{};LRESULT selected{};
        std::exception_ptr failure{};} context{native_combo};
    const auto step=[](void* opaque,std::uint32_t operation,
                       std::uint32_t) noexcept -> std::uint64_t {
        auto& state=*static_cast<Context*>(opaque);
        try {
            switch(operation) {
            case 0:return state.combo!=nullptr;
            case 1:state.window=reinterpret_cast<HWND>(a2fo_call_delphi_one_register(
                at(kWinControlGetHandleRva),state.combo));return state.window!=nullptr;
            case 2:return IsWindow(state.window)!=FALSE;
            case 3:state.selected=SendMessageA(state.window,CB_GETCURSEL,0,0);
                return static_cast<std::uint32_t>(state.selected);
            default:return 0;
            }
        }catch(...){state.failure=std::current_exception();return UINT64_MAX;}
    };
    const int result=a2fo_rs_stage9_renderer_native_display(&context,step);
    if(context.failure)std::rethrow_exception(context.failure);
    return result;
}

void refresh_game_monitor_combo(void* native_combo) {
    struct Context {
        void* native_combo;std::string configured_device;
        GameMonitorResolution resolution{};int selected{};
        std::exception_ptr error{};
        explicit Context(void* value):native_combo(value){}
    } c(native_combo);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 270:return g_monitor_combo_control!=nullptr;
            case 271:return g_monitor_combo_window!=nullptr;
            case 272:return IsWindow(g_monitor_combo_window)!=FALSE;
            case 273:InterlockedIncrement(&g_syncing_monitor_combo);return 0;
            case 274:SendMessageA(g_monitor_combo_window,CB_RESETCONTENT,0,0);return 0;
            case 275:g_game_monitors=enumerate_game_monitors();return 0;
            case 276:return g_game_monitors.size();
            case 277:{const auto label=game_monitor_label(g_game_monitors[arg]);
                add_combo_item(g_monitor_combo_control,label.c_str());return 0;}
            case 278:return g_game_monitors.empty();
            case 279:add_combo_item(g_monitor_combo_control,
                "No connected monitors detected");return 0;
            case 280:a2fo_call_delphi_two_registers(at(kCustomComboSetItemIndexRva),
                g_monitor_combo_control,nullptr);return 0;
            case 281:SendMessageA(g_monitor_combo_window,CB_SETCURSEL,0,0);return 0;
            case 282:g_observed_monitor_selection=-1;return 0;
            case 283:log("Renderer options: no connected game monitors were detected");return 0;
            case 284:InterlockedDecrement(&g_syncing_monitor_combo);return 0;
            case 285:c.resolution=resolve_configured_game_monitor(g_data_root,
                g_game_monitors,native_display_selection(c.native_combo),
                &c.configured_device);return 0;
            case 286:c.selected=c.resolution.monitor_index>=0?
                c.resolution.monitor_index:0;return 0;
            case 287:a2fo_call_delphi_two_registers(at(kCustomComboSetItemIndexRva),
                g_monitor_combo_control,
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(c.selected)));return 0;
            case 288:SendMessageA(g_monitor_combo_window,CB_SETCURSEL,c.selected,0);return 0;
            case 289:g_observed_monitor_selection=c.selected;return 0;
            case 290:InterlockedDecrement(&g_syncing_monitor_combo);return 0;
            case 291:log("Renderer options: detected "+
                std::to_string(g_game_monitors.size())+" connected game monitor(s)");return 0;
            case 292:return c.resolution.configured;
            case 293:return c.resolution.configured_device_present;
            case 294:log("Renderer options: saved game monitor "+c.configured_device+
                " is disconnected; primary monitor selected as fallback");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_refresh_monitor(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

void save_game_monitor_selection(int selected) {
    struct Context {int selected;std::exception_ptr error{};
        explicit Context(int value):selected(value){}} c(selected);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 300:return g_game_monitors.size();
            case 301:g_observed_monitor_selection=c.selected;return 0;
            case 302:return write_configured_game_monitor(g_data_root,
                g_game_monitors[c.selected].device_name);
            case 303:refresh_game_monitor_combo(nullptr);return 0;
            case 304:set_restart_text("Could not save the game monitor setting.");return 0;
            case 305:log("Renderer options: A2FORenderer.ini game monitor could not be written");return 0;
            case 306:set_restart_text("Game monitor saved. Fully exit and relaunch Fleet Operations to apply it.");return 0;
            case 307:{const auto& monitor=g_game_monitors[c.selected];
                log("Renderer options: requested game monitor "+
                    game_monitor_label(monitor)+" ["+monitor.device_name+"]");return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_save_monitor(&c,op,selected);
    if(c.error)std::rethrow_exception(c.error);
}

void ensure_game_monitor_controls(void* form, void* native_combo,
                                  void* native_label) {
    struct Context {
        void* form;void* native_combo;void* native_label;
        GraphicsOptionsLayout layout{};int combo_height{};
        void* native_font{};void* monitor_font{};
        std::exception_ptr error{};
        Context(void* f,void* combo,void* label)
            :form(f),native_combo(combo),native_label(label){}
    } c(form,native_combo,native_label);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 400:return c.form!=nullptr;
            case 401:return c.native_combo!=nullptr;
            case 402:return c.native_label!=nullptr;
            case 403:return g_monitor_control_form==c.form;
            case 404:return is_owned_control_for_form(g_monitor_label,c.native_label,c.form);
            case 405:return is_owned_control_for_form(g_monitor_combo_control,c.native_combo,c.form);
            case 406:g_monitor_combo_window=reinterpret_cast<HWND>(
                a2fo_call_delphi_one_register(at(kWinControlGetHandleRva),g_monitor_combo_control));return 0;
            case 407:return g_monitor_combo_window!=nullptr;
            case 408:return IsWindow(g_monitor_combo_window)!=FALSE;
            case 409:install_combo_change_handler(g_monitor_combo_control);return 0;
            case 410:refresh_game_monitor_combo(c.native_combo);return 0;
            case 411:set_control_visible(g_monitor_label,true);return 0;
            case 412:set_control_visible(g_monitor_combo_control,true);return 0;
            case 413:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_monitor_label);return 0;
            case 414:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_monitor_combo_control);return 0;
            case 415:log("Renderer options: reused and refreshed Game Monitor selector");return 0;
            case 416:g_monitor_label=create_vcl_control(c.form,c.native_label,kCustomLabelCreateRva);return 0;
            case 417:g_monitor_combo_control=create_vcl_control(c.form,c.native_combo,
                kJvCustomHTComboBoxCreateRva);return 0;
            case 418:return g_monitor_label!=nullptr;
            case 419:return g_monitor_combo_control!=nullptr;
            case 420:g_monitor_label=nullptr;g_monitor_combo_control=nullptr;return 0;
            case 421:log("Renderer options: Game Monitor controls could not be created");return 0;
            case 422:g_monitor_control_form=c.form;return 0;
            case 423:set_control_text(g_monitor_label,"Game Monitor");return 0;
            case 424:a2fo_call_delphi_two_registers(at(kCustomLabelSetTransparentRva),
                g_monitor_label,reinterpret_cast<void*>(1));return 0;
            case 425:c.layout=graphics_options_layout(c.form);return 0;
            case 426:c.combo_height=field<int>(c.native_combo,kControlHeightOffset);return 0;
            case 427:return set_initial_control_bounds(g_monitor_label,c.layout.x(288),
                c.layout.y(282),c.layout.x(176),18);
            case 428:return set_initial_control_bounds(g_monitor_combo_control,c.layout.x(480),
                c.layout.y(278),c.layout.x(408),c.combo_height);
            case 429:log("Renderer options: Game Monitor control bounds were unreadable");return 0;
            case 430:a2fo_call_delphi_two_registers(at(kCustomComboBoxSetStyleRva),
                g_monitor_combo_control,reinterpret_cast<void*>(2));return 0;
            case 431:a2fo_call_delphi_two_registers(at(kControlSetParentRva),
                g_monitor_label,c.form);return 0;
            case 432:a2fo_call_delphi_two_registers(at(kControlSetParentRva),
                g_monitor_combo_control,c.form);return 0;
            case 433:c.native_font=field<void*>(c.native_combo,kControlFontOffset);
                return c.native_font!=nullptr;
            case 434:return readable_range(c.native_font,sizeof(void*));
            case 435:a2fo_call_delphi_two_registers(at(kControlSetParentFontRva),
                g_monitor_combo_control,nullptr);return 0;
            case 436:a2fo_call_delphi_two_registers(at(kControlSetFontRva),
                g_monitor_combo_control,c.native_font);return 0;
            case 437:c.monitor_font=field<void*>(g_monitor_combo_control,kControlFontOffset);
                return c.monitor_font!=nullptr;
            case 438:return readable_range(c.monitor_font,sizeof(void*));
            case 439:a2fo_call_delphi_two_registers(at(kFontSetColorRva),c.monitor_font,
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xff000008u)));return 0;
            case 440:g_monitor_combo_window=reinterpret_cast<HWND>(
                a2fo_call_delphi_one_register(at(kWinControlGetHandleRva),g_monitor_combo_control));return 0;
            case 441:return g_monitor_combo_window!=nullptr;
            case 442:return IsWindow(g_monitor_combo_window)!=FALSE;
            case 443:log("Renderer options: Game Monitor combo has no window handle");return 0;
            case 444:refresh_game_monitor_combo(c.native_combo);return 0;
            case 445:return install_combo_change_handler(g_monitor_combo_control);
            case 446:log("Renderer options: Game Monitor change handler was not installed");return 0;
            case 447:set_control_visible(g_monitor_label,true);return 0;
            case 448:set_control_visible(g_monitor_combo_control,true);return 0;
            case 449:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_monitor_label);return 0;
            case 450:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_monitor_combo_control);return 0;
            case 451:a2fo_call_delphi_one_register(at(kControlRepaintRva),g_monitor_label);return 0;
            case 452:a2fo_call_delphi_one_register(at(kControlRepaintRva),g_monitor_combo_control);return 0;
            case 453:log("Renderer options: Game Monitor selector added");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_monitor_controls(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

void ensure_renderer_controls(void* form) {
    struct Context {
        void* form;HWND form_window{};void* native_combo{};void* native_label{};
        void* native_font{};void* renderer_font{};
        GraphicsOptionsLayout layout{};int combo_height{};
        RendererBackend requested{};LRESULT item_count{},selected_index{};
        std::string last_error;std::exception_ptr error{};
        explicit Context(void* value):form(value){}
    } c(form);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 500:return c.form!=nullptr;
            case 501:c.form_window=*reinterpret_cast<HWND*>(
                static_cast<std::uint8_t*>(c.form)+kFormHandleOffset);return 0;
            case 502:return c.form_window!=nullptr;
            case 503:return IsWindow(c.form_window)!=FALSE;
            case 504:return ensure_graphics_form_subclass(c.form_window);
            case 505:ensure_effect_checkboxes(c.form);return 0;
            case 506:c.native_combo=field<void*>(c.form,kPrimaryDeviceComboOffset);return 0;
            case 507:c.native_label=field<void*>(c.form,kFirstNativeLabelOffset);return 0;
            case 508:ensure_game_monitor_controls(c.form,c.native_combo,c.native_label);return 0;
            case 509:return g_renderer_control_form==c.form;
            case 510:return is_owned_control_for_form(g_renderer_label,c.native_label,c.form);
            case 511:return is_owned_control_for_form(g_renderer_combo_control,c.native_combo,c.form);
            case 512:return is_owned_control_for_form(g_restart_label,c.native_label,c.form);
            case 513:g_renderer_combo_window=reinterpret_cast<HWND>(
                a2fo_call_delphi_one_register(at(kWinControlGetHandleRva),
                    g_renderer_combo_control));return 0;
            case 514:return c.form_window==g_form_window;
            case 515:return g_renderer_combo_window!=nullptr;
            case 516:return IsWindow(g_renderer_combo_window)!=FALSE;
            case 517:install_combo_change_handler(g_renderer_combo_control);return 0;
            case 518:c.requested=requested_backend();return static_cast<std::uint32_t>(c.requested);
            case 519:set_combo_index(arg==1?1:0);return 0;
            case 520:set_control_visible(g_renderer_label,true);return 0;
            case 521:set_control_visible(g_renderer_combo_control,true);return 0;
            case 522:set_control_visible(g_restart_label,true);return 0;
            case 523:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),
                g_renderer_combo_control);return 0;
            case 524:refresh_game_monitor_combo(c.native_combo);return 0;
            case 525:ensure_game_monitor_selection_timer();return 0;
            case 526:log("Renderer options: reused Fleet Ops Graphics Options selector after handle recreation");return 0;
            case 527:log("Renderer options: constructing native VCL controls");return 0;
            case 528:g_renderer_label=create_vcl_control(c.form,c.native_label,kCustomLabelCreateRva);return 0;
            case 529:log("Renderer options: VCL renderer label constructed");return 0;
            case 530:g_renderer_combo_control=create_vcl_control(c.form,c.native_combo,
                kJvCustomHTComboBoxCreateRva);return 0;
            case 531:log("Renderer options: VCL renderer combo constructed");return 0;
            case 532:g_restart_label=create_vcl_control(c.form,c.native_label,kCustomLabelCreateRva);return 0;
            case 533:log("Renderer options: VCL restart label constructed");return 0;
            case 534:return g_renderer_label!=nullptr;
            case 535:return g_renderer_combo_control!=nullptr;
            case 536:return g_restart_label!=nullptr;
            case 537:log("Renderer options: Fleet Ops VCL controls could not be created");return 0;
            case 538:g_renderer_control_form=c.form;return 0;
            case 539:set_control_text(g_renderer_label,"Renderer");return 0;
            case 540:set_control_text(g_restart_label,
                "Renderer changes require fully exiting and relaunching Fleet Operations.");return 0;
            case 541:a2fo_call_delphi_two_registers(at(kCustomLabelSetTransparentRva),
                g_renderer_label,reinterpret_cast<void*>(1));return 0;
            case 542:a2fo_call_delphi_two_registers(at(kCustomLabelSetTransparentRva),
                g_restart_label,reinterpret_cast<void*>(1));return 0;
            case 543:log("Renderer options: VCL label properties configured");return 0;
            case 544:c.layout=graphics_options_layout(c.form);return 0;
            case 545:c.combo_height=field<int>(c.native_combo,kControlHeightOffset);return 0;
            case 546:return set_initial_control_bounds(g_renderer_label,c.layout.x(288),
                c.layout.y(246),c.layout.x(176),18);
            case 547:return set_initial_control_bounds(g_renderer_combo_control,
                c.layout.x(480),c.layout.y(242),c.layout.x(408),c.combo_height);
            case 548:return set_initial_control_bounds(g_restart_label,c.layout.x(288),
                c.layout.y(612),c.layout.x(720),18);
            case 549:log("Renderer options: VCL control bounds were unreadable");return 0;
            case 550:log("Renderer options: VCL control bounds configured");return 0;
            case 551:a2fo_call_delphi_two_registers(at(kCustomComboBoxSetStyleRva),
                g_renderer_combo_control,reinterpret_cast<void*>(2));return 0;
            case 552:log("Renderer options: VCL combo style configured");return 0;
            case 553:a2fo_call_delphi_two_registers(at(kControlSetParentRva),
                g_renderer_label,c.form);return 0;
            case 554:a2fo_call_delphi_two_registers(at(kControlSetParentRva),
                g_renderer_combo_control,c.form);return 0;
            case 555:a2fo_call_delphi_two_registers(at(kControlSetParentRva),
                g_restart_label,c.form);return 0;
            case 556:log("Renderer options: configured VCL controls attached to form");return 0;
            case 557:c.native_font=field<void*>(c.native_combo,kControlFontOffset);
                return c.native_font!=nullptr;
            case 558:return readable_range(c.native_font,sizeof(void*));
            case 559:a2fo_call_delphi_two_registers(at(kControlSetParentFontRva),
                g_renderer_combo_control,nullptr);return 0;
            case 560:a2fo_call_delphi_two_registers(at(kControlSetFontRva),
                g_renderer_combo_control,c.native_font);return 0;
            case 561:c.renderer_font=field<void*>(g_renderer_combo_control,kControlFontOffset);
                return c.renderer_font!=nullptr;
            case 563:return readable_range(c.renderer_font,sizeof(void*));
            case 564:a2fo_call_delphi_two_registers(at(kFontSetColorRva),c.renderer_font,
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xff000008u)));return 0;
            case 565:log("Renderer options: renderer combo normal text colour set to clWindowText");return 0;
            case 566:log("Renderer options: renderer combo font object was unavailable");return 0;
            case 567:log("Renderer options: native device selector font was unavailable");return 0;
            case 568:g_renderer_combo_window=reinterpret_cast<HWND>(
                a2fo_call_delphi_one_register(at(kWinControlGetHandleRva),
                    g_renderer_combo_control));return 0;
            case 569:return g_renderer_combo_window!=nullptr;
            case 570:return IsWindow(g_renderer_combo_window)!=FALSE;
            case 571:log("Renderer options: Fleet Ops renderer combo has no window handle");return 0;
            case 572:log("Renderer options: Fleet Ops renderer combo window created");return 0;
            case 573:add_combo_item(g_renderer_combo_control,
                "System Direct3D 9 (Windows / WineD3D)");return 0;
            case 574:add_combo_item(g_renderer_combo_control,"DXVK (Vulkan)");return 0;
            case 575:c.requested=requested_backend();return static_cast<std::uint32_t>(c.requested);
            case 576:set_combo_index(arg==1?1:0);return 0;
            case 577:return install_combo_change_handler(g_renderer_combo_control);
            case 578:log("Renderer options: renderer combo change handler was not installed");return 0;
            case 579:log("Renderer options: renderer combo change handler installed");return 0;
            case 580:set_control_visible(g_renderer_label,true);return 0;
            case 581:set_control_visible(g_renderer_combo_control,true);return 0;
            case 582:set_control_visible(g_restart_label,true);return 0;
            case 583:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_renderer_label);return 0;
            case 584:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),
                g_renderer_combo_control);return 0;
            case 585:a2fo_call_delphi_one_register(at(kControlBringToFrontRva),g_restart_label);return 0;
            case 586:a2fo_call_delphi_one_register(at(kControlRepaintRva),g_renderer_label);return 0;
            case 587:a2fo_call_delphi_one_register(at(kControlRepaintRva),
                g_renderer_combo_control);return 0;
            case 588:a2fo_call_delphi_one_register(at(kControlRepaintRva),g_restart_label);return 0;
            case 589:c.item_count=SendMessageA(g_renderer_combo_window,CB_GETCOUNT,0,0);return 0;
            case 590:c.selected_index=SendMessageA(g_renderer_combo_window,CB_GETCURSEL,0,0);return 0;
            case 591:log("Renderer options: renderer combo populated (items="+
                std::to_string(c.item_count)+", selected="+std::to_string(c.selected_index)+")");return 0;
            case 592:c.last_error=read_last_error();return 0;
            case 593:return c.last_error.empty();
            case 594:set_restart_text("Renderer change failed: "+c.last_error);return 0;
            case 595:return static_cast<std::uint32_t>(applied_backend());
            case 596:return schedule_renderer_helper();
            case 597:set_restart_text(
                "Renderer change pending. Fully exit and relaunch Fleet Operations.");return 0;
            case 598:set_restart_text(
                "Renderer change is pending, but the helper could not start.");return 0;
            case 599:refresh_game_monitor_combo(c.native_combo);return 0;
            case 600:ensure_game_monitor_selection_timer();return 0;
            case 601:log("Renderer options: VCL selector added to Fleet Ops Graphics Options");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_renderer_controls(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

}  // namespace

extern "C" void a2fo_graphics_options_form_show_hook_cpp(void* form,
                                                          void* sender) {
    struct Context {
        void* form;
        void* sender;
        std::uint32_t phase{};
        std::exception_ptr failure{};
    } context{form, sender};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uint32_t) noexcept -> std::uint64_t {
        auto& state = *static_cast<Context*>(opaque);
        state.phase = operation;
        try {
            switch (operation) {
                case 0: log("Renderer options: Graphics Options FormShow entered"); return 0;
                case 1: a2fo_call_delphi_two_registers(
                    g_form_show_hook.gateway, state.form, state.sender); return 0;
                case 2: ensure_renderer_controls(state.form); return 0;
                default: return 0;
            }
        } catch (...) {
            state.failure = std::current_exception();
            return UINT64_MAX;
        }
    };
    a2fo_rs_stage9_renderer_form_show(&context, step);
    if (!context.failure) return;
    if (context.phase == 2) {
        log("Renderer options: Graphics Options integration was skipped after "
            "an exception");
    } else {
        std::rethrow_exception(context.failure);
    }
}

extern "C" void a2fo_jvg_checkbox_set_checked_hook_cpp(
    void* checkbox, void* checked_argument) {
    struct Context {void* checkbox;void* checked_argument;std::uint32_t phase{};
        std::exception_ptr failure{};} context{checkbox,checked_argument};
    const auto step=[](void* opaque,std::uint32_t operation,
                       std::uint32_t) noexcept -> std::uint64_t {
        auto& state=*static_cast<Context*>(opaque);
        state.phase=operation;
        try {
            switch(operation) {
            case 0:a2fo_call_delphi_two_registers(
                g_jvg_checkbox_set_checked_hook.gateway,state.checkbox,
                reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(
                    state.checked_argument)&0xff));return 0;
            case 1:return InterlockedCompareExchange(&g_syncing_effect_checkboxes,0,0)!=0;
            case 2:return state.checkbox==g_emissive_checkbox;
            case 3:save_effect_checkbox(state.checkbox,"EmissiveMaps","emissive",
                &g_emissive_maps_enabled);return 0;
            case 4:return state.checkbox==g_specular_checkbox;
            case 5:save_effect_checkbox(state.checkbox,"SpecularMaps","specular",
                &g_specular_maps_enabled);return 0;
            default:return 0;
            }
        }catch(...){state.failure=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage9_renderer_checkbox(&context,step);
    if (!context.failure) return;
    if (context.phase == 3 || context.phase == 5) {
        log("Renderer options: map-effect checkbox change was ignored after "
            "an exception");
    } else {
        std::rethrow_exception(context.failure);
    }
}

extern "C" void a2fo_renderer_combo_change_hook_cpp(
    void* method_instance, void* sender) {
    struct Context {
        void* method_instance;
        void* sender;
        LRESULT selected{};
        std::uint32_t phase{};
        std::exception_ptr failure{};
    } context{method_instance,sender};
    const auto step=[](void* opaque,std::uint32_t operation,
                       std::uint32_t) noexcept -> std::uint64_t {
        auto& state=*static_cast<Context*>(opaque);
        state.phase=operation;
        try {
            switch(operation) {
            case 0:return state.method_instance==g_renderer_combo_control;
            case 1:return state.sender==g_renderer_combo_control;
            case 2:return InterlockedCompareExchange(&g_syncing_renderer_combo,0,0)==0;
            case 3:return g_renderer_combo_window!=nullptr;
            case 4:return IsWindow(g_renderer_combo_window)!=FALSE;
            case 5:return state.method_instance==g_monitor_combo_control;
            case 6:return state.sender==g_monitor_combo_control;
            case 7:return InterlockedCompareExchange(&g_syncing_monitor_combo,0,0)==0;
            case 8:return g_monitor_combo_window!=nullptr;
            case 9:return IsWindow(g_monitor_combo_window)!=FALSE;
            case 10:state.selected=SendMessageA(g_renderer_combo_window,CB_GETCURSEL,0,0);
                return static_cast<std::uint32_t>(state.selected);
            case 11:log("Renderer options: renderer combo change event (selected="+
                std::to_string(state.selected)+")");return 0;
            case 12:select_renderer(state.selected==1?RendererBackend::dxvk:
                RendererBackend::system);return 0;
            case 13:state.selected=SendMessageA(g_monitor_combo_window,CB_GETCURSEL,0,0);
                return static_cast<std::uint32_t>(state.selected);
            case 14:log("Renderer options: Game Monitor combo change event "
                "(selected="+std::to_string(state.selected)+")");return 0;
            case 15:g_observed_monitor_selection=static_cast<int>(state.selected);return 0;
            case 16:save_game_monitor_selection(static_cast<int>(state.selected));return 0;
            default:return 0;
            }
        }catch(...){state.failure=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage9_renderer_combo_change(&context,step);
    if(context.failure && context.phase>=10) {
        log("Renderer options: combo change was ignored after an exception");
    } else if(context.failure) {
        std::rethrow_exception(context.failure);
    }
}

bool install_renderer_options(HMODULE fleet_ops, const std::string& data_root,
                              void (*log_line)(const std::string&)) {
    struct Context {
        HMODULE fleet_ops;const std::string& data_root;
        void (*log_line)(const std::string&);
        RendererBackend requested{},applied{};std::string last_error;
        std::exception_ptr error{};
        Context(HMODULE module,const std::string& root,
                void (*logger)(const std::string&))
            :fleet_ops(module),data_root(root),log_line(logger){}
    } c{fleet_ops,data_root,log_line};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 40:return c.fleet_ops!=nullptr;
            case 41:return c.data_root.empty();
            case 42:return g_form_show_hook.target!=nullptr;
            case 43:return g_fleet_ops==c.fleet_ops;
            case 44:return g_data_root==c.data_root;
            case 45:g_fleet_ops=c.fleet_ops;return 0;
            case 46:g_data_root=c.data_root;return 0;
            case 47:g_log_line=c.log_line;return 0;
            case 48:reconcile_applied_backend_with_files();return 0;
            case 49:{struct Signature {std::uintptr_t rva;const std::uint8_t* bytes;
                    std::size_t length;};
                const std::array<Signature,24> signatures{{
                    {kGraphicsOptionsFormShowRva,kExpectedFormShow,sizeof(kExpectedFormShow)},
                    {kCustomLabelCreateRva,kExpectedCustomLabelCreate,sizeof(kExpectedCustomLabelCreate)},
                    {kCustomLabelSetTransparentRva,kExpectedCustomLabelSetTransparent,sizeof(kExpectedCustomLabelSetTransparent)},
                    {kStringsAddRva,kExpectedStringsAdd,sizeof(kExpectedStringsAdd)},
                    {kFontSetColorRva,kExpectedFontSetColor,sizeof(kExpectedFontSetColor)},
                    {kJvCustomHTComboBoxCreateRva,kExpectedJvCustomHTComboBoxCreate,sizeof(kExpectedJvCustomHTComboBoxCreate)},
                    {kJvgCheckBoxCreateRva,kExpectedJvgCheckBoxCreate,sizeof(kExpectedJvgCheckBoxCreate)},
                    {kJvgCheckBoxSetCheckedRva,kExpectedJvgCheckBoxSetChecked,sizeof(kExpectedJvgCheckBoxSetChecked)},
                    {kAssignJvgCheckBoxImagesRva,kExpectedAssignJvgCheckBoxImages,sizeof(kExpectedAssignJvgCheckBoxImages)},
                    {kCustomComboSetItemIndexRva,kExpectedCustomComboSetItemIndex,sizeof(kExpectedCustomComboSetItemIndex)},
                    {kCustomComboBoxSetStyleRva,kExpectedCustomComboBoxSetStyle,sizeof(kExpectedCustomComboBoxSetStyle)},
                    {kCustomComboSelectRva,kExpectedCustomComboSelect,sizeof(kExpectedCustomComboSelect)},
                    {kControlSetParentRva,kExpectedControlSetParent,sizeof(kExpectedControlSetParent)},
                    {kControlSetFontRva,kExpectedControlSetFont,sizeof(kExpectedControlSetFont)},
                    {kControlSetParentFontRva,kExpectedControlSetParentFont,sizeof(kExpectedControlSetParentFont)},
                    {kWinControlGetHandleRva,kExpectedWinControlGetHandle,sizeof(kExpectedWinControlGetHandle)},
                    {kControlSetVisibleRva,kExpectedControlSetVisible,sizeof(kExpectedControlSetVisible)},
                    {kControlSetBoundsRva,kExpectedControlSetBounds,sizeof(kExpectedControlSetBounds)},
                    {kControlSetTextRva,kExpectedControlSetText,sizeof(kExpectedControlSetText)},
                    {kControlBringToFrontRva,kExpectedControlBringToFront,sizeof(kExpectedControlBringToFront)},
                    {kControlRepaintRva,kExpectedControlRepaint,sizeof(kExpectedControlRepaint)},
                    {kComponentInsertRva,kExpectedComponentInsert,sizeof(kExpectedComponentInsert)},
                    {kLongStringClearRva,kExpectedLongStringClear,sizeof(kExpectedLongStringClear)},
                    {kLongStringFromPcharRva,kExpectedLongStringFromPchar,sizeof(kExpectedLongStringFromPchar)}
                }};
                const auto& item=signatures[arg];
                return std::memcmp(at(item.rva),item.bytes,item.length)==0;}
            case 50:log("Renderer options: Fleet Ops Graphics Options/VCL signatures did not match");return 0;
            case 51:return a2fo_rs_windows_hook_install(at(kJvgCheckBoxSetCheckedRva),
                reinterpret_cast<void*>(&a2fo_jvg_checkbox_set_checked_bridge),
                sizeof(kExpectedJvgCheckBoxSetChecked),kExpectedJvgCheckBoxSetChecked, &(g_jvg_checkbox_set_checked_hook));
            case 52:return a2fo_rs_windows_hook_install(at(kGraphicsOptionsFormShowRva),
                reinterpret_cast<void*>(&a2fo_graphics_options_form_show_bridge),
                sizeof(kExpectedFormShow),kExpectedFormShow, &(g_form_show_hook));
            case 53:log("Renderer options: Fleet Ops Graphics Options hook was not installed");return 0;
            case 54:log("Renderer options: restart-applied renderer and Game Monitor selectors enabled");return 0;
            case 55:c.requested=requested_backend();return static_cast<std::uint32_t>(c.requested);
            case 56:c.applied=applied_backend();return static_cast<std::uint32_t>(c.applied);
            case 57:log(std::string("Renderer options: requested backend ")+
                backend_text(c.requested)+", helper-confirmed backend "+backend_text(c.applied));return 0;
            case 58:append_renderer_audit(std::string("observed requested backend ")+
                backend_text(c.requested)+", helper-confirmed backend "+backend_text(c.applied));return 0;
            case 59:c.last_error=read_last_error();return 0;
            case 60:return c.last_error.empty();
            case 61:return schedule_renderer_helper();
            case 62:log("Renderer options: pending renderer request will be applied after this process exits");
                append_renderer_audit("pending renderer request scheduled for process exit");return 0;
            case 63:append_renderer_audit("pending renderer request could not schedule the helper");return 0;
            case 64:append_renderer_audit("no renderer-helper action was required");return 0;
            case 65:append_renderer_audit(
                std::string("pending renderer request retained its previous error: ")+
                c.last_error);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_renderer_install(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

bool renderer_emissive_maps_enabled() noexcept {
    return read_effect_enabled(
        "EmissiveMaps", &g_emissive_maps_enabled);
}

bool renderer_specular_maps_enabled() noexcept {
    return read_effect_enabled(
        "SpecularMaps", &g_specular_maps_enabled);
}

}  // namespace a2fo
