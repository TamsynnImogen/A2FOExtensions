#pragma once

#include <array>
#include <cstdint>

namespace a2fo::craft_identity {

struct RawRectangle {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct NativeRectangle {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

inline bool usable_native_rectangle(const NativeRectangle& rectangle) noexcept {
    return rectangle.right > rectangle.left &&
        rectangle.bottom > rectangle.top;
}

inline NativeRectangle translated_rectangle(
    const NativeRectangle& live_anchor,
    const RawRectangle& configured_anchor,
    const RawRectangle& configured_target) noexcept {
    NativeRectangle result = live_anchor;
    result.left += configured_target.x - configured_anchor.x;
    result.top += configured_target.y - configured_anchor.y;
    result.right += configured_target.x - configured_anchor.x +
        configured_target.width - configured_anchor.width;
    result.bottom += configured_target.y - configured_anchor.y +
        configured_target.height - configured_anchor.height;
    return result;
}

struct SelectedPanelTextAnchor {
    void* component = nullptr;
    NativeRectangle captain_rectangle{};
};

struct SelectedPanelAnchorCandidate {
    void* component = nullptr;
    NativeRectangle rectangle{};
    const RawRectangle* configured_rectangle = nullptr;
};

inline SelectedPanelTextAnchor resolve_selected_panel_anchor(
    const SelectedPanelAnchorCandidate& captain,
    const RawRectangle* configured_captain,
    const std::array<SelectedPanelAnchorCandidate, 2>& fallbacks) noexcept {
    // Armada initializes the captain GUIText for the whole InfoDisplay,
    // including when only its producer panel is rendered. Reuse its geometry
    // AND font/display state for both ships and shipyards.
    if (captain.component && usable_native_rectangle(captain.rectangle)) {
        return {captain.component, captain.rectangle};
    }
    for (const auto& candidate : fallbacks) {
        if (!candidate.component ||
            !usable_native_rectangle(candidate.rectangle)) continue;
        // Without a configured captain origin, existing automatic rows are
        // positioned below the available native name/class row.
        if (!configured_captain) {
            return {candidate.component, candidate.rectangle};
        }
        // A live name rectangle alone cannot establish the captain origin.
        // Skip it rather than silently shifting every configured A2FO row.
        if (!candidate.configured_rectangle) continue;
        const auto rebased = translated_rectangle(
            candidate.rectangle, *candidate.configured_rectangle,
            *configured_captain);
        if (usable_native_rectangle(rebased)) {
            return {candidate.component, rebased};
        }
    }
    return {};
}

}  // namespace a2fo::craft_identity
