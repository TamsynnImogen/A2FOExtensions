#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsA1OfficerStep)(void*,std::uint32_t,std::size_t);
std::uint8_t a2fo_rs_stage8_a1_officer(
    std::uint32_t,void*,A2foRsA1OfficerStep);
}
