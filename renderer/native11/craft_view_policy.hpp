#pragma once
#include <cstdint>

namespace a2fo::native11 {
struct CraftViewInput {
    float camera[3]{},position[3]{};
    float radius=0,max_distance=0;
    std::uint32_t mode=0;
    bool detailed_range=false,reveal_all=false,instance_cull_flag=false;
    bool skip_near_fade=false,fade_enabled=false;
    float near_fade_min=.5f,near_fade_max=2.f,near_fade_amount=.875f;
};
struct CraftViewResult {
    float weighted_distance=0,distance=0,alpha=1;
    bool culled=false,fade_written=false;
};
// Fresh CPU view policy under the captured native x87 PC/RC. Failure leaves
// output unchanged; it reads no native state and makes no visibility claim.
bool native_craft_view(const CraftViewInput&,std::uint32_t native_x87_control,
    CraftViewResult& output) noexcept;
} // namespace a2fo::native11
