#include "../modules/A2FOCraftIdentity/directional_shield_display_config.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>

using a2fo::craft_identity::DirectionalShieldDisplayConfig;
using a2fo::craft_identity::RectangleF;

namespace {

bool check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "directional shield layout: " << message << '\n';
    return false;
}

bool same(const RectangleF& a, const RectangleF& b) {
    return std::fabs(a.x - b.x) < 0.001f &&
        std::fabs(a.y - b.y) < 0.001f &&
        std::fabs(a.width - b.width) < 0.001f &&
        std::fabs(a.height - b.height) < 0.001f;
}

bool same_array(
    const std::array<RectangleF, 4>& a,
    const std::array<RectangleF, 4>& b) {
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!same(a[i], b[i])) return false;
    }
    return true;
}

} // namespace

int main() {
    using namespace a2fo::craft_identity;

    // Input order is deliberately scrambled: port, starboard, aft, forward.
    const std::array<RectangleF, 4> odf{{
        {380, 260, 245, 16}, {380, 104, 245, 16},
        {626, 120, 20, 140}, {360, 120, 20, 140}}};
    std::array<RectangleF, 4> remapped{};
    std::array<std::size_t, 4> source_indices{};
    DirectionalShieldDisplayConfig config;
    config.facing_positions = {{3, 1, 2, 0}};
    config.position_mapping_configured = true;
    if (!check(remap_directional_shield_positions(
                   config, odf, &remapped, &source_indices),
               "valid ART mapping was rejected")) return 1;
    const std::array<std::size_t, 4> expected_sources{{3, 2, 0, 1}};
    if (!check(source_indices == expected_sources,
               "remap source provenance differs")) return 1;

    const std::array<RectangleF, 4> authored_expected{{
        {360, 120, 20, 140}, {626, 120, 20, 140},
        {380, 260, 245, 16}, {380, 104, 245, 16}}};
    // Preserve authored status through the remap by indexing the mask with
    // each reported source. First, every KA2 ODF rectangle is authored.
    const std::array<bool, 4> input_authored{{true, true, true, true}};
    std::array<bool, 4> remapped_authored{};
    for (std::size_t i = 0; i < remapped_authored.size(); ++i)
        remapped_authored[i] = input_authored[source_indices[i]];
    scale_directional_shield_default_positions(
        &remapped, remapped_authored, 511, 127);
    if (!check(same_array(remapped, authored_expected),
               "authored ODF rectangles changed during scaling")) return 1;
    for (const auto& rectangle : remapped) {
        const float left = 360.0f + rectangle.x;
        if (!check(left >= 0.0f && left + rectangle.width <= 1600.0f,
                   "KA2 shield segment exceeds display width")) return 1;
    }

    // A partial mask follows original source rectangles through remapping.
    // Here port and forward stay authored, while aft/starboard use defaults.
    remapped = authored_expected;
    const std::array<bool, 4> partial_input_authored{{true, false, false, true}};
    std::array<bool, 4> partial_remapped_authored{};
    for (std::size_t i = 0; i < partial_remapped_authored.size(); ++i)
        partial_remapped_authored[i] = partial_input_authored[source_indices[i]];
    scale_directional_shield_default_positions(
        &remapped, partial_remapped_authored, 511, 127);
    if (!check(same(remapped[0], authored_expected[0]) &&
                   same(remapped[2], authored_expected[2]) &&
                   same(remapped[1], {626 * 511.0f / 128.0f,
                                       120 * 127.0f / 128.0f,
                                       20 * 511.0f / 128.0f,
                                       140 * 127.0f / 128.0f}) &&
                   same(remapped[3], {380 * 511.0f / 128.0f,
                                       104 * 127.0f / 128.0f,
                                       245 * 511.0f / 128.0f,
                                       16 * 127.0f / 128.0f}),
               "authored status did not follow source through remapping")) return 1;

    // Reproduce the old scale from the ODF rectangles and compare its rounded
    // values to the logged failure. Coordinates use 511x127 over 128x128.
    auto old_scaled = authored_expected;
    for (auto& rectangle : old_scaled) {
        rectangle.x = std::round(rectangle.x * 511.0f / 128.0f);
        rectangle.y = std::round(rectangle.y * 127.0f / 128.0f);
        rectangle.width = std::round(rectangle.width * 511.0f / 128.0f);
        rectangle.height = std::round(rectangle.height * 127.0f / 128.0f);
    }
    const std::array<RectangleF, 4> logged_old_scaled{{
        {1437, 119, 80, 139}, {2499, 119, 80, 139},
        {1517, 258, 978, 16}, {1517, 103, 978, 16}}};
    if (!check(same_array(old_scaled, logged_old_scaled),
               "legacy blanket scale does not reproduce logged coordinates")) return 1;
    for (const auto& rectangle : old_scaled) {
        const float left = 360.0f + rectangle.x;
        if (!check(left + rectangle.width > 1600.0f,
                   "legacy blanket-scaled KA2 segment unexpectedly fits")) return 1;
    }

    const std::array<RectangleF, 4> defaults{{
        {26, 0, 76, 20}, {26, 108, 76, 20},
        {0, 26, 20, 76}, {108, 26, 20, 76}}};
    std::array<RectangleF, 4> scaled_defaults = defaults;
    scale_directional_shield_default_positions(
        &scaled_defaults, {{false, false, false, false}}, 511, 127);
    const std::array<RectangleF, 4> expected_scaled_defaults{{
        {26 * 511.0f / 128.0f, 0, 76 * 511.0f / 128.0f,
         20 * 127.0f / 128.0f},
        {26 * 511.0f / 128.0f, 108 * 127.0f / 128.0f,
         76 * 511.0f / 128.0f, 20 * 127.0f / 128.0f},
        {0, 26 * 127.0f / 128.0f, 20 * 511.0f / 128.0f,
         76 * 127.0f / 128.0f},
        {108 * 511.0f / 128.0f, 26 * 127.0f / 128.0f,
         20 * 511.0f / 128.0f, 76 * 127.0f / 128.0f}}};
    if (!check(same_array(scaled_defaults, expected_scaled_defaults),
               "built-in defaults did not scale from 128x128")) return 1;
    auto defaults_128 = defaults;
    scale_directional_shield_default_positions(
        &defaults_128, {{false, false, false, false}}, 128, 128);
    if (!check(same_array(defaults_128, defaults),
               "128x128 defaults changed")) return 1;

    auto mixed = defaults;
    const std::array<RectangleF, 4> mixed_before{{
        defaults[0], {26, 108, 76, 20}, defaults[2], {108, 26, 20, 76}}};
    scale_directional_shield_default_positions(
        &mixed, {{true, false, true, false}}, 511, 127);
    if (!check(same(mixed[0], mixed_before[0]) &&
                   same(mixed[2], mixed_before[2]) &&
                   same(mixed[1], {26 * 511.0f / 128.0f,
                                   108 * 127.0f / 128.0f,
                                   76 * 511.0f / 128.0f,
                                   20 * 127.0f / 128.0f}) &&
                   same(mixed[3], {108 * 511.0f / 128.0f,
                                   26 * 127.0f / 128.0f,
                                   20 * 511.0f / 128.0f,
                                   76 * 127.0f / 128.0f}),
               "partial authored/default scaling mismatch")) return 1;

    auto invalid_config = config;
    invalid_config.facing_positions = {{3, 3, 2, 0}};
    const auto untouched_output = remapped;
    const auto untouched_sources = source_indices;
    if (!check(!remap_directional_shield_positions(
                   invalid_config, odf, &remapped, &source_indices),
               "duplicate position mapping was accepted") ||
        !check(same_array(remapped, untouched_output) &&
                   source_indices == untouched_sources,
               "failed remap modified outputs")) return 1;

    std::cout << "directional shield layout: passed\n";
    return 0;
}
