#include "damage_controls.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>

using a2fo::weapon_damage_controls::DamagePolicy;
using a2fo::weapon_damage_controls::apply_policy_to_flags;
using a2fo::weapon_damage_controls::damage_scale_for_hit;
using a2fo::weapon_damage_controls::hull_spillover_scale;
using a2fo::weapon_damage_controls::kIgnoreShieldsFlag;
using a2fo::weapon_damage_controls::kShieldsOnlyFlag;

int main() {
    assert(apply_policy_to_flags(0, {true, true}, true) == 0);
    assert(apply_policy_to_flags(0, {true, true}, false) == 0);

    // A hull-capable weapon which cannot damage shields is stopped while any
    // shield strength remains, then damages hull normally once shields fall.
    assert(apply_policy_to_flags(0, {false, true}, true) ==
           (kIgnoreShieldsFlag | kShieldsOnlyFlag));
    assert(apply_policy_to_flags(0, {false, true}, false) == 0);

    assert(apply_policy_to_flags(0, {true, false}, true) ==
           kShieldsOnlyFlag);
    assert(apply_policy_to_flags(0, {true, false}, false) ==
           kShieldsOnlyFlag);
    assert(apply_policy_to_flags(0, {false, false}, true) ==
           (kIgnoreShieldsFlag | kShieldsOnlyFlag));
    assert(apply_policy_to_flags(0, {false, false}, false) ==
           kShieldsOnlyFlag);

    constexpr std::uint32_t unrelated = 0x1240u;
    assert(apply_policy_to_flags(unrelated, {false, false}, true) ==
           (unrelated | kIgnoreShieldsFlag | kShieldsOnlyFlag));

    // Enabling a channel never clears an existing native ordnance flag.
    assert(apply_policy_to_flags(
               kIgnoreShieldsFlag, {true, true}, true) ==
           kIgnoreShieldsFlag);
    assert(apply_policy_to_flags(
               kShieldsOnlyFlag, {true, true}, true) ==
           kShieldsOnlyFlag);

    DamagePolicy modifiers{};
    modifiers.shield_damage_modifier = 2.0f;
    modifiers.hull_damage_modifier = 0.5f;
    assert(damage_scale_for_hit(modifiers, true) == 2.0f);
    assert(damage_scale_for_hit(modifiers, false) == 0.5f);
    assert(hull_spillover_scale(modifiers, true) == 0.25f);
    assert(hull_spillover_scale(modifiers, false) == 1.0f);

    modifiers.can_damage_shields = false;
    assert(damage_scale_for_hit(modifiers, true) == 1.0f);
    assert(hull_spillover_scale(modifiers, true) == 1.0f);

    modifiers.can_damage_shields = true;
    modifiers.shield_damage_modifier = 0.0f;
    assert(damage_scale_for_hit(modifiers, true) == 0.0f);
    assert(hull_spillover_scale(modifiers, true) == 1.0f);

    std::puts("weapon damage control tests passed");
    return 0;
}
