#pragma once
#include <cstdint>
#include <cstddef>
extern "C" {
typedef std::uint64_t(*A2foRsHybridStateStep)(void*,std::uint32_t,std::uintptr_t);
std::uint64_t a2fo_rs_stage8_hybrid_state(std::uint32_t,A2foRsHybridStateStep,void*);
}
