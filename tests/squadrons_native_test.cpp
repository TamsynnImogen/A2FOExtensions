// Headless x86 ABI/regression tests for the actual native adapter. The fake
// image contains only tiny test gateways; this never starts Armada or a GUI.
#include "../modules/A2FOSquadrons/module.cpp"

#include <cassert>
#include <iostream>

namespace {
std::array<std::array<std::uint8_t, 0x300>, 3> ships{};
std::array<std::uint8_t, 0x500> selection{};
std::array<void*, 40> selection_vtable{};
std::vector<std::int32_t> clear_flags;
std::array<std::uint8_t, 0x400> yard{};
std::array<void*, 100> yard_vtable{};
std::array<void*, 8> mission_vtable{};
std::array<void*, 2> mission{};
std::array<void*, 2> output_queue{};
std::array<std::uint8_t, 0x200> member_class{};
std::size_t constructed = 0;
std::size_t published = 0;
std::size_t fail_construct_at = 99;
std::string drawn_badge;
Rectangle drawn_rectangle{};
std::array<float, 2> drawn_health{};
std::array<float, 3> drawn_colour{};

template<class T> void put(void* object, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}

void* __cdecl entity(std::uint32_t handle) {
    if (handle == 900) return yard.data();
    return handle >= 1 && handle <= ships.size() ? ships[handle - 1].data() : nullptr;
}

void* __cdecl find_class_method(const char*) { return member_class.data(); }
const char* __attribute__((fastcall)) odf_method(void*, void*) { return "fighter"; }
void* __cdecl mission_method() { return mission.data(); }
void __attribute__((fastcall)) publish_method(void*, void*, void*) { ++published; }
void __attribute__((fastcall)) expire_method(void* craft, void*) {
    put<std::uint8_t>(craft, kObjectExpiredOffset, 1);
}
void __attribute__((fastcall)) transform_method(void*, void*, Matrix34* matrix) {
    matrix->values[9] = 123.0f;
}
void* __attribute__((fastcall)) construct_method(void*, void*, const Matrix34* matrix,
                                                std::int32_t team, std::uintptr_t,
                                                const char*) {
    if (constructed == fail_construct_at) return nullptr;
    assert(constructed < ships.size());
    assert(matrix->values[9] == 123.0f);
    auto* craft = ships[constructed++].data();
    put(craft, kObjectTeamOffset, team);
    put(craft, kObjectClassOffset, member_class.data());
    return craft;
}
void __attribute__((fastcall)) queue_method(void*, void*, void* craft) {
    put<std::uint32_t>(craft, kCraftQueueStageOffset, 1);
    put<std::uint32_t>(craft, kCraftQueueOwnerOffset, 900);
}

std::uintptr_t __attribute__((fastcall)) count_method(void* self, void*) {
    return read_at<std::uint32_t>(self, kNativeSelectionCountOffset, 0);
}

std::uintptr_t __attribute__((fastcall)) handles_method(void* self, void*) {
    return reinterpret_cast<std::uintptr_t>(self) + kNativeSelectionHandlesOffset;
}

void __stdcall select_method(void* self, void* craft, std::int32_t operation,
                             std::int32_t clear, std::int32_t, std::int32_t) {
    // FleetOpsHook RVA 0x1dad58 tests [ebp+0x14] (argument FOUR),
    // then calls selection vtable+0xa0 to clear before normalizing mode 2.
    clear_flags.push_back(clear);
    auto count = read_at<std::uint32_t>(self, kNativeSelectionCountOffset, 0);
    auto* handles = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(self) + kNativeSelectionHandlesOffset);
    if (clear) {
        count = 0;
        for (auto& ship : ships) put<std::uint8_t>(ship.data(), kNativeSelectedFlagOffset, 0);
    }
    const auto handle = object_handle(craft);
    if (operation == 2)
        operation = read_at<std::uint8_t>(craft, kNativeSelectedFlagOffset, 0) ? 1 : 0;
    const auto found = std::find(handles, handles + count, handle);
    if (!operation && found == handles + count) {
        assert(count < kNativeSelectionLimit);
        handles[count++] = handle;
        put<std::uint8_t>(craft, kNativeSelectedFlagOffset, 1);
    } else if (operation == 1 && found != handles + count) {
        std::move(found + 1, handles + count, found);
        --count;
        put<std::uint8_t>(craft, kNativeSelectedFlagOffset, 0);
    }
    put(self, kNativeSelectionCountOffset, count);
}

