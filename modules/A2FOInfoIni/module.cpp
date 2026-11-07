/*
 * File: modules/A2FOInfoIni/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Supply SettingsDirectory and DefaultGameSpeed info.ini policy.
 */

#include "../../core/extension_roots.hpp"
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* kModuleName = "A2FOInfoIni";
constexpr std::size_t kMaximumPathLength = 32767;
constexpr std::streamoff kMaximumInfoSize = 1024 * 1024;

const A2FO_ModuleApi* g_api = nullptr;
a2fo::FleetOpsInfoDefaults g_defaults;
std::string g_configured_settings_directory;
std::string g_shared_settings_root;
std::string g_default_config_root;
bool g_ready = false;
const char* g_selected_root = nullptr;
std::string g_resolved_settings_directory;
std::string g_shared_source_settings_directory;
std::exception_ptr g_native_error;
bool A2FO_CALL provide_defaults(const char*, char*, std::uint32_t,
                                std::uint32_t*, std::int32_t*, void*);

void log_line(const std::string& message) {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

std::string read_small_text_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > kMaximumInfoSize) return {};
    input.seekg(0, std::ios::beg);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::string expand_environment_strings(const std::string& value) {
    if (value.empty()) return {};
    const DWORD required =
        ExpandEnvironmentStringsA(value.c_str(), nullptr, 0);
    if (required == 0 || required > kMaximumPathLength) return {};
    std::vector<char> output(required);
    const DWORD written = ExpandEnvironmentStringsA(
        value.c_str(), output.data(), static_cast<DWORD>(output.size()));
    if (written == 0 || written > output.size()) return {};
    return std::string(output.data());
}

std::string default_fleet_ops_config_root() {
    const DWORD required = GetEnvironmentVariableA("APPDATA", nullptr, 0);
    if (required == 0 || required > kMaximumPathLength) return {};
    std::vector<char> app_data(required);
    const DWORD written = GetEnvironmentVariableA(
        "APPDATA", app_data.data(), static_cast<DWORD>(app_data.size()));
    if (written == 0 || written >= app_data.size()) return {};
    return join_path(std::string(app_data.data()),
                     "Star Trek Armada II Fleet Ops Config");
}

std::uint32_t info_step(void*, std::uint32_t operation,
                        std::uint32_t index) noexcept {
    if (g_native_error) return 0;
    try {
        switch (operation) {
        case 0:
            g_selected_root = index == UINT32_MAX ? g_api->root_directory() :
                g_api->extension_root(index);
            return g_selected_root && *g_selected_root;
        case 1:
            g_defaults = a2fo::fleet_ops_defaults_from_info(
                read_small_text_file(join_path(g_selected_root, "info.ini")));
            g_shared_source_settings_directory = g_defaults.settings_directory;
            return 1;
        case 2:
            g_defaults = a2fo::fleet_ops_defaults_from_info(
                read_small_text_file(join_path(g_selected_root, "info.ini")));
            return 1;
        case 3: {
            g_shared_settings_root = expand_environment_strings(
                g_shared_source_settings_directory);
            return 1;
        }
        case 4:
            g_configured_settings_directory = expand_environment_strings(
                g_defaults.settings_directory);
            return 1;
        case 5:
            g_default_config_root = default_fleet_ops_config_root();
            return 1;
        case 6:
            g_ready = true;
            if (!g_api->register_info_ini_defaults_handler(
                    kModuleName, &provide_defaults, nullptr)) {
                g_ready = false;
                return 0;
            }
            return 1;
        default:
            return 0;
        }
    } catch (...) {
        g_native_error = std::current_exception();
        return 0;
    }
}

