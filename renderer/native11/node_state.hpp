#pragma once
#include "material_observation.hpp"
#include "part_state.hpp"

namespace a2fo::native11 {
// Captures the post-animation node hierarchy in source.parts order. The caller
// supplies an already validated profile and a read-only engine memory callback.
bool capture_node_states(const Object& object, const ModelMaterialSource& source,
    MaterialReadProfile profile, void* read_context, Read read,
    std::vector<PartState>& out, std::string& error) noexcept;
}
