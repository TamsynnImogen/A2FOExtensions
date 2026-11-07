#pragma once
#include "asset_snapshot.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Storm3D texture +0x18 metadata, distinct from renderer protocol flags.
// FleetOpsHook 3fd18e...4c11 DDS parameter reader ORs bit 2 after detecting
// alpha-capable pixel formats (RVA 0x1eff42..0x1eff6c). Encoded DDS/TGA pixels
// retain that alpha; the SOD material determines opaque/cutout/blend behavior.
inline constexpr std::uint32_t texture_has_alpha = 2;

enum class TextureSourceKind { resolved_file, opened_stream };

struct TextureAssetInfo {
    std::uint64_t serial = 0;
    std::uint32_t texture = 0, flags = 0;
    bool colour_key = false;
    std::string logical_name, resolved_path;
    TextureSourceKind source_kind = TextureSourceKind::resolved_file;
    std::string requested_path;
};

struct TextureAsset {
    std::uint64_t serial = 0;
    std::uint32_t texture = 0, flags = 0;
    bool colour_key = false;
    std::string logical_name, resolved_path;
    TextureSourceKind source_kind = TextureSourceKind::resolved_file;
    std::string requested_path;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};

// Externally synchronized. Returned shared references survive replacement and clear.
class ResolvedTextureStore {
public:
    explicit ResolvedTextureStore(std::uint64_t max_bytes = 128ull*1024*1024,
                                  std::size_t max_records = 2048) noexcept :
        max_bytes_(max_bytes), max_records_(max_records) {}
    bool publish(std::uint32_t texture, std::string logical_name, std::string resolved_path,
                 std::uint32_t flags, bool colour_key, std::vector<std::uint8_t>&& bytes) noexcept;
    bool publish_stream(std::uint32_t texture, std::string logical_name,
                        std::string requested_path, std::uint32_t flags, bool colour_key,
                        std::vector<std::uint8_t>&& bytes) noexcept;
    void invalidate(std::uint32_t texture) noexcept;
    void clear() noexcept;
    void clear_source(TextureSourceKind) noexcept;
    std::shared_ptr<const TextureAsset> current(std::uint32_t texture) const noexcept;
    std::shared_ptr<const TextureAsset> find(std::uint32_t texture,
                                             std::uint64_t serial) const noexcept;
    bool info(std::uint32_t texture, TextureAssetInfo& output) const noexcept;
    bool collect(std::vector<SnapshotTexture>& output, std::string& error) const noexcept;
    bool collect_required(const std::vector<std::string>& exact_names,
                          std::vector<SnapshotTexture>& output,
                          std::string& error) const noexcept;
    std::uint64_t generation() const noexcept { return generation_; }
    std::size_t size() const noexcept { return records_.size(); }
    std::uint64_t bytes() const noexcept { return bytes_; }

private:
    bool publish_impl(std::uint32_t texture, std::string logical_name,
                      std::string resolved_path, TextureSourceKind source_kind,
                      std::string requested_path, std::uint32_t flags, bool colour_key,
                      std::vector<std::uint8_t>&& bytes) noexcept;
    void retire(std::uint32_t texture) noexcept;
    void advance() noexcept;
    const std::uint64_t max_bytes_;
    const std::size_t max_records_;
    std::map<std::uint32_t, std::shared_ptr<const TextureAsset>> records_;
    std::uint64_t bytes_ = 0, last_serial_ = 0, generation_ = 1;
};

} // namespace a2fo::native11
