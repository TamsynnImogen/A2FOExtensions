#include "directional_shields.hpp"

#include <algorithm>
#include <cmath>

namespace a2fo::directional_shields {
namespace {

constexpr float kDirectionEpsilon = 0.000001f;
constexpr float kAmountEpsilon = 0.0001f;

float finite_non_negative(float value) noexcept {
    return std::isfinite(value) && value > 0.0f ? value : 0.0f;
}

float clamp_total(float value, float maximum) noexcept {
    if (!std::isfinite(value) || value <= 0.0f) return 0.0f;
    return std::min(value, maximum);
}

void clamp_stores(ShieldStores* stores,
                  const ShieldPolicy& policy) noexcept {
    if (!stores) return;
    for (std::size_t index = 0; index < kFacingCount; ++index) {
        const float value = stores->current[index];
        stores->current[index] = std::isfinite(value)
            ? std::max(0.0f, std::min(value, policy.maximum[index]))
            : 0.0f;
    }
}

float dot_axis(const float direction[3], const Matrix34& transform,
               std::size_t axis) noexcept {
    const std::size_t offset = axis * 3;
    return direction[0] * transform.values[offset] +
        direction[1] * transform.values[offset + 1] +
        direction[2] * transform.values[offset + 2];
}

bool select_horizontal_facing(float local_right, float local_forward,
                              Facing* facing) noexcept {
    if (!facing) return false;
    const float horizontal_length_squared =
        local_right * local_right + local_forward * local_forward;
    if (!std::isfinite(horizontal_length_squared) ||
        horizontal_length_squared <= kDirectionEpsilon) {
        return false;
    }

    if (std::fabs(local_forward) >= std::fabs(local_right)) {
        *facing = local_forward >= 0.0f
            ? Facing::forward : Facing::aft;
    } else {
        *facing = local_right >= 0.0f
            ? Facing::starboard : Facing::port;
    }
    return true;
}

}  // namespace

std::size_t facing_index(Facing facing) noexcept {
    const std::size_t index = static_cast<std::size_t>(facing);
    return index < kFacingCount ? index : 0;
}

ShieldPolicy normalize_policy(ShieldPolicy policy) noexcept {
    for (float& maximum : policy.maximum) {
        maximum = finite_non_negative(maximum);
    }
    return policy;
}

bool valid_policy(const ShieldPolicy& policy) noexcept {
    for (float maximum : policy.maximum) {
        if (!std::isfinite(maximum) || maximum <= 0.0f) return false;
    }
    return true;
}

float total_capacity(const ShieldPolicy& policy) noexcept {
    float total = 0.0f;
    for (float maximum : policy.maximum) {
        total += finite_non_negative(maximum);
    }
    return total;
}

float total_current(const ShieldStores& stores) noexcept {
    float total = 0.0f;
    for (float current : stores.current) {
        total += finite_non_negative(current);
    }
    return total;
}

float effect_aggregate_for_facing(
    const ShieldPolicy& raw_policy, const ShieldStores& stores,
    Facing facing) noexcept {
    const ShieldPolicy policy = normalize_policy(raw_policy);
    const std::size_t index = facing_index(facing);
    const float facing_maximum = policy.maximum[index];
    const float aggregate_maximum = total_capacity(policy);
    if (facing_maximum <= 0.0f || aggregate_maximum <= 0.0f) return 0.0f;
    const float facing_current = std::isfinite(stores.current[index])
        ? std::max(0.0f, std::min(stores.current[index], facing_maximum))
        : 0.0f;
    return aggregate_maximum * (facing_current / facing_maximum);
}

bool should_clear_native_collapse_effect(
    float facing_remaining, std::int32_t effect_id) noexcept {
    return effect_id >= 0 &&
        (!std::isfinite(facing_remaining) ||
         facing_remaining <= kAmountEpsilon);
}

bool should_suppress_native_impact_effect(
    std::int32_t shield_type, bool facing_resolved,
    float facing_remaining) noexcept {
    constexpr std::int32_t kNormalImpactShieldType = 0;
    return shield_type == kNormalImpactShieldType && facing_resolved &&
        (!std::isfinite(facing_remaining) ||
         facing_remaining <= kAmountEpsilon);
}

bool select_facing(const Matrix34& target_transform,
                   const float attacker_position[3],
                   Facing* facing) noexcept {
    if (!attacker_position || !facing) return false;
    const float direction[3]{
        attacker_position[0] - target_transform.values[9],
        attacker_position[1] - target_transform.values[10],
        attacker_position[2] - target_transform.values[11],
    };
    const float local_right = dot_axis(direction, target_transform, 0);
    const float local_forward = dot_axis(direction, target_transform, 2);
    return select_horizontal_facing(local_right, local_forward, facing);
}

bool select_local_effect_facing(const Matrix34& shield_transform,
                                Facing* facing) noexcept {
    return select_horizontal_facing(
        shield_transform.values[9], shield_transform.values[11], facing);
}

void initialize_from_total(ShieldStores* stores,
                           const ShieldPolicy& raw_policy,
                           float native_total) noexcept {
    if (!stores) return;
    const ShieldPolicy policy = normalize_policy(raw_policy);
    const float maximum = total_capacity(policy);
    const float target = clamp_total(native_total, maximum);
    if (maximum <= 0.0f || target <= 0.0f) {
        stores->current.fill(0.0f);
        return;
    }
    const float ratio = target / maximum;
    for (std::size_t index = 0; index < kFacingCount; ++index) {
        stores->current[index] = policy.maximum[index] * ratio;
    }
}

void reconcile_total(ShieldStores* stores,
                     const ShieldPolicy& raw_policy,
                     float native_total) noexcept {
    if (!stores) return;
    const ShieldPolicy policy = normalize_policy(raw_policy);
    clamp_stores(stores, policy);
    const float maximum = total_capacity(policy);
    const float target = clamp_total(native_total, maximum);
    float current = total_current(*stores);
    if (std::fabs(target - current) <= kAmountEpsilon) return;

    if (target < current) {
        if (target <= 0.0f || current <= 0.0f) {
            stores->current.fill(0.0f);
            return;
        }
        const float ratio = target / current;
        for (float& value : stores->current) value *= ratio;
        return;
    }

    float remaining = target - current;
    for (std::size_t pass = 0;
         pass < kFacingCount && remaining > kAmountEpsilon; ++pass) {
        std::size_t depleted = 0;
        for (std::size_t index = 0; index < kFacingCount; ++index) {
            if (policy.maximum[index] - stores->current[index] >
                kAmountEpsilon) {
                ++depleted;
            }
        }
        if (depleted == 0) break;
        const float share = remaining / static_cast<float>(depleted);
        float applied = 0.0f;
        for (std::size_t index = 0; index < kFacingCount; ++index) {
            const float deficit =
                policy.maximum[index] - stores->current[index];
            if (deficit <= kAmountEpsilon) continue;
            const float addition = std::min(deficit, share);
            stores->current[index] += addition;
            applied += addition;
        }
        if (applied <= kAmountEpsilon) break;
        remaining -= applied;
    }
    clamp_stores(stores, policy);
}

}  // namespace a2fo::directional_shields
