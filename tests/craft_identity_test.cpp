#include "directional_shield_display_config.hpp"
#include "directional_shield_fill.hpp"
#include "extended_weapon_icons.hpp"
#include "identity_selection.hpp"
#include "selected_panel_anchor.hpp"
#include "system_icon_state.hpp"

#include <cassert>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

bool close(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

void test_selected_panel_anchor() {
    using namespace a2fo::craft_identity;
    int captain_component = 0;
    int name_component = 0;
    int class_component = 0;
    const RawRectangle captain_cfg{675, 91, 240, 20};
    const RawRectangle photon_cfg{700, 40, 103, 10};
    const RawRectangle quantum_cfg{700, 68, 103, 10};
    const RawRectangle builder_name_cfg{29, 21, 240, 20};
    const RawRectangle builder_class_cfg{29, 36, 240, 20};
    // Native rectangles have inclusive right/bottom bounds and retain the
    // interface's vertical text adjustment (10 pixels in this fixture).
    const SelectedPanelAnchorCandidate captain{
        &captain_component, {675, 81, 914, 100}, &captain_cfg};
    const std::array<SelectedPanelAnchorCandidate, 2> builder{{
        {&name_component, {29, 11, 268, 30}, &builder_name_cfg},
        {&class_component, {29, 26, 268, 45}, &builder_class_cfg},
    }};
    const auto check_ammunition = [&](const SelectedPanelTextAnchor& anchor) {
        const auto photon = translated_rectangle(
            anchor.captain_rectangle, captain_cfg, photon_cfg);
        assert(photon.left == 700 && photon.top == 30);
        assert(photon.right == 802 && photon.bottom == 39);
        const auto quantum = translated_rectangle(
            anchor.captain_rectangle, captain_cfg, quantum_cfg);
        assert(quantum.left == 700 && quantum.top == 58);
        assert(quantum.right == 802 && quantum.bottom == 67);
    };
    const auto ship = resolve_selected_panel_anchor(captain, &captain_cfg, {});
    const auto yard = resolve_selected_panel_anchor(captain, &captain_cfg, builder);
    assert(ship.component == &captain_component);
    assert(yard.component == ship.component);
    check_ammunition(ship);
    check_ammunition(yard);

    // Repair-only yards still use the producer panel. A hidden build-name
    // row or a different live build layout must not move/hide ammunition.
    auto custom_builder = builder;
    custom_builder[0].rectangle = {29, 2000, 29, 2000};
    custom_builder[1].rectangle = {900, 900, 999, 919};
    custom_builder[1].configured_rectangle = nullptr;
    const auto custom = resolve_selected_panel_anchor(
        captain, &captain_cfg, custom_builder);
    assert(custom.component == &captain_component);
    check_ammunition(custom);

    // Custom interfaces without a usable captain component can rebase a
    // build-name/class anchor, but only when its source rectangle is known.
    const auto fallback = resolve_selected_panel_anchor({}, &captain_cfg, builder);
    assert(fallback.component == &name_component);
    check_ammunition(fallback);
    auto missing_name_cfg = builder;
    missing_name_cfg[0].configured_rectangle = nullptr;
    const auto class_fallback = resolve_selected_panel_anchor(
        {}, &captain_cfg, missing_name_cfg);
    assert(class_fallback.component == &class_component);
    check_ammunition(class_fallback);
    assert(!resolve_selected_panel_anchor({}, &captain_cfg, custom_builder).component);
    const auto automatic = resolve_selected_panel_anchor({}, nullptr, builder);
    assert(automatic.component == &name_component);
    assert(automatic.captain_rectangle.left == 29);
    assert(automatic.captain_rectangle.top == 11);
    auto hidden_captain = captain;
    hidden_captain.rectangle = {};
    assert(resolve_selected_panel_anchor(hidden_captain, &captain_cfg, builder)
               .component == &name_component);
}

}  // namespace

