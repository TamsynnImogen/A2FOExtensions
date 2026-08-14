#include "firing_cycle.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace a2fo::point_defense {
namespace {

bool valid_delay(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f;
}

bool whole_nonnegative(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f &&
        value <= static_cast<float>(
            std::numeric_limits<std::uint32_t>::max()) &&
        std::floor(value) == value;
}

}  // namespace

PolicyStatus build_policy(const OptionalNumber* numbered,
                          std::size_t numbered_count,
                          float legacy_delay,
                          const OptionalNumber& save_point,
                          const OptionalNumber& reset_time,
                          CyclePolicy* output) noexcept {
    if (!output) return PolicyStatus::missing_delay;
    *output = CyclePolicy{};

    if (numbered_count > kMaximumShotDelays) {
        numbered_count = kMaximumShotDelays;
    }

    std::size_t contiguous_count = 0;
    while (numbered && contiguous_count < numbered_count &&
           numbered[contiguous_count].present) {
        const OptionalNumber& setting = numbered[contiguous_count];
        if (!setting.valid || !valid_delay(setting.value)) {
            return PolicyStatus::invalid_delay;
        }
        ++contiguous_count;
    }
    for (std::size_t index = contiguous_count; index < numbered_count;
         ++index) {
        if (numbered[index].present) {
            return PolicyStatus::non_contiguous_delays;
        }
    }

    try {
        if (contiguous_count != 0) {
            output->delays.reserve(contiguous_count);
            for (std::size_t index = 0; index < contiguous_count; ++index) {
                output->delays.push_back(numbered[index].value);
            }
            output->uses_numbered_delays = true;
        } else {
            if (!valid_delay(legacy_delay)) {
                return PolicyStatus::missing_delay;
            }
            output->delays.push_back(legacy_delay);
        }
    } catch (...) {
        *output = CyclePolicy{};
        return PolicyStatus::missing_delay;
    }

    if (save_point.present) {
        if (!save_point.valid || !whole_nonnegative(save_point.value)) {
            *output = CyclePolicy{};
            return PolicyStatus::invalid_save_point;
        }
        const auto index = static_cast<std::size_t>(save_point.value);
        if (index >= output->delays.size()) {
            *output = CyclePolicy{};
            return PolicyStatus::invalid_save_point;
        }
        output->save_fire_cycle_point = index;
    }

    if (reset_time.present) {
        if (!reset_time.valid || !valid_delay(reset_time.value)) {
            *output = CyclePolicy{};
            return PolicyStatus::invalid_reset_time;
        }
        output->shot_cycle_reset_time = reset_time.value;
    }
    return PolicyStatus::valid;
}

float consume_delay(const CyclePolicy& policy,
                    CycleState* state) noexcept {
    if (!state || policy.delays.empty()) return 0.0f;
    normalize_state(policy, state);
    const float delay = policy.delays[state->next_delay_index];
    if (state->next_delay_index + 1 < policy.delays.size()) {
        ++state->next_delay_index;
    } else {
        state->next_delay_index = std::min(
            policy.save_fire_cycle_point, policy.delays.size() - 1);
    }
    state->idle_reset_remaining = policy.shot_cycle_reset_time;
    ++state->fire_count;
    return delay;
}

void update_idle_reset(const CyclePolicy& policy,
                       CycleState* state,
                       bool native_timer_advanced,
                       float native_timer_after,
                       float elapsed_seconds) noexcept {
    if (!state || policy.delays.empty() ||
        policy.shot_cycle_reset_time <= 0.0f ||
        !native_timer_advanced || !std::isfinite(native_timer_after) ||
        native_timer_after > 0.0f || !std::isfinite(elapsed_seconds) ||
        elapsed_seconds <= 0.0f) {
        return;
    }
    state->idle_reset_remaining -= elapsed_seconds;
    if (!std::isfinite(state->idle_reset_remaining) ||
        state->idle_reset_remaining <= 0.0f) {
        state->next_delay_index = 0;
        state->idle_reset_remaining = 0.0f;
    }
}

void normalize_state(const CyclePolicy& policy,
                     CycleState* state) noexcept {
    if (!state) return;
    if (policy.delays.empty() ||
        state->next_delay_index >= policy.delays.size()) {
        state->next_delay_index = 0;
    }
    if (!std::isfinite(state->idle_reset_remaining) ||
        state->idle_reset_remaining < 0.0f) {
        state->idle_reset_remaining = 0.0f;
    }
    if (policy.shot_cycle_reset_time <= 0.0f) {
        state->idle_reset_remaining = 0.0f;
    } else if (state->idle_reset_remaining >
               policy.shot_cycle_reset_time) {
        state->idle_reset_remaining = policy.shot_cycle_reset_time;
    }
}

}  // namespace a2fo::point_defense
