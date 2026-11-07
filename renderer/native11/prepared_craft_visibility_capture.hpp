#pragma once
#include "craft_visibility_capture.hpp"
#include "craft_preparation_capture.hpp"
namespace a2fo::native11 {
// Logical preparation is copied and retained, and must still agree with fresh
// reads before/after the independently checked roles, topology and context.
// Source code, frame, class behavior, render context and world/model lifetime
// remain caller obligations. This supplies base flag policy, not FOW, selected
// material, pose/LOD, complete geometry or a registered game provider.
// Failure preserves output. No native functions or mutable node outputs read.
bool capture_prepared_craft_visibility(const WorldModelAssociation&,
    const CapturedCraftPreparation&,std::shared_ptr<const ModelNodeTemplate>,
    CraftVisibilityReadProfile,CraftPreparationReadProfile,
    void* read_context,Read,void* class_context,PinLightClass,
    CapturedCraftVisibility&,std::string& error) noexcept;
}
