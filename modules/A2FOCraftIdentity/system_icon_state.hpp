/*
 * Pure subsystem-state and sprite-tint helpers for the selected-panel icons.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace a2fo::craft_identity {

enum class SystemIconState : std::uint8_t {
    healthy,
    low,
    critical,
    disabled,
    destroyed,
};

enum class WeaponIconKind : std::uint8_t {
    normal,
    special,
    passive,
};

enum class WeaponTechnologyState : std::uint8_t {
    unknown,
    available,
    unavailable,
};

enum class WeaponIconPresentation : std::uint8_t {
    live_status,
    disabled,
    hidden,
    passive_neutral,
};

enum class WeaponIconColourSource : std::uint8_t {
    native,
    system_fallback,
    weapon,
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

// UtilityWeapon is Fleet Operations' passive WeaponClass. Matching is ASCII
// case-insensitive because classLabel values are case-insensitive ODF tokens.
bool is_passive_weapon_classlabel(std::string_view classlabel) noexcept;

WeaponIconPresentation weapon_icon_presentation(
    WeaponIconKind kind, WeaponTechnologyState technology,
    bool hide_when_unavailable) noexcept;

// A weapon-specific state colour wins when present. The matching system-icon
// colour remains a backward-compatible fallback for existing GUI configs.
WeaponIconColourSource weapon_icon_colour_source(
    bool weapon_colour_found, bool system_colour_found) noexcept;

}  // namespace a2fo::craft_identity
