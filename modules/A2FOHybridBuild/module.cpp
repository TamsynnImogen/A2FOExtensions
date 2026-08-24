/*
 * File: modules/A2FOHybridBuild/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Bootstrap HybridBuild aliases, cocoon policy, and runtime hooks.
 */

#include "../A2FOFeaturePack/hybrid_bridge_api.hpp"
#include "hybrid_production_runtime.hpp"
#include "refit_ui_bridge_api.hpp"
#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kModuleName = "A2FOHybridBuild";

const A2FO_ModuleApi* g_api = nullptr;
constexpr std::size_t kMaximumOdfSize = 2 * 1024 * 1024;
std::map<std::string, std::string> g_odf_paths;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool has_odf_extension(const std::string& name) {
    const std::string lowered = lower_ascii(name);
    return lowered.size() >= 4 &&
        lowered.compare(lowered.size() - 4, 4, ".odf") == 0;
}

void index_odf_directory(const std::string& directory, std::size_t depth) {
    if (depth > 64) return;
    WIN32_FIND_DATAA data{};
    const std::string pattern = join_path(directory, "*");
    HANDLE search = FindFirstFileA(pattern.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return;
    std::vector<std::pair<std::string, bool>> children;
    do {
        const std::string name = data.cFileName;
        if (name == "." || name == "..") continue;
        const bool is_directory =
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_directory &&
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        if (is_directory || has_odf_extension(name)) {
            children.emplace_back(name, is_directory);
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);
    std::sort(children.begin(), children.end(),
              [](const auto& left, const auto& right) {
                  return lower_ascii(left.first) < lower_ascii(right.first);
              });
    for (const auto& child : children) {
        const std::string path = join_path(directory, child.first);
        if (child.second) {
            index_odf_directory(path, depth + 1);
        } else {
            g_odf_paths[lower_ascii(child.first)] = path;
        }
    }
}

void index_odf_files() {
    g_odf_paths.clear();
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return;
    }
    const std::uint32_t count = g_api->extension_root_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* root = g_api->extension_root(index);
        if (root && *root) index_odf_directory(join_path(root, "odf"), 0);
    }
}

bool read_small_text_file(const std::string& path, std::string* contents) {
    if (!contents) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(kMaximumOdfSize)) {
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream stream;
    stream << input.rdbuf();
    *contents = stream.str();
    return input.good() || input.eof();
}

std::string event_source_name(const A2FO_StringView& source) {
    if (!source.data || source.size == 0 || source.size > 1024) return {};
    std::string name(source.data, source.size);
    std::replace(name.begin(), name.end(), '/', '\\');
    const std::size_t slash = name.find_last_of('\\');
    if (slash != std::string::npos) name.erase(0, slash + 1);
    name = lower_ascii(std::move(name));
    if (!has_odf_extension(name)) name += ".odf";
    return name;
}

void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->object_class) {
        return;
    }
    const std::string source = event_source_name(event->source_odf);
    const auto found = g_odf_paths.find(source);
    if (source.empty() || found == g_odf_paths.end()) return;
    std::string contents;
    if (!read_small_text_file(found->second, &contents)) return;
    a2fo::build_submenu::Config config;
    std::string error;
    if (!a2fo::build_submenu::parse_config(contents, &config, &error) ||
        config.empty()) {
        return;
    }
    if (!a2fo::register_live_build_submenus(
            event->object_class, config, source)) {
        log_line("Live build submenus in " + source +
                 " were rejected; native build list retained");
    }
}

