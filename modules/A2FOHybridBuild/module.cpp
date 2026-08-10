/*
 * File: modules/A2FOHybridBuild/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Bootstrap HybridBuild aliases, cocoon policy, and runtime hooks.
 */

#include "../A2FOFeaturePack/hybrid_bridge_api.hpp"
#include "hybrid_production_runtime.hpp"
#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <cstring>
#include <string>

namespace {

constexpr const char* kModuleName = "A2FOHybridBuild";

const A2FO_ModuleApi* g_api = nullptr;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
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
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
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

    // Keep the DLL loaded even if a checked runtime site is unavailable. A
    // partial inline-hook transaction cannot safely be unloaded after code has
    // been patched to this image; the runtime itself fails closed per feature.
    const bool runtime_ready = a2fo::initialize_hybrid_production_registry(
        api, armada, fleet_ops);
    log_line(runtime_ready
        ? "HybridBuild module initialized"
        : "HybridBuild module loaded with runtime adapter disabled");
    return true;
}
