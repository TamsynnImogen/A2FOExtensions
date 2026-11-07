#pragma once
#include <cstddef>
#include <cstdint>

// Callbacks borrow native state synchronously. Status is separate from output:
// name: phase 0 checked read, phase 1 native append; 0 abort, 1 continue.
// texture: 0 abort, 1 continue.
using A2foM3dNameStep = std::uint8_t (*)(void*, std::size_t, std::uint32_t, std::uint8_t*);
using A2foM3dTextureStep = std::uint8_t (*)(void*, std::uint32_t, std::uint8_t*);
extern "C" std::uint8_t a2fo_rs_stage10_m3d_bounded_name(
    A2foM3dNameStep, void*);
extern "C" void a2fo_rs_stage10_m3d_texture_candidates(
    A2foM3dTextureStep, void*);
