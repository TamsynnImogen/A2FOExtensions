/*
 * File: modules/A2FOInfoIni/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Supply SettingsDirectory and DefaultGameSpeed info.ini policy.
 */

#include "../../core/extension_roots.hpp"
#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
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

bool load_defaults() {
    const std::uint32_t root_count = g_api->extension_root_count
        ? g_api->extension_root_count() : 0;
    const char* data_root = root_count != 0 && g_api->extension_root
        ? g_api->extension_root(0) : g_api->root_directory();
    if (!data_root || !*data_root) {
        log_line("Data root is unavailable");
        return false;
    }

    const a2fo::FleetOpsInfoDefaults root_defaults =
        a2fo::fleet_ops_defaults_from_info(
            read_small_text_file(join_path(data_root, "info.ini")));
    g_defaults = root_defaults;
    if (root_count > 1 && g_api->extension_root) {
        const char* active_root = g_api->extension_root(root_count - 1);
        if (active_root && *active_root) {
            g_defaults = a2fo::fleet_ops_defaults_from_info(
                read_small_text_file(join_path(active_root, "info.ini")));
        }
        g_shared_settings_root = expand_environment_strings(
            root_defaults.settings_directory);
        if (!root_defaults.settings_directory.empty() &&
            g_shared_settings_root.empty()) {
            log_line("Data-level SettingsDirectory was rejected");
        }
    }

    g_configured_settings_directory = expand_environment_strings(
        g_defaults.settings_directory);
    if (!g_defaults.settings_directory.empty() &&
        g_configured_settings_directory.empty()) {
        log_line("Selected SettingsDirectory was rejected");
    }
    g_default_config_root = default_fleet_ops_config_root();

    if (g_defaults.has_default_game_speed) {
        log_line("DefaultGameSpeed=" +
                 std::to_string(g_defaults.default_game_speed));
    }
    if (!g_configured_settings_directory.empty()) {
        log_line("SettingsDirectory=" + g_configured_settings_directory);
    }
    if (!g_shared_settings_root.empty()) {
        log_line("Shared settings root=" + g_shared_settings_root);
    }
    return true;
}

bool A2FO_CALL provide_defaults(
    const char* normal_settings_directory,
    char* resolved_settings_directory,
    std::uint32_t resolved_settings_directory_size,
    std::uint32_t* has_default_game_speed,
    std::int32_t* default_game_speed,
    void*) {
    if (!g_ready || !has_default_game_speed || !default_game_speed) {
        return false;
    }
    *has_default_game_speed = g_defaults.has_default_game_speed ? 1u : 0u;
    *default_game_speed = g_defaults.default_game_speed;

    if (resolved_settings_directory && resolved_settings_directory_size) {
        resolved_settings_directory[0] = '\0';
        if (!g_configured_settings_directory.empty() &&
            normal_settings_directory) {
            const std::string resolved =
                a2fo::resolve_fleet_ops_settings_directory(
                    normal_settings_directory,
                    g_configured_settings_directory,
                    g_default_config_root,
                    g_shared_settings_root);
            if (resolved.size() + 1 > resolved_settings_directory_size) {
                return false;
            }
            std::memcpy(resolved_settings_directory, resolved.c_str(),
                        resolved.size() + 1);
        }
    }
    return true;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->root_directory || !api->extension_root_count ||
        !api->extension_root ||
        !A2FO_MODULE_API_HAS(api, register_info_ini_defaults_handler) ||
        api->api_revision < 5 ||
        (api->capabilities & A2FO_CAP_INFO_INI_DEFAULTS) == 0 ||
        !api->register_info_ini_defaults_handler) {
        return false;
    }
    g_api = api;
    if (!load_defaults()) return false;
    g_ready = true;
    if (!api->register_info_ini_defaults_handler(
            kModuleName, &provide_defaults, nullptr)) {
        g_ready = false;
        return false;
    }
    log_line("info.ini defaults module initialized");
    return true;
}
