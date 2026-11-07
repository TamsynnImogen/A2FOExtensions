#pragma once

#include "scene.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {

// UV coordinates address a normalized atlas rectangle, including its edges.
struct UvRect {
    float u0=0, v0=0, u1=1, v1=1;
};

struct PixelClipRect {
    float left=0, top=0, right=0, bottom=0;
};

struct ScreenSprite {
    float left=0, top=0, width=0, height=0;
    float depth=0; // clip-space z in [0,1]
    UvRect uv{};
    bool has_clip=false;
    PixelClipRect clip{}; // pixel edges; intersected with the target
};

struct WorldBillboard {
    float center[3]{};
    float camera_right[3]{1,0,0};
    float camera_up[3]{0,1,0};
    float width=0, height=0;
    UvRect uv{};
};

struct QuadGeometry {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Externally serialized CPU-only builders. Success replaces out (an invisible
// sprite produces empty geometry); failure leaves out untouched. Each visible
// quad uses top-left, top-right, bottom-right, bottom-left and CW front indices.
bool build_screen_sprite(const ScreenSprite& sprite, std::uint32_t target_width,
                         std::uint32_t target_height, QuadGeometry& out,
                         std::string& error) noexcept;

// The camera-facing normal is -cross(camera_right, camera_up), matching the
// CW vertex winding and a camera looking along +cross(right, up).
bool build_world_billboard(const WorldBillboard& billboard, QuadGeometry& out,
                           std::string& error) noexcept;

} // namespace a2fo::native11
