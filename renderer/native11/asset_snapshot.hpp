#pragma once
#include "model_store.hpp"
#include "scene_capture.hpp"
#include "../../sdk/include/a2fo_native11_api.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
using ResolveSnapshotModel = std::shared_ptr<const ModelAsset>(*)(void*, std::uint32_t, std::uint64_t);
struct SnapshotTexture {
    std::string identifier;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};

// Static-preview binding only. Callers must publish objects without mutable
// aliases and must not mutate them while a BoundCapture retains them.
// Raw parser scalar metadata is retained, never interpreted or approved here.
class BoundCapture final {
public:
    const Frame& frame() const noexcept { return frame_; }
    std::uint64_t source_epoch() const noexcept { return source_epoch_; }
    const std::map<std::string, std::shared_ptr<const ModelAsset>>& models() const noexcept { return models_; }
    // The copied Frame's model_name is a host opaque ID, not an engine filename.
    // Original database/name/scalar metadata remains available through this owner.
    std::shared_ptr<const ModelAsset> model_asset(const std::string& opaque_id) const noexcept;
    static std::int32_t A2FO_N11_CALL read_asset(void* context, std::uint32_t kind,
        const std::uint8_t* identifier, std::uint32_t length, std::uint32_t max_bytes,
        A2FO_N11_BytesSink sink, void* sink_context) noexcept;
private:
    friend bool build_bound_capture(const Frame&, std::uint64_t, ResolveSnapshotModel, void*,
        const std::vector<SnapshotTexture>&, std::shared_ptr<const BoundCapture>&, std::string&) noexcept;
    BoundCapture() = default;
    Frame frame_{};
    std::uint64_t source_epoch_ = 0;
    std::map<std::string, std::shared_ptr<const ModelAsset>> models_;
    std::map<std::string, std::shared_ptr<const std::vector<std::uint8_t>>> textures_;
};

// Transactional: on failure output is unchanged. Resolver runs once per
// distinct (database, serial). No engine address is dereferenced by this API.
bool build_bound_capture(const Frame& input, std::uint64_t source_epoch,
    ResolveSnapshotModel resolve, void* resolve_context,
    const std::vector<SnapshotTexture>& textures,
    std::shared_ptr<const BoundCapture>& output, std::string& error) noexcept;
} // namespace a2fo::native11
