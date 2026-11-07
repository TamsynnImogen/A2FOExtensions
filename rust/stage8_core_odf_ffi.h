#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsCoreOdfOp)(void*,std::uint32_t);
std::uint8_t a2fo_rs_stage8_core_odf_snapshot(void*,A2foRsCoreOdfOp);
void a2fo_rs_stage8_core_odf_race(void*,A2foRsCoreOdfOp);
}
