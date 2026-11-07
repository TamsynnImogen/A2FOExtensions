#pragma once

#include <cstddef>
#include <cstdint>

using A2foStage9DirectionalParseStep = std::uint8_t (*)(
    void*, std::uint32_t, std::uint32_t, std::uint64_t*);

extern "C" {
bool a2fo_rs_stage9_directional_field_value(
    void*, A2foStage9DirectionalParseStep, std::uint32_t);
bool a2fo_rs_stage9_directional_float_suffix(
    const std::uint8_t*, std::size_t, bool, std::uint32_t);
}
