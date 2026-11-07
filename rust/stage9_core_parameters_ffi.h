#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t (*A2foRsParameterStep)(void*, std::uint32_t);
std::uint8_t a2fo_rs_stage9_parameter_get(
    std::uint32_t, A2foRsParameterStep, void*);
}
