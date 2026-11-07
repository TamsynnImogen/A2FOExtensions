// Headless x86 tests of the native attached-yard selection policy.
#include "../modules/A2FOSquadrons/module.cpp"

#include <cassert>
#include <iostream>

#ifdef NDEBUG
#error "This regression requires active assertions"
#endif

namespace {
std::unordered_map<std::uint32_t, void*>* entity_map = nullptr;
std::vector<std::uint32_t> native_calls;
bool refuse_native_add = false;

template <typename T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

struct Object {
    std::array<std::uint8_t, 0x300> bytes{};
    Object(std::uint32_t handle, void* klass, int team, void* vtable = nullptr) {
        put(bytes.data(), 0, vtable);
        put(bytes.data(), kObjectHandleOffset, handle);
        put(bytes.data(), kObjectClassOffset, klass);
        put(bytes.data(), kObjectTeamOffset, team);
    }
    void* get() { return bytes.data(); }
    void expired(bool value) {
        put(bytes.data(), kObjectExpiredOffset, static_cast<std::uint8_t>(value));
    }
};

void* __cdecl entity(std::uint32_t handle) {
    if (!entity_map) return nullptr;
    const auto it = entity_map->find(handle);
    return it == entity_map->end() ? nullptr : it->second;
}

void jump_to(void* address, void* target) {
    auto* code = static_cast<std::uint8_t*>(address);
    code[0] = 0xe9;
    put(code, 1, static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(target) -
        reinterpret_cast<std::uintptr_t>(code + 5)));
    FlushInstructionCache(GetCurrentProcess(), code, 5);
}

void __stdcall native_select(void* selection, void* craft, std::int32_t mode,
                             std::int32_t clear, std::int32_t, std::int32_t) {
    native_calls.push_back(object_handle(craft));
    if (refuse_native_add && mode == 0) return;
    auto count = read_at<std::uint32_t>(selection, kNativeSelectionCountOffset, 0);
    auto* handles = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(selection) + kNativeSelectionHandlesOffset);
    if (clear) {
        for (std::uint32_t i = 0; i < count; ++i) {
            if (void* member = entity(handles[i]))
                put(member, kNativeSelectedFlagOffset, std::uint8_t{0});
        }
        count = 0;
    }
    if (mode == 2)
        mode = read_at<std::uint8_t>(craft, kNativeSelectedFlagOffset, 0) ? 1 : 0;
    const auto handle = object_handle(craft);
    auto* found = std::find(handles, handles + count, handle);
    if (mode == 0 && found == handles + count) {
        assert(count < kNativeSelectionLimit);
        handles[count++] = handle;
        put(craft, kNativeSelectedFlagOffset, std::uint8_t{1});
    } else if (mode == 1 && found != handles + count) {
        std::move(found + 1, handles + count, found);
        --count;
        put(craft, kNativeSelectedFlagOffset, std::uint8_t{0});
    }
    put(selection, kNativeSelectionCountOffset, count);
}

