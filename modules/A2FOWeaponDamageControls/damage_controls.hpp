#pragma once

#include <cstdint>

namespace a2fo::weapon_damage_controls {

// These are existing Armada DamageInfo flags. Combining them suppresses the
// complete primary hit: ignore_shields prevents shield loss, then
// shields_only zeros the hull/system amount.
constexpr std::uint32_t kIgnoreShieldsFlag = 0x01u;
constexpr std::uint32_t kShieldsOnlyFlag = 0x80u;

struct DamagePolicy {
    bool can_damage_shields = true;
    bool can_damage_hull = true;
    float shield_damage_modifier = 1.0f;
    float hull_damage_modifier = 1.0f;
};

constexpr std::uint32_t apply_policy_to_flags(
    std::uint32_t flags, const DamagePolicy& policy,
    bool shields_up) noexcept {
    if (!policy.can_damage_shields && shields_up) {
        // A weapon which cannot damage shields is stopped by them; it does not
        // bypass them. The paired native flags leave both shield and hull
        // strength unchanged for this hit.
        flags |= kIgnoreShieldsFlag | kShieldsOnlyFlag;
    } else if (!policy.can_damage_hull) {
        // Shield damage remains native, but any spillover is discarded.
        flags |= kShieldsOnlyFlag;
    }
    return flags;
}

constexpr float damage_scale_for_hit(
    const DamagePolicy& policy, bool shields_up) noexcept {
    if (shields_up && policy.can_damage_shields) {
        return policy.shield_damage_modifier;
    }
    if (!shields_up) return policy.hull_damage_modifier;
    // The permission flags suppress this hit completely.
    return 1.0f;
}

constexpr float hull_spillover_scale(
    const DamagePolicy& policy, bool shields_up) noexcept {
    if (!shields_up || !policy.can_damage_shields ||
        !policy.can_damage_hull) {
        return 1.0f;
    }
    if (policy.shield_damage_modifier <= 0.0f) {
        // A zero-damage shield hit cannot generate hull spillover.
        return 1.0f;
    }
    return policy.hull_damage_modifier /
        policy.shield_damage_modifier;
}

}  // namespace a2fo::weapon_damage_controls
