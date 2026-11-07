#pragma once

#include <cstdint>

struct A2foRsA1MissionScanResult {
    std::uint32_t markers;
    std::uint32_t last_distance;
};

#ifdef _WIN32
using A2foRsA1MissionProbe = std::uint8_t (__cdecl*)(void*, std::uint32_t);
#else
using A2foRsA1MissionProbe = std::uint8_t (*)(void*, std::uint32_t);
#endif

extern "C" void a2fo_rs_stage9_a1_mission_scan(
    std::uint32_t remaining, std::uint32_t record_size, void* context,
    A2foRsA1MissionProbe probe, A2foRsA1MissionScanResult* output);
