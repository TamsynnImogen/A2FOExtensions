#pragma once
#include "model_node_template.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct ModelLodLevel {
    std::uint32_t node_index=UINT32_MAX,face_count=0;
};
struct ModelLodControl {
    std::uint32_t node_index=UINT32_MAX;
    std::array<float,3> centre{};
    float radius=0;
    std::vector<ModelLodLevel> levels; // native list order, never sorted
};
struct ModelLodTemplateData {
    std::vector<ModelLodControl> controls;
};
// Reads only verified LOD list and bounds fields from the pinned node template.
// Caller retains the baseline owners and supplies a nonthrowing checked reader.
// No mutable current selection, flags, matrices or native functions are read.
// Failure leaves output unchanged; this does not establish LOD/class authority.
bool capture_model_lod_template(const ModelNodeTemplate&,std::uint32_t lod_type_function,
    void* context,Read,std::shared_ptr<const ModelLodTemplateData>& output,
    std::string& error) noexcept;
} // namespace a2fo::native11
