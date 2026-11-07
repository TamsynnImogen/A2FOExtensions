#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
typedef std::uint64_t (*A2foRsStage10CoreMenuStep)(void*, std::uint32_t, std::uint64_t);

// 0: right, 1: left, 2: concatenate, 3: insert backslash.
std::uint32_t a2fo_rs_stage10_core_menu_join_path(
    std::size_t left_length, std::size_t right_length, std::uint8_t left_last);
void a2fo_rs_stage10_core_menu_ascii_lower(
    std::size_t length, void* context, A2foRsStage10CoreMenuStep step,
    const std::uint8_t* failed);
std::uint64_t a2fo_rs_stage10_core_menu_flow(
    std::uint32_t kind, void* context, A2foRsStage10CoreMenuStep step,
    const std::uint8_t* failed);
}
