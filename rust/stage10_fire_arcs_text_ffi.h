#pragma once
#include <cstddef>
#include <cstdint>
// Synchronous caller-owned native callbacks; no C++ exception crosses Rust.
using A2foFireArcsTextStep = std::uintptr_t (*)(void*, std::uint32_t, std::uintptr_t);
extern "C" std::uint8_t a2fo_rs_stage10_fire_arcs_join(std::size_t, std::size_t, std::uint8_t);
extern "C" std::uint8_t a2fo_rs_stage10_fire_arcs_read(A2foFireArcsTextStep, void*, const std::int64_t*, const std::uint8_t*);
extern "C" std::uintptr_t a2fo_rs_stage10_fire_arcs_root(A2foFireArcsTextStep, void*, const std::uint8_t*, const std::uint8_t*);
extern "C" void a2fo_rs_stage10_fire_arcs_trim(A2foFireArcsTextStep, void*, const std::uint8_t*);
extern "C" void a2fo_rs_stage10_fire_arcs_lower(A2foFireArcsTextStep, void*);
extern "C" std::uint8_t a2fo_rs_stage10_fire_arcs_query(A2foFireArcsTextStep, void*, const std::uint8_t*);
extern "C" std::uint8_t a2fo_rs_stage10_fire_arcs_float(A2foFireArcsTextStep, void*);