void info_log(void*, std::uint32_t code) noexcept {
    if (g_native_error) return;
    try {
        switch (code) {
        case 0: log_line("Data root is unavailable"); break;
        case 1:
            if (!g_shared_source_settings_directory.empty() &&
                g_shared_settings_root.empty())
                log_line("Data-level SettingsDirectory was rejected");
            break;
        case 2:
            if (!g_defaults.settings_directory.empty() &&
                g_configured_settings_directory.empty())
                log_line("Selected SettingsDirectory was rejected");
            break;
        case 3:
            if (g_defaults.has_default_game_speed)
                log_line("DefaultGameSpeed=" +
                         std::to_string(g_defaults.default_game_speed));
            if (!g_configured_settings_directory.empty())
                log_line("SettingsDirectory=" + g_configured_settings_directory);
            if (!g_shared_settings_root.empty())
                log_line("Shared settings root=" + g_shared_settings_root);
            break;
        case 5: log_line("info.ini defaults module initialized"); break;
        }
    } catch (...) { g_native_error = std::current_exception(); }
}

std::uint8_t resolve_settings(void*, A2foRsRuntimeBytes normal,
                              A2foRsRuntimeBytes* result) noexcept {
    try {
        g_resolved_settings_directory =
            a2fo::resolve_fleet_ops_settings_directory(
                std::string(reinterpret_cast<const char*>(normal.data)),
                g_configured_settings_directory, g_default_config_root,
                g_shared_settings_root);
        *result = {reinterpret_cast<const std::uint8_t*>(
                       g_resolved_settings_directory.data()),
                   g_resolved_settings_directory.size()};
        return 1;
    } catch (...) { g_native_error = std::current_exception(); return 0; }
}

bool A2FO_CALL provide_defaults(
    const char* normal_settings_directory,
    char* resolved_settings_directory,
    std::uint32_t resolved_settings_directory_size,
    std::uint32_t* has_default_game_speed,
    std::int32_t* default_game_speed,
    void*) {
    const bool enabled =
#ifdef A2FO_BUILTIN_FEATURE
        a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
        true;
#endif
    const A2foRsRuntimeInfoState state{
        static_cast<std::uint8_t>(g_defaults.has_default_game_speed),
        g_defaults.default_game_speed,
        {reinterpret_cast<const std::uint8_t*>(g_configured_settings_directory.data()),
         g_configured_settings_directory.size()},
        {reinterpret_cast<const std::uint8_t*>(g_shared_settings_root.data()),
         g_shared_settings_root.size()},
        {reinterpret_cast<const std::uint8_t*>(g_default_config_root.data()),
         g_default_config_root.size()}};
    // Treat normal as an opaque C string until the resolver callback runs.
    // Rust writes the speed outputs and clears the directory first, matching
    // the native order even when input and output storage overlap.
    const A2foRsRuntimeBytes normal{
        reinterpret_cast<const std::uint8_t*>(normal_settings_directory), 0};
    g_native_error = nullptr;
    const bool result = a2fo_rs_runtime_info_provide(
        enabled, g_ready, has_default_game_speed != nullptr,
        default_game_speed != nullptr, state, normal,
        normal_settings_directory != nullptr,
        resolved_settings_directory != nullptr,
        resolved_settings_directory_size, &resolve_settings, nullptr,
        has_default_game_speed, default_game_speed,
        reinterpret_cast<std::uint8_t*>(resolved_settings_directory)) != 0;
    if (g_native_error) std::rethrow_exception(g_native_error);
    return result;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid = api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version == A2FO_MODULE_API_VERSION && api->log &&
        api->root_directory && api->extension_root_count && api->extension_root &&
        A2FO_MODULE_API_HAS(api, register_info_ini_defaults_handler) &&
        api->api_revision >= 5 &&
        (api->capabilities & A2FO_CAP_INFO_INI_DEFAULTS) != 0 &&
        api->register_info_ini_defaults_handler;
    if (!valid) return false;
    g_api = api;
    const std::uint32_t root_count = api->extension_root_count();
    g_native_error = nullptr;
    const bool result = a2fo_rs_runtime_info_init(
        valid, root_count, &info_step, &info_log, nullptr) != 0;
    if (g_native_error) std::rethrow_exception(g_native_error);
    return result;
}
