#pragma once
#include <cstdint>

extern "C" {
using A2foStage8UnsupportedFail = std::int32_t (*)(void*, const char*);
std::int32_t a2fo_rs_stage8_b_unsupported(std::uint32_t operation, void* context,
                                           A2foStage8UnsupportedFail fail);
}
