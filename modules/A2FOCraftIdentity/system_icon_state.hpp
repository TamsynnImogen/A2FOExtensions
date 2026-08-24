/*
 * Pure subsystem-state and sprite-tint helpers for the selected-panel icons.
 */

#pragma once

#include <array>
#include <cstdint>

namespace a2fo::craft_identity {

enum class SystemIconState : std::uint8_t {
    healthy,
    low,
    critical,
    disabled,
    destroyed,
};

// Returns false when the native record is not sufficiently sane to replace
// Armada's own colour decision.
bool classify_system_icon_state(
    bool operational, bool forced_disabled,
    std::int32_t maximum_hitpoints, double current_hitpoints,
    float disable_time, SystemIconState* output) noexcept;

// Replaces hue while retaining the native layer's intensity. This keeps the
// stock fill, flash, and black-background passes intact.
std::array<float, 3> tint_system_icon_colour(
    const std::array<float, 3>& native_colour,
    const std::array<float, 3>& configured_colour) noexcept;

}  // namespace a2fo::craft_identity
