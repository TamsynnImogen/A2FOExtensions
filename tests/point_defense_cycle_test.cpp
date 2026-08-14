#include "firing_cycle.hpp"

#include <array>
#include <cassert>
#include <cmath>

int main() {
    using namespace a2fo::point_defense;

    std::array<OptionalNumber, kMaximumShotDelays> settings{};
    settings[0] = {true, true, 0.1f};
    settings[1] = {true, true, 0.1f};
    settings[2] = {true, true, 2.0f};
    CyclePolicy policy{};
    assert(build_policy(settings.data(), settings.size(), 9.0f,
                        {true, true, 2.0f}, {true, true, 15.0f},
                        &policy) == PolicyStatus::valid);
    assert(policy.uses_numbered_delays);
    assert(policy.delays.size() == 3);

    CycleState state{};
    assert(std::fabs(consume_delay(policy, &state) - 0.1f) < 0.0001f);
    assert(state.next_delay_index == 1);
    assert(std::fabs(consume_delay(policy, &state) - 0.1f) < 0.0001f);
    assert(state.next_delay_index == 2);
    assert(std::fabs(consume_delay(policy, &state) - 2.0f) < 0.0001f);
    assert(state.next_delay_index == 2);
    assert(std::fabs(consume_delay(policy, &state) - 2.0f) < 0.0001f);
    assert(state.fire_count == 4);

    update_idle_reset(policy, &state, true, 0.5f, 20.0f);
    assert(state.next_delay_index == 2);
    update_idle_reset(policy, &state, true, 0.0f, 10.0f);
    assert(state.next_delay_index == 2);
    assert(std::fabs(state.idle_reset_remaining - 5.0f) < 0.0001f);
    update_idle_reset(policy, &state, true, -0.1f, 5.0f);
    assert(state.next_delay_index == 0);
    assert(state.idle_reset_remaining == 0.0f);

    // No numbered command leaves the native unnumbered delay as a one-entry
    // sequence. Numbered commands always take precedence over it.
    settings.fill(OptionalNumber{});
    assert(build_policy(settings.data(), settings.size(), 3.5f,
                        {}, {}, &policy) == PolicyStatus::valid);
    assert(!policy.uses_numbered_delays);
    assert(policy.delays.size() == 1);
    assert(policy.delays[0] == 3.5f);

    settings[0] = {true, true, 0.2f};
    settings[2] = {true, true, 0.4f};
    assert(build_policy(settings.data(), settings.size(), 3.5f,
                        {}, {}, &policy) ==
           PolicyStatus::non_contiguous_delays);
    settings[2] = {};
    settings[0] = {true, true, -0.2f};
    assert(build_policy(settings.data(), settings.size(), 3.5f,
                        {}, {}, &policy) == PolicyStatus::invalid_delay);
    settings[0] = {true, true, 0.2f};
    assert(build_policy(settings.data(), settings.size(), 3.5f,
                        {true, true, 1.0f}, {}, &policy) ==
           PolicyStatus::invalid_save_point);

    settings.fill(OptionalNumber{});
    assert(build_policy(settings.data(), settings.size(), 1.0f,
                        {}, {true, true, 5.0f}, &policy) ==
           PolicyStatus::valid);
    state.next_delay_index = 900;
    state.idle_reset_remaining = 50.0f;
    normalize_state(policy, &state);
    assert(state.next_delay_index == 0);
    assert(state.idle_reset_remaining == 5.0f);

    return 0;
}
