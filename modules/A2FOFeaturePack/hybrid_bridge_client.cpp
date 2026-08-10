/*
 * File: modules/A2FOFeaturePack/hybrid_bridge_client.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: HybridBuild callback resolution and fail-open dispatch wrappers.
 */

#include "hybrid_bridge_client.hpp"

#include "hybrid_bridge_api.hpp"

#include <windows.h>

#include <cstring>

namespace {

A2FO_HybridBridge g_bridge{};
volatile LONG g_bridge_ready = 0;

bool valid_bridge(const A2FO_HybridBridge* bridge) noexcept {
    return bridge && bridge->version == A2FO_HYBRID_BRIDGE_VERSION &&
        bridge->struct_size >= sizeof(A2FO_HybridBridge) &&
        bridge->has_queued_research_conflict &&
        bridge->is_evolve_target && bridge->has_evolution_barrier &&
        bridge->should_defer_construct_order &&
        bridge->finalize_construct_order &&
        bridge->discard_construct_placement &&
        bridge->clear_construct_placements &&
        bridge->retain_research_menu_after_order &&
        bridge->register_research_station_lists &&
        bridge->publish_research_station_items &&
        bridge->cleanup_cocoon && bridge->cleanup_construction;
}

const A2FO_HybridBridge* bridge() noexcept {
    return InterlockedCompareExchange(&g_bridge_ready, 0, 0) != 0
        ? &g_bridge : nullptr;
}

}  // namespace

extern "C" __declspec(dllexport)
bool __cdecl A2FO_RegisterHybridBridge(const A2FO_HybridBridge* value) {
    if (!valid_bridge(value) ||
        InterlockedCompareExchange(&g_bridge_ready, 0, 0) != 0) {
        return false;
    }
    std::memcpy(&g_bridge, value, sizeof(g_bridge));
    InterlockedExchange(&g_bridge_ready, 1);
    return true;
}

namespace a2fo {

bool hybrid_production_has_queued_research_conflict(
    void* producer, void* target_class) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->has_queued_research_conflict(
        producer, target_class);
}

bool hybrid_production_is_evolve_target(
    void* producer, void* target_class) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->is_evolve_target(
        producer, target_class);
}

bool hybrid_production_has_evolution_barrier(void* producer) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->has_evolution_barrier(producer);
}

bool hybrid_production_should_defer_construct_order(
    void* producer, void* target_class) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->should_defer_construct_order(
        producer, target_class);
}

void finalize_hybrid_construct_order(
    void* producer, void* target_class, bool admitted) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) {
        callbacks->finalize_construct_order(
            producer, target_class, admitted);
    }
}

void discard_hybrid_construct_placement(
    void* producer, std::uint32_t queue_id) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) {
        callbacks->discard_construct_placement(producer, queue_id);
    }
}

void clear_hybrid_construct_placements(void* producer) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) callbacks->clear_construct_placements(producer);
}

void retain_hybrid_research_menu_after_order(
    void* producer, void* target_class) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) {
        callbacks->retain_research_menu_after_order(
            producer, target_class);
    }
}

bool register_research_station_hybrid_lists(
    void* station_class, void* parameter_db) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->register_research_station_lists(
        station_class, parameter_db);
}

std::size_t publish_research_station_hybrid_items(
    void* station_class) noexcept {
    const auto* callbacks = bridge();
    return callbacks
        ? callbacks->publish_research_station_items(station_class) : 0;
}

void cleanup_hybrid_cocoon(void* station) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) callbacks->cleanup_cocoon(station);
}

void cleanup_hybrid_construction(void* station) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) callbacks->cleanup_construction(station);
}

}  // namespace a2fo
