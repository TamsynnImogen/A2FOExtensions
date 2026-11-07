#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t (*A2foRsConfigStep)(void*, std::uint32_t, std::uint64_t);
std::uint64_t a2fo_rs_stage9_config_flow(std::uint32_t, A2foRsConfigStep, void*, const std::uint8_t*);
}
