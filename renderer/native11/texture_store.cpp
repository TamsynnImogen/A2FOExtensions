#include "texture_store.hpp"
#include <limits>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_texture_bytes = 64ull*1024*1024;
bool printable(const std::string& name) noexcept {
    if (name.empty() || name.size() > 1024) return false;
    for (unsigned char c : name) if (c < 0x20 || c > 0x7e) return false;
    return true;
}
bool valid_path(const std::string& path) noexcept {
    return !path.empty() && path.size() <= 32767 &&
        path.find('\0') == std::string::npos;
}
bool valid_stream_path(const std::string& path) noexcept {
    if (path.empty() || path.size() > 1023) return false;
    for (unsigned char c : path) if (c < 0x20 || c > 0x7e) return false;
    return true;
}
void failure(std::string& error, const char* message) noexcept {
    try { error = message; } catch (...) { error.clear(); }
}
}

void ResolvedTextureStore::advance() noexcept {
    if (generation_ != std::numeric_limits<std::uint64_t>::max()) ++generation_;
}
void ResolvedTextureStore::retire(std::uint32_t texture) noexcept {
    const auto found = records_.find(texture);
    if (found == records_.end()) return;
    bytes_ -= found->second->bytes->size();
    records_.erase(found);
}
void ResolvedTextureStore::invalidate(std::uint32_t texture) noexcept {
    advance();
    retire(texture);
}
void ResolvedTextureStore::clear() noexcept {
    advance();
    records_.clear();
    bytes_ = 0;
}
void ResolvedTextureStore::clear_source(TextureSourceKind kind) noexcept {
    bool changed=false;
    for(auto it=records_.begin();it!=records_.end();){
        if(it->second->source_kind==kind){bytes_-=it->second->bytes->size();it=records_.erase(it);changed=true;}
        else ++it;
    }
    if(changed)advance();
}
bool ResolvedTextureStore::publish(std::uint32_t texture, std::string logical_name,
    std::string resolved_path, std::uint32_t flags, bool colour_key,
    std::vector<std::uint8_t>&& bytes) noexcept {
    return publish_impl(texture, std::move(logical_name), std::move(resolved_path),
        TextureSourceKind::resolved_file, {}, flags, colour_key, std::move(bytes));
}
bool ResolvedTextureStore::publish_stream(std::uint32_t texture, std::string logical_name,
    std::string requested_path, std::uint32_t flags, bool colour_key,
    std::vector<std::uint8_t>&& bytes) noexcept {
    return publish_impl(texture, std::move(logical_name), {},
        TextureSourceKind::opened_stream, std::move(requested_path),
        flags, colour_key, std::move(bytes));
}
bool ResolvedTextureStore::publish_impl(std::uint32_t texture, std::string logical_name,
    std::string resolved_path, TextureSourceKind source_kind, std::string requested_path,
    std::uint32_t flags, bool colour_key, std::vector<std::uint8_t>&& bytes) noexcept {
    const bool exhausted = generation_ == std::numeric_limits<std::uint64_t>::max();
    advance();
    retire(texture); // A rejected replacement still retires the prior address binding.
    const auto count = bytes.size();
    if (exhausted || !texture || !printable(logical_name) ||
        (source_kind == TextureSourceKind::resolved_file ? !valid_path(resolved_path) :
         !valid_stream_path(requested_path)) ||
        !count || count > max_texture_bytes || records_.size() >= max_records_ ||
        count > max_bytes_ || bytes_ > max_bytes_ - count ||
        last_serial_ == std::numeric_limits<std::uint64_t>::max()) return false;
    try {
        auto asset = std::make_shared<TextureAsset>();
        asset->serial = last_serial_ + 1;
        asset->texture = texture;
        asset->flags = flags;
        asset->colour_key = colour_key;
        asset->logical_name = std::move(logical_name);
        asset->resolved_path = std::move(resolved_path);
        asset->source_kind = source_kind;
        asset->requested_path = std::move(requested_path);
        asset->bytes = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
        records_.emplace(texture, std::move(asset));
        bytes_ += count;
        ++last_serial_;
        return true;
    } catch (...) { return false; }
}
std::shared_ptr<const TextureAsset> ResolvedTextureStore::current(std::uint32_t texture) const noexcept {
    if(generation_==std::numeric_limits<std::uint64_t>::max())return nullptr;
    const auto found = records_.find(texture);
    return found == records_.end() ? nullptr : found->second;
}
std::shared_ptr<const TextureAsset> ResolvedTextureStore::find(std::uint32_t texture,
    std::uint64_t serial) const noexcept {
    auto asset = current(texture);
    return asset && asset->serial == serial ? asset : nullptr;
}
bool ResolvedTextureStore::info(std::uint32_t texture, TextureAssetInfo& output) const noexcept {
    auto asset = current(texture);
    if (!asset) return false;
    try {
        TextureAssetInfo pending;
        pending.serial = asset->serial;
        pending.texture = asset->texture;
        pending.flags = asset->flags;
        pending.colour_key = asset->colour_key;
        pending.logical_name = asset->logical_name;
        pending.resolved_path = asset->resolved_path;
        pending.source_kind = asset->source_kind;
        pending.requested_path = asset->requested_path;
        output = std::move(pending);
        return true;
    } catch (...) { return false; }
}
bool ResolvedTextureStore::collect(std::vector<SnapshotTexture>& output,
    std::string& error) const noexcept {
    if(generation_==std::numeric_limits<std::uint64_t>::max()){
        failure(error,"texture generation exhausted");return false;
    }
    try {
        std::map<std::string, std::shared_ptr<const TextureAsset>> unique;
        for (const auto& entry : records_) {
            const auto& asset = entry.second;
            if (asset->colour_key || (asset->flags & 4u)) {
                failure(error, "colour-key or surface texture unsupported in static preview");
                return false;
            }
            auto inserted = unique.emplace(asset->logical_name, asset);
            if (!inserted.second) {
                const auto& previous = inserted.first->second;
                if (previous->flags != asset->flags || previous->colour_key != asset->colour_key ||
                    *previous->bytes != *asset->bytes) {
                    failure(error, "ambiguous logical texture name"); return false;
                }
            }
            if (unique.size() > 256) {
                failure(error, "too many unique textures"); return false;
            }
        }
        std::vector<SnapshotTexture> pending;
        pending.reserve(unique.size());
        for (const auto& entry : unique)
            pending.push_back({entry.first, entry.second->bytes});
        output.swap(pending);
        error.clear();
        return true;
    } catch (...) { failure(error, "texture collection allocation failure"); return false; }
}
bool ResolvedTextureStore::collect_required(const std::vector<std::string>& exact_names,
    std::vector<SnapshotTexture>& output, std::string& error) const noexcept {
    if(generation_==std::numeric_limits<std::uint64_t>::max()){
        failure(error,"texture generation exhausted");return false;
    }
    try {
        std::map<std::string, bool> requested;
        for (const auto& name : exact_names) {
            if (!printable(name)) {
                failure(error, "invalid required texture name"); return false;
            }
            requested.emplace(name, true);
            if (requested.size() > 256) {
                failure(error, "too many required textures"); return false;
            }
        }

        if(requested.empty()){
            std::vector<SnapshotTexture> empty;output.swap(empty);error.clear();return true;
        }
        std::map<std::string, std::shared_ptr<const TextureAsset>> found;
        for (const auto& entry : records_) {
            const auto& asset = entry.second;
            auto required = requested.find(asset->logical_name);
            if (required == requested.end()) continue;
            if (asset->colour_key || (asset->flags & 4u)) {
                failure(error, "colour-key or surface texture unsupported in static preview");
                return false;
            }
            auto inserted = found.emplace(asset->logical_name, asset);
            if (!inserted.second) {
                const auto& previous = inserted.first->second;
                if (previous->flags != asset->flags || previous->colour_key != asset->colour_key ||
                    *previous->bytes != *asset->bytes) {
                    failure(error, "ambiguous logical texture name"); return false;
                }
            }
        }
        for (const auto& name : requested) {
            if (found.find(name.first) == found.end()) {
                failure(error, "required texture missing"); return false;
            }
        }
        std::vector<SnapshotTexture> pending;
        pending.reserve(found.size());
        for (const auto& entry : found)
            pending.push_back({entry.first, entry.second->bytes});
        output.swap(pending);
        error.clear();
        return true;
    } catch (...) { failure(error, "texture collection allocation failure"); return false; }
}
} // namespace a2fo::native11
