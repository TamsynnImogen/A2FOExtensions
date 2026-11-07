#pragma once

#include <cmath>

namespace a2fo {

// Validate the complete edit before publishing any facing or native total.
// A zero maximum represents an inactive directional-shield facing and is
// valid only when its current value is also exactly zero.
inline bool valid_shield_values(const float* current,
                                const float* maximum) noexcept {
    if (!current || !maximum) return false;
    float current_total = 0.0f;
    float maximum_total = 0.0f;
    bool any_active = false;
    for (unsigned i = 0; i < 4; ++i) {
        if (!std::isfinite(current[i]) || !std::isfinite(maximum[i]) ||
            maximum[i] < 0.0f || current[i] < 0.0f) {
            return false;
        }
        if (maximum[i] == 0.0f) {
            if (current[i] != 0.0f) return false;
        } else {
            if (current[i] > maximum[i]) return false;
            any_active = true;
        }
        current_total += current[i];
        maximum_total += maximum[i];
    }
    return any_active && std::isfinite(current_total) &&
        std::isfinite(maximum_total) && maximum_total > 0.0f;
}

}  // namespace a2fo
