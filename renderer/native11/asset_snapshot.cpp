#include "asset_snapshot.hpp"
#include <cstring>
#include <limits>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_model_bytes = 64ull * 1024 * 1024;
constexpr std::size_t max_total_bytes = 128ull * 1024 * 1024;
bool printable(const char* data, std::size_t size) noexcept {
    if (!size) return false;
    for (std::size_t i=0; i<size; ++i)
        if (static_cast<unsigned char>(data[i]) < 0x20 ||
            static_cast<unsigned char>(data[i]) > 0x7e) return false;
    return true;
}
bool name_length(const char* data, std::size_t capacity, std::size_t& length) noexcept {
    const auto* end = static_cast<const char*>(std::memchr(data, 0, capacity));
    if (!end) return false;
    length = static_cast<std::size_t>(end-data);
    return printable(data, length);
}
void failure(std::string& error, const char* message) noexcept {
    try { error = message; } catch (...) { error.clear(); }
}
}
std::shared_ptr<const ModelAsset> BoundCapture::model_asset(const std::string& id) const noexcept {
    const auto found = models_.find(id);
    return found == models_.end() ? nullptr : found->second;
}
std::int32_t A2FO_N11_CALL BoundCapture::read_asset(void* context, std::uint32_t kind,
    const std::uint8_t* identifier, std::uint32_t length, std::uint32_t max_bytes,
    A2FO_N11_BytesSink sink, void* sink_context) noexcept {
    if (!context || !identifier || !length || !sink ||
        (kind!=1&&kind!=2) || length>(kind==1?259u:1024u)) return 0;
    try {
        const auto& capture = *static_cast<const BoundCapture*>(context);
        const std::string id(reinterpret_cast<const char*>(identifier), length);
        const std::vector<std::uint8_t>* bytes = nullptr;
        if (kind == 1) {
            const auto found = capture.models_.find(id);
            if (found != capture.models_.end()) bytes = &found->second->bytes;
        } else if (kind == 2) {
            const auto found = capture.textures_.find(id);
            if (found != capture.textures_.end()) bytes = found->second.get();
        }
        if (!bytes || bytes->empty() || bytes->size() > max_bytes ||
            bytes->size() > std::numeric_limits<std::uint32_t>::max()) return 0;
        return sink(sink_context, bytes->data(), static_cast<std::uint32_t>(bytes->size())) == 1 ? 1 : 0;
    } catch (...) { return 0; }
}
bool build_bound_capture(const Frame& input, std::uint64_t source_epoch,
    ResolveSnapshotModel resolve, void* resolve_context,
    const std::vector<SnapshotTexture>& textures,
    std::shared_ptr<const BoundCapture>& output, std::string& error) noexcept {
    if (!source_epoch || !resolve || input.object_count > capacity || textures.size() > 256) {
        failure(error, "invalid snapshot bounds or source"); return false;
    }
    try {
        auto pending = std::shared_ptr<BoundCapture>(new BoundCapture);
        pending->frame_ = input;
        pending->source_epoch_ = source_epoch;
        std::map<std::pair<std::uint32_t, std::uint64_t>, std::shared_ptr<const ModelAsset>> resolved;
        std::map<std::uint64_t, std::uint32_t> serial_databases;
        std::size_t total_bytes = 0;
        for (std::size_t i=0; i<input.object_count; ++i) {
            const Object& object = input.objects[i];
            std::size_t name_size = 0;
            if (object.model_identity != ModelIdentity::plain_database_name ||
                (object.placement_identity != PlacementIdentity::authored_placement &&
                 object.placement_identity != PlacementIdentity::craft_runtime_scale) ||
                !object.database || !object.model_serial ||
                !name_length(object.model_name, sizeof(object.model_name), name_size)) {
                failure(error, "invalid captured model identity"); return false;
            }
            const auto serial = serial_databases.emplace(object.model_serial, object.database);
            if (!serial.second && serial.first->second != object.database) {
                failure(error, "model serial shared across databases"); return false;
            }
            const auto key = std::make_pair(object.database, object.model_serial);
            auto found = resolved.find(key);
            if (found == resolved.end()) {
                auto asset = resolve(resolve_context, object.database, object.model_serial);
                std::size_t asset_name_size = 0;
                if (!asset || asset->info.database != object.database ||
                    asset->info.serial != object.model_serial ||
                    !name_length(asset->info.name, sizeof(asset->info.name), asset_name_size) ||
                    asset_name_size != name_size ||
                    std::memcmp(asset->info.name, object.model_name, name_size) != 0 ||
                    asset->bytes.empty() || asset->bytes.size() > max_model_bytes ||
                    asset->info.byte_count != asset->bytes.size() ||
                    asset->bytes.size() > max_total_bytes - total_bytes) {
                    failure(error, "invalid resolved model asset"); return false;
                }
                total_bytes += asset->bytes.size();
                found = resolved.emplace(key, std::move(asset)).first;
            } else if (std::memcmp(found->second->info.name, object.model_name, name_size) != 0 ||
                       found->second->info.name[name_size] != 0) {
                failure(error, "conflicting captured model name"); return false;
            }
            const std::string id = "n11:model:" + std::to_string(object.model_serial);
            if (id.size() >= sizeof(object.model_name)) {
                failure(error, "opaque model identifier too long"); return false;
            }
            pending->models_.emplace(id, found->second);
            auto& destination = pending->frame_.objects[i];
            std::memset(destination.model_name, 0, sizeof(destination.model_name));
            std::memcpy(destination.model_name, id.data(), id.size());
        }
        for (const auto& texture : textures) {
            if (texture.identifier.size() < 1 || texture.identifier.size() > 1024 ||
                !printable(texture.identifier.data(), texture.identifier.size()) ||
                !texture.bytes || texture.bytes->empty() ||
                texture.bytes->size() > max_total_bytes - total_bytes ||
                pending->textures_.find(texture.identifier) != pending->textures_.end()) {
                failure(error, "invalid snapshot texture"); return false;
            }
            total_bytes += texture.bytes->size();
            pending->textures_.emplace(texture.identifier, texture.bytes);
        }
        output = std::move(pending);
        error.clear();
        return true;
    } catch (...) { failure(error, "snapshot allocation or resolver failure"); return false; }
}
} // namespace a2fo::native11