int main() {
    test_selected_panel_anchor();
    using a2fo::craft_identity::extended_weapon_icon_sidecar_index;
    using a2fo::craft_identity::is_extended_weapon_icon_index;
    using a2fo::craft_identity::kExtendedWeaponIconCount;
    using a2fo::craft_identity::kExtendedWeaponIconLimit;
    using a2fo::craft_identity::kNativeWeaponIconLimit;
    static_assert(kNativeWeaponIconLimit == 32);
    static_assert(kExtendedWeaponIconLimit == 128);
    static_assert(kExtendedWeaponIconCount == 96);
    assert(!is_extended_weapon_icon_index(31));
    assert(is_extended_weapon_icon_index(32));
    assert(is_extended_weapon_icon_index(127));
    assert(!is_extended_weapon_icon_index(128));
    assert(extended_weapon_icon_sidecar_index(32) == 0);
    assert(extended_weapon_icon_sidecar_index(127) == 95);

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

    using a2fo::craft_identity::DirectionalShieldValueDisplayMode;
    using a2fo::craft_identity::format_directional_shield_value;
    char shield_value[32]{};
    assert(!format_directional_shield_value(
        DirectionalShieldValueDisplayMode::none, 75.0f, 100.0f,
        shield_value, sizeof(shield_value)));
    assert(shield_value[0] == '\0');
    assert(format_directional_shield_value(
        DirectionalShieldValueDisplayMode::percent, 149.0f, 200.0f,
        shield_value, sizeof(shield_value)));
    assert(std::strcmp(shield_value, "75") == 0);
    assert(std::strchr(shield_value, '%') == nullptr);
    assert(format_directional_shield_value(
        DirectionalShieldValueDisplayMode::amount, 149.0f, 200.0f,
        shield_value, sizeof(shield_value)));
    assert(std::strcmp(shield_value, "149/200") == 0);
    assert(!format_directional_shield_value(
        DirectionalShieldValueDisplayMode::percent, 10.0f, 0.0f,
        shield_value, sizeof(shield_value)));
    char too_small[3]{};
    assert(!format_directional_shield_value(
        DirectionalShieldValueDisplayMode::amount, 149.0f, 200.0f,
        too_small, sizeof(too_small)));
    assert(too_small[0] == '\0');

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

    using a2fo::craft_identity::is_passive_weapon_classlabel;
    assert(is_passive_weapon_classlabel("UtilityWeapon"));
    assert(is_passive_weapon_classlabel(" utilityweapon\t"));
    assert(!is_passive_weapon_classlabel("SpecialWeapon"));

    using a2fo::craft_identity::WeaponIconKind;
    using a2fo::craft_identity::WeaponIconColourSource;
    using a2fo::craft_identity::WeaponIconPresentation;
    using a2fo::craft_identity::WeaponTechnologyState;
    using a2fo::craft_identity::weapon_icon_colour_source;
    using a2fo::craft_identity::weapon_icon_presentation;
    assert(weapon_icon_colour_source(true, true) ==
        WeaponIconColourSource::weapon);
    assert(weapon_icon_colour_source(true, false) ==
        WeaponIconColourSource::weapon);
    assert(weapon_icon_colour_source(false, true) ==
        WeaponIconColourSource::system_fallback);
    assert(weapon_icon_colour_source(false, false) ==
        WeaponIconColourSource::native);
    assert(weapon_icon_presentation(
        WeaponIconKind::passive,
        WeaponTechnologyState::unavailable, true) ==
        WeaponIconPresentation::passive_neutral);
    assert(weapon_icon_presentation(
        WeaponIconKind::normal,
        WeaponTechnologyState::unavailable, true) ==
        WeaponIconPresentation::hidden);
    assert(weapon_icon_presentation(
        WeaponIconKind::normal,
        WeaponTechnologyState::unavailable, false) ==
        WeaponIconPresentation::disabled);
    assert(weapon_icon_presentation(
        WeaponIconKind::special,
        WeaponTechnologyState::unavailable, true) ==
        WeaponIconPresentation::hidden);
    assert(weapon_icon_presentation(
        WeaponIconKind::special,
        WeaponTechnologyState::unavailable, false) ==
        WeaponIconPresentation::disabled);
    assert(weapon_icon_presentation(
        WeaponIconKind::normal,
        WeaponTechnologyState::available, true) ==
        WeaponIconPresentation::live_status);
    assert(weapon_icon_presentation(
        WeaponIconKind::special,
        WeaponTechnologyState::unknown, true) ==
        WeaponIconPresentation::live_status);
    return 0;
}
