/*
 * Pure firing-cycle policy shared by the PointDefenseLaser and
 * OrdnanceDefenseWeapon runtime hooks.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace a2fo::point_defense {

constexpr std::size_t kMaximumShotDelays = 64;

struct OptionalNumber {
    bool present = false;
    bool valid = false;
    float value = 0.0f;
};

struct CyclePolicy {
    std::vector<float> delays;
    std::size_t save_fire_cycle_point = 0;
    float shot_cycle_reset_time = 0.0f;
    bool uses_numbered_delays = false;
};

struct CycleState {
    std::size_t next_delay_index = 0;
    float idle_reset_remaining = 0.0f;
    std::uint64_t fire_count = 0;
};

enum class PolicyStatus {
    valid,
    missing_delay,
    non_contiguous_delays,
    invalid_delay,
    invalid_save_point,
    invalid_reset_time,
};

// Builds the effective sequence. Numbered settings take precedence over the
// legacy delay and must form one contiguous shotDelay0..X run.
PolicyStatus build_policy(const OptionalNumber* numbered,
                          std::size_t numbered_count,
                          float legacy_delay,
                          const OptionalNumber& save_point,
                          const OptionalNumber& reset_time,
                          CyclePolicy* output) noexcept;

// Returns the delay for this firing attempt, advances the next index once,
// and restarts the idle-reset countdown.
float consume_delay(const CyclePolicy& policy,
                    CycleState* state) noexcept;

// CannonImp counts reset time only after the weapon's active delay has
// elapsed. A completed countdown starts the sequence from shotDelay0.
void update_idle_reset(const CyclePolicy& policy,
                       CycleState* state,
                       bool native_timer_advanced,
                       float native_timer_after,
                       float elapsed_seconds) noexcept;

// Sanitizes state restored from a save before it is used by the runtime.
void normalize_state(const CyclePolicy& policy,
                     CycleState* state) noexcept;

}  // namespace a2fo::point_defense
