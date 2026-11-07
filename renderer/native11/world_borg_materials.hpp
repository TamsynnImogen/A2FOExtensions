#pragma once
#include "world_preparation_capture.hpp"
#include "craft_borg_material_capture.hpp"
namespace a2fo::native11 {
using PinWorldMaterialBaseline=std::shared_ptr<const ModelMaterialBaseline>(*)(
    void*,const std::shared_ptr<const ModelAsset>&);
inline bool same_material_baseline_owners(const std::shared_ptr<const ModelMaterialBaseline>& a,
    const std::shared_ptr<const ModelMaterialBaseline>& b) noexcept {
    return (!a&&!b)||(a&&b&&a->model==b->model&&a->authored==b->authored&&
        a->nodes==b->nodes&&a->data==b->data);
}
enum class WorldBorgMaterialStatus {
    association_unavailable,preparation_unavailable,baseline_unavailable,
    table_unavailable,correlated,route_unavailable,selector_bypassed
};
struct WorldBorgMaterialMember {
    WorldMember member;
    WorldBorgMaterialStatus status=WorldBorgMaterialStatus::association_unavailable;
    std::shared_ptr<const ModelMaterialBaseline> baseline;
    std::shared_ptr<const CapturedCraftPreparation> preparation;
    bool route_valid=false;
    CraftBorgMaterialRoute route;
    CapturedCraftBorgTable table;
    std::vector<CraftMaterialSlotWrite> writes;
    std::string reason;
};
struct WorldBorgMaterials {
    std::uint32_t native_x87_control=0;
    std::vector<WorldBorgMaterialMember> members;
};
// Owner-serialized diagnostic correlation for an exact world/model/preparation
// roster. Two full sweeps, shared baseline/class-table consistency, final
// actual model/preparation rewalks and version pins. Never reads live slots.
// Caller qualifies named native operations, frame and lifetime. These outputs
// do NOT qualify class readiness/overrides, render callback or prior slot state,
// group/channel mapping, complete effective materials, or geometry submission.
// No native calls; unavailable members stay explicit; failure preserves output.
bool capture_world_borg_materials(const WorldModelAssociations&,const WorldCraftPreparations&,
    WorldModelReadProfile,CraftPreparationReadProfile,MaterialBaselineReadProfile,
    CraftBorgMaterialRouteProfile,
    void* read_context,Read,void* model_context,PinWorldModel,
    void* class_context,PinLightClass,void* baseline_context,PinWorldMaterialBaseline,
    void* texture_context,ResolveTextureAsset,WorldBorgMaterials&,std::string& error) noexcept;
}
