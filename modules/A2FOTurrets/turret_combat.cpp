/*
 * Pure inherited-order policy for linked hull turrets.
 */

#include "turret_combat.hpp"

#include "../../rust/combat_config_ffi.h"

namespace a2fo::turrets {

std::uint32_t attack_order_target_handle(
    std::uint32_t command_id,
    std::uint32_t target_form,
    std::uint32_t target_handle,
    bool target_exists) noexcept {
    return a2fo_rs_combat_attack_target(
        command_id, target_form, target_handle, target_exists ? 1 : 0);
}

OrderTargetState update_order_target(
    std::uint32_t previous_ordered_target_handle,
    std::uint32_t next_ordered_target_handle,
    std::uint32_t current_visual_target_handle) noexcept {
    A2foRsOrderTargetState output{};
    a2fo_rs_combat_update_target(previous_ordered_target_handle,
                                 next_ordered_target_handle,
                                 current_visual_target_handle, &output);
    return {output.ordered_target_handle, output.visual_target_handle};
}

}  // namespace a2fo::turrets
