#include "nebula_emissive.hpp"
#include "../rust/emissive_ffi.h"
#include <vector>

namespace a2fo::nebula {
SubsystemLightState classify_subsystem_light(bool operational, bool forced_disabled,
                                             std::int32_t maximum_hitpoints,
                                             double current_hitpoints,
                                             float disable_time) noexcept {
    return static_cast<SubsystemLightState>(
        a2fo_rs_emissive_subsystem(operational, forced_disabled, maximum_hitpoints,
                                   current_hitpoints, disable_time));
}
CraftMotionLightState
classify_craft_motion_light(std::uint32_t warp_effect_state, float velocity_squared,
                            float maximum_impulse_speed) noexcept {
    return static_cast<CraftMotionLightState>(a2fo_rs_emissive_motion(
        warp_effect_state, velocity_squared, maximum_impulse_speed));
}
std::array<std::uint32_t, kEmissiveSystemCount>
emissive_intensity_percent(CraftMotionLightState state) noexcept {
    std::array<std::uint32_t, kEmissiveSystemCount> output{};
    a2fo_rs_emissive_intensity(static_cast<std::uint8_t>(state), output.data());
    return output;
}
std::string normalize_texture_key(const std::string &value) {
    const auto *input = reinterpret_cast<const std::uint8_t *>(value.data());
    const auto length = a2fo_rs_emissive_key(input, value.size(), nullptr, 0);
    std::string result(length, '\0');
    a2fo_rs_emissive_key(input, value.size(),
                         reinterpret_cast<std::uint8_t *>(result.data()),
                         result.size());
    return result;
}
std::uint32_t merge_emissive_pixel(std::uint32_t combined, std::uint32_t source,
                                   std::uint32_t intensity_percent) noexcept {
    return a2fo_rs_emissive_merge(combined, source, intensity_percent);
}
std::uint32_t
combine_emissive_pixel(const std::array<std::uint32_t, kEmissiveSystemCount> &pixels,
                       std::uint8_t enabled_mask) noexcept {
    return a2fo_rs_emissive_combine(pixels.data(), enabled_mask, nullptr);
}
std::uint32_t combine_emissive_pixel(
    const std::array<std::uint32_t, kEmissiveSystemCount> &pixels,
    std::uint8_t enabled_mask,
    const std::array<std::uint32_t, kEmissiveSystemCount> &intensity_percent) noexcept {
    return a2fo_rs_emissive_combine(pixels.data(), enabled_mask,
                                    intensity_percent.data());
}
bool add_emissive_bloom(std::vector<std::uint32_t> &pixels, std::size_t width,
                        std::size_t height, std::size_t radius,
                        std::uint32_t strength_percent) noexcept {
    const auto plan = a2fo_rs_emissive_bloom_plan(pixels.size(), width, height, radius,
                                                  strength_percent);
    if (!plan.valid)
        return false;
    if (!plan.process)
        return true;
    try {
        const std::vector<std::uint32_t> original = pixels;
        std::vector<std::uint32_t> scratch(pixels.size());
        std::vector<std::uint32_t> blurred(pixels.size());
        a2fo_rs_emissive_bloom_process(pixels.data(), original.data(), scratch.data(),
                                       blurred.data(), pixels.size(), width, height,
                                       plan.radius, strength_percent);
    } catch (...) {
        return false;
    }
    return true;
}
} // namespace a2fo::nebula
