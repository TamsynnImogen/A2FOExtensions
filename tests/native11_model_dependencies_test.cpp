#include "../renderer/native11/model_dependencies.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool ok, const char* label) {
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}
struct PartInput { std::string texture; bool white = false; bool bad = false; };
std::vector<PartInput> parts;
int parser_calls = 0;
bool parser_fails = false, parser_throws = false, ignore_rejection = false;
A2FO_N11_Vertex vertex{};
std::uint32_t index = 0;

int fake_parser(const std::uint8_t*, std::uint32_t, A2FO_N11_PartSink sink, void* context) {
    ++parser_calls;
    if (parser_throws) throw 7;
    for (const auto& input : parts) {
        A2FO_N11_Part part{};
        part.vertices = &vertex; part.vertex_count = 1;
        part.indices = &index; part.index_count = 1;
        if (!input.white) {
            part.texture_name = reinterpret_cast<const std::uint8_t*>(input.texture.data());
            part.texture_name_length = static_cast<std::uint32_t>(input.texture.size());
        }
        if (input.bad) { part.texture_name = nullptr; part.texture_name_length = 1; }
        if (!sink(context, &part) && !ignore_rejection) return 0;
    }
    return parser_fails ? 0 : 1;
}

std::shared_ptr<ModelAsset> asset(std::uint64_t serial = 1, std::size_t bytes = 1) {
    auto value = std::make_shared<ModelAsset>();
    value->info.serial = serial;
    value->info.byte_count = static_cast<std::uint32_t>(bytes);
    value->bytes.resize(bytes, 1);
    return value;
}
void reset() { parser_calls = 0; parser_fails = false; parser_throws = false; ignore_rejection = false; }
}

int main() {
    ModelDependencyCache cache;
    std::string error;
    std::shared_ptr<const ModelDependencies> result;
    parts = {{"Hull", false, false}, {"", true, false}, {"Hull", false, false},
             {"hull", false, false}};
    auto a = asset();
    need(cache.get(a, &fake_parser, result, error), "parse manifest");
    need(parser_calls == 1 && result->part_count == 4 && result->textures.size() == 2 &&
         result->textures[0] == "Hull" && result->textures[1] == "hull", "white, dedupe, case and order");
    auto retained = result;
    result.reset();
    need(cache.get(a, &fake_parser, result, error) && parser_calls == 1 && result == retained,
         "immutable owner cache hit skips parser");
    const auto alternate=+[](const std::uint8_t* data,std::uint32_t size,A2FO_N11_PartSink sink,void* context)->int{return fake_parser(data,size,sink,context);};
    need(cache.get(a,alternate,result,error)&&parser_calls==2&&result!=retained,"changed parser policy does not reuse manifest");
    need(cache.get(a,fake_parser,result,error)&&parser_calls==2&&result==retained,"original parser policy retains its own cache entry");

    parts = {{"first", false, false}, {"", false, true}};
    auto rollback_asset = asset(2);
    auto prior = result;
    reset(); ignore_rejection = true;
    need(!cache.get(rollback_asset, &fake_parser, result, error) && result == prior && parser_calls == 1,
         "malformed later callback rolls back output");
    parts = {{"valid", false, false}};
    reset(); parser_fails = true;
    need(!cache.get(rollback_asset, &fake_parser, result, error) && result == prior,
         "parser failure leaves output unchanged");
    reset(); parser_throws = true;
    need(!cache.get(rollback_asset, &fake_parser, result, error) && result == prior,
         "parser exception contained");
    reset(); parts.clear();
    need(!cache.get(asset(22), &fake_parser, result, error) && result == prior,
         "empty model parse rejected");
    reset();
    auto bad_size = asset(3, 2); bad_size->info.byte_count = 1;
    need(!cache.get(bad_size, &fake_parser, result, error) && result == prior && parser_calls == 0,
         "byte metadata mismatch rejected before parser");
    auto large = asset(4, 64u * 1024u * 1024u + 1u);
    need(!cache.get(large, &fake_parser, result, error) && parser_calls == 0, "oversized model rejected");

    parts.clear();
    for (unsigned i = 0; i < 8192; ++i) parts.push_back({"", true, false});
    auto max_parts = asset(5);
    need(cache.get(max_parts, &fake_parser, result, error) && result->part_count == 8192,
         "part count limit accepted");
    parts.push_back({"", true, false});
    auto too_many = asset(6); reset(); ignore_rejection = true;
    need(!cache.get(too_many, &fake_parser, result, error), "part count limit enforced");

    parts = {{"", true, false}};
    auto bad_counts = asset(7);
    auto count_parser = +[](const std::uint8_t*, std::uint32_t, A2FO_N11_PartSink sink, void* ctx) -> int {
        A2FO_N11_Part p{}; p.vertices = &vertex; p.vertex_count = 1048577;
        p.indices = &index; p.index_count = 1; sink(ctx, &p); return 1;
    };
    need(!cache.get(bad_counts, count_parser, result, error), "element cap enforced");

    parts.clear();
    for (unsigned i = 0; i < 257; ++i) parts.push_back({std::string("t") + std::to_string(i), false, false});
    auto too_many_names = asset(8); reset(); ignore_rejection = true;
    need(!cache.get(too_many_names, &fake_parser, result, error), "distinct texture cap enforced");
    parts = {{std::string(1025, 'x'), false, false}};
    need(!cache.get(asset(9), &fake_parser, result, error), "texture length cap enforced");
    parts = {{std::string("bad\0name", 8), false, false}};
    need(!cache.get(asset(10), &fake_parser, result, error), "NUL rejected");

    ModelDependencyCache bounded(1, 4);
    parts = {{"large", false, false}};
    auto noncached = asset(11); reset();
    need(bounded.get(noncached, &fake_parser, result, error) && bounded.size() == 0 && bounded.bytes() == 0,
         "oversized manifest returned without insertion");
    parts = {{"a", false, false}};
    auto first = asset(12); need(bounded.get(first, &fake_parser, result, error), "bounded insert one");
    auto second = asset(13); need(bounded.get(second, &fake_parser, result, error) && bounded.size() == 1,
         "old entry evicted before insertion");
    auto owner_alias = asset(13); reset();
    need(bounded.get(owner_alias, &fake_parser, result, error) && parser_calls == 1,
         "serial collision reparses different owner");

    ModelDependencyCache expiration;
    std::weak_ptr<ModelAsset> weak;
    { auto temporary = asset(20); weak = temporary; parts = {{"z", false, false}};
      need(expiration.get(temporary, &fake_parser, result, error), "expiration setup"); }
    parts = {{"q", false, false}};
    need(expiration.get(asset(21), &fake_parser, result, error) && weak.expired() && expiration.size() == 1,
         "expired weak entry purged");
    auto output_kept = result;
    expiration.clear();
    need(expiration.size() == 0 && expiration.bytes() == 0 && output_kept->textures[0] == "q",
         "clear preserves returned manifest");
    std::puts("native11 model dependencies test passed");
}
