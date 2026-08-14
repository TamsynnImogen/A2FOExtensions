#include "nebula_emissive.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace {

std::uint32_t opaque_average(std::uint64_t red, std::uint64_t green,
                             std::uint64_t blue,
                             std::size_t count) noexcept {
    if (count == 0) return 0xff000000u;
    return 0xff000000u |
        (static_cast<std::uint32_t>(red / count) << 16) |
        (static_cast<std::uint32_t>(green / count) << 8) |
        static_cast<std::uint32_t>(blue / count);
}

void add_pixel(std::uint32_t pixel, std::uint64_t& red,
               std::uint64_t& green, std::uint64_t& blue) noexcept {
    red += (pixel >> 16) & 0xffu;
    green += (pixel >> 8) & 0xffu;
    blue += pixel & 0xffu;
}

void remove_pixel(std::uint32_t pixel, std::uint64_t& red,
                  std::uint64_t& green, std::uint64_t& blue) noexcept {
    red -= (pixel >> 16) & 0xffu;
    green -= (pixel >> 8) & 0xffu;
    blue -= pixel & 0xffu;
}

void box_blur_horizontal(const std::vector<std::uint32_t>& source,
                         std::vector<std::uint32_t>& destination,
                         std::size_t width, std::size_t height,
                         std::size_t radius) noexcept {
    for (std::size_t y = 0; y < height; ++y) {
        const std::size_t row = y * width;
        const std::size_t initial_right = std::min(radius, width - 1);
        std::uint64_t red = 0;
        std::uint64_t green = 0;
        std::uint64_t blue = 0;
        std::size_t count = initial_right + 1;
        for (std::size_t x = 0; x <= initial_right; ++x) {
            add_pixel(source[row + x], red, green, blue);
        }
        for (std::size_t x = 0; x < width; ++x) {
            destination[row + x] = opaque_average(red, green, blue, count);
            const std::size_t next = x + 1;
            if (next >= width) break;
            if (next > radius) {
                remove_pixel(source[row + next - radius - 1],
                             red, green, blue);
                --count;
            }
            if (next + radius < width) {
                add_pixel(source[row + next + radius], red, green, blue);
                ++count;
            }
        }
    }
}

void box_blur_vertical(const std::vector<std::uint32_t>& source,
                       std::vector<std::uint32_t>& destination,
                       std::size_t width, std::size_t height,
                       std::size_t radius) noexcept {
    for (std::size_t x = 0; x < width; ++x) {
        const std::size_t initial_bottom = std::min(radius, height - 1);
        std::uint64_t red = 0;
        std::uint64_t green = 0;
        std::uint64_t blue = 0;
        std::size_t count = initial_bottom + 1;
        for (std::size_t y = 0; y <= initial_bottom; ++y) {
            add_pixel(source[y * width + x], red, green, blue);
        }
        for (std::size_t y = 0; y < height; ++y) {
            destination[y * width + x] =
                opaque_average(red, green, blue, count);
            const std::size_t next = y + 1;
            if (next >= height) break;
            if (next > radius) {
                remove_pixel(source[(next - radius - 1) * width + x],
                             red, green, blue);
                --count;
            }
            if (next + radius < height) {
                add_pixel(source[(next + radius) * width + x],
                          red, green, blue);
                ++count;
            }
        }
    }
}

}  // namespace