void selection_tests(void* root, void* a, void* b,
                     std::unordered_map<std::uint32_t, void*>& live) {
    std::array<std::uint8_t, 0x500> selection{};
    entity_map = &live;
    jump_to(at(kEntityGetRva), reinterpret_cast<void*>(&entity));
    g_selection_add_hook.gateway = reinterpret_cast<void*>(&native_select);
    g_ui_selection_vtable = reinterpret_cast<void*>(1);
    g_runtime_ready = true;

    selection_add_hook(selection.data(), a, 0, 1, 0, 1);
    assert((native_calls == std::vector<std::uint32_t>{2, 1, 3}));
    assert(read_at<std::uint32_t>(selection.data(), kNativeSelectionCountOffset) == 3);
    native_calls.clear();
    selection_add_hook(selection.data(), root, 2, 0, 0, 1);
    assert((native_calls == std::vector<std::uint32_t>{1, 2, 3}));
    assert(read_at<std::uint32_t>(selection.data(), kNativeSelectionCountOffset) == 0);
    native_calls.clear();
    selection_add_hook(selection.data(), b, 2, 1, 0, 1);
    assert((native_calls == std::vector<std::uint32_t>{3, 1, 2}));
    assert(read_at<std::uint32_t>(selection.data(), kNativeSelectionCountOffset) == 3);

    // Mixed selection with only one free slot must remain unchanged.
    for (std::uint32_t i = 0; i < 29; ++i)
        put(selection.data(), kNativeSelectionHandlesOffset + i * 4, i + 100);
    put(selection.data(), kNativeSelectionCountOffset, std::uint32_t{29});
    put(root, kNativeSelectedFlagOffset, std::uint8_t{0});
    put(a, kNativeSelectedFlagOffset, std::uint8_t{0});
    put(b, kNativeSelectedFlagOffset, std::uint8_t{0});
    native_calls.clear();
    selection_add_hook(selection.data(), root, 0, 0, 0, 1);
    assert(native_calls.empty());
    assert(read_at<std::uint32_t>(selection.data(), kNativeSelectionCountOffset) == 29);
    selection_add_hook(selection.data(), root, 0, 1, 0, 1);
    assert((native_calls == std::vector<std::uint32_t>{1, 2, 3}));
    assert(read_at<std::uint32_t>(selection.data(), kNativeSelectionCountOffset) == 3);
    put(selection.data(), kNativeSelectionCountOffset, std::uint32_t{0});
    put(root, kNativeSelectedFlagOffset, std::uint8_t{0});
    native_calls.clear();
    refuse_native_add = true;
    selection_add_hook(selection.data(), root, 0, 0, 0, 1);
    refuse_native_add = false;
    assert((native_calls == std::vector<std::uint32_t>{1}));
    assert(read_at<std::uint32_t>(selection.data(), kNativeSelectionCountOffset) == 0);
    entity_map = nullptr;
    g_selection_add_hook.gateway = nullptr;
    g_ui_selection_vtable = nullptr;
    g_runtime_ready = false;
    std::cout << "PASS native selection expansion, toggle, clear, capacity preflight, and native refusal\n";
}

void expect(void* clicked, const std::vector<std::uint32_t>& expected,
            const std::unordered_map<std::uint32_t, void*>& live) {
    const auto peers = attached_yard_peers(clicked, [&](std::uint32_t handle) -> void* {
        const auto it = live.find(handle);
        return it == live.end() ? nullptr : it->second;
    });
    assert(peers == expected);
}

