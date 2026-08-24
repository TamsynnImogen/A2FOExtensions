#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace a1compat {

constexpr std::size_t kPlayerTeamColorCount = 16;

struct TeamColorRgb {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
};

struct TeamColorPalettePolicy {
    std::array<TeamColorRgb, kPlayerTeamColorCount> colors{};
    std::array<bool, kPlayerTeamColorCount> present{};
};

struct TeamColorMergeResult {
    std::uint32_t indexed_values = 0;
    std::uint32_t legacy_aliases = 0;
};

// Merge one teamcolor.odf layer into an already accumulated palette. Fleet
// Operations' mpcolorXX spelling wins over the corresponding Armada 1 named
// colour when both occur in the same file. Calling this from lowest to highest
// extension-root precedence reproduces normal child-over-parent policy while
// allowing a legacy A1 child to translate its named palette at runtime.
TeamColorMergeResult merge_team_color_odf(
    std::string_view contents, TeamColorPalettePolicy& palette);

}  // namespace a1compat
