/*
 * Restores Instant Action's Load Settings command and corrects Fleet
 * Operations' spaced text-profile payload before Armada validates it.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "load_button_bounds.hpp"
#include "setup_details_line.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" std::uintptr_t __cdecl
a2fo_instant_action_settings_call_thiscall_0(void* function, void* self);
extern "C" std::uintptr_t __cdecl
a2fo_instant_action_settings_call_thiscall_2(
    void* function, void* self, void* first, std::uintptr_t second);
extern "C" std::uintptr_t __cdecl
a2fo_instant_action_settings_call_delphi_2(
    void* function, void* first, std::uintptr_t second);
extern "C" void a2fo_instant_action_settings_read_advanced_bridge();
extern "C" void a2fo_instant_action_settings_file_reader_bridge();

namespace {

using a2fo::instant_action_settings::Bounds;
using a2fo::instant_action_settings::EffectiveBounds;

constexpr char kModuleName[] = "A2FOInstantActionSettings";
constexpr char kSettingsProfileName[] = "Settings.prf";
constexpr std::size_t kMaximumProfilePath = 32768;

constexpr bool kEnableProfileLoadRepair = true;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kMultiplayerSetupDlgProcRva = 0x001c2270;
constexpr std::uintptr_t kLoadSettingsRva = 0x001c9430;
constexpr std::uintptr_t kFileReaderConstructorRva = 0x0012d3c0;
constexpr std::uintptr_t kReadBlobRva = 0x0012d7a0;
constexpr std::uintptr_t kReadIntRva = 0x0012ef70;
constexpr std::uintptr_t kRaceByIdRva = 0x0008b180;
constexpr std::uintptr_t kStartingUnitsRaceLookupReturnRva = 0x00088b77;
constexpr std::uintptr_t kShellMultipleConstructorScanRvaA = 0x001a6b47;
constexpr std::uintptr_t kShellMultipleConstructorScanRvaB = 0x001a6cd3;
constexpr std::uintptr_t kShellMultipleDestructorReadRva = 0x001a6d2e;
constexpr std::uintptr_t kShellMultipleDestructorContinueRva = 0x001a6d4b;
constexpr std::uintptr_t kShellMultipleCycleScanRvaA = 0x001a6dde;
constexpr std::uintptr_t kShellMultipleCycleScanRvaB = 0x001a6e62;
constexpr std::uintptr_t kShellMultipleSelectScanRvaA = 0x001a6ec1;
constexpr std::uintptr_t kShellMultipleSelectScanRvaB = 0x001a6f21;
constexpr std::uintptr_t kNativeEmptyStringPointerRva = 0x003b7dc4;
constexpr std::uintptr_t kCurrentSetupShellRva = 0x0036b8d4;
constexpr std::uintptr_t kGetGameSetupRva = 0x00157940;
constexpr std::uintptr_t kGameSetupIsHostRva = 0x00146de0;
constexpr std::uintptr_t kSaveButtonSlotRva = 0x003a32dc;
constexpr std::uintptr_t kLoadButtonSlotRva = 0x003a32d8;

// FleetOpsHook.map offsets omit the PE .text section's 0x1000 RVA.
constexpr std::uintptr_t kReadAdvancedSettingsRva = 0x001c7220;

constexpr std::size_t kButtonLeftOffset = 0x14;
constexpr std::size_t kButtonTopOffset = 0x18;
constexpr std::size_t kButtonRightOffset = 0x1c;
constexpr std::size_t kButtonBottomOffset = 0x20;
constexpr std::size_t kSetupDetailsPointerOffset = 0x10;
constexpr std::size_t kSetupDetailsSize = 0x338;
constexpr std::size_t kFerengiAllowedOffset = 0xb6;
constexpr std::size_t kTechLevelOffset = 0xc4;
constexpr std::size_t kAdvancedFerengiCheckboxOffset = 0x428;
constexpr std::size_t kCheckBoxCheckedOffset = 0x1c8;

constexpr std::array<std::uint8_t, 5> kExpectedFunctionPrologue{{
    0x55, 0x8b, 0xec, 0x6a, 0xff}};
constexpr std::array<std::uint8_t, 5> kExpectedReadBlob{{
    0x55, 0x8b, 0xec, 0x53, 0x56}};
constexpr std::array<std::uint8_t, 6> kExpectedReadInt{{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c}};
constexpr std::array<std::uint8_t, 5> kExpectedRaceById{{
    0x55, 0x8b, 0xec, 0x53, 0x56}};
constexpr std::array<std::uint8_t, 4> kExpectedGetGameSetup{{
    0x8b, 0x41, 0x30, 0xc3}};
constexpr std::array<std::uint8_t, 11> kExpectedGameSetupIsHost{{
    0x8b, 0x51, 0x18, 0x33, 0xc0, 0x85, 0xd2, 0x0f, 0x95, 0xc0, 0xc3}};
constexpr std::array<std::uint8_t, 8> kExpectedReadAdvancedSettings{{
    0x55, 0x8b, 0xec, 0xb9, 0x08, 0x00, 0x00, 0x00}};

using DialogProc = INT_PTR (CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
using LoadSettings = void (__cdecl*)(void* game_setup);
using ReadBlob = bool (__cdecl*)(void* reader, void* destination,
                                std::uint32_t size);
using ReadInt = bool (__cdecl*)(void* reader, std::int32_t* destination);
using RaceById = void* (__cdecl*)(std::int32_t race_id);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleetops = nullptr;
A2FO_InlineHook g_dialog_hook{};
A2FO_InlineHook g_load_settings_hook{};
A2FO_InlineHook g_file_reader_hook{};
A2FO_InlineHook g_read_blob_hook{};
A2FO_InlineHook g_read_int_hook{};
A2FO_InlineHook g_race_by_id_hook{};
A2FO_InlineHook g_read_advanced_settings_hook{};
PVOID g_shell_multiple_exception_handler = nullptr;
volatile LONG g_load_invocations = 0;
volatile LONG g_header_read_invocations = 0;
volatile LONG g_missing_starting_race_reports = 0;
volatile LONG g_shell_multiple_recovery_reports = 0;
bool g_runtime_ready = false;
bool g_geometry_logged = false;
bool g_fallback_logged = false;
bool g_command_logged = false;
bool g_native_load_logged = false;
bool g_blob_candidate_logged = false;
bool g_corrected_blob_logged = false;
void* g_active_load_game_setup = nullptr;
char g_resolved_profile_path[kMaximumProfilePath]{};

// GameSetup's starting-unit pass assumes every active slot resolves to a
// loaded Race and immediately indexes Race+0x298. A stale Settings.prf can
// retain a race identifier that is absent from the current mod. Returning this
// zeroed table only to that one call site makes the native loop skip the
// missing race's starting objects instead of dereferencing address 0x32c.
alignas(void*) std::array<std::uint8_t, 0x500>
    g_empty_starting_race_table{};

struct SetupState {
    void* details = nullptr;
    std::uint8_t ferengi_allowed = 0;
    std::int32_t tech_level = 0;
    bool valid = false;
};

void* g_last_loaded_game_setup = nullptr;
SetupState g_last_loaded_state{};

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

void* __cdecl race_by_id_hook(std::int32_t race_id) noexcept {
    void* const caller = __builtin_extract_return_addr(
        __builtin_return_address(0));
    const auto original = reinterpret_cast<RaceById>(
        g_race_by_id_hook.gateway);
    void* const race = original ? original(race_id) : nullptr;
    if (race || caller != at(
            g_armada, kStartingUnitsRaceLookupReturnRva)) {
        return race;
    }

    if (InterlockedCompareExchange(
            &g_missing_starting_race_reports, 1, 0) == 0) {
        char message[240]{};
        std::snprintf(
            message, sizeof(message),
            "Skipped starting-unit table for unavailable race ID %ld "
            "loaded from Instant Action setup",
            static_cast<long>(race_id));
        log_line(message);
    }
    return g_empty_starting_race_table.data();
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

bool readable_c_string(const char* text) noexcept {
    if (!text) return false;
    for (std::size_t index = 0; index < 1024; ++index) {
        const char* current = text + index;
        if (!readable_range(current, 1)) return false;
        if (*current == '\0') return true;
    }
    return false;
}

char* native_empty_string() noexcept {
    char* empty = nullptr;
    const void* slot = at(g_armada, kNativeEmptyStringPointerRva);
    if (readable_range(slot, sizeof(empty))) {
        std::memcpy(&empty, slot, sizeof(empty));
    }
    static char fallback[] = "";
    return empty ? empty : fallback;
}

void clear_shell_multiple_label(std::uintptr_t object_address,
                                std::uint32_t index) noexcept {
    if (!object_address || index > 127) return;
    auto* object = reinterpret_cast<std::uint8_t*>(object_address);
    std::uint8_t* entries = nullptr;
    if (!readable_range(object + 0x190, sizeof(entries))) return;
    std::memcpy(&entries, object + 0x190, sizeof(entries));
    if (!entries) return;
    auto* label_field = entries + index * 0x10 + 0x04;
    if (!readable_range(label_field, sizeof(char*))) return;
    char* empty = nullptr;
    std::memcpy(label_field, &empty, sizeof(empty));
}

void sanitize_shell_multiple_labels(std::uintptr_t object_address) noexcept {
    if (!object_address) return;
    auto* object = reinterpret_cast<std::uint8_t*>(object_address);
    std::uint8_t* entries = nullptr;
    std::uint32_t count = 0;
    if (!readable_range(object + 0x190, sizeof(entries)) ||
        !readable_range(object + 0x19c, sizeof(count))) {
        return;
    }
    std::memcpy(&entries, object + 0x190, sizeof(entries));
    std::memcpy(&count, object + 0x19c, sizeof(count));
    if (!entries || count == 0 || count > 128) return;

    for (std::uint32_t index = 0; index < count; ++index) {
        auto* label_field = entries + index * 0x10 + 0x04;
        if (!readable_range(label_field, sizeof(char*))) return;
        char* label = nullptr;
        std::memcpy(&label, label_field, sizeof(label));
        if (label && !readable_c_string(label)) {
            label = nullptr;
            std::memcpy(label_field, &label, sizeof(label));
        }
    }
}

void report_shell_multiple_recovery_once() noexcept {
    if (InterlockedCompareExchange(
            &g_shell_multiple_recovery_reports, 1, 0) == 0) {
        log_line("Recovered corrupt native Instant Action toggle label");
    }
}

LONG WINAPI shell_multiple_exception_handler(
    EXCEPTION_POINTERS* pointers) noexcept {
    if (!pointers || !pointers->ExceptionRecord ||
        !pointers->ContextRecord ||
        pointers->ExceptionRecord->ExceptionCode !=
            EXCEPTION_ACCESS_VIOLATION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const void* fault = pointers->ExceptionRecord->ExceptionAddress;
    CONTEXT* context = pointers->ContextRecord;
    if (fault == at(g_armada, kShellMultipleConstructorScanRvaA) ||
        fault == at(g_armada, kShellMultipleConstructorScanRvaB)) {
        sanitize_shell_multiple_labels(context->Ebx);
        clear_shell_multiple_label(context->Ebx, 0);
        context->Edi = static_cast<DWORD>(
            reinterpret_cast<std::uintptr_t>(native_empty_string()));
        report_shell_multiple_recovery_once();
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (fault == at(g_armada, kShellMultipleCycleScanRvaA) ||
        fault == at(g_armada, kShellMultipleSelectScanRvaA) ||
        fault == at(g_armada, kShellMultipleSelectScanRvaB)) {
        const std::uintptr_t object =
            static_cast<std::uintptr_t>(context->Ebx) - 0x3c;
        clear_shell_multiple_label(object, context->Esi);
        context->Edi = static_cast<DWORD>(
            reinterpret_cast<std::uintptr_t>(native_empty_string()));
        report_shell_multiple_recovery_once();
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (fault == at(g_armada, kShellMultipleCycleScanRvaB)) {
        const std::uintptr_t object = context->Esi;
        std::uint32_t index = 0;
        const void* index_field = reinterpret_cast<const void*>(
            object + 0x188);
        if (readable_range(index_field, sizeof(index))) {
            std::memcpy(&index, index_field, sizeof(index));
            clear_shell_multiple_label(object, index);
        }
        context->Edi = static_cast<DWORD>(
            reinterpret_cast<std::uintptr_t>(native_empty_string()));
        report_shell_multiple_recovery_once();
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (fault == at(g_armada, kShellMultipleDestructorReadRva)) {
        auto* label_field = reinterpret_cast<std::uint8_t*>(
            static_cast<std::uintptr_t>(context->Esi)) + 0x04;
        if (readable_range(label_field, sizeof(char*))) {
            char* empty = nullptr;
            std::memcpy(label_field, &empty, sizeof(empty));
        }
        context->Eip = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(
            at(g_armada, kShellMultipleDestructorContinueRva)));
        report_shell_multiple_recovery_once();
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

std::size_t module_image_size(HMODULE module) noexcept {
    if (!module || !readable_range(module, sizeof(IMAGE_DOS_HEADER))) return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        dos->e_lfanew >= 0x1000) {
        return 0;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    if (!readable_range(nt, sizeof(*nt)) ||
        nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return 0;
    }
    return nt->OptionalHeader.SizeOfImage;
}

bool module_contains(HMODULE module, const void* address,
                     std::size_t size) noexcept {
    const std::size_t image_size = module_image_size(module);
    if (!module || !address || size == 0 || image_size == 0) return false;
    const auto image_begin = reinterpret_cast<std::uintptr_t>(module);
    const auto target_begin = reinterpret_cast<std::uintptr_t>(address);
    if (target_begin < image_begin) return false;
    const auto offset = target_begin - image_begin;
    return offset <= image_size && size <= image_size - offset;
}

template <typename Value>
bool read_value(const void* address, Value* value) noexcept {
    if (!value || !readable_range(address, sizeof(Value))) return false;
    std::memcpy(value, address, sizeof(Value));
    return true;
}

template <typename Value>
bool read_member(const void* object, std::size_t offset,
                 Value* value) noexcept {
    return object && read_value(
        static_cast<const std::uint8_t*>(object) + offset, value);
}

SetupState read_setup_state(void* game_setup) noexcept {
    SetupState state{};
    if (!read_member(game_setup, kSetupDetailsPointerOffset,
                     &state.details) || !state.details ||
        !read_member(state.details, kFerengiAllowedOffset,
                     &state.ferengi_allowed) ||
        !read_member(state.details, kTechLevelOffset,
                     &state.tech_level)) {
        return state;
    }
    state.valid = true;
    return state;
}

const char* enabled_word(std::uint8_t value) noexcept {
    return value ? "enabled" : "disabled";
}

void log_setup_transition(const char* stage, const SetupState& before,
                          const SetupState& after) noexcept {
    char message[320]{};
    if (before.valid && after.valid) {
        std::snprintf(
            message, sizeof(message),
            "%s: Ferengi %s -> %s; tech level %ld -> %ld",
            stage, enabled_word(before.ferengi_allowed),
            enabled_word(after.ferengi_allowed),
            static_cast<long>(before.tech_level),
            static_cast<long>(after.tech_level));
    } else {
        std::snprintf(message, sizeof(message),
                      "%s: live GameSetup state was unavailable", stage);
    }
    log_line(message);
}

bool read_button_bounds(std::uintptr_t slot_rva, Bounds* bounds) noexcept {
    if (!bounds) return false;
    void* button = nullptr;
    if (!read_value(at(g_armada, slot_rva), &button) || !button) return false;

    void* vtable = nullptr;
    if (!read_member(button, 0, &vtable) ||
        (!module_contains(g_fleetops, vtable, sizeof(void*)) &&
         !module_contains(g_armada, vtable, sizeof(void*)))) {
        return false;
    }
    return read_member(button, kButtonLeftOffset, &bounds->left) &&
        read_member(button, kButtonTopOffset, &bounds->top) &&
        read_member(button, kButtonRightOffset, &bounds->right) &&
        read_member(button, kButtonBottomOffset, &bounds->bottom);
}

template <std::size_t Size>
bool signature_matches(
    std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    const void* target = at(g_armada, rva);
    return readable_range(target, expected.size()) &&
        std::memcmp(target, expected.data(), expected.size()) == 0;
}

void log_geometry_once(const Bounds& raw_load, const Bounds& raw_save,
                       const EffectiveBounds& effective) noexcept {
    if (g_geometry_logged) return;
    g_geometry_logged = true;
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "Load click geometry: load=(%ld,%ld)-(%ld,%ld), "
        "save=(%ld,%ld)-(%ld,%ld), effective=(%ld,%ld)-(%ld,%ld)%s",
        static_cast<long>(raw_load.left), static_cast<long>(raw_load.top),
        static_cast<long>(raw_load.right), static_cast<long>(raw_load.bottom),
        static_cast<long>(raw_save.left), static_cast<long>(raw_save.top),
        static_cast<long>(raw_save.right), static_cast<long>(raw_save.bottom),
        static_cast<long>(effective.bounds.left),
        static_cast<long>(effective.bounds.top),
        static_cast<long>(effective.bounds.right),
        static_cast<long>(effective.bounds.bottom),
        effective.repaired ? " (repaired)" : "");
    log_line(message);
}

void __cdecl load_settings_hook(void* game_setup) noexcept {
    const SetupState before = read_setup_state(game_setup);
    const LONG header_reads_before = InterlockedCompareExchange(
        &g_header_read_invocations, 0, 0);
    InterlockedIncrement(&g_load_invocations);
    if (!g_native_load_logged) {
        g_native_load_logged = true;
        log_line("Armada's native LoadSettings routine was invoked");
    }
    const auto original = reinterpret_cast<LoadSettings>(
        g_load_settings_hook.gateway);
    g_active_load_game_setup = game_setup;
    if (original) original(game_setup);
    g_active_load_game_setup = nullptr;
    const LONG header_reads_after = InterlockedCompareExchange(
        &g_header_read_invocations, 0, 0);
    if (header_reads_after == header_reads_before) {
        log_line("Native loader exited before reading the Settings.prf "
                 "header (profile open rejected)");
    }
    const SetupState after = read_setup_state(game_setup);
    g_last_loaded_game_setup = game_setup;
    g_last_loaded_state = after;
    log_setup_transition("Native Settings.prf reader", before, after);
}

bool __cdecl read_int_hook(void* reader,
                           std::int32_t* destination) noexcept {
    const auto original = reinterpret_cast<ReadInt>(g_read_int_hook.gateway);
    const bool tracked = g_active_load_game_setup != nullptr;
    const bool result = original ? original(reader, destination) : false;
    if (tracked) {
        InterlockedIncrement(&g_header_read_invocations);
        char* current = nullptr;
        char prefix[17]{};
        if (read_member(reader, 0x54, &current) && current &&
            readable_range(current, sizeof(prefix) - 1)) {
            std::memcpy(prefix, current, sizeof(prefix) - 1);
            for (char& value : prefix) {
                if (value == '\r' || value == '\n' || value == '\0') {
                    value = '\0';
                    break;
                }
            }
        }
        char message[260]{};
        std::snprintf(message, sizeof(message),
                      "Settings.prf header read: result=%s; value=%ld; "
                      "next='%s'",
                      result ? "success" : "failure",
                      destination ? static_cast<long>(*destination) : -1L,
                      prefix);
        log_line(message);
    }
    return result;
}

extern "C" std::uintptr_t __cdecl
a2fo_instant_action_settings_file_reader_hook_cpp(
    void* reader, void* path_string, std::uintptr_t options) noexcept {
    char* path = nullptr;
    char path_copy[512]{};
    if (read_member(path_string, 0x04, &path) && path) {
        for (std::size_t index = 0; index + 1 < sizeof(path_copy); ++index) {
            if (!readable_range(path + index, 1)) break;
            path_copy[index] = path[index];
            if (path_copy[index] == '\0') break;
        }
    }
    const bool tracked = g_active_load_game_setup != nullptr;
    char* original_path = path;
    char* opened_path = path;
    bool redirected = false;
    if (tracked && std::strcmp(path_copy, kSettingsProfileName) == 0 &&
        A2FO_MODULE_API_HAS(g_api, get_settings_directory) &&
        g_api->get_settings_directory) {
        char directory[kMaximumProfilePath]{};
        if (g_api->get_settings_directory(
                directory, static_cast<std::uint32_t>(sizeof(directory)))) {
            std::size_t length = 0;
            while (length < sizeof(directory) && directory[length] != '\0') {
                ++length;
            }
            const std::size_t name_length = sizeof(kSettingsProfileName) - 1;
            const bool needs_separator = length != 0 &&
                directory[length - 1] != '\\' && directory[length - 1] != '/';
            if (length != 0 && length < sizeof(directory) &&
                length + (needs_separator ? 1 : 0) + name_length + 1 <=
                    sizeof(g_resolved_profile_path)) {
                std::memcpy(g_resolved_profile_path, directory, length);
                if (needs_separator) {
                    g_resolved_profile_path[length++] = '\\';
                }
                std::memcpy(g_resolved_profile_path + length,
                            kSettingsProfileName, name_length + 1);
                opened_path = g_resolved_profile_path;
                std::memcpy(
                    static_cast<std::uint8_t*>(path_string) + 0x04,
                    &opened_path, sizeof(opened_path));
                redirected = true;
            }
        }
    }
    const std::uintptr_t result =
        a2fo_instant_action_settings_call_thiscall_2(
            g_file_reader_hook.gateway, reader, path_string, options);
    if (redirected) {
        std::memcpy(static_cast<std::uint8_t*>(path_string) + 0x04,
                    &original_path, sizeof(original_path));
    }
    if (tracked) {
        std::uint8_t open_error = 1;
        read_member(reader, 0x04, &open_error);
        char message[700]{};
        std::snprintf(message, sizeof(message),
                      "Native Settings.prf open: path='%s'; result=%s%s",
                      opened_path ? opened_path : path_copy,
                      open_error ? "rejected" : "accepted",
                      redirected ? " (resolved SettingsDirectory)" : "");
        log_line(message);
    }
    return result;
}

bool __cdecl read_blob_hook(void* reader, void* destination,
                            std::uint32_t size) noexcept {
    const auto original = reinterpret_cast<ReadBlob>(g_read_blob_hook.gateway);
    void* active_details = nullptr;
    std::uint8_t binary_mode = 1;
    char* current = nullptr;
    char* end = nullptr;
    const bool details_available = g_active_load_game_setup &&
        read_member(g_active_load_game_setup, kSetupDetailsPointerOffset,
                    &active_details);
    const bool mode_available = read_member(reader, 0x06, &binary_mode);
    const bool cursor_available =
        read_member(reader, 0x54, &current) &&
        read_member(reader, 0x58, &end) && current && end && current < end;
    const bool is_setup_details = size == kSetupDetailsSize &&
        cursor_available;

    if (size == kSetupDetailsSize && !g_blob_candidate_logged) {
        g_blob_candidate_logged = true;
        char message[320]{};
        char prefix[17]{};
        if (cursor_available && readable_range(current, sizeof(prefix) - 1)) {
            std::memcpy(prefix, current, sizeof(prefix) - 1);
            for (char& value : prefix) {
                if (value == '\r' || value == '\n' || value == '\0') {
                    value = '\0';
                    break;
                }
            }
        }
        std::snprintf(
            message, sizeof(message),
            "824-byte blob candidate: tracked=%s; destination=%s; "
            "mode=%s%u; cursor=%s; prefix='%s'",
            g_active_load_game_setup ? "yes" : "no",
            details_available && active_details == destination
                ? "setupDetails" : "other",
            mode_available ? "" : "unknown/",
            static_cast<unsigned>(binary_mode),
            cursor_available ? "available" : "unavailable", prefix);
        log_line(message);
    }

    if (is_setup_details) {
        const std::size_t available = static_cast<std::size_t>(end - current);
        constexpr std::size_t kMaximumSetupDetailsLine =
            kSetupDetailsSize * 2 + 64;
        const std::size_t scan_size =
            available < kMaximumSetupDetailsLine
                ? available : kMaximumSetupDetailsLine;
        if (readable_range(current, scan_size)) {
            std::array<std::uint8_t, kSetupDetailsSize> decoded{};
            const auto result =
                a2fo::instant_action_settings::decode_setup_details_line(
                    current, scan_size, decoded.data(), decoded.size());
            if (result.decoded &&
                readable_range(destination, decoded.size())) {
                std::memcpy(destination, decoded.data(), decoded.size());
                char* next = current + result.next_line_offset;
                std::memcpy(static_cast<std::uint8_t*>(reader) + 0x54,
                            &next, sizeof(next));
                if (!g_corrected_blob_logged) {
                    g_corrected_blob_logged = true;
                    log_line("Corrected Fleet Operations' spaced "
                             "setupDetails payload before Armada validation");
                }
                return true;
            }
        }
        log_line("Could not decode the scoped setupDetails payload; "
                 "falling back to Armada's reader");
    }
    return original ? original(reader, destination, size) : false;
}

void* current_game_setup() noexcept;

extern "C" std::uintptr_t __cdecl
a2fo_instant_action_settings_read_advanced_hook_cpp(
    void* form, std::uintptr_t use_defaults) noexcept {
    const SetupState before = use_defaults == 0
        ? read_setup_state(current_game_setup()) : SetupState{};
    const std::uintptr_t result =
        a2fo_instant_action_settings_call_delphi_2(
            g_read_advanced_settings_hook.gateway, form, use_defaults);

    if (use_defaults == 0) {
        void* checkbox = nullptr;
        std::uint8_t checked = 0;
        const bool checkbox_valid =
            read_member(form, kAdvancedFerengiCheckboxOffset, &checkbox) &&
            checkbox &&
            read_member(checkbox, kCheckBoxCheckedOffset, &checked);
        char message[280]{};
        if (before.valid && checkbox_valid) {
            std::snprintf(
                message, sizeof(message),
                "Advanced Settings refresh: GameSetup Ferengi=%s; "
                "checkbox=%s; tech level=%ld",
                enabled_word(before.ferengi_allowed),
                enabled_word(checked),
                static_cast<long>(before.tech_level));
        } else {
            std::snprintf(message, sizeof(message),
                          "Advanced Settings refresh state was unavailable");
        }
        log_line(message);
    }
    return result;
}

void* current_game_setup() noexcept {
    void* shell = nullptr;
    if (!read_value(at(g_armada, kCurrentSetupShellRva), &shell) || !shell) {
        return nullptr;
    }
    return reinterpret_cast<void*>(
        a2fo_instant_action_settings_call_thiscall_0(
            at(g_armada, kGetGameSetupRva), shell));
}

bool is_load_button_command(WPARAM wparam, LPARAM lparam) noexcept {
    if (HIWORD(wparam) != BN_CLICKED || lparam == 0) return false;
    const HWND control = reinterpret_cast<HWND>(lparam);
    if (!IsWindow(control)) return false;

    char caption[128]{};
    const int length = GetWindowTextA(
        control, caption, static_cast<int>(sizeof(caption)));
    const bool matched = length > 0 &&
        a2fo::instant_action_settings::is_load_settings_caption(caption);
    if (matched && !g_command_logged) {
        g_command_logged = true;
        char message[240]{};
        std::snprintf(message, sizeof(message),
                      "Received Load Settings button command (control ID %d, "
                      "caption '%s')",
                      GetDlgCtrlID(control), caption);
        log_line(message);
    }
    return matched;
}

INT_PTR CALLBACK multiplayer_setup_dialog_hook(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    const auto original = reinterpret_cast<DialogProc>(g_dialog_hook.gateway);
    if (!original) return FALSE;

    bool load_click = message == WM_COMMAND &&
        is_load_button_command(wparam, lparam);
    if (message == WM_LBUTTONUP) {
        Bounds raw_load{};
        Bounds raw_save{};
        if (read_button_bounds(kLoadButtonSlotRva, &raw_load) &&
            read_button_bounds(kSaveButtonSlotRva, &raw_save)) {
            const EffectiveBounds effective =
                a2fo::instant_action_settings::effective_load_bounds(
                    raw_load, raw_save);
            if (effective.valid) {
                log_geometry_once(raw_load, raw_save, effective);
                const std::int32_t x = static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(LOWORD(lparam)));
                const std::int32_t y = static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(HIWORD(lparam)));
                load_click = a2fo::instant_action_settings::contains(
                    effective.bounds, x, y);
            }
        }
    }

    const LONG before = InterlockedCompareExchange(
        &g_load_invocations, 0, 0);
    const INT_PTR result = original(window, message, wparam, lparam);
    const LONG after = InterlockedCompareExchange(
        &g_load_invocations, 0, 0);
    if (after != before && g_last_loaded_game_setup) {
        const SetupState post_dialog =
            read_setup_state(g_last_loaded_game_setup);
        log_setup_transition("After setup dialog dispatch",
                             g_last_loaded_state, post_dialog);
    }
    if (!load_click || after != before) {
        return result;
    }

    void* game_setup = current_game_setup();
    if (!game_setup ||
        a2fo_instant_action_settings_call_thiscall_0(
            at(g_armada, kGameSetupIsHostRva), game_setup) == 0) {
        return result;
    }

    if (!g_fallback_logged) {
        g_fallback_logged = true;
        log_line("Native Load button dispatch was missed; invoking Armada's "
                 "LoadSettings routine through the repaired click route");
    }
    load_settings_hook(game_setup);
    return result;
}

bool preflight() noexcept {
    if (a2fo::supported_armada::identify(g_armada) ==
        a2fo::supported_armada::Identity::unsupported) {
        log_line("Unsupported ArmadaL.exe identity; runtime disabled");
        return false;
    }
    if (!g_fleetops || module_image_size(g_fleetops) == 0) {
        log_line("FleetOpsHook.dll image could not be identified; runtime "
                 "disabled");
        return false;
    }
    if (!signature_matches(kMultiplayerSetupDlgProcRva,
                           kExpectedFunctionPrologue) ||
        !signature_matches(kLoadSettingsRva, kExpectedFunctionPrologue) ||
        !signature_matches(kFileReaderConstructorRva,
                           kExpectedFunctionPrologue) ||
        !signature_matches(kReadBlobRva, kExpectedReadBlob) ||
        !signature_matches(kReadIntRva, kExpectedReadInt) ||
        !signature_matches(kRaceByIdRva, kExpectedRaceById)) {
        log_line("Supported Instant Action dialog signatures were not found; "
                 "runtime disabled");
        return false;
    }
    if (!signature_matches(kGetGameSetupRva, kExpectedGetGameSetup) ||
        !signature_matches(kGameSetupIsHostRva,
                           kExpectedGameSetupIsHost) ||
        !readable_range(at(g_armada, kCurrentSetupShellRva), sizeof(void*)) ||
        !readable_range(at(g_armada, kSaveButtonSlotRva), sizeof(void*)) ||
        !readable_range(at(g_armada, kLoadButtonSlotRva), sizeof(void*))) {
        log_line("An Instant Action helper or button binding did not match; "
                 "runtime disabled");
        return false;
    }
    const void* read_advanced = at(g_fleetops, kReadAdvancedSettingsRva);
    if (!readable_range(read_advanced,
                        kExpectedReadAdvancedSettings.size()) ||
        std::memcmp(read_advanced, kExpectedReadAdvancedSettings.data(),
                    kExpectedReadAdvancedSettings.size()) != 0) {
        log_line("Fleet Operations' Advanced Settings reader did not match; "
                 "runtime disabled");
        return false;
    }
    return true;
}

bool install_hooks() noexcept {
    if (!preflight()) return false;
    g_shell_multiple_exception_handler = AddVectoredExceptionHandler(
        1, shell_multiple_exception_handler);
    if (!g_shell_multiple_exception_handler) {
        log_line("Could not install the Instant Action toggle-label guard");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kRaceByIdRva),
            reinterpret_cast<void*>(&race_by_id_hook),
            kExpectedRaceById.size(), kExpectedRaceById.data(),
            &g_race_by_id_hook)) {
        log_line("Could not install the missing starting-race guard");
        return false;
    }
    if (!kEnableProfileLoadRepair) {
        log_line("Profile-load repair isolated; missing starting-race guard "
                 "remains active");
        return true;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kLoadSettingsRva),
            reinterpret_cast<void*>(&load_settings_hook),
            kExpectedFunctionPrologue.size(),
            kExpectedFunctionPrologue.data(), &g_load_settings_hook)) {
        log_line("Could not install the LoadSettings tracking hook");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kReadBlobRva),
            reinterpret_cast<void*>(&read_blob_hook),
            kExpectedReadBlob.size(), kExpectedReadBlob.data(),
            &g_read_blob_hook)) {
        log_line("Could not install the scoped setupDetails decoder hook; "
                 "the LoadSettings hook remains a transparent pass-through");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kFileReaderConstructorRva),
            reinterpret_cast<void*>(
                &a2fo_instant_action_settings_file_reader_bridge),
            kExpectedFunctionPrologue.size(),
            kExpectedFunctionPrologue.data(), &g_file_reader_hook)) {
        log_line("Could not install the Settings.prf pathname diagnostics "
                 "hook");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kReadIntRva),
            reinterpret_cast<void*>(&read_int_hook),
            kExpectedReadInt.size(), kExpectedReadInt.data(),
            &g_read_int_hook)) {
        log_line("Could not install the Settings.prf header diagnostics hook");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kMultiplayerSetupDlgProcRva),
            reinterpret_cast<void*>(&multiplayer_setup_dialog_hook),
            kExpectedFunctionPrologue.size(),
            kExpectedFunctionPrologue.data(), &g_dialog_hook)) {
        log_line("Could not install the Instant Action dialog hook; the "
                 "LoadSettings hook remains a transparent pass-through");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_fleetops, kReadAdvancedSettingsRva),
            reinterpret_cast<void*>(
                &a2fo_instant_action_settings_read_advanced_bridge),
            kExpectedReadAdvancedSettings.size(),
            kExpectedReadAdvancedSettings.data(),
            &g_read_advanced_settings_hook)) {
        log_line("Could not install the Advanced Settings refresh "
                 "diagnostics hook");
        return false;
    }
    return true;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleetops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleetops) return false;

    g_runtime_ready = install_hooks();
    log_line(g_runtime_ready
                 ? (kEnableProfileLoadRepair
                        ? "Instant Action Load Settings repair initialized"
                        : "Instant Action missing-race guard initialized; "
                          "profile-load repair isolated")
                 : "Instant Action settings module loaded with runtime disabled");
    // Inline hooks are process-lifetime patches, so the module must remain
    // resident even if only the transparent tracking hook was installed.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {}
