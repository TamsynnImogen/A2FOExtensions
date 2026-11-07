#pragma once

#include "model_store.hpp"
#include "assets.h"
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {

struct ModelDependencies {
    std::vector<std::string> textures;
    std::uint32_t part_count = 0;
};

using ParseSod = decltype(&a2fo_native11_parse_sod);

// Externally synchronized. Entries retain only a weak reference to their model.
class ModelDependencyCache final {
public:
    explicit ModelDependencyCache(std::size_t max_entries = 2048,
                                  std::size_t max_identifier_bytes = 8u * 1024u * 1024u) noexcept;
    bool get(const std::shared_ptr<const ModelAsset>& asset, ParseSod parser,
             std::shared_ptr<const ModelDependencies>& output, std::string& error) noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept { return entries_.size(); }
    std::size_t bytes() const noexcept { return identifier_bytes_; }

private:
    struct Entry {
        std::uint64_t serial = 0;
        ParseSod parser = nullptr;
        std::weak_ptr<const ModelAsset> owner;
        std::shared_ptr<const ModelDependencies> dependencies;
        std::size_t identifier_bytes = 0;
    };
    const std::size_t max_entries_;
    const std::size_t max_identifier_bytes_;
    std::list<Entry> entries_;
    std::size_t identifier_bytes_ = 0;
};

} // namespace a2fo::native11
