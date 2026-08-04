#include "extension_roots.hpp"
#include "hook.hpp"
#include "lua_host.hpp"
#include "module_loader.hpp"
#include "../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// This file bridges two different binaries and ABIs:
//   * ArmadaL.exe owns the gameplay classes and SOD database (MSVC thiscall).
//   * FleetOpsHook.dll owns the replacement virtual filesystem (Delphi register
//     convention: EAX/EDX/ECX before any stack arguments).
// Every address below is an RVA from the supported module's load base, never a
// process-global absolute address. docs/addresses.md records their provenance.
constexpr std::uint32_t kArmadaTimestamp = 0x3c4c76bd;
constexpr std::uint32_t kArmadaImageSize = 0x00403999;
constexpr std::uint32_t kFleetOpsTimestamp = 0x51f6475c;
constexpr std::uint32_t kFleetOpsImageSize = 0x00322000;

constexpr std::uintptr_t kEvolverClassBuildClassRva = 0x0a85e0;
constexpr std::uintptr_t kEvolverClassDtorRva = 0x0a85d0;
constexpr std::uintptr_t kCocoonSelectorRva = 0x0b0534;
constexpr std::uintptr_t kCocoonUpdateSelectorRva = 0x400d10;
constexpr std::uintptr_t kCocoonUpdateResumeRva = 0x0b0c1b;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x135350;
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kCraftExplodeRva = 0x0c6ab0;
constexpr std::uintptr_t kGameObjectClassFindProjectIdRva = 0x0cd1f0;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x0cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x0cd390;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x0ce370;
constexpr std::uintptr_t kGameObjectGetTransformRva = 0x0cfd50;
constexpr std::uintptr_t kDefaultUserProfileGameSpeedRva = 0x13c8a0;
constexpr std::uintptr_t kLoadSodRva = 0x22cf10;
constexpr std::uintptr_t kSodDatabaseRva = 0x3ad508;
constexpr std::uintptr_t kDefaultCocoonRva = 0x33fccc;
constexpr std::uintptr_t kAlternativeCocoonRva = 0x33fd3c;
constexpr char kDefaultCocoonName[] = "8472_cocoon.sod";
constexpr char kAlternativeCocoonName[] = "8472_cocoon2.sod";

// FleetOpsHook.dll RVAs.
constexpr std::uintptr_t kFofsItemGetHashLookupCallRva = 0x105fec;
constexpr std::uintptr_t kFofsItemLocateHashLookupCallRva = 0x1061e2;
constexpr std::uintptr_t kFofsItemExistsHashLookupCallRva = 0x106263;
constexpr std::uintptr_t kFofsProjectIdHashLookupCallRva = 0x1063ee;
constexpr std::uintptr_t kGetFileFromHashTableRva = 0x109c14;
constexpr std::uintptr_t kGetModUserDirectoryRva = 0x10ab98;
constexpr std::uintptr_t kFoSettingsGetInstanceRva = 0x13e744;
constexpr std::uintptr_t kFoSettingsSaveRva = 0x13e824;
constexpr std::uintptr_t kGameConfigurationNewRva = 0x13e93c;
constexpr std::uintptr_t kGameConfigurationLoadProfileRva = 0x13ea8c;
constexpr std::uintptr_t kDelphiLStrAsgRva = 0x00570c;

constexpr std::size_t kMaximumPathLength = 32767;
// Temporarily leave Fleet Ops/Armada in control of shell and menu sizing.
// The monitor remains available for later repair without affecting gameplay.
constexpr bool kShellDisplayMonitorEnabled = false;

