#pragma once
#include "model_node_template.hpp"
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>

namespace a2fo::native11 {
// Externally synchronized. Entries keep weak model ownership and strong
// immutable metadata; returned snapshots pin all three owners. No live entry
// is evicted or replaced with a different authored/data owner tuple.
class ModelNodeTemplateCache final {
public:
    explicit ModelNodeTemplateCache(std::size_t max_entries=64,
        std::size_t max_bytes=64u*1024u*1024u) noexcept;
    bool publish(std::shared_ptr<const ModelNodeTemplate>,std::string& error) noexcept;
    bool find(std::shared_ptr<const ModelAsset>,
        std::shared_ptr<const ModelNodeTemplate>& out,std::string& error) const noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept;
    std::size_t bytes() const noexcept;
private:
    struct Entry {
        std::uint64_t serial=0;
        std::weak_ptr<const ModelAsset> model;
        std::shared_ptr<const AuthoredPoseData> authored;
        std::shared_ptr<const ModelNodeTemplateData> data;
        std::size_t bytes=0;
    };
    void prune() const noexcept;
    const std::size_t max_entries_,max_bytes_;
    mutable std::list<Entry> entries_;
    mutable std::size_t bytes_=0;
};
} // namespace a2fo::native11
