/* Host-testable policy for ODF-driven wreckage replacements. */

#pragma once

#include <cstdint>
#include <string_view>

namespace a2fo::wreckage {

enum class DecisionStatus {
    none,
    replace,
    invalid_chance,
};

struct Decision {
    DecisionStatus status = DecisionStatus::none;
    std::string_view replacement_odf{};
};

// `wreckageChance` defaults to 100 when the field is absent. Its accepted
// range is 0 through 100 inclusive. The roll is deterministic for a supplied
// object-destruction seed so synchronized games reach the same decision.
Decision decide_replacement(
    std::string_view wreckage,
    bool chance_present,
    std::string_view chance_text,
    std::uint32_t random_seed) noexcept;

}  // namespace a2fo::wreckage
