#pragma once
#include "model_lod_context.hpp"
#include "model_node_template.hpp"
#include "owned_animation_pose.hpp"
#include <vector>
namespace a2fo::native11 {
struct ModelLodVisibility {
    std::vector<NativeMatrix34> world;
    std::vector<std::uint32_t> flags,selected_nodes;
    std::vector<bool> visible;
};
// Combines a fresh pose/context with caller-evaluated class flags. LOD levels
// must be hidden at load and class policy must leave their two low bits alone.
// class_low_bit_writes records bits written even when their value did not
// change; comparing only final flags cannot detect native equal-selection
// no-op behavior. The caller must provide proven write masks, not infer them.
// ambiguous stateful aliases fail closed. Native traversal order is retained.
// No live state or previous instance's selection is consumed. Failure is atomic.
// Still requires class/material authority and complete world coverage upstream.
bool evaluate_model_lod_visibility(const ModelNodeTemplate&,
    const OwnedAnimationPose&,const ModelLodContext&,
    const std::vector<std::uint32_t>& class_flags,
    const std::vector<std::uint8_t>& class_low_bit_writes,ModelLodVisibility&,
    std::string& error) noexcept;
} // namespace a2fo::native11