// A timestamp alone is not enough protection for an injected hook. Each patch
// also verifies the exact instructions it is about to replace, and fails
// closed when another Fleet Ops/Armada build is detected.
const std::uint8_t kExpectedBuildClass[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedDtor[] = {0xc7, 0x01, 0x44, 0x1b, 0x6b, 0x00};
const std::uint8_t kExpectedCocoonJump[] = {0xe9, 0xa7, 0x07, 0x35, 0x00};
const std::uint8_t kExpectedCocoonUpdateSelector[] = {
    0x8b, 0x83, 0xe8, 0x01, 0x00, 0x00};
const std::uint8_t kExpectedParameterDbGetString[] = {
    0x55,
    0x8b, 0xec,
    0x81, 0xec, 0x00, 0x01, 0x00, 0x00
};
const std::uint8_t kExpectedAiMissionGetCurrent[] = {
    0xa1, 0xdc, 0x47, 0x73, 0x00, 0xc3};
const std::uint8_t kExpectedCraftExplode[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x20};
const std::uint8_t kExpectedGameObjectClassFindProjectId[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedGameObjectClassFind[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedGameObjectClassConstruct[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00};
const std::uint8_t kExpectedGameObjectClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00,
    0xe9, 0x25, 0xb0, 0x18, 0x00};
const std::uint8_t kExpectedGameObjectGetTransform[] = {
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
const std::uint8_t kExpectedDefaultUserProfileGameSpeed[] = {
    0xb8, 0x05, 0x00, 0x00, 0x00, 0xc3};
const std::uint8_t kExpectedFofsItemGetHashLookupCall[] = {
    0xe8, 0x23, 0x3c, 0x00, 0x00};
const std::uint8_t kExpectedFofsItemLocateHashLookupCall[] = {
    0xe8, 0x2d, 0x3a, 0x00, 0x00};
const std::uint8_t kExpectedFofsItemExistsHashLookupCall[] = {
    0xe8, 0xac, 0x39, 0x00, 0x00};
const std::uint8_t kExpectedFofsProjectIdHashLookupCall[] = {
    0xe8, 0x21, 0x38, 0x00, 0x00};
const std::uint8_t kExpectedGetModUserDirectory[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf2};
const std::uint8_t kExpectedFoSettingsGetInstance[] = {
    0x55, 0x8b, 0xec, 0x6a, 0x00};
const std::uint8_t kExpectedGameConfigurationNew[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedGameConfigurationLoadProfile[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53};

// Implemented in delphi_bridge.S. Normal C++ calls cannot express all of the
// Delphi register ABI and Armada's 32-bit thiscall ABI reliably, so these
// wrappers translate ordinary C stack arguments into the required registers.
extern "C" void* a2fo_call_get_file_from_hash(void* function,
                                                void* file_system,
                                                void* delphi_name,
                                                std::uint32_t flags);
extern "C" void* a2fo_call_evolver_build_class(void* function, void* self,
                                                void* parameter_db);
extern "C" void* a2fo_call_evolver_dtor(void* function, void* self);
extern "C" bool a2fo_parameter_db_get_string(void* function, void* parameter_db,
                                               const char* key, char* output,
                                               std::uint32_t output_size,
                                               const char* default_value);
extern "C" void* a2fo_load_sod(void* function, void* database,
                                const char* name);
extern "C" void a2fo_call_craft_explode(void* function, void* self);
extern "C" void* a2fo_call_game_object_method(void* function, void* self);
extern "C" void* a2fo_call_game_object_construct(
    void* function, void* object_class, const void* transform,
    int team, int parent, char* label);
extern "C" void a2fo_call_game_object_argument(
    void* function, void* self, void* argument);
extern "C" void* a2fo_call_delphi_one_register(
    void* function, void* eax_argument);
extern "C" void* a2fo_call_delphi_two_registers(
    void* function, void* eax_argument, void* edx_argument);
extern "C" void a2fo_cocoon_selector_hook();
extern "C" void a2fo_cocoon_update_selector_hook();

HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_state_lock;
bool g_state_lock_ready = false;
bool g_evolver_hooks_ready = false;
bool g_fofs_item_get_lookup_hook_ready = false;
bool g_mod_user_directory_hook_ready = false;
bool g_fo_settings_default_hook_ready = false;
bool g_default_game_speed_hook_ready = false;
bool g_runtime_game_speed_hook_ready = false;
bool g_user_profile_game_speed_hook_ready = false;
bool g_object_destroyed_hook_ready = false;
a2fo::InlineHook g_build_class_hook;
a2fo::InlineHook g_dtor_hook;
a2fo::InlineHook g_parameter_db_get_string_hook;
a2fo::InlineHook g_craft_explode_hook;
a2fo::InlineHook g_game_object_class_find_project_id_hook;
a2fo::InlineHook g_mod_user_directory_hook;
a2fo::InlineHook g_fo_settings_get_instance_hook;
a2fo::InlineHook g_game_configuration_new_hook;
a2fo::InlineHook g_game_configuration_load_profile_hook;
bool g_classlabel_alias_hook_ready = false;
std::unordered_map<void*, std::string> g_class_cocoons;
std::unordered_map<std::string, void*> g_loaded_cocoons;
std::set<void*> g_logged_cocoon_classes;
std::string g_root_directory;
std::vector<std::string> g_extension_roots;
std::vector<a2fo::LoadedModule> g_loaded_modules;
a2fo::LuaHost g_lua_host;
A2FO_ModuleApi g_module_api{};
volatile LONG g_initialize_state = 0;
volatile LONG g_deferred_initialization_finished = 0;
volatile DWORD g_initialize_thread_id = 0;
volatile LONG g_mod_defaults_state = 0;
volatile LONG g_fo_settings_default_applied = 0;
volatile LONG g_fo_settings_first_run = 0;
volatile LONG g_default_game_speed_logged = 0;
volatile LONG g_runtime_game_speed_logged = 0;
volatile LONG g_user_profile_game_speed_logged = 0;
volatile PVOID g_fo_settings_instance = nullptr;
bool g_has_default_game_speed = false;
int g_default_game_speed = 0;
const char* g_delphi_settings_directory = nullptr;
A2FO_FofsItemLookupHandler g_fofs_item_lookup_handler = nullptr;
void* g_fofs_item_lookup_user_data = nullptr;
std::string g_fofs_item_lookup_owner;
A2FO_InfoIniDefaultsHandler g_info_ini_defaults_handler = nullptr;
void* g_info_ini_defaults_user_data = nullptr;
std::string g_info_ini_defaults_owner;
struct ClasslabelAliasPolicy {
    std::string target;
    std::string owner;
};

struct NativeObjectDestroyedRegistration {
    std::string owner;
    std::vector<std::string> required_odf_fields;
    A2FO_ObjectDestroyedHandler handler = nullptr;
    void* user_data = nullptr;
    bool enabled = true;
};

std::unordered_map<std::string, ClasslabelAliasPolicy> g_classlabel_aliases;
std::unordered_map<void*, std::string> g_original_classlabels;
std::unordered_map<std::string, a2fo::LuaOdfSnapshot> g_odf_snapshots;

struct DestroyedOdfLoadContext {
    a2fo::LuaOdfSnapshot fields;
};

thread_local DestroyedOdfLoadContext* g_destroyed_odf_load_context = nullptr;

std::string g_evolver_cocoon_command;
std::string g_evolver_cocoon_owner;
std::vector<NativeObjectDestroyedRegistration>
    g_object_destroyed_handlers;
std::set<std::string> g_destroyed_odf_fields{"basename"};
bool g_policy_registration_open = true;

struct RegistrationTransaction {
    bool active = false;
    std::string path;
    A2FO_FofsItemLookupHandler fofs_handler = nullptr;
    void* fofs_user_data = nullptr;
    std::string fofs_owner;
    A2FO_InfoIniDefaultsHandler info_ini_defaults_handler = nullptr;
    void* info_ini_defaults_user_data = nullptr;
    std::string info_ini_defaults_owner;
    std::unordered_map<std::string, ClasslabelAliasPolicy> classlabel_aliases;
    std::string cocoon_command;
    std::string cocoon_owner;
    std::vector<NativeObjectDestroyedRegistration>
        object_destroyed_handlers;
    std::set<std::string> destroyed_odf_fields;
};

RegistrationTransaction g_registration_transaction;

class StateLockGuard {
public:
    StateLockGuard() { EnterCriticalSection(&g_state_lock); }
    ~StateLockGuard() { LeaveCriticalSection(&g_state_lock); }

    StateLockGuard(const StateLockGuard&) = delete;
    StateLockGuard& operator=(const StateLockGuard&) = delete;
};

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void replace_slashes(std::string& value, char slash = '\\') {
    for (char& ch : value) {
        if (ch == '/' || ch == '\\') {
            ch = slash;
        }
    }
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == '\\' || left.back() == '/') {
        return left + right;
    }
    return left + "\\" + right;
}

std::string module_directory(HMODULE module) {
    std::vector<char> buffer(kMaximumPathLength);
    const DWORD length = GetModuleFileNameA(module, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    std::string path(buffer.data(), length);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string{} : path.substr(0, slash);
}

void open_log() {
    const std::string directory = module_directory(nullptr);
    const std::string path = join_path(directory, "A2FOExtensions.log");
    g_log = CreateFileA(path.c_str(), GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void log_line(const std::string& text) {
    if (g_log == INVALID_HANDLE_VALUE) {
        return;
    }
    const std::string line = text + "\r\n";
    DWORD written = 0;
    WriteFile(g_log, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    // WriteFile makes new diagnostics visible to readers immediately. Forcing
    // every line through FlushFileBuffers turns large recursive ODF indexes
    // into hundreds or thousands of synchronous disk commits. Windows flushes
    // the handle when the process closes; avoid imposing that durability cost
    // on the game's startup thread.
}

bool validate_module(HMODULE module, std::uint32_t timestamp,
                     std::uint32_t image_size, const char* label) {
    if (!module) {
        log_line(std::string(label) + " is not loaded");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        log_line(std::string(label) + " has no DOS header");
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != timestamp ||
        nt->OptionalHeader.SizeOfImage != image_size) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "%s version mismatch (timestamp=%08lx, image=%08lx)",
                      label,
                      static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
                      static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage));
        log_line(message);
        return false;
    }
    return true;
}

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(module) + rva);
}

void wait_for_deferred_initialization() {
    if (InterlockedCompareExchange(
            &g_deferred_initialization_finished, 0, 0) != 0 ||
        GetCurrentThreadId() == g_initialize_thread_id) {
        return;
    }
    for (unsigned attempt = 0; attempt < 10000; ++attempt) {
        if (InterlockedCompareExchange(
                &g_deferred_initialization_finished, 0, 0) != 0) {
            return;
        }
        Sleep(1);
    }
}

bool query_info_ini_defaults(const char* normal_directory,
                             std::string* resolved_directory,
                             bool& has_default_game_speed,
                             int& default_game_speed) {
    try {
        wait_for_deferred_initialization();
        A2FO_InfoIniDefaultsHandler handler = nullptr;
        void* user_data = nullptr;
        {
            StateLockGuard lock;
            handler = g_info_ini_defaults_handler;
            user_data = g_info_ini_defaults_user_data;
        }
        if (!handler) return false;

        std::vector<char> output;
        if (resolved_directory) output.resize(kMaximumPathLength + 1);
        std::uint32_t has_speed = 0;
        std::int32_t speed = 0;
        if (!handler(normal_directory,
                     output.empty() ? nullptr : output.data(),
                     static_cast<std::uint32_t>(output.size()),
                     &has_speed, &speed, user_data)) {
            return false;
        }
        if (!output.empty()) {
            output.back() = '\0';
            resolved_directory->assign(output.data());
        }
        has_default_game_speed = has_speed != 0 && speed >= 1 && speed <= 6;
        default_game_speed = has_default_game_speed ? speed : 0;
        return true;
    } catch (...) {
        log_line("Fleet Ops info.ini defaults provider failed");
        return false;
    }
}

void ensure_fleet_ops_info_defaults() {
    const LONG prior = InterlockedCompareExchange(&g_mod_defaults_state, 1, 0);
    if (prior == 0) {
        bool has_speed = false;
        int speed = 0;
        const bool loaded = query_info_ini_defaults(
            nullptr, nullptr, has_speed, speed);
        if (loaded) {
            g_has_default_game_speed = has_speed;
            g_default_game_speed = speed;
        }
        InterlockedExchange(&g_mod_defaults_state, loaded ? 2 : 0);
        return;
    }
    while (InterlockedCompareExchange(&g_mod_defaults_state, 0, 0) == 1) {
        Sleep(0);
    }
}

const char* make_static_delphi_string(const std::string& value) {
    if (value.empty() || value.size() > kMaximumPathLength) return nullptr;
    const std::size_t allocation_size = 8 + value.size() + 1;
    // Delphi treats a -1 reference count as an immortal string literal. The
    // one allocation deliberately lives until process exit, allowing
    // @LStrAsg to manage every returned destination without crossing heaps.
    auto* allocation = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, allocation_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!allocation) return nullptr;

    const std::int32_t static_reference_count = -1;
    const std::int32_t length = static_cast<std::int32_t>(value.size());
    std::memcpy(allocation, &static_reference_count,
                sizeof(static_reference_count));
    std::memcpy(allocation + 4, &length, sizeof(length));
    std::memcpy(allocation + 8, value.c_str(), value.size() + 1);
    return reinterpret_cast<const char*>(allocation + 8);
}

const char* resolved_settings_directory(const char* normal_directory) {
    ensure_fleet_ops_info_defaults();
    {
        StateLockGuard lock;
        if (g_delphi_settings_directory) return g_delphi_settings_directory;
    }

    std::string resolved;
    bool has_speed = false;
    int speed = 0;
    if (!query_info_ini_defaults(
            normal_directory, &resolved, has_speed, speed) ||
        resolved.empty()) {
        return nullptr;
    }
    const char* replacement = make_static_delphi_string(resolved);
    {
        StateLockGuard lock;
        if (!g_delphi_settings_directory) {
            g_delphi_settings_directory = replacement;
        }
        replacement = g_delphi_settings_directory;
    }
    if (replacement) {
        log_line("Fleet Ops settings directory: " + resolved);
    } else {
        log_line("Fleet Ops SettingsDirectory allocation failed");
    }
    return replacement;
}

void __attribute__((regparm(2))) mod_user_directory_hook(
    void* modification_support, void* result_string) {
    a2fo_call_delphi_two_registers(g_mod_user_directory_hook.gateway,
                                    modification_support, result_string);
    try {
        if (!result_string) return;
        const auto* result = static_cast<const char* const*>(result_string);
        const char* replacement = resolved_settings_directory(*result);
        if (!replacement) return;
        a2fo_call_delphi_two_registers(
            at(g_fleet_ops, kDelphiLStrAsgRva), result_string,
            const_cast<char*>(replacement));
    } catch (...) {
        log_line("Fleet Ops SettingsDirectory override was skipped");
    }
}

void* __attribute__((regparm(1))) fo_settings_get_instance_hook(
    void* settings_class) {
    void* settings = a2fo_call_delphi_one_register(
        g_fo_settings_get_instance_hook.gateway, settings_class);
    if (settings) {
        InterlockedExchangePointer(&g_fo_settings_instance, settings);
    }
    try {
        ensure_fleet_ops_info_defaults();
        if (!g_has_default_game_speed || !settings) return settings;

        auto* bytes = static_cast<std::uint8_t*>(settings);
        const bool first_run = bytes[0x70] != 0;
        InterlockedExchange(&g_fo_settings_first_run, first_run ? 1 : 0);
        if (!first_run || InterlockedCompareExchange(
                              &g_fo_settings_default_applied, 1, 0) != 0) {
            return settings;
        }

        *reinterpret_cast<int*>(bytes + 0x08) = g_default_game_speed;
        a2fo_call_delphi_one_register(
            at(g_fleet_ops, kFoSettingsSaveRva), settings);
        log_line("Fleet Ops first-run Settings.xml game speed saved: " +
                 std::to_string(g_default_game_speed));
    } catch (...) {
        log_line("Fleet Ops first-run Settings.xml default was skipped");
    }
    return settings;
}

struct ShellDisplaySettings {
    int display = 0;
    int width = 0;
    int height = 0;
    bool windowed = false;
};

struct ShellDisplayRuntime {
    HWND window = nullptr;
    DWORD windowed_style = 0;
    DWORD windowed_extended_style = 0;
    bool window_styles_captured = false;
    bool settings_logged = false;
    bool mode_applied = false;
    bool apply_failure_logged = false;
    int window_tree_logs = 0;
    ShellDisplaySettings settings;
};

struct ArmadaWindowSearch {
    DWORD process_id = 0;
    HWND window = nullptr;
    std::uint64_t score = 0;
};

struct ArmadaMonitorSearch {
    int target_index = 0;
    int current_index = 0;
    HMONITOR monitor = nullptr;
};

bool is_armada_main_window(HWND window, DWORD process_id) {
    if (!window || !IsWindowVisible(window)) return false;

    DWORD window_process_id = 0;
    GetWindowThreadProcessId(window, &window_process_id);
    if (window_process_id != process_id) return false;

    char class_name[64]{};
    return GetClassNameA(window, class_name, sizeof(class_name)) > 0 &&
           _stricmp(class_name, "TMainWindowForm") == 0;
}

bool same_shell_display_settings(const ShellDisplaySettings& left,
                                 const ShellDisplaySettings& right) {
    return left.display == right.display &&
           left.width == right.width &&
           left.height == right.height &&
           left.windowed == right.windowed;
}

bool read_shell_display_settings(ShellDisplaySettings& settings) {
    void* instance = InterlockedCompareExchangePointer(
        &g_fo_settings_instance, nullptr, nullptr);
    if (!instance) return false;

    const auto* bytes = static_cast<const std::uint8_t*>(instance);
    settings.display = *reinterpret_cast<const int*>(bytes + 0x30);
    settings.width = *reinterpret_cast<const int*>(bytes + 0x34);
    settings.height = *reinterpret_cast<const int*>(bytes + 0x38);
    settings.windowed = bytes[0x72] != 0;
    if (settings.display < 0 || settings.display > 31 ||
        settings.width < 0 || settings.width > 16384 ||
        settings.height < 0 || settings.height > 16384) {
        return false;
    }
    return true;
}

BOOL CALLBACK find_armada_window(HWND window, LPARAM parameter) {
    auto* search = reinterpret_cast<ArmadaWindowSearch*>(parameter);
    if (!search || !is_armada_main_window(window, search->process_id)) {
        return TRUE;
    }

    RECT rect{};
    if (!GetWindowRect(window, &rect)) return TRUE;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return TRUE;

    std::uint64_t score = static_cast<std::uint64_t>(width) *
                          static_cast<std::uint64_t>(height);
    if (!GetWindow(window, GW_OWNER)) score += (std::uint64_t{1} << 48);
    if (window == GetForegroundWindow()) score += (std::uint64_t{1} << 52);
    if (!search->window || score > search->score) {
        search->window = window;
        search->score = score;
    }
    return TRUE;
}

HWND get_armada_window() {
    const DWORD process_id = GetCurrentProcessId();
    const HWND foreground = GetForegroundWindow();
    if (is_armada_main_window(foreground, process_id)) return foreground;

    ArmadaWindowSearch search;
    search.process_id = process_id;
    EnumWindows(find_armada_window, reinterpret_cast<LPARAM>(&search));
    return search.window;
}

BOOL CALLBACK find_armada_monitor(HMONITOR monitor, HDC, LPRECT,
                                  LPARAM parameter) {
    auto* search = reinterpret_cast<ArmadaMonitorSearch*>(parameter);
    if (!search) return FALSE;
    if (search->current_index++ == search->target_index) {
        search->monitor = monitor;
        return FALSE;
    }
    return TRUE;
}

HMONITOR get_configured_monitor(HWND window, int display) {
    ArmadaMonitorSearch search;
    search.target_index = display;
    EnumDisplayMonitors(nullptr, nullptr, find_armada_monitor,
                        reinterpret_cast<LPARAM>(&search));
    if (search.monitor) return search.monitor;
    return MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
}

void log_armada_window(const char* kind, HWND window) {
    char title[256]{};
    char class_name[128]{};
    GetWindowTextA(window, title, sizeof(title));
    GetClassNameA(window, class_name, sizeof(class_name));
    RECT rect{};
    RECT client{};
    GetWindowRect(window, &rect);
    GetClientRect(window, &client);
    log_line(std::string("Shell display: ") + kind + " window hwnd=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(window)) +
             ", title=\"" + title + "\", class=\"" + class_name +
             "\", outer=" +
             std::to_string(rect.right - rect.left) + "x" +
             std::to_string(rect.bottom - rect.top) + "@" +
             std::to_string(rect.left) + "," + std::to_string(rect.top) +
             ", client=" +
             std::to_string(client.right - client.left) + "x" +
             std::to_string(client.bottom - client.top) +
             ", parent=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(
                 GetParent(window))) +
             ", owner=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(
                 GetWindow(window, GW_OWNER))) +
             ", style=" +
             std::to_string(static_cast<DWORD>(
                 GetWindowLongA(window, GWL_STYLE))));
}

