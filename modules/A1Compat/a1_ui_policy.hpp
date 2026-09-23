#pragma once

#include <cstdint>
#include <string_view>

namespace a1compat {

constexpr std::int32_t kLegacyUiReferenceWidth = 640;
constexpr std::int32_t kLegacyUiReferenceHeight = 480;
constexpr std::uint32_t kLegacyControlButtonCount = 12;
// CommandInfo's menu enum and PopupPalette's live menu field both use 1 for
// the Orders submenu. A1's scout.odf basename collision must reproduce the
// stock A2 `menu = "orders"` command instead of publishing Explore at root.
constexpr std::int32_t kLegacyOrdersMenu = 1;

enum class LegacyCrewHealth {
    healthy,
    low,
    critical,
};

// A1 uses the same three condition bands for its multi-selection Crew dot as
// its other live status indicators: green above half, yellow above one
// quarter, and red at one quarter or below while the object is operational.
constexpr LegacyCrewHealth legacy_crew_health(
    float current_crew, float maximum_crew) noexcept {
    if (maximum_crew <= 0.0f || current_crew <= 0.0f) {
        return LegacyCrewHealth::critical;
    }
    if (current_crew > maximum_crew * 0.5f) {
        return LegacyCrewHealth::healthy;
    }
    if (current_crew > maximum_crew * 0.25f) {
        return LegacyCrewHealth::low;
    }
    return LegacyCrewHealth::critical;
}

// A compatibility-owned officer build target is presented on A1's root
// palette even though Fleet Operations normally owns class targets on its
// Build submenu. After accepting that target, native ControlButton dispatch
// can therefore leave PopupPalette on Build. Only restore Root when the
// officer binding was armed and an officer order was actually accepted; a
// normal click on A1's Build command must still open the Build submenu.
constexpr bool should_restore_legacy_officer_root_menu(
    bool officer_root_order_pending,
    bool selected_producer_matches,
    std::uint32_t current_menu) noexcept {
    return officer_root_order_pending && selected_producer_matches &&
        current_menu == 2;
}

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

constexpr NativeUiRectangle legacy_area_to_native_rectangle(
    const LegacyUiArea& area) noexcept {
    return NativeUiRectangle{
        area.x,
        area.y,
        area.x + area.width - 1,
        area.y + area.height - 1};
}

constexpr bool usable_legacy_ui_area(const LegacyUiArea& area) noexcept {
    return area.width > 0 && area.height > 0;
}

constexpr LegacyUiArea legacy_ui_area_union(
    const LegacyUiArea& first, const LegacyUiArea& second,
    const LegacyUiArea& third) noexcept {
    const std::int32_t left = first.x < second.x
        ? (first.x < third.x ? first.x : third.x)
        : (second.x < third.x ? second.x : third.x);
    const std::int32_t top = first.y < second.y
        ? (first.y < third.y ? first.y : third.y)
        : (second.y < third.y ? second.y : third.y);
    const std::int32_t first_right = first.x + first.width;
    const std::int32_t second_right = second.x + second.width;
    const std::int32_t third_right = third.x + third.width;
    const std::int32_t right = first_right > second_right
        ? (first_right > third_right ? first_right : third_right)
        : (second_right > third_right ? second_right : third_right);
    const std::int32_t first_bottom = first.y + first.height;
    const std::int32_t second_bottom = second.y + second.height;
    const std::int32_t third_bottom = third.y + third.height;
    const std::int32_t bottom = first_bottom > second_bottom
        ? (first_bottom > third_bottom ? first_bottom : third_bottom)
        : (second_bottom > third_bottom ? second_bottom : third_bottom);
    return LegacyUiArea{left, top, right - left, bottom - top};
}

constexpr LegacyUiArea legacy_child_area_in_union(
    const LegacyUiArea& child_panel, const LegacyUiArea& child_area,
    const LegacyUiArea& panel_union) noexcept {
    return LegacyUiArea{
        child_panel.x - panel_union.x + child_area.x,
        child_panel.y - panel_union.y + child_area.y,
        child_area.width,
        child_area.height};
}

constexpr LegacyUiArea legacy_child_area_in_parent(
    const LegacyUiArea& parent_area,
    const LegacyUiArea& child_area) noexcept {
    return LegacyUiArea{
        parent_area.x + child_area.x,
        parent_area.y + child_area.y,
        child_area.width,
        child_area.height};
}

struct LegacyRaceIconLayout {
    NativeUiRectangle component{};
    NativeUiRectangle display{};
};

// RaceIcon has two independent rectangles. The component rectangle owns the
// team-coloured race-icon bar, while the embedded display rectangle positions
// the smaller race insignia inside it. Armada II retained both fields, but its
// same-named CFG values belong to the much larger A2 Status Report.
constexpr LegacyRaceIconLayout legacy_race_icon_layout(
    const LegacyUiArea& component_area,
    const LegacyUiArea& display_area) noexcept {
    return LegacyRaceIconLayout{
        legacy_area_to_native_rectangle(component_area),
        legacy_area_to_native_rectangle(display_area)};
}

// Armada I's SpeedRail has seven logical positions. The original renderer
// walks five producer-queue buttons first, then the separator sentinel, then
// the Transport command. Each completed position advances by its own width
// plus speedSingleButtonGap. This ordering is visible in Armada.exe's
// SpeedRail render/layout pair at 0x4942e0/0x494370.
constexpr std::uint32_t kLegacySpeedQueueFirstSlot = 0;
constexpr std::uint32_t kLegacySpeedQueueSlotCount = 5;
constexpr std::uint32_t kLegacySpeedSeparatorSlot = 5;
constexpr std::uint32_t kLegacySpeedTransportSlot = 6;

constexpr LegacyUiArea legacy_speed_rail_button_slot(
    const LegacyUiArea& single_button,
    const LegacyUiArea& separator,
    std::int32_t gap,
    std::uint32_t slot) noexcept {
    const std::int32_t single_advance = single_button.width + gap;
    if (slot < kLegacySpeedSeparatorSlot) {
        return LegacyUiArea{
            single_button.x +
                static_cast<std::int32_t>(slot) * single_advance,
            single_button.y,
            single_button.width,
            single_button.height};
    }
    const std::int32_t separator_offset =
        static_cast<std::int32_t>(kLegacySpeedSeparatorSlot) *
        single_advance;
    if (slot == kLegacySpeedSeparatorSlot) {
        return LegacyUiArea{
            separator.x + separator_offset,
            separator.y,
            separator.width,
            separator.height};
    }
    const std::int32_t offset = separator_offset +
        separator.width + gap +
        static_cast<std::int32_t>(
            slot - kLegacySpeedTransportSlot) * single_advance;
    return LegacyUiArea{
        single_button.x + offset,
        single_button.y,
        single_button.width,
        single_button.height};
}

constexpr std::int32_t legacy_indexed_ui_key(
    std::string_view key, std::string_view prefix,
    std::int32_t maximum_index) noexcept {
    if (maximum_index < 0 || key.size() <= prefix.size() ||
        key.substr(0, prefix.size()) != prefix) {
        return -1;
    }
    std::int32_t value = 0;
    for (std::size_t index = prefix.size(); index < key.size(); ++index) {
        const char digit = key[index];
        if (digit < '0' || digit > '9') return -1;
        value = value * 10 + static_cast<std::int32_t>(digit - '0');
        if (value > maximum_index) return -1;
    }
    return value;
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
    if (key == "infoCrewNum" || key == "infoBuildCrew") {
        return "infoSingleCrewTextInformationArea";
    }
    if (key == "infoCrewNumIcon" || key == "infoBuildCrewIcon") {
        return "infoSingleCrewDotArea";
    }
    if (key == "infoOfficers" || key == "infoBuildOfficers") {
        return "infoSingleOfficerTextInformationArea";
    }
    if (key == "infoBuildClass") return "infoSingleClassTextArea";
    if (key == "infoBuildName") return "infoSingleNameTextArea";
    if (key == "infoBuildWireframe") {
        return "infoSingleWireframeIconArea";
    }
    if (key == "infoBuildShieldBar") return "infoSingleShieldBarArea";
    if (key == "infoProgressBar" ||
        key == "infoSingleConstructionArea") {
        return "infoSingleConstructionBarArea";
    }
    if (key == "infoSystemIcon_0") return "infoSystem_0";
    if (key == "infoSystemIcon_1") return "infoSystem_1";
    if (key == "infoSystemIcon_2") return "infoSystem_2";
    if (key == "infoSystemIcon_3") return "infoSystem_3";
    if (key == "infoSystemIcon_4") return "infoSystem_4";
    if (key == "infoBuildSystemIcon_0") return "infoSystem_0";
    if (key == "infoBuildSystemIcon_1") return "infoSystem_1";
    if (key == "infoBuildSystemIcon_2") return "infoSystem_2";
    if (key == "infoBuildSystemIcon_3") return "infoSystem_3";
    if (key == "infoBuildSystemIcon_4") return "infoSystem_4";
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
