#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t (*A2foRsNebulaStep)(void*, std::uint32_t, std::uint64_t);
std::uint64_t a2fo_rs_stage9_nebula_flow(std::uint32_t, A2foRsNebulaStep, void*, const std::uint8_t*);
std::uint32_t a2fo_rs_stage9_nebula_texture_bytes(std::uint32_t,std::uint32_t);
}
