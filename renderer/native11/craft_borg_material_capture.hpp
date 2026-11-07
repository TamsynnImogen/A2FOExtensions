#pragma once
#include "craft_borg_material_policy.hpp"
#include "model_material_baseline.hpp"
#include "light_class_store.hpp"
#include "craft_preparation_capture.hpp"
namespace a2fo::native11 {
using PinBorgModel=std::shared_ptr<const ModelAsset>(*)(void*,std::uint32_t,std::uint64_t);
using PinBorgClass=std::shared_ptr<const LightClassRecord>(*)(void*,std::uint64_t);
struct CapturedCraftBorgTable {
    std::shared_ptr<const LightClassRecord> object_class;
    std::shared_ptr<const ModelMaterialBaseline> baseline;
    std::uint32_t class_vtable=0,table_pointer=0;
    std::vector<CraftBorgMaterialRow> rows;
};
struct CraftBorgMaterialRouteProfile {
    std::uint32_t instance_vtable=0,render_function=0;
};
struct CraftBorgMaterialRoute {
    std::uint32_t instance_vtable=0,class_vtable=0,primary_database=0;
    bool selector_reached=false;
};
inline bool same_craft_borg_material_route(const CraftBorgMaterialRoute& a,
    const CraftBorgMaterialRoute& b) noexcept {
    return a.instance_vtable==b.instance_vtable&&a.class_vtable==b.class_vtable&&
        a.primary_database==b.primary_database&&a.selector_reached==b.selector_reached;
}
// Qualifies the pinned base Craft render dispatch (+0x14) and the branch that
// reaches its Borg selector: instance DB == class +0x1dc. The caller supplies
// fresh preparation and qualifies the exact render code; no function is called.
// Two identity/dispatch/primary-DB passes and exact class owner pins. This is
// neither class initialization readiness nor complete callback/overlay authority.
// Failure preserves output. Current slots and cached instance Borg flags are
// never read. A bypass is explicit, not proof of an empty effective material.
bool capture_craft_borg_material_route(const CapturedCraftPreparation&,
    CraftBorgMaterialRouteProfile,void* read_context,Read,
    void* class_context,PinBorgClass,CraftBorgMaterialRoute&,std::string&) noexcept;
// A raw class table correlated with an actual completed-load material baseline.
// Caller establishes CraftClass type, table source, frame/world synchronization
// and policy ordering. A LightClassRecord alone is NOT a type/readiness proof.
// Two complete static topology/alternate/table reads plus current model/class/
// texture version checks. Never reads live slot vectors, flags or transforms.
// Eligible rows must have 4..8 baseline slots; smaller vectors remain unsupported.
// Does not infer effective materials or absence of module/class overrides.
// No native calls; failure preserves output.
bool capture_craft_borg_table(std::shared_ptr<const LightClassRecord>,
    std::shared_ptr<const ModelMaterialBaseline>,MaterialBaselineReadProfile,
    void* read_context,Read,void* model_context,PinBorgModel,
    void* class_context,PinBorgClass,void* texture_context,ResolveTextureAsset,
    CapturedCraftBorgTable& out,std::string& error) noexcept;
} // namespace a2fo::native11
