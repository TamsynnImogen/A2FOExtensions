/*
 * Host-testable helpers for ODF-driven subsystem emissive maps.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::nebula {

constexpr std::size_t kEmissiveSystemCount = 6;

enum EmissiveSystem : std::size_t {
    warp = 0,
    impulse = 1,
    shields = 2,
    life_support = 3,
    sensors = 4,
    weapons = 5,
};

enum class SubsystemLightState {
    operational,
    disabled,
    destroyed,
};

// Classifies the native CraftSystem fields used by the renderer. A healthy
// inactive system is control-disabled; a damaged inactive system with no
// forced/timed disable remains dark until native repairs fully reactivate it.
SubsystemLightState classify_subsystem_light(
    bool operational, bool forced_disabled,
    std::int32_t maximum_hitpoints, double current_hitpoints,
    float disable_time) noexcept;

// Converts an ODF texture reference or Storm3D texture name into the common
// case-insensitive basename used to bind textureX to a live material draw.
// Directory names and the final extension are deliberately ignored so
// "fbattle", "Fbattle.tga", and "Textures/RGB/Fbattle.dds" all match.
std::string normalize_texture_key(const std::string& value);

// Produces an opaque ARGB pixel whose colour channels are the brightest
// contribution from every enabled subsystem map. Black remains transparent
// to the additive shader pass; alpha is intentionally not used as intensity.
std::uint32_t combine_emissive_pixel(
    const std::array<std::uint32_t, kEmissiveSystemCount>& pixels,
    std::uint8_t enabled_mask) noexcept;

// Expands an emissive mask across nearby texels before it is uploaded. This
// produces a soft material-space halo without replacing Fleet Operations'
// stable native D3D8 device with a post-processing wrapper. The original
// emissive colour is retained and the blurred contribution is added to it.
bool add_emissive_bloom(std::vector<std::uint32_t>& pixels,
                        std::size_t width, std::size_t height,
                        std::size_t radius,
                        std::uint32_t strength_percent) noexcept;

}  // namespace a2fo::nebula
