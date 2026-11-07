#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace a2fo::m3d {

struct RuntimeHierarchyBinding {
    std::string node_name;
    std::string parent_name;
    std::string authored_name;
};

struct RuntimeTextureBinding {
    std::string mesh_name;
    std::string diffuse_texture;
    std::string bump_texture;
    std::array<float, 3> local_bounds_min{};
    std::array<float, 3> local_bounds_max{};
    bool has_local_bounds = false;
};

struct ConversionStats {
    float source_version = 0.0f;
    std::size_t materials = 0;
    std::size_t source_nodes = 0;
    std::size_t output_nodes = 0;
    std::size_t meshes = 0;
    std::size_t groups = 0;
    std::size_t triangles = 0;
    std::size_t animations = 0;
    std::size_t texture_references = 0;
    std::size_t source_roots = 0;
    bool synthesized_root = false;
    std::size_t reordered_nodes = 0;
    std::size_t disambiguated_node_names = 0;
    std::size_t damage_emitters_adapted = 0;
    std::size_t sod_materials_end = 0;
    std::size_t sod_nodes_end = 0;
    std::size_t sod_animations_end = 0;
    std::size_t sod_references_end = 0;
    bool legacy_sod_passthrough = false;
    std::vector<std::string> warnings;
    std::vector<RuntimeHierarchyBinding> hierarchy_bindings;
    std::vector<RuntimeTextureBinding> texture_bindings;
};

bool convert_bytes_to_sod(const std::vector<std::uint8_t>& input,
                          std::vector<std::uint8_t>& output,
                          ConversionStats& stats,
                          std::string& error);

bool convert_file_to_sod(const std::string& source_path,
                         const std::string& destination_path,
                         ConversionStats& stats,
                         std::string& error);

}  // namespace a2fo::m3d
