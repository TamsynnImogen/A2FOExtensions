#pragma once
#include "craft_preparation_capture.hpp"
#include <vector>
namespace a2fo::native11 {
enum class WorldPreparationStatus {association_unavailable,preparation_unavailable,captured};
struct WorldCraftPreparation {
    WorldMember member;
    WorldPreparationStatus status=WorldPreparationStatus::association_unavailable;
    CapturedCraftPreparation preparation;
    std::string reason;
};
struct WorldCraftPreparations {
    std::uint32_t native_x87_control=0;
    std::vector<WorldCraftPreparation> members;
};
bool same_world_craft_preparations(const WorldCraftPreparations&,const WorldCraftPreparations&) noexcept;
// Two full logical-state sweeps, shared class/race/cache consistency and final
// actual model/list rewalk. Models/current class, callback ownership and frame
// are externally bracketed. No cached node flags, geometry or visibility claim.
bool capture_world_craft_preparations(const WorldModelAssociations&,WorldModelReadProfile,
    CraftPreparationReadProfile,std::uint32_t native_x87_control,
    void* read_context,Read,void* model_context,PinWorldModel,
    void* class_context,PinLightClass,WorldCraftPreparations&,std::string& error) noexcept;
}
