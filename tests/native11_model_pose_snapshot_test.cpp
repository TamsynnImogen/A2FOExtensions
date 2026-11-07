#include "../renderer/native11/model_pose_snapshot.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace a2fo::native11;
namespace {
constexpr float identity[12] = {1,0,0,0,1,0,0,0,1,0,0,0};
int calls = 0;
int mode = 0;
std::uint8_t mutable_name[] = {'c','h','i','l','d'};
float mutable_keys[] = {2.0f};

int parser(const std::uint8_t*, std::uint32_t, A2FO_N11_PoseSink sink, void* context) {
    ++calls;
    A2FO_N11_PoseNode nodes[2]{};
    nodes[0].index = 0; nodes[0].parent = 1; nodes[0].kind = 1;
    nodes[0].group_count = 2; nodes[0].name = mutable_name; nodes[0].name_length = 5;
    nodes[1].index = 1; nodes[1].parent = UINT32_MAX; nodes[1].kind = 0;
    const std::uint8_t root[] = {'r','o','o','t'};
    nodes[1].name = root; nodes[1].name_length = 4;
    std::memcpy(nodes[0].local_matrix34, identity, sizeof(identity));
    std::memcpy(nodes[1].local_matrix34, identity, sizeof(identity));
    A2FO_N11_PoseChannel channel{};
    channel.index = 0; channel.node_index = 0; channel.channel_type = 5;
    channel.key_count = 1; channel.period = 2.0f;
    channel.keys = mutable_keys; channel.key_width = 1;
    A2FO_N11_PoseMetadata metadata{2, 1, 7, nodes, &channel};
    if (mode == 1) nodes[1].parent = 0; // cycle in later node
    if (mode == 2) channel.keys = nullptr;
    if (mode == 3) channel.key_width = 12;
    if (mode == 4) nodes[1].local_matrix34[10] = NAN;
    if (mode == 5) metadata.nodes = nullptr;
    if (mode == 6) nodes[1].index = 0;
    if (mode == 7) channel.node_index = 2;
    if (mode == 8) channel.period = 0.0f;
    if (mode == 9) mutable_name[2] = '\n';
    if (mode == 13) throw std::runtime_error("parser exception");
    if (mode == 14) { metadata.channel_count = 0; metadata.channels = nullptr; }
    const int accepted = sink(context, &metadata);
    if (mode == 10) sink(context, &metadata); // extra callback
    if (mode == 11) return 0; // parser reports failure after accepted callback
    if (mode == 12) return 2; // noncanonical success
    return accepted;
}
int alternate(const std::uint8_t* bytes, std::uint32_t length,
              A2FO_N11_PoseSink sink, void* context) {
    return parser(bytes, length, sink, context);
}
std::shared_ptr<const ModelAsset> asset(std::uint64_t serial) {
    auto value = std::make_shared<ModelAsset>();
    value->info.serial = serial; value->info.byte_count = 1;
    value->bytes = {42};
    return value;
}
void reset() { mode = 0; mutable_name[0] = 'c'; mutable_name[2] = 'i'; mutable_keys[0] = 2.0f; }
}

int main() {
    std::string error;
    ModelPoseSnapshot output;
    ModelPoseCache cache;
    auto first = asset(1);
    reset();
    assert(cache.inspect(first, parser, output, error) && calls == 1 && error.empty());
    assert(output.model == first && output.pose->nodes.size() == 2 &&
           output.pose->nodes[0].parent == 1 && output.pose->nodes[0].group_count == 2 &&
           output.pose->channels[0].keys == std::vector<float>{2.0f} &&
           output.pose->reference_count == 7);
    mutable_name[0] = 'X'; mutable_keys[0] = 9.0f;
    assert(output.pose->nodes[0].name == "child" && output.pose->channels[0].keys[0] == 2.0f);
    assert(cache.inspect(first, parser, output, error) && calls == 1); // hot
    auto collision = asset(1);
    assert(!cache.inspect(collision, parser, output, error) && calls == 1 &&
           error.find("collides") != std::string::npos);
    reset();
    assert(cache.inspect(first, alternate, output, error) && calls == 2 && cache.size() == 2);
    auto pinned = output;
    cache.clear();
    assert(cache.size() == 0 && cache.bytes() == 0 && pinned.model == first &&
           pinned.pose->nodes[0].name == "child");

    // Failure in late metadata or in the parser never replaces a previous output.
    for (int bad = 1; bad <= 13; ++bad) {
        reset(); mode = bad;
        auto owner = asset(100 + bad);
        assert(!cache.inspect(owner, parser, output, error));
        assert(output.model == pinned.model && output.pose == pinned.pose && cache.size() == 0);
    }
    reset();
    mode = 14;
    auto no_channels = asset(250);
    assert(cache.inspect(no_channels, parser, output, error) && output.pose->channels.empty());
    cache.clear();
    reset();
    ModelPoseCache limited(1, 512);
    ModelPoseSnapshot one;
    auto live = asset(300);
    assert(limited.inspect(live, parser, one, error));
    auto retained = one;
    auto second = asset(301);
    assert(!limited.inspect(second, parser, one, error) && one.pose == retained.pose &&
           limited.size() == 1);
    one = {}; retained = {}; // held cache owner remains live through `live`
    live.reset();
    assert(limited.inspect(second, parser, one, error) && limited.size() == 1);
    assert(one.model == second);

    // A failed small-budget miss leaves its prior snapshot and cache intact.
    ModelPoseCache no_room(2, 1);
    auto old = one;
    assert(!no_room.inspect(asset(500), parser, one, error) && one.pose == old.pose &&
           no_room.size() == 0);
}
