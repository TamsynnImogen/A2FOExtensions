/*
 * File: core/renderer_options.cpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Restart-applied renderer selection on the Fleet Ops Graphics form.
 */

#include "renderer_options.hpp"

#include "build_identity.hpp"
#include "hook.hpp"

#include <windows.h>

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
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
constexpr std::uintptr_t kControlSetTextRva = 0x0c574c;
constexpr std::uintptr_t kControlBringToFrontRva = 0x0c59ac;
constexpr std::uintptr_t kControlRepaintRva = 0x0c5d30;
constexpr std::uintptr_t kComponentInsertRva = 0x00088274;
constexpr std::uintptr_t kLongStringClearRva = 0x000056b8;
constexpr std::uintptr_t kLongStringFromPcharRva = 0x000058b0;
constexpr std::size_t kFormHandleOffset = 0x1c4;
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
constexpr int kEffectsTop = 468;
constexpr int kEffectWidth = 142;
constexpr int kEffectGap = 8;

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
    0x53, 0x8b, 0xd8, 0x8b, 0xc3};
const std::uint8_t kExpectedControlSetVisible[] = {
    0x53, 0x56, 0x57, 0x8b, 0xda, 0x8b, 0xf8};
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
InlineHook g_form_show_hook;
InlineHook g_jvg_checkbox_set_checked_hook;

HWND g_form_window = nullptr;
void* g_renderer_label = nullptr;
void* g_renderer_combo_control = nullptr;
HWND g_renderer_combo_window = nullptr;
void* g_renderer_control_form = nullptr;
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

extern "C" void* a2fo_call_delphi_one_register(
    void* function, void* eax_argument);
extern "C" void* a2fo_call_delphi_two_registers(
    void* function, void* eax_argument, void* edx_argument);
extern "C" void* a2fo_call_delphi_constructor(
    void* function, void* class_reference, void* owner);
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
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

std::string renderer_ini_path() {
    return join_path(g_data_root, "A2FORenderer.ini");
}

bool read_effect_enabled(const char* key, volatile LONG* cached) noexcept {
    if (!key || !cached) return true;
    LONG value = InterlockedCompareExchange(cached, 0, 0);
    if (value >= 0) return value != 0;
    const LONG loaded = g_data_root.empty()
        ? 1
        : (GetPrivateProfileIntA(
               "Effects", key, 1, renderer_ini_path().c_str()) != 0
               ? 1 : 0);
    InterlockedCompareExchange(cached, loaded, -1);
    return InterlockedCompareExchange(cached, 0, 0) != 0;
}

