#pragma once

#include <cstdint>
#include <cstring>
#include "../../rust/stage9_a1_crew_ffi.h"
#include <string_view>
#include "../../rust/stage9_a1_ui_ffi.h"

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
inline LegacyCrewHealth legacy_crew_health(float current_crew,float maximum_crew) noexcept {
    std::uint32_t current,maximum;
    std::memcpy(&current,&current_crew,4);std::memcpy(&maximum,&maximum_crew,4);
    return static_cast<LegacyCrewHealth>(a2fo_rs_stage9_a1_crew_health(current,maximum));
}

// A compatibility-owned officer build target is presented on A1's root
// palette even though Fleet Operations normally owns class targets on its
// Build submenu. After accepting that target, native ControlButton dispatch
// can therefore leave PopupPalette on Build. Only restore Root when the
// officer binding was armed and an officer order was actually accepted; a
// normal click on A1's Build command must still open the Build submenu.
inline bool should_restore_legacy_officer_root_menu(
    bool officer_root_order_pending,
    bool selected_producer_matches,
    std::uint32_t current_menu) noexcept {
    return a2fo_rs_stage9_a1_ui_restore(
        officer_root_order_pending, selected_producer_matches, current_menu) != 0;
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

inline LegacyUiDecision decide_legacy_gameplay_ui(
    const LegacyUiEvidence& evidence) noexcept {
    LegacyUiDecision decision{};
    std::uint8_t legacy = 0, apply = 0;
    a2fo_rs_stage9_a1_ui_decide(evidence.has_speed_panel_area,
        evidence.has_control_panel_area, evidence.has_screen_width,
        evidence.has_screen_height, &legacy, &apply,
        &decision.reference_width, &decision.reference_height);
    decision.legacy_layout = legacy != 0;
    decision.apply_reference_resolution = apply != 0;
    return decision;
}

inline A2foA1UiArea private_area(const LegacyUiArea& area) noexcept {
    return {area.x, area.y, area.width, area.height};
}

inline LegacyUiArea public_area(const A2foA1UiArea& area) noexcept {
    return {area.x, area.y, area.width, area.height};
}

inline NativeUiRectangle public_rectangle(const A2foA1UiRect& rectangle) noexcept {
    return {rectangle.left, rectangle.top, rectangle.right, rectangle.bottom};
}

inline NativeUiRectangle legacy_area_to_native_rectangle(
    const LegacyUiArea& area) noexcept {
    const A2foA1UiArea input = private_area(area);
    A2foA1UiRect output{};
    a2fo_rs_stage9_a1_ui_rect(&input, &output);
    return public_rectangle(output);
}

inline bool usable_legacy_ui_area(const LegacyUiArea& area) noexcept {
    const A2foA1UiArea input = private_area(area);
    return a2fo_rs_stage9_a1_ui_usable(&input) != 0;
}

inline LegacyUiArea legacy_ui_area_union(
    const LegacyUiArea& first, const LegacyUiArea& second,
    const LegacyUiArea& third) noexcept {
    const A2foA1UiArea a = private_area(first), b = private_area(second),
                       c = private_area(third);
    A2foA1UiArea output{};
    a2fo_rs_stage9_a1_ui_union(&a, &b, &c, &output);
    return public_area(output);
}

inline LegacyUiArea legacy_child_area_in_union(
    const LegacyUiArea& child_panel, const LegacyUiArea& child_area,
    const LegacyUiArea& panel_union) noexcept {
    const A2foA1UiArea panel = private_area(child_panel),
                       child = private_area(child_area),
                       combined = private_area(panel_union);
    A2foA1UiArea output{};
    a2fo_rs_stage9_a1_ui_child_union(&panel, &child, &combined, &output);
    return public_area(output);
}

inline LegacyUiArea legacy_child_area_in_parent(
    const LegacyUiArea& parent_area,
    const LegacyUiArea& child_area) noexcept {
    const A2foA1UiArea parent = private_area(parent_area),
                       child = private_area(child_area);
    A2foA1UiArea output{};
    a2fo_rs_stage9_a1_ui_child_parent(&parent, &child, &output);
    return public_area(output);
}

struct LegacyRaceIconLayout {
    NativeUiRectangle component{};
    NativeUiRectangle display{};
};

// RaceIcon has two independent rectangles. The component rectangle owns the
// team-coloured race-icon bar, while the embedded display rectangle positions
// the smaller race insignia inside it. Armada II retained both fields, but its
// same-named CFG values belong to the much larger A2 Status Report.
inline LegacyRaceIconLayout legacy_race_icon_layout(
    const LegacyUiArea& component_area,
    const LegacyUiArea& display_area) noexcept {
    const A2foA1UiArea component = private_area(component_area),
                       display = private_area(display_area);
    A2foA1UiRect component_output{}, display_output{};
    a2fo_rs_stage9_a1_ui_race_icon(&component, &display,
                                    &component_output, &display_output);
    return {public_rectangle(component_output), public_rectangle(display_output)};
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

inline LegacyUiArea legacy_speed_rail_button_slot(
    const LegacyUiArea& single_button,
    const LegacyUiArea& separator,
    std::int32_t gap,
    std::uint32_t slot) noexcept {
    const A2foA1UiArea button = private_area(single_button),
                       divider = private_area(separator);
    A2foA1UiArea output{};
    a2fo_rs_stage9_a1_ui_speed_slot(&button, &divider, gap, slot, &output);
    return public_area(output);
}

inline std::int32_t legacy_indexed_ui_key(
    std::string_view key, std::string_view prefix,
    std::int32_t maximum_index) noexcept {
    return a2fo_rs_stage9_a1_ui_index(
        reinterpret_cast<const std::uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const std::uint8_t*>(prefix.data()), prefix.size(),
        maximum_index);
}

// Armada II split ShipDisplay into low/middle/tall layouts. Armada I has one
// Status Report panel and one background for all selection modes, so the A2
// loader names must resolve back to those original entries for a raw A1 CFG.
inline const char* legacy_ship_display_rectangle_alias(
    std::string_view key) noexcept {
    return a2fo_rs_stage9_a1_ui_rect_alias(
        reinterpret_cast<const std::uint8_t*>(key.data()), key.size());
}

inline const char* legacy_ship_display_string_alias(
    std::string_view key) noexcept {
    return a2fo_rs_stage9_a1_ui_string_alias(
        reinterpret_cast<const std::uint8_t*>(key.data()), key.size());
}

}  // namespace a1compat
