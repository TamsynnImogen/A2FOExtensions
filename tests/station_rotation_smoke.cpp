// Headless module integration: map real EXE bytes without its entry point,
// capture hook registration, substitute native callees, execute x86 bridge.
// No windows, real input, or game processes are created or controlled.
#include "../modules/A2FOStationRotation/module.cpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

extern "C" int __cdecl station_test_position(
    void*, void*, const float*, const void*, const void*);
extern "C" int __cdecl station_test_footprint(
    void*, void*, const float*, const float*, bool, const void*);

namespace {
std::uint8_t* fixture = nullptr;
unsigned installs = 0;
unsigned fail_install = 0;
void* position_bridge = nullptr;
void* replacements[11]{};
std::vector<void*> path_bridges;
std::uint8_t* fleet_fixture = nullptr;
std::array<std::uint8_t, 0x30> mode{};
std::array<std::uint8_t, 0x300> producer{};
std::array<std::uint8_t, 0x100> second_producer{};
std::array<std::uint8_t, 0x80> preview{};
std::array<std::uint8_t, 0x400> class_token{};
std::array<std::uint8_t, 0x50> geometry{};
std::array<float, 7> cleared{};
std::array<float, 6> planner_added{}, planner_removed{};
bool prerequisite = true;
const float local_box[6] = {-10,-3,-20, 30,7,50};
const float requested[3] = {123, 456, 789};
std::array<float, 12> drawn{};
std::uint32_t sent_command = 0;
const void* sent_class = nullptr;
const float* sent_position = nullptr;

template<class T> void put(void* base, unsigned offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}
void A2FO_CALL test_log(const char*, const char* message) { std::puts(message); }
void* A2FO_CALL test_armada() { return fixture; }
void* A2FO_CALL test_fleet() { return fleet_fixture; }

void __fastcall native_queue(void*, void*, std::uint32_t cmd, const void* cls, const float* pos) {
    sent_command = cmd; sent_class = cls; sent_position = pos;
}
bool __fastcall native_broadcast(void*, void*, std::uint32_t action, const void* pos, void*) {
    if (action != kBuildCommand) return false;
    reinterpret_cast<Queue>(replacements[2])(producer.data(), nullptr, action,
        &class_token, static_cast<const float*>(pos));
    return true;
}
void __fastcall native_set_command(void* self, void*, std::uint32_t cmd,
        const void* cls, const float* pos, bool user) {
    assert(user);
    put(self, 0x7c, std::uint32_t(3));
    put(self, 0x80, cmd);
    put(self, 0x8c, std::uint32_t(0));
    put(self, 0x9c, cls);
    std::memcpy(static_cast<std::uint8_t*>(self) + 0x90, pos, 12);
}
void __fastcall native_preview(void* self, void*, void*, bool) {
    std::memcpy(drawn.data(), static_cast<std::uint8_t*>(self) + 0x38, sizeof(drawn));
}
void __fastcall native_set_position(void* self, void*, const float* pos, const void* cls) {
    assert(cls == &class_token);
    // Represent native snapping/cookie generation; the module must preserve it.
    float snapped[3] = {pos[0] + 0.5f, pos[1], pos[2] + 0.5f};
    std::memcpy(static_cast<std::uint8_t*>(self) + 0x28, snapped, sizeof(snapped));
    put(self, 0x34, std::uint32_t(0x11223344));
    put(self, 0x38, std::uint8_t(1));
}
bool __fastcall native_clear(void*, void*, float x0, float z0, float x1, float z1,
        float y0, float y1, float clearance, const void* ignored, bool test) {
    assert(ignored == producer.data() && test);
    cleared = {x0,z0,x1,z1,y0,y1,clearance};
    return true;
}
bool __fastcall native_can_place(void* cls, void*, const float* pos, const void* ignored, bool test) {
    if (!prerequisite) return false; // FO build-near restriction must not be bypassed
    const float snapped[3] = {pos[0]+0.5f, pos[1], pos[2]+0.5f};
    // Execute the real native formula. Its grid call is redirected below.
    return reinterpret_cast<CanPlace>(fixture+kCanPlaceRva)(cls, nullptr, snapped, ignored, test);
}
void* __cdecl native_action(void* result, const void*, const float* pos, bool*) {
    *static_cast<bool*>(result) = can_place_hook(&class_token, nullptr, pos, producer.data(), true);
    return result;
}
void __fastcall native_build(void*, void*) {
    assert(can_place_hook(&class_token, nullptr, requested, producer.data(), true));
}
void __fastcall native_render_footprints(void*, void*, void*, float) {}
void __fastcall native_add(void* cls, void*, const float* pos, const float* box, bool flag) {
    assert(cls == &class_token && pos == requested && flag); std::memcpy(planner_added.data(), box, 24);
}
void __fastcall native_remove(void* cls, void*, const float* pos, const float* box, bool flag) {
    assert(cls == &class_token && pos == requested && flag); std::memcpy(planner_removed.data(), box, 24);
}
float* __fastcall native_construction_matrix(void* self, void*, float* result) {
    auto* instance = read<const std::uint8_t*>(self, 4);
    std::memcpy(result, instance+0x44, 48); return result;
}
void jump_to(std::uint8_t* target, void* function) {
    target[0] = 0xe9;
    const auto delta = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(function) -
        reinterpret_cast<std::intptr_t>(target+5));
    std::memcpy(target+1, &delta, 4);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
}
bool A2FO_CALL test_install(void* target, void* replacement, std::size_t length,
                          const std::uint8_t* bytes, A2FO_InlineHook* hook) {
    assert(std::memcmp(target, bytes, length) == 0);
    if (++installs == fail_install) return false;
    const auto rva = static_cast<std::uint8_t*>(target) - fixture;
    void* gateway = nullptr;
    unsigned slot = 0;
    if (rva == kKeyboardRva) { gateway = target; slot = 0; }
    if (rva == kBroadcastRva) { gateway = reinterpret_cast<void*>(&native_broadcast); slot = 1; }
    if (rva == kQueueRva) { gateway = reinterpret_cast<void*>(&native_queue); slot = 2; }
    if (rva == kSetCommandRva) { gateway = reinterpret_cast<void*>(&native_set_command); slot = 3; }
    if (rva == kPreviewRva) { gateway = reinterpret_cast<void*>(&native_preview); slot = 4; }
    if (rva == kActionRva) { gateway = reinterpret_cast<void*>(&native_action); slot = 5; }
    if (target == fleet_fixture+kFoRenderFootprintsRva) { gateway = reinterpret_cast<void*>(&native_render_footprints); slot = 6; }
    if (rva == kFootprintRva) { gateway = target; slot = 7; }
    if (target == fleet_fixture+kFoCanPlaceRva) { gateway = reinterpret_cast<void*>(&native_can_place); slot = 8; }
    if (rva == kPlacementClearRva) { gateway = reinterpret_cast<void*>(&native_clear); slot = 9; }
    if (rva == kBuildRva) { gateway = reinterpret_cast<void*>(&native_build); slot = 10; }
    assert(gateway);
    replacements[slot] = replacement;
    *hook = {target, gateway, length};
    return true;
}
bool A2FO_CALL test_patch(void* target, void* replacement,
                        const std::uint8_t* bytes, std::size_t length) {

    assert(length == 5 && std::memcmp(target, bytes, length) == 0);
    if (++installs == fail_install) return false;
    if (target == fixture+kPositionCallRva) position_bridge = replacement;
    else path_bridges.push_back(replacement);
    return true;
}
std::uint8_t* map_fixture(const char* path, void* base) {
    std::ifstream file(path, std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    assert(bytes.size() > sizeof(IMAGE_DOS_HEADER));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data() + dos->e_lfanew);
    auto* mapped = static_cast<std::uint8_t*>(VirtualAlloc(base, nt->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(mapped);
    std::memcpy(mapped, bytes.data(), nt->OptionalHeader.SizeOfHeaders);
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& section = sections[i];
        assert(section.PointerToRawData + section.SizeOfRawData <= bytes.size());
        std::memcpy(mapped + section.VirtualAddress,
            bytes.data() + section.PointerToRawData, section.SizeOfRawData);
    }
    return mapped;
}
void relocate_fixture() {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(fixture);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(fixture+dos->e_lfanew);
    const auto delta = reinterpret_cast<std::uintptr_t>(fixture) - nt->OptionalHeader.ImageBase;
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for (unsigned offset = 0; offset < directory.Size;) {
        auto* block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(fixture+directory.VirtualAddress+offset);
        assert(block->SizeOfBlock >= sizeof(*block));
        const auto* fixups = reinterpret_cast<const std::uint16_t*>(block+1);
        for (unsigned i = 0; i < (block->SizeOfBlock-sizeof(*block))/2; ++i) {
            if ((fixups[i] >> 12) == IMAGE_REL_BASED_HIGHLOW) {
                auto* location = fixture+block->VirtualAddress+(fixups[i]&0xfff);
                put(location, 0, read<std::uint32_t>(location, 0)+delta);
            } else assert((fixups[i] >> 12) == IMAGE_REL_BASED_ABSOLUTE);
        }
        offset += block->SizeOfBlock;
    }
}
void assert_matrix(const void* object, unsigned offset, unsigned turns) {
    float expected[12]{}; set_yaw(expected, turns);
    assert(std::memcmp(static_cast<const std::uint8_t*>(object) + offset, expected, 36) == 0);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 3);
    fixture = map_fixture(argv[1], nullptr);
    fleet_fixture = map_fixture(argv[2], nullptr);
    // FO redirects this entry before extension modules initialize.
    fixture[kAddFootprintRva] = 0x68;
    put(fixture, kAddFootprintRva+1, fleet_fixture+kFoAddFootprintRva);
    fixture[kAddFootprintRva+5] = 0xc3;
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = test_log; api.armada_module = test_armada; api.fleetops_module = test_fleet;
    api.install_inline_hook = test_install; api.patch_call = test_patch;
    assert(!A2FO_ModuleInit(nullptr));
    // The old E9-only preflight rejected the real FO HookCodeNt shape.
    // Exact PUSH/RET works; the same shape aimed at another function must fail.
    put(fixture, kAddFootprintRva+1, fleet_fixture+kFoAddFootprintRva+1);
    assert(A2FO_ModuleInit(&api) && !g_ready && installs == 0);
    put(fixture, kAddFootprintRva+1, fleet_fixture+kFoAddFootprintRva);
    fixture[kPositionCallRva] ^= 1;
    assert(A2FO_ModuleInit(&api) && !g_ready && installs == 0);
    fixture[kPositionCallRva] ^= 1;
    fail_install = 4;
    assert(A2FO_ModuleInit(&api) && !g_ready && installs == 4);
    sent_command = 0;
    set_command_hook(producer.data(), nullptr, kBuildCommand, &class_token, requested, true);
    assert(read<std::uint32_t>(producer.data(), 0x80) == kBuildCommand);
    installs = 0; fail_install = 0;
    assert(A2FO_ModuleInit(&api) && g_ready && installs == 24);
    assert(path_bridges.size() == 12);
    assert(position_bridge == reinterpret_cast<void*>(&a2fo_station_position_bridge));
    // The exact-byte preflight runs first; relocation then permits executing
    // the few pure native routines at any Wine-compatible mapping address.
    relocate_fixture();

    // Replace only the native position callee, after validating its real bytes.
    fixture[kSetPositionRva] = 0xe9;
    const auto jump = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&native_set_position) -
        reinterpret_cast<std::intptr_t>(fixture + kSetPositionRva + 5));
    std::memcpy(fixture + kSetPositionRva + 1, &jump, 4);
    FlushInstructionCache(GetCurrentProcess(), fixture + kSetPositionRva, 5);
    put(fixture, kActiveModeRva, mode.data());
    put(mode.data(), 4, std::uint32_t(1)); put(mode.data(), 0xc, &class_token);
    put(preview.data(), 0x34, &class_token);
    float original[12]{}; set_yaw(original, 0);
    std::memcpy(original + 9, requested, 12);
    std::memcpy(preview.data() + 0x38, original, sizeof(original));

    constexpr std::uint32_t r_bit = 1u << 18;
    constexpr std::uint32_t other_keys = 0x80000001;
    std::uint32_t buttons[4] = {r_bit | other_keys, 0x1234, 0x5678, 0x9abc};
    update_placement_keys(buttons, true, false);
    assert(g_control.turns == 1 && buttons[0] == other_keys);
    assert(buttons[1] == 0x1234 && buttons[2] == 0x5678 && buttons[3] == 0x9abc);
    buttons[0] |= r_bit; update_placement_keys(buttons, true, false);
    assert(g_control.turns == 1 && buttons[0] == other_keys); // held R never activates Repair
    update_placement_keys(buttons, true, false); // release
    buttons[0] |= r_bit; update_placement_keys(buttons, true, true);
    assert(g_control.turns == 0 && buttons[0] == other_keys);
    buttons[0] |= r_bit; update_placement_keys(buttons, false, false);
    assert(g_control.turns == 0 && buttons[0] == (r_bit | other_keys));
    // Execute the real native keyboard copy. Headless focus prevents rotation;
    // physical state and unrelated channels retain native behavior.
    put(fixture, 0x362ee4, r_bit | other_keys);
    assert(reinterpret_cast<Keyboard>(replacements[0])(nullptr, nullptr, nullptr, nullptr, buttons) == 0);
    assert(buttons[0] == (r_bit | other_keys));
    // No camera hook or wheel accumulator writes remain in this module.
    put(fixture, 0x363320, std::int32_t(120));

    for (unsigned turns = 0; turns != 4; ++turns) {
        g_control.turns = turns;
        reinterpret_cast<Preview>(replacements[4])(preview.data(), nullptr, nullptr, true);
        assert_matrix(drawn.data(), 0, turns);
        assert(std::memcmp(drawn.data() + 9, requested, 12) == 0);
        assert(std::memcmp(preview.data() + 0x38, original, sizeof(original)) == 0);

        assert(reinterpret_cast<Broadcast>(replacements[1])(
            nullptr, nullptr, kBuildCommand, requested, nullptr));
        assert(sent_command == encode_build(turns));
        assert(sent_class == &class_token && sent_position == requested);
        const auto wire_command = static_cast<std::uint8_t>(sent_command);
        // Native serialization transports all three floats unchanged.
        assert(!g_sending_class && g_sending_turns == 0);
        reinterpret_cast<Queue>(replacements[2])(
            nullptr, nullptr, kBuildCommand, &class_token, requested);
        assert(sent_command == kBuildCommand); // AI/script/out-of-scope orders unchanged

        g_control.turns = (turns + 2) & 3; // receiving peer has a different local preview
        reinterpret_cast<SetCommand>(replacements[3])(
            producer.data(), nullptr, wire_command, &class_token, requested, true);
        const auto* command = producer.data() + 0x7c;
        assert(read<std::uint32_t>(command, 4) == kBuildCommand);
        assert(parameter_turns(read<std::uint32_t>(command, 0x10)) == turns);
        assert(std::memcmp(command + 0x14, requested, 12) == 0);
        assert(read<void*>(command, 0x20) == &class_token);
        std::array<std::uint8_t, 0x3c> build_interface{};
        assert(station_test_position(position_bridge, build_interface.data(), requested, &class_token, command));
        assert_matrix(build_interface.data(), 4, turns);
        assert(read<float>(build_interface.data(), 0x28) == requested[0] + 0.5f);
        assert(read<float>(build_interface.data(), 0x2c) == requested[1]);
        assert(read<float>(build_interface.data(), 0x30) == requested[2] + 0.5f);
        assert(read<std::uint32_t>(build_interface.data(), 0x34) == 0x11223344);
        assert(read<std::uint8_t>(build_interface.data(), 0x38) == 1);
        // A subsequent ordinary build resets the previous orientation to zero.
        reinterpret_cast<SetCommand>(replacements[3])(
            producer.data(), nullptr, kBuildCommand, &class_token, requested, true);
        assert(station_test_position(position_bridge, build_interface.data(), requested, &class_token, command));
        assert_matrix(build_interface.data(), 4, 0);
    }
    // Independent producers and copied command records retain their own angles.
    set_command_hook(producer.data(), nullptr, 0xb1, &class_token, requested, true);
    set_command_hook(second_producer.data(), nullptr, 0xb3, &class_token, requested, true);
    std::array<std::uint8_t, 0x30> copied_command{};
    std::memcpy(copied_command.data(), producer.data() + 0x7c, copied_command.size());
    std::array<std::uint8_t, 0x3c> interface_a{}, interface_b{};
    assert(station_test_position(position_bridge, interface_b.data(), requested, &class_token, second_producer.data() + 0x7c));
    assert(station_test_position(position_bridge, interface_a.data(), requested, &class_token, copied_command.data()));
    assert_matrix(interface_a.data(), 4, 1); assert_matrix(interface_b.data(), 4, 3);
    // Real native class bounds + the module's placement/path transforms.
    std::memcpy(geometry.data()+4, local_box, sizeof(local_box));
    put(class_token.data(), 0x1d8, geometry.data());
    put(class_token.data(), 0x214, 5.0f);
    put(class_token.data(), 0x218, 2.0f); put(class_token.data(), 0x21c, 4.0f);
    put(class_token.data(), 0x220, 6.0f); put(class_token.data(), 0x224, 8.0f);
    put(class_token.data(), 0x228, 9.0f); put(class_token.data(), 0x22c, 11.0f);
    jump_to(fixture+kPlacementClearRva, reinterpret_cast<void*>(&placement_clear_hook));
    jump_to(fixture+kAddFootprintRva, reinterpret_cast<void*>(&native_add));
    jump_to(fixture+kRemoveFootprintRva, reinterpret_cast<void*>(&native_remove));
    std::array<std::uint8_t, 0x350> station{};
    std::array<std::uint8_t, 0x100> instance{};
    std::array<void*, 0x190/4> vtable{};
    vtable[0x188/4] = reinterpret_cast<void*>(&native_construction_matrix);
    put(station.data(), 0, vtable.data()); put(station.data(), 4, instance.data());
    std::memcpy(station.data()+0xac, requested, 12);
    put(instance.data(), 0x40, 50.0f); // native bounding-sphere radius
    put(fixture, 0x339590, -10000.0f); put(fixture, 0x33959c, 10000.0f);
    put(fixture, 0x339598, -10000.0f); put(fixture, 0x3395a4, 10000.0f);
    const a2fo::station_rotation::Rectangle expected[4] = {{-14,-28,32,56},{-28,-32,56,14},
                                 {-32,-56,14,28},{-56,-14,28,32}};
    for (unsigned turns = 0; turns != 4; ++turns) {
        g_control.turns = turns;
        float m[12]{}; set_yaw(m, turns);
        std::memcpy(m+9, requested, 12); std::memcpy(instance.data()+0x44, m, sizeof(m));
        bool valid = false;
        assert(action_hook(&valid, producer.data(), requested, nullptr) == &valid && valid);
        const auto e = expected[turns];
        assert(cleared[0] == requested[0]+0.5f+e.min_x && cleared[1] == requested[2]+0.5f+e.min_z);
        assert(cleared[2] == requested[0]+0.5f+e.max_x && cleared[3] == requested[2]+0.5f+e.max_z);
        assert(cleared[4] == requested[1]-12 && cleared[5] == requested[1]+18 && cleared[6] == 5);
        assert(g_ui_footprint_depth == 0 && !g_placement_class);
        const auto old_clear = cleared;
        prerequisite = false; valid = true;
        action_hook(&valid, producer.data(), requested, nullptr);
        assert(!valid && cleared == old_clear); prerequisite = true;
        float x0=0,z0=0,x1=0,z1=0;
        {
            UiFootprintScope scope;
            footprint_hook(&class_token, nullptr, requested[0], requested[2], &x0,&z0,&x1,&z1);
        }
        assert(x0 == requested[0]+e.min_x-5 && z0 == requested[2]+e.min_z-5);
        assert(x1 == requested[0]+e.max_x+5 && z1 == requested[2]+e.max_z+5);
        // Simulation must use the queued transform despite a different UI angle.
        std::array<std::uint8_t, 0x40> process{};
        std::array<std::uint8_t, 0x3c> placement{};
        put(process.data(), 0x30, producer.data());
        put(producer.data(), 0x2a4, placement.data());
        std::memcpy(placement.data()+4, m, sizeof(m)); placement[0x38] = 1;
        g_control.turns = (turns+2)&3;
        build_hook(process.data(), nullptr);
        assert(!g_build_process && !g_placement_class);
        assert(cleared == old_clear);
        g_control.turns = turns;
        for (unsigned i = 0; i < path_bridges.size(); ++i) {
            planner_added.fill(0); planner_removed.fill(0);
            assert(station_test_footprint(path_bridges[i], &class_token, requested,
                local_box, true, station.data()));
            const auto& actual = kFootprintCalls[i].native == kAddFootprintRva ? planner_added : planner_removed;
            const auto occupied = padded_footprint(actual.data(), class_margins(&class_token));
            assert(std::memcmp(&occupied, &e, sizeof(e)) == 0);
        }
        a2fo_station_add_footprint(&class_token, requested, local_box, true, station.data());
        a2fo_station_remove_footprint(&class_token, requested, local_box, true, station.data());
        assert(planner_added == planner_removed);
        const auto occupied = padded_footprint(planner_added.data(), class_margins(&class_token));
        assert(std::memcmp(&occupied, &e, sizeof(e)) == 0);
        assert(std::memcmp(geometry.data()+4, local_box, sizeof(local_box)) == 0);
        // Execute Armada's actual Shipyard::mRecomputeRallyPoint, not a copy.
        using Rally = void (__fastcall*)(void*, void*);
        reinterpret_cast<Rally>(fixture+0x0bc150)(station.data(), nullptr);
        const float distance = 50.0f * read<float>(fixture, 0x2aea08);
        assert(read<float>(station.data(), 0x2c4) == requested[0]+m[6]*distance);
        assert(read<float>(station.data(), 0x2c8) == requested[1]);
        assert(read<float>(station.data(), 0x2cc) == requested[2]+m[8]*distance);
    }
    // Cancellation restores the normal R binding; the wheel is untouched.
    put(mode.data(), 4, std::uint32_t(0));
    buttons[0] = r_bit; update_placement_keys(buttons, true, false);
    assert(g_control.turns == 0 && buttons[0] == r_bit);
    assert(read<int>(fixture, 0x363320) == 120);
    A2FO_ModuleShutdown();
    VirtualFree(fleet_fixture, 0, MEM_RELEASE);
    VirtualFree(fixture, 0, MEM_RELEASE);
    std::puts("Station rotation native bridge / real-image signature smoke passed");
}
