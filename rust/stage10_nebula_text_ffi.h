#pragma once
#include <cstddef>
#include <cstdint>
// All callbacks are synchronous and must be noexcept; no C++ exception crosses Rust.
using A2foNebulaTextStep = std::uintptr_t (*)(void*, std::uint32_t, std::size_t);
extern "C" std::uint8_t a2fo_rs_stage10_nebula_join(std::size_t, std::size_t, std::uint8_t);
extern "C" void a2fo_rs_stage10_nebula_trim(std::uint8_t*, std::size_t, A2foNebulaTextStep, void*, std::size_t*, std::size_t*);
extern "C" void a2fo_rs_stage10_nebula_slashes(std::uint8_t*, std::size_t);
extern "C" std::uint8_t a2fo_rs_stage10_nebula_query_admit(const void*, const std::uint8_t*, const void*, const void*);
extern "C" std::uint8_t a2fo_rs_stage10_nebula_query_found(std::size_t, std::uint8_t);
extern "C" std::uint8_t a2fo_rs_stage10_nebula_copy(A2foNebulaTextStep, void*, std::size_t, const std::uint8_t*);
extern "C" void a2fo_rs_stage10_nebula_lower(A2foNebulaTextStep, void*, std::size_t, const std::uint8_t*);
extern "C" std::uint8_t a2fo_rs_stage10_nebula_floats(const std::uint8_t*, float*, std::size_t, A2foNebulaTextStep, void*);
extern "C" void a2fo_rs_stage10_nebula_vector(A2foNebulaTextStep, void*);