bool register_build_submenu_parser() noexcept {
    if (!g_api ||
        !A2FO_MODULE_API_HAS(
            g_api, register_game_object_class_loaded_handler) ||
        (g_api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        !g_api->register_game_object_class_loaded_handler) {
        log_line("GameObjectClass-loaded API unavailable; live build "
                 "submenus disabled");
        return false;
    }
    return g_api->register_game_object_class_loaded_handler(
        kModuleName, nullptr, 0, &class_loaded_handler, nullptr);
}

bool register_feature_pack_bridge() noexcept {
    HMODULE feature_pack = GetModuleHandleA("A2FOFeaturePack.dll");
    if (!feature_pack) {
        log_line("A2FOFeaturePack.dll is required for shared queue and "
                 "ResearchStation hook ownership");
        return false;
    }
    FARPROC address = GetProcAddress(
        feature_pack, "A2FO_RegisterHybridBridge");
    A2FO_RegisterHybridBridgeFn register_bridge = nullptr;
    static_assert(sizeof(register_bridge) == sizeof(address),
                  "bridge function pointer must match FARPROC");
    std::memcpy(&register_bridge, &address, sizeof(register_bridge));
    if (!register_bridge) {
        log_line("A2FOFeaturePack.dll does not expose the HybridBuild bridge");
        return false;
    }

    A2FO_HybridBridge bridge{};
    bridge.struct_size = sizeof(bridge);
    bridge.version = A2FO_HYBRID_BRIDGE_VERSION;
    bridge.has_queued_research_conflict =
        &a2fo::hybrid_production_has_queued_research_conflict;
    bridge.is_evolve_target =
        &a2fo::hybrid_production_is_evolve_target;
    bridge.has_evolution_barrier =
        &a2fo::hybrid_production_has_evolution_barrier;
    bridge.should_defer_construct_order =
        &a2fo::hybrid_production_should_defer_construct_order;
    bridge.finalize_construct_order =
        &a2fo::finalize_hybrid_construct_order;
    bridge.discard_construct_placement =
        &a2fo::discard_hybrid_construct_placement;
    bridge.clear_construct_placements =
        &a2fo::clear_hybrid_construct_placements;
    bridge.retain_research_menu_after_order =
        &a2fo::retain_hybrid_research_menu_after_order;
    bridge.register_research_station_lists =
        &a2fo::register_research_station_hybrid_lists;
    bridge.publish_research_station_items =
        &a2fo::publish_research_station_hybrid_items;
    bridge.cleanup_cocoon = &a2fo::cleanup_hybrid_cocoon;
    bridge.cleanup_construction = &a2fo::cleanup_hybrid_construction;
    if (!register_bridge(&bridge)) {
        log_line("A2FOFeaturePack rejected the HybridBuild bridge");
        return false;
    }
    log_line("Shared queue and ResearchStation bridge registered");
    return true;
}

}  // namespace

extern "C" __declspec(dllexport)
bool __cdecl A2FO_RegisterRefitUiBridge(
    const A2FO_RefitUiBridge* bridge) {
    return a2fo::register_refit_ui_bridge(bridge);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->extension_root_count || !api->extension_root ||
        !api->register_classlabel_alias ||
        !api->register_evolver_cocoon_command) {
        return false;
    }
    g_api = api;
    HMODULE armada = static_cast<HMODULE>(api->armada_module());
    HMODULE fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!armada || !fleet_ops) return false;

    const bool alias_registered = api->register_classlabel_alias(
        kModuleName, "hybridbuild", "research");
    const bool cocoon_registered = api->register_evolver_cocoon_command(
        kModuleName, "cocoon");
    if (!alias_registered || !cocoon_registered) {
        log_line("HybridBuild semantic policy registration failed");
        return false;
    }
    if (!register_feature_pack_bridge()) return false;

    try {
        index_odf_files();
    } catch (...) {
        log_line("Could not index loose ODF files for live build submenus");
    }

    // Keep the DLL loaded even if a checked runtime site is unavailable. A
    // partial inline-hook transaction cannot safely be unloaded after code has
    // been patched to this image; the runtime itself fails closed per feature.
    const bool runtime_ready = a2fo::initialize_hybrid_production_registry(
        api, armada, fleet_ops);
    const bool submenu_parser_ready = runtime_ready &&
        register_build_submenu_parser();
    log_line(runtime_ready
        ? std::string("HybridBuild module initialized; live build submenus ") +
              (submenu_parser_ready ? "enabled" : "disabled") +
              " (" + std::to_string(g_odf_paths.size()) +
              " loose ODFs indexed)"
        : "HybridBuild module loaded with runtime adapter disabled");
    return true;
}
