#pragma once
#include "world_preparation_capture.hpp"
#include "prepared_craft_visibility_capture.hpp"
namespace a2fo::native11 {
using PinWorldTemplate=std::shared_ptr<const ModelNodeTemplate>(*)(void*,const std::shared_ptr<const ModelAsset>&);
enum class WorldPreparedVisibilityStatus {
    association_unavailable,preparation_unavailable,template_unavailable,visibility_unavailable,captured
};
struct WorldPreparedVisibilityMember {
    WorldMember member;
    WorldPreparedVisibilityStatus status=WorldPreparedVisibilityStatus::association_unavailable;
    CapturedCraftVisibility capture;
    CraftVisibilityResult policy;
    std::string reason;
};
struct WorldPreparedVisibility {
    std::uint32_t native_x87_control=0,engine=0,render_context=0,quality=0;
    std::vector<WorldPreparedVisibilityMember> members;
};
// Serialized world boundary, pure callbacks. Two full sweeps, template tuple
// pins, shared context consistency and final actual model/preparation rewalks.
// This is base node-flag policy, never FOW/full visibility, material or geometry.
// Caller qualifies loaded policy code, main-view context, frame and lifetimes.
bool capture_world_prepared_visibility(const WorldModelAssociations&,const WorldCraftPreparations&,
    WorldModelReadProfile,CraftVisibilityReadProfile,CraftPreparationReadProfile,
    void* read_context,Read,void* model_context,PinWorldModel,
    void* class_context,PinLightClass,void* template_context,PinWorldTemplate,
    WorldPreparedVisibility&,std::string& error) noexcept;
}
