#pragma once
#include "owned_animation_pose.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct RuntimePoseReadProfile {
    InstanceAnimationTimeProfile clock;
    std::uint32_t scalar_vtable=0,matrix_vtable=0;
    std::uint32_t scalar_play=0,matrix_play=0;
};
struct RuntimePoseChannels {
    ModelPoseSnapshot model; // same model owner; pose owns copied runtime keys
    InstanceAnimationTime clock;
    std::vector<PoseChannelPlayback> playback;
};
// Read-only capture of the verified database channel list and key vectors.
// The result owns every copied byte and does not use the native selected-key
// field or shared current-instance slot. It certifies only these fields, not
// hierarchy, class policy, visibility, materials or full caster coverage.
// Caller externally serializes the observation; failure leaves output intact.
bool capture_runtime_pose_channels(const Object&,const ModelPoseSnapshot& authored,
    RuntimePoseReadProfile,void* context,Read,RuntimePoseChannels& output,
    std::string& error) noexcept;
} // namespace a2fo::native11
