#include "../../rust/native_fp_ffi.h"
#include "recovery_tests.hpp"

#include "armadal/engine_context.hpp"
#include "armadal/node_transform.hpp"

#include <cstring>
#include <limits>

namespace a2fo::recovery_tests {
namespace {

constexpr std::uint32_t no_offset = std::numeric_limits<std::uint32_t>::max();

Comparison result(Outcome outcome, const char* reason, std::uint32_t expected = 0,
                  std::uint32_t offset = no_offset, std::uint32_t wanted = 0,
                  std::uint32_t actual = 0) {
    return {outcome, reason, expected, offset, wanted, actual};
}

std::uint32_t word(const std::array<std::uint8_t, 0x88>& bytes, std::uint32_t offset) {
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
           (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}

void put_word(std::array<std::uint8_t, 0x88>& bytes, std::uint32_t offset,
              std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = std::uint8_t(value >> (8 * i));
}

bool finite(std::uint32_t bits) { return (bits & 0x7f800000u) != 0x7f800000u; }

struct ContextMemory {
    const Snapshot& snapshot;
    bool valid = true;

    static std::uint32_t read(void* pointer, std::uint32_t address) {
        auto& self = *static_cast<ContextMemory*>(pointer);
        for (unsigned i = 0; i < 3; ++i)
            if (self.snapshot.context_addresses[i] == address)
                return self.snapshot.context_before[i];
        self.valid = false;
        return 0;
    }
};

struct NodeMemory {
    std::uint32_t base;
    std::array<std::uint8_t, 0x88> bytes;
    bool valid = true;

    static std::uint32_t read(void* pointer, std::uint32_t address) {
        auto& self = *static_cast<NodeMemory*>(pointer);
        const std::uint32_t offset = address - self.base;
        if (offset > self.bytes.size() - 4) { self.valid = false; return 0; }
        return word(self.bytes, offset);
    }
    static void write(void* pointer, std::uint32_t address, std::uint32_t value) {
        auto& self = *static_cast<NodeMemory*>(pointer);
        const std::uint32_t offset = address - self.base;
        if (offset > self.bytes.size() - 4) { self.valid = false; return; }
        put_word(self.bytes, offset, value);
    }
};

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
class ScopedX87 {
public:
    explicit ScopedX87(std::uint16_t control_word) {
        a2fo_rs_fp_save_reset(saved_, &control_word);
    }
    ~ScopedX87() { a2fo_rs_fp_restore(saved_); }
    ScopedX87(const ScopedX87&) = delete;
    ScopedX87& operator=(const ScopedX87&) = delete;
private:
    alignas(16) std::uint8_t saved_[512]{};
};
constexpr bool has_scoped_x87 = true;
#else
constexpr bool has_scoped_x87 = false;
#endif

Comparison context(const Snapshot& snapshot) {
    const auto& address = snapshot.context_addresses;
    const auto& before = snapshot.context_before;
    const auto& after = snapshot.context_after;
    if (address[0] != snapshot.object + 0xc0u ||
        address[1] != snapshot.object + before[0] * 4u + 0xccu ||
        address[2] != before[1] + 0x28u)
        return result(Outcome::skip, "context_addresses_invalid");
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned j = i + 1; j < 3; ++j)
            if (address[i] == address[j] &&
                (before[i] != before[j] || after[i] != after[j]))
                return result(Outcome::skip, "context_alias_inconsistent");
        if (before[i] != after[i]) return result(Outcome::skip, "context_changed");
    }
    ContextMemory provider{snapshot};
    const armadal::entity_registry::Memory memory{&provider, ContextMemory::read,
                                                  nullptr, nullptr, nullptr};
    const auto expected = armadal::engine_context::lookup(snapshot.object, memory);
    if (!provider.valid) return result(Outcome::skip, "context_access_invalid");
    if (expected != snapshot.actual_return)
        return result(Outcome::mismatch, "return_mismatch", expected, no_offset,
                      expected, snapshot.actual_return);
    return result(Outcome::match, "matched", expected);
}

Comparison node(const Snapshot& snapshot) {
    for (std::uint32_t offset = 0; offset < 0x58; ++offset)
        if (snapshot.node_before[offset] != snapshot.node_after[offset])
            return result(Outcome::skip, "node_inputs_changed");
    const auto scale = word(snapshot.node_before, 0x54);
    const bool unit = scale == 0x3f800000u;
    if (!unit) {
        if (!has_scoped_x87 || snapshot.fp_tag != 0 ||
            (snapshot.control_word & 0x3fu) != 0x3fu ||
            ((snapshot.control_word >> 8) & 3u) == 1u)
            return result(Outcome::skip, "fp_domain_unsupported");
        if (!finite(scale)) return result(Outcome::skip, "nonfinite_arithmetic_input");
        for (std::uint32_t offset = 0x24; offset < 0x48; offset += 4)
            if (!finite(word(snapshot.node_before, offset)))
                return result(Outcome::skip, "nonfinite_arithmetic_input");
    }
    NodeMemory provider{snapshot.object, snapshot.node_before};
    const armadal::entity_registry::Memory memory{&provider, NodeMemory::read,
                                                  nullptr, NodeMemory::write, nullptr};
    std::uint32_t expected = 0;
    if (unit) {
        expected = armadal::node_transform::get(snapshot.object, memory);
    } else {
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
        ScopedX87 fp(snapshot.control_word);
        expected = armadal::node_transform::get(snapshot.object, memory);
#endif
    }
    if (!provider.valid) return result(Outcome::skip, "node_access_invalid");
    for (std::uint32_t i = 0; i < provider.bytes.size(); ++i) {
        if (provider.bytes[i] != snapshot.node_after[i]) {
            const auto aligned = i & ~3u;
            return result(Outcome::mismatch, "node_image_mismatch", expected, i,
                          word(provider.bytes, aligned), word(snapshot.node_after, aligned));
        }
    }
    if (expected != snapshot.actual_return)
        return result(Outcome::mismatch, "return_mismatch", expected, no_offset,
                      expected, snapshot.actual_return);
    return result(Outcome::match, "matched", expected);
}

void append_hex(std::string& out, const std::array<std::uint8_t, 0x88>& bytes) {
    constexpr char digit[] = "0123456789abcdef";
    out.push_back('"');
    for (const auto byte : bytes) {
        out.push_back(digit[byte >> 4]);
        out.push_back(digit[byte & 15]);
    }
    out.push_back('"');
}

void append_words(std::string& out, const std::array<std::uint32_t, 3>& words) {
    out.push_back('[');
    for (unsigned i = 0; i < words.size(); ++i) {
        if (i) out.push_back(',');
        out += std::to_string(words[i]);
    }
    out.push_back(']');
}

} // namespace

