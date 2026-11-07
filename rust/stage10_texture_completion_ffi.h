#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
using A2foStage10TextureStep=std::uintptr_t (*)(void*,std::uint32_t);
std::uint8_t a2fo_rs_stage10_texture_scorch(void*,A2foStage10TextureStep,std::uint32_t*,std::size_t*);
}
