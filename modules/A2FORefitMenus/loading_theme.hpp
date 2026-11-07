#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace a2fo::refit_menu {

enum class LoadingForm { shell, elbow, blade, wing, arch, cube, organic };

// Corners are top-left, top-right, bottom-right, bottom-left. Positive values
// are rounded radii; negative values are 45-degree chamfers, in design pixels.
struct LoadingTheme {
    std::string name = "Shell";
    std::string race;
    LoadingForm form = LoadingForm::shell;
    std::uint32_t panel_top = 0xe0172230u, panel = 0xe00a1018u;
    std::uint32_t panel_edge = 0xff4c5a6au, panel_hi = 0xffa6b6c8u;
    std::uint32_t text = 0xffe4eaf0u, text_dim = 0xff8e9aa8u;
    std::uint32_t title = 0xfff2b23au, accent = 0xfff2a93bu;
    std::uint32_t accent2 = 0xff6f93bau, fill = 0xfff2a93bu;
    std::uint32_t good = 0xff5fd36au;
    std::array<float, 4> corners{{-12.0f, -12.0f, -12.0f, -12.0f}};
};

// Roots are ordered from lowest to highest precedence. A malformed selected
// section falls back to the shell rather than silently using a partial theme.
LoadingTheme loading_theme(const std::vector<std::string>& roots, std::string_view race);

} // namespace a2fo::refit_menu
