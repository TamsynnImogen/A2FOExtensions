#pragma once

#include <algorithm>
#include <cmath>

namespace a2fo::cloak {

// Stock CloakingDevice fades between alpha 1 and 0.2. Rendering only reads
// this progression; ability timing and visibility remain engine-owned.
inline bool visibility(unsigned state, float alpha, float& result) noexcept {
    if (state == 2) {
        result = 0.0f;
        return true;
    }
    if ((state != 1 && state != 3) || !std::isfinite(alpha)) return false;
    result = std::max(0.0f, std::min(1.0f, (alpha - 0.2f) / 0.8f));
    return true;
}

// The prototype replaces only an authored opaque hull's admitted DOT3 draw.
// Effects/transparent materials and every unsupported context keep their draw.
constexpr bool eligible(bool enabled, bool dxvk, bool compositor_active,
                        bool matching_craft, unsigned cloak_state,
                        unsigned destination_blend) noexcept {
    return enabled && dxvk && compositor_active && matching_craft &&
           cloak_state >= 1 && cloak_state <= 3 &&
           destination_blend == 1; // D3DBLEND_ZERO
}

} // namespace a2fo::cloak
