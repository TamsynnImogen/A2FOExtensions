#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
using A2foStage10VisibilityLowerStep = void (*)(void*, std::size_t);
using A2foStage10VisibilityLastStep = std::uint8_t (*)(const void*);

// All callbacks run synchronously while caller-owned context remains alive.
// Native code contains exceptions; failed is a separate byte from callback data.
void a2fo_rs_stage10_visibility_lower(
    std::size_t length, void* context, A2foStage10VisibilityLowerStep step,
    const std::uint8_t* failed);
// 0: right, 1: left, 2: left+right, 3: left+"\\"+right.
std::uint32_t a2fo_rs_stage10_visibility_join(
    std::size_t left_length, std::size_t right_length, const void* context,
    A2foStage10VisibilityLastStep last);
// Presence is separate from the full-width selected index. `selected` is
// written only when size is nonzero; the vector stays native.
std::uint8_t a2fo_rs_stage10_swarm_hardpoint_index(
    std::size_t size, std::size_t request, std::size_t* selected);
}