void* __attribute__((fastcall)) health_method(void* bar, void*, float* values) {
    const auto* craft = read_at<void*>(bar, 0x428, nullptr);
    values[0] = read_at<float>(craft, 0x15c, 0);
    values[1] = read_at<float>(craft, 0x160, 0);
    return values;
}

void* __attribute__((fastcall)) colour_method(void* bar, void*, float* values) {
    const auto* craft = read_at<void*>(bar, 0x428, nullptr);
    const float maximum = read_at<float>(craft, 0x1cc, 0);
    const float fraction = maximum > 0 ? read_at<float>(craft, 0x1c8, 0) / maximum : 0;
    values[0] = 1.0f - fraction;
    values[1] = fraction;
    values[2] = 0.0f;
    return values;
}

void __attribute__((fastcall)) text_method(
    void*, void*, const char* text, const Rectangle* rectangle,
    std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t) {
    drawn_badge = text;
    drawn_rectangle = *rectangle;
}

void __attribute__((fastcall)) render_method(void* panel, void*) {
    void* bar = read_at<void*>(panel, 0xa0, nullptr);
    void* vtable = read_at<void*>(bar, 0, nullptr);
    a2fo_squadrons_call_thiscall_1(read_at<void*>(vtable, 0x24, nullptr), bar,
        reinterpret_cast<std::uintptr_t>(drawn_health.data()));
    a2fo_squadrons_call_thiscall_1(read_at<void*>(vtable, 0x20, nullptr), bar,
        reinterpret_cast<std::uintptr_t>(drawn_colour.data()));
    put(panel, 0x1e8, ships[1].data());
}

void jump_to(void* address, void* target) {
    auto* code = static_cast<std::uint8_t*>(address);
    code[0] = 0xe9;
    const auto displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(target) -
        reinterpret_cast<std::uintptr_t>(code + 5));
    put(code, 1, displacement);
}

// Execute the real indirect call instruction at the supported image RVA.
// The return-address filters must match RVAs, not objdump's .text offsets.
std::uintptr_t query_at(std::uintptr_t return_rva, bool handles) {
    auto* code = static_cast<std::uint8_t*>(at(return_rva - 13));
    code[0] = 0xb9; // mov ecx, selection
    put(code, 1, selection.data());
    code[5] = 0x8b;
    code[6] = handles ? 0x11 : 0x01; // mov edx/eax, [ecx]
    std::memcpy(code + 7, handles ? kExpectedUiHandlesCall : kExpectedUiCountCall, 6);
    code[13] = 0xc3;
    FlushInstructionCache(GetCurrentProcess(), code, 14);
    return reinterpret_cast<std::uintptr_t (__cdecl*)()>(code)();
}

void setup() {
    g_armada = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x370000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(g_armada);
    jump_to(at(kEntityGetRva), reinterpret_cast<void*>(&entity));
    for (std::size_t i = 0; i < ships.size(); ++i) {
        put(ships[i].data(), kObjectHandleOffset, static_cast<std::uint32_t>(i + 1));
        put<std::int32_t>(ships[i].data(), kObjectTeamOffset, 1);
    }
    selection_vtable[kSelectionGetCountVtableOffset / 4] = reinterpret_cast<void*>(&count_method);
    selection_vtable[kSelectionGetHandlesVtableOffset / 4] = reinterpret_cast<void*>(&handles_method);
    put(selection.data(), 0, selection_vtable.data());
    put(at(kUiSelectionPointerRva), 0, selection.data());
    for (auto rva : kShipDisplayCountReturns)
        std::memcpy(at(rva - 6), kExpectedUiCountCall, 6);
    for (auto rva : kShipDisplayHandlesReturns)
        std::memcpy(at(rva - 6), kExpectedUiHandlesCall, 6);
    g_selection_add_hook.gateway = reinterpret_cast<void*>(&select_method);
    g_runtime_ready = true;
    const Definition definition{"patrol", {{0, "fighter", 3}}, true};
    const auto created = g_registry.create(definition, 1,
        {{1, 1, "fighter"}, {2, 1, "fighter"}, {3, 1, "fighter"}},
        [](const std::string&) { return MemberClass::mobile_craft; });
    assert(created);
}

