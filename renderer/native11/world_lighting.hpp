#pragma once
#include "light_config.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct PlacedLight {
    std::uint64_t source_id=0;
    LightConfig config{};
    float local_to_world[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
};
struct LightingFrame {
    std::vector<Light> lights;
    std::vector<std::uint64_t> source_ids;
    float ambient[3]{0.04f,0.04f,0.04f};
    ShadowSettings shadows{};
};

// Requires a complete world-source list, including sources outside the camera
// frustum. Disabled IDs still participate in uniqueness checks. Enabled source
// poses must be finite, affine, and invertible even when their energy is zero.
// A light is located at its source's local origin; its direction is local.
// Failure leaves output unchanged. No engine-owned pointers are retained.
bool assemble_world_lighting(const std::vector<PlacedLight>& sources,
    bool complete_world_sources,const float ambient[3],LightingFrame& output,
    std::string& error) noexcept;
} // namespace a2fo::native11
