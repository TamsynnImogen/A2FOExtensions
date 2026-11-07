#include "../renderer/native11/texture_store.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

using namespace a2fo::native11;
namespace {
void need(bool value, const char* label) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}
bool put(ResolvedTextureStore& store, unsigned key, std::string name,
         std::vector<std::uint8_t> bytes, unsigned flags=0, bool colour=false,
         std::string path="path") {
    return store.publish(key, std::move(name), std::move(path), flags, colour, std::move(bytes));
}
bool put_stream(ResolvedTextureStore& store, unsigned key, std::string name,
                std::string requested, std::vector<std::uint8_t> bytes,
                unsigned flags=0, bool colour=false) {
    return store.publish_stream(key, std::move(name), std::move(requested), flags,
                                colour, std::move(bytes));
}
}
int main() {
    ResolvedTextureStore store(8, 3);
    need(store.generation() == 1, "initial generation");
    std::vector<std::uint8_t> source{1, 2, 3};
    const auto* original = source.data();
    need(store.publish(7, "a.dds", "file", 2, false, std::move(source)), "first publish");
    auto held = store.current(7);
    need(held && held->serial == 1 && held->texture == 7 && held->flags == 2 &&
         held->logical_name == "a.dds" && held->resolved_path == "file" &&
         held->source_kind == TextureSourceKind::resolved_file && held->requested_path.empty() &&
         held->bytes->data() == original && (*held->bytes)[0] == 1 &&
         store.bytes() == 3 && store.generation() == 2, "owned moved payload and metadata");
    TextureAssetInfo info;
    need(store.info(7, info) && info.serial == 1 && info.logical_name == "a.dds" &&
         info.source_kind == TextureSourceKind::resolved_file && info.requested_path.empty() &&
         !store.find(7, 2), "metadata and serial lookup");
    need(!put(store, 7, "bad", std::vector<std::uint8_t>(9, 0)), "failed replacement");
    need(!store.current(7) && !store.find(7, 1) && store.bytes() == 0 &&
         store.generation() == 3 && (*held->bytes)[0] == 1, "failed replacement retires binding");
    need(put(store, 7, "a.dds", {8}), "republish");
    need(store.current(7)->serial == 2 && !store.find(7, 1), "monotonic serial");
    const auto before_clear = store.generation();
    store.clear();
    need(store.generation() == before_clear + 1 && store.size() == 0 &&
         (*held->bytes)[1] == 2, "clear preserves old reader");
    need(put(store, 7, "a.dds", {9}) && store.current(7)->serial == 3,
         "serial survives clear");
    const auto before_missing = store.generation();
    store.invalidate(99);
    need(store.generation() == before_missing + 1, "missing invalidation advances");
    store.invalidate(7);
    need(store.size() == 0 && store.bytes() == 0, "invalidation counters");
    auto absent = info;
    need(!store.info(7, absent) && absent.serial == info.serial, "missing info preserves output");

    ResolvedTextureStore limits(4, 1);
    need(put(limits, 1, "a", {1, 2, 3, 4}), "exact byte limit");
    need(!put(limits, 2, "b", {1}) && limits.size() == 1, "record cap");
    limits.invalidate(1);
    need(!put(limits, 0, "a", {1}), "zero key");
    need(!put(limits, 1, "", {1}) && !put(limits, 1, "bad\n", {1}) &&
         !put(limits, 1, std::string(1025, 'x'), {1}), "logical name bounds");
    need(!put(limits, 1, "x", {1}, 0, false, "") &&
         !put(limits, 1, "x", {1}, 0, false, std::string("a\0b", 3)),
         "empty and embedded NUL path");
    need(put(limits, 1, std::string(1024, 'x'), {1}, 0, false,
             std::string(32767, 'p')), "maximum name and path");
    need(!put(limits, 1, "x", {1}, 0, false, std::string(32768, 'p')) &&
         limits.size() == 0, "oversize path retires binding");
    ResolvedTextureStore aggregate(4, 2);
    need(put(aggregate, 1, "a", {1,2,3}) && !put(aggregate, 2, "b", {4,5}) &&
         aggregate.bytes() == 3, "aggregate budget");
    std::shared_ptr<const TextureAsset> after_store;
    {
        ResolvedTextureStore transient;
        need(put(transient, 8, "persist", {5,6}), "transient publish");
        after_store = transient.current(8);
    }
    need(after_store && *after_store->bytes == std::vector<std::uint8_t>({5,6}),
         "asset survives store destruction");
    ResolvedTextureStore payload_limit;
    need(put(payload_limit, 1, "max", std::vector<std::uint8_t>(64ull*1024*1024, 1)),
         "64 MiB payload accepted");
    need(!put(payload_limit, 1, "over", std::vector<std::uint8_t>(64ull*1024*1024+1, 1)) &&
         payload_limit.size() == 0, "payload above 64 MiB rejected and retired");

    ResolvedTextureStore streams(4, 2);
    std::vector<std::uint8_t> opened{4, 5};
    const auto* opened_data = opened.data();
    need(put_stream(streams, 1, "a.tga", "Textures\\a.tga", std::move(opened), 2),
         "opened stream published");
    auto stream_owner = streams.current(1);
    need(stream_owner && stream_owner->source_kind == TextureSourceKind::opened_stream &&
         stream_owner->requested_path == "Textures\\a.tga" &&
         stream_owner->resolved_path.empty() && stream_owner->bytes->data() == opened_data &&
         stream_owner->serial == 1 && streams.bytes() == 2 && streams.generation() == 2,
         "stream provenance and moved opened bytes");
    TextureAssetInfo stream_info;
    need(streams.info(1, stream_info) &&
         stream_info.source_kind == TextureSourceKind::opened_stream &&
         stream_info.requested_path == stream_owner->requested_path &&
         stream_info.resolved_path.empty() && stream_info.flags == 2 &&
         stream_info.serial == stream_owner->serial && stream_info.logical_name == "a.tga",
         "stream info copies complete metadata");
    stream_info.requested_path.clear();
    need(stream_owner->requested_path == "Textures\\a.tga", "info owns metadata copy");
    need(put(streams, 1, "a.dds", {8}, 0, false, "physical.dds") &&
         stream_owner->requested_path == "Textures\\a.tga" &&
         *stream_owner->bytes == std::vector<std::uint8_t>({4, 5}),
         "stream owner retained after DDS replacement");
    need(!put_stream(streams, 1, "a.tga", "bad\npath", {3}) &&
         !streams.current(1) && streams.bytes() == 0,
         "failed stream retires prior DDS binding");
    need(put_stream(streams, 1, "a.tga", "requested", {6}),
         "stream republish after failure");
    auto cleared_stream = streams.current(1);
    need(cleared_stream && cleared_stream->serial == 3 &&
         streams.generation() == 5 && streams.find(1, 3) == cleared_stream &&
         !streams.find(1, 2), "stream serial and generation follow DDS replacements");
    streams.clear();
    need(streams.size() == 0 && cleared_stream && (*cleared_stream->bytes)[0] == 6 &&
         stream_owner->requested_path == "Textures\\a.tga", "stream owners survive clear");

    ResolvedTextureStore stream_limits(4, 1);
    need(!put_stream(stream_limits, 1, "a", "", {1}) &&
         !put_stream(stream_limits, 1, "a", std::string("bad\0path", 8), {1}) &&
         !put_stream(stream_limits, 1, "a", "bad\n", {1}) &&
         !put_stream(stream_limits, 1, "a", std::string(1, '\x7f'), {1}) &&
         !put_stream(stream_limits, 1, "a", std::string(1, '\x80'), {1}) &&
         !put_stream(stream_limits, 1, "a", std::string(1024, 'p'), {1}),
         "stream requested path bounds and ASCII");
    need(!put_stream(stream_limits, 1, "", "p", {1}) &&
         !put_stream(stream_limits, 1, std::string(1025, 'n'), "p", {1}) &&
         !put_stream(stream_limits, 1, "bad\n", "p", {1}),
         "stream logical name bounds");
    need(put_stream(stream_limits, 1, std::string(1024, 'n'),
                    std::string(1023, 'p'), {1, 2, 3, 4}),
         "maximum stream names and byte budget");
    need(!put_stream(stream_limits, 1, "x", "p", {1, 2, 3, 4, 5}) &&
         stream_limits.size() == 0, "stream payload budget retires binding");
    need(put_stream(stream_limits, 1, "x", "p", {1}) &&
         !put_stream(stream_limits, 2, "y", "p", {1}) &&
         stream_limits.size() == 1, "stream record budget");

    ResolvedTextureStore duplicates;
    std::vector<SnapshotTexture> output{{"sentinel", std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{9})}};
    std::string error;
    need(put(duplicates, 1, "Exact", {1,2}, 3, false, "one") &&
         put(duplicates, 2, "Exact", {1,2}, 3, false, "two") &&
         put(duplicates, 3, "exact", {3}), "duplicate and case-distinct inputs");
    need(duplicates.collect(output, error) && output.size() == 2 &&
         output[0].identifier == "Exact" && output[1].identifier == "exact" &&
         *output[0].bytes == std::vector<std::uint8_t>({1,2}), "exact dedup and case sensitivity");
    need(put_stream(duplicates, 4, "Exact", "requested.tga", {1,2}, 3) &&
         duplicates.collect(output, error) && output.size() == 2,
         "same bytes and settings dedup across source provenance");
    auto stream_duplicate = duplicates.current(4);
    need(stream_duplicate && stream_duplicate->source_kind == TextureSourceKind::opened_stream &&
         stream_duplicate->requested_path == "requested.tga" &&
         stream_duplicate->resolved_path.empty(), "collection retains stream metadata");
    need(put_stream(duplicates, 4, "Exact", "other.tga", {3}, 3) &&
         !duplicates.collect(output, error) && !error.empty(),
         "cross provenance conflicting bytes rejected");
    need(put_stream(duplicates, 4, "Exact", "other.tga", {1,2}, 2) &&
         !duplicates.collect(output, error), "cross provenance conflicting settings rejected");
    auto retained = output[0].bytes;
    duplicates.clear();
    need((*retained)[0] == 1, "collected owner survives store clear");
    need(put(duplicates, 1, "Same", {1}, 0) && put(duplicates, 2, "Same", {2}, 0),
         "conflicting content setup");
    output = {{"sentinel", retained}};
    need(!duplicates.collect(output, error) && !error.empty() &&
         output.size() == 1 && output[0].identifier == "sentinel", "content conflict atomic");
    need(put(duplicates, 2, "Same", {1}, 1) && !duplicates.collect(output, error),
         "flags conflict");
    need(put(duplicates, 2, "Same", {1}, 0, true) && !duplicates.collect(output, error),
         "colour key rejected");
    need(put(duplicates, 2, "Same", {1}, 4) && !duplicates.collect(output, error),
         "surface rejected");
    need(output.size() == 1 && output[0].identifier == "sentinel", "failed collect atomic");
    duplicates.clear();
    for (unsigned i=0; i<257; ++i)
        need(put(duplicates, i+1, std::to_string(i), {1}), "unique count setup");
    need(!duplicates.collect(output, error) && output[0].identifier == "sentinel",
         "256 unique cap");
    ResolvedTextureStore required;
    need(put(required, 1, "wanted", {1,2}) &&
         put(required, 2, "wanted", {1,2}) &&
         put(required, 3, "unrelated", {3}, 0, true) &&
         put(required, 4, "unrelated", {4}, 4), "required collection setup");
    std::vector<std::string> requested{"wanted", "wanted"};
    need(required.collect_required(requested, output, error) && error.empty() &&
         output.size() == 1 && output[0].identifier == "wanted" &&
         *output[0].bytes == std::vector<std::uint8_t>({1,2}),
         "required exact names deduplicate and ignore unrelated unsupported records");
    auto required_owner = output[0].bytes;
    required.clear();
    need(*required_owner == std::vector<std::uint8_t>({1,2}),
         "required snapshot owner survives store clear");
    need(required.collect_required({}, output, error) && output.empty() && error.empty(),
         "empty required list succeeds empty");
    output = {{"sentinel", retained}};
    need(put(required, 1, "wanted", {1}) &&
         !required.collect_required({"wanted", "missing"}, output, error) &&
         !error.empty() && output.size() == 1 && output[0].identifier == "sentinel",
         "missing required name fails atomically");
    need(!required.collect_required({"Wanted"}, output, error) &&
         output.size() == 1 && output[0].identifier == "sentinel",
         "required names are exact case-sensitive matches");
    need(!required.collect_required({"bad\nname"}, output, error) &&
         !required.collect_required({std::string(1025, 'x')}, output, error) &&
         output.size() == 1 && output[0].identifier == "sentinel",
         "required name ASCII and length bounds preserve output");
    std::vector<std::string> too_many;
    for (unsigned i=0; i<257; ++i) too_many.push_back("n" + std::to_string(i));
    need(!required.collect_required(too_many, output, error) &&
         output.size() == 1 && output[0].identifier == "sentinel",
         "required unique-name cap and atomic output");
    need(put(required, 2, "wanted", {2}) &&
         !required.collect_required({"wanted"}, output, error) &&
         output.size() == 1 && output[0].identifier == "sentinel",
         "needed duplicate conflict rejected atomically");
    need(put(required, 2, "wanted", {1}, 1) &&
         !required.collect_required({"wanted"}, output, error),
         "needed duplicate flag conflict rejected");
    need(put(required, 2, "wanted", {1}, 0, true) &&
         !required.collect_required({"wanted"}, output, error),
         "needed colour key rejected");
    need(put(required, 2, "wanted", {1}, 4) &&
         !required.collect_required({"wanted"}, output, error),
         "needed surface rejected");
    ResolvedTextureStore unrelated_count;
    for (unsigned i=0; i<257; ++i)
        need(put(unrelated_count, i+1, "other" + std::to_string(i), {1}),
             "unrelated record count setup");
    need(put(unrelated_count, 300, "other0", {8}) &&
         put(unrelated_count, 301, "other1", {8}, 0, true) &&
         put(unrelated_count, 302, "other2", {8}, 4) &&
         put(unrelated_count, 303, "wanted", {7}) &&
         unrelated_count.collect_required({"wanted"}, output, error) &&
         output.size() == 1 && output[0].identifier == "wanted" &&
         *output[0].bytes == std::vector<std::uint8_t>({7}),
         "unrelated count and conflicts do not block requested collection");
    ResolvedTextureStore mixed;
    need(put(mixed,1,"dds",{1})&&mixed.publish_stream(2,"tga","Textures\\tga.tga",0,false,{2}),"mixed source setup");
    const auto dds=mixed.current(1),tga=mixed.current(2);const auto before=mixed.generation();
    mixed.clear_source(TextureSourceKind::opened_stream);
    need(mixed.current(1)==dds&&!mixed.current(2)&&mixed.bytes()==1&&mixed.generation()>before&&(*tga->bytes)[0]==2,
         "stream-only reset preserves DDS and retained stream owners");
    const auto unchanged=mixed.generation();mixed.clear_source(TextureSourceKind::opened_stream);
    need(mixed.current(1)==dds&&mixed.generation()==unchanged,"empty stream reset leaves DDS epoch unchanged");
    std::puts("native11 texture store test passed");
}
