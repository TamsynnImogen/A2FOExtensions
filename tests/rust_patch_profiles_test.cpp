// Headless callback-memory contract for the three historical patch profiles.
// The fixed arena is never marked executable or entered as code.
#include "../rust/native_hooks_ffi.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
alignas(16) std::uint8_t arena[256];
constexpr std::size_t span = 6;
constexpr std::uint8_t prologue[span] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
constexpr std::uint8_t replacement[span] = {0x90, 0x90, 0x90, 0xcc, 0xcc, 0xcc};
constexpr std::uint8_t raw_bytes[span] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
std::uint8_t sentinel_storage = 0;
std::uint8_t publish_context = 0;
void* published = nullptr;
bool callback_valid = true;
bool fail_first_protect = false;
bool fail_restore = false;
bool mutate_on_protect = false;
bool fail_alloc = false;
bool mutate_on_alloc = false;
unsigned protect_calls = 0;
unsigned error_calls = 0;
unsigned cases = 0;
std::uint64_t digest = 14695981039346656037ull;

struct Event { char kind; unsigned slot; std::size_t size; std::uint32_t flags; };
Event events[32]{};
unsigned event_count = 0;

std::uint8_t* target() { return arena; }
std::uint8_t* gateway() { return arena + 64; }
std::uint8_t* destination() { return arena + 128; }
unsigned slot(const void* pointer) {
    if (pointer == target()) return 0;
    if (pointer == gateway()) return 1;
    if (pointer == destination()) return 2;
    if (!pointer) return 3;
    callback_valid = false;
    return 4;
}
void event(char kind, const void* pointer, std::size_t size = 0,
           std::uint32_t flags = 0) {
    if (event_count == 32) { callback_valid = false; return; }
    events[event_count++] = {kind, slot(pointer), size, flags};
}
extern "C" std::int32_t protect(void* address, std::size_t n,
                                std::uint32_t flags, std::uint32_t* old) {
    const bool first = flags == 0x40;
    event(first ? 'P' : 'R', address, n, flags);
    callback_valid &= address == target() && n == span && old != nullptr;
    if (old) *old = 0x20;
    if (first) {
        ++protect_calls;
        if (mutate_on_protect) target()[0] ^= 1;
        return fail_first_protect ? 0 : 1;
    }
    callback_valid &= flags == 0x20 && protect_calls == 1;
    return fail_restore ? 0 : 1;
}
extern "C" void* allocate(std::size_t n) {
    event('A', nullptr, n);
    callback_valid &= n == span + 5;
    if (mutate_on_alloc) target()[0] ^= 1;
    return fail_alloc ? nullptr : gateway();
}
extern "C" void free_memory(void* address) {
    event('D', address);
    callback_valid &= address == gateway();
}
extern "C" void flush(void* address, std::size_t n) {
    event('F', address, n);
    callback_valid &= (address == target() && n == span) ||
                      (address == gateway() && n == span + 5);
}
extern "C" std::uint32_t last_error() {
    ++error_calls;
    event('E', nullptr);
    return 1234;
}
extern "C" void publish(void* context, void* value) {
    event(value ? 'U' : 'N', value);
    callback_valid &= context == &publish_context &&
                      (value == gateway() || value == nullptr);
    published = value;
}
const A2foRsHookMemory api{protect, allocate, free_memory, flush};

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "case %u: %s\n", cases + 1, message);
        std::exit(1);
    }
}
void hash_word(std::uint32_t value) {
    for (unsigned shift = 0; shift != 32; shift += 8) {
        digest ^= static_cast<std::uint8_t>(value >> shift);
        digest *= 1099511628211ull;
    }
}
void reset() {
    std::memset(arena, 0xcc, sizeof(arena));
    std::memcpy(target(), prologue, span);
    std::memset(events, 0, sizeof(events));
    event_count = 0;
    callback_valid = true;
    fail_first_protect = fail_restore = mutate_on_protect = false;
    fail_alloc = mutate_on_alloc = false;
    protect_calls = error_calls = 0;
    published = &sentinel_storage;
}
void finish(const char* sequence, std::uint32_t result) {
    require(callback_valid, "callback argument contract");
    const std::size_t count = std::strlen(sequence);
    require(event_count == count, "event count");
    for (unsigned i = 0; i < event_count; ++i) {
        require(events[i].kind == sequence[i], "event order");
        hash_word(static_cast<unsigned char>(events[i].kind));
        hash_word(events[i].slot);
        hash_word(static_cast<std::uint32_t>(events[i].size));
        hash_word(events[i].flags);
    }
    for (unsigned i = 0; i < span; ++i) hash_word(target()[i]);
    for (unsigned i = 0; i < span + 5; ++i) hash_word(gateway()[i]);
    hash_word(result);
    ++cases;
}
std::int32_t displacement(const std::uint8_t* from, const std::uint8_t* to) {
    return static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(to) -
                                     reinterpret_cast<std::intptr_t>(from + 5));
}
void check_jump(const std::uint8_t* from, const std::uint8_t* to) {
    require(from[0] == 0xe9, "jump opcode");
    std::int32_t actual = 0;
    std::memcpy(&actual, from + 1, sizeof(actual));
    require(actual == displacement(from, to), "jump displacement");
}

