#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
std::int32_t a2fo_rs_stage10_energy_whole(std::uint32_t bits);
std::uint8_t a2fo_rs_stage10_energy_colour_valid(const float* colour);
std::uint8_t a2fo_rs_stage10_energy_rectangle_usable(const std::int32_t* rectangle);
void a2fo_rs_stage10_energy_translate(std::int32_t* rectangle,
                                     const std::int32_t* anchor,
                                     const std::int32_t* target);
void a2fo_rs_stage10_energy_default_rectangles(std::int32_t* rectangles,
                                              const std::int32_t* anchor);
void a2fo_rs_stage10_energy_add(float* amount, std::uint32_t current);
void a2fo_rs_stage10_energy_age(float* remaining, const float* elapsed);
std::uint8_t a2fo_rs_stage10_energy_active(std::uint32_t bits);
std::size_t a2fo_rs_stage10_energy_unquote(char* data, std::size_t size);
}
