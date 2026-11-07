#pragma once
#include "model_sources.h"
#include "model_store.hpp"
#include "scene_capture.hpp"
#include "texture_store.hpp"
#include "part_state.hpp"
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
using InspectSod = int(*)(const std::uint8_t*,std::uint32_t,A2FO_N11_DetailedPartSink,void*);
struct MaterialPartSource {
    std::string node_name, authored_texture;
    std::uint32_t node_index = 0, group_index = 0;
    std::uint32_t source_triangles=0, source_flags=0;
};
// Owners are treated as immutable after publication; callers must not retain
// mutable aliases to the model or any resolved texture bytes.
struct ModelMaterialSource {
    std::shared_ptr<const ModelAsset> model;
    std::vector<MaterialPartSource> parts;
};
bool inspect_model_materials(const std::shared_ptr<const ModelAsset>& asset, InspectSod parser,
    std::shared_ptr<const ModelMaterialSource>& out, std::string& error) noexcept;

// Addresses are supplied only after the caller validates the engine profile.
struct MaterialReadProfile {
    std::uint32_t instance_vtable = 0, database_vtable = 0, mesh_type_function = 0;
};
struct MaterialBinding {
    std::string authored_texture;
    std::shared_ptr<const TextureAsset> texture;
    std::uint32_t node_flags = 0;
    std::uint32_t node_index = UINT32_MAX, group_index = UINT32_MAX;
    std::uint32_t native_node = 0; // telemetry identity, never dereferenced by consumers
    std::uint32_t source_triangles=0, source_flags=0; // immutable authored group policy
    bool node_state_valid=false;
    PartState node_state{};
};
struct MaterialObservation {
    std::uint32_t instance = 0, database = 0;
    std::uint64_t model_serial = 0;
    std::uint32_t native_x87_control = 0; // 0 means unknown legacy source
    std::shared_ptr<const ModelAsset> model;
    std::vector<MaterialBinding> parts;
};
using ResolveTextureAsset = std::shared_ptr<const TextureAsset>(*)(void*,std::uint32_t);
// posed_states, when supplied, must come from capture_node_states for this exact
// object/source at the same post-animation draw boundary. Hidden nodes retain
// their authoritative state but do not demand resident texture bytes.
bool capture_material_observation(const Object& object, const ModelMaterialSource& source,
    MaterialReadProfile profile, void* read_context, Read read,
    void* texture_context, ResolveTextureAsset resolve,
    std::shared_ptr<const MaterialObservation>& out, std::string& error,
    const std::vector<PartState>* posed_states=nullptr) noexcept;
} // namespace a2fo::native11