void selection_tests() {
    // A full prior selection must not reject an ordinary replacement click.
    put<std::uint32_t>(selection.data(), kNativeSelectionCountOffset, 30);
    selection_add_hook(selection.data(), ships[1].data(), 0, 1, 0, 1);
    assert(count_method(selection.data(), nullptr) == 3);
    assert((clear_flags == std::vector<std::int32_t>{1, 0, 0}));
    for (const auto& ship : ships)
        assert(read_at<std::uint8_t>(ship.data(), kNativeSelectedFlagOffset, 0) == 1);
    assert(g_ui_selection_vtable);
    for (auto rva : kShipDisplayCountReturns) assert(query_at(rva, false) == 1);
    for (auto rva : kShipDisplayHandlesReturns) {
        auto* projected = reinterpret_cast<std::uint32_t*>(query_at(rva, true));
        assert(projected[0] == 2);
    }
    // Calls outside ShipDisplay must still see every physical command handle.
    assert(a2fo_squadrons_call_thiscall_0(selection_vtable[0x94 / 4], selection.data()) == 3);
    for (auto& ship : ships) {
        clear_flags.clear();
        selection_add_hook(selection.data(), ship.data(), 0, 1, 1, 1);
        assert(count_method(selection.data(), nullptr) == 3);
        assert((clear_flags == std::vector<std::int32_t>{1, 0, 0}));
    }
    // Clearing precedes native toggle normalization, even for a member that
    // was already selected when the gesture began.
    selection_add_hook(selection.data(), ships[1].data(), 2, 1, 0, 0);
    assert(count_method(selection.data(), nullptr) == 3);
    selection_add_hook(selection.data(), ships[2].data(), 2, 0, 0, 0);
    assert(count_method(selection.data(), nullptr) == 0);
    selection_add_hook(selection.data(), ships[0].data(), 2, 0, 0, 0);
    assert(count_method(selection.data(), nullptr) == 3);
    // Compatibility checks cannot grant space in a full selection. Only the
    // separate clear-selection argument permits replacing it.
    for (auto& ship : ships) put<std::uint8_t>(ship.data(), kNativeSelectedFlagOffset, 0);
    for (std::uint32_t i = 0; i < 30; ++i)
        put(selection.data(), kNativeSelectionHandlesOffset + i * 4, i + 100);
    put<std::uint32_t>(selection.data(), kNativeSelectionCountOffset, 30);
    clear_flags.clear();
    selection_add_hook(selection.data(), ships[0].data(), 0, 0, 0, 1);
    assert(clear_flags.empty() && count_method(selection.data(), nullptr) == 30);
    selection_add_hook(selection.data(), ships[0].data(), 0, 1, 0, 0);
    assert(count_method(selection.data(), nullptr) == 3);
    assert((clear_flags == std::vector<std::int32_t>{1, 0, 0}));
    std::cout << "PASS single click, full-selection replacement, toggle, one UI slot and physical command list\n";
}

