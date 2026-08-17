#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace a2fo::directional_shields {

enum class Facing : std::uint32_t {
    forward = 0,
    aft = 1,
    port = 2,
    starboard = 3,
};

constexpr std::size_t kFacingCount = 4;

// Armada Matrix34 layout: world-space right, up, and forward axes followed
// by translation.
struct Matrix34 {
    float values[12]{};
};

struct ShieldPolicy {
    std::array<float, kFacingCount> maximum{};
};

struct ShieldStores {
    std::array<float, kFacingCount> current{};
};

std::size_t facing_index(Facing facing) noexcept;
ShieldPolicy normalize_policy(ShieldPolicy policy) noexcept;
bool valid_policy(const ShieldPolicy& policy) noexcept;
float total_capacity(const ShieldPolicy& policy) noexcept;
float total_current(const ShieldStores& stores) noexcept;

// Converts one facing's percentage back into the equivalent native aggregate
// value. Presenting this value briefly to ShieldEffect makes Armada's stock
// red/green flare gradient follow the impacted facing rather than the sum of
// every facing.
float effect_aggregate_for_facing(
    const ShieldPolicy& policy, const ShieldStores& stores,
    Facing facing) noexcept;

// Armada creates a native shield-collapse effect when the shield value it
// sees reaches zero. Directional damage must remove that effect while the
// struck facing is empty, even if another facing keeps the aggregate pool up.
bool should_clear_native_collapse_effect(
    float facing_remaining, std::int32_t effect_id) noexcept;

// Type 0 is Armada's ordinary xshldx01 weapon-impact flare. It is the only
// CreateShieldHit type suppressed by the directional runtime; type 1 is the
// native collapse effect and type 7 may be owned by alwaysShowShields.
bool should_suppress_native_impact_effect(
    std::int32_t shield_type, bool facing_resolved,
    float facing_remaining) noexcept;

// Chooses the dominant horizontal quadrant in target-local space. Exact
// diagonal boundaries belong to forward/aft, keeping the result stable.
bool select_facing(const Matrix34& target_transform,
                   const float attacker_position[3],
                   Facing* facing) noexcept;

// ShieldEffect receives a matrix already expressed in the target craft's
// local space. Its translation therefore identifies the impacted facing
// directly and must not be transformed as another world-space position.
bool select_local_effect_facing(const Matrix34& shield_transform,
                                Facing* facing) noexcept;

// Initializes a newly observed craft from Armada's aggregate native shield
// value, preserving the same percentage in every facing.
void initialize_from_total(ShieldStores* stores,
                           const ShieldPolicy& policy,
                           float native_total) noexcept;

// Reconciles native aggregate changes. Damage not associated with a weapon is
// shared proportionally; native recharge is spread evenly over depleted
// facings. Values remain clamped to their configured maxima.
void reconcile_total(ShieldStores* stores,
                     const ShieldPolicy& policy,
                     float native_total) noexcept;

}  // namespace a2fo::directional_shields
