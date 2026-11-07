/*
 * Restores Instant Action's Load Settings command and corrects Fleet
 * Operations' spaced text-profile payload before Armada validates it.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_b5_ffi.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "load_button_bounds.hpp"
#include "setup_details_line.hpp"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

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

constexpr std::size_t kButtonLeftOffset = 0x14;
constexpr std::size_t kButtonTopOffset = 0x18;
constexpr std::size_t kButtonRightOffset = 0x1c;
constexpr std::size_t kButtonBottomOffset = 0x20;
constexpr std::size_t kSetupDetailsSize = 0x338;

constexpr std::array<std::uint8_t, 5> kExpectedFunctionPrologue{{
    0x55, 0x8b, 0xec, 0x6a, 0xff}};
constexpr std::array<std::uint8_t, 5> kExpectedReadBlob{{
    0x55, 0x8b, 0xec, 0x53, 0x56}};
constexpr std::array<std::uint8_t, 5> kExpectedRaceById{{
    0x55, 0x8b, 0xec, 0x53, 0x56}};
constexpr std::array<std::uint8_t, 4> kExpectedGetGameSetup{{
    0x8b, 0x41, 0x30, 0xc3}};
constexpr std::array<std::uint8_t, 11> kExpectedGameSetupIsHost{{
    0x8b, 0x51, 0x18, 0x33, 0xc0, 0x85, 0xd2, 0x0f, 0x95, 0xc0, 0xc3}};

using DialogProc = INT_PTR (CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
using LoadSettings = void (__cdecl*)(void* game_setup);
using ReadBlob = bool (__cdecl*)(void* reader, void* destination,
                                std::uint32_t size);
using RaceById = void* (__cdecl*)(std::int32_t race_id);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleetops = nullptr;
A2FO_InlineHook g_dialog_hook{};
A2FO_InlineHook g_load_settings_hook{};
A2FO_InlineHook g_file_reader_hook{};
A2FO_InlineHook g_read_blob_hook{};
A2FO_InlineHook g_race_by_id_hook{};
PVOID g_shell_multiple_exception_handler = nullptr;
volatile LONG g_load_invocations = 0;
volatile LONG g_missing_starting_race_reports = 0;
volatile LONG g_shell_multiple_recovery_reports = 0;
bool g_runtime_ready = false;
void* g_active_load_game_setup = nullptr;
#ifdef A2FO_BUILTIN_FEATURE
std::uint64_t g_active_load_activation = 0;
#endif
char g_resolved_profile_path[kMaximumProfilePath]{};

std::uint64_t feature_activation() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    // Availability is published only after Ready confirms complete setup.
    // The atomic acquire also keeps callbacks passive during preparation.
    return a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE);
#else
    return 1;
#endif
}

bool active_load_scope() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    const auto activation = feature_activation();
    return activation != 0 && g_active_load_game_setup &&
        g_active_load_activation == activation;
#else
    return g_active_load_game_setup != nullptr;
#endif
}

// GameSetup's starting-unit pass assumes every active slot resolves to a
// loaded Race and immediately indexes Race+0x298. A stale Settings.prf can
// retain a race identifier that is absent from the current mod. Returning this
// zeroed table only to that one call site makes the native loop skip the
// missing race's starting objects instead of dereferencing address 0x32c.
alignas(void*) std::array<std::uint8_t, 0x500>
    g_empty_starting_race_table{};

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

#ifdef A2FO_BUILTIN_FEATURE
// Refit loading verifies this exact core entry when admitting our detour.
extern "C" __declspec(dllexport)
#endif
void* __cdecl race_by_id_hook(std::int32_t race_id) noexcept {
    void* const caller = __builtin_extract_return_addr(
        __builtin_return_address(0));
    const auto original = reinterpret_cast<RaceById>(
        g_race_by_id_hook.gateway);
    void* const race = original ? original(race_id) : nullptr;
    if (!feature_activation() || race || caller != at(
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
    if (!feature_activation() || !pointers || !pointers->ExceptionRecord ||
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

struct LoadContext {
    void* game_setup;
    LoadSettings original = nullptr;
#ifdef A2FO_BUILTIN_FEATURE
    void* previous_setup = nullptr;
    std::uint64_t previous_activation = 0;
#endif
};
std::uint64_t load_op(void* opaque, std::uint32_t operation,
                      std::uint32_t) noexcept {
    auto& context = *static_cast<LoadContext*>(opaque);
    switch (operation) {
        case 0:
            InterlockedIncrement(&g_load_invocations);
            context.original = reinterpret_cast<LoadSettings>(
                g_load_settings_hook.gateway);
            return 0;
        case 1:
#ifdef A2FO_BUILTIN_FEATURE
            context.previous_setup = g_active_load_game_setup;
            context.previous_activation = g_active_load_activation;
            g_active_load_activation = feature_activation();
            g_active_load_game_setup = g_active_load_activation
                ? context.game_setup : nullptr;
#else
            g_active_load_game_setup = context.game_setup;
#endif
            return 0;
        case 2: {
            if (context.original) context.original(context.game_setup);
            return 0;
        }
        case 3:
#ifdef A2FO_BUILTIN_FEATURE
            g_active_load_game_setup = context.previous_setup;
            g_active_load_activation = context.previous_activation;
#else
            g_active_load_game_setup = nullptr;
#endif
            return 0;
    }
    return 0;
}
void __cdecl load_settings_hook(void* game_setup) noexcept {
    LoadContext context{game_setup};
    a2fo_rs_instant_load(&context, &load_op);
}

struct ReaderContext {
    void* reader;
    void* path_string;
    std::uintptr_t options;
    char* path = nullptr;
    char path_copy[512]{};
    char* original_path = nullptr;
};
std::uint64_t reader_op(void* opaque, std::uint32_t operation,
                        std::uint32_t) noexcept {
    auto& context = *static_cast<ReaderContext*>(opaque);
    switch (operation) {
        case 0: return active_load_scope();
        case 1:
            if (read_member(context.path_string, 0x04, &context.path) &&
                context.path) {
                for (std::size_t index = 0;
                     index + 1 < sizeof(context.path_copy); ++index) {
                    if (!readable_range(context.path + index, 1)) break;
                    context.path_copy[index] = context.path[index];
                    if (context.path_copy[index] == '\0') break;
                }
            }
            context.original_path = context.path;
            return 0;
        case 2: return active_load_scope();
        case 3:
            return std::strcmp(context.path_copy, kSettingsProfileName) == 0 &&
                A2FO_MODULE_API_HAS(g_api, get_settings_directory) &&
                g_api->get_settings_directory;
        case 4: {
            char directory[kMaximumProfilePath]{};
            if (!g_api->get_settings_directory(
                    directory, static_cast<std::uint32_t>(sizeof(directory))))
                return 0;
            std::size_t length = 0;
            while (length < sizeof(directory) && directory[length] != '\0') ++length;
            const std::size_t name_length = sizeof(kSettingsProfileName) - 1;
            const bool needs_separator = length != 0 &&
                directory[length - 1] != '\\' && directory[length - 1] != '/';
            if (length == 0 || length >= sizeof(directory) ||
                length + (needs_separator ? 1 : 0) + name_length + 1 >
                    sizeof(g_resolved_profile_path)) return 0;
            std::memcpy(g_resolved_profile_path, directory, length);
            if (needs_separator) g_resolved_profile_path[length++] = '\\';
            std::memcpy(g_resolved_profile_path + length,
                        kSettingsProfileName, name_length + 1);
            char* opened_path = g_resolved_profile_path;
            std::memcpy(static_cast<std::uint8_t*>(context.path_string) + 0x04,
                        &opened_path, sizeof(opened_path));
            return 1;
        }
        case 6:
            return a2fo_instant_action_settings_call_thiscall_2(
                g_file_reader_hook.gateway, context.reader,
                context.path_string, context.options);
        case 7:
            std::memcpy(static_cast<std::uint8_t*>(context.path_string) + 0x04,
                        &context.original_path, sizeof(context.original_path));
            return 0;
    }
    return 0;
}
extern "C" std::uintptr_t __cdecl
 a2fo_instant_action_settings_file_reader_hook_cpp(
    void* reader, void* path_string, std::uintptr_t options) noexcept {
    ReaderContext context{reader, path_string, options};
    return a2fo_rs_instant_reader(&context, &reader_op);
}

struct BlobContext {
    void* reader;
    void* destination;
    std::uint32_t size;
    ReadBlob original;
    char* current = nullptr;
    char* end = nullptr;
};
std::uint64_t blob_op(void* opaque, std::uint32_t operation,
                      std::uint32_t) noexcept {
    auto& context = *static_cast<BlobContext*>(opaque);
    switch (operation) {
        case 0: return active_load_scope();
        case 1:
            return read_member(context.reader, 0x54, &context.current) &&
                read_member(context.reader, 0x58, &context.end) &&
                context.current && context.end && context.current < context.end;
        case 2: return active_load_scope();
        case 3: {
            const std::size_t available = static_cast<std::size_t>(
                context.end - context.current);
            constexpr std::size_t kMaximumSetupDetailsLine =
                kSetupDetailsSize * 2 + 64;
            const std::size_t scan_size = available < kMaximumSetupDetailsLine
                ? available : kMaximumSetupDetailsLine;
            if (!readable_range(context.current, scan_size)) return 0;
            std::array<std::uint8_t, kSetupDetailsSize> decoded{};
            const auto result =
                a2fo::instant_action_settings::decode_setup_details_line(
                    context.current, scan_size, decoded.data(), decoded.size());
            if (!result.decoded ||
                !readable_range(context.destination, decoded.size())) return 0;
            std::memcpy(context.destination, decoded.data(), decoded.size());
            char* next = context.current + result.next_line_offset;
            std::memcpy(static_cast<std::uint8_t*>(context.reader) + 0x54,
                        &next, sizeof(next));
            return 1;
        }
        case 4:
            log_line("Could not decode the scoped setupDetails payload; "
                     "falling back to Armada's reader");
            return 0;
        case 5:
            return context.original && context.original(
                context.reader, context.destination, context.size);
    }
    return 0;
}
bool __cdecl read_blob_hook(void* reader, void* destination,
                            std::uint32_t size) noexcept {
    BlobContext context{reader, destination, size,
        reinterpret_cast<ReadBlob>(g_read_blob_hook.gateway)};
    return a2fo_rs_instant_blob(size, kSetupDetailsSize,
                                &context, &blob_op) != 0;
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
    return length > 0 &&
        a2fo::instant_action_settings::is_load_settings_caption(caption);
}

struct DialogContext {
    HWND window;
    UINT message;
    WPARAM wparam;
    LPARAM lparam;
    DialogProc original;
    void* game_setup = nullptr;
};
std::uint64_t dialog_op(void* opaque, std::uint32_t operation,
                        std::uint32_t) noexcept {
    auto& context = *static_cast<DialogContext*>(opaque);
    switch (operation) {
        case 0: return context.original != nullptr;
        case 1: return feature_activation();
        case 2: {
            bool load_click = context.message == WM_COMMAND &&
                is_load_button_command(context.wparam, context.lparam);
            if (context.message == WM_LBUTTONUP) {
                Bounds raw_load{};
                Bounds raw_save{};
                if (read_button_bounds(kLoadButtonSlotRva, &raw_load) &&
                    read_button_bounds(kSaveButtonSlotRva, &raw_save)) {
                    const EffectiveBounds effective =
                        a2fo::instant_action_settings::effective_load_bounds(
                            raw_load, raw_save);
                    if (effective.valid) {
                        const std::int32_t x = static_cast<std::int16_t>(
                            static_cast<std::uint16_t>(LOWORD(context.lparam)));
                        const std::int32_t y = static_cast<std::int16_t>(
                            static_cast<std::uint16_t>(HIWORD(context.lparam)));
                        load_click = a2fo::instant_action_settings::contains(
                            effective.bounds, x, y);
                    }
                }
            }
            return load_click;
        }
        case 3: return static_cast<std::uint32_t>(
            InterlockedCompareExchange(&g_load_invocations, 0, 0));
        case 4:
            return static_cast<std::uintptr_t>(context.original(
                context.window, context.message, context.wparam,
                context.lparam));
        case 5: return static_cast<std::uint32_t>(
            InterlockedCompareExchange(&g_load_invocations, 0, 0));
        case 6: return feature_activation();
        case 7:
            context.game_setup = current_game_setup();
            return context.game_setup != nullptr;
        case 8:
            return a2fo_instant_action_settings_call_thiscall_0(
                at(g_armada, kGameSetupIsHostRva), context.game_setup) != 0;
        case 9: load_settings_hook(context.game_setup); return 0;
    }
    return 0;
}
INT_PTR CALLBACK multiplayer_setup_dialog_hook(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    DialogContext context{window, message, wparam, lparam,
        reinterpret_cast<DialogProc>(g_dialog_hook.gateway)};
    return a2fo_rs_instant_dialog(&context, &dialog_op);
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
    return true;
}

std::uint64_t instant_init_op(void* opaque, std::uint32_t operation,
                              std::uint32_t argument) noexcept {
    const auto* api = static_cast<const A2FO_ModuleApi*>(opaque);
    try {
    switch (operation) {
        case 0:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            g_fleetops = static_cast<HMODULE>(api->fleetops_module());
            return g_armada && g_fleetops;
        case 1: return preflight();
        case 2:
            g_shell_multiple_exception_handler = AddVectoredExceptionHandler(
                1, shell_multiple_exception_handler);
            if (!g_shell_multiple_exception_handler)
                log_line("Could not install the Instant Action toggle-label guard");
            return g_shell_multiple_exception_handler != nullptr;
        case 3: {
            const bool installed = api->install_inline_hook(
                at(g_armada, kRaceByIdRva),
                reinterpret_cast<void*>(&race_by_id_hook),
                kExpectedRaceById.size(), kExpectedRaceById.data(),
                &g_race_by_id_hook);
            if (!installed)
                log_line("Could not install the missing starting-race guard");
            return installed;
        }
        case 4: {
            const bool installed = api->install_inline_hook(
                at(g_armada, kLoadSettingsRva),
                reinterpret_cast<void*>(&load_settings_hook),
                kExpectedFunctionPrologue.size(),
                kExpectedFunctionPrologue.data(), &g_load_settings_hook);
            if (!installed) log_line("Could not install the LoadSettings tracking hook");
            return installed;
        }
        case 5: {
            const bool installed = api->install_inline_hook(
                at(g_armada, kReadBlobRva),
                reinterpret_cast<void*>(&read_blob_hook),
                kExpectedReadBlob.size(), kExpectedReadBlob.data(),
                &g_read_blob_hook);
            if (!installed)
                log_line("Could not install the scoped setupDetails decoder hook; "
                         "the LoadSettings hook remains a transparent pass-through");
            return installed;
        }
        case 6: {
            const bool installed = api->install_inline_hook(
                at(g_armada, kFileReaderConstructorRva),
                reinterpret_cast<void*>(
                    &a2fo_instant_action_settings_file_reader_bridge),
                kExpectedFunctionPrologue.size(),
                kExpectedFunctionPrologue.data(), &g_file_reader_hook);
            if (!installed)
                log_line("Could not install the Settings.prf path-resolution hook");
            return installed;
        }
        case 7: {
            const bool installed = api->install_inline_hook(
                at(g_armada, kMultiplayerSetupDlgProcRva),
                reinterpret_cast<void*>(&multiplayer_setup_dialog_hook),
                kExpectedFunctionPrologue.size(),
                kExpectedFunctionPrologue.data(), &g_dialog_hook);
            if (!installed)
                log_line("Could not install the Instant Action dialog hook; the "
                         "LoadSettings hook remains a transparent pass-through");
            return installed;
        }
        case 8:
            g_runtime_ready = argument != 0;
            log_line(g_runtime_ready
                ? (kEnableProfileLoadRepair
                    ? "Instant Action Load Settings repair initialized"
                    : "Instant Action missing-race guard initialized; profile-load repair isolated")
                : "Instant Action settings module loaded with runtime disabled");
            return 0;
        case 9:
            log_line("Profile-load repair isolated; missing starting-race guard "
                     "remains active");
            return 0;
    }
    return 0;
    } catch (...) {
        return 0;
    }
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
    return a2fo_rs_instant_init(true, kEnableProfileLoadRepair,
        const_cast<A2FO_ModuleApi*>(api), &instant_init_op) != 0;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {}
