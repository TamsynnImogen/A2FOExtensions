#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
using A2foStage10CoreRenderStep=std::uintptr_t (*)(void*,std::uint32_t,std::uintptr_t);
std::uintptr_t a2fo_rs_stage10_core_render_flow(std::uint32_t,void*,A2foStage10CoreRenderStep,std::uintptr_t);
void a2fo_rs_stage10_core_render_lower16(std::uint16_t*,std::size_t);
std::uint8_t a2fo_rs_stage10_core_render_separator(std::uint8_t,std::uint8_t);
}
