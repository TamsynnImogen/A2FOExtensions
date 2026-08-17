/*
 * Host-testable ART_CFG.h controls for the directional-shield ring.
 */

#pragma once

#include "directional_shield_fill.hpp"

#include <array>
#include <string_view>

namespace a2fo::craft_identity {

// ART_CFG.h compass codes used by directionalShield*Position.
enum class DirectionalShieldPosition : int {
    north = 0,
    east = 1,
    south = 2,
    west = 3,
};

struct DirectionalShieldDisplayConfig {
    // 1: proportional drain, 2: full segment whose colour shows health.
    int display_mode = 1;

    // Indexed forward, aft, port, starboard. The stock/default arrangement
    // is forward=north, aft=south, port=west, starboard=east.
    std::array<int, 4> facing_positions{{0, 2, 3, 1}};
    bool position_mapping_configured = false;
};

struct DirectionalShieldDisplayParseReport {
    unsigned valid_assignments = 0;
    unsigned invalid_assignments = 0;
    bool display_mode_found = false;
    std::array<bool, 4> position_found{};
};

// Applies recognized assignments from one ART_CFG.h file to config. Calling
// this repeatedly in extension-root order gives child roots normal override
// behavior. Comments and optional C-style declaration prefixes are accepted.
DirectionalShieldDisplayParseReport parse_directional_shield_display_config(
    std::string_view source, DirectionalShieldDisplayConfig* config);

bool valid_directional_shield_position_mapping(
    const DirectionalShieldDisplayConfig& config) noexcept;

// Treats input as four facing-indexed rectangles, identifies their physical
// north/east/south/west slots, then assigns each facing the ART-configured
// slot. Returns false without changing output when the layout is ambiguous or
// the configured mapping is invalid.
bool remap_directional_shield_positions(
    const DirectionalShieldDisplayConfig& config,
    const std::array<RectangleF, 4>& input,
    std::array<RectangleF, 4>* output) noexcept;

}  // namespace a2fo::craft_identity
