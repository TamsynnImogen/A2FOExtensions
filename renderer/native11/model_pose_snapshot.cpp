#include "model_pose_snapshot.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t kMaxModelBytes = 64u * 1024u * 1024u;
constexpr std::size_t kMaxCopiedBytes = 64u * 1024u * 1024u;
constexpr std::uint32_t kMaxNodes = 4096, kMaxChannels = 256;
constexpr std::size_t kMaxKeyFloats = 1048576;

bool add_bytes(std::size_t& total, std::size_t amount) noexcept {
    if (amount > kMaxCopiedBytes - total) return false;
    total += amount;
    return true;
}

struct InspectContext {
    AuthoredPoseData data;
    std::size_t copied_bytes = sizeof(AuthoredPoseData);
    unsigned calls = 0;
    bool accepted = false;
};

int collect_pose(void* raw, const A2FO_N11_PoseMetadata* metadata) noexcept {
    auto* context = static_cast<InspectContext*>(raw);
    if (!context || ++context->calls != 1 || !metadata) return 0;
    try {
        if (metadata->node_count > kMaxNodes || metadata->channel_count > kMaxChannels ||
            (metadata->node_count && !metadata->nodes) ||
            (metadata->channel_count && !metadata->channels)) return 0;
        auto& data = context->data;
        data.reference_count = metadata->reference_count;
        if (!add_bytes(context->copied_bytes,
                       std::size_t(metadata->node_count) * sizeof(AuthoredPoseNode)) ||
            !add_bytes(context->copied_bytes,
                       std::size_t(metadata->channel_count) * sizeof(AuthoredPoseChannel))) return 0;
        data.nodes.reserve(metadata->node_count);
        data.channels.reserve(metadata->channel_count);
        for (std::uint32_t i = 0; i < metadata->node_count; ++i) {
            const auto& source = metadata->nodes[i];
            if (source.index != i || !source.name || source.name_length == 0 ||
                source.name_length > 259 ||
                (source.parent != UINT32_MAX &&
                 (source.parent >= metadata->node_count || source.parent == i)) ||
                (source.kind != 0 && source.kind != 1 && source.kind != 2 &&
                 source.kind != 3 && source.kind != 12) ||
                !add_bytes(context->copied_bytes, source.name_length)) return 0;
            AuthoredPoseNode node;
            node.parent = source.parent;
            node.kind = source.kind;
            node.group_count = source.group_count;
            node.name.reserve(source.name_length);
            for (std::uint32_t c = 0; c < source.name_length; ++c) {
                const auto ch = source.name[c];
                if (ch < 0x20 || ch > 0x7e) return 0;
                node.name.push_back(static_cast<char>(ch));
            }
            for (std::size_t j = 0; j < 12; ++j) {
                if (!std::isfinite(source.local_matrix34[j])) return 0;
                node.local[j] = source.local_matrix34[j];
            }
            data.nodes.push_back(std::move(node));
        }
        for (std::size_t i = 0; i < data.nodes.size(); ++i) {
            auto parent = data.nodes[i].parent;
            std::size_t steps = 0;
            while (parent != UINT32_MAX) {
                if (parent >= data.nodes.size() || parent == i || steps++ >= data.nodes.size())
                    return 0;
                parent = data.nodes[parent].parent;
            }
        }
        std::size_t total_floats = 0;
        for (std::uint32_t i = 0; i < metadata->channel_count; ++i) {
            const auto& source = metadata->channels[i];
            if (source.index != i || source.node_index >= metadata->node_count ||
                !std::isfinite(source.period) || source.period <= 0 ||
                source.key_count == 0 || source.key_count > 65535 || !source.keys ||
                source.key_width != (source.channel_type == 5 ? 1u : 12u)) return 0;
            const auto count = std::size_t(source.key_count) * source.key_width;
            if (count > kMaxKeyFloats - total_floats ||
                !add_bytes(context->copied_bytes, count * sizeof(float))) return 0;
            total_floats += count;
            AuthoredPoseChannel channel;
            channel.node_index = source.node_index;
            channel.channel_type = source.channel_type;
            channel.period = source.period;
            channel.key_width = source.key_width;
            channel.keys.reserve(count);
            for (std::size_t j = 0; j < count; ++j) {
                if (!std::isfinite(source.keys[j])) return 0;
                channel.keys.push_back(source.keys[j]);
            }
            data.channels.push_back(std::move(channel));
        }
        context->accepted = true;
        return 1;
    } catch (...) { return 0; }
}
} // namespace

ModelPoseCache::ModelPoseCache(std::size_t max_entries,
                               std::size_t max_copied_bytes) noexcept
    : max_entries_(std::min<std::size_t>(max_entries, 64)),
      max_copied_bytes_(std::min<std::size_t>(max_copied_bytes, 128u * 1024u * 1024u)) {}

bool ModelPoseCache::inspect(const std::shared_ptr<const ModelAsset>& asset,
                             InspectSodPose parser, ModelPoseSnapshot& output,
                             std::string& error) noexcept {
    try {
        entries_.remove_if([this](const Entry& entry) {
            if (!entry.owner.expired()) return false;
            copied_bytes_ -= entry.copied_bytes;
            return true;
        });
        if (!asset || !parser) { error = "missing model asset or pose parser"; return false; }
        if (!asset->info.serial || asset->bytes.empty() ||
            asset->bytes.size() > kMaxModelBytes ||
            asset->bytes.size() != asset->info.byte_count) {
            error = "invalid model asset bytes or serial"; return false;
        }
        for (const auto& entry : entries_) {
            if (entry.serial != asset->info.serial) continue;
            const auto owner = entry.owner.lock();
            if (owner && owner.get() != asset.get()) {
                error = "model serial collides with a different live owner"; return false;
            }
            if (entry.parser == parser && owner) {
                output = ModelPoseSnapshot{asset, entry.pose};
                error.clear();
                return true;
            }
        }
        InspectContext context;
        int result = 0;
        try {
            result = parser(asset->bytes.data(), static_cast<std::uint32_t>(asset->bytes.size()),
                            &collect_pose, &context);
        } catch (...) { result = 0; }
        if (result != 1 || context.calls != 1 || !context.accepted) {
            error = "SOD pose inspection failed or returned invalid metadata"; return false;
        }
        if (context.copied_bytes > max_copied_bytes_ || entries_.size() >= max_entries_ ||
            copied_bytes_ > max_copied_bytes_ - context.copied_bytes) {
            error = "SOD pose cache budget exceeded"; return false;
        }
        auto pose = std::make_shared<const AuthoredPoseData>(std::move(context.data));
        Entry entry;
        entry.serial = asset->info.serial;
        entry.parser = parser;
        entry.owner = asset;
        entry.pose = pose;
        entry.copied_bytes = context.copied_bytes;
        entries_.push_back(std::move(entry));
        copied_bytes_ += context.copied_bytes;
        output = ModelPoseSnapshot{asset, std::move(pose)};
        error.clear();
        return true;
    } catch (...) {
        try { error = "allocation or pose parser exception"; } catch (...) {}
        return false;
    }
}

void ModelPoseCache::clear() noexcept {
    entries_.clear();
    copied_bytes_ = 0;
}

} // namespace a2fo::native11
