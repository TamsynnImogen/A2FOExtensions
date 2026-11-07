#pragma once
#include "model_node_template.hpp"
#include "material_observation.hpp"

namespace a2fo::native11 {
struct MaterialBaselineReadProfile {
    std::uint32_t database_vtable=0,mesh_type_function=0;
};
struct ModelMeshMaterialBaseline {
    std::uint32_t node_index=0,node_address=0,slot_vector=0;
    // Raw native slot order; no mesh-group or shader-channel mapping is implied.
    // A null owner records a null slot. Non-null owners pin exact texture versions.
    // Flags/key metadata must match; retaining it does not approve draw support.
    std::vector<std::shared_ptr<const TextureAsset>> slots;
    std::shared_ptr<const TextureAsset> borg_alternate;
};
struct ModelMaterialBaselineData {
    std::vector<ModelMeshMaterialBaseline> meshes;
};
struct ModelMaterialBaseline {
    std::shared_ptr<const ModelAsset> model;
    std::shared_ptr<const AuthoredPoseData> authored;
    std::shared_ptr<const ModelNodeTemplateData> nodes;
    std::shared_ptr<const ModelMaterialBaselineData> data;
};
// Completed-load boundary only, before native caller resumes. Caller establishes
// source ownership and timing. Two complete checked passes; no native calls or
// reads of mutable flags/transforms. Failure preserves output. This does not
// approve effective materials, class overlays, group mapping or caster coverage.
bool capture_model_material_baseline(const ModelNodeTemplate&,
    MaterialBaselineReadProfile,void* read_context,Read,
    void* texture_context,ResolveTextureAsset,
    std::shared_ptr<const ModelMaterialBaseline>& output,std::string& error) noexcept;
} // namespace a2fo::native11
