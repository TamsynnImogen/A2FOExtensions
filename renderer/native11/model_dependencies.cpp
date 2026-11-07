#include "model_dependencies.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <unordered_set>

namespace a2fo::native11 {
namespace {
constexpr std::size_t kMaxModelBytes = 64u * 1024u * 1024u;
constexpr std::uint32_t kMaxParts = 8192;
constexpr std::uint32_t kMaxElements = 1048576;
constexpr std::uint32_t kMaxTextureName = 1024;
constexpr std::size_t kMaxTextures = 256;
constexpr std::size_t kMaxTextureBytes = 256u * 1024u;

struct ParseContext {
    ModelDependencies value;
    std::unordered_set<std::string> seen;
    std::size_t identifier_bytes = 0;
    bool failed = false;
};

int collect_part(void* raw, const A2FO_N11_Part* part) noexcept {
    auto* context = static_cast<ParseContext*>(raw);
    if (!context || context->failed || !part) return 0;
    try {
        if (context->value.part_count >= kMaxParts || !part->vertices || !part->indices ||
            part->vertex_count == 0 || part->vertex_count > kMaxElements ||
            part->index_count == 0 || part->index_count > kMaxElements) {
            context->failed = true;
            return 0;
        }
        ++context->value.part_count;
        if (part->texture_name_length == 0) return 1;
        if (!part->texture_name || part->texture_name_length > kMaxTextureName) {
            context->failed = true;
            return 0;
        }
        std::string name;
        name.reserve(part->texture_name_length);
        for (std::uint32_t i = 0; i < part->texture_name_length; ++i) {
            const auto c = part->texture_name[i];
            if (c < 0x20 || c > 0x7e) { context->failed = true; return 0; }
            name.push_back(static_cast<char>(c));
        }
        if (context->seen.find(name) != context->seen.end()) return 1;
        if (context->value.textures.size() >= kMaxTextures ||
            name.size() > kMaxTextureBytes - context->identifier_bytes) {
            context->failed = true;
            return 0;
        }
        context->identifier_bytes += name.size();
        context->seen.insert(name);
        context->value.textures.push_back(std::move(name));
        return 1;
    } catch (...) {
        context->failed = true;
        return 0;
    }
}

}

ModelDependencyCache::ModelDependencyCache(std::size_t max_entries,
                                           std::size_t max_identifier_bytes) noexcept
    : max_entries_(max_entries), max_identifier_bytes_(max_identifier_bytes) {}

bool ModelDependencyCache::get(const std::shared_ptr<const ModelAsset>& asset, ParseSod parser,
    std::shared_ptr<const ModelDependencies>& output, std::string& error) noexcept {
    try {
        entries_.remove_if([this](const Entry& entry) {
            if (!entry.owner.expired()) return false;
            identifier_bytes_ = entry.identifier_bytes <= identifier_bytes_ ?
                identifier_bytes_ - entry.identifier_bytes : 0;
            return true;
        });
        if (!asset || !parser) { error = "missing model asset or SOD parser"; return false; }
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->serial != asset->info.serial || it->parser != parser) continue;
            auto owner = it->owner.lock();
            if (owner && owner.get() == asset.get()) {
                auto value = it->dependencies;
                entries_.splice(entries_.begin(), entries_, it);
                output = std::move(value);
                error.clear();
                return true;
            }
        }
        if (asset->bytes.empty() || asset->bytes.size() > kMaxModelBytes ||
            asset->bytes.size() != asset->info.byte_count) {
            error = "invalid SOD byte count";
            return false;
        }
        ParseContext context;
        if (parser(asset->bytes.data(), static_cast<std::uint32_t>(asset->bytes.size()),
                   &collect_part, &context) != 1 || context.failed || context.value.part_count == 0) {
            error = "SOD dependency parse failed or returned invalid parts";
            return false;
        }
        auto dependencies = std::make_shared<ModelDependencies>(std::move(context.value));
        if (max_entries_ != 0 && context.identifier_bytes <= max_identifier_bytes_) {
            while (!entries_.empty() && (entries_.size() >= max_entries_ ||
                identifier_bytes_ > max_identifier_bytes_ - context.identifier_bytes)) {
                identifier_bytes_ = entries_.back().identifier_bytes <= identifier_bytes_ ?
                    identifier_bytes_ - entries_.back().identifier_bytes : 0;
                entries_.pop_back();
            }
            Entry entry;
            entry.serial = asset->info.serial;
            entry.parser = parser;
            entry.owner = asset;
            entry.dependencies = dependencies;
            entry.identifier_bytes = context.identifier_bytes;
            entries_.push_front(std::move(entry));
            identifier_bytes_ += context.identifier_bytes;
        }
        output = std::move(dependencies);
        error.clear();
        return true;
    } catch (...) {
        try { error = "allocation or parser exception"; } catch (...) {}
        return false;
    }
}

void ModelDependencyCache::clear() noexcept {
    entries_.clear();
    identifier_bytes_ = 0;
}

} // namespace a2fo::native11