BOOL CALLBACK log_armada_child_window(HWND window, LPARAM) {
    log_armada_window("child", window);
    return TRUE;
}

BOOL CALLBACK log_armada_top_window(HWND window, LPARAM parameter) {
    const DWORD expected_process_id = static_cast<DWORD>(parameter);
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != expected_process_id) return TRUE;
    log_armada_window("top-level", window);
    EnumChildWindows(window, log_armada_child_window, 0);
    return TRUE;
}

void log_armada_window_tree() {
    log_line("Shell display: current Armada window tree follows");
    EnumWindows(log_armada_top_window,
                static_cast<LPARAM>(GetCurrentProcessId()));
}

bool set_window_style(HWND window, int index, DWORD style) {
    SetLastError(ERROR_SUCCESS);
    const LONG previous = SetWindowLongA(window, index,
                                         static_cast<LONG>(style));
    return previous != 0 || GetLastError() == ERROR_SUCCESS;
}

bool same_rect(const RECT& left, const RECT& right) {
    return left.left == right.left && left.top == right.top &&
           left.right == right.right && left.bottom == right.bottom;
}

void capture_window_styles(ShellDisplayRuntime& runtime, HWND window) {
    runtime.window = window;
    runtime.windowed_style =
        static_cast<DWORD>(GetWindowLongA(window, GWL_STYLE));
    runtime.windowed_extended_style =
        static_cast<DWORD>(GetWindowLongA(window, GWL_EXSTYLE));
    runtime.window_styles_captured = true;
    runtime.mode_applied = false;
    runtime.apply_failure_logged = false;
    log_line("Shell display: Armada window found through Win32");
}

bool apply_shell_fullscreen(ShellDisplayRuntime& runtime, HWND window,
                            const MONITORINFO& monitor) {
    const DWORD style =
        (runtime.windowed_style &
         ~(WS_OVERLAPPEDWINDOW | WS_CHILD)) |
        WS_POPUP | WS_VISIBLE;
    const DWORD extended_style =
        runtime.windowed_extended_style &
        ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE |
          WS_EX_STATICEDGE);
    if (static_cast<DWORD>(GetWindowLongA(window, GWL_STYLE)) != style &&
        !set_window_style(window, GWL_STYLE, style)) {
        return false;
    }
    if (static_cast<DWORD>(GetWindowLongA(window, GWL_EXSTYLE)) !=
            extended_style &&
        !set_window_style(window, GWL_EXSTYLE, extended_style)) {
        return false;
    }

    RECT current{};
    if (!GetWindowRect(window, &current)) return false;
    if (!same_rect(current, monitor.rcMonitor) &&
        !SetWindowPos(
            window, HWND_TOP,
            monitor.rcMonitor.left, monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left,
            monitor.rcMonitor.bottom - monitor.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW)) {
        return false;
    }

    if (!runtime.mode_applied) {
        log_line("Shell display: borderless fullscreen applied: " +
                 std::to_string(monitor.rcMonitor.right -
                                monitor.rcMonitor.left) +
                 "x" +
                 std::to_string(monitor.rcMonitor.bottom -
                                monitor.rcMonitor.top));
    }
    return true;
}

bool apply_shell_windowed_size(ShellDisplayRuntime& runtime, HWND window,
                               const MONITORINFO& monitor,
                               const ShellDisplaySettings& settings) {
    const int target_client_width =
        settings.width >= 640 ? settings.width : 1200;
    const int target_client_height =
        settings.height >= 480 ? settings.height : 900;
    DWORD style = runtime.windowed_style;
    if ((style & WS_CAPTION) == 0) {
        style = (style & ~(WS_POPUP | WS_CHILD)) |
                WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    }
    const DWORD extended_style = runtime.windowed_extended_style;
    if (static_cast<DWORD>(GetWindowLongA(window, GWL_STYLE)) != style &&
        !set_window_style(window, GWL_STYLE, style)) {
        return false;
    }
    if (static_cast<DWORD>(GetWindowLongA(window, GWL_EXSTYLE)) !=
            extended_style &&
        !set_window_style(window, GWL_EXSTYLE, extended_style)) {
        return false;
    }

    RECT outer{0, 0, target_client_width, target_client_height};
    if (!AdjustWindowRectEx(&outer, style, FALSE, extended_style)) {
        return false;
    }

    const int outer_width = outer.right - outer.left;
    const int outer_height = outer.bottom - outer.top;
    const int work_width = monitor.rcWork.right - monitor.rcWork.left;
    const int work_height = monitor.rcWork.bottom - monitor.rcWork.top;
    const int left = monitor.rcWork.left + (work_width - outer_width) / 2;
    const int top = monitor.rcWork.top + (work_height - outer_height) / 2;
    const RECT target{left, top, left + outer_width, top + outer_height};
    RECT current{};
    if (!GetWindowRect(window, &current)) return false;
    if (!same_rect(current, target) &&
        !SetWindowPos(window, nullptr, left, top, outer_width, outer_height,
                      SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER |
                          SWP_FRAMECHANGED | SWP_SHOWWINDOW)) {
        return false;
    }

    if (!runtime.mode_applied) {
        log_line("Shell display: configured windowed size applied: " +
                 std::to_string(target_client_width) + "x" +
                 std::to_string(target_client_height));
    }
    return true;
}

void poll_shell_display(ShellDisplayRuntime& runtime) {
    ShellDisplaySettings settings;
    if (!read_shell_display_settings(settings)) return;
    const bool settings_changed =
        !runtime.settings_logged ||
        !same_shell_display_settings(runtime.settings, settings);
    if (settings_changed) {
        runtime.settings = settings;
        runtime.settings_logged = true;
        runtime.mode_applied = false;
        runtime.apply_failure_logged = false;
        log_line("Shell display: graphics settings detected: display=" +
                 std::to_string(settings.display) + ", resolution=" +
                 std::to_string(settings.width) + "x" +
                 std::to_string(settings.height) + ", mode=" +
                 (settings.windowed ? "windowed" : "fullscreen"));
    }

    const HWND window = get_armada_window();
    if (!window || !IsWindow(window)) return;
    if (runtime.window != window || !runtime.window_styles_captured) {
        capture_window_styles(runtime, window);
        if (runtime.window_tree_logs++ < 4) log_armada_window_tree();
    }

    const HMONITOR monitor_handle =
        get_configured_monitor(window, settings.display);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!monitor_handle || !GetMonitorInfoA(monitor_handle, &monitor)) return;

    const bool applied = settings.windowed
        ? apply_shell_windowed_size(runtime, window, monitor, settings)
        : apply_shell_fullscreen(runtime, window, monitor);
    if (applied) {
        runtime.mode_applied = true;
        runtime.apply_failure_logged = false;
    } else if (!runtime.apply_failure_logged) {
        runtime.apply_failure_logged = true;
        log_line("Shell display: window mode application failed (error " +
                 std::to_string(GetLastError()) + ")");
    }
}

void run_shell_display_monitor() {
    ShellDisplayRuntime runtime;
    log_line("Shell display: graphics-settings monitor started");
    for (;;) {
        try {
            poll_shell_display(runtime);
        } catch (...) {
            log_line("Shell display: update skipped after an unexpected C++ "
                     "exception");
        }
        Sleep(100);
    }
}

void __attribute__((fastcall)) game_configuration_new_hook(
    void* game_configuration, void*) {
    a2fo_call_game_object_method(g_game_configuration_new_hook.gateway,
                                 game_configuration);
    try {
        ensure_fleet_ops_info_defaults();
        if (!g_has_default_game_speed || !game_configuration) return;
        *reinterpret_cast<int*>(
            static_cast<std::uint8_t*>(game_configuration) + 4) =
            g_default_game_speed;
        if (InterlockedCompareExchange(
                &g_default_game_speed_logged, 1, 0) == 0) {
            log_line("Fleet Ops first-run game speed default applied: " +
                     std::to_string(g_default_game_speed));
        }
    } catch (...) {
        log_line("Fleet Ops DefaultGameSpeed override was skipped");
    }
}

bool __attribute__((fastcall)) game_configuration_load_profile_hook(
    void* game_configuration, void*) {
    const std::uintptr_t original_result = reinterpret_cast<std::uintptr_t>(
        a2fo_call_game_object_method(
            g_game_configuration_load_profile_hook.gateway,
            game_configuration));
    try {
        ensure_fleet_ops_info_defaults();
        if (g_has_default_game_speed && game_configuration &&
            InterlockedCompareExchange(&g_fo_settings_first_run, 0, 0) != 0) {
            auto* speed = reinterpret_cast<int*>(
                static_cast<std::uint8_t*>(game_configuration) + 4);
            const int previous = *speed;
            *speed = g_default_game_speed;
            if (InterlockedCompareExchange(
                    &g_runtime_game_speed_logged, 1, 0) == 0) {
                log_line("Fleet Ops first-run runtime game speed applied: " +
                         std::to_string(g_default_game_speed) + " (was " +
                         std::to_string(previous) + ")");
            }
        }
    } catch (...) {
        log_line("Fleet Ops runtime DefaultGameSpeed override was skipped");
    }
    return (original_result & 0xffu) != 0;
}

int default_user_profile_game_speed_hook() {
    try {
        ensure_fleet_ops_info_defaults();
        if (g_has_default_game_speed) {
            if (InterlockedCompareExchange(
                    &g_user_profile_game_speed_logged, 1, 0) == 0) {
                log_line("Armada default user-profile game speed supplied: " +
                         std::to_string(g_default_game_speed));
            }
            return g_default_game_speed;
        }
    } catch (...) {
        log_line("Armada default user-profile game-speed override was "
                 "skipped");
    }

    // The supported Fleet Ops ArmadaL.exe returns 5 from the original leaf
    // function. Preserve that behavior when info.ini has no override.
    return 5;
}

bool valid_policy_identifier(const char* value) {
    if (!value || !*value) return false;
    std::size_t length = 0;
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(value);
         *cursor; ++cursor) {
        if (++length > 63 ||
            (!std::isalnum(*cursor) && *cursor != '_' && *cursor != '-')) {
            return false;
        }
    }
    return true;
}

bool valid_odf_field_name(const char* value) {
    if (!value || !*value) return false;
    std::size_t length = 0;
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(value);
         *cursor; ++cursor) {
        if (++length > 127 ||
            (!std::isalnum(*cursor) && *cursor != '_' && *cursor != '-' &&
             *cursor != '.')) {
            return false;
        }
    }
    return true;
}

bool valid_replacement_odf_name(const char* value, std::size_t length) {
    if (!value || length == 0 || length > 255) return false;
    for (std::size_t index = 0; index < length; ++index) {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (!std::isalnum(ch) && ch != '_' && ch != '-' && ch != '.') {
            return false;
        }
    }
    return true;
}

