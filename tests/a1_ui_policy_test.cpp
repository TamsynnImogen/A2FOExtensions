#include "a1_ui_policy.hpp"

#include <cassert>
#include <string_view>

int main() {
    using a1compat::LegacyUiEvidence;

    // CommandInfo menu 1 is the Orders submenu used by the stock A2 scout
    // and search-and-destroy commands. The A1 scout.odf collision repair must
    // not publish Explore directly on PopupPalette's root page.
    static_assert(a1compat::kLegacyOrdersMenu == 1);

    // Fleet Operations treats every type-1 class target as a Build-menu
    // command. A successful compatibility-owned officer click is the one
    // root-to-Build transition that must be returned to Root immediately.
    assert(a1compat::should_restore_legacy_officer_root_menu(
        true, true, 2));
    // A genuine Build-button click takes the same menu transition but does
    // not publish a pending officer-root order, so it must remain on Build.
    assert(!a1compat::should_restore_legacy_officer_root_menu(
        false, true, 2));
    assert(!a1compat::should_restore_legacy_officer_root_menu(
        true, false, 2));
    assert(!a1compat::should_restore_legacy_officer_root_menu(
        true, true, 0));

    const auto raw_a1 = a1compat::decide_legacy_gameplay_ui(
        LegacyUiEvidence{true, true, false, false});
    assert(raw_a1.legacy_layout);
    assert(raw_a1.apply_reference_resolution);
    assert(raw_a1.reference_width == 640);
    assert(raw_a1.reference_height == 480);

    const auto converted_a2 = a1compat::decide_legacy_gameplay_ui(
        LegacyUiEvidence{true, true, true, true});
    assert(converted_a2.legacy_layout);
    assert(!converted_a2.apply_reference_resolution);

    const auto partial_resolution = a1compat::decide_legacy_gameplay_ui(
        LegacyUiEvidence{true, true, true, false});
    assert(partial_resolution.legacy_layout);
    assert(!partial_resolution.apply_reference_resolution);

    const auto native_a2 = a1compat::decide_legacy_gameplay_ui(
        LegacyUiEvidence{false, false, true, true});
    assert(!native_a2.legacy_layout);
    assert(!native_a2.apply_reference_resolution);

    const auto coincidental_panel = a1compat::decide_legacy_gameplay_ui(
        LegacyUiEvidence{true, false, false, false});
    assert(!coincidental_panel.legacy_layout);
    assert(!coincidental_panel.apply_reference_resolution);

    // ParameterDB keeps the CFG's x/y/width/height representation after native
    // scaling. DisplayComponent stores inclusive LTRB coordinates instead.
    // PopupPalette's outer rectangle is screen-relative, while each child
    // ControlButton rectangle remains local to that parent.
    const auto federation_panel =
        a1compat::legacy_area_to_native_rectangle(
            a1compat::LegacyUiArea{524, 334, 116, 146});
    assert(federation_panel.left == 524);
    assert(federation_panel.top == 334);
    assert(federation_panel.right == 639);
    assert(federation_panel.bottom == 479);

    const auto federation_button =
        a1compat::legacy_area_to_native_rectangle(
            a1compat::LegacyUiArea{9, 9, 32, 32});
    assert(federation_button.left == 9);
    assert(federation_button.top == 9);
    assert(federation_button.right == 40);
    assert(federation_button.bottom == 40);

    const auto scaled_button =
        a1compat::legacy_area_to_native_rectangle(
            a1compat::LegacyUiArea{27, 20, 96, 72});
    assert(scaled_button.left == 27);
    assert(scaled_button.top == 20);
    assert(scaled_button.right == 122);
    assert(scaled_button.bottom == 91);

    // A1 uses an explicit black backing rectangle only on races which declare
    // a non-empty area. Zero-sized masks mean the panel artwork owns the
    // interior and must remain disabled.
    assert(a1compat::usable_legacy_ui_area(
        a1compat::LegacyUiArea{10, 9, 227, 104}));
    assert(!a1compat::usable_legacy_ui_area(
        a1compat::LegacyUiArea{0, 0, 0, 0}));

    const auto resource_union = a1compat::legacy_ui_area_union(
        a1compat::LegacyUiArea{477, 0, 81, 16},
        a1compat::LegacyUiArea{559, 0, 81, 16},
        a1compat::LegacyUiArea{395, 0, 81, 16});
    assert(resource_union.x == 395);
    assert(resource_union.y == 0);
    assert(resource_union.width == 245);
    assert(resource_union.height == 16);

    const auto crew_text = a1compat::legacy_child_area_in_union(
        a1compat::LegacyUiArea{477, 0, 81, 16},
        a1compat::LegacyUiArea{16, 3, 63, 12}, resource_union);
    assert(crew_text.x == 98);
    assert(crew_text.y == 3);
    assert(crew_text.width == 63);
    assert(crew_text.height == 12);
    assert(a1compat::usable_legacy_ui_area(crew_text));

    // A1's SpeedRail background is a component-local mosaic. Federation's
    // two 189x40 pieces exactly fill its 378x40 panel, while other races may
    // give the background component a non-zero local origin.
    const a1compat::LegacyUiArea speed_background{0, 0, 378, 40};
    const auto speed_piece_0 = a1compat::legacy_child_area_in_parent(
        speed_background, a1compat::LegacyUiArea{0, 0, 189, 40});
    const auto speed_piece_1 = a1compat::legacy_child_area_in_parent(
        speed_background, a1compat::LegacyUiArea{189, 0, 189, 40});
    assert(speed_piece_0.x == 0);
    assert(speed_piece_0.width == 189);
    assert(speed_piece_1.x == 189);
    assert(speed_piece_1.x + speed_piece_1.width == 378);
    const auto offset_speed_piece = a1compat::legacy_child_area_in_parent(
        a1compat::LegacyUiArea{3, 2, 245, 37},
        a1compat::LegacyUiArea{-19, -5, 23, 50});
    assert(offset_speed_piece.x == -16);
    assert(offset_speed_piece.y == -3);
    assert(offset_speed_piece.width == 23);
    assert(offset_speed_piece.height == 50);

    // Armada I lays out five producer queue controls first, then the separator
    // sentinel and Transport in the far-right position. These values come
    // from the original Federation and Future Tense CFGs and the original
    // Armada.exe SpeedRail renderer.
    const a1compat::LegacyUiArea a1_speed_button{15, 4, 32, 32};
    const a1compat::LegacyUiArea a1_speed_separator{15, 4, 8, 33};
    const auto a1_queue_first = a1compat::legacy_speed_rail_button_slot(
        a1_speed_button, a1_speed_separator, 3,
        a1compat::kLegacySpeedQueueFirstSlot);
    const auto a1_queue_last = a1compat::legacy_speed_rail_button_slot(
        a1_speed_button, a1_speed_separator, 3,
        a1compat::kLegacySpeedQueueFirstSlot +
            a1compat::kLegacySpeedQueueSlotCount - 1);
    const auto a1_transport = a1compat::legacy_speed_rail_button_slot(
        a1_speed_button, a1_speed_separator, 3,
        a1compat::kLegacySpeedTransportSlot);
    const auto a1_separator = a1compat::legacy_speed_rail_button_slot(
        a1_speed_button, a1_speed_separator, 3,
        a1compat::kLegacySpeedSeparatorSlot);
    assert(a1_queue_first.x == 15);
    assert(a1_queue_last.x == 155);
    assert(a1_separator.x == 190);
    assert(a1_transport.x == 201);
    assert(a1_transport.x + a1_transport.width == 233);

    const a1compat::LegacyUiArea ft_speed_button{30, 4, 32, 32};
    const a1compat::LegacyUiArea ft_speed_separator{35, 4, 32, 33};
    const auto ft_queue_first = a1compat::legacy_speed_rail_button_slot(
        ft_speed_button, ft_speed_separator, 15,
        a1compat::kLegacySpeedQueueFirstSlot);
    const auto ft_transport = a1compat::legacy_speed_rail_button_slot(
        ft_speed_button, ft_speed_separator, 15,
        a1compat::kLegacySpeedTransportSlot);
    const auto ft_separator = a1compat::legacy_speed_rail_button_slot(
        ft_speed_button, ft_speed_separator, 15,
        a1compat::kLegacySpeedSeparatorSlot);
    assert(ft_queue_first.x == 30);
    assert(ft_separator.x == 270);
    assert(ft_transport.x == 312);
    assert(ft_transport.x + ft_transport.width == 344);

    // CinematicView stores its outer frame in screen coordinates and the
    // live 3D viewport relative to that frame. Federation's A1 viewport must
    // therefore land inside the Viewscreen panel, not in ShipDisplay.
    const auto federation_cinematic_display =
        a1compat::legacy_child_area_in_parent(
            a1compat::LegacyUiArea{382, 374, 142, 106},
            a1compat::LegacyUiArea{21, 13, 113, 80});
    assert(federation_cinematic_display.x == 403);
    assert(federation_cinematic_display.y == 387);
    assert(federation_cinematic_display.width == 113);
    assert(federation_cinematic_display.height == 80);
    const auto federation_cinematic_native =
        a1compat::legacy_area_to_native_rectangle(
            federation_cinematic_display);
    assert(federation_cinematic_native.left == 403);
    assert(federation_cinematic_native.top == 387);
    assert(federation_cinematic_native.right == 515);
    assert(federation_cinematic_native.bottom == 466);

    // A1's RaceIcon uses the wider team-coloured race_icon_bar as its
    // component and a smaller nested rectangle for the race insignia.
    // Restoring only the component would leave the insignia at A2's
    // off-panel display coordinates.
    const auto federation_race_icon = a1compat::legacy_race_icon_layout(
        a1compat::LegacyUiArea{34, 13, 60, 13},
        a1compat::LegacyUiArea{55, 13, 19, 13});
    assert(federation_race_icon.component.left == 34);
    assert(federation_race_icon.component.right == 93);
    assert(federation_race_icon.display.left == 55);
    assert(federation_race_icon.display.right == 73);
    assert(federation_race_icon.display.bottom == 25);

    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoPanelArea_0")} == "infoPanelArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoPanelArea_2")} == "infoPanelArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBlackArea_1")} == "infoBlackArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoLowBackgroundPanelArea")} ==
        "infoBackgroundPanelArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoSystemIcon_0")} == "infoSystem_0");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoSystemIcon_4")} == "infoSystem_4");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildSystemIcon_0")} == "infoSystem_0");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildSystemIcon_4")} == "infoSystem_4");
    assert(a1compat::legacy_ship_display_rectangle_alias(
        "infoMultiShipShield_0") == nullptr);
    assert(a1compat::legacy_indexed_ui_key(
        "infoMultiShipIcon_0", "infoMultiShipIcon_", 15) == 0);
    assert(a1compat::legacy_indexed_ui_key(
        "infoMultiShipIcon_15", "infoMultiShipIcon_", 15) == 15);
    assert(a1compat::legacy_indexed_ui_key(
        "infoMultiShipShield_7", "infoMultiShipShield_", 15) == 7);
    assert(a1compat::legacy_indexed_ui_key(
        "infoMultiShipShield_16", "infoMultiShipShield_", 15) == -1);
    assert(a1compat::legacy_indexed_ui_key(
        "infoMultiShipShield_x", "infoMultiShipShield_", 15) == -1);

    // A1's wireframe and Crew indicator are nested inside each multi-unit
    // tile. Federation slot zero is the exact stock A1 geometry.
    const auto multi_wireframe = a1compat::legacy_child_area_in_parent(
        a1compat::LegacyUiArea{30, 12, 43, 42},
        a1compat::LegacyUiArea{2, 1, 40, 40});
    assert(multi_wireframe.x == 32);
    assert(multi_wireframe.y == 13);
    assert(multi_wireframe.width == 40);
    assert(multi_wireframe.height == 40);
    const auto multi_crew_dot = a1compat::legacy_child_area_in_parent(
        a1compat::LegacyUiArea{30, 12, 43, 42},
        a1compat::LegacyUiArea{39, 36, 3, 3});
    assert(multi_crew_dot.x == 69);
    assert(multi_crew_dot.y == 48);
    assert(multi_crew_dot.width == 3);
    assert(multi_crew_dot.height == 3);
    assert(a1compat::legacy_crew_health(500.0f, 500.0f) ==
        a1compat::LegacyCrewHealth::healthy);
    assert(a1compat::legacy_crew_health(251.0f, 500.0f) ==
        a1compat::LegacyCrewHealth::healthy);
    assert(a1compat::legacy_crew_health(250.0f, 500.0f) ==
        a1compat::LegacyCrewHealth::low);
    assert(a1compat::legacy_crew_health(126.0f, 500.0f) ==
        a1compat::LegacyCrewHealth::low);
    assert(a1compat::legacy_crew_health(125.0f, 500.0f) ==
        a1compat::LegacyCrewHealth::critical);
    assert(a1compat::legacy_crew_health(0.0f, 500.0f) ==
        a1compat::LegacyCrewHealth::critical);
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoCrewNum")} ==
        "infoSingleCrewTextInformationArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoOfficers")} ==
        "infoSingleOfficerTextInformationArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildCrew")} ==
        "infoSingleCrewTextInformationArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoCrewNumIcon")} ==
        "infoSingleCrewDotArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildCrewIcon")} ==
        "infoSingleCrewDotArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildOfficers")} ==
        "infoSingleOfficerTextInformationArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildClass")} == "infoSingleClassTextArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildName")} == "infoSingleNameTextArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildWireframe")} ==
        "infoSingleWireframeIconArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoBuildShieldBar")} == "infoSingleShieldBarArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoProgressBar")} == "infoSingleConstructionBarArea");
    assert(std::string_view{
        a1compat::legacy_ship_display_rectangle_alias(
            "infoSingleConstructionArea")} ==
        "infoSingleConstructionBarArea");
    assert(a1compat::legacy_ship_display_rectangle_alias(
        "infoSingleNameTextArea") == nullptr);
    assert(std::string_view{
        a1compat::legacy_ship_display_string_alias(
            "infoMiddleBackgroundPanel")} == "infoBackgroundPanel");
    assert(a1compat::legacy_ship_display_string_alias(
        "infoBackgroundPanel") == nullptr);
    static_assert(a1compat::kLegacyControlButtonCount == 12);
    return 0;
}
