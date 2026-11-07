#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsA1ShutdownOp)(void*,std::uint32_t);
void a2fo_rs_stage8_a1_shutdown(void*,A2foRsA1ShutdownOp);
}
