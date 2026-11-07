#pragma once
#include "../../sdk/include/a2fo_native11_world_api.h"
#include "scene_capture.hpp"
#include "world_lighting.hpp"
namespace a2fo::native11 {
// API5 world adapter: complete_casters is an explicit host claim, never inferred
// from camera-visible geometry. The owned Frame includes shadow-only objects.
std::int32_t submit_world_capture(const A2FO_N11_API5&,A2FO_N11_Session,
    const Frame&,std::uint64_t source_epoch,const LightingFrame&,bool complete_casters) noexcept;
// Strict-S_OK variant for a presentation transaction. The checkpoint runs
// immediately before and after each backend call; false aborts the sequence.
std::int32_t submit_world_capture_checked(const A2FO_N11_API5&,A2FO_N11_Session,
    const Frame&,std::uint64_t source_epoch,const LightingFrame&,bool complete_casters,
    bool (*checkpoint)(void*) noexcept,void* context,bool* submitted=nullptr) noexcept;
}
