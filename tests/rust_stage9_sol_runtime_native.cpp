// Full existing native mock plus staged-only residual policy scenarios.
#define main stage8_solmaps_fixture_main
#include "rust_stage8_c_solmaps_mock.cpp"
#undef main

#include <array>
#include <cassert>
#include <float.h>
#include <vector>

namespace {
std::array<std::array<std::uint8_t, 32>, 64> extra_start_bytes{};
std::array<void*, 64> extra_starts{};
bool fail_allocate = false;

extern "C" void* __cdecl supplement_new(std::size_t size) {
    return fail_allocate ? nullptr : std::malloc(size);
}

extern "C" void* __attribute__((thiscall)) supplement_details_ctor(void* self) {
    std::memset(self, 0, kMapDetailsSize);
    void* start_array = extra_starts.data();
    std::memcpy(static_cast<std::uint8_t*>(self) + kMapDetailsStartLocationArrayOffset,
                &start_array, sizeof(start_array));
    g_fixture_details = self;
    return self;
}

void set_slots(std::int32_t slots) {
    std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada) + kGameSetupSlotCountRva,
                &slots, sizeof(slots));
}

void detail_scenarios() {
    for (std::size_t i = 0; i < extra_starts.size(); ++i)
        extra_starts[i] = extra_start_bytes[i].data();
    for (std::int32_t slots : {-1, 0, 1, 8, 64, 65}) {
        set_slots(slots);
        for (std::size_t starts : {std::size_t{0}, std::size_t{3}, std::size_t{70}}) {
            a2fo::sol::Map map{};
            map.filename = "fixture.sol";
            map.game_type = "Deathmatch";
            map.title = starts == 3 ? "Title" : "";
            map.start_locations.resize(starts);
            if (starts > 1) map.start_locations[1].type = "human";
            auto* details = static_cast<std::uint8_t*>(create_map_details(map, "bzn\\fixture.sol"));
            assert(details);
            std::uint32_t count = 0;
            std::memcpy(&count, details + kMapDetailsStartLocationCountOffset, sizeof(count));
            std::printf("supp-details:%ld:%lu:%lu\n", static_cast<long>(slots),
                        static_cast<unsigned long>(starts), static_cast<unsigned long>(count));
            std::free(details);
        }
    }
    fail_allocate = true;
    a2fo::sol::Map map{};
    std::printf("supp-alloc-fail:%d\n", create_map_details(map, "bzn\\fixture.sol") == nullptr);
    fail_allocate = false;
}

void precedence_scenarios(A2FO_ModuleApi& api) {
    char temporary[MAX_PATH]{};
    assert(GetTempPathA(MAX_PATH, temporary));
    g_fixture_roots[0] = join_path(temporary, "stage9_solroot0");
    g_fixture_roots[1] = join_path(temporary, "stage9_solroot1");
    for (const auto& root : g_fixture_roots) {
        CreateDirectoryA(root.c_str(), nullptr);
        CreateDirectoryA(join_path(root, "sol").c_str(), nullptr);
        CreateDirectoryA(join_path(root, "bzn").c_str(), nullptr);
    }
    const std::array<std::string, 6> paths{{
        join_path(join_path(g_fixture_roots[0], "sol"), "foo.sol"),
        join_path(join_path(g_fixture_roots[0], "bzn"), "foo.sol"),
        join_path(join_path(g_fixture_roots[1], "sol"), "foo.sol"),
        join_path(join_path(g_fixture_roots[1], "bzn"), "foo.sol"),
        join_path(join_path(g_fixture_roots[0], "sol"), "bar.sol"),
        join_path(join_path(g_fixture_roots[0], "bzn"), "bar.sol"),
    }};
    for (const auto& path : paths) fixture_file(path);
    api.extension_root_count = &fixture_root_count;
    api.extension_root = &fixture_root;
    g_api = &api;
    assert(build_source_index());
    for (const char* name : {"foo.sol", "bar.sol"}) {
        const auto& source = g_sources.at(name);
        std::printf("supp-winner:%s:%u:%d\n", name, source.root_index,
                    source.path.find("\\bzn\\") != std::string::npos);
    }
    for (const auto& path : paths) std::remove(path.c_str());
    for (const auto& root : g_fixture_roots) {
        RemoveDirectoryA(join_path(root, "sol").c_str());
        RemoveDirectoryA(join_path(root, "bzn").c_str());
        RemoveDirectoryA(root.c_str());
    }
}

