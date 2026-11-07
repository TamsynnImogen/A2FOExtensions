#pragma once
#include "model_pose.h"
#include "model_store.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {

struct AuthoredPoseNode {
    std::string name;
    std::uint32_t parent = UINT32_MAX, kind = 0, group_count = 0;
    std::array<float, 12> local{}; // a2fmt native Matrix34 order
};
struct AuthoredPoseChannel {
    std::uint32_t node_index = 0, channel_type = 0;
    float period = 0;
    std::uint32_t key_width = 0;
    std::vector<float> keys;
};
struct AuthoredPoseData {
    std::vector<AuthoredPoseNode> nodes;
    std::vector<AuthoredPoseChannel> channels;
    std::uint32_t reference_count = 0; // preserved, not interpreted here
};
struct ModelPoseSnapshot {
    std::shared_ptr<const ModelAsset> model;
    std::shared_ptr<const AuthoredPoseData> pose;
};
using InspectSodPose = int (*)(const std::uint8_t*, std::uint32_t, A2FO_N11_PoseSink, void*);

// Externally synchronized. Cache entries retain only weak model ownership;
// snapshots pin both owners. No automatic eviction of live entries.
class ModelPoseCache final {
public:
    explicit ModelPoseCache(std::size_t max_entries = 64,
                            std::size_t max_copied_bytes = 128u * 1024u * 1024u) noexcept;
    bool inspect(const std::shared_ptr<const ModelAsset>& asset, InspectSodPose parser,
                 ModelPoseSnapshot& output, std::string& error) noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept { return entries_.size(); }
    std::size_t bytes() const noexcept { return copied_bytes_; }

private:
    struct Entry {
        std::uint64_t serial = 0;
        InspectSodPose parser = nullptr;
        std::weak_ptr<const ModelAsset> owner;
        std::shared_ptr<const AuthoredPoseData> pose;
        std::size_t copied_bytes = 0;
    };
    const std::size_t max_entries_, max_copied_bytes_;
    std::list<Entry> entries_;
    std::size_t copied_bytes_ = 0;
};

} // namespace a2fo::native11
