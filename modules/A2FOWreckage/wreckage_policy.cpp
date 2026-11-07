// Borrowed string-view adapter for Rust's deterministic replacement policy.
#include "wreckage_policy.hpp"
#include "../../rust/wreckage_ffi.h"

namespace a2fo::wreckage {
Decision decide_replacement(std::string_view wreckage, bool present,
    std::string_view chance, std::uint32_t seed) noexcept {
    std::size_t offset = 0, length = 0;
    const auto status = a2fo_rs_wreckage_decide(
        reinterpret_cast<const std::uint8_t*>(wreckage.data()), wreckage.size(), present,
        reinterpret_cast<const std::uint8_t*>(chance.data()), chance.size(), seed, &offset, &length);
    if (status == 1) return {DecisionStatus::replace, wreckage.substr(offset, length)};
    return {status == 2 ? DecisionStatus::invalid_chance : DecisionStatus::none, {}};
}
} // namespace a2fo::wreckage