void report_cases() {
    A2foRsPatchReport out{};
    reset();
    require(a2fo_rs_hook_write_report(target(), raw_bytes, span, &out, &api,
                                      last_error) == 1, "report success result");
    require(out.written == 1 && out.restored == 1 && out.restore_error == 0 &&
            std::memcmp(target(), raw_bytes, span) == 0 && error_calls == 0,
            "report success state");
    finish("PFR", out.written);

    reset(); fail_restore = true; out = {9, 9, 9};
    require(a2fo_rs_hook_write_report(target(), raw_bytes, span, &out, &api,
                                      last_error) == 1, "report restore failure result");
    require(out.written == 1 && out.restored == 0 && out.restore_error == 1234 &&
            std::memcmp(target(), raw_bytes, span) == 0 && error_calls == 1,
            "report restore failure state");
    finish("PFRE", out.restore_error);

    reset(); fail_first_protect = true; out = {9, 9, 9};
    require(a2fo_rs_hook_write_report(target(), raw_bytes, span, &out, &api,
                                      last_error) == 0, "report first protect failure result");
    require(out.written == 0 && out.restored == 0 && out.restore_error == 0 &&
            std::memcmp(target(), prologue, span) == 0 && error_calls == 0,
            "report first protect failure state");
    finish("P", out.written);

    reset();
    require(a2fo_rs_hook_write_report(target(), raw_bytes, span, nullptr, &api,
                                      last_error) == 0, "report null out guard");
    finish("", 0);
}

void double_cases() {
    reset(); target()[0] ^= 1;
    require(a2fo_rs_hook_patch_double(target(), replacement, prologue, span, &api) == 0,
            "double initial mismatch result");
    require(protect_calls == 0, "double mismatch has no callbacks");
    finish("", 0);

    reset(); mutate_on_protect = true;
    require(a2fo_rs_hook_patch_double(target(), replacement, prologue, span, &api) == 0,
            "double second check result");
    require(target()[0] == (prologue[0] ^ 1) &&
            std::memcmp(target() + 1, prologue + 1, span - 1) == 0,
            "double mutation does not write");
    finish("PR", 0);

    reset(); fail_restore = true;
    require(a2fo_rs_hook_patch_double(target(), replacement, prologue, span, &api) == 1,
            "double ignores restore failure");
    require(std::memcmp(target(), replacement, span) == 0 && error_calls == 0,
            "double success state");
    finish("PFR", 1);

    reset(); fail_first_protect = true;
    require(a2fo_rs_hook_patch_double(target(), replacement, prologue, span, &api) == 0,
            "double first protect failure");
    require(std::memcmp(target(), prologue, span) == 0, "double failure preserves target");
    finish("P", 0);
}

void video_cases() {
    A2foRsVideoInstall out{};
    reset(); target()[0] ^= 1; out = {gateway(), 9};
    require(a2fo_rs_hook_video_install(target(), destination(), span, prologue,
                                       &out, &api, &publish_context, publish) == 0,
            "video signature failure result");
    require(out.gateway == nullptr && out.status == 1 &&
            published == &sentinel_storage, "video signature failure state");
    finish("", out.status);

    reset(); fail_alloc = true; out = {gateway(), 9};
    require(a2fo_rs_hook_video_install(target(), destination(), span, prologue,
                                       &out, &api, &publish_context, publish) == 0,
            "video allocation failure result");
    require(out.gateway == nullptr && out.status == 2 &&
            published == &sentinel_storage &&
            std::memcmp(target(), prologue, span) == 0,
            "video allocation failure state");
    finish("A", out.status);

    reset();
    require(a2fo_rs_hook_video_install(target(), destination(), span, prologue,
                                       &out, &api, &publish_context, publish) == 1,
            "video success result");
    require(out.gateway == gateway() && out.status == 0 && published == gateway(),
            "video publication");
    require(std::memcmp(gateway(), prologue, span) == 0 &&
            gateway()[span] == 0xe9, "video copied prologue");
    check_jump(gateway() + span, target() + span);
    check_jump(target(), destination());
    require(target()[5] == 0x90 && error_calls == 0, "video target padding");
    finish("AFUPFR", out.status);

    reset(); fail_first_protect = true;
    require(a2fo_rs_hook_video_install(target(), destination(), span, prologue,
                                       &out, &api, &publish_context, publish) == 0,
            "video target patch failure result");
    require(out.gateway == nullptr && out.status == 3 && published == nullptr &&
            std::memcmp(target(), prologue, span) == 0,
            "video target patch failure state");
    finish("AFUPDN", out.status);

    reset(); fail_restore = true;
    require(a2fo_rs_hook_video_install(target(), destination(), span, prologue,
                                       &out, &api, &publish_context, publish) == 1,
            "video restore failure remains installed");
    require(out.gateway == gateway() && out.status == 0 && published == gateway() &&
            error_calls == 0, "video restore failure publication");
    check_jump(target(), destination());
    finish("AFUPFR", out.status);

    reset(); mutate_on_alloc = true;
    require(a2fo_rs_hook_video_install(target(), destination(), span, prologue,
                                       &out, &api, &publish_context, publish) == 1,
            "video allocation mutation is not rechecked");
    require(out.gateway == gateway() && out.status == 0 &&
            gateway()[0] == (prologue[0] ^ 1) &&
            std::memcmp(gateway() + 1, prologue + 1, span - 1) == 0,
            "video copies current prologue after allocation");
    check_jump(target(), destination());
    finish("AFUPFR", out.status);
}
} // namespace

int main() {
    report_cases();
    double_cases();
    video_cases();
    std::printf("patch-profiles cases=%u digest=%016llx\n", cases,
                static_cast<unsigned long long>(digest));
    return 0;
}
