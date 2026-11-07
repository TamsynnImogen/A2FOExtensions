#pragma once
#include "model_pose_snapshot.hpp"
#include "scene_capture.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct ModelLodTemplateData;
struct NodeTemplateReadProfile {
    std::uint32_t db_vtable=0;
    std::uint32_t generic_type=0,lod_type=0,mesh_type=0;
    std::uint32_t light_type=0,sprite_type=0,emitter_type=0;
};
struct ModelNodeTemplateNode {
    std::uint32_t address=0,vtable=0,type_function=0,name_pointer=0;
    std::uint32_t parent_address=0,child_address=0,sibling_address=0;
    std::uint32_t parent=UINT32_MAX,first_child=UINT32_MAX,next_sibling=UINT32_MAX;
    std::uint32_t flags=0;
    float scale=0;
    std::array<float,12> local{}; // native Matrix34 order
    std::string name;
};
struct ModelNodeTemplateData {
    std::uint32_t root_address=0;
    std::uint32_t hardpoints=UINT32_MAX,damage=UINT32_MAX,lights=UINT32_MAX;
    std::shared_ptr<const ModelLodTemplateData> lods; // optional copied native LOD values
    // File-order indexing, matching authored metadata; addresses are telemetry.
    std::vector<ModelNodeTemplateNode> nodes;
};
struct ModelNodeTemplate {
    std::shared_ptr<const ModelAsset> model;
    std::shared_ptr<const AuthoredPoseData> authored;
    std::shared_ptr<const ModelNodeTemplateData> data;
};
// One-root, complete authored-node tree only. Caller invokes this after fresh
// ReadDatabase and before mutation. This read-only helper cannot establish
// time-of-capture, class, LOD, material or caster authority by itself.
// Failure leaves output untouched; no native functions are called.
bool capture_model_node_template(const ModelPoseSnapshot&,NodeTemplateReadProfile,
    void* context,Read,std::shared_ptr<const ModelNodeTemplate>& output,
    std::string& error) noexcept;
} // namespace a2fo::native11
