/*
 * File: modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Additional per-module BuildYard technology-tree gates.
 */

#include "buildyard_pseudo_technology.hpp"

#include <windows.h>

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace a2fo {
namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVA. This is called directly so
// the core's ParameterDB::GetString detour and other modules' signature checks
// remain untouched.
constexpr std::uintptr_t kParameterDbGetProjectIdRva = 0x00135200;

// FleetOpsHook.dll RVAs from the supported 3.2.7 executable. BuildYard's
// configuration parser uses a private Delphi wrapper around Armada's
// ParameterDB::GetProjectId. Only that one call site is redirected.
constexpr std::uintptr_t kBuildYardConfigurationParseRva = 0x0013b488;
constexpr std::uintptr_t kBuildYardRequiredTechnologyCallRva = 0x0013b5cb;
constexpr std::uintptr_t kBuildYardConfigurationDestroyRva = 0x0013b880;
constexpr std::uintptr_t kFleetOpsParameterDbGetProjectIdRva = 0x001e2d70;
constexpr std::uintptr_t kProjectIdTechnologyAvailableRva = 0x001def64;

constexpr std::size_t kMaximumModuleIndex = 1023;
constexpr std::size_t kMaximumConfigurationName = 160;

constexpr std::array<std::uint8_t, 9> kExpectedParameterDbGetProjectId{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};
constexpr std::array<std::uint8_t, 8> kExpectedBuildYardConfigurationParse{
    0x55, 0x8b, 0xec, 0xb9, 0x11, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 5> kExpectedRequiredTechnologyCall{
    0xe8, 0xa0, 0x77, 0x0a, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedConfigurationDestroy{
    0x53, 0x56, 0x8b, 0xf0, 0x8b, 0xc6};
constexpr std::array<std::uint8_t, 6> kExpectedTechnologyAvailable{
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf4};

extern "C" std::uintptr_t a2fo_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
extern "C" std::uintptr_t a2fo_buildyard_call_delphi_reg3_stack1(
    void* function, void* argument_eax, void* argument_edx,
    void* argument_ecx, std::uintptr_t stack_argument);
extern "C" void a2fo_buildyard_project_id_call_bridge();

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
A2FO_InlineHook g_configuration_parse_hook{};
A2FO_InlineHook g_configuration_destroy_hook{};
A2FO_InlineHook g_technology_available_hook{};
CRITICAL_SECTION g_policy_lock;
bool g_policy_lock_ready = false;

struct CapturedPseudoTechnology {
    std::size_t module_index = 0;
    std::uint32_t* required_field = nullptr;
    std::uint32_t pseudo_project_id = 0;
};

struct ParseCapture {
    std::uint32_t depth = 0;
    std::array<char, kMaximumConfigurationName> configuration_name{};
    std::vector<CapturedPseudoTechnology> entries;
};

thread_local ParseCapture g_parse_capture;

// Only modules which declare both commands need a second lookup. When the
// native RequiredTechnology field is empty, the pseudo ID is placed into that
// field and Fleet Operations handles it completely natively.
std::unordered_map<const std::uint32_t*, std::uint32_t>
    g_additional_project_ids;
std::unordered_map<void*, std::vector<const std::uint32_t*>>
    g_configuration_fields;

class PolicyLockGuard {
public:
    PolicyLockGuard() noexcept {
        if (g_policy_lock_ready) {
            EnterCriticalSection(&g_policy_lock);
            locked_ = true;
        }
    }

    ~PolicyLockGuard() {
        if (locked_) LeaveCriticalSection(&g_policy_lock);
    }

    PolicyLockGuard(const PolicyLockGuard&) = delete;
    PolicyLockGuard& operator=(const PolicyLockGuard&) = delete;

private:
    bool locked_ = false;
};

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uintptr_t>(module) + rva);
}

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

bool readable_range(const void* pointer, std::size_t length) noexcept {
    if (!pointer || length == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
        (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    const auto region_begin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto region_end = region_begin + info.RegionSize;
    return begin >= region_begin && begin <= region_end &&
        length <= region_end - begin;
}

bool writable_range(void* pointer, std::size_t length) noexcept {
    if (!readable_range(pointer, length)) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }
    const DWORD protection = info.Protect & 0xffu;
    return protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) {
    return module && readable_range(at(module, rva), expected.size()) &&
        std::memcmp(at(module, rva), expected.data(), expected.size()) == 0;
}

bool ascii_equal_case_insensitive(char left, char right) noexcept {
    return std::tolower(static_cast<unsigned char>(left)) ==
        std::tolower(static_cast<unsigned char>(right));
}

bool starts_with_case_insensitive(const char* text,
                                  const char* prefix) noexcept {
    if (!text || !prefix) return false;
    for (; *prefix; ++text, ++prefix) {
        if (!*text || !ascii_equal_case_insensitive(*text, *prefix)) {
            return false;
        }
    }
    return true;
}

bool parse_required_technology_command(const char* command,
                                       std::size_t* module_index) noexcept {
    constexpr char kPrefix[] = "Module";
    constexpr char kSuffix[] = "RequiredTechnology";
    if (!module_index ||
        !starts_with_case_insensitive(command, kPrefix)) {
        return false;
    }

    const char* cursor = command + sizeof(kPrefix) - 1;
    if (!std::isdigit(static_cast<unsigned char>(*cursor))) return false;
    std::size_t index = 0;
    do {
        const std::size_t digit = static_cast<std::size_t>(*cursor - '0');
        if (index > (kMaximumModuleIndex - digit) / 10) return false;
        index = index * 10 + digit;
        ++cursor;
    } while (std::isdigit(static_cast<unsigned char>(*cursor)));
    if (index > kMaximumModuleIndex) return false;

    for (const char* suffix = kSuffix; *suffix; ++suffix, ++cursor) {
        if (!*cursor ||
            !ascii_equal_case_insensitive(*cursor, *suffix)) {
            return false;
        }
    }
    if (*cursor != '\0') return false;
    *module_index = index;
    return true;
}

void copy_delphi_string(void* value, char* output,
                        std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    const char* text = static_cast<const char*>(value);
    if (!text) return;
    for (std::size_t index = 0; index + 1 < output_size; ++index) {
        if (!readable_range(text + index, 1)) break;
        const unsigned char character =
            static_cast<unsigned char>(text[index]);
        if (character == 0) break;
        if (character < 0x20 && character != '\t') break;
        output[index] = static_cast<char>(character);
        output[index + 1] = '\0';
    }
}

bool read_pseudo_project_id(void* parameter_db, std::size_t module_index,
                            std::uint32_t* project_id) noexcept {
    if (!parameter_db || !project_id) return false;
    std::array<char, 64> command{};
    const int written = std::snprintf(
        command.data(), command.size(), "Module%luPseudoTechnology",
        static_cast<unsigned long>(module_index));
    if (written <= 0 || static_cast<std::size_t>(written) >= command.size()) {
        return false;
    }
    *project_id = 0;
    const std::uintptr_t found = a2fo_call_thiscall_3(
        at(g_armada, kParameterDbGetProjectIdRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command.data()),
        reinterpret_cast<std::uintptr_t>(project_id), 0);
    return (found & 0xffu) != 0;
}

using ConfigurationParseFunction =
    void* (__attribute__((regparm(3))) *)(void* configuration_name);
using ConfigurationDestroyFunction =
    void (__attribute__((regparm(3))) *)(void* configuration);
using TechnologyAvailableFunction =
    bool (__attribute__((regparm(3))) *)(
        const std::uint32_t* project_id, std::uintptr_t team_index);

void erase_configuration_policy_locked(void* configuration) {
    const auto found = g_configuration_fields.find(configuration);
    if (found == g_configuration_fields.end()) return;
    for (const std::uint32_t* field : found->second) {
        g_additional_project_ids.erase(field);
    }
    g_configuration_fields.erase(found);
}

void register_captured_policy(void* configuration) noexcept {
    if (!configuration || !g_policy_lock_ready) return;
    try {
        std::vector<const std::uint32_t*> combined_fields;
        std::size_t native_replacements = 0;
        std::size_t combined_requirements = 0;

        {
            PolicyLockGuard lock;
            erase_configuration_policy_locked(configuration);
            for (const CapturedPseudoTechnology& entry :
                 g_parse_capture.entries) {
                std::uint32_t* field = entry.required_field;
                if (!field || entry.pseudo_project_id == 0 ||
                    !writable_range(field, sizeof(*field))) {
                    continue;
                }
                if (*field == 0) {
                    *field = entry.pseudo_project_id;
                    ++native_replacements;
                } else if (*field != entry.pseudo_project_id) {
                    g_additional_project_ids[field] =
                        entry.pseudo_project_id;
                    combined_fields.push_back(field);
                    ++combined_requirements;
                }
            }
            if (!combined_fields.empty()) {
                g_configuration_fields.emplace(
                    configuration, std::move(combined_fields));
            }
        }

        if (native_replacements != 0 || combined_requirements != 0) {
            char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "BuildYard pseudo technology registered for '%s': "
                "%lu pseudo-only module%s, %lu combined module%s",
                g_parse_capture.configuration_name[0]
                    ? g_parse_capture.configuration_name.data()
                    : "<configuration>",
                static_cast<unsigned long>(native_replacements),
                native_replacements == 1 ? "" : "s",
                static_cast<unsigned long>(combined_requirements),
                combined_requirements == 1 ? "" : "s");
            log_line(message);
        }
    } catch (...) {
        log_line("BuildYard pseudo-technology policy registration failed; "
                 "native RequiredTechnology behavior retained");
    }
}

void* __attribute__((regparm(3))) configuration_parse_hook(
    void* configuration_name) noexcept {
    const auto original = reinterpret_cast<ConfigurationParseFunction>(
        g_configuration_parse_hook.gateway);
    if (!original) return nullptr;

    const bool outermost = g_parse_capture.depth++ == 0;
    if (outermost) {
        g_parse_capture.entries.clear();
        g_parse_capture.configuration_name.fill('\0');
        copy_delphi_string(
            configuration_name, g_parse_capture.configuration_name.data(),
            g_parse_capture.configuration_name.size());
    }

    void* configuration = original(configuration_name);
    if (--g_parse_capture.depth == 0) {
        register_captured_policy(configuration);
        g_parse_capture.entries.clear();
    }
    return configuration;
}

void __attribute__((regparm(3))) configuration_destroy_hook(
    void* configuration) noexcept {
    if (configuration && g_policy_lock_ready) {
        PolicyLockGuard lock;
        erase_configuration_policy_locked(configuration);
    }
    const auto original = reinterpret_cast<ConfigurationDestroyFunction>(
        g_configuration_destroy_hook.gateway);
    if (original) original(configuration);
}

bool __attribute__((regparm(3))) technology_available_hook(
    const std::uint32_t* project_id, std::uintptr_t team_index) noexcept {
    const auto original = reinterpret_cast<TechnologyAvailableFunction>(
        g_technology_available_hook.gateway);
    if (!original) return true;
    const bool native_available = original(project_id, team_index);
    if (!native_available || !project_id || !g_policy_lock_ready) {
        return native_available;
    }

    std::uint32_t pseudo_project_id = 0;
    {
        PolicyLockGuard lock;
        const auto found = g_additional_project_ids.find(project_id);
        if (found != g_additional_project_ids.end()) {
            pseudo_project_id = found->second;
        }
    }

    return pseudo_project_id == 0 ||
        original(&pseudo_project_id, team_index);
}

}  // namespace

