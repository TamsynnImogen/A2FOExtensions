/*
 * Host-testable alwaysShowShields state transition policy.
 */

#include "shield_visibility.hpp"

#include <cmath>

namespace a2fo::shields {

EffectAction choose_effect_action(bool configured,
                                  float current_shields,
                                  std::int32_t effect_id) noexcept {
    if (!configured) return EffectAction::none;

    const bool shields_up =
        std::isfinite(current_shields) && current_shields > 0.0f;
    if (shields_up && effect_id < 0) return EffectAction::show;
    if (!shields_up && effect_id >= 0) return EffectAction::hide;
    return EffectAction::none;
}

bool global_scan_due(std::uint32_t now,
                     std::uint32_t last_scan,
                     bool has_scanned,
                     std::uint32_t interval) noexcept {
    return !has_scanned || interval == 0 || now - last_scan >= interval;
}

}  // namespace a2fo::shields
