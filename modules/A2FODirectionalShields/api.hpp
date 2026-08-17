#pragma once

#include "../../sdk/include/a2fo_module_api.h"

#include <cstdint>

struct A2FO_DirectionalShieldDamageScope {
    std::uint32_t struct_size;
    std::uint32_t active;
    void* craft;
    std::uint32_t facing;
};

enum A2FO_DirectionalShieldFacing : std::uint32_t {
    A2FO_DIRECTIONAL_SHIELD_FORWARD = 0,
    A2FO_DIRECTIONAL_SHIELD_AFT = 1,
    A2FO_DIRECTIONAL_SHIELD_PORT = 2,
    A2FO_DIRECTIONAL_SHIELD_STARBOARD = 3,
};

using A2FO_DirectionalShieldsConnectDamageBridgeFn =
    bool (A2FO_CALL*)();
using A2FO_DirectionalShieldsBeginDamageFn = bool (A2FO_CALL*)(
    void* craft,
    const void* source_damage_info,
    A2FO_DirectionalShieldDamageScope* scope);
using A2FO_DirectionalShieldsEndDamageFn = void (A2FO_CALL*)(
    A2FO_DirectionalShieldDamageScope* scope);
using A2FO_DirectionalShieldsIsEnabledFn = bool (A2FO_CALL*)(void* craft);
using A2FO_DirectionalShieldsGetValueFn = float (A2FO_CALL*)(
    void* craft, std::uint32_t facing);
