#include "firing_cycle.hpp"
#include "../../rust/gameplay_ffi.h"
#include <algorithm>

namespace a2fo::point_defense {
namespace {
A2foRsOptionalNumber pod(const OptionalNumber &value) noexcept {
    return {static_cast<std::uint8_t>(value.present), static_cast<std::uint8_t>(value.valid),
            value.value};
}
A2foRsCycleState pod(const CycleState &state) noexcept {
    return {state.next_delay_index, state.idle_reset_remaining, state.fire_count};
}
void apply(CycleState *state, const A2foRsCycleState &value) noexcept {
    state->next_delay_index = value.next_delay_index;
    state->idle_reset_remaining = value.idle_reset_remaining;
    state->fire_count = value.fire_count;
}
} // namespace

PolicyStatus build_policy(const OptionalNumber *numbered, std::size_t numbered_count,
                          float legacy_delay, const OptionalNumber &save_point,
                          const OptionalNumber &reset_time, CyclePolicy *output) noexcept {
    if (!output)
        return PolicyStatus::missing_delay;
    *output = CyclePolicy{};
    A2foRsOptionalNumber input[kMaximumShotDelays]{};
    const std::size_t count = std::min(numbered_count, kMaximumShotDelays);
    if (numbered) {
        for (std::size_t i = 0; i < count; ++i)
            input[i] = pod(numbered[i]);
    }
    float delays[kMaximumShotDelays]{};
    std::size_t length = 0, save_index = 0;
    float reset = 0;
    std::uint8_t uses_numbered = 0;
    const auto status = static_cast<PolicyStatus>(a2fo_rs_gameplay_build_cycle(
        numbered ? input : nullptr, count, legacy_delay, pod(save_point), pod(reset_time), delays,
        &length, &save_index, &reset, &uses_numbered));
    if (status != PolicyStatus::valid)
        return status;
    try {
        output->delays.assign(delays, delays + length);
    } catch (...) {
        *output = CyclePolicy{};
        return PolicyStatus::missing_delay;
    }
    output->save_fire_cycle_point = save_index;
    output->shot_cycle_reset_time = reset;
    output->uses_numbered_delays = uses_numbered != 0;
    return PolicyStatus::valid;
}

float consume_delay(const CyclePolicy &policy, CycleState *state) noexcept {
    if (!state)
        return 0.0f;
    auto value = pod(*state);
    const float delay = a2fo_rs_gameplay_consume_cycle(policy.delays.data(), policy.delays.size(),
                                                       policy.save_fire_cycle_point,
                                                       policy.shot_cycle_reset_time, &value);
    apply(state, value);
    return delay;
}

void update_idle_reset(const CyclePolicy &policy, CycleState *state, bool native_timer_advanced,
                       float native_timer_after, float elapsed_seconds) noexcept {
    if (!state)
        return;
    auto value = pod(*state);
    a2fo_rs_gameplay_update_cycle(policy.delays.data(), policy.delays.size(),
                                  policy.shot_cycle_reset_time, &value, native_timer_advanced,
                                  native_timer_after, elapsed_seconds);
    apply(state, value);
}

void normalize_state(const CyclePolicy &policy, CycleState *state) noexcept {
    if (!state)
        return;
    auto value = pod(*state);
    a2fo_rs_gameplay_normalize_cycle(policy.delays.data(), policy.delays.size(),
                                     policy.save_fire_cycle_point, policy.shot_cycle_reset_time,
                                     &value);
    apply(state, value);
}
} // namespace a2fo::point_defense
