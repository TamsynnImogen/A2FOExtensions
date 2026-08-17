#include "wreckage_policy.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace a2fo::wreckage {
namespace {

std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t' ||
            value.front() == '\r' || value.front() == '\n')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' ||
            value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }
    return value;
}

bool parse_chance(std::string_view text, float* chance) noexcept {
    if (!chance) return false;
    text = trim(text);
    if (text.empty() || text.size() >= 64) return false;
    char buffer[64]{};
    std::memcpy(buffer, text.data(), text.size());
    char* end = nullptr;
    const float parsed = std::strtof(buffer, &end);
    if (!end || end == buffer || *end != '\0' || !std::isfinite(parsed) ||
        parsed < 0.0f || parsed > 100.0f) {
        return false;
    }
    *chance = parsed;
    return true;
}

std::uint32_t mix_random_seed(std::uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

bool roll_percent(std::uint32_t seed, float chance) noexcept {
    const double roll = static_cast<double>(mix_random_seed(seed)) *
        (100.0 / 4294967296.0);
    return roll < static_cast<double>(chance);
}

}  // namespace

Decision decide_replacement(
    std::string_view wreckage,
    bool chance_present,
    std::string_view chance_text,
    std::uint32_t random_seed) noexcept {
    wreckage = trim(wreckage);
    if (wreckage.empty()) return {};

    float chance = 100.0f;
    if (chance_present && !parse_chance(chance_text, &chance)) {
        return {DecisionStatus::invalid_chance, {}};
    }
    if (!roll_percent(random_seed, chance)) return {};
    return {DecisionStatus::replace, wreckage};
}

}  // namespace a2fo::wreckage