std::string policy_owner(const char* value) {
    return value && *value ? value : "unnamed extension";
}

bool register_classlabel_alias_policy(const char* owner_value,
                                      const char* source_value,
                                      const char* target_value) {
    const std::string owner = policy_owner(owner_value);
    if (!valid_policy_identifier(source_value) ||
        !valid_policy_identifier(target_value)) {
        log_line("Classlabel alias policy rejected for " + owner +
                 "; source and target must be 1-63 character identifiers");
        return false;
    }

    const std::string source = lower_ascii(source_value);
    ClasslabelAliasPolicy policy{lower_ascii(target_value), owner};
    bool registered = false;
    bool registration_open = false;
    std::string existing_owner;
    {
        StateLockGuard lock;
        registration_open = g_policy_registration_open &&
                            g_registration_transaction.active;
        if (registration_open) {
            const auto existing = g_classlabel_aliases.find(source);
            if (existing == g_classlabel_aliases.end()) {
                g_classlabel_aliases.emplace(source, std::move(policy));
                registered = true;
            } else {
                existing_owner = existing->second.owner;
            }
        }
    }

    if (registered) {
        log_line("Classlabel alias policy registered by " + owner + ": " +
                 source + " -> " + lower_ascii(target_value));
    } else if (!registration_open) {
        log_line("Classlabel alias policy rejected for " + owner +
                 "; startup registration is closed");
    } else {
        log_line("Classlabel alias policy rejected for " + owner +
                 "; " + source + " is already owned by " + existing_owner);
    }
    return registered;
}

bool register_evolver_cocoon_policy(const char* owner_value,
                                    const char* command_value) {
    const std::string owner = policy_owner(owner_value);
    if (!valid_policy_identifier(command_value)) {
        log_line("Evolver cocoon policy rejected for " + owner +
                 "; command must be a 1-63 character identifier");
        return false;
    }

    std::string command = lower_ascii(command_value);
    bool registered = false;
    bool registration_open = false;
    std::string existing_owner;
    {
        StateLockGuard lock;
        registration_open = g_policy_registration_open &&
                            g_registration_transaction.active;
        if (registration_open && g_evolver_cocoon_command.empty()) {
            g_evolver_cocoon_command = std::move(command);
            g_evolver_cocoon_owner = owner;
            registered = true;
        } else if (registration_open) {
            existing_owner = g_evolver_cocoon_owner;
        }
    }

    if (registered) {
        log_line("Evolver cocoon policy registered by " + owner + ": " +
                 lower_ascii(command_value));
    } else if (!registration_open) {
        log_line("Evolver cocoon policy rejected for " + owner +
                 "; startup registration is closed");
    } else {
        log_line("Evolver cocoon policy rejected for " + owner +
                 "; already owned by " + existing_owner);
    }
    return registered;
}

void begin_module_registration(const std::string& path) {
    try {
        StateLockGuard lock;
        RegistrationTransaction snapshot;
        snapshot.active = true;
        snapshot.path = path;
        snapshot.fofs_handler = g_fofs_item_lookup_handler;
        snapshot.fofs_user_data = g_fofs_item_lookup_user_data;
        snapshot.fofs_owner = g_fofs_item_lookup_owner;
        snapshot.info_ini_defaults_handler = g_info_ini_defaults_handler;
        snapshot.info_ini_defaults_user_data =
            g_info_ini_defaults_user_data;
        snapshot.info_ini_defaults_owner = g_info_ini_defaults_owner;
        snapshot.classlabel_aliases = g_classlabel_aliases;
        snapshot.cocoon_command = g_evolver_cocoon_command;
        snapshot.cocoon_owner = g_evolver_cocoon_owner;
        snapshot.object_destroyed_handlers = g_object_destroyed_handlers;
        snapshot.destroyed_odf_fields = g_destroyed_odf_fields;
        g_registration_transaction = std::move(snapshot);
    } catch (...) {
        // An empty/inactive transaction still lets the loader fail the module
        // cleanly if one of its registrations is rejected.
        g_registration_transaction = RegistrationTransaction{};
        log_line("Module registration transaction could not start: " + path);
    }
}

void finish_module_registration(const std::string& path, bool initialized) {
    bool rolled_back = false;
    {
        StateLockGuard lock;
        if (!g_registration_transaction.active ||
            g_registration_transaction.path != path) {
            return;
        }
        if (!initialized) {
            g_fofs_item_lookup_handler =
                g_registration_transaction.fofs_handler;
            g_fofs_item_lookup_user_data =
                g_registration_transaction.fofs_user_data;
            g_fofs_item_lookup_owner =
                std::move(g_registration_transaction.fofs_owner);
            g_info_ini_defaults_handler =
                g_registration_transaction.info_ini_defaults_handler;
            g_info_ini_defaults_user_data =
                g_registration_transaction.info_ini_defaults_user_data;
            g_info_ini_defaults_owner = std::move(
                g_registration_transaction.info_ini_defaults_owner);
            g_classlabel_aliases =
                std::move(g_registration_transaction.classlabel_aliases);
            g_evolver_cocoon_command =
                std::move(g_registration_transaction.cocoon_command);
            g_evolver_cocoon_owner =
                std::move(g_registration_transaction.cocoon_owner);
            g_object_destroyed_handlers = std::move(
                g_registration_transaction.object_destroyed_handlers);
            g_destroyed_odf_fields = std::move(
                g_registration_transaction.destroyed_odf_fields);
            rolled_back = true;
        }
        g_registration_transaction = RegistrationTransaction{};
    }
    if (rolled_back) {
        log_line("Module loader: rolled back registrations from " + path);
    }
}

void* __attribute__((regparm(3))) fofs_item_get_hash_lookup_hook(
    void* file_system, void* delphi_name, std::uintptr_t flags) {
    // Patched into the one GetFileFromHashTable call inside FOFS_ItemGet. At
    // this point EAX=file_system, EDX=Delphi filename, and CL=lookup flags.
    // Wait briefly for the post-attach worker so a native module can claim the
    // semantic dispatcher before the first ODF lookup races past it.
    wait_for_deferred_initialization();

    A2FO_FofsItemLookupHandler external_handler = nullptr;
    void* external_user_data = nullptr;
    EnterCriticalSection(&g_state_lock);
    external_handler = g_fofs_item_lookup_handler;
    external_user_data = g_fofs_item_lookup_user_data;
    LeaveCriticalSection(&g_state_lock);
    if (external_handler) {
        void* external_result = nullptr;
        if (external_handler(file_system, delphi_name,
                             static_cast<std::uint32_t>(flags & 0xffu),
                             &external_result, external_user_data)) {
            return external_result;
        }
    }
    return a2fo_call_get_file_from_hash(
        at(g_fleet_ops, kGetFileFromHashTableRva), file_system, delphi_name,
        static_cast<std::uint32_t>(flags & 0xffu));
}

std::string normalize_cocoon_name(const char* value) {
    // ODF authors may omit .sod and add surrounding whitespace. Empty values
    // deliberately mean "use the original Fleet Ops selection".
    if (!value) {
        return {};
    }
    std::string name(value);
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = name.find_last_not_of(" \t\r\n");
    name = name.substr(first, last - first + 1);
    replace_slashes(name);
    const std::string lower = lower_ascii(name);
    if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".sod") != 0) {
        name += ".sod";
    }
    return name;
}

void* original_parameter_db_get_string() {
    if (g_parameter_db_get_string_hook.gateway) {
        return g_parameter_db_get_string_hook.gateway;
    }

    return at(g_armada, kParameterDbGetStringRva);
}

bool lua_parameter_db_get_string(void* parameter_db,
                                 const char* key,
                                 char* output,
                                 std::uint32_t output_size,
                                 const char* default_value) {
    if (!parameter_db || !key || !output || output_size == 0) return false;
    return a2fo_parameter_db_get_string(
        original_parameter_db_get_string(), parameter_db, key, output,
        output_size, default_value ? default_value : "");
}

std::string normalize_odf_basename(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    replace_slashes(value);
    const std::size_t slash = value.find_last_of('\\');
    if (slash != std::string::npos) value.erase(0, slash + 1);
    value = lower_ascii(std::move(value));
    if (value.size() > 4 &&
        value.compare(value.size() - 4, 4, ".odf") == 0) {
        value.resize(value.size() - 4);
    }
    return value;
}

bool read_odf_snapshot_value(void* parameter_db, const char* key,
                             std::string& value) {
    std::array<char, 4096> output{};
    const bool found = a2fo_parameter_db_get_string(
        original_parameter_db_get_string(), parameter_db, key,
        output.data(), static_cast<std::uint32_t>(output.size()), "");
    output.back() = '\0';
    const char* end = static_cast<const char*>(
        std::memchr(output.data(), '\0', output.size()));
    if (!end) return false;
    value.assign(output.data(), static_cast<std::size_t>(end - output.data()));
    return found;
}

void capture_destroyed_object_odf_fields(void* parameter_db) {
    if (!parameter_db || !g_destroyed_odf_load_context) return;

    // `basename` is not an ODF command. Capture only the fields requested by
    // active handlers here; the surrounding GameObjectClass load hook supplies
    // the real basename once Armada has built the class.
    std::vector<std::string> fields;
    {
        StateLockGuard lock;
        fields.assign(g_destroyed_odf_fields.begin(),
                      g_destroyed_odf_fields.end());
    }
    for (const std::string& command : fields) {
        if (command == "basename") continue;
        std::string value;
        if (read_odf_snapshot_value(parameter_db, command.c_str(), value)) {
            g_destroyed_odf_load_context->fields[command] = std::move(value);
        }
    }
}

bool __attribute__((fastcall)) parameter_db_get_string_hook(
    void* self,
    void*,
    const char* key,
    char* output,
    std::uint32_t output_size,
    const char* default_value) {

    const bool found = a2fo_parameter_db_get_string(
        original_parameter_db_get_string(),
        self,
        key,
        output,
        output_size,
        default_value);

    if (!key || !output || output_size == 0) {
        return found;
    }

    wait_for_deferred_initialization();
    try {
        if (_stricmp(key, "classlabel") != 0) return found;
        capture_destroyed_object_odf_fields(self);
        const char* output_end = static_cast<const char*>(
            std::memchr(output, '\0', output_size));
        if (!output_end) return found;

        const std::string source = lower_ascii(
            std::string(output, static_cast<std::size_t>(output_end - output)));
        std::string replacement;
        {
            StateLockGuard lock;
            g_original_classlabels[self] = source;
            if (!a2fo::transform_classlabel(
                    g_lua_host, self, source, replacement)) {
                const auto alias = g_classlabel_aliases.find(source);
                if (alias != g_classlabel_aliases.end()) {
                    replacement = alias->second.target;
                }
            }
        }

        if (!replacement.empty() && replacement.size() + 1 <= output_size) {
            std::memcpy(output, replacement.c_str(), replacement.size() + 1);
            log_line("Classlabel alias applied: " + source + " -> " +
                     replacement);
        }
    } catch (...) {
        log_line("ParameterDB string policy skipped after an unexpected "
                 "C++ exception");
    }

    return found;
}

void* A2FO_CALL game_object_class_find_project_id_hook(
    const std::uint32_t* project_id) {
    DestroyedOdfLoadContext context;
    DestroyedOdfLoadContext* previous_context =
        g_destroyed_odf_load_context;
    g_destroyed_odf_load_context = &context;

    using FindProjectIdFunction = void* (A2FO_CALL*)(const std::uint32_t*);
    const auto original = reinterpret_cast<FindProjectIdFunction>(
        g_game_object_class_find_project_id_hook.gateway);
    void* object_class = original(project_id);

    g_destroyed_odf_load_context = previous_context;
    if (!object_class || context.fields.empty()) return object_class;

    const char* odf_name = static_cast<const char*>(
        a2fo_call_game_object_method(
            at(g_armada, kGameObjectClassGetOdfNameRva), object_class));
    if (!odf_name) return object_class;
    const char* odf_end = static_cast<const char*>(
        std::memchr(odf_name, '\0', 256));
    if (!odf_end) return object_class;

    const std::string basename(
        odf_name, static_cast<std::size_t>(odf_end - odf_name));
    const std::string key = normalize_odf_basename(basename);
    if (key.empty()) return object_class;

    std::string field_names;
    for (const auto& field : context.fields) {
        if (!field_names.empty()) field_names += ", ";
        field_names += field.first;
    }
    context.fields["basename"] = basename;
    {
        StateLockGuard lock;
        g_odf_snapshots[key] = std::move(context.fields);
    }
    log_line("Destroyed-object ODF fields cached: " + basename +
             " (" + field_names + ")");
    return object_class;
}