void policy_tests() {
    g_armada = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x2b4000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(g_armada);
    const auto station_vtable = at(kResearchStationVtableRva);
    int research_class = 0, yard_class = 0, other_class = 0;
    Object root(1, &research_class, 1, station_vtable);
    Object a(2, &yard_class, 1), b(3, &yard_class, 1);
    Object other_root(4, &research_class, 1, station_vtable);
    Object c(5, &yard_class, 1), unrelated(6, &other_class, 1);
    std::array<void*, 3> first_slots{a.get(), b.get(), unrelated.get()};
    std::array<void*, 1> second_slots{c.get()};
    put(root.get(), kStationPodCountOffset, std::uint32_t{first_slots.size()});
    put(root.get(), kStationPodsOffset, first_slots.data());
    put(other_root.get(), kStationPodCountOffset, std::uint32_t{second_slots.size()});
    put(other_root.get(), kStationPodsOffset, second_slots.data());
    std::unordered_map<std::uint32_t, void*> live{{1, root.get()}, {2, a.get()},
        {3, b.get()}, {4, other_root.get()}, {5, c.get()}, {6, unrelated.get()}};

    g_pod_group_layout_ready = false;
    capture_pod_group_class(&research_class, "research", {{"selectPodsTogether", "1"}});
    capture_pod_group_class(&yard_class, "shipyard", {});
    assert(g_pod_group_classes.empty());
    expect(root.get(), {}, live);
    g_pod_group_layout_ready = true;
    capture_pod_group_class(&research_class, "research", {{"selectPodsTogether", "0"}});
    assert(g_pod_group_classes.empty());
    capture_pod_group_class(&research_class, "research", {{"selectPodsTogether", "1"}});
    assert(is_pod_group_root(root.get()));
    track_pod_group_root(root.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    track_pod_group_root(other_root.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    expect(root.get(), {2, 3}, live);
    expect(a.get(), {1, 3}, live);
    expect(b.get(), {1, 2}, live);
    expect(other_root.get(), {5}, live);
    expect(c.get(), {4}, live);
    expect(unrelated.get(), {}, live);
    std::cout << "PASS opt-in, root/child peers, unrelated class, and two separate roots\n";

    b.expired(true);
    expect(root.get(), {2}, live);
    b.expired(false);
    put(b.get(), kObjectTeamOffset, 2);
    expect(root.get(), {2}, live);
    put(b.get(), kObjectTeamOffset, 1);
    live.erase(3);
    expect(root.get(), {2}, live);
    live[3] = b.get();
    put(b.get(), kObjectHandleOffset, std::uint32_t{9});
    expect(root.get(), {2}, live);
    put(b.get(), kObjectHandleOffset, std::uint32_t{3});
    first_slots[1] = nullptr;
    expect(root.get(), {2}, live);
    first_slots[1] = b.get();
    std::cout << "PASS null, expired, captured, missing and stale-identity children\n";

    put(root.get(), 0, static_cast<void*>(nullptr));
    expect(a.get(), {}, live);
    put(root.get(), 0, station_vtable);
    put(root.get(), kStationPodCountOffset, kMaxStationPodSlots + 1);
    expect(root.get(), {}, live);
    put(root.get(), kStationPodCountOffset, std::uint32_t{3});
    put(root.get(), kStationPodsOffset, static_cast<void*>(nullptr));
    expect(root.get(), {}, live);
    put(root.get(), kStationPodsOffset, first_slots.data());
    expect(root.get(), {2, 3}, live);
    track_pod_group_root(root.get(), A2FO_CRAFT_EVENT_CLEANUP);
    expect(a.get(), {}, live);
    g_runtime_ready = true;
    const A2FO_CraftEvent load_event{
        sizeof(A2FO_CraftEvent), A2FO_CRAFT_EVENT_POST_LOAD, root.get(), 0.0f};
    craft_event_handler(&load_event, nullptr);
    g_runtime_ready = false;
    expect(a.get(), {1, 3}, live);
    track_pod_group_root(root.get(), A2FO_CRAFT_EVENT_CLEANUP);
    expect(a.get(), {}, live);
    track_pod_group_root(root.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    expect(a.get(), {1, 3}, live);
    live[1] = other_root.get();
    expect(a.get(), {}, live);
    live[1] = root.get();
    track_pod_group_root(root.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    expect(a.get(), {1, 3}, live);
    std::cout << "PASS vtable, malformed native list, cleanup, POST_LOAD, simulation, and handle reuse guards\n";

    selection_tests(root.get(), a.get(), b.get(), live);

    g_pod_group_roots.clear();
    g_pod_group_classes.clear();
    g_pod_yard_classes.clear();
    g_pod_group_layout_ready = false;
    VirtualFree(g_armada, 0, MEM_RELEASE);
    g_armada = nullptr;
}

void installed_image_test() {
    constexpr char path[] =
        "Z:\\home\\tamsynn\\NVMeData\\Fleet Ops Roots\\Data\\ArmadaL.exe";
    HMODULE image = LoadLibraryExA(path, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    assert(image);
    g_armada = image;
    assert(preflight_signatures());
    assert(pod_group_layout_matches());
    constexpr char hook_path[] =
        "Z:\\home\\tamsynn\\NVMeData\\Fleet Ops Roots\\FleetOpsHook.dll";
    HMODULE fleet_ops = LoadLibraryExA(
        hook_path, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    assert(fleet_ops);
    auto* selection_add = reinterpret_cast<std::uint8_t*>(fleet_ops) +
        kFoSelectionAddRva;
    assert(readable_range(selection_add, sizeof(kExpectedFoSelectionAdd)));
    assert(std::memcmp(selection_add, kExpectedFoSelectionAdd,
                       sizeof(kExpectedFoSelectionAdd)) == 0);
    auto* byte = static_cast<std::uint8_t*>(at(0xba68f));
    DWORD old_protection = 0;
    assert(VirtualProtect(byte, 1, PAGE_EXECUTE_READWRITE, &old_protection));
    const auto saved = *byte;
    *byte ^= 1;
    assert(!pod_group_layout_matches());
    *byte = saved;
    DWORD ignored = 0;
    assert(VirtualProtect(byte, 1, old_protection, &ignored));
    assert(pod_group_layout_matches());
    FreeLibrary(fleet_ops);
    g_armada = nullptr;
    FreeLibrary(image);
    std::cout << "PASS installed ArmadaL preflight/pod signatures, FleetOpsHook selection signature, and mutation rejection\n";
}
}

int main() {
    policy_tests();
    installed_image_test();
    std::cout << "Pod group selection regressions passed\n";
}
