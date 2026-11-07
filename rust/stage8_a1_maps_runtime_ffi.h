#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsA1MapsOp)(void*,std::uint32_t,std::uint64_t,std::uint64_t);
std::uint8_t a2fo_rs_stage8_a1_find_bzn(void*,A2foRsA1MapsOp);
std::uint8_t a2fo_rs_stage8_a1_read_mdf(void*,A2foRsA1MapsOp);
std::uint8_t a2fo_rs_stage8_a1_apply_mdf(void*,A2foRsA1MapsOp);
std::uint8_t a2fo_rs_stage8_a1_prefers_legacy_aip(void*,A2foRsA1MapsOp);
std::uint64_t a2fo_rs_stage8_a1_resolve_aip(void*,A2foRsA1MapsOp);
}
