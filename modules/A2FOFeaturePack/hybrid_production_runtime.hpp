#pragma once

#include "hybrid_production.hpp"

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <cstdint>

namespace a2fo {

// Installs the first signature-checked runtime adapter. ResearchStation hosts
// expose yardItem and researchItem through separate native Build/Research
// buttons. Fleet Ops normally overlays Build, Research, Evolve, and Trade on
// one root-palette control; hybrid stations move Build to its own otherwise
// unused control. Both lists feed the station's native ten-slot Producer FIFO,
// while only one queued item executes at a time. Construction/evolution remain
// pending.
bool initialize_hybrid_production_registry(const A2FO_ModuleApi* api,
                                           HMODULE armada,
                                           HMODULE fleet_ops) noexcept;

// Reads the explicit commands from the proven ResearchStation class callback.
// Keeping parsing on the exact supported host avoids intercepting every
// Producer-derived class during the engine's startup class-loading sweep.
bool register_research_station_hybrid_lists(
    void* station_class, void* parameter_db) noexcept;

// Resolves the first supported yard/research slice into separate stable runtime
// tables after Fleet Ops and upgrade-tier replacement have completed. Legacy
// research buildItem/tier tables remain untouched unless researchItemX exists.
std::size_t publish_research_station_hybrid_items(
    void* station_class) noexcept;

// Resolves explicit list membership captured for a Producer class. Returning
// false means the caller should use legacy classLabel behavior.
bool resolve_explicit_production_method(void* producer_class,
                                        std::uint32_t target_project_id,
                                        ProductionMethod& method) noexcept;

// Extends ResearchStation's attached-pod uniqueness rule across the hybrid
// station's active job and inherited Producer FIFO. This is shared with the
// synchronized command receiver so a disabled UI button cannot be bypassed by
// a direct or networked build order.
bool hybrid_production_has_queued_research_conflict(
    void* producer, void* target_class) noexcept;

// Records a local research-pod order so the next popup refresh restores the
// hybrid station's Research palette instead of dropping back to root.
void retain_hybrid_research_menu_after_order(
    void* producer, void* target_class) noexcept;

}  // namespace a2fo
