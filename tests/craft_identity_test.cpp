#include "directional_shield_display_config.hpp"
#include "directional_shield_fill.hpp"
#include "identity_selection.hpp"
#include "system_icon_state.hpp"

#include <cassert>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

bool close(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

}  // namespace

int main() {
    using a2fo::craft_identity::aligned_identity_index;

    std::size_t index = 99;
    assert(!aligned_identity_index(-1, 10, &index));
    assert(index == 0);
    assert(!aligned_identity_index(0, 0, &index));
    assert(!aligned_identity_index(10, 10, &index));
    assert(aligned_identity_index(0, 10, &index));
    assert(index == 0);
    assert(aligned_identity_index(7, 10, &index));
    assert(index == 7);
    assert(aligned_identity_index(9, 10, nullptr));

    using a2fo::craft_identity::RectangleF;
    using a2fo::craft_identity::centered_directional_shield_fill;
    const RectangleF horizontal_source{26.0f, 0.0f, 76.0f, 20.0f};
    const RectangleF horizontal_destination{0.0f, 0.0f, 152.0f, 20.0f};
    const auto half_horizontal = centered_directional_shield_fill(
        horizontal_source, horizontal_destination, 0.5f, true);
    assert(close(half_horizontal.source.x, 45.0f));
    assert(close(half_horizontal.source.width, 38.0f));
    assert(close(half_horizontal.destination.x, 38.0f));
    assert(close(half_horizontal.destination.width, 76.0f));
    assert(close(half_horizontal.destination.height, 20.0f));

    const RectangleF vertical_source{0.0f, 26.0f, 20.0f, 76.0f};
    const RectangleF vertical_destination{0.0f, 0.0f, 20.0f, 152.0f};
    const auto quarter_vertical = centered_directional_shield_fill(
        vertical_source, vertical_destination, 0.25f, false);
    assert(close(quarter_vertical.source.y, 54.5f));
    assert(close(quarter_vertical.source.height, 19.0f));
    assert(close(quarter_vertical.destination.y, 57.0f));
    assert(close(quarter_vertical.destination.height, 38.0f));

    const auto empty = centered_directional_shield_fill(
        horizontal_source, horizontal_destination, -1.0f, true);
    assert(close(empty.source.x, 64.0f));
    assert(close(empty.source.width, 0.0f));
    assert(close(empty.destination.x, 76.0f));
    assert(close(empty.destination.width, 0.0f));

    using a2fo::craft_identity::directional_shield_segment_at;
    const std::array<RectangleF, 4> hit_regions{{
        RectangleF{60.0f, 130.0f, 40.0f, 15.0f},
        RectangleF{60.0f, 200.0f, 40.0f, 15.0f},
        RectangleF{20.0f, 140.0f, 15.0f, 40.0f},
        RectangleF{120.0f, 140.0f, 15.0f, 40.0f},
    }};
    assert(directional_shield_segment_at(hit_regions, 70.0f, 135.0f) == 0);
    assert(directional_shield_segment_at(hit_regions, 80.0f, 208.0f) == 1);
    assert(directional_shield_segment_at(hit_regions, 25.0f, 160.0f) == 2);
    assert(directional_shield_segment_at(hit_regions, 125.0f, 160.0f) == 3);
    assert(directional_shield_segment_at(hit_regions, 110.0f, 160.0f) == -1);

    using a2fo::craft_identity::DirectionalShieldDisplayConfig;
    using a2fo::craft_identity::parse_directional_shield_display_config;
    using a2fo::craft_identity::remap_directional_shield_positions;
    using a2fo::craft_identity::valid_directional_shield_position_mapping;
    DirectionalShieldDisplayConfig display_config{};
    const auto display_report = parse_directional_shield_display_config(
        R"(
            // 1 drains; 2 retains a full coloured segment.
            int directionalShieldDisplayMode = 2;
            int directionalShieldForwardPosition = 3;
            int directionalShieldAftPosition = 1;
            /* Comments must not become settings. */
            int directionalShieldPortPosition = 2;
            int directionalShieldStarboardPosition = 0;
        )",
        &display_config);
    assert(display_report.valid_assignments == 5);
    assert(display_report.invalid_assignments == 0);
    assert(display_config.display_mode == 2);
    assert(display_config.position_mapping_configured);
    assert(valid_directional_shield_position_mapping(display_config));

    const std::array<RectangleF, 4> stock_positions{{
        RectangleF{26.0f, 0.0f, 76.0f, 20.0f},     // north
        RectangleF{26.0f, 108.0f, 76.0f, 20.0f},   // south
        RectangleF{0.0f, 26.0f, 20.0f, 76.0f},     // west
        RectangleF{108.0f, 26.0f, 20.0f, 76.0f},   // east
    }};
    std::array<RectangleF, 4> rotated_positions{};
    assert(remap_directional_shield_positions(
        display_config, stock_positions, &rotated_positions));
    assert(close(rotated_positions[0].x, 0.0f));    // forward -> west
    assert(close(rotated_positions[1].x, 108.0f));  // aft -> east
    assert(close(rotated_positions[2].y, 108.0f));  // port -> south
    assert(close(rotated_positions[3].y, 0.0f));    // starboard -> north

    const auto invalid_report = parse_directional_shield_display_config(
        R"(
            int directionalShieldDisplayMode = 3;
            int directionalShieldForwardPosition = -1;
        )",
        &display_config);
    assert(invalid_report.valid_assignments == 0);
    assert(invalid_report.invalid_assignments == 2);
    assert(display_config.display_mode == 2);

    DirectionalShieldDisplayConfig duplicate_positions{};
    duplicate_positions.position_mapping_configured = true;
    duplicate_positions.facing_positions = {{0, 0, 2, 3}};
    assert(!valid_directional_shield_position_mapping(duplicate_positions));
    std::array<RectangleF, 4> unchanged = stock_positions;
    assert(!remap_directional_shield_positions(
        duplicate_positions, stock_positions, &unchanged));
    assert(close(unchanged[0].x, stock_positions[0].x));

    using a2fo::craft_identity::classify_system_icon_state;
    using a2fo::craft_identity::SystemIconState;
    SystemIconState icon_state = SystemIconState::destroyed;
    assert(classify_system_icon_state(
        true, false, 100, 100.0, 0.0f, &icon_state));
    assert(icon_state == SystemIconState::healthy);
    assert(classify_system_icon_state(
        true, false, 100, 50.0, 0.0f, &icon_state));
    assert(icon_state == SystemIconState::low);
    assert(classify_system_icon_state(
        true, false, 100, 25.0, 0.0f, &icon_state));
    assert(icon_state == SystemIconState::critical);
    assert(classify_system_icon_state(
        false, true, 100, 80.0, 0.0f, &icon_state));
    assert(icon_state == SystemIconState::disabled);
    assert(classify_system_icon_state(
        false, false, 100, 80.0, 0.0f, &icon_state));
    assert(icon_state == SystemIconState::destroyed);
    assert(classify_system_icon_state(
        false, false, 100, 80.0, 3.0f, &icon_state));
    assert(icon_state == SystemIconState::disabled);
    assert(classify_system_icon_state(
        false, true, 100, 0.0, 3.0f, &icon_state));
    assert(icon_state == SystemIconState::destroyed);
    assert(!classify_system_icon_state(
        true, false, 0, 0.0, 0.0f, &icon_state));

    using a2fo::craft_identity::tint_system_icon_colour;
    const auto tinted = tint_system_icon_colour(
        {{0.0f, 0.5f, 0.0f}}, {{1.0f, 0.0f, 1.0f}});
    assert(close(tinted[0], 0.5f));
    assert(close(tinted[1], 0.0f));
    assert(close(tinted[2], 0.5f));
    const auto black_layer = tint_system_icon_colour(
        {{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 1.0f}});
    assert(close(black_layer[0], 0.0f));
    assert(close(black_layer[2], 0.0f));
    return 0;
}
