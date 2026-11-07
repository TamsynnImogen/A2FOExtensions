#include "system_icon_state.hpp"
#include "../../rust/gameplay_ffi.h"

namespace a2fo::craft_identity {

bool classify_system_icon_state(bool operational, bool forced_disabled,
                                std::int32_t maximum_hitpoints, double current_hitpoints,
                                float disable_time, SystemIconState *output) noexcept {
    std::uint8_t state = 0;
    if (!a2fo_rs_gameplay_icon_state(operational, forced_disabled, maximum_hitpoints,
                                     current_hitpoints, disable_time, &state) ||
        !output)
        return false;
    *output = static_cast<SystemIconState>(state);
    return true;
}

std::array<float, 3>
tint_system_icon_colour(const std::array<float, 3> &native_colour,
                        const std::array<float, 3> &configured_colour) noexcept {
    std::array<float, 3> result{};
    a2fo_rs_gameplay_tint_colour(native_colour.data(), configured_colour.data(), result.data());
    return result;
}

bool is_passive_weapon_classlabel(std::string_view classlabel) noexcept {
    return a2fo_rs_gameplay_passive_weapon_class(
               reinterpret_cast<const std::uint8_t *>(classlabel.data()), classlabel.size()) != 0;
}

WeaponIconPresentation weapon_icon_presentation(WeaponIconKind kind,
                                                WeaponTechnologyState technology,
                                                bool hide_when_unavailable) noexcept {
    return static_cast<WeaponIconPresentation>(a2fo_rs_gameplay_weapon_presentation(
        static_cast<std::uint8_t>(kind), static_cast<std::uint8_t>(technology),
        hide_when_unavailable));
}

WeaponIconColourSource weapon_icon_colour_source(bool weapon_colour_found,
                                                 bool system_colour_found) noexcept {
    return static_cast<WeaponIconColourSource>(
        a2fo_rs_gameplay_weapon_colour_source(weapon_colour_found, system_colour_found));
}

} // namespace a2fo::craft_identity