void spawn_scenarios() {
    fixture_stub(kGameObjectClassFindRva, reinterpret_cast<void*>(&fixture_native_find));
    void* mission = g_fixture_spawned_object.data();
    fixture_stub(kAiMissionGetCurrentRva, reinterpret_cast<void*>(&fixture_native_current));
    g_fixture_mission_ptr = mission;
    void* object_list = g_fixture_start.data();
    std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada) + kAiMissionCurrentRva,
                &mission, sizeof(mission));
    std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada) + kGameObjectListPointerRva,
                &object_list, sizeof(object_list));
    g_fixture_vtable[6] = reinterpret_cast<void*>(&fixture_native_add);
    void* vtable = g_fixture_vtable;
    std::memcpy(g_fixture_spawned_object.data(), &vtable, sizeof(vtable));
    fixture_stub(kGameObjectClassConstructRva,
                 reinterpret_cast<void*>(&fixture_native_details_ctor));
    g_selected_map = {};
    g_selected_valid = true;
    for (int i = 0; i < 33; ++i) {
        a2fo::sol::Object object{}; object.odf = "good";
        if (i & 1) object.instance_name = "Named";
        g_selected_map.objects.push_back(object);
    }
    for (int i = 0; i < 17; ++i) {
        a2fo::sol::Object object{}; object.odf = "missing";
        g_selected_map.objects.push_back(object);
    }
    for (int i = 0; i < 17; ++i) {
        a2fo::sol::Object object{}; object.odf = "failed";
        g_selected_map.objects.push_back(object);
    }
    InterlockedExchange(&g_manual_spawn_state, 0);
    SolSpawnSummary summary{};
    std::printf("supp-spawn:%d\n", spawn_selected_sol_objects(&summary));
    std::printf("supp-summary:%u:%u:%u:%u:%u\n", summary.attempted,
                summary.created, summary.missing_class, summary.construct_failed,
                summary.mission_add_failed);
    InterlockedExchange(&g_manual_spawn_created, -1);
    InterlockedExchange(&g_manual_spawn_failed, -1);
    summary = {};
    std::printf("supp-repeat:%d\n", spawn_selected_sol_objects(&summary));
    std::printf("supp-clamped:%u:%u\n", summary.created, summary.construct_failed);

    // A nonnull native current slot satisfies setup, while the callable
    // getCurrent returns null: every successfully constructed object fails
    // publication through the actual add_object_to_current_mission path.
    InterlockedExchange(&g_manual_spawn_state, 0);
    g_fixture_mission_ptr = nullptr;
    summary = {};
    const bool no_mission = spawn_selected_sol_objects(&summary);
    assert(no_mission && summary.attempted == 67 && summary.created == 33 &&
           summary.missing_class == 17 && summary.construct_failed == 17 &&
           summary.mission_add_failed == 33);
    std::printf("supp-no-mission:%u:%u:%u:%u:%u:%u\n",
                static_cast<unsigned>(no_mission), summary.attempted,
                summary.created, summary.missing_class,
                summary.construct_failed, summary.mission_add_failed);
    g_fixture_mission_ptr = mission;
}

using BoundsEntry = bool (*)() noexcept;
BoundsEntry volatile bounds_entry = &apply_sol_world_bounds;

std::uint16_t bounds_status() {
    std::uint16_t status;
    __asm__ __volatile__("fnstsw %0" : "=am"(status));
    return status;
}

std::uint16_t bounds_control() {
    std::uint16_t control;
    __asm__ __volatile__("fnstcw %0" : "=m"(control));
    return control;
}