struct Matrix34Snapshot {
    std::array<float, 12> values{};
};

struct DestroyedObjectSnapshot {
    bool valid = false;
    std::string source_odf;
    Matrix34Snapshot transform;
    int source_team = 0;
    std::uint32_t source_handle = 0;
    a2fo::LuaObjectDestroyedEvent lua_event;
};

std::uint32_t hash_destroyed_object(const std::string& odf,
                                    std::uint32_t handle,
                                    int team,
                                    const Matrix34Snapshot& transform) {
    std::uint32_t hash = 2166136261u;
    const auto append = [&hash](const void* data, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        for (std::size_t index = 0; index < size; ++index) {
            hash ^= bytes[index];
            hash *= 16777619u;
        }
    };
    append(odf.data(), odf.size());
    append(&handle, sizeof(handle));
    append(&team, sizeof(team));
    append(transform.values.data(), transform.values.size() * sizeof(float));
    return hash;
}

DestroyedObjectSnapshot snapshot_destroyed_object(void* self) {
    DestroyedObjectSnapshot snapshot;
    if (!self) {
        log_line("Object-destroyed snapshot unavailable: null Craft pointer");
        return snapshot;
    }

    auto* bytes = static_cast<std::uint8_t*>(self);
    if (bytes[0x113] != 0) {
        log_line("Object-destroyed snapshot unavailable: Craft explosion "
                 "state was already " + std::to_string(bytes[0x113]));
        return snapshot;
    }
    void* object_class = *reinterpret_cast<void**>(bytes + 0x40);
    if (!object_class) {
        log_line("Object-destroyed snapshot unavailable: Craft class was null");
        return snapshot;
    }

    const char* odf_name = static_cast<const char*>(
        a2fo_call_game_object_method(
            at(g_armada, kGameObjectClassGetOdfNameRva), object_class));
    if (!odf_name) {
        log_line("Object-destroyed snapshot unavailable: class ODF name was "
                 "null");
        return snapshot;
    }
    const char* odf_end = static_cast<const char*>(
        std::memchr(odf_name, '\0', 256));
    if (!odf_end) {
        log_line("Object-destroyed snapshot unavailable: class ODF name was "
                 "not terminated");
        return snapshot;
    }
    snapshot.source_odf.assign(
        odf_name, static_cast<std::size_t>(odf_end - odf_name));
    const std::string normalized_odf =
        normalize_odf_basename(snapshot.source_odf);
    if (normalized_odf.empty()) {
        log_line("Object-destroyed snapshot unavailable: class ODF name was "
                 "empty");
        return snapshot;
    }

    const void* transform = a2fo_call_game_object_method(
        at(g_armada, kGameObjectGetTransformRva), self);
    if (!transform) {
        log_line("Object-destroyed snapshot unavailable: " +
                 snapshot.source_odf + " had no transform");
        return snapshot;
    }
    std::memcpy(snapshot.transform.values.data(), transform,
                snapshot.transform.values.size() * sizeof(float));
    snapshot.source_team = *reinterpret_cast<const int*>(bytes + 0xec);
    snapshot.source_handle =
        *reinterpret_cast<const std::uint32_t*>(bytes + 0x28);

    snapshot.lua_event.odf["basename"] = snapshot.source_odf;
    {
        StateLockGuard lock;
        const auto cached = g_odf_snapshots.find(normalized_odf);
        if (cached != g_odf_snapshots.end()) {
            snapshot.lua_event.odf = cached->second;
        }
    }
    snapshot.lua_event.random_seed = hash_destroyed_object(
        normalized_odf, snapshot.source_handle, snapshot.source_team,
        snapshot.transform);
    snapshot.valid = true;
    log_line("Object-destroyed snapshot ready: " + snapshot.source_odf +
             " (" + std::to_string(snapshot.lua_event.odf.size()) +
             " ODF field" +
             (snapshot.lua_event.odf.size() == 1 ? "" : "s") + ")");
    return snapshot;
}

bool resolve_native_object_destroyed(
    const DestroyedObjectSnapshot& source,
    a2fo::LuaObjectReplacement& selected_replacement) {
    std::vector<NativeObjectDestroyedRegistration> handlers;
    {
        StateLockGuard lock;
        handlers = g_object_destroyed_handlers;
    }
    if (handlers.empty()) return false;

    std::vector<A2FO_OdfFieldView> fields;
    fields.reserve(source.lua_event.odf.size());
    for (const auto& field : source.lua_event.odf) {
        fields.push_back({
            {field.first.data(), static_cast<std::uint32_t>(field.first.size())},
            {field.second.data(), static_cast<std::uint32_t>(field.second.size())},
        });
    }

    A2FO_ObjectDestroyedEvent event{};
    event.struct_size = sizeof(event);
    event.source_odf = {
        source.source_odf.data(),
        static_cast<std::uint32_t>(source.source_odf.size())};
    event.odf_fields = fields.empty() ? nullptr : fields.data();
    event.odf_field_count = static_cast<std::uint32_t>(fields.size());
    std::copy(source.transform.values.begin(), source.transform.values.end(),
              event.transform);
    event.source_team = source.source_team;
    event.source_handle = source.source_handle;
    event.random_seed = source.lua_event.random_seed;

    for (std::size_t index = 0; index < handlers.size(); ++index) {
        const NativeObjectDestroyedRegistration& registration = handlers[index];
        if (!registration.enabled || !registration.handler) continue;

        A2FO_ObjectReplacement replacement{};
        replacement.struct_size = sizeof(replacement);
        replacement.flags = A2FO_REPLACEMENT_INHERIT_POSITION |
                            A2FO_REPLACEMENT_INHERIT_ROTATION;
        replacement.owner = A2FO_REPLACEMENT_OWNER_NEUTRAL;
        bool claimed = false;
        try {
            claimed = registration.handler(
                &event, &replacement, registration.user_data);
        } catch (...) {
            log_line("Native object-destroyed handler threw and was disabled: " +
                     registration.owner);
            StateLockGuard lock;
            if (index < g_object_destroyed_handlers.size()) {
                g_object_destroyed_handlers[index].enabled = false;
            }
            continue;
        }
        if (!claimed) continue;

        const char* odf_end = replacement.odf
            ? static_cast<const char*>(std::memchr(replacement.odf, '\0', 256))
            : nullptr;
        const std::uint32_t known_flags =
            A2FO_REPLACEMENT_INHERIT_POSITION |
            A2FO_REPLACEMENT_INHERIT_ROTATION;
        const std::size_t odf_length = odf_end
            ? static_cast<std::size_t>(odf_end - replacement.odf) : 0;
        if (replacement.struct_size < sizeof(A2FO_ObjectReplacement) ||
            !odf_end ||
            !valid_replacement_odf_name(replacement.odf, odf_length) ||
            (replacement.flags & ~known_flags) != 0 ||
            replacement.owner > A2FO_REPLACEMENT_OWNER_ORIGINAL) {
            log_line("Native object-destroyed handler returned an invalid "
                     "replacement: " + registration.owner);
            continue;
        }

        selected_replacement.odf.assign(
            replacement.odf, odf_length);
        selected_replacement.inherit_position =
            (replacement.flags & A2FO_REPLACEMENT_INHERIT_POSITION) != 0;
        selected_replacement.inherit_rotation =
            (replacement.flags & A2FO_REPLACEMENT_INHERIT_ROTATION) != 0;
        selected_replacement.owner =
            replacement.owner == A2FO_REPLACEMENT_OWNER_ORIGINAL
                ? a2fo::LuaReplacementOwner::Original
                : a2fo::LuaReplacementOwner::Neutral;
        return true;
    }
    return false;
}

void spawn_object_replacement(const DestroyedObjectSnapshot& source,
                              const a2fo::LuaObjectReplacement& replacement) {
    using FindFunction = void* (A2FO_CALL*)(const char*);
    const auto find_object_class = reinterpret_cast<FindFunction>(
        at(g_armada, kGameObjectClassFindRva));
    void* object_class = find_object_class(replacement.odf.c_str());
    if (!object_class) {
        log_line("Object replacement ODF was not loaded: " +
                 replacement.odf);
        return;
    }

    Matrix34Snapshot transform;
    transform.values[0] = 1.0f;
    transform.values[4] = 1.0f;
    transform.values[8] = 1.0f;
    if (replacement.inherit_rotation) {
        std::copy_n(source.transform.values.begin(), 9,
                    transform.values.begin());
    }
    if (replacement.inherit_position) {
        std::copy_n(source.transform.values.begin() + 9, 3,
                    transform.values.begin() + 9);
    }

    const int team = replacement.owner == a2fo::LuaReplacementOwner::Original
        ? source.source_team : 0;
    void* object = a2fo_call_game_object_construct(
        at(g_armada, kGameObjectClassConstructRva), object_class,
        &transform, team, 0, nullptr);
    if (!object) {
        log_line("Object replacement could not construct: " +
                 replacement.odf);
        return;
    }
    using GetCurrentMissionFunction = void* (A2FO_CALL*)();
    const auto get_current_mission =
        reinterpret_cast<GetCurrentMissionFunction>(
            at(g_armada, kAiMissionGetCurrentRva));
    void* mission = get_current_mission();
    if (!mission) {
        log_line("Object replacement was constructed without an active "
                 "mission: " + replacement.odf);
        return;
    }
    void** mission_vtable = *reinterpret_cast<void***>(mission);
    if (!mission_vtable || !mission_vtable[6]) {
        log_line("Object replacement could not reach AiMission::AddObject: " +
                 replacement.odf);
        return;
    }
    a2fo_call_game_object_argument(mission_vtable[6], mission, object);
    log_line("Object replacement: " + source.source_odf + " -> " +
             replacement.odf);
}

void __attribute__((fastcall)) craft_explode_hook(void* self, void*) {
    wait_for_deferred_initialization();
    DestroyedObjectSnapshot source;
    try {
        source = snapshot_destroyed_object(self);
    } catch (...) {
        log_line("Object-destroyed snapshot skipped after an unexpected "
                 "C++ exception");
    }

    a2fo_call_craft_explode(g_craft_explode_hook.gateway, self);
    if (!source.valid) return;

    try {
        a2fo::LuaObjectReplacement replacement;
        bool replace = resolve_native_object_destroyed(source, replacement);
        if (!replace) {
            StateLockGuard lock;
            replace = a2fo::resolve_object_destroyed(
                g_lua_host, source.lua_event, replacement);
        }
        if (replace) {
            log_line("Object-destroyed event claimed: " +
                     source.source_odf + " -> " + replacement.odf);
            spawn_object_replacement(source, replacement);
        } else if (source.lua_event.odf.size() > 1) {
            log_line("Object-destroyed event was not claimed for configured "
                     "ODF: " + source.source_odf);
        }
    } catch (...) {
        log_line("Object-destroyed callback skipped after an unexpected "
                 "C++ exception");
    }
}

