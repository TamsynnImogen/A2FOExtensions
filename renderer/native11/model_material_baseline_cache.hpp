#pragma once
#include "model_material_baseline.hpp"
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
namespace a2fo::native11 {
// Externally synchronized. Weak model identity and strong copied metadata;
// returned wrappers pin the exact model and texture versions.
class ModelMaterialBaselineCache final {
public:
    explicit ModelMaterialBaselineCache(std::size_t max_entries=64,
        std::size_t max_bytes=128u*1024u*1024u) noexcept;
    bool publish(std::shared_ptr<const ModelMaterialBaseline>,std::string& error) noexcept;
    bool find(std::shared_ptr<const ModelAsset>,
        std::shared_ptr<const ModelMaterialBaseline>& output,std::string& error) const noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept;
    std::size_t bytes() const noexcept;
private:
    struct Entry {
        std::uint64_t serial=0;
        std::weak_ptr<const ModelAsset> model;
        std::shared_ptr<const AuthoredPoseData> authored;
        std::shared_ptr<const ModelNodeTemplateData> nodes;
        std::shared_ptr<const ModelMaterialBaselineData> data;
        std::size_t metadata=0;
    };
    void prune() const noexcept;
    const std::size_t max_entries_,max_bytes_;
    mutable std::list<Entry> entries_;
    mutable std::size_t bytes_=0;
};
} // namespace a2fo::native11
