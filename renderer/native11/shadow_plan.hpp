#pragma once
#include "scene.hpp"

namespace a2fo::native11 {
struct ShadowBounds { float low[3], high[3]; };
struct ShadowLightPlan { std::uint32_t count=0; float world_to_clip[6][16]{}; };

bool plan_shadow_light(const Light& light,const ShadowBounds& bounds,ShadowLightPlan& output) noexcept;
// Major absolute component; ties prefer X, then Y, then Z. Zero selects +X.
std::uint32_t shadow_point_face(const float direction[3]) noexcept;
} // namespace a2fo::native11
