/*
 * Pure inherited-order policy for linked hull turrets.
 */

#include "turret_combat.hpp"

namespace a2fo::turrets {

std::uint32_t attack_order_target_handle(
    std::uint32_t command_id,
    std::uint32_t target_form,
    std::uint32_t target_handle,
    bool target_exists) noexcept {
    if (command_id != kAttackCommand ||
        target_form != kObjectCommandTargetForm || target_handle == 0 ||
        !target_exists) {
        return 0;
    }
    return target_handle;
}

OrderTargetState update_order_target(
    std::uint32_t previous_ordered_target_handle,
    std::uint32_t next_ordered_target_handle,
    std::uint32_t current_visual_target_handle) noexcept {
    OrderTargetState result{};
    result.ordered_target_handle = next_ordered_target_handle;
    result.visual_target_handle = current_visual_target_handle;
    if (next_ordered_target_handle != 0) {
        result.visual_target_handle = next_ordered_target_handle;
    } else if (previous_ordered_target_handle != 0 &&
               current_visual_target_handle ==
                   previous_ordered_target_handle) {
        result.visual_target_handle = 0;
    }
    return result;
}

}  // namespace a2fo::turrets
