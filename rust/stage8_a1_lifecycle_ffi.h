#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsA1LifecycleOp)(void*,std::uint32_t,std::uint64_t);
std::uint8_t a2fo_rs_stage8_a1_lifecycle_init(void*,A2foRsA1LifecycleOp);
}
