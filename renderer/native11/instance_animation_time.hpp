#pragma once
#include "scene_capture.hpp"
#include <cstdint>
#include <string>

namespace a2fo::native11 {
struct InstanceAnimationTimeProfile {
    std::uint32_t engine_pointer=0,instance_vtable=0,database_vtable=0;
};
struct InstanceAnimationTime {
    std::uint32_t engine=0,instance=0,database=0,flags=0;
    bool animating=false;
    float engine_time=0,start_time=0,elapsed=0;
};
// Read-only, two-pass snapshot of one observed instance. The caller supplies
// an exact-image profile and nonthrowing reader. Fresh timing alone does not
// certify pose, class policy, visibility, or complete world geometry.
// Failure leaves output untouched. No engine state is written or called.
bool capture_instance_animation_time(const Object&,InstanceAnimationTimeProfile,
    void* context,Read,InstanceAnimationTime&,std::string& error) noexcept;
} // namespace a2fo::native11