bool install_object_destroyed_hook() {
    if (!g_armada ||
        std::memcmp(at(g_armada, kAiMissionGetCurrentRva),
                    kExpectedAiMissionGetCurrent,
                    sizeof(kExpectedAiMissionGetCurrent)) != 0 ||
        std::memcmp(at(g_armada, kCraftExplodeRva),
                    kExpectedCraftExplode,
                    sizeof(kExpectedCraftExplode)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassFindProjectIdRva),
                    kExpectedGameObjectClassFindProjectId,
                    sizeof(kExpectedGameObjectClassFindProjectId)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassFindRva),
                    kExpectedGameObjectClassFind,
                    sizeof(kExpectedGameObjectClassFind)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassConstructRva),
                    kExpectedGameObjectClassConstruct,
                    sizeof(kExpectedGameObjectClassConstruct)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassGetOdfNameRva),
                    kExpectedGameObjectClassGetOdfName,
                    sizeof(kExpectedGameObjectClassGetOdfName)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectGetTransformRva),
                    kExpectedGameObjectGetTransform,
                    sizeof(kExpectedGameObjectGetTransform)) != 0) {
        log_line("Object-destroyed hook signature mismatch; object "
                 "replacement dispatch disabled");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_armada, kGameObjectClassFindProjectIdRva),
            reinterpret_cast<void*>(&game_object_class_find_project_id_hook),
            sizeof(kExpectedGameObjectClassFindProjectId),
            kExpectedGameObjectClassFindProjectId,
            g_game_object_class_find_project_id_hook)) {
        log_line("Could not install destroyed-object ODF field capture hook");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_armada, kCraftExplodeRva),
            reinterpret_cast<void*>(&craft_explode_hook),
            sizeof(kExpectedCraftExplode), kExpectedCraftExplode,
            g_craft_explode_hook)) {
        log_line("Could not install object-destroyed hook");
        return false;
    }
    log_line("Object-destroyed dispatcher and ODF field capture enabled");
    return true;
}

bool associate_evolver_cocoon_class(void* class_object,
                                    void* parameter_db,
                                    const char* class_kind);

void* __attribute__((fastcall)) evolver_class_build_class_hook(
    void* self, void*, void* parameter_db) {
    // Let Armada construct the complete class first, then associate the final
    // class object returned in EAX with its optional cocoon setting.
    void* result = a2fo_call_evolver_build_class(g_build_class_hook.gateway,
                                                  self, parameter_db);
    wait_for_deferred_initialization();
    associate_evolver_cocoon_class(result, parameter_db, "EvolverClass");
    return result;
}

bool associate_evolver_cocoon_class(void* class_object,
                                    void* parameter_db,
                                    const char* class_kind) {
    if (!class_object || !parameter_db || !g_state_lock_ready) return false;

    char basename[MAX_PATH]{};
    std::string value;
    std::string command;
    try {
        StateLockGuard lock;
        a2fo::resolve_evolver_cocoon(
            g_lua_host, parameter_db, value);
        command = g_evolver_cocoon_command;
    } catch (...) {
        log_line("Evolver cocoon policy lookup failed; using Fleet Ops default");
    }

    if (parameter_db && value.empty() && !command.empty()) {
        char native_value[MAX_PATH]{};
        a2fo_parameter_db_get_string(original_parameter_db_get_string(),
                                     parameter_db, command.c_str(), native_value,
                                     sizeof(native_value), "");
        value = native_value;
    }
    if (parameter_db) {
        a2fo_parameter_db_get_string(original_parameter_db_get_string(),
                                     parameter_db, "basename", basename,
                                     sizeof(basename), "<unnamed>");
    }
    const std::string cocoon = normalize_cocoon_name(value.c_str());
    try {
        StateLockGuard lock;
        if (cocoon.empty()) {
            g_class_cocoons.erase(class_object);
        } else {
            g_class_cocoons[class_object] = cocoon;
        }
    } catch (...) {
        log_line("Cocoon class association could not retain its policy; "
                 "using Fleet Ops default");
        return false;
    }
    log_line(std::string(class_kind ? class_kind : "EvolverClass") + " " +
             std::string(basename) + " cocoon: " +
             (cocoon.empty() ? "<Fleet Ops default>" : cocoon));
    return true;
}

void* __attribute__((fastcall)) evolver_class_dtor_hook(void* self, void*) {
    // Class pointers can be reused after destruction; erase all pointer-keyed
    // state so a later class cannot inherit an old cocoon choice.
    EnterCriticalSection(&g_state_lock);
    g_class_cocoons.erase(self);
    g_logged_cocoon_classes.erase(self);
    LeaveCriticalSection(&g_state_lock);
    return a2fo_call_evolver_dtor(g_dtor_hook.gateway, self);
}

bool install_classlabel_alias_hook() {
    if (!g_armada) {
        log_line("ArmadaL.exe is unavailable; ParameterDB string policies "
                 "disabled");
        return false;
    }

    if (std::memcmp(
            at(g_armada, kParameterDbGetStringRva),
            kExpectedParameterDbGetString,
            sizeof(kExpectedParameterDbGetString)) != 0) {

        log_line(
            "ParameterDB GetString signature mismatch; "
            "string policies disabled");
        return false;
    }

    if (!a2fo::install_inline_hook(
            at(g_armada, kParameterDbGetStringRva),
            reinterpret_cast<void*>(&parameter_db_get_string_hook),
            sizeof(kExpectedParameterDbGetString),
            kExpectedParameterDbGetString,
            g_parameter_db_get_string_hook)) {

        log_line("Could not install ParameterDB GetString hook");
        return false;
    }

    log_line("ParameterDB string policy hook enabled");
    return true;
}

bool install_evolver_hooks() {
    if (std::memcmp(at(g_armada, kEvolverClassBuildClassRva),
                    kExpectedBuildClass, sizeof(kExpectedBuildClass)) != 0 ||
        std::memcmp(at(g_armada, kEvolverClassDtorRva), kExpectedDtor,
                    sizeof(kExpectedDtor)) != 0 ||
        std::memcmp(at(g_armada, kCocoonSelectorRva), kExpectedCocoonJump,
                    sizeof(kExpectedCocoonJump)) != 0 ||
        std::memcmp(at(g_armada, kCocoonUpdateSelectorRva),
                    kExpectedCocoonUpdateSelector,
                    sizeof(kExpectedCocoonUpdateSelector)) != 0) {
        log_line("Evolver hook signature mismatch; no evolver hooks installed");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_armada, kEvolverClassBuildClassRva),
            reinterpret_cast<void*>(&evolver_class_build_class_hook),
            sizeof(kExpectedBuildClass), kExpectedBuildClass,
            g_build_class_hook)) {
        log_line("EvolverClass BuildClass hook signature mismatch");
        return false;
    }
    if (!a2fo::install_inline_hook(at(g_armada, kEvolverClassDtorRva),
                                   reinterpret_cast<void*>(&evolver_class_dtor_hook),
                                   sizeof(kExpectedDtor), kExpectedDtor, g_dtor_hook)) {
        log_line("EvolverClass destructor hook signature mismatch");
        return false;
    }
    if (!a2fo::patch_jump(at(g_armada, kCocoonSelectorRva),
                          reinterpret_cast<void*>(&a2fo_cocoon_selector_hook),
                          kExpectedCocoonJump, sizeof(kExpectedCocoonJump))) {
        log_line("Cocoon selector hook signature mismatch");
        return false;
    }
    if (!a2fo::patch_jump(
            at(g_armada, kCocoonUpdateSelectorRva),
            reinterpret_cast<void*>(&a2fo_cocoon_update_selector_hook),
            kExpectedCocoonUpdateSelector,
            sizeof(kExpectedCocoonUpdateSelector))) {
        log_line("Cocoon update selector hook signature mismatch");
        return false;
    }
    log_line("Evolver policy hooks enabled");
    return true;
}

bool install_fofs_item_get_lookup_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kFofsItemGetHashLookupCallRva),
                    kExpectedFofsItemGetHashLookupCall,
                    sizeof(kExpectedFofsItemGetHashLookupCall)) != 0 ||
        std::memcmp(at(g_fleet_ops, kFofsItemLocateHashLookupCallRva),
                    kExpectedFofsItemLocateHashLookupCall,
                    sizeof(kExpectedFofsItemLocateHashLookupCall)) != 0 ||
        std::memcmp(at(g_fleet_ops, kFofsItemExistsHashLookupCallRva),
                    kExpectedFofsItemExistsHashLookupCall,
                    sizeof(kExpectedFofsItemExistsHashLookupCall)) != 0 ||
        std::memcmp(at(g_fleet_ops, kFofsProjectIdHashLookupCallRva),
                    kExpectedFofsProjectIdHashLookupCall,
                    sizeof(kExpectedFofsProjectIdHashLookupCall)) != 0) {
        log_line("Fleet Operations ODF lookup call signature mismatch");
        return false;
    }
    if (!a2fo::patch_call(
            at(g_fleet_ops, kFofsItemGetHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsItemGetHashLookupCall,
            sizeof(kExpectedFofsItemGetHashLookupCall)) ||
        !a2fo::patch_call(
            at(g_fleet_ops, kFofsItemLocateHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsItemLocateHashLookupCall,
            sizeof(kExpectedFofsItemLocateHashLookupCall)) ||
        !a2fo::patch_call(
            at(g_fleet_ops, kFofsItemExistsHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsItemExistsHashLookupCall,
            sizeof(kExpectedFofsItemExistsHashLookupCall)) ||
        !a2fo::patch_call(
            at(g_fleet_ops, kFofsProjectIdHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsProjectIdHashLookupCall,
            sizeof(kExpectedFofsProjectIdHashLookupCall))) {
        log_line("Could not install Fleet Operations ODF lookup hooks");
        return false;
    }
    log_line("FOFS item lookup dispatcher enabled for get, locate, exists, "
             "and project IDs");
    return true;
}

bool install_mod_user_directory_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kGetModUserDirectoryRva),
                    kExpectedGetModUserDirectory,
                    sizeof(kExpectedGetModUserDirectory)) != 0) {
        log_line("Fleet Operations mod user-directory signature mismatch; "
                 "SettingsDirectory disabled");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_fleet_ops, kGetModUserDirectoryRva),
            reinterpret_cast<void*>(&mod_user_directory_hook),
            sizeof(kExpectedGetModUserDirectory),
            kExpectedGetModUserDirectory, g_mod_user_directory_hook)) {
        log_line("Could not install Fleet Operations SettingsDirectory hook");
        return false;
    }
    log_line("Fleet Operations info.ini SettingsDirectory hook enabled");
    return true;
}

bool install_fo_settings_default_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kFoSettingsGetInstanceRva),
                    kExpectedFoSettingsGetInstance,
                    sizeof(kExpectedFoSettingsGetInstance)) != 0) {
        log_line("Fleet Operations settings-singleton signature mismatch; "
                 "Settings.xml DefaultGameSpeed disabled");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_fleet_ops, kFoSettingsGetInstanceRva),
            reinterpret_cast<void*>(&fo_settings_get_instance_hook),
            sizeof(kExpectedFoSettingsGetInstance),
            kExpectedFoSettingsGetInstance,
            g_fo_settings_get_instance_hook)) {
        log_line("Could not install Fleet Operations Settings.xml default "
                 "hook");
        return false;
    }
    log_line("Fleet Operations first-run Settings.xml default hook enabled");
    return true;
}

bool install_default_game_speed_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kGameConfigurationNewRva),
                    kExpectedGameConfigurationNew,
                    sizeof(kExpectedGameConfigurationNew)) != 0) {
        log_line("Fleet Operations game-configuration signature mismatch; "
                 "DefaultGameSpeed disabled");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_fleet_ops, kGameConfigurationNewRva),
            reinterpret_cast<void*>(&game_configuration_new_hook),
            sizeof(kExpectedGameConfigurationNew),
            kExpectedGameConfigurationNew,
            g_game_configuration_new_hook)) {
        log_line("Could not install Fleet Operations DefaultGameSpeed hook");
        return false;
    }
    log_line("Fleet Operations info.ini DefaultGameSpeed hook enabled");
    return true;
}

