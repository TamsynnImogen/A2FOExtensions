#pragma once

#include <cstdint>
#include <string_view>

namespace a1compat {

constexpr std::int32_t kLegacyUiReferenceWidth = 640;
constexpr std::int32_t kLegacyUiReferenceHeight = 480;
constexpr std::uint32_t kLegacyControlButtonCount = 12;

struct LegacyUiArea {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct NativeUiRectangle {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

struct LegacyUiEvidence {
    bool has_speed_panel_area = false;
    bool has_control_panel_area = false;
    bool has_screen_width = false;
    bool has_screen_height = false;
};

struct LegacyUiDecision {
    bool legacy_layout = false;
    bool apply_reference_resolution = false;
    std::int32_t reference_width = 0;
    std::int32_t reference_height = 0;
};

constexpr LegacyUiDecision decide_legacy_gameplay_ui(
    const LegacyUiEvidence& evidence) noexcept {
    LegacyUiDecision decision{};
    decision.legacy_layout = evidence.has_speed_panel_area &&
        evidence.has_control_panel_area;
    decision.apply_reference_resolution = decision.legacy_layout &&
        !evidence.has_screen_width && !evidence.has_screen_height;
    if (decision.apply_reference_resolution) {
        decision.reference_width = kLegacyUiReferenceWidth;
        decision.reference_height = kLegacyUiReferenceHeight;
    }
    return decision;
}

constexpr NativeUiRectangle place_legacy_control_button(
    const LegacyUiArea& control_panel,
    const LegacyUiArea& local_button) noexcept {
    const std::int32_t left = control_panel.x + local_button.x;
    const std::int32_t top = control_panel.y + local_button.y;
    return NativeUiRectangle{
        left,
        top,
        left + local_button.width - 1,
        top + local_button.height - 1};
}

// Armada II split ShipDisplay into low/middle/tall layouts. Armada I has one
// Status Report panel and one background for all selection modes, so the A2
// loader names must resolve back to those original entries for a raw A1 CFG.
constexpr const char* legacy_ship_display_rectangle_alias(
    std::string_view key) noexcept {
    if (key == "infoPanelArea_0" || key == "infoPanelArea_1" ||
        key == "infoPanelArea_2") {
        return "infoPanelArea";
    }
    if (key == "infoBlackArea_0" || key == "infoBlackArea_1" ||
        key == "infoBlackArea_2") {
        return "infoBlackArea";
    }
    if (key == "infoLowBackgroundPanelArea" ||
        key == "infoMiddleBackgroundPanelArea") {
        return "infoBackgroundPanelArea";
    }
    return nullptr;
}

constexpr const char* legacy_ship_display_string_alias(
    std::string_view key) noexcept {
    if (key == "infoLowBackgroundPanel" ||
        key == "infoMiddleBackgroundPanel") {
        return "infoBackgroundPanel";
    }
    return nullptr;
}

}  // namespace a1compat