Comparison compare(const Snapshot& snapshot) {
    switch (snapshot.routine) {
    case Routine::engine_context: return context(snapshot);
    case Routine::node_transform: return node(snapshot);
    }
    return result(Outcome::skip, "routine_unknown");
}

std::string record_json(const Snapshot& snapshot, const Comparison& comparison) {
    const char* routine = snapshot.routine == Routine::engine_context ? "engine_context" :
                          snapshot.routine == Routine::node_transform ? "node_transform" : "unknown";
    const char* outcome = comparison.outcome == Outcome::match ? "match" :
                          comparison.outcome == Outcome::mismatch ? "mismatch" : "skip";
    std::string out = "{\"version\":1,\"routine\":\"";
    out += routine;
    out += "\",\"object\":" + std::to_string(snapshot.object);
    out += ",\"control_word\":" + std::to_string(snapshot.control_word);
    out += ",\"fp_tag\":" + std::to_string(snapshot.fp_tag);
    out += ",\"reserved\":" + std::to_string(snapshot.reserved);
    out += ",\"node_before\":";
    append_hex(out, snapshot.node_before);
    out += ",\"node_after\":";
    append_hex(out, snapshot.node_after);
    out += ",\"context_addresses\":";
    append_words(out, snapshot.context_addresses);
    out += ",\"context_before\":";
    append_words(out, snapshot.context_before);
    out += ",\"context_after\":";
    append_words(out, snapshot.context_after);
    out += ",\"actual_return\":" + std::to_string(snapshot.actual_return);
    out += ",\"expected_return\":" + std::to_string(comparison.expected_return);
    out += ",\"outcome\":\"";
    out += outcome;
    out += "\",\"reason\":\"";
    out += comparison.reason ? comparison.reason : "unknown";
    out += "\",\"mismatch_offset\":" + std::to_string(comparison.mismatch_offset);
    out += ",\"expected_word\":" + std::to_string(comparison.expected_word);
    out += ",\"actual_word\":" + std::to_string(comparison.actual_word);
    out.push_back('}');
    return out;
}

} // namespace a2fo::recovery_tests