bool install_runtime_game_speed_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kGameConfigurationLoadProfileRva),
                    kExpectedGameConfigurationLoadProfile,
                    sizeof(kExpectedGameConfigurationLoadProfile)) != 0) {
        log_line("Fleet Operations profile-load signature mismatch; runtime "
                 "DefaultGameSpeed disabled");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_fleet_ops, kGameConfigurationLoadProfileRva),
            reinterpret_cast<void*>(&game_configuration_load_profile_hook),
            sizeof(kExpectedGameConfigurationLoadProfile),
            kExpectedGameConfigurationLoadProfile,
            g_game_configuration_load_profile_hook)) {
        log_line("Could not install Fleet Operations runtime "
                 "DefaultGameSpeed hook");
        return false;
    }
    log_line("Fleet Operations runtime DefaultGameSpeed hook enabled");
    return true;
}

bool install_user_profile_game_speed_hook() {
    if (!g_armada ||
        std::memcmp(at(g_armada, kDefaultUserProfileGameSpeedRva),
                    kExpectedDefaultUserProfileGameSpeed,
                    sizeof(kExpectedDefaultUserProfileGameSpeed)) != 0) {
        log_line("Armada default user-profile game-speed signature mismatch; "
                 "DefaultGameSpeed supplier disabled");
        return false;
    }
    if (!a2fo::patch_jump(
            at(g_armada, kDefaultUserProfileGameSpeedRva),
            reinterpret_cast<void*>(&default_user_profile_game_speed_hook),
            kExpectedDefaultUserProfileGameSpeed,
            sizeof(kExpectedDefaultUserProfileGameSpeed))) {
        log_line("Could not install Armada default user-profile game-speed "
                 "hook");
        return false;
    }
    log_line("Armada default user-profile game-speed hook enabled");
    return true;
}


void A2FO_CALL module_log(const char* module_name, const char* message) {
    const std::string name = module_name && *module_name ? module_name : "module";
    const std::string text = message ? message : "";
    log_line("[" + name + "] " + text);
}

void* A2FO_CALL api_armada_module() {
    return g_armada;
}

void* A2FO_CALL api_fleetops_module() {
    return g_fleet_ops;
}

const char* A2FO_CALL api_root_directory() {
    return g_root_directory.c_str();
}

bool A2FO_CALL api_install_inline_hook(
    void* target, void* replacement, std::size_t length,
    const std::uint8_t* expected, A2FO_InlineHook* output) {
    if (!output) {
        return false;
    }
    a2fo::InlineHook hook;
    if (!a2fo::install_inline_hook(target, replacement, length, expected, hook)) {
        return false;
    }
    output->target = hook.target;
    output->gateway = hook.gateway;
    output->length = hook.length;
    return true;
}

bool A2FO_CALL api_patch_jump(void* target, void* replacement,
                              const std::uint8_t* expected,
                              std::size_t length) {
    return a2fo::patch_jump(target, replacement, expected, length);
}

bool A2FO_CALL api_patch_call(void* target, void* replacement,
                              const std::uint8_t* expected,
                              std::size_t length) {
    return a2fo::patch_call(target, replacement, expected, length);
}

bool A2FO_CALL api_register_fofs_item_lookup_handler(
    const char* module_name, A2FO_FofsItemLookupHandler handler,
    void* user_data) {
    if (!handler || !g_state_lock_ready) {
        return false;
    }
    const std::string owner =
        module_name && *module_name ? module_name : "unnamed module";
    bool registered = false;
    EnterCriticalSection(&g_state_lock);
    if (g_policy_registration_open &&
        g_registration_transaction.active &&
        !g_fofs_item_lookup_handler) {
        g_fofs_item_lookup_handler = handler;
        g_fofs_item_lookup_user_data = user_data;
        g_fofs_item_lookup_owner = owner;
        registered = true;
    }
    const std::string existing_owner = g_fofs_item_lookup_owner;
    LeaveCriticalSection(&g_state_lock);
    if (registered) {
        log_line("FOFS item lookup handler registered by " + owner);
    } else {
        log_line("FOFS item lookup handler rejected for " + owner +
                 "; already owned by " + existing_owner);
    }
    return registered;
}

bool A2FO_CALL api_register_info_ini_defaults_handler(
    const char* module_name, A2FO_InfoIniDefaultsHandler handler,
    void* user_data) {
    if (!handler || !g_state_lock_ready) return false;
    const std::string owner = policy_owner(module_name);
    bool registered = false;
    std::string existing_owner;
    {
        StateLockGuard lock;
        if (g_policy_registration_open &&
            g_registration_transaction.active &&
            !g_info_ini_defaults_handler) {
            g_info_ini_defaults_handler = handler;
            g_info_ini_defaults_user_data = user_data;
            g_info_ini_defaults_owner = owner;
            registered = true;
        }
        existing_owner = g_info_ini_defaults_owner;
    }
    if (registered) {
        log_line("info.ini defaults handler registered by " + owner);
    } else {
        log_line("info.ini defaults handler rejected for " + owner +
                 "; already owned by " + existing_owner);
    }
    return registered;
}

bool A2FO_CALL api_register_classlabel_alias(
    const char* module_name, const char* source, const char* target) {
    try {
        return register_classlabel_alias_policy(module_name, source, target);
    } catch (...) {
        log_line("Classlabel alias policy registration failed after an "
                 "unexpected C++ exception");
        return false;
    }
}

bool A2FO_CALL api_register_evolver_cocoon_command(
    const char* module_name, const char* command) {
    try {
        return register_evolver_cocoon_policy(module_name, command);
    } catch (...) {
        log_line("Evolver cocoon policy registration failed after an "
                 "unexpected C++ exception");
        return false;
    }
}

bool A2FO_CALL api_register_object_destroyed_handler(
    const char* module_name,
    const char* const* required_odf_fields,
    std::uint32_t required_odf_field_count,
    A2FO_ObjectDestroyedHandler handler,
    void* user_data) {
    const std::string owner = policy_owner(module_name);
    if (!handler || required_odf_field_count > 64 ||
        (required_odf_field_count != 0 && !required_odf_fields)) {
        log_line("Object-destroyed handler rejected for " + owner +
                 "; invalid callback or field list");
        return false;
    }

    NativeObjectDestroyedRegistration registration;
    registration.owner = owner;
    registration.handler = handler;
    registration.user_data = user_data;
    try {
        std::set<std::string> unique_fields;
        for (std::uint32_t index = 0;
             index < required_odf_field_count; ++index) {
            const char* field = required_odf_fields[index];
            if (!valid_odf_field_name(field)) {
                log_line("Object-destroyed handler rejected for " + owner +
                         "; ODF fields must be 1-127 character identifiers");
                return false;
            }
            unique_fields.insert(lower_ascii(field));
        }
        registration.required_odf_fields.assign(
            unique_fields.begin(), unique_fields.end());
    } catch (...) {
        log_line("Object-destroyed handler rejected for " + owner +
                 "; could not copy registration data");
        return false;
    }

    bool registered = false;
    {
        StateLockGuard lock;
        if (g_policy_registration_open &&
            g_registration_transaction.active &&
            g_object_destroyed_handlers.size() < 64) {
            for (const std::string& field :
                 registration.required_odf_fields) {
                g_destroyed_odf_fields.insert(field);
            }
            g_object_destroyed_handlers.push_back(std::move(registration));
            registered = true;
        }
    }
    if (registered) {
        log_line("Object-destroyed handler registered by " + owner);
    } else {
        log_line("Object-destroyed handler rejected for " + owner +
                 "; registration is closed or the handler limit was reached");
    }
    return registered;
}

std::uint32_t A2FO_CALL api_extension_root_count() {
    return static_cast<std::uint32_t>(g_extension_roots.size());
}

const char* A2FO_CALL api_extension_root(std::uint32_t index) {
    if (index >= g_extension_roots.size()) return nullptr;
    return g_extension_roots[index].c_str();
}

std::uint32_t A2FO_CALL api_upgrade_pod_maximum_tier() {
    return g_lua_host.upgrade_pod_maximum_tier;
}

bool A2FO_CALL api_get_original_classlabel(
    void* parameter_db, char* output, std::uint32_t output_size) {
    if (!parameter_db || !output || output_size == 0 ||
        !g_state_lock_ready) {
        return false;
    }
    output[0] = '\0';
    try {
        StateLockGuard lock;
        const auto found = g_original_classlabels.find(parameter_db);
        if (found == g_original_classlabels.end() ||
            found->second.size() + 1 > output_size) {
            return false;
        }
        std::memcpy(output, found->second.c_str(), found->second.size() + 1);
        return true;
    } catch (...) {
        output[0] = '\0';
        return false;
    }
}

bool A2FO_CALL api_associate_evolver_cocoon_class(
    void* class_object, void* parameter_db) {
    if (!g_evolver_hooks_ready) return false;
    try {
        return associate_evolver_cocoon_class(
            class_object, parameter_db, "Hybrid class");
    } catch (...) {
        log_line("Hybrid cocoon class association failed; using Fleet Ops "
                 "default");
        return false;
    }
}

A2FO_ModuleApi make_module_api() {
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &module_log;
    api.armada_module = &api_armada_module;
    api.fleetops_module = &api_fleetops_module;
    api.root_directory = &api_root_directory;
    api.install_inline_hook = &api_install_inline_hook;
    api.patch_jump = &api_patch_jump;
    api.patch_call = &api_patch_call;
    api.register_fofs_item_lookup_handler =
        &api_register_fofs_item_lookup_handler;
    api.extension_root_count = &api_extension_root_count;
    api.extension_root = &api_extension_root;
    api.register_classlabel_alias = &api_register_classlabel_alias;
    api.register_evolver_cocoon_command =
        &api_register_evolver_cocoon_command;
    api.api_revision = A2FO_MODULE_API_REVISION;
    api.capabilities = A2FO_CAP_OBJECT_DESTROYED_DISPATCH |
        A2FO_CAP_UPGRADE_POD_POLICY |
        A2FO_CAP_ORIGINAL_CLASSLABEL;
    if (g_evolver_hooks_ready) {
        api.capabilities |= A2FO_CAP_COCOON_CLASS_ASSOCIATION;
    }
    api.capabilities |= A2FO_CAP_INFO_INI_DEFAULTS;
    api.register_object_destroyed_handler =
        &api_register_object_destroyed_handler;
    api.upgrade_pod_maximum_tier = &api_upgrade_pod_maximum_tier;
    api.get_original_classlabel = &api_get_original_classlabel;
    api.associate_evolver_cocoon_class =
        &api_associate_evolver_cocoon_class;
    api.register_info_ini_defaults_handler =
        &api_register_info_ini_defaults_handler;
    return api;
}

