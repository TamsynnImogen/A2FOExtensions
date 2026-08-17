#include "directional_shield_fill.hpp"

#include <algorithm>
#include <cmath>

namespace a2fo::craft_identity {

DirectionalShieldFill centered_directional_shield_fill(
    const RectangleF& source, const RectangleF& destination,
    float ratio, bool horizontal) noexcept {
    const float retained_ratio = std::isfinite(ratio)
        ? std::max(0.0f, std::min(1.0f, ratio)) : 0.0f;
    DirectionalShieldFill fill{source, destination};
    if (horizontal) {
        const float source_width = source.width * retained_ratio;
        const float destination_width =
            destination.width * retained_ratio;
        fill.source.x += (source.width - source_width) * 0.5f;
        fill.source.width = source_width;
        fill.destination.x +=
            (destination.width - destination_width) * 0.5f;
        fill.destination.width = destination_width;
    } else {
        const float source_height = source.height * retained_ratio;
        const float destination_height =
            destination.height * retained_ratio;
        fill.source.y += (source.height - source_height) * 0.5f;
        fill.source.height = source_height;
        fill.destination.y +=
            (destination.height - destination_height) * 0.5f;
        fill.destination.height = destination_height;
    }
    return fill;
}

int directional_shield_segment_at(
    const std::array<RectangleF, 4>& rectangles,
    float cursor_x, float cursor_y) noexcept {
    if (!std::isfinite(cursor_x) || !std::isfinite(cursor_y)) return -1;
    for (std::size_t index = 0; index < rectangles.size(); ++index) {
        const RectangleF& rectangle = rectangles[index];
        if (!std::isfinite(rectangle.x) ||
            !std::isfinite(rectangle.y) ||
            !std::isfinite(rectangle.width) || rectangle.width <= 0.0f ||
            !std::isfinite(rectangle.height) || rectangle.height <= 0.0f) {
            continue;
        }
        if (cursor_x >= rectangle.x &&
            cursor_x <= rectangle.x + rectangle.width &&
            cursor_y >= rectangle.y &&
            cursor_y <= rectangle.y + rectangle.height) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

}  // namespace a2fo::craft_identity
