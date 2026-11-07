#pragma once
#include "animation_channel.hpp"
#include "instance_animation_time.hpp"
#include "model_pose_snapshot.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct ModelNodeTemplate;
struct PoseChannelPlayback {
    float offset=0;
    std::uint32_t interpolation=1;
    std::vector<float> timestamps;
    bool has_guard=false;
    float scalar_guard=0;
};
struct OwnedAnimationPose {
    std::uint32_t instance=0,database=0;
    std::uint64_t model_serial=0;
    std::vector<std::array<float,12>> local; // native Matrix34 order
    std::vector<float> scales;
    std::vector<std::array<float,16>> local_to_model; // row-major, column-vector
};
// Fresh owned transforms only. Does not certify game class/procedural transforms,
// visibility, LOD, materials, or complete caster coverage. No engine state is
// read or written. Failure leaves output unchanged.
bool evaluate_owned_animation_pose(const ModelPoseSnapshot&,
    const InstanceAnimationTime&,const std::vector<PoseChannelPlayback>&,
    OwnedAnimationPose&,std::string& error) noexcept;
// Same channel policy, seeded from a captured native node template's local
// matrices and scales. Does not certify class, LOD, material or caster policy.
bool evaluate_owned_animation_pose_from_template(const ModelPoseSnapshot&,
    const InstanceAnimationTime&,const std::vector<PoseChannelPlayback>&,
    const ModelNodeTemplate&,OwnedAnimationPose&,std::string& error) noexcept;
} // namespace a2fo::native11