extern "C" std::uintptr_t __cdecl
a2fo_buildyard_project_id_call_handler(
    void* parameter_db, void* delphi_command,
    std::uint32_t* required_project_id,
    std::uintptr_t default_project_id) noexcept {
    const std::uintptr_t result =
        a2fo_buildyard_call_delphi_reg3_stack1(
            at(g_fleet_ops, kFleetOpsParameterDbGetProjectIdRva),
            parameter_db, delphi_command, required_project_id,
            default_project_id);

    if (g_parse_capture.depth == 0 || !parameter_db ||
        !required_project_id) {
        return result;
    }

    std::array<char, 64> native_command{};
    copy_delphi_string(
        delphi_command, native_command.data(), native_command.size());
    std::size_t module_index = 0;
    if (!parse_required_technology_command(
            native_command.data(), &module_index)) {
        return result;
    }

    try {
        std::uint32_t pseudo_project_id = 0;
        if (read_pseudo_project_id(
                parameter_db, module_index, &pseudo_project_id)) {
            if (pseudo_project_id == 0) {
                char message[192]{};
                std::snprintf(
                    message, sizeof(message),
                    "BuildYard module%luPseudoTechnology resolved to "
                    "project ID 0 and was ignored",
                    static_cast<unsigned long>(module_index));
                log_line(message);
            } else {
                g_parse_capture.entries.push_back(
                    {module_index, required_project_id,
                     pseudo_project_id});
            }
        }
    } catch (...) {
        log_line("BuildYard pseudo-technology command capture failed; "
                 "native RequiredTechnology behavior retained");
    }
    return result;
}

