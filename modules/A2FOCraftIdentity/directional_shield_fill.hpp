#pragma once

#include <array>

namespace a2fo::craft_identity {

struct RectangleF {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct DirectionalShieldFill {
    RectangleF source{};
    RectangleF destination{};
};

// Retains the centre of a directional-shield segment. Forward and aft use
// the horizontal axis; port and starboard use the vertical axis.
DirectionalShieldFill centered_directional_shield_fill(
    const RectangleF& source, const RectangleF& destination,
    float ratio, bool horizontal) noexcept;

// Returns forward/aft/port/starboard as 0..3, or -1 when the cursor is not
// over a visible arc rectangle.
int directional_shield_segment_at(
    const std::array<RectangleF, 4>& rectangles,
    float cursor_x, float cursor_y) noexcept;

}  // namespace a2fo::craft_identity
