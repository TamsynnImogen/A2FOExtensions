#pragma once
#include "capture_service.hpp"
#include "world_lighting.hpp"
namespace a2fo::native11 {
// Synchronous owned world-light -> native service adapter. Configures the
// source-index shadow mask, then submits the same light records for shading.
// Failure preserves rendered pixels; shadow configuration is session state.
std::int32_t submit_lit_capture(const A2FO_N11_API4&,A2FO_N11_Session,
    const Frame&,std::uint64_t source_epoch,const LightingFrame&) noexcept;
}
