#pragma once
#include "material_observation.hpp"
#include "quad_geometry.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace a2fo::native11 {
struct SpriteDispatch {
    std::uint32_t device=0,dispatch_table=0,sprite=0,position=0,size=0;
    float uv[4]{}; // u, v, du, dv copied from the caller's immutable request
    std::uint32_t native_x87_control=0;
};
struct SpriteReadProfile {
    std::uint32_t device_vtable=0,sprite_vtable=0,draw_target=0;
};
// Native style and effective sampler numbers remain raw observations.
struct SpriteObservation {
    SpriteDispatch dispatch{};
    ScreenSprite geometry{};
    std::uint32_t packed_argb=0,material=0;
    std::uint32_t alpha_test=0,src_blend=0,dst_blend=0,fill_mode=0;
    std::uint32_t address_u=0,address_v=0;
    std::uint32_t mag_filter=0,min_filter=0,mip_filter=0;
    std::uint32_t texture=0;
    std::shared_ptr<const TextureAsset> texture_owner;
};
// Reads the supplied address layout twice and re-resolves the same immutable
// texture owner. Failure leaves output untouched. No native call is made.
bool capture_sprite_observation(const SpriteDispatch&,SpriteReadProfile,
    void* read_context,Read,void* texture_context,ResolveTextureAsset,
    std::shared_ptr<const SpriteObservation>& output,std::string& error) noexcept;
} // namespace a2fo::native11