void health_tests() {
    std::array<std::uint8_t, 0x440> bar{};
    std::array<void*, 11> vtable{};
    vtable[9] = reinterpret_cast<void*>(&health_method);
    put(bar.data(), 0, vtable.data());
    put(bar.data(), 0x428, ships[1].data());
    const std::array<float, 3> current{{25, 100, 25}};
    const std::array<float, 3> maximum{{100, 200, 50}};
    for (std::size_t i = 0; i < ships.size(); ++i) {
        put(ships[i].data(), 0x15c, current[i]);
        put(ships[i].data(), 0x160, maximum[i]);
    }
    const auto before = ships;
    SquadBarOverride binding;
    binding.bar = bar.data();
    binding.original_vtable = vtable.data();
    g_active_bars = &binding;
    g_active_bar_count = 1;
    std::array<float, 2> result{};
    auto* returned = reinterpret_cast<void*>(a2fo_squadrons_call_thiscall_1(
        reinterpret_cast<void*>(&squad_bar_values_hook), bar.data(),
        reinterpret_cast<std::uintptr_t>(result.data())));
    assert(returned == result.data());
    assert(result[0] == 150 && result[1] == 350);
    assert(read_at<void*>(bar.data(), 0x428, nullptr) == ships[1].data());
    assert(ships == before);
    g_registry.remove(3);
    squad_bar_values_hook(bar.data(), nullptr, result.data());
    assert(result[0] == 125 && result[1] == 300);
    assert(live_squad(ships[0].data()).count == 2);
    assert(live_squad(ships[0].data()).maximum == 3);
    g_registry.remove(1);
    g_registry.remove(2);
    squad_bar_values_hook(bar.data(), nullptr, result.data());
    assert(result[0] == 100 && result[1] == 200);
    g_active_bars = nullptr;
    g_active_bar_count = 0;
    std::cout << "PASS weighted squad health, casualty count, ordinary bar fallback and unchanged ship health\n";
}

