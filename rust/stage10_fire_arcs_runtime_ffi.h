#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
using A2foStage10FireArcStep = std::uintptr_t (*)(void*, std::uint32_t, std::uintptr_t);
std::uintptr_t a2fo_rs_stage10_firearc_flow(std::uint32_t, void*, A2foStage10FireArcStep);
std::uint32_t a2fo_rs_stage10_firearc_colour_index(std::uintptr_t);
std::uint32_t a2fo_rs_stage10_firearc_root_count(std::uint32_t);
std::uint32_t a2fo_rs_stage10_firearc_radius(std::uint32_t);
std::uint8_t a2fo_rs_stage10_firearc_colour(std::uint32_t*);
}