bool write_effect_enabled(const char* key, bool enabled) noexcept {
    return key && !g_data_root.empty() &&
        WritePrivateProfileStringA(
            "Effects", key, enabled ? "1" : "0",
            renderer_ini_path().c_str()) != FALSE;
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
    if (!checkbox || !key || !label || !cached ||
        !readable_range(checkbox, kJvgCheckBoxCheckedOffset + 1)) {
        return;
    }
    const bool enabled = field<std::uint8_t>(
        checkbox, kJvgCheckBoxCheckedOffset) != 0;
    if (!write_effect_enabled(key, enabled)) {
        set_effect_checkbox_state(checkbox, read_effect_enabled(key, cached));
        log(std::string("Renderer options: could not save ") + label +
            " map setting");
        return;
    }
    InterlockedExchange(cached, enabled ? 1 : 0);
    log(std::string("Renderer options: ") + label + " maps " +
        (enabled ? "enabled" : "disabled"));
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
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool same_file_contents(const std::string& left,
                        const std::string& right) {
    HANDLE left_file = CreateFileA(
        left.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (left_file == INVALID_HANDLE_VALUE) return false;
    HANDLE right_file = CreateFileA(
        right.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (right_file == INVALID_HANDLE_VALUE) {
        CloseHandle(left_file);
        return false;
    }

    LARGE_INTEGER left_size{};
    LARGE_INTEGER right_size{};
    bool equal = GetFileSizeEx(left_file, &left_size) &&
                 GetFileSizeEx(right_file, &right_size) &&
                 left_size.QuadPart == right_size.QuadPart;
    std::array<std::uint8_t, 16 * 1024> left_buffer{};
    std::array<std::uint8_t, 16 * 1024> right_buffer{};
    while (equal) {
        DWORD left_read = 0;
        DWORD right_read = 0;
        if (!ReadFile(left_file, left_buffer.data(), left_buffer.size(),
                      &left_read, nullptr) ||
            !ReadFile(right_file, right_buffer.data(), right_buffer.size(),
                      &right_read, nullptr) ||
            left_read != right_read ||
            std::memcmp(left_buffer.data(), right_buffer.data(), left_read) !=
                0) {
            equal = false;
            break;
        }
        if (left_read == 0) break;
    }
    CloseHandle(right_file);
    CloseHandle(left_file);
    return equal;
}

bool ensure_directory(const std::string& path) {
    if (path.empty()) return false;
    const DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    const std::size_t slash = path.find_last_of("\\/");
    if (slash != std::string::npos && slash > 0 &&
        !ensure_directory(path.substr(0, slash))) {
        return false;
    }
    return CreateDirectoryA(path.c_str(), nullptr) != FALSE ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

RendererBackend backend_from_text(const char* text) {
    return text && _stricmp(text, "dxvk") == 0
               ? RendererBackend::dxvk
               : RendererBackend::system;
}

const char* backend_text(RendererBackend backend) {
    return backend == RendererBackend::dxvk ? "dxvk" : "system";
}

RendererBackend read_backend(const char* key, RendererBackend fallback) {
    char value[32]{};
    GetPrivateProfileStringA("Renderer", key, backend_text(fallback), value,
                             sizeof(value), renderer_ini_path().c_str());
    return backend_from_text(value);
}

RendererBackend requested_backend() {
    const RendererBackend applied =
        read_backend("AppliedBackend", RendererBackend::system);
    return read_backend("Backend", applied);
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
    RendererBackend detected = RendererBackend::system;
    const bool active_exists = file_exists(active_renderer_path());
    if (active_exists) {
        if (!file_exists(dxvk_payload_path()) ||
            !same_file_contents(active_renderer_path(), dxvk_payload_path())) {
            log("Renderer options: Data\\d3d9.dll is not the managed DXVK "
                "payload; helper state was left unchanged");
            return;
        }
        detected = RendererBackend::dxvk;
    }

    const RendererBackend recorded = applied_backend();
    if (recorded == detected) return;
    if (WritePrivateProfileStringA(
            "Renderer", "AppliedBackend", backend_text(detected),
            renderer_ini_path().c_str()) == FALSE) {
        log("Renderer options: could not reconcile AppliedBackend with the "
            "active renderer file");
        return;
    }
    log(std::string("Renderer options: reconciled helper state to active ") +
        backend_text(detected) + " renderer file");
}

std::string read_last_error() {
    char value[256]{};
    GetPrivateProfileStringA("Renderer", "LastError", "", value,
                             sizeof(value), renderer_ini_path().c_str());
    return value;
}

bool preserve_active_dxvk_payload() {
    if (file_exists(dxvk_payload_path())) return true;
    if (!file_exists(active_renderer_path())) return false;
    const std::string directory =
        join_path(g_data_root, "renderers\\dxvk");
    if (!ensure_directory(directory)) return false;
    return CopyFileA(active_renderer_path().c_str(),
                     dxvk_payload_path().c_str(), TRUE) != FALSE ||
           GetLastError() == ERROR_FILE_EXISTS;
}

void set_restart_text(const std::string& text) {
    set_control_text(g_restart_label, text.c_str());
    if (g_restart_label) {
        a2fo_call_delphi_one_register(at(kControlRepaintRva), g_restart_label);
    }
}

bool schedule_renderer_helper() {
    if (g_helper_scheduled) return true;
    const std::string helper = renderer_helper_path();
    if (!file_exists(helper)) {
        log("Renderer options: A2FORendererHelper.exe is not installed");
        return false;
    }

    std::string command = "\"" + helper + "\" --wait-pid " +
                          std::to_string(GetCurrentProcessId()) +
                          " --data-root \"" + g_data_root + "\"";
    std::vector<char> mutable_command(command.begin(), command.end());
    mutable_command.push_back('\0');

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessA(
        helper.c_str(), mutable_command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, g_data_root.c_str(), &startup, &process);
    if (!created) {
        log("Renderer options: could not start renderer helper (error " +
            std::to_string(GetLastError()) + ")");
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    g_helper_scheduled = true;
    log("Renderer options: renderer helper scheduled for game exit");
    return true;
}

void select_renderer(RendererBackend backend) {
    const RendererBackend previous = requested_backend();
    const RendererBackend applied = applied_backend();
    if (backend == previous && backend == applied &&
        read_last_error().empty()) {
        set_restart_text("Renderer is already selected for the next start.");
        return;
    }

    if (backend == RendererBackend::dxvk &&
        !file_exists(dxvk_payload_path())) {
        if (applied_backend() != RendererBackend::dxvk ||
            !preserve_active_dxvk_payload()) {
            set_combo_index(previous == RendererBackend::dxvk ? 1 : 0);
            set_restart_text("DXVK is not installed in Data\\renderers\\dxvk.");
            log("Renderer options: DXVK selection rejected because its payload "
                "is missing");
            return;
        }
    }

    // A loaded d3d9.dll must never be overwritten. If this session is using
    // managed DXVK, preserve a byte-identical payload now; the helper removes
    // the active copy only after Armada has fully exited.
    if (backend == RendererBackend::system &&
        applied == RendererBackend::dxvk &&
        !preserve_active_dxvk_payload()) {
        set_combo_index(previous == RendererBackend::dxvk ? 1 : 0);
        set_restart_text("Could not preserve DXVK; renderer was not changed.");
        log("Renderer options: could not preserve the active DXVK payload");
        return;
    }

    if (!write_backend(backend)) {
        set_combo_index(previous == RendererBackend::dxvk ? 1 : 0);
        set_restart_text("Could not save the renderer setting.");
        log("Renderer options: A2FORenderer.ini could not be written");
        return;
    }
    WritePrivateProfileStringA("Renderer", "LastError", "",
                               renderer_ini_path().c_str());

    if (!schedule_renderer_helper()) {
        set_restart_text(
            "Saved, but the renderer helper is missing; restart change pending.");
        return;
    }
    set_restart_text(
        "Renderer saved. Fully exit and relaunch Fleet Operations to apply it.");
    log(std::string("Renderer options: requested backend ") +
        backend_text(backend));
}

LRESULT CALLBACK graphics_form_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
    const bool renderer_selection_notification =
        message == WM_COMMAND &&
        reinterpret_cast<HWND>(lparam) == g_renderer_combo_window &&
        HIWORD(wparam) == CBN_SELCHANGE &&
        InterlockedCompareExchange(&g_syncing_renderer_combo, 0, 0) == 0;
    const RendererBackend requested_before = renderer_selection_notification
        ? requested_backend() : RendererBackend::system;
    WNDPROC original = g_original_form_proc;
    const LRESULT result = original
        ? CallWindowProcA(original, window, message, wparam, lparam)
        : DefWindowProcA(window, message, wparam, lparam);
    // TJvHTComboBox normally reaches our Delphi OnChange method. Retain the
    // native parent notification as a Windows fallback: some Jv/VCL builds
    // consume CBN_SELCHANGE without invoking a dynamically assigned handler.
    // If OnChange already persisted the request, this comparison is a no-op.
    if (renderer_selection_notification && g_renderer_combo_window &&
        IsWindow(g_renderer_combo_window)) {
        const LRESULT selected =
            SendMessageA(g_renderer_combo_window, CB_GETCURSEL, 0, 0);
        if (selected == 0 || selected == 1) {
            const RendererBackend backend =
                selected == 1 ? RendererBackend::dxvk
                              : RendererBackend::system;
            if (requested_backend() == requested_before &&
                backend != requested_before) {
                log("Renderer options: native Windows combo notification "
                    "handled as OnChange fallback");
                select_renderer(backend);
            }
        }
    }
    if (message == WM_NCDESTROY && window == g_form_window) {
        // Fleet Operations retains the TGraphicOptionsForm component while
        // destroying and recreating only its HWND between visits. Its owned
        // dynamic controls therefore remain valid VCL objects. Keep their
        // identities and discard only native-window/subclass state so the next
        // FormShow reacquires their handles instead of inserting duplicates.
        log("Renderer options: Graphics Options window destroyed; retained "
            "form-owned VCL controls for handle recreation");
        g_form_window = nullptr;
        g_renderer_combo_window = nullptr;
        g_original_form_proc = nullptr;
    }
    return result;
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

void ensure_effect_checkboxes(void* form) {
    if (!form) return;
    void* bump_checkbox = find_native_bump_checkbox(form);
    if (!bump_checkbox) {
        log("Renderer options: native Bump Mapping checkbox was not found");
        return;
    }
    if (g_effect_checkbox_form == form &&
        is_owned_control_for_form(
            g_emissive_checkbox, bump_checkbox, form) &&
        is_owned_control_for_form(
            g_specular_checkbox, bump_checkbox, form) &&
        readable_range(
            g_emissive_checkbox, kJvgCheckBoxCheckedOffset + 1) &&
        readable_range(
            g_specular_checkbox, kJvgCheckBoxCheckedOffset + 1)) {
        set_effect_checkbox_state(
            g_emissive_checkbox,
            read_effect_enabled("EmissiveMaps", &g_emissive_maps_enabled));
        set_effect_checkbox_state(
            g_specular_checkbox,
            read_effect_enabled("SpecularMaps", &g_specular_maps_enabled));
        set_control_visible(g_emissive_checkbox, true);
        set_control_visible(g_specular_checkbox, true);
        a2fo_call_delphi_one_register(
            at(kControlBringToFrontRva), g_emissive_checkbox);
        a2fo_call_delphi_one_register(
            at(kControlBringToFrontRva), g_specular_checkbox);
        log("Renderer options: reused native map-effect controls after "
            "Graphics Options handle recreation");
        return;
    }
    g_emissive_checkbox = create_vcl_control(
        form, bump_checkbox, kJvgCheckBoxCreateRva);
    g_specular_checkbox = create_vcl_control(
        form, bump_checkbox, kJvgCheckBoxCreateRva);
    if (!g_emissive_checkbox || !g_specular_checkbox) {
        g_emissive_checkbox = nullptr;
        g_specular_checkbox = nullptr;
        log("Renderer options: native map-effect checkboxes could not be "
            "constructed");
        return;
    }
    g_effect_checkbox_form = form;

    // Match the native Bump Mapping control's Jvg options (notably
    // fcoFastDraw), while leaving all owned glyph/brush objects independent.
    *reinterpret_cast<std::uint8_t*>(
        static_cast<std::uint8_t*>(g_emissive_checkbox) +
        kJvgCheckBoxOptionsOffset) =
        field<std::uint8_t>(bump_checkbox, kJvgCheckBoxOptionsOffset);
    *reinterpret_cast<std::uint8_t*>(
        static_cast<std::uint8_t*>(g_specular_checkbox) +
        kJvgCheckBoxOptionsOffset) =
        field<std::uint8_t>(bump_checkbox, kJvgCheckBoxOptionsOffset);
    if (!set_initial_control_bounds(
            g_emissive_checkbox, kNativeBumpLeft, kEffectsTop,
            kEffectWidth, kNativeBumpHeight) ||
        !set_initial_control_bounds(
            g_specular_checkbox,
            kNativeBumpLeft + kEffectWidth + kEffectGap, kEffectsTop,
            kEffectWidth, kNativeBumpHeight)) {
        log("Renderer options: native map-effect checkbox bounds were "
            "unreadable");
        return;
    }
    a2fo_call_delphi_two_registers(
        at(kControlSetParentRva), g_emissive_checkbox, form);
    a2fo_call_delphi_two_registers(
        at(kControlSetParentRva), g_specular_checkbox, form);
    // AssignJvgCheckBoxImages changes three owned image objects. Each image's
    // OnChange handler immediately asks the checkbox's visual Parent (+0x30)
    // for its canvas. Streamed checkboxes are already parented when FormShow
    // runs, but our dynamically constructed controls are not. Calling the
    // helper before SetParent therefore dereferences null at FleetOpsHook
    // 0x5A966CDE (Parent + 0x1A4).
    if (field<void*>(g_emissive_checkbox, kControlParentOffset) != form ||
        field<void*>(g_specular_checkbox, kControlParentOffset) != form) {
        log("Renderer options: native map-effect checkboxes did not accept "
            "their visual parent");
        return;
    }
    // FormShow applies Fleet Operations' shared CHEKOV/CHEKON images before
    // assigning each translated caption. Dynamically constructed
    // TJvgCheckBoxes do not receive that DFM setup automatically.
    a2fo_call_delphi_one_register(
        at(kAssignJvgCheckBoxImagesRva), g_emissive_checkbox);
    a2fo_call_delphi_one_register(
        at(kAssignJvgCheckBoxImagesRva), g_specular_checkbox);
    set_control_text(g_emissive_checkbox, "Emissive Maps");
    set_control_text(g_specular_checkbox, "Specular Maps");
    set_effect_checkbox_state(
        g_emissive_checkbox,
        read_effect_enabled("EmissiveMaps", &g_emissive_maps_enabled));
    set_effect_checkbox_state(
        g_specular_checkbox,
        read_effect_enabled("SpecularMaps", &g_specular_maps_enabled));
    set_control_visible(g_emissive_checkbox, true);
    set_control_visible(g_specular_checkbox, true);
    a2fo_call_delphi_one_register(
        at(kControlBringToFrontRva), g_emissive_checkbox);
    a2fo_call_delphi_one_register(
        at(kControlBringToFrontRva), g_specular_checkbox);
    a2fo_call_delphi_one_register(
        at(kControlRepaintRva), g_emissive_checkbox);
    a2fo_call_delphi_one_register(
        at(kControlRepaintRva), g_specular_checkbox);
    log("Renderer options: native Emissive Maps and Specular Maps controls "
        "added beside Bump Mapping");
}

bool ensure_graphics_form_subclass(HWND form_window) {
    if (!form_window || !IsWindow(form_window)) return false;
    if (form_window == g_form_window && g_original_form_proc) return true;
    if (g_original_form_proc) {
        log("Renderer options: refused to subclass a second live Graphics "
            "Options window");
        return false;
    }
    g_form_window = form_window;
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrA(
        form_window, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&graphics_form_window_proc));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        g_form_window = nullptr;
        log("Renderer options: Graphics Options window could not be subclassed");
        return false;
    }
    g_original_form_proc = reinterpret_cast<WNDPROC>(previous);
    return true;
}

void add_combo_item(const char* text) {
    void* items = field<void*>(g_renderer_combo_control, kComboItemsOffset);
    if (!items || !readable_range(items, sizeof(void*))) return;
    char* delphi_text = nullptr;
    a2fo_call_delphi_two_registers(
        at(kLongStringFromPcharRva), &delphi_text,
        const_cast<char*>(text));
    a2fo_call_delphi_two_registers(at(kStringsAddRva), items, delphi_text);
    a2fo_call_delphi_one_register(at(kLongStringClearRva), &delphi_text);
}

bool install_renderer_combo_change_handler() {
    if (!g_renderer_combo_control ||
        !readable_range(g_renderer_combo_control,
                        kComboOnChangeDataOffset + sizeof(void*))) {
        return false;
    }
    // TNotifyEvent is a Delphi method pointer: Code followed by Data. The
    // dynamically constructed combo has no streamed OnChange handler, so use
    // the control itself as the method instance and receive selection changes
    // through TCustomCombo.Select instead of relying on a raw parent
    // WM_COMMAND notification that TJvHTComboBox consumes internally.
    *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(g_renderer_combo_control) +
        kComboOnChangeCodeOffset) =
        reinterpret_cast<void*>(&a2fo_renderer_combo_change_bridge);
    *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(g_renderer_combo_control) +
        kComboOnChangeDataOffset) = g_renderer_combo_control;
    return true;
}

void* create_vcl_control(void* form, void* exemplar,
                         std::uintptr_t constructor_rva) {
    if (!exemplar || !readable_range(exemplar, sizeof(void*))) return nullptr;
    void* class_reference = field<void*>(exemplar, 0);
    if (!class_reference || !readable_range(class_reference, sizeof(void*))) {
        return nullptr;
    }
    void* control = a2fo_call_delphi_constructor(
        at(constructor_rva), class_reference, form);
    if (!control || !readable_range(control, kFormHandleOffset + sizeof(HWND))) {
        return nullptr;
    }
    if (field<void*>(control, 0) != class_reference) {
        log("Renderer options: constructed VCL control class did not match "
            "its exemplar");
        return nullptr;
    }
    // The Jv controls used by Fleet Operations deliberately invoke their
    // inherited constructors with AOwner=nil. Register every dynamically
    // created control explicitly so the form frees it before its visual parent
    // disappears. Without this, reopening Graphics Options leaves windowless
    // controls pointing at destroyed form instances.
    a2fo_call_delphi_two_registers(at(kComponentInsertRva), form, control);
    if (field<void*>(control, sizeof(void*)) != form) {
        log("Renderer options: VCL control did not accept form ownership");
        return nullptr;
    }
    return control;
}

void ensure_renderer_controls(void* form) {
    if (!form) return;
    HWND form_window = *reinterpret_cast<HWND*>(
        static_cast<std::uint8_t*>(form) + kFormHandleOffset);
    if (!form_window || !IsWindow(form_window)) return;
    if (!ensure_graphics_form_subclass(form_window)) return;
    ensure_effect_checkboxes(form);

    void* native_combo = field<void*>(form, kPrimaryDeviceComboOffset);
    void* native_label = field<void*>(form, kFirstNativeLabelOffset);
    const bool reusable_controls =
        g_renderer_control_form == form &&
        is_owned_control_for_form(g_renderer_label, native_label, form) &&
        is_owned_control_for_form(
            g_renderer_combo_control, native_combo, form) &&
        is_owned_control_for_form(g_restart_label, native_label, form);
    if (reusable_controls) {
        g_renderer_combo_window = reinterpret_cast<HWND>(
            a2fo_call_delphi_one_register(
                at(kWinControlGetHandleRva), g_renderer_combo_control));
    }
    if (form_window == g_form_window && reusable_controls &&
        g_renderer_combo_window && IsWindow(g_renderer_combo_window)) {
        install_renderer_combo_change_handler();
        const RendererBackend requested = requested_backend();
        set_combo_index(requested == RendererBackend::dxvk ? 1 : 0);
        set_control_visible(g_renderer_label, true);
        set_control_visible(g_renderer_combo_control, true);
        set_control_visible(g_restart_label, true);
        a2fo_call_delphi_one_register(
            at(kControlBringToFrontRva), g_renderer_combo_control);
        log("Renderer options: reused Fleet Ops Graphics Options selector "
            "after handle recreation");
        return;
    }

    log("Renderer options: constructing native VCL controls");
    g_renderer_label = create_vcl_control(
        form, native_label, kCustomLabelCreateRva);
    log("Renderer options: VCL renderer label constructed");
    g_renderer_combo_control = create_vcl_control(
        form, native_combo, kJvCustomHTComboBoxCreateRva);
    log("Renderer options: VCL renderer combo constructed");
    g_restart_label = create_vcl_control(
        form, native_label, kCustomLabelCreateRva);
    log("Renderer options: VCL restart label constructed");
    if (!g_renderer_label || !g_renderer_combo_control || !g_restart_label) {
        log("Renderer options: Fleet Ops VCL controls could not be created");
        return;
    }
    g_renderer_control_form = form;

    set_control_text(g_renderer_label, "Renderer");
    set_control_text(
        g_restart_label,
        "Renderer changes require fully exiting and relaunching Fleet Operations.");
    a2fo_call_delphi_two_registers(
        at(kCustomLabelSetTransparentRva), g_renderer_label,
        reinterpret_cast<void*>(1));
    a2fo_call_delphi_two_registers(
        at(kCustomLabelSetTransparentRva), g_restart_label,
        reinterpret_cast<void*>(1));
    log("Renderer options: VCL label properties configured");
    if (!set_initial_control_bounds(g_renderer_label, 288, 246, 176, 18) ||
        !set_initial_control_bounds(g_renderer_combo_control,
                                    480, 242, 408, 30) ||
        !set_initial_control_bounds(g_restart_label, 480, 282, 520, 18)) {
        log("Renderer options: VCL control bounds were unreadable");
        return;
    }
    log("Renderer options: VCL control bounds configured");
    a2fo_call_delphi_two_registers(
        at(kCustomComboBoxSetStyleRva), g_renderer_combo_control,
        reinterpret_cast<void*>(2));
    log("Renderer options: VCL combo style configured");
    // Configure the windowed combo before parenting it. Once it has a live
    // HWND, this old Jv/VCL combination crashes if the base SetBounds path
    // delivers its synthetic WM_WINDOWPOSCHANGED with a null WINDOWPOS.
    a2fo_call_delphi_two_registers(
        at(kControlSetParentRva), g_renderer_label, form);
    a2fo_call_delphi_two_registers(
        at(kControlSetParentRva), g_renderer_combo_control, form);
    a2fo_call_delphi_two_registers(
        at(kControlSetParentRva), g_restart_label, form);
    log("Renderer options: configured VCL controls attached to form");

    // The Graphics form's font is white for its labels. A dynamically added
    // combo defaults to ParentFont=true, so parenting it changes its text to
    // white as well while the native combo background remains white. Copy the
    // DFM-configured device combo font and opt out of subsequent inheritance.
    void* native_combo_font = field<void*>(native_combo, kControlFontOffset);
    if (native_combo_font && readable_range(native_combo_font, sizeof(void*))) {
        a2fo_call_delphi_two_registers(
            at(kControlSetParentFontRva), g_renderer_combo_control, nullptr);
        a2fo_call_delphi_two_registers(
            at(kControlSetFontRva), g_renderer_combo_control,
            native_combo_font);
        void* renderer_combo_font =
            field<void*>(g_renderer_combo_control, kControlFontOffset);
        if (renderer_combo_font &&
            readable_range(renderer_combo_font, sizeof(void*))) {
            // TJvHTComboBox's owner-draw code supplies highlight colours but
            // uses Font.Color for every ordinary/closed row. Both the form
            // and its streamed combo inherit white, which is invisible on the
            // native white combo background. clWindowText is $FF000008 in
            // this VCL generation.
            a2fo_call_delphi_two_registers(
                at(kFontSetColorRva), renderer_combo_font,
                reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(0xff000008u)));
            log("Renderer options: renderer combo normal text colour set to "
                "clWindowText");
        } else {
            log("Renderer options: renderer combo font object was unavailable");
        }
    } else {
        log("Renderer options: native device selector font was unavailable");
    }

    // TWinControl keeps its HWND at +0x1b4 in this VCL build. +0x1c4 is the
    // form-specific handle used above, so reading that offset from a combo
    // always reported an empty handle. Use the VCL getter instead: it also
    // performs HandleNeeded and creates the child HWND before we populate it.
    g_renderer_combo_window = reinterpret_cast<HWND>(
        a2fo_call_delphi_one_register(
            at(kWinControlGetHandleRva), g_renderer_combo_control));
    if (!g_renderer_combo_window || !IsWindow(g_renderer_combo_window)) {
        log("Renderer options: Fleet Ops renderer combo has no window handle");
        return;
    }
    log("Renderer options: Fleet Ops renderer combo window created");

    add_combo_item("System Direct3D 9 (Windows / WineD3D)");
    add_combo_item("DXVK (Vulkan)");
    const RendererBackend requested = requested_backend();
    set_combo_index(requested == RendererBackend::dxvk ? 1 : 0);
    if (!install_renderer_combo_change_handler()) {
        log("Renderer options: renderer combo change handler was not installed");
        return;
    }
    log("Renderer options: renderer combo change handler installed");
    set_control_visible(g_renderer_label, true);
    set_control_visible(g_renderer_combo_control, true);
    set_control_visible(g_restart_label, true);
    a2fo_call_delphi_one_register(
        at(kControlBringToFrontRva), g_renderer_label);
    a2fo_call_delphi_one_register(
        at(kControlBringToFrontRva), g_renderer_combo_control);
    a2fo_call_delphi_one_register(
        at(kControlBringToFrontRva), g_restart_label);
    a2fo_call_delphi_one_register(
        at(kControlRepaintRva), g_renderer_label);
    a2fo_call_delphi_one_register(
        at(kControlRepaintRva), g_renderer_combo_control);
    a2fo_call_delphi_one_register(
        at(kControlRepaintRva), g_restart_label);

    const LRESULT item_count =
        SendMessageA(g_renderer_combo_window, CB_GETCOUNT, 0, 0);
    const LRESULT selected_index =
        SendMessageA(g_renderer_combo_window, CB_GETCURSEL, 0, 0);
    log("Renderer options: renderer combo populated (items=" +
        std::to_string(item_count) + ", selected=" +
        std::to_string(selected_index) + ")");

    const std::string last_error = read_last_error();
    if (!last_error.empty()) {
        set_restart_text("Renderer change failed: " + last_error);
    } else if (requested != applied_backend()) {
        if (schedule_renderer_helper()) {
            set_restart_text(
                "Renderer change pending. Fully exit and relaunch Fleet Operations.");
        } else {
            set_restart_text(
                "Renderer change is pending, but the helper could not start.");
        }
    }

    log("Renderer options: VCL selector added to Fleet Ops Graphics Options");
}

}  // namespace

