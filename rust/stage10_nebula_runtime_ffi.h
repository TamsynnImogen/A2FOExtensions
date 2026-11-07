#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
std::uint8_t a2fo_rs_stage10_nebula_float_valid(std::uintptr_t, std::uint32_t);
void a2fo_rs_stage10_nebula_scale(float*, float*, std::uint32_t);
std::uint32_t a2fo_rs_stage10_nebula_colour_key(const std::uint32_t*);
std::uint32_t a2fo_rs_stage10_nebula_threshold(std::uint8_t, std::uint32_t, std::size_t);
std::uint8_t a2fo_rs_stage10_nebula_suffix_before(std::size_t, std::size_t);
}
