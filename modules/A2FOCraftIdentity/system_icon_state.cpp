#include "system_icon_state.hpp"

#include <algorithm>
#include <cmath>

namespace a2fo::craft_identity {

bool classify_system_icon_state(
    bool operational, bool forced_disabled,
    std::int32_t maximum_hitpoints, double current_hitpoints,
    float disable_time, SystemIconState* output) noexcept {
    if (!output || maximum_hitpoints <= 0 ||
        !std::isfinite(current_hitpoints) ||
        !std::isfinite(disable_time)) {
        return false;
    }

    const double maximum = static_cast<double>(maximum_hitpoints);
    if (current_hitpoints <= 0.0) {
        *output = SystemIconState::destroyed;
        return true;
    }
    if (!operational) {
        // A control/timed disable retains the part. Native damage with neither
        // disable marker remains destroyed until the operational byte returns.
        *output = (forced_disabled || disable_time > 0.0f ||
                   current_hitpoints + 0.0001 >= maximum)
            ? SystemIconState::disabled
            : SystemIconState::destroyed;
        return true;
    }

    const double ratio = current_hitpoints / maximum;
    if (!std::isfinite(ratio)) return false;
    if (ratio <= 0.25) {
        *output = SystemIconState::critical;
    } else if (ratio <= 0.50) {
        *output = SystemIconState::low;
    } else {
        *output = SystemIconState::healthy;
    }
    return true;
}

std::array<float, 3> tint_system_icon_colour(
    const std::array<float, 3>& native_colour,
    const std::array<float, 3>& configured_colour) noexcept {
    for (float channel : native_colour) {
        if (!std::isfinite(channel)) return native_colour;
    }
    for (float channel : configured_colour) {
        if (!std::isfinite(channel)) return native_colour;
    }

    const float native_intensity = std::clamp(
        std::max({native_colour[0], native_colour[1], native_colour[2]}),
        0.0f, 1.0f);
    const float configured_peak = std::clamp(
        std::max({configured_colour[0], configured_colour[1],
                  configured_colour[2]}),
        0.0f, 1.0f);
    if (native_intensity <= 0.0f || configured_peak <= 0.0f) {
        return {{0.0f, 0.0f, 0.0f}};
    }

    const float scale = native_intensity / configured_peak;
    return {{
        std::clamp(configured_colour[0] * scale, 0.0f, 1.0f),
        std::clamp(configured_colour[1] * scale, 0.0f, 1.0f),
        std::clamp(configured_colour[2] * scale, 0.0f, 1.0f),
    }};
}

}  // namespace a2fo::craft_identity
