#pragma once

#include <cmath>

namespace a2fo {

// Validate the complete edit before publishing any facing or native total.
inline bool valid_shield_values(const float* current,
                                const float* maximum) noexcept {
    if (!current || !maximum) return false;
    float current_total = 0.0f;
    float maximum_total = 0.0f;
    for (unsigned i = 0; i < 4; ++i) {
        if (!std::isfinite(current[i]) || !std::isfinite(maximum[i]) ||
            maximum[i] <= 0.0f || current[i] < 0.0f ||
            current[i] > maximum[i]) return false;
        current_total += current[i];
        maximum_total += maximum[i];
    }
    return std::isfinite(current_total) && std::isfinite(maximum_total);
}

}  // namespace a2fo
