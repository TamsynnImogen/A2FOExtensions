/*
 * Pure policy for the alwaysShowShields visual effect. Keeping the decision
 * independent from Armada pointers makes its zero/default and shield-down
 * boundaries host-testable.
 */

#pragma once

#include <cstdint>

namespace a2fo::shields {

enum class EffectAction {
    none,
    show,
    hide,
};

// Chooses the one native effect operation needed for the current frame.
// effect_id follows Armada's convention: -1 means no persistent effect.
EffectAction choose_effect_action(bool configured,
                                  float current_shields,
                                  std::int32_t effect_id) noexcept;

}  // namespace a2fo::shields