namespace a2fo::nebula {

SubsystemLightState classify_subsystem_light(
    bool operational, bool forced_disabled,
    std::int32_t maximum_hitpoints, double current_hitpoints,
    float disable_time) noexcept {
    if (operational) return SubsystemLightState::operational;
    if (!std::isfinite(current_hitpoints) ||
        !std::isfinite(disable_time) || maximum_hitpoints <= 0) {
        return SubsystemLightState::operational;
    }
    const double maximum = static_cast<double>(maximum_hitpoints);
    if ((!forced_disabled && disable_time <= 0.0f &&
         current_hitpoints + 0.0001 < maximum) ||
        current_hitpoints <= 0.0) {
        return SubsystemLightState::destroyed;
    }
    return SubsystemLightState::disabled;
}

CraftMotionLightState classify_craft_motion_light(
    std::uint32_t warp_effect_state, float velocity_squared,
    float maximum_impulse_speed) noexcept {
    constexpr float kTravelSpeedSquared = 0.01f;
    const bool moving = std::isfinite(velocity_squared) &&
        velocity_squared > kTravelSpeedSquared;

    if (warp_effect_state <= 3u) {
        if (warp_effect_state == 2u) {
            return CraftMotionLightState::warp;
        }
        if (warp_effect_state == 0u && moving) {
            return CraftMotionLightState::impulse;
        }
        // Warp-in and warp-out remain on the lower nacelle profile. Only the
        // native steady at-warp state receives the full warp boost.
        return CraftMotionLightState::gravity_well;
    }

    if (!moving) return CraftMotionLightState::idle;
    if (std::isfinite(maximum_impulse_speed) &&
        maximum_impulse_speed > 0.0f) {
        const float warp_threshold = maximum_impulse_speed * 1.05f;
        if (std::isfinite(warp_threshold) &&
            velocity_squared > warp_threshold * warp_threshold) {
            return CraftMotionLightState::warp;
        }
    }
    return CraftMotionLightState::impulse;
}

std::array<std::uint32_t, kEmissiveSystemCount>
emissive_intensity_percent(CraftMotionLightState state) noexcept {
    std::array<std::uint32_t, kEmissiveSystemCount> result{};
    result.fill(kBaseEmissiveIntensityPercent);
    switch (state) {
        case CraftMotionLightState::gravity_well:
            result[warp] = kGravityWellWarpIntensityPercent;
            break;
        case CraftMotionLightState::impulse:
            result[warp] = kGravityWellWarpIntensityPercent;
            result[impulse] = kImpulseTravelIntensityPercent;
            break;
        case CraftMotionLightState::warp:
            result[warp] = kWarpTravelIntensityPercent;
            break;
        case CraftMotionLightState::idle:
            break;
    }
    return result;
}

std::string normalize_texture_key(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    const std::size_t slash = value.find_last_of("\\/", last - 1);
    if (slash != std::string::npos && slash >= first) first = slash + 1;
    const std::size_t dot = value.find_last_of('.', last - 1);
    if (dot != std::string::npos && dot > first) last = dot;

    std::string key = value.substr(first, last - first);
    std::transform(key.begin(), key.end(), key.begin(), [](char character) {
        return static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    });
    return key;
}

std::uint32_t combine_emissive_pixel(
    const std::array<std::uint32_t, kEmissiveSystemCount>& pixels,
    std::uint8_t enabled_mask) noexcept {
    std::array<std::uint32_t, kEmissiveSystemCount> intensity_percent{};
    intensity_percent.fill(kBaseEmissiveIntensityPercent);
    return combine_emissive_pixel(
        pixels, enabled_mask, intensity_percent);
}

std::uint32_t combine_emissive_pixel(
    const std::array<std::uint32_t, kEmissiveSystemCount>& pixels,
    std::uint8_t enabled_mask,
    const std::array<std::uint32_t, kEmissiveSystemCount>&
        intensity_percent) noexcept {
    std::uint32_t red = 0;
    std::uint32_t green = 0;
    std::uint32_t blue = 0;
    const auto scaled_channel = [](std::uint32_t channel,
                                   std::uint32_t percent) noexcept {
        const std::uint64_t scaled =
            static_cast<std::uint64_t>(channel) * percent + 50u;
        return static_cast<std::uint32_t>(
            std::min<std::uint64_t>(255u, scaled / 100u));
    };
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        if ((enabled_mask & (1u << index)) == 0) continue;
        const std::uint32_t pixel = pixels[index];
        red = std::max(red, scaled_channel(
            (pixel >> 16) & 0xffu, intensity_percent[index]));
        green = std::max(green, scaled_channel(
            (pixel >> 8) & 0xffu, intensity_percent[index]));
        blue = std::max(blue, scaled_channel(
            pixel & 0xffu, intensity_percent[index]));
    }
    return 0xff000000u | (red << 16) | (green << 8) | blue;
}

bool add_emissive_bloom(std::vector<std::uint32_t>& pixels,
                        std::size_t width, std::size_t height,
                        std::size_t radius,
                        std::uint32_t strength_percent) noexcept {
    if (width == 0 || height == 0 || width > pixels.size() / height ||
        width * height != pixels.size()) {
        return false;
    }
    if (radius == 0 || strength_percent == 0) return true;
    radius = std::min(radius, std::max(width, height) - 1);

    try {
        const std::vector<std::uint32_t> original = pixels;
        std::vector<std::uint32_t> scratch(pixels.size());
        std::vector<std::uint32_t> blurred(pixels.size());

        // Two separable box passes form a soft triangular filter while
        // remaining cheap enough to build lazily during the first ship draw.
        box_blur_horizontal(original, scratch, width, height, radius);
        box_blur_vertical(scratch, blurred, width, height, radius);
        box_blur_horizontal(blurred, scratch, width, height, radius);
        box_blur_vertical(scratch, blurred, width, height, radius);

        for (std::size_t index = 0; index < pixels.size(); ++index) {
            const std::uint32_t source = original[index];
            const std::uint32_t glow = blurred[index];
            const auto add_channel = [strength_percent](
                                         std::uint32_t base,
                                         std::uint32_t bloom) noexcept {
                const std::uint64_t contribution =
                    static_cast<std::uint64_t>(bloom) * strength_percent /
                    100u;
                return static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(255u, base + contribution));
            };
            const std::uint32_t red = add_channel(
                (source >> 16) & 0xffu, (glow >> 16) & 0xffu);
            const std::uint32_t green = add_channel(
                (source >> 8) & 0xffu, (glow >> 8) & 0xffu);
            const std::uint32_t blue = add_channel(
                source & 0xffu, glow & 0xffu);
            pixels[index] =
                0xff000000u | (red << 16) | (green << 8) | blue;
        }
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace a2fo::nebula
