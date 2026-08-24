#include "team_color_policy.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

void expect_color(const a1compat::TeamColorPalettePolicy& palette,
                  std::size_t index, float red, float green, float blue) {
    assert(palette.present[index]);
    assert(near(palette.colors[index].red, red));
    assert(near(palette.colors[index].green, green));
    assert(near(palette.colors[index].blue, blue));
}

}  // namespace

int main() {
    a1compat::TeamColorPalettePolicy palette;
    std::string parent;
    for (int index = 1; index <= 16; ++index) {
        char line[96]{};
        std::snprintf(line, sizeof(line),
                      "mpcolor%02d = 0.%02d 0.25 0.75\n", index, index);
        parent += line;
    }
    const a1compat::TeamColorMergeResult parent_result =
        a1compat::merge_team_color_odf(parent, palette);
    assert(parent_result.indexed_values == 16);
    assert(parent_result.legacy_aliases == 0);

    constexpr std::string_view legacy = R"ODF(
        // Original Armada 1 names; comments and case are accepted.
        WHITE = 0.9 0.9 0.9
        red = 1.0 0.0 0.0
        blue = 0.0 0.4 1.0
        green = 0.0 1.0 0.0
        yellow = 0.9 0.9 0.0
        purple = 0.686 0.0 1.0
        cyan = 0.0 1.0 0.784
        brown = 0.6 0.4 0.0
        orange = 1.0 0.588 0.0
        pink = 1.0 0.5 0.5
        magenta = 1.0 0.0 1.0
        gray = 0.5 0.5 0.5
        black = 0.0 0.0 0.0
    )ODF";
    const a1compat::TeamColorMergeResult legacy_result =
        a1compat::merge_team_color_odf(legacy, palette);
    assert(legacy_result.indexed_values == 0);
    assert(legacy_result.legacy_aliases == 13);
    expect_color(palette, 0, 0.9f, 0.9f, 0.9f);
    expect_color(palette, 1, 1.0f, 0.0f, 0.0f);
    expect_color(palette, 2, 0.0f, 0.4f, 1.0f);
    expect_color(palette, 12, 0.0f, 0.0f, 0.0f);
    // A1 has only thirteen named choices; inherited FO slots remain intact.
    expect_color(palette, 13, 0.14f, 0.25f, 0.75f);
    expect_color(palette, 15, 0.16f, 0.25f, 0.75f);

    // Explicit Fleet Operations spelling in one layer wins over its legacy
    // alias, while another legacy alias still translates normally.
    constexpr std::string_view mixed = R"ODF(
        red = 0.2 0.2 0.2
        mpColor02 = 0.7 0.1 0.1
        grey = 0.3f 0.4f 0.5f; /* alternate spelling */
        blue = nope
    )ODF";
    const a1compat::TeamColorMergeResult mixed_result =
        a1compat::merge_team_color_odf(mixed, palette);
    assert(mixed_result.indexed_values == 1);
    assert(mixed_result.legacy_aliases == 1);
    expect_color(palette, 1, 0.7f, 0.1f, 0.1f);
    expect_color(palette, 11, 0.3f, 0.4f, 0.5f);
    expect_color(palette, 2, 0.0f, 0.4f, 1.0f);

    return 0;
}
