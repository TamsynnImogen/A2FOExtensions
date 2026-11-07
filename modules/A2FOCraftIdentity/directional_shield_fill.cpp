#include "directional_shield_fill.hpp"
#include "../../rust/gameplay_ffi.h"

namespace a2fo::craft_identity {

DirectionalShieldFill centered_directional_shield_fill(const RectangleF &source,
                                                       const RectangleF &destination, float ratio,
                                                       bool horizontal) noexcept {
    const A2foRsRect native_source{source.x, source.y, source.width, source.height};
    const A2foRsRect native_destination{destination.x, destination.y, destination.width,
                                        destination.height};
    A2foRsFill result{};
    a2fo_rs_gameplay_centered_fill(&native_source, &native_destination, ratio, horizontal, &result);
    DirectionalShieldFill fill{
        {result.source.x, result.source.y, result.source.width, result.source.height},
        {result.destination.x, result.destination.y, result.destination.width,
         result.destination.height}};
    return fill;
}

int directional_shield_segment_at(const std::array<RectangleF, 4> &rectangles, float cursor_x,
                                  float cursor_y) noexcept {
    A2foRsRect native[4]{};
    for (std::size_t i = 0; i < 4; ++i)
        native[i] = {rectangles[i].x, rectangles[i].y, rectangles[i].width, rectangles[i].height};
    return a2fo_rs_gameplay_segment_at(native, cursor_x, cursor_y);
}

} // namespace a2fo::craft_identity
