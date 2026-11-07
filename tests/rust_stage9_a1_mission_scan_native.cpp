// Actual complete A1 unit; synthetic reader/image, never a game process.
#define serialized_a1_mission_record_at fixture_mission_record_at
#include A1_SOURCE
#undef serialized_a1_mission_record_at
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace a1compat {
bool serialized_a1_mission_record_at(const std::uint8_t*, const std::uint8_t*) noexcept;
}
namespace {
std::uint8_t* stream_base;
std::uint8_t* reader_bytes;
std::size_t stream_size;
std::uint32_t probe_count, last_probe, probe_hash;
std::uint32_t native_calls, native_offset;
bool native_result = true;
std::uint32_t mutate_at = UINT32_MAX, mutate_marker = UINT32_MAX;
std::uintptr_t expected_end;
unsigned log_count;

template<class T> void put(std::uint8_t* p, std::size_t offset, T value) {
    std::memcpy(p + offset, &value, sizeof(value));
}
void marker(std::uint32_t offset, const char* name = "EmptyMission") {
    assert(512u + offset + 48u <= stream_size);
    auto* p = stream_base + 512 + offset;
    std::memset(p, 0, 48);
    put(p, 0, std::uint32_t{0x8174f002});
    put(p, 4, std::uint32_t{40});
    std::memcpy(p + 8, name, std::strlen(name));
}
void A2FO_CALL fixture_log(const char*, const char* message) {
    ++log_count;
    std::printf("log:%s\n", message);
}
bool __cdecl fixture_native_load(void* reader) {
    ++native_calls;
    assert(reader == reader_bytes);
    std::uintptr_t cursor = 0;
    std::memcpy(&cursor, reader_bytes + 0x54, sizeof(cursor));
    native_offset = static_cast<std::uint32_t>(cursor -
        reinterpret_cast<std::uintptr_t>(stream_base));
    return native_result;
}
void stub(void* image, std::uintptr_t offset, void* destination) {
    // Only this fixture's allocated RWX image is modified.
    std::uint8_t code[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    const auto pointer = reinterpret_cast<std::uintptr_t>(destination);
    static_assert(sizeof(pointer) == 4);
    std::memcpy(code + 1, &pointer, sizeof(pointer));
    std::memcpy(static_cast<std::uint8_t*>(image) + offset, code, sizeof(code));
    assert(FlushInstructionCache(GetCurrentProcess(),
        static_cast<std::uint8_t*>(image) + offset, sizeof(code)));
}
void run_case(const char* label, std::uint32_t remaining,
              std::initializer_list<std::uint32_t> hits,
              std::uint32_t expected_offset, bool read_only = false,
              bool valid_header = true, bool malformed_cursor = false,
              bool result = true, bool campaign = false,
              std::uint32_t injection_at = UINT32_MAX,
              std::uint32_t injection_marker = UINT32_MAX) {
    std::vector<std::uint8_t> bytes(std::max<std::size_t>(1024, 512u + remaining + 48u), 0xcc);
    const std::string prefix = "version [1] =\r\n2053\r\nsaveGameDesc = " +
        std::string(256, 'A') + "\r\nbinarySave [1] =\r\ntrue\r\n";
    std::memcpy(bytes.data(), prefix.data(), prefix.size());
    stream_base = bytes.data(); stream_size = bytes.size();
    for (auto hit : hits) marker(hit, campaign ? "Inst4XMission" : "EmptyMission");
    std::memset(reader_bytes, 0, 4096);
    reader_bytes[6] = valid_header ? 1 : 0; reader_bytes[7] = 1;
    put(reader_bytes, 0x08, std::uint32_t{2053});
    put(reader_bytes, 0x0c, reinterpret_cast<std::uintptr_t>(stream_base));
    put(reader_bytes, 0x10, static_cast<std::uint32_t>(stream_size));
    put(reader_bytes, 0x54, reinterpret_cast<std::uintptr_t>(stream_base + 512));
    expected_end = reinterpret_cast<std::uintptr_t>(stream_base + 512 + remaining);
    put(reader_bytes, 0x58, malformed_cursor ?
        reinterpret_cast<std::uintptr_t>(stream_base + 511) : expected_end);
    probe_count = last_probe = native_calls = log_count = 0;
    probe_hash = 2166136261u; native_offset = UINT32_MAX;
    mutate_at = injection_at; mutate_marker = injection_marker;
    native_result = result;
    InterlockedExchange(&g_a1_bzn_ai_mission_load_count, 0);
    DWORD previous = 0;
    if (read_only) assert(VirtualProtect(reader_bytes, 4096, PAGE_READONLY, &previous));
    const auto loaded = load_ai_mission(reader_bytes);
    if (read_only) { DWORD ignored = 0; assert(VirtualProtect(reader_bytes, 4096, previous, &ignored)); }
    assert(loaded == (result ? 1u : 0u));
    assert(native_calls == 1 && native_offset == expected_offset);
    std::uintptr_t cursor = 0;
    std::memcpy(&cursor, reader_bytes + 0x54, sizeof(cursor));
    assert(cursor - reinterpret_cast<std::uintptr_t>(stream_base) == expected_offset);
    std::printf("case:%s:loaded=%u:native=%u:cursor=%u:probes=%u:last=%u:hash=%08x:logs=%u:counter=%ld\n",
        label, static_cast<unsigned>(loaded), native_calls, native_offset,
        probe_count, last_probe, probe_hash, log_count,
        static_cast<long>(g_a1_bzn_ai_mission_load_count));
    assert(valid_header && !malformed_cursor ? probe_count > 0 : probe_count == 0);
}
} // namespace

namespace a1compat {
bool fixture_mission_record_at(const std::uint8_t* candidate,
                               const std::uint8_t* end) noexcept {
    assert(reinterpret_cast<std::uintptr_t>(end) == expected_end);
    const auto distance = static_cast<std::uint32_t>(candidate - stream_base - 512);
    ++probe_count; last_probe = distance;
    probe_hash = (probe_hash * 16777619u) ^ distance;
    if (distance == mutate_at) {
        marker(mutate_marker);
        mutate_at = UINT32_MAX;
    }
    return serialized_a1_mission_record_at(candidate, end);
}
}

int main() {
    static_assert(sizeof(std::uintptr_t) == 4);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    auto* image = VirtualAlloc(nullptr, 0x400000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    reader_bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(image && reader_bytes);
    g_armada = static_cast<HMODULE>(image);
    stub(image, kAiMissionLoadRva, reinterpret_cast<void*>(&fixture_native_load));
    A2FO_ModuleApi api{}; api.log = &fixture_log; g_api = &api;
    assert(load_ai_mission(nullptr) == 0);
    g_armada = nullptr; assert(load_ai_mission(reader_bytes) == 0); g_armada = static_cast<HMODULE>(image);
    run_case("header-native-fallback", 200, {8}, 512, false, false);
    run_case("malformed-cursor", 200, {8}, 512, false, true, true);
    run_case("empty", 0, {}, 512);
    run_case("short-record", 47, {0}, 512);
    run_case("initial", 48, {0}, 512);
    run_case("initial-short-circuit", 200, {0, 64}, 512);
    run_case("first-distance", 49, {1}, 513);
    run_case("last-distance", 80, {32}, 544);
    run_case("partial-after-boundary", 80, {33}, 512);
    run_case("no-marker", 200, {}, 512);
    run_case("ambiguous", 200, {8, 64}, 512);
    run_case("read-only-slot", 200, {8}, 512, true);
    run_case("native-load-fails", 200, {8}, 520, false, true, false, false);
    run_case("campaign-record", 200, {8}, 520, false, true, false, true, true);
    run_case("mutation-new-marker", 200, {}, 521, false, true, false, true, false, 2, 9);
    run_case("mutation-second-marker", 200, {64}, 512, false, true, false, true, false, 2, 9);
    run_case("cap-last-complete", 1024u * 1024u + 64u, {1024u * 1024u - 48u}, 512u + 1024u * 1024u - 48u);
    run_case("cap-excluded", 1024u * 1024u + 64u, {1024u * 1024u - 47u}, 512);
    g_api = nullptr; g_armada = nullptr;
    VirtualFree(reader_bytes, 0, MEM_RELEASE); VirtualFree(image, 0, MEM_RELEASE);
    std::puts("A1 mission actual-source scope:18 scenarios +null guards PASS");
}
