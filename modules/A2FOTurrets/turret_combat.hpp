/*
 * Host-testable combat-state rules for A2FOTurrets.
 *
 * The engine-facing module supplies command fields and validates handles;
 * these helpers decide whether a command may become an inherited weapon
 * target and how the visual target changes when that order begins or ends.
 */

#pragma once

#include <cstdint>

namespace a2fo::turrets {

// Confirmed from Armada's native target-acquisition call sites: after an
// isEnemy check, the engine queues AiCommand 6 against the chosen object.
constexpr std::uint32_t kAttackCommand = 6;

// Armada command targets use form 2 for an object handle. Vector and empty
// commands use other forms and must never be interpreted as attack orders.
constexpr std::uint32_t kObjectCommandTargetForm = 2;

std::uint32_t attack_order_target_handle(
    std::uint32_t command_id,
    std::uint32_t target_form,
    std::uint32_t target_handle,
    bool target_exists) noexcept;

struct OrderTargetState {
    std::uint32_t ordered_target_handle = 0;
    std::uint32_t visual_target_handle = 0;
};

// Starting an explicit Attack order immediately makes it the visual target.
// Ending that order clears the visual target only when it still came from the
// old order; a newer independently selected native target is preserved.
OrderTargetState update_order_target(
    std::uint32_t previous_ordered_target_handle,
    std::uint32_t next_ordered_target_handle,
    std::uint32_t current_visual_target_handle) noexcept;

}  // namespace a2fo::turrets
