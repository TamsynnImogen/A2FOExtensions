#include "a1_ui_policy.hpp"

#include <cassert>
#include <string_view>

int main() {
    using a1compat::LegacyUiEvidence;

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
    // scaling. ControlButton stores inclusive LTRB coordinates instead.
    const auto federation_button = a1compat::place_legacy_control_button(
        a1compat::LegacyUiArea{524, 334, 116, 146},
        a1compat::LegacyUiArea{9, 9, 32, 32});
    assert(federation_button.left == 533);
    assert(federation_button.top == 343);
    assert(federation_button.right == 564);
    assert(federation_button.bottom == 374);

    const auto scaled_button = a1compat::place_legacy_control_button(
        a1compat::LegacyUiArea{1572, 752, 348, 328},
        a1compat::LegacyUiArea{27, 20, 96, 72});
    assert(scaled_button.left == 1599);
    assert(scaled_button.top == 772);
    assert(scaled_button.right == 1694);
    assert(scaled_button.bottom == 843);

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
