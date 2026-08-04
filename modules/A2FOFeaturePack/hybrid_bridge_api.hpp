#pragma once

#include <cstddef>
#include <cstdint>

#define A2FO_HYBRID_BRIDGE_VERSION 1u

// A2FOFeaturePack owns the general Producer queue and ResearchStation class
// hooks. A2FOHybridBuild supplies the optional policy callbacks below so those
// shared hooks do not need to be installed twice by separate DLLs.
struct A2FO_HybridBridge {
    std::uint32_t struct_size;
    std::uint32_t version;

    bool (*has_queued_research_conflict)(void* producer,
                                         void* target_class) noexcept;
    bool (*is_evolve_target)(void* producer,
                             void* target_class) noexcept;
    bool (*has_evolution_barrier)(void* producer) noexcept;
    bool (*should_defer_construct_order)(void* producer,
                                         void* target_class) noexcept;
    void (*finalize_construct_order)(void* producer, void* target_class,
                                     bool admitted) noexcept;
    void (*discard_construct_placement)(void* producer,
                                        std::uint32_t queue_id) noexcept;
    void (*clear_construct_placements)(void* producer) noexcept;
    void (*retain_research_menu_after_order)(void* producer,
                                              void* target_class) noexcept;
    bool (*register_research_station_lists)(void* station_class,
                                            void* parameter_db) noexcept;
    std::size_t (*publish_research_station_items)(
        void* station_class) noexcept;
    void (*cleanup_cocoon)(void* station) noexcept;
    void (*cleanup_construction)(void* station) noexcept;
};

using A2FO_RegisterHybridBridgeFn = bool (__cdecl*)(
    const A2FO_HybridBridge* bridge);

