/*
 * Fleet Operations allocates 32 ShipSystemIcon controls inside ShipDisplay.
 * A2FOCraftIdentity supplies matching sidecar controls for weapon slots
 * 33 through 128 without changing ShipDisplay's fixed object layout.
 */

#pragma once

#include <cstddef>

struct A2FO_ModuleApi;

namespace a2fo::craft_identity {

constexpr std::size_t kNativeWeaponIconLimit = 32;
constexpr std::size_t kExtendedWeaponIconLimit = 128;
constexpr std::size_t kExtendedWeaponIconCount =
    kExtendedWeaponIconLimit - kNativeWeaponIconLimit;

constexpr bool is_extended_weapon_icon_index(
    std::size_t zero_based_weapon_index) noexcept {
    return zero_based_weapon_index >= kNativeWeaponIconLimit &&
        zero_based_weapon_index < kExtendedWeaponIconLimit;
}

constexpr std::size_t extended_weapon_icon_sidecar_index(
    std::size_t zero_based_weapon_index) noexcept {
    return zero_based_weapon_index - kNativeWeaponIconLimit;
}

bool install_extended_weapon_icons(
    const A2FO_ModuleApi* api,
    void* armada_module,
    void* fleetops_module) noexcept;

}  // namespace a2fo::craft_identity
