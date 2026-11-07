#pragma once
#include <cstdint>
extern "C" {
typedef std::uintptr_t (*A2foRsCraftSpriteStep)(void*,std::uint32_t,std::uintptr_t);
std::uint8_t a2fo_rs_stage9_craft_sprite_flow(std::uint32_t,A2foRsCraftSpriteStep,void*,const std::uint8_t*);
}
extern "C" {
struct A2foRsAnchorCandidate {
    std::uintptr_t component;
    std::int32_t rectangle[4];
    const std::int32_t* configured;
};
std::uintptr_t a2fo_rs_stage9_craft_anchor(
    const A2foRsAnchorCandidate*, const std::int32_t*,
    const A2foRsAnchorCandidate*, std::int32_t*);
void a2fo_rs_stage9_craft_anchor_fallback(const std::int32_t*,std::int32_t,std::int32_t*);
}
static_assert(sizeof(A2foRsAnchorCandidate) == 2 * sizeof(std::uintptr_t) + 4 * sizeof(std::int32_t),
              "anchor ABI contains only pointer tokens and four i32 fields");
