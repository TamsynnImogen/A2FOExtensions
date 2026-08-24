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

// The three motion-dependent profiles deliberately remain a small, discrete
// set so the renderer can cache their combined textures instead of rebuilding
// an emissive map on every draw.
enum class CraftMotionLightState : std::uint8_t {
    idle,
    gravity_well,
    impulse,
    warp,
};

constexpr std::uint32_t kBaseEmissiveIntensityPercent = 100;
constexpr std::uint32_t kGravityWellWarpIntensityPercent = 125;
constexpr std::uint32_t kImpulseTravelIntensityPercent = 150;
constexpr std::uint32_t kWarpTravelIntensityPercent = 200;
constexpr std::uint32_t kUnknownWarpEffectState = 0xffffffffu;

// Classifies the native CraftSystem fields used by the renderer. A healthy
// inactive system is control-disabled; a damaged inactive system with no
// forced/timed disable remains dark until native repairs fully reactivate it.
SubsystemLightState classify_subsystem_light(
    bool operational, bool forced_disabled,
    std::int32_t maximum_hitpoints, double current_hitpoints,
    float disable_time) noexcept;

// Classifies Fleet Operations' native Trek-physics warp effect state and live
// linear velocity for lighting. Native states are 0 normal/gravity-well,
// 1 warp-in, 2 at warp, and 3 warp-out. When that state is unavailable, the
// class impulse-speed limit provides a conservative fallback.
CraftMotionLightState classify_craft_motion_light(
    std::uint32_t warp_effect_state, float velocity_squared,
    float maximum_impulse_speed) noexcept;

std::array<std::uint32_t, kEmissiveSystemCount>
emissive_intensity_percent(CraftMotionLightState state) noexcept;

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

// As above, with a percentage multiplier per subsystem. Each RGB channel is
// rounded and saturated before the brightest enabled contribution is chosen.
std::uint32_t combine_emissive_pixel(
    const std::array<std::uint32_t, kEmissiveSystemCount>& pixels,
    std::uint8_t enabled_mask,
    const std::array<std::uint32_t, kEmissiveSystemCount>&
        intensity_percent) noexcept;

// Merges one source into an already-combined opaque ARGB pixel. The operation
// is associative per colour channel, so render caches can process dense and
// sparse source maps without changing the final image.
std::uint32_t merge_emissive_pixel(
    std::uint32_t combined, std::uint32_t source,
    std::uint32_t intensity_percent) noexcept;

// Expands an emissive mask across nearby texels before it is uploaded. This
// produces a soft material-space halo without replacing Fleet Operations'
// stable native D3D8 device with a post-processing wrapper. The original
// emissive colour is retained and the blurred contribution is added to it.
bool add_emissive_bloom(std::vector<std::uint32_t>& pixels,
                        std::size_t width, std::size_t height,
                        std::size_t radius,
                        std::uint32_t strength_percent) noexcept;

}  // namespace a2fo::nebula