DWORD WINAPI initialize(void*) {
    // Called by the post-attach worker through A2FO_Initialize, so polling and
    // native-module discovery both happen outside loader lock.
    g_armada = GetModuleHandleA(nullptr);
    for (unsigned attempt = 0; attempt < 100 && !g_fleet_ops; ++attempt) {
        g_fleet_ops = GetModuleHandleA("FleetOpsHook.dll");
        if (!g_fleet_ops) {
            Sleep(10);
        }
    }
    if (!validate_module(g_armada, kArmadaTimestamp, kArmadaImageSize, "ArmadaL.exe") ||
        !validate_module(g_fleet_ops, kFleetOpsTimestamp, kFleetOpsImageSize,
                         "FleetOpsHook.dll")) {
        log_line("No hooks installed");
        return 1;
    }
    try {
      if (g_classlabel_alias_hook_ready) {
        log_line("ParameterDB string policy hook enabled before class loading");
      } else {
        g_classlabel_alias_hook_ready =
          install_classlabel_alias_hook();
      }
      if (g_evolver_hooks_ready) {
          log_line("Evolver policy hooks enabled before class loading");
      } else {
          g_evolver_hooks_ready = install_evolver_hooks();
      }
      if (g_user_profile_game_speed_hook_ready) {
          log_line("Armada default user-profile game-speed hook enabled "
                   "before profile loading");
      } else {
          g_user_profile_game_speed_hook_ready =
              install_user_profile_game_speed_hook();
      }
      if (g_fofs_item_get_lookup_hook_ready) {
          log_line("FOFS item lookup dispatcher enabled before ODF loading");
      } else {
          g_fofs_item_get_lookup_hook_ready =
              install_fofs_item_get_lookup_hook();
      }
      if (g_mod_user_directory_hook_ready) {
          log_line("Fleet Operations SettingsDirectory hook enabled before "
                   "settings loading");
      } else {
          g_mod_user_directory_hook_ready =
              install_mod_user_directory_hook();
      }
      if (g_fo_settings_default_hook_ready) {
          log_line("Fleet Operations Settings.xml default hook enabled before "
                   "settings loading");
      } else {
          g_fo_settings_default_hook_ready =
              install_fo_settings_default_hook();
      }
      if (g_default_game_speed_hook_ready) {
          log_line("Fleet Operations DefaultGameSpeed hook enabled before "
                   "profile loading");
      } else {
          g_default_game_speed_hook_ready =
              install_default_game_speed_hook();
      }
      if (g_runtime_game_speed_hook_ready) {
          log_line("Fleet Operations runtime DefaultGameSpeed hook enabled "
                   "before profile loading");
      } else {
          g_runtime_game_speed_hook_ready =
              install_runtime_game_speed_hook();
      }

      // Preserve the proven monolithic startup order: all built-in hooks are
      // installed first. Native modules are discovered only afterwards, from
      // the worker thread and outside the loader lock.
      if (g_root_directory.empty()) {
          g_root_directory = module_directory(nullptr);
      }
      const a2fo::ExtensionRootDiscovery root_discovery =
          a2fo::discover_extension_roots(
              g_root_directory,
              GetCommandLineA() ? GetCommandLineA() : "");
      g_extension_roots = root_discovery.roots;
      for (const std::string& diagnostic : root_discovery.diagnostics) {
          log_line(diagnostic);
      }
      if (!root_discovery.active_mod.empty()) {
          log_line("Extension roots: active mod: " +
                   root_discovery.active_mod);
      }
      log_line("Extension roots: " +
               std::to_string(g_extension_roots.size()) +
               " search roots");
      for (const std::string& root : g_extension_roots) {
          log_line("  extension root: " + root);
      }

      // Modules may retain this pointer and the extension-root strings for
      // their entire loaded lifetime. Keep both in process-lifetime storage.
      g_module_api = make_module_api();
      a2fo::ModuleRegistrationObserver registration_observer;
      registration_observer.begin = &begin_module_registration;
      registration_observer.finish = &finish_module_registration;
      a2fo::load_native_modules_from_roots(
          g_extension_roots, g_module_api, g_loaded_modules, &log_line,
          registration_observer);
      a2fo::LuaEngineApi lua_engine;
      lua_engine.parameter_db_get_string = &lua_parameter_db_get_string;
      if (!a2fo::initialize_lua_host(
              g_extension_roots, g_lua_host, &log_line, lua_engine)) {
          log_line("Lua host: initialization completed with script errors");
      }
      {
          StateLockGuard lock;
          const std::vector<std::string> lua_fields =
              a2fo::object_destroyed_odf_fields(g_lua_host);
          g_destroyed_odf_fields.insert(
              lua_fields.begin(), lua_fields.end());
      }
      if (!g_object_destroyed_handlers.empty() ||
          !g_lua_host.object_destroyed_callbacks.empty()) {
          g_object_destroyed_hook_ready = install_object_destroyed_hook();
      }

      std::size_t alias_count = 0;
      std::string cocoon_command;
      {
          StateLockGuard lock;
          g_policy_registration_open = false;
          alias_count = g_classlabel_aliases.size();
          cocoon_command = g_evolver_cocoon_command;
      }
      log_line("Policy registration closed: " +
               std::to_string(alias_count) + " classlabel alias" +
               (alias_count == 1 ? "" : "es") +
               ", Evolver cocoon command: " +
               (cocoon_command.empty() ? "<none>" : cocoon_command));
      log_line("Lua callbacks ready: " +
               std::to_string(g_lua_host.classlabel_callbacks.size()) +
               " classlabel, Evolver cocoon: " +
               (g_lua_host.evolver_cocoon_callback.reference == -2
                    ? "<none>" : "registered") +
               ", " +
               std::to_string(g_lua_host.object_destroyed_callbacks.size()) +
               " Lua object-destroyed, " +
               std::to_string(g_object_destroyed_handlers.size()) +
               " native object-destroyed");
    } catch (...) {
        log_line("Initialization aborted by an unexpected C++ exception");
        return 1;
    }
    log_line("A2FOExtensions initialization complete");
    return 0;
  }
} //namespace

extern "C" {
void* a2fo_cocoon_resume = nullptr;
void* a2fo_cocoon_update_resume = nullptr;
}

extern "C" void* a2fo_select_cocoon(void* evolver) {
    // Called by the small assembly splice at Armada's cocoon-selection site.
    // Return a cached/custom geometry when possible, otherwise reproduce the
    // two original Fleet Ops defaults exactly.
    if (!evolver || !g_state_lock_ready) {
        return nullptr;
    }
    const auto* bytes = static_cast<std::uint8_t*>(evolver);
    void* class_object = *reinterpret_cast<void* const*>(bytes + 0x40);
    std::string name;
    bool cache_known = false;
    void* cached_geometry = nullptr;
    bool first_selection = false;
    EnterCriticalSection(&g_state_lock);
    const auto selected = g_class_cocoons.find(class_object);
    if (selected != g_class_cocoons.end()) {
        name = selected->second;
    }
    if (!name.empty()) {
        const auto cached = g_loaded_cocoons.find(lower_ascii(name));
        if (cached != g_loaded_cocoons.end()) {
            cache_known = true;
            cached_geometry = cached->second;
        }
    }
    first_selection = g_logged_cocoon_classes.insert(class_object).second;
    LeaveCriticalSection(&g_state_lock);

    if (first_selection) {
        log_line("Cocoon selector for EvolverClass: " +
                 (name.empty() ? std::string("<Fleet Ops default>") : name));
    }

    if (cache_known) {
        if (cached_geometry) {
            return cached_geometry;
        }
        name.clear();
    }

    if (!name.empty()) {
        void* database = *at<void*>(g_armada, kSodDatabaseRva);
        void* geometry = database ? a2fo_load_sod(at(g_armada, kLoadSodRva),
                                                  database, name.c_str())
                                  : nullptr;
        EnterCriticalSection(&g_state_lock);
        g_loaded_cocoons[lower_ascii(name)] = geometry;
        LeaveCriticalSection(&g_state_lock);
        if (geometry) {
            return geometry;
        }
        log_line("Could not load cocoon SOD; using Fleet Ops default: " + name);
    }

    const bool alternative =
        *reinterpret_cast<const std::uint32_t*>(bytes + 0x1e8) == 0;
    const std::uintptr_t pointer_rva = alternative ? kAlternativeCocoonRva
                                                   : kDefaultCocoonRva;
    void** fallback_slot = at<void*>(g_armada, pointer_rva);
    if (*fallback_slot) {
        return *fallback_slot;
    }

    // Evolver's constructor normally initializes these two process globals.
    // A ResearchStation-hosted hybrid can start evolution before any native
    // Evolver exists, so reproduce that lazy load here instead of returning a
    // null geometry to the start/update effect paths.
    const char* fallback_name = alternative ? kAlternativeCocoonName
                                            : kDefaultCocoonName;
    void* database = *at<void*>(g_armada, kSodDatabaseRva);
    void* geometry = database ? a2fo_load_sod(at(g_armada, kLoadSodRva),
                                              database, fallback_name)
                              : nullptr;
    if (geometry) {
        *fallback_slot = geometry;
        log_line("Initialized Fleet Ops default cocoon on demand: " +
                 std::string(fallback_name));
    } else {
        log_line("Could not initialize Fleet Ops default cocoon: " +
                 std::string(fallback_name));
    }
    return geometry;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_Initialize() {
    // This export must be called outside DllMain. Module discovery uses
    // LoadLibrary, which is unsafe while Windows holds the loader lock.
    const LONG prior = InterlockedCompareExchange(&g_initialize_state, 1, 0);
    if (prior == 2) {
        return true;
    }
    if (prior != 0) {
        return false;
    }

    if (g_log == INVALID_HANDLE_VALUE) {
        open_log();
    }
    log_line("A2FOExtensions deferred initialization started");
    if (g_root_directory.empty()) {
        g_root_directory = module_directory(nullptr);
    }
    g_armada = GetModuleHandleA(nullptr);
    if (!g_fleet_ops) {
        g_fleet_ops = GetModuleHandleA("FleetOpsHook.dll");
    }
    if (!g_armada) {
        log_line("ArmadaL.exe was unavailable during A2FO_Initialize");
        InterlockedExchange(&g_initialize_state, 0);
        return false;
    }

    a2fo_cocoon_resume = at(g_armada, kCocoonSelectorRva + 5);
    a2fo_cocoon_update_resume = at(g_armada, kCocoonUpdateResumeRva);
    if (!g_state_lock_ready) {
        InitializeCriticalSection(&g_state_lock);
        g_state_lock_ready = true;
    }

    const bool ok = initialize(nullptr) == 0;
    InterlockedExchange(&g_initialize_state, ok ? 2 : 0);
    return ok;
}

DWORD WINAPI initialize_worker(void*) {
    g_initialize_thread_id = GetCurrentThreadId();
    const bool initialized = A2FO_Initialize();
    g_initialize_thread_id = 0;
    InterlockedExchange(&g_deferred_initialization_finished, 1);
    if (initialized) {
        if (kShellDisplayMonitorEnabled) {
            run_shell_display_monitor();
        } else {
            log_line("Shell display: graphics-settings monitor disabled");
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Preserve the startup timing of the proven monolithic build. These
        // checked memory hooks must exist before Armada builds its ODF classes.
        // Native modules are not loaded here; the worker is serialized behind
        // DLL attachment and calls A2FO_Initialize after loader lock releases.
        DisableThreadLibraryCalls(instance);
        open_log();
        log_line("A2FOExtensions initialization started");
        g_root_directory = module_directory(nullptr);
        g_armada = GetModuleHandleA(nullptr);
        g_fleet_ops = GetModuleHandleA("FleetOpsHook.dll");
        if (g_armada) {
            a2fo_cocoon_resume = at(g_armada, kCocoonSelectorRva + 5);
            a2fo_cocoon_update_resume = at(
                g_armada, kCocoonUpdateResumeRva);
        }
        InitializeCriticalSection(&g_state_lock);
        g_state_lock_ready = true;
        if (validate_module(g_armada, kArmadaTimestamp, kArmadaImageSize,
                            "ArmadaL.exe")) {
            g_evolver_hooks_ready = install_evolver_hooks();
            g_user_profile_game_speed_hook_ready =
                install_user_profile_game_speed_hook();
            if (validate_module(g_fleet_ops, kFleetOpsTimestamp,
                                kFleetOpsImageSize, "FleetOpsHook.dll")) {
                g_fofs_item_get_lookup_hook_ready =
                    install_fofs_item_get_lookup_hook();
                g_mod_user_directory_hook_ready =
                    install_mod_user_directory_hook();
                g_fo_settings_default_hook_ready =
                    install_fo_settings_default_hook();
                g_default_game_speed_hook_ready =
                    install_default_game_speed_hook();
                g_runtime_game_speed_hook_ready =
                    install_runtime_game_speed_hook();
            }
        }
        HANDLE thread =
            CreateThread(nullptr, 0, initialize_worker, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        } else {
            log_line("Could not create deferred initialization worker");
        }
        (void)instance;
    }
    return TRUE;
}
