#pragma once
#include "material_observation.hpp"
#include "owned_animation_pose.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Bind a caller-supplied fresh owned pose and explicit per-node visibility to
// an already selected material observation. This only translates supplied
// state; it does not establish class, LOD, visibility, or caster authority.
// Failure leaves output unchanged. Owners and texture assets remain pinned.
bool bind_owned_part_states(const Object&,const ModelPoseSnapshot&,
    const ModelMaterialSource&,const OwnedAnimationPose&,
    const std::vector<std::uint8_t>& visible,
    const MaterialObservation& selected_materials,
    std::shared_ptr<const MaterialObservation>& output,std::string& error) noexcept;
} // namespace a2fo::native11
