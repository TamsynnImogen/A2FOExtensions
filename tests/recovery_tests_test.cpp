#include "../modules/A2FORecoveryTests/recovery_tests.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>

using namespace a2fo::recovery_tests;

namespace {
void put(std::array<std::uint8_t, 0x88>& bytes, unsigned offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = std::uint8_t(value >> (8 * i));
}
std::uint32_t bits(float value) {
    std::uint32_t result;
    std::memcpy(&result, &value, 4);
    return result;
}

Snapshot context(std::uint32_t engine, std::uint32_t index, std::uint32_t selected,
                 std::uint32_t value) {
    Snapshot snapshot{};
    snapshot.routine = Routine::engine_context;
    snapshot.object = engine;
    snapshot.context_addresses = {engine + 0xc0u, engine + index * 4u + 0xccu,
                                  selected + 0x28u};
    snapshot.context_before = {index, selected, value};
    snapshot.context_after = snapshot.context_before;
    snapshot.actual_return = value;
    return snapshot;
}

Snapshot node(bool unit, std::uint16_t control = 0x037f) {
    Snapshot snapshot{};
    snapshot.routine = Routine::node_transform;
    snapshot.object = 0xfffffff0u; // All node-address arithmetic is modulo 2^32.
    snapshot.control_word = control;
    snapshot.fp_tag = 0;
    for (unsigned i = 0; i < snapshot.node_before.size(); ++i)
        snapshot.node_before[i] = std::uint8_t(i * 17u + 3u);
    put(snapshot.node_before, 0x54, unit ? 0x3f800000u : bits(2.0f));
    if (unit) put(snapshot.node_before, 0x24, 0x7fc12345u);
    if (!unit) {
        for (unsigned i = 0; i < 9; ++i)
            put(snapshot.node_before, 0x24 + i * 4, bits(float(int(i) - 4)));
    }
    // Translation is copied as raw words, including non-finite bit patterns.
    put(snapshot.node_before, 0x48, 0x7fc12345u);
    put(snapshot.node_before, 0x4c, 0xff800000u);
    put(snapshot.node_before, 0x50, 0x80000000u);
    snapshot.node_after = snapshot.node_before;
    if (!unit) {
        for (unsigned i = 0; i < 9; ++i)
            put(snapshot.node_after, 0x58 + i * 4, bits(float((int(i) - 4) * 2)));
        for (unsigned i = 0; i < 12; ++i)
            snapshot.node_after[0x7c + i] = snapshot.node_before[0x48 + i];
    }
    snapshot.actual_return = snapshot.object + (unit ? 0x24u : 0x58u);
    return snapshot;
}
} // namespace

int main() {
    auto simple = context(0x20001000u, 2, 0x20003000u, 0xdeadbeefu);
    auto match = compare(simple);
    assert(match.outcome == Outcome::match && match.expected_return == 0xdeadbeefu);
    auto json = record_json(simple, match);
    assert(json.find("\"version\":1") != std::string::npos);
    assert(json.find("\"context_addresses\":[") != std::string::npos);
    assert(json.find("\"actual_return\":3735928559") != std::string::npos);

    simple.actual_return ^= 1;
    auto mismatch = compare(simple);
    assert(mismatch.outcome == Outcome::mismatch);
    assert(mismatch.expected_word == 0xdeadbeefu && mismatch.actual_word == simple.actual_return);
    simple.context_after[2] ^= 1;
    assert(compare(simple).outcome == Outcome::skip);

    auto wrapped = context(0xffffff44u, 0, 0xffffffe0u, 0);
    assert(wrapped.context_addresses[0] == 4 && wrapped.context_addresses[1] == 0x10);
    assert(wrapped.context_addresses[2] == 8);
    assert(compare(wrapped).outcome == Outcome::match);
    wrapped.context_addresses[1] ^= 4;
    assert(compare(wrapped).outcome == Outcome::skip);

    auto alias = context(0x20001000u, 0xfffffffdu, 0xfffffffdu, 0);
    assert(alias.context_addresses[0] == alias.context_addresses[1]);
    assert(compare(alias).outcome == Outcome::match);
    alias.context_before[1] = 0;
    assert(compare(alias).outcome == Outcome::skip);

    auto unchanged = node(true);
    assert(compare(unchanged).outcome == Outcome::match);
    assert(record_json(unchanged, compare(unchanged)).find("\"node_before\":\"") != std::string::npos);
    unchanged.node_after[0x58] ^= 1;
    mismatch = compare(unchanged);
    assert(mismatch.outcome == Outcome::mismatch && mismatch.mismatch_offset == 0x58);
    for (const unsigned offset : {0u, 0x24u, 0x54u}) {
        auto changed_input = node(true);
        changed_input.node_after[offset] ^= 1;
        const auto unstable = compare(changed_input);
        assert(unstable.outcome == Outcome::skip);
        assert(std::strcmp(unstable.reason, "node_inputs_changed") == 0);
    }

    constexpr std::uint16_t pc[] = {0x007f, 0x027f, 0x037f};
    constexpr std::uint16_t rc[] = {0x0000, 0x0400, 0x0800, 0x0c00};
    for (auto precision : pc) for (auto rounding : rc) {
        auto scaled = node(false, precision | rounding);
        match = compare(scaled);
        assert(match.outcome == Outcome::match && match.expected_return == scaled.actual_return);
    }
    auto scaled = node(false);
    scaled.node_after[0x60] ^= 1;
    mismatch = compare(scaled);
    assert(mismatch.outcome == Outcome::mismatch && mismatch.mismatch_offset == 0x60);
    scaled = node(false);
    scaled.node_after[0x54] ^= 1;
    assert(compare(scaled).outcome == Outcome::skip);
    scaled = node(false);
    scaled.actual_return ^= 1;
    assert(compare(scaled).outcome == Outcome::mismatch);
    scaled = node(false);
    put(scaled.node_before, 0x24, 0x7fc00001u);
    assert(compare(scaled).outcome == Outcome::skip);
    scaled = node(false);
    scaled.fp_tag = 1;
    assert(compare(scaled).outcome == Outcome::skip);
    scaled = node(false, 0x0300); // Unmasked x87 exceptions.
    assert(compare(scaled).outcome == Outcome::skip);
    scaled = node(false, 0x017f); // Reserved precision setting.
    assert(compare(scaled).outcome == Outcome::skip);

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    alignas(16) std::array<std::uint8_t, 512> before{}, after{};
    const auto fp_snapshot = node(false);
    asm volatile("fld1\n\tfxsave %0" : "=m"(before) : : "memory");
    assert(compare(fp_snapshot).outcome == Outcome::match);
    asm volatile("fxsave %0\n\tfstp %%st(0)" : "=m"(after) : : "memory");
    // XMM0-15 are caller-clobbered by the host ABI; x87 and MXCSR occupy 0..159.
    assert(std::memcmp(before.data(), after.data(), 160) == 0);
#endif
}
