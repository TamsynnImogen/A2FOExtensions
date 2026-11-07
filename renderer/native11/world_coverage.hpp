#pragma once
#include "material_snapshot.hpp"
#include "world_light_capture.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct CoveredWorldMember {
    WorldMember member{};
    bool visible=false; // Found in the visible capture, preferred on overlap.
    bool shadow_only=false; // Role in the selected capture; overlap uses visible role.
};
struct CoverageReport {
    std::size_t world_objects=0,observed_objects=0;
    std::vector<WorldMember> missing;
    std::vector<CoveredWorldMember> covered;
    // Completeness is confined to the checked GameObject list. Other game
    // feature, background, effect, and transient caster domains are not audited.
    bool game_objects_complete=false;
};

// Pure, owned telemetry. The report keeps IDs and roles, never capture pointers
// or engine memory. Callers must retain inputs and re-audit before submission.
// Failure leaves output unchanged; a successful partial report lists gaps.
bool audit_coverage(const BoundMaterialCapture& visible,const WorldLightCapture& world,
    const std::vector<std::shared_ptr<const BoundMaterialCapture>>& shadow_candidates,
    CoverageReport& output,std::string& error) noexcept;
} // namespace a2fo::native11