bool initialize_buildyard_pseudo_technology(
    const A2FO_ModuleApi* api, HMODULE armada,
    HMODULE fleet_ops) noexcept {
    if (!api || !api->log || !api->install_inline_hook ||
        !api->patch_call || !armada || !fleet_ops) {
        return false;
    }

    const bool signatures_match =
        signature_matches(
            armada, kParameterDbGetProjectIdRva,
            kExpectedParameterDbGetProjectId) &&
        signature_matches(
            fleet_ops, kBuildYardConfigurationParseRva,
            kExpectedBuildYardConfigurationParse) &&
        signature_matches(
            fleet_ops, kBuildYardRequiredTechnologyCallRva,
            kExpectedRequiredTechnologyCall) &&
        signature_matches(
            fleet_ops, kBuildYardConfigurationDestroyRva,
            kExpectedConfigurationDestroy) &&
        signature_matches(
            fleet_ops, kProjectIdTechnologyAvailableRva,
            kExpectedTechnologyAvailable);
    if (!signatures_match) {
        api->log(kModuleName,
                 "BuildYard pseudo-technology signatures mismatch; "
                 "moduleXPseudoTechnology disabled");
        return false;
    }

    g_api = api;
    g_armada = armada;
    g_fleet_ops = fleet_ops;
    InitializeCriticalSection(&g_policy_lock);
    g_policy_lock_ready = true;

    const bool installed =
        api->install_inline_hook(
            at(fleet_ops, kBuildYardConfigurationDestroyRva),
            reinterpret_cast<void*>(&configuration_destroy_hook),
            kExpectedConfigurationDestroy.size(),
            kExpectedConfigurationDestroy.data(),
            &g_configuration_destroy_hook) &&
        api->install_inline_hook(
            at(fleet_ops, kProjectIdTechnologyAvailableRva),
            reinterpret_cast<void*>(&technology_available_hook),
            kExpectedTechnologyAvailable.size(),
            kExpectedTechnologyAvailable.data(),
            &g_technology_available_hook) &&
        api->install_inline_hook(
            at(fleet_ops, kBuildYardConfigurationParseRva),
            reinterpret_cast<void*>(&configuration_parse_hook),
            kExpectedBuildYardConfigurationParse.size(),
            kExpectedBuildYardConfigurationParse.data(),
            &g_configuration_parse_hook) &&
        api->patch_call(
            at(fleet_ops, kBuildYardRequiredTechnologyCallRva),
            reinterpret_cast<void*>(
                &a2fo_buildyard_project_id_call_bridge),
            kExpectedRequiredTechnologyCall.data(),
            kExpectedRequiredTechnologyCall.size());

    if (!installed) {
        api->log(kModuleName,
                 "BuildYard pseudo-technology hook installation was "
                 "incomplete; native BuildYard behavior retained where "
                 "unpatched");
        return false;
    }

    api->log(kModuleName,
             "BuildYard moduleXPseudoTechnology support initialized");
    return true;
}

}  // namespace a2fo
