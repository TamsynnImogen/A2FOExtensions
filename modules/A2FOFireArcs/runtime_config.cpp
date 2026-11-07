#include "runtime_config.hpp"

#include "../../rust/combat_config_ffi.h"

#include <cstdint>

namespace a2fo::fire_arcs {

FireArcSettingStatus parse_firearc_setting(
    std::string_view source, bool* enabled) {
    if (!enabled) return FireArcSettingStatus::invalid;
    std::uint8_t candidate = *enabled ? 1 : 0;
    const auto status = a2fo_rs_combat_parse_firearc(
        reinterpret_cast<const std::uint8_t*>(source.data()),
        source.size(), &candidate);
    if (status == 1) *enabled = candidate != 0;
    return static_cast<FireArcSettingStatus>(status);
}

}  // namespace a2fo::fire_arcs