extern "C" void a2fo_graphics_options_form_show_hook_cpp(void* form,
                                                          void* sender) {
    log("Renderer options: Graphics Options FormShow entered");
    a2fo_call_delphi_two_registers(g_form_show_hook.gateway, form, sender);
    try {
        ensure_renderer_controls(form);
    } catch (...) {
        log("Renderer options: Graphics Options integration was skipped after "
            "an exception");
    }
}

extern "C" void a2fo_jvg_checkbox_set_checked_hook_cpp(
    void* checkbox, void* checked_argument) {
    a2fo_call_delphi_two_registers(
        g_jvg_checkbox_set_checked_hook.gateway, checkbox,
        reinterpret_cast<void*>(
            reinterpret_cast<std::uintptr_t>(checked_argument) & 0xff));
    if (InterlockedCompareExchange(&g_syncing_effect_checkboxes, 0, 0) != 0) {
        return;
    }
    try {
        if (checkbox == g_emissive_checkbox) {
            save_effect_checkbox(checkbox, "EmissiveMaps", "emissive",
                                 &g_emissive_maps_enabled);
        } else if (checkbox == g_specular_checkbox) {
            save_effect_checkbox(checkbox, "SpecularMaps", "specular",
                                 &g_specular_maps_enabled);
        }
    } catch (...) {
        log("Renderer options: map-effect checkbox change was ignored after "
            "an exception");
    }
}

