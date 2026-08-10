/*
 * File: modules/A2FOFeaturePack/hybrid_bridge_client.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Bridge API declarations that let FeaturePack talk to HybridBuild runtime policies through callbacks.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo {

bool hybrid_production_has_queued_research_conflict(
    void* producer, void* target_class) noexcept;
bool hybrid_production_is_evolve_target(
    void* producer, void* target_class) noexcept;
bool hybrid_production_has_evolution_barrier(void* producer) noexcept;
bool hybrid_production_should_defer_construct_order(
    void* producer, void* target_class) noexcept;
void finalize_hybrid_construct_order(
    void* producer, void* target_class, bool admitted) noexcept;
void discard_hybrid_construct_placement(
    void* producer, std::uint32_t queue_id) noexcept;
void clear_hybrid_construct_placements(void* producer) noexcept;
void retain_hybrid_research_menu_after_order(
    void* producer, void* target_class) noexcept;
bool register_research_station_hybrid_lists(
    void* station_class, void* parameter_db) noexcept;
std::size_t publish_research_station_hybrid_items(
    void* station_class) noexcept;
void cleanup_hybrid_cocoon(void* station) noexcept;
void cleanup_hybrid_construction(void* station) noexcept;

}  // namespace a2fo

