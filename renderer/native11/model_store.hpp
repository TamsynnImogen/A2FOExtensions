#pragma once
#include "opened_file.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace a2fo::native11 {

struct ModelAssetInfo {
    std::uint64_t serial = 0;
    std::uint32_t database = 0, byte_count = 0, parser_scalar_bits = 0;
    char name[260]{};
    std::uint32_t parser_native_x87_control = 0; // captured at immutable model load; zero is unknown
};
struct ModelAsset {
    ModelAssetInfo info;
    std::vector<std::uint8_t> bytes;
};

// Externally synchronized. Returned shared references outlive invalidation.
class ModelAssetStore {
public:
    explicit ModelAssetStore(std::uint64_t max_bytes = 128ull*1024*1024,
                             std::size_t max_records = 2048) noexcept :
        max_bytes_(max_bytes), max_records_(max_records) {}
    bool publish(std::uint32_t database, std::uint32_t parser_scalar_bits,
                 OpenedFile&& file, std::uint32_t native_x87_control = 0) noexcept;
    void invalidate(std::uint32_t database) noexcept;
    bool info(std::uint32_t database, ModelAssetInfo& output) const noexcept;
    std::shared_ptr<const ModelAsset> find(std::uint32_t database,
                                           std::uint64_t serial) const noexcept;
    void clear() noexcept;
    std::uint64_t bytes() const noexcept { return bytes_; }
    std::size_t size() const noexcept { return records_.size(); }

private:
    const std::uint64_t max_bytes_;
    const std::size_t max_records_;
    std::map<std::uint32_t, std::shared_ptr<const ModelAsset>> records_;
    std::uint64_t bytes_ = 0, last_serial_ = 0;
};

} // namespace a2fo::native11
