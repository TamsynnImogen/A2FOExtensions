#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
using A2foA1SpeedStep=std::uint64_t (*)(void*,std::uint32_t,std::size_t);
std::uintptr_t a2fo_rs_stage9_a1_speed(void*,A2foA1SpeedStep);
const char* a2fo_rs_stage9_a1_speed_kind(std::uint32_t);
const char* a2fo_rs_stage9_a1_speed_source(std::uint32_t);
}
