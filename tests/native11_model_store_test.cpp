#include "../renderer/native11/model_store.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

using namespace a2fo::native11;
namespace {
void need(bool value, const char* label) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}
OpenedFile file(std::string name, std::vector<std::uint8_t> bytes) {
    OpenedFile result;
    result.name = std::move(name);
    result.bytes = std::move(bytes);
    return result;
}
}

int main() {
    ModelAssetStore store(8, 2);
    auto source = file("ship.sod", {1, 2, 3});
    const auto* original_bytes = source.bytes.data();
    need(store.publish(7, 0xdeadbeefu, std::move(source)), "first publish");
    need(store.size() == 1 && store.bytes() == 3, "first counters");
    ModelAssetInfo info{};
    need(store.info(7, info) && info.serial == 1 && info.database == 7 &&
         info.byte_count == 3 && info.parser_scalar_bits == 0xdeadbeefu &&
         std::strcmp(info.name, "ship.sod") == 0, "metadata lookup");
    auto held = store.find(7, info.serial);
    need(held && held->bytes == std::vector<std::uint8_t>({1, 2, 3}) &&
         held->bytes.data() == original_bytes, "owned moved bytes");
    need(!store.find(7, info.serial + 1), "serial mismatch");
    auto missing = info;
    need(!store.info(99, missing) && std::memcmp(&missing, &info, sizeof(info)) == 0,
         "failed info leaves output unchanged");

    need(!store.publish(7, 0, file("ship.sod", std::vector<std::uint8_t>(9, 4))),
         "over-budget replacement rejected");
    need(store.size() == 0 && store.bytes() == 0 && !store.find(7, info.serial) &&
         held->bytes[0] == 1, "failed replacement invalidates but retained reader survives");
    need(store.publish(7, 1, file("ship.sod", {8})), "same database republish");
    ModelAssetInfo fresh{};
    need(store.info(7, fresh) && fresh.serial > info.serial &&
         !store.find(7, info.serial) && store.find(7, fresh.serial)->bytes[0] == 8,
         "pointer reuse requires fresh serial");
    store.clear();
    need(store.size() == 0 && store.bytes() == 0 && held->bytes.size() == 3,
         "clear preserves retained reader");
    need(store.publish(7, 2, file("again", {9})) && store.info(7, fresh) &&
         fresh.serial > 2, "clear does not recycle serial");

    ModelAssetStore limited(4, 1);
    need(limited.publish(1, 0, file("a", {1, 2, 3, 4})), "exact byte budget");
    need(!limited.publish(2, 0, file("b", {1})), "record limit");
    need(limited.size() == 1 && limited.bytes() == 4, "rejected other key preserves entry");
    limited.invalidate(1);
    need(limited.size() == 0 && limited.bytes() == 0, "invalidate counters");
    need(!limited.publish(0, 0, file("a", {1})), "zero database");
    need(!limited.publish(3, 0, file("a", {})), "empty payload");
    need(!limited.publish(3, 0, file("", {1})), "empty name");
    need(!limited.publish(3, 0, file(std::string(260, 'x'), {1})), "oversize name");
    need(!limited.publish(3, 0, file("bad\nname", {1})), "control in name");
    need(!limited.publish(3, 0, file(std::string(1, '\x7f'), {1})), "DEL in name");
    need(!limited.publish(3, 0, file(std::string(1, '\x80'), {1})), "non-ASCII name");
    need(limited.publish(3, 0, file(std::string(259, 'x'), {1})), "maximum name");
    ModelAssetInfo longest{};
    need(limited.info(3, longest) && longest.name[259] == 0 &&
         std::strlen(longest.name) == 259, "bounded terminated name");
    need(!limited.publish(3, 0, file("bad\n", {1})) &&
         limited.size() == 0 && limited.bytes() == 0, "invalid replacement retires binding");
    ModelAssetStore aggregate(4, 2);
    need(aggregate.publish(1, 0, file("one", {1, 2, 3})), "aggregate first");
    need(!aggregate.publish(2, 0, file("two", {4, 5})) &&
         aggregate.size() == 1 && aggregate.bytes() == 3, "aggregate byte budget");
    need(aggregate.publish(2, 0, file("two", {4})) && aggregate.bytes() == 4,
         "aggregate exact budget");

    constexpr auto known = encode_native_x87(0x037f);
    ModelAssetStore provenance(4, 1);
    need(provenance.publish(40, 0x12345678u, file("known.sod", {1, 2}), known),
         "known load-time arithmetic published");
    ModelAssetInfo known_info{};
    need(provenance.info(40, known_info) &&
         known_info.parser_native_x87_control == known &&
         known_info.parser_scalar_bits == 0x12345678u &&
         provenance.bytes() == 2 && provenance.size() == 1,
         "known provenance and scalar bits retained without reinterpretation");
    auto known_asset = provenance.find(40, known_info.serial);
    need(known_asset && known_asset->info.parser_native_x87_control == known,
         "snapshot pins known load-time provenance");
    need(!provenance.publish(40, 0, file("bad.sod", {3}), 0x037f) &&
         provenance.size() == 0 && provenance.bytes() == 0 &&
         !provenance.find(40, known_info.serial) &&
         known_asset->info.parser_native_x87_control == known,
         "raw unmarked control word rejects replacement and retires binding");
    need(!provenance.publish(41, 0, file("bad.sod", {3}),
                             encode_native_x87(0x0100)) &&
         provenance.size() == 0 && provenance.bytes() == 0,
         "reserved precision encoding rejected without accounting change");
    need(provenance.publish(40, 0xabcdef01u, file("unknown.sod", {4})) &&
         provenance.size() == 1 && provenance.bytes() == 1,
         "older diagnostic publisher retains unknown-zero default");
    ModelAssetInfo unknown_info{};
    need(provenance.info(40, unknown_info) &&
         unknown_info.serial == known_info.serial + 1 &&
         unknown_info.parser_native_x87_control == 0 &&
         unknown_info.parser_scalar_bits == 0xabcdef01u,
         "invalid publications do not consume serial; unknown provenance is explicit");
    auto unknown_asset = provenance.find(40, unknown_info.serial);
    provenance.clear();
    need(provenance.size() == 0 && provenance.bytes() == 0 &&
         known_asset->info.parser_native_x87_control == known &&
         unknown_asset->info.parser_native_x87_control == 0,
         "clear preserves both retained immutable provenance snapshots");
    std::puts("native11 model store test passed");
}
