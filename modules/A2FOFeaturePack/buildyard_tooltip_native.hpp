#pragma once

// MSVCP60 ABI verified against Armada's imports and native call sites.
// See docs/buildyard-tooltip-abi.md. Never pass a MinGW std::string here.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace a2fo::buildyard_tooltip_native {

struct String {
    std::uint32_t allocator;
    char* data;
    std::uint32_t size;
    std::uint32_t capacity;
};
static_assert(sizeof(String) == 16 && offsetof(String, data) == 4 &&
              offsetof(String, size) == 8 && offsetof(String, capacity) == 12);

using Tidy = void (__attribute__((thiscall)) *)(String*, bool);
using Assign = String* (__attribute__((thiscall)) *)(String*, const char*, std::uint32_t);
using Insert = void* (__cdecl *)(void*, const String*);
using Snapshot = String* (__attribute__((thiscall)) *)(void*, String*);

struct Api {
    Tidy tidy = nullptr;
    Assign assign = nullptr;
    Insert insert = nullptr;
    Snapshot snapshot = nullptr;
};

class Temporary {
public:
    String value{};
    explicit Temporary(Tidy tidy) : tidy_(tidy) { tidy_(&value, false); }
    ~Temporary() { tidy_(&value, true); }
    Temporary(const Temporary&) = delete;
    Temporary& operator=(const Temporary&) = delete;
private:
    Tidy tidy_;
};

struct Result {
    bool verified = false;
    const char* reason = "native helpers unavailable";
    std::uint32_t before = 0;
    std::uint32_t after = 0;
};

// Literal insertion only: no localization or technology evaluation here.
// snapshot is basic_stringstream::str with an explicit hidden result pointer.
inline Result append_text(const Api& api, void* stream,
                          const char* text, std::uint32_t length) {
    Result result;
    if (!api.tidy || !api.assign || !api.insert || !api.snapshot) return result;
    if (!stream) { result.reason = "null stringstream"; return result; }
    // The native string insertion overload forwards to a C-string writer.
    if (!text || !length || std::memchr(text, '\0', length)) {
        result.reason = "empty or embedded-NUL literal";
        return result;
    }
    Temporary before(api.tidy), literal(api.tidy), after(api.tidy);
    if (api.snapshot(stream, &before.value) != &before.value) {
        result.reason = "str returned an unexpected result object";
        return result;
    }
    result.before = before.value.size;
    if (api.assign(&literal.value, text, length) != &literal.value ||
        literal.value.size != length || !literal.value.data ||
        std::memcmp(literal.value.data, text, length) != 0) {
        result.reason = "literal string construction failed";
        return result;
    }
    void* output = static_cast<unsigned char*>(stream) + 8;
    if (api.insert(output, &literal.value) != output) {
        result.reason = "operator<< returned an unexpected ostream";
        return result;
    }
    if (api.snapshot(stream, &after.value) != &after.value) {
        result.reason = "str readback returned an unexpected result object";
        return result;
    }
    result.after = after.value.size;
    if (result.before > UINT32_MAX - length || !after.value.data ||
        result.after != result.before + length ||
        (result.before && (!before.value.data ||
         std::memcmp(before.value.data, after.value.data, result.before) != 0)) ||
        std::memcmp(after.value.data + result.before, text, length) != 0) {
        result.reason = "stream readback differs from original plus literal";
        return result;
    }
    result.verified = true;
    result.reason = "original prefix and appended literal verified in native stream";
    return result;
}

} // namespace a2fo::buildyard_tooltip_native
