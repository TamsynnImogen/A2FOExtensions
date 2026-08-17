#include "../modules/A2FODirectionalShields/directional_shields.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using a2fo::directional_shields::Facing;
using a2fo::directional_shields::Matrix34;
using a2fo::directional_shields::ShieldPolicy;
using a2fo::directional_shields::ShieldStores;

namespace {

bool close(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

}  // namespace

int main() {
    const Matrix34 identity{{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        10.0f, 20.0f, 30.0f,
    }};
    Facing facing = Facing::aft;
    const float forward[3]{10.0f, 20.0f, 130.0f};
    const float aft[3]{10.0f, 20.0f, -70.0f};
    const float port[3]{-90.0f, 20.0f, 30.0f};
    const float starboard[3]{110.0f, 20.0f, 30.0f};
    assert(a2fo::directional_shields::select_facing(
        identity, forward, &facing) && facing == Facing::forward);
    assert(a2fo::directional_shields::select_facing(
        identity, aft, &facing) && facing == Facing::aft);
    assert(a2fo::directional_shields::select_facing(
        identity, port, &facing) && facing == Facing::port);
    assert(a2fo::directional_shields::select_facing(
        identity, starboard, &facing) && facing == Facing::starboard);

    // Forward/aft own exact diagonal boundaries.
    const float forward_starboard_boundary[3]{
        110.0f, 20.0f, 130.0f};
    assert(a2fo::directional_shields::select_facing(
        identity, forward_starboard_boundary, &facing));
    assert(facing == Facing::forward);
    const float directly_above[3]{10.0f, 120.0f, 30.0f};
    assert(!a2fo::directional_shields::select_facing(
        identity, directly_above, &facing));

    Matrix34 local_effect{};
    local_effect.values[9] = 4.0f;
    local_effect.values[11] = 10.0f;
    assert(a2fo::directional_shields::select_local_effect_facing(
        local_effect, &facing) && facing == Facing::forward);
    local_effect.values[9] = -10.0f;
    local_effect.values[11] = 4.0f;
    assert(a2fo::directional_shields::select_local_effect_facing(
        local_effect, &facing) && facing == Facing::port);
    local_effect.values[9] = 10.0f;
    local_effect.values[11] = -4.0f;
    assert(a2fo::directional_shields::select_local_effect_facing(
        local_effect, &facing) && facing == Facing::starboard);
    local_effect.values[9] = -4.0f;
    local_effect.values[11] = -10.0f;
    assert(a2fo::directional_shields::select_local_effect_facing(
        local_effect, &facing) && facing == Facing::aft);
    local_effect.values[9] = 0.0f;
    local_effect.values[11] = 0.0f;
    assert(!a2fo::directional_shields::select_local_effect_facing(
        local_effect, &facing));

    ShieldPolicy invalid{{200.0f, -1.0f, 150.0f, 150.0f}};
    assert(!a2fo::directional_shields::valid_policy(invalid));
    invalid = a2fo::directional_shields::normalize_policy(invalid);
    assert(invalid.maximum[1] == 0.0f);
    assert(!a2fo::directional_shields::valid_policy(invalid));

    const ShieldPolicy policy{{200.0f, 150.0f, 150.0f, 150.0f}};
    assert(a2fo::directional_shields::valid_policy(policy));
    assert(close(a2fo::directional_shields::total_capacity(policy), 650.0f));

    // The native collapse-effect handle is cleared only after the struck
    // facing is depleted. A missing native effect remains a no-op.
    assert(!a2fo::directional_shields::should_clear_native_collapse_effect(
        1.0f, 12));
    assert(a2fo::directional_shields::should_clear_native_collapse_effect(
        0.0f, 12));
    assert(a2fo::directional_shields::should_clear_native_collapse_effect(
        -1.0f, 12));
    assert(!a2fo::directional_shields::should_clear_native_collapse_effect(
        0.0f, -1));

    // Only the normal type-0 hit flare is face-filtered. Collapse and
    // alwaysShowShields effects retain their dedicated ownership paths.
    assert(!a2fo::directional_shields::
        should_suppress_native_impact_effect(0, true, 1.0f));
    assert(a2fo::directional_shields::
        should_suppress_native_impact_effect(0, true, 0.0f));
    assert(!a2fo::directional_shields::
        should_suppress_native_impact_effect(0, false, 0.0f));
    assert(!a2fo::directional_shields::
        should_suppress_native_impact_effect(1, true, 0.0f));
    assert(!a2fo::directional_shields::
        should_suppress_native_impact_effect(7, true, 0.0f));

    ShieldStores stores{};
    a2fo::directional_shields::initialize_from_total(
        &stores, policy, 325.0f);
    assert(close(stores.current[0], 100.0f));
    assert(close(stores.current[1], 75.0f));
    assert(close(stores.current[2], 75.0f));
    assert(close(stores.current[3], 75.0f));
    assert(close(a2fo::directional_shields::effect_aggregate_for_facing(
        policy, stores, Facing::forward), 325.0f));
    stores.current[0] = 50.0f;
    assert(close(a2fo::directional_shields::effect_aggregate_for_facing(
        policy, stores, Facing::forward), 162.5f));
    stores.current[0] = 100.0f;

    // Aggregate damage not associated with a weapon preserves facing ratios.
    a2fo::directional_shields::reconcile_total(&stores, policy, 162.5f);
    assert(close(stores.current[0], 50.0f));
    assert(close(stores.current[1], 37.5f));
    assert(close(a2fo::directional_shields::total_current(stores), 162.5f));

    // Native recharge is shared equally between all depleted facings.
    a2fo::directional_shields::reconcile_total(&stores, policy, 202.5f);
    assert(close(stores.current[0], 60.0f));
    assert(close(stores.current[1], 47.5f));
    assert(close(stores.current[2], 47.5f));
    assert(close(stores.current[3], 47.5f));

    // Reconciliation cannot exceed the configured total.
    a2fo::directional_shields::reconcile_total(&stores, policy, 10000.0f);
    assert(close(a2fo::directional_shields::total_current(stores), 650.0f));
    assert(close(stores.current[0], 200.0f));
    assert(close(stores.current[1], 150.0f));

    std::puts("directional shield tests passed");
    return 0;
}
