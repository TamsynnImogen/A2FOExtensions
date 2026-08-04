#pragma once

#include "hybrid_production.hpp"

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <cstdint>

namespace a2fo {

// Installs the signature-checked ResearchStation runtime adapter. Supported
// hybrid hosts expose yardItem, researchItem, and evolveItem through separate
// native Build/Research/Evolve buttons. All three feed the inherited ten-slot
// Producer FIFO while only its front item executes. Evolution uses generic
// Producer timing, a protected cocoon sidecar, and a tail-safe final
// replacement handoff. Placement-based ConstructionRig orders use a separate
// protected sidecar for their native cursor, placeholder, and completion data.
bool initialize_hybrid_production_registry(const A2FO_ModuleApi* api,
                                           HMODULE armada,
                                           HMODULE fleet_ops) noexcept;

// Reads the explicit commands from the proven ResearchStation class callback.
// Keeping parsing on the exact supported host avoids intercepting every
// Producer-derived class during the engine's startup class-loading sweep.
bool register_research_station_hybrid_lists(
    void* station_class, void* parameter_db) noexcept;

// Resolves the supported construct/yard/research/evolve slice into separate
// stable runtime tables after Fleet Ops and upgrade-tier replacement have
// completed. Legacy research buildItem/tier tables remain untouched unless
// researchItemX exists.
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

// Evolution is a terminal FIFO barrier. These helpers let the synchronized
// queue/repeat layer reject trailing work and disable continuous refill using
// the same explicit-method registry as the runtime executor.
bool hybrid_production_is_evolve_target(
    void* producer, void* target_class) noexcept;
bool hybrid_production_has_evolution_barrier(void* producer) noexcept;

// True only during the synchronous GUI press which arms a hybrid
// constructItem placement cursor. The queue layer uses this as a safety gate:
// a build order emitted by the button itself is premature, while the later
// order emitted by the confirmed map placement is allowed normally.
bool hybrid_production_should_defer_construct_order(
    void* producer, void* target_class) noexcept;

// Completes the placement-command handoff after the synchronized Producer
// receiver returns. Every admitted construct item owns a native placement
// interface keyed to its queue ID, preserving its full position/rotation
// transform and cookie until that exact FIFO item completes or is removed.
void finalize_hybrid_construct_order(
    void* producer, void* target_class, bool admitted) noexcept;
void discard_hybrid_construct_placement(
    void* producer, std::uint32_t queue_id) noexcept;
void clear_hybrid_construct_placements(void* producer) noexcept;

// Records a local research-pod order so the next popup refresh restores the
// hybrid station's Research palette instead of dropping back to root.
void retain_hybrid_research_menu_after_order(
    void* producer, void* target_class) noexcept;

// Removes any hybrid cocoon while the ResearchStation object is still live.
// The station destructor hook calls this before native teardown because the
// temporary Evolver tail overlaps ResearchStation's attached-pod arrays.
void cleanup_hybrid_cocoon(void* station) noexcept;

// Removes any native ConstructionRig placeholder and destroys the protected
// placement interface before ResearchStation tears down its overlapping pod
// state.
void cleanup_hybrid_construction(void* station) noexcept;

}  // namespace a2fo