extern "C" void a2fo_renderer_combo_change_hook_cpp(
    void* method_instance, void* sender) {
    if (method_instance != g_renderer_combo_control ||
        sender != g_renderer_combo_control ||
        InterlockedCompareExchange(&g_syncing_renderer_combo, 0, 0) != 0 ||
        !g_renderer_combo_window || !IsWindow(g_renderer_combo_window)) {
        return;
    }
    try {
        const LRESULT selected =
            SendMessageA(g_renderer_combo_window, CB_GETCURSEL, 0, 0);
        log("Renderer options: renderer combo change event (selected=" +
            std::to_string(selected) + ")");
        if (selected == 0 || selected == 1) {
            select_renderer(selected == 1 ? RendererBackend::dxvk
                                          : RendererBackend::system);
        }
    } catch (...) {
        log("Renderer options: renderer combo change was ignored after an "
            "exception");
    }
}

bool install_renderer_options(HMODULE fleet_ops, const std::string& data_root,
                              void (*log_line)(const std::string&)) {
    if (!fleet_ops || data_root.empty()) return false;
    g_fleet_ops = fleet_ops;
    g_data_root = data_root;
    g_log_line = log_line;
    reconcile_applied_backend_with_files();

    if (std::memcmp(at(kGraphicsOptionsFormShowRva), kExpectedFormShow,
                    sizeof(kExpectedFormShow)) != 0 ||
        std::memcmp(at(kCustomLabelCreateRva), kExpectedCustomLabelCreate,
                    sizeof(kExpectedCustomLabelCreate)) != 0 ||
        std::memcmp(at(kCustomLabelSetTransparentRva),
                    kExpectedCustomLabelSetTransparent,
                    sizeof(kExpectedCustomLabelSetTransparent)) != 0 ||
        std::memcmp(at(kStringsAddRva), kExpectedStringsAdd,
                    sizeof(kExpectedStringsAdd)) != 0 ||
        std::memcmp(at(kFontSetColorRva), kExpectedFontSetColor,
                    sizeof(kExpectedFontSetColor)) != 0 ||
        std::memcmp(at(kJvCustomHTComboBoxCreateRva),
                    kExpectedJvCustomHTComboBoxCreate,
                    sizeof(kExpectedJvCustomHTComboBoxCreate)) != 0 ||
        std::memcmp(at(kJvgCheckBoxCreateRva), kExpectedJvgCheckBoxCreate,
                    sizeof(kExpectedJvgCheckBoxCreate)) != 0 ||
        std::memcmp(at(kJvgCheckBoxSetCheckedRva),
                    kExpectedJvgCheckBoxSetChecked,
                    sizeof(kExpectedJvgCheckBoxSetChecked)) != 0 ||
        std::memcmp(at(kAssignJvgCheckBoxImagesRva),
                    kExpectedAssignJvgCheckBoxImages,
                    sizeof(kExpectedAssignJvgCheckBoxImages)) != 0 ||
        std::memcmp(at(kCustomComboSetItemIndexRva),
                    kExpectedCustomComboSetItemIndex,
                    sizeof(kExpectedCustomComboSetItemIndex)) != 0 ||
        std::memcmp(at(kCustomComboBoxSetStyleRva),
                    kExpectedCustomComboBoxSetStyle,
                    sizeof(kExpectedCustomComboBoxSetStyle)) != 0 ||
        std::memcmp(at(kCustomComboSelectRva), kExpectedCustomComboSelect,
                    sizeof(kExpectedCustomComboSelect)) != 0 ||
        std::memcmp(at(kControlSetParentRva), kExpectedControlSetParent,
                    sizeof(kExpectedControlSetParent)) != 0 ||
        std::memcmp(at(kControlSetFontRva), kExpectedControlSetFont,
                    sizeof(kExpectedControlSetFont)) != 0 ||
        std::memcmp(at(kControlSetParentFontRva),
                    kExpectedControlSetParentFont,
                    sizeof(kExpectedControlSetParentFont)) != 0 ||
        std::memcmp(at(kWinControlGetHandleRva),
                    kExpectedWinControlGetHandle,
                    sizeof(kExpectedWinControlGetHandle)) != 0 ||
        std::memcmp(at(kControlSetVisibleRva), kExpectedControlSetVisible,
                    sizeof(kExpectedControlSetVisible)) != 0 ||
        std::memcmp(at(kControlSetTextRva), kExpectedControlSetText,
                    sizeof(kExpectedControlSetText)) != 0 ||
        std::memcmp(at(kControlBringToFrontRva),
                    kExpectedControlBringToFront,
                    sizeof(kExpectedControlBringToFront)) != 0 ||
        std::memcmp(at(kControlRepaintRva), kExpectedControlRepaint,
                    sizeof(kExpectedControlRepaint)) != 0 ||
        std::memcmp(at(kComponentInsertRva), kExpectedComponentInsert,
                    sizeof(kExpectedComponentInsert)) != 0 ||
        std::memcmp(at(kLongStringClearRva), kExpectedLongStringClear,
                    sizeof(kExpectedLongStringClear)) != 0 ||
        std::memcmp(at(kLongStringFromPcharRva), kExpectedLongStringFromPchar,
                    sizeof(kExpectedLongStringFromPchar)) != 0) {
        log("Renderer options: Fleet Ops Graphics Options/VCL signatures did "
            "not match");
        return false;
    }
    if (!install_inline_hook(
            at(kJvgCheckBoxSetCheckedRva),
            reinterpret_cast<void*>(&a2fo_jvg_checkbox_set_checked_bridge),
            sizeof(kExpectedJvgCheckBoxSetChecked),
            kExpectedJvgCheckBoxSetChecked,
            g_jvg_checkbox_set_checked_hook) ||
        !install_inline_hook(
            at(kGraphicsOptionsFormShowRva),
            reinterpret_cast<void*>(&a2fo_graphics_options_form_show_bridge),
            sizeof(kExpectedFormShow), kExpectedFormShow, g_form_show_hook)) {
        log("Renderer options: Fleet Ops Graphics Options hook was not "
            "installed");
        return false;
    }
    log("Renderer options: restart-applied selector enabled");
    const RendererBackend requested = requested_backend();
    const RendererBackend applied = applied_backend();
    log(std::string("Renderer options: requested backend ") +
        backend_text(requested) + ", helper-confirmed backend " +
        backend_text(applied));
    append_renderer_audit(
        std::string("observed requested backend ") +
        backend_text(requested) + ", helper-confirmed backend " +
        backend_text(applied));
    const std::string last_error = read_last_error();
    if (requested != applied && last_error.empty()) {
        if (schedule_renderer_helper()) {
            log("Renderer options: pending renderer request will be applied "
                "after this process exits");
            append_renderer_audit(
                "pending renderer request scheduled for process exit");
        } else {
            append_renderer_audit(
                "pending renderer request could not schedule the helper");
        }
    } else if (requested == applied) {
        append_renderer_audit("no renderer-helper action was required");
    } else {
        append_renderer_audit(
            std::string("pending renderer request retained its previous error: ") +
            last_error);
    }
    return true;
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