std::uint32_t next_word(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

struct BoundsOutcome {
    bool published;
    std::array<std::uint32_t, 6> words;
    std::uint16_t control;
    std::uint16_t top;
    std::uint16_t flags;
};

constexpr std::array<std::uint32_t, 6> kBoundsSentinel{{
    0xdeadbeefu, 0x12345678u, 0x87654321u,
    0xcafef00du, 0x0badf00du, 0x13579bdfu}};

BoundsOutcome call_bounds(const std::array<std::uint32_t, 6>& input) {
    static_assert(sizeof(a2fo::sol::Vec3) == 3 * sizeof(std::uint32_t));
    std::memcpy(static_cast<void*>(&g_selected_map.minimum_extent), input.data(),
                3 * sizeof(std::uint32_t));
    std::memcpy(static_cast<void*>(&g_selected_map.size), input.data() + 3,
                3 * sizeof(std::uint32_t));
    auto* image = static_cast<std::uint8_t*>(g_fixture_armada);
    std::memcpy(image + kWorldMinimumExtentRva, kBoundsSentinel.data(), 3 * sizeof(std::uint32_t));
    std::memcpy(image + kWorldMaximumExtentRva, kBoundsSentinel.data() + 3,
                3 * sizeof(std::uint32_t));
    const std::uint16_t before_control = bounds_control();
    const std::uint16_t before_top = static_cast<std::uint16_t>((bounds_status() >> 11) & 7);
    __asm__ __volatile__("fnclex");
    const bool published = bounds_entry();
    const std::uint16_t status = bounds_status();
    const std::uint16_t control = bounds_control();
    const std::uint16_t top = static_cast<std::uint16_t>((status >> 11) & 7);
    assert(control == before_control && top == before_top);
    std::array<std::uint32_t, 6> output{};
    std::memcpy(output.data(), image + kWorldMinimumExtentRva,
                3 * sizeof(std::uint32_t));
    std::memcpy(output.data() + 3, image + kWorldMaximumExtentRva,
                3 * sizeof(std::uint32_t));
    return {published, output, control, top,
            static_cast<std::uint16_t>(status & 0x3f)};
}

void bounds_case(const char* mode, unsigned number,
                 const std::array<std::uint32_t, 6>& input,
                 bool expected_published) {
    const BoundsOutcome result = call_bounds(input);
    assert(result.published == expected_published);
    std::printf("bounds:%s:%u in=%08x,%08x,%08x,%08x,%08x,%08x "
                "out=%08x,%08x,%08x,%08x,%08x,%08x ok=%u cw=%04x top=%u flags=%02x\n",
                mode, number,
                input[0], input[1], input[2], input[3], input[4], input[5],
                result.words[0], result.words[1], result.words[2],
                result.words[3], result.words[4], result.words[5],
                static_cast<unsigned>(result.published), result.control,
                result.top, result.flags);
}

void bounds_scenarios() {
    const unsigned old_control = _controlfp(0, 0);
    struct Mode { const char* name; unsigned value; };
    const Mode precisions[] = {{"pc24", _PC_24}, {"pc53", _PC_53}, {"pc64", _PC_64}};
    const Mode roundings[] = {{"near", _RC_NEAR}, {"down", _RC_DOWN},
                              {"up", _RC_UP}, {"chop", _RC_CHOP}};
    const std::array<std::array<std::uint32_t, 6>, 7> known{{
        {{0, 0, 0, 0, 0, 0}},
        {{0x80000000u, 0, 0x80000000u, 0, 0x80000000u, 0}},
        {{0x3f800000u, 0x40000000u, 0x40400000u,
          0x3f800000u, 0x3f800000u, 0x3f800000u}},
        {{1, 0x80000001u, 0x007fffffu, 1, 1, 0x807fffffu}},
        {{0x7fc00001u, 0, 0, 0x3f800000u, 0, 0}},
        {{0x7f800001u, 0, 0, 0x3f800000u, 0, 0}},
        {{0x7f800000u, 0xff800000u, 0, 0xff800000u, 0x7f800000u, 0}},
    }};
    for (const auto& precision : precisions) {
        for (const auto& rounding : roundings) {
            _controlfp(precision.value | rounding.value, _MCW_PC | _MCW_RC);
            const std::string mode = std::string(precision.name) + "/" + rounding.name;
            unsigned number = 0;
            g_selected_valid = true;
            for (const auto& input : known)
                bounds_case(mode.c_str(), number++, input, true);
            std::uint32_t state = 0x9e3779b9u;
            for (unsigned i = 0; i < 512; ++i) {
                std::array<std::uint32_t, 6> input{};
                for (auto& word : input) word = next_word(state);
                bounds_case(mode.c_str(), number++, input, true);
            }
            assert(number == 519);
        }
    }
    _controlfp(old_control, _MCW_PC | _MCW_RC);

    const std::array<std::uint32_t, 6> input{{1, 2, 3, 4, 5, 6}};
    g_selected_valid = false;
    const auto disabled = call_bounds(input);
    assert(!disabled.published && disabled.words == kBoundsSentinel);
    std::printf("bounds-guard:disabled:%u:%08x:%08x\n",
                static_cast<unsigned>(disabled.published), disabled.words[0], disabled.words[3]);
    g_selected_valid = true;
    HMODULE saved_armada = g_armada;
    g_armada = nullptr;
    const auto absent = call_bounds(input);
    assert(!absent.published && absent.words == kBoundsSentinel);
    std::printf("bounds-guard:null-armada:%u:%08x:%08x\n",
                static_cast<unsigned>(absent.published), absent.words[0], absent.words[3]);
    g_armada = saved_armada;

    auto* first_output = static_cast<std::uint8_t*>(g_fixture_armada) +
                         kWorldMinimumExtentRva;
    DWORD old_protection = 0;
    assert(VirtualProtect(first_output, 6 * sizeof(std::uint32_t),
                          PAGE_READONLY, &old_protection));
    const std::uint16_t before_control = bounds_control();
    const std::uint16_t before_top = static_cast<std::uint16_t>((bounds_status() >> 11) & 7);
    __asm__ __volatile__("fnclex");
    const bool readonly = bounds_entry();
    const std::uint16_t status = bounds_status();
    assert(!readonly && bounds_control() == before_control &&
           static_cast<std::uint16_t>((status >> 11) & 7) == before_top);
    std::array<std::uint32_t, 6> unchanged{};
    std::memcpy(unchanged.data(), first_output, unchanged.size() * sizeof(std::uint32_t));
    assert(unchanged == kBoundsSentinel);
    DWORD restored = 0;
    assert(VirtualProtect(first_output, 6 * sizeof(std::uint32_t),
                          old_protection, &restored));
    std::printf("bounds-guard:readonly:%u:%08x:%08x:flags=%02x\n",
                static_cast<unsigned>(readonly), unchanged[0], unchanged[3],
                static_cast<unsigned>(status & 0x3f));
}
#include "rust_stage9_sol_discovery_cases.inl"
#include "rust_stage9_sol_completion_cases.inl"
} // namespace

int main() {
    const int baseline = stage8_solmaps_fixture_main();
    if (baseline) return baseline;
    g_fixture_armada = VirtualAlloc(nullptr, 0x400000, MEM_RESERVE | MEM_COMMIT,
                                    PAGE_EXECUTE_READWRITE);
    assert(g_fixture_armada);
    g_armada = static_cast<HMODULE>(g_fixture_armada);
    A2FO_ModuleApi api{};
    api.log = &fixture_log;
    g_api = &api;
    fixture_stub(kOperatorNewRva, reinterpret_cast<void*>(&supplement_new));
    fixture_stub(kMapDetailsConstructorRva,
                 reinterpret_cast<void*>(&supplement_details_ctor));
    void* assign = reinterpret_cast<void*>(&fixture_native_assign);
    std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada) + kStdStringAssignCharIatRva,
                &assign, sizeof(assign));
    detail_scenarios();
    precedence_scenarios(api);
    discovery_scenarios();
    sol_completion_scenarios();
    spawn_scenarios();
    bounds_scenarios();
    g_api = nullptr;
    g_armada = nullptr;
    VirtualFree(g_fixture_armada, 0, MEM_RELEASE);
    g_fixture_armada = nullptr;
}
