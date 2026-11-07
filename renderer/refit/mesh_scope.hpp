#pragma once

#include "../native11/material_observation.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace a2fo::refit {
struct MeshSelection2 {
    std::shared_ptr<const native11::MaterialObservation> observation;
    std::uint32_t triangles=0,node_index=0,group_index=0;
};

// Header-only admission for the initial native mesh path. The owned SOD
// parser must still validate the complete file before any use.
bool mesh_model_format_supported2(const native11::ModelAsset&) noexcept;

// Select one visible authored group for an observational native mesh scope.
// The result owns a copy of the metadata and selected binding; model and
// texture owners remain shared immutable snapshots. No engine memory is read.
bool select_opaque_mesh2(const native11::MaterialObservation& source,
                         std::uint32_t native_node,std::uint32_t native_group_count,
                         MeshSelection2& output,std::string& error) noexcept;

// Two-group admission (opt-in [Refit]TwoGroupMeshes). The native legacy body
// draws each of a node's 0x1c-byte group records as one checked opaque draw,
// in group order (live-observed on fed_saber, base + team_ material). This
// selects exactly one authored group of such a node; every other gate equals
// select_opaque_mesh2. Exactly one binding per (node, group) must exist and
// the node must expose bindings for every native group. Transactional output.
bool select_opaque_group2(const native11::MaterialObservation& source,
                          std::uint32_t native_node,std::uint32_t native_group_count,
                          std::uint32_t group_index,
                          MeshSelection2& output,std::string& error) noexcept;
} // namespace a2fo::refit
