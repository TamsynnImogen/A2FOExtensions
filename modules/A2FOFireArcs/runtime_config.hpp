/*
 * Host-testable parser for A2FOFireArcs' RTS_CFG.h feature switch.
 */

#pragma once

#include <string_view>

namespace a2fo::fire_arcs {

enum class FireArcSettingStatus {
    absent,
    valid,
    invalid,
};

// Reads the last `firearc = 0/1;` assignment in one C-style configuration
// file. Comments and an optional declaration prefix such as `int` are
// accepted. `enabled` changes only when the returned status is valid.
FireArcSettingStatus parse_firearc_setting(
    std::string_view source, bool* enabled);

}  // namespace a2fo::fire_arcs
