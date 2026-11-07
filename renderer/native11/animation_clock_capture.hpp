#pragma once
#include "animation_phase.hpp"
#include "scene_capture.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct AnimationClockProfile {
    std::uint32_t engine_pointer=0,elapsed_pointer=0;
    std::uint32_t instance_vtable=0,database_vtable=0;
    std::uint32_t float_channel_vtable=0,matrix_channel_vtable=0;
    std::uint32_t float_play=0,matrix_play=0;
};
struct AnimationChannelClock {
    std::uint32_t identity=0,native_key=0;
    AnimationPhaseInput timing;
    AnimationPhase evaluated;
};
struct AnimationClockSnapshot {
    std::uint32_t instance=0,database=0,flags=0;
    bool animating=false;
    float elapsed=0;
    std::vector<AnimationChannelClock> channels;
};
// Read only, externally serialized post-render observation. Addresses are opaque
// identities. The caller supplies an exact-image profile and a nonthrowing reader.
bool capture_animation_clocks(const Object&,AnimationClockProfile,void* context,
                              Read,AnimationClockSnapshot& output,
                              std::string& error) noexcept;
} // namespace a2fo::native11