void launch_tests() {
    const Definition definition{"patrol", {{0, "fighter", 3}}, true};
    const auto reset = [&] {
        g_registry = Registry{};
        g_pending_launches.clear();
        ships = {};
        for (std::size_t i = 0; i < ships.size(); ++i)
            put<std::uint32_t>(ships[i].data(), kObjectHandleOffset, i + 1);
        constructed = published = 0;
        fail_construct_at = 99;
    };
    jump_to(at(kGameObjectClassFindRva), reinterpret_cast<void*>(&find_class_method));
    jump_to(at(kGameObjectClassGetOdfNameRva), reinterpret_cast<void*>(&odf_method));
    jump_to(at(kGameObjectClassConstructRva), reinterpret_cast<void*>(&construct_method));
    jump_to(at(kAiMissionGetCurrentRva), reinterpret_cast<void*>(&mission_method));
    jump_to(at(kCraftDoExpireRva), reinterpret_cast<void*>(&expire_method));
    put(yard.data(), 0, yard_vtable.data());
    put<std::uint32_t>(yard.data(), kObjectHandleOffset, 900);
    put<std::int32_t>(yard.data(), kObjectTeamOffset, 1);
    yard_vtable[kProducerBuildTransformVtableOffset / 4] = reinterpret_cast<void*>(&transform_method);
    put(yard.data(), kShipyardBuildOutputQueueOffset, output_queue.data());
    output_queue[0] = at(kOutputQueueManagerVtableRva);
    put(at(kOutputQueueManagerVtableRva), 0x10, reinterpret_cast<void*>(&queue_method));
    mission[0] = mission_vtable.data();
    mission_vtable[0x18 / 4] = reinterpret_cast<void*>(&publish_method);
    FlushInstructionCache(GetCurrentProcess(), g_armada, 0x370000);
    reset();
    assert(spawn_squadron(yard.data(), definition));
    assert(constructed == 1 && published == 1 && g_pending_launches.size() == 1);
    update_pending_launches(ships[0].data(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(constructed == 1); // Still moving through the native yard stage.
    put<std::uint32_t>(ships[0].data(), kCraftQueueStageOffset, 0);
    update_pending_launches(ships[0].data(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(constructed == 2 && published == 2);
    update_pending_launches(ships[0].data(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(constructed == 2); // An earlier member cannot launch another one.
    update_pending_launches(ships[1].data(), A2FO_CRAFT_EVENT_CLEANUP);
    g_registry.remove(2);
    update_pending_launches(yard.data(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(constructed == 3 && published == 3);
    put<std::uint32_t>(ships[2].data(), kCraftQueueStageOffset, 0);
    update_pending_launches(ships[2].data(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(g_pending_launches.empty());
    assert(g_registry.containing(1)->badge() == "2/3");
    reset();
    assert(spawn_squadron(yard.data(), definition));
    update_pending_launches(yard.data(), A2FO_CRAFT_EVENT_CLEANUP);
    put<std::uint32_t>(ships[0].data(), kCraftQueueStageOffset, 0);
    update_pending_launches(ships[0].data(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(constructed == 1 && g_pending_launches.empty());
    reset();
    assert(spawn_squadron(yard.data(), definition));
    fail_construct_at = 1;
    put<std::uint32_t>(ships[0].data(), kCraftQueueStageOffset, 0);
    update_pending_launches(ships[0].data(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(constructed == 1 && g_pending_launches.empty());
    std::cout << "PASS native sequential output, prior-exit gate, launch casualty, yard removal and construction failure\n";
}

void render_tests() {
    const Definition definition{"patrol", {{0, "fighter", 3}}, true};
    assert(g_registry.create(definition, 1,
        {{1, 1, "fighter"}, {2, 1, "fighter"}, {3, 1, "fighter"}},
        [](const std::string&) { return MemberClass::mobile_craft; }));
    std::array<std::uint8_t, 0x200> panel{};
    std::array<std::uint8_t, 0x88> text{};
    std::array<std::uint8_t, 0x40> icon{};
    std::array<std::uint8_t, 0x440> bar{};
    std::array<void*, 11> vtable{};
    vtable[8] = reinterpret_cast<void*>(&colour_method);
    vtable[9] = reinterpret_cast<void*>(&health_method);
    put(panel.data(), 0x94, text.data());
    put(panel.data(), 0xac, icon.data());
    put(panel.data(), 0xa0, bar.data());
    put(text.data(), 4, panel.data());
    put(icon.data(), 4, panel.data());
    put(icon.data(), 8, Rectangle{10, 10, 110, 110});
    put(bar.data(), 0, vtable.data());
    put(bar.data(), 4, panel.data());
    put(bar.data(), 0x428, ships[1].data());
    for (auto& ship : ships) {
        put(ship.data(), 0x1c8, read_at<float>(ship.data(), 0x15c, 0));
        put(ship.data(), 0x1cc, read_at<float>(ship.data(), 0x160, 0));
    }
    const auto before = ships;
    jump_to(at(kDrawTextRva), reinterpret_cast<void*>(&text_method));
    FlushInstructionCache(GetCurrentProcess(), at(kDrawTextRva), 5);
    g_ship_display_render_original = reinterpret_cast<void*>(&render_method);
    a2fo_squadrons_call_thiscall_0(reinterpret_cast<void*>(&squad_panel_render_hook), panel.data());
    assert(drawn_badge == "3/3");
    assert(drawn_rectangle.left == 68 && drawn_rectangle.top == 92);
    assert(drawn_health[0] == 150 && drawn_health[1] == 350);
    assert(std::fabs(drawn_colour[1] - 150.0f / 350.0f) < 0.0001f);
    assert(ships == before);
    assert(read_at<void*>(bar.data(), 0, nullptr) == vtable.data());
    assert(read_at<void*>(bar.data(), 0x428, nullptr) == ships[1].data());
    assert(!g_active_bars && !g_active_bar_count);
    std::cout << "PASS render scope, badge position/text ABI, aggregate shield tint and restored native UI state\n";
}
#include "squadrons_native_repair_test.inl"
#include "squadrons_native_economics_test.inl"
}

int main() {
    setup();
    selection_tests();
    health_tests();
    render_tests();
    launch_tests();
    repair_installation_tests();
    repair_tests();
    economics_native_tests();
    VirtualFree(g_armada, 0, MEM_RELEASE);
    std::cout << "Squadron native x86 regressions passed\n";
}
