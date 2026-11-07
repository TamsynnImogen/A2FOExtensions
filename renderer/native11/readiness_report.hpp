#pragma once
#include "observed_world.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Owned diagnostic for the checked GameObject list and three explicitly
// certified-absent caster domains. It is no claim of full game coverage,
// Windows presentation, or game acceptance. IDs are opaque local telemetry.
struct ReadinessReport {
    std::uint64_t generation=0;
    std::uint32_t camera=0;
    std::size_t world_objects=0,observed_objects=0,missing_objects=0;
    bool game_objects_complete=false;
    bool observed_domain_ready=false;
    std::vector<std::string> gates;
    std::vector<WorldMember> missing_members;
};

// Transactional: failure leaves output unchanged. No engine address is read.
bool make_readiness_report(std::uint32_t camera,std::uint64_t generation,
    const CoverageReport&,DomainEvidence,ReadinessReport& output,
    std::string& error) noexcept;
// Deterministic, at most 1 MiB; failure leaves output unchanged.
bool format_readiness_report(const ReadinessReport&,std::string& output,
    std::string& error) noexcept;
} // namespace a2fo::native11
