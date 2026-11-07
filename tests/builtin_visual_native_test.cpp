// Synthetic i386/Wine fixture for the two visual built-ins. No game image or UI.
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace a2fo;
using WireHook = void** (__attribute__((fastcall)) *)(void*, void*, const void*);
using WorldHook = std::uintptr_t (__attribute__((fastcall)) *)(void*, void*, void*, void*);
std::uint8_t* image = nullptr;
std::string root;
bool root_owned = false, fail_wire_hook = false, fail_second_world = false;
unsigned wire_installs = 0, world_installs = 0, begins = 0, ends = 0;
unsigned wire_native_calls = 0, lookups = 0, world_native_calls = 0;
unsigned node_finds = 0, evaluations = 0, first_frames = 0;
WireHook wire_hook = nullptr;
WorldHook position_hook = nullptr, transform_hook = nullptr;
std::array<void*, 5> native_layers{};
void* build_sprite = reinterpret_cast<void*>(0x1010);
void* faction_sprite = reinterpret_cast<void*>(0x2020);
bool provide_build = true, provide_faction = true;
std::vector<std::string> lookup_names;
void* observed_self = nullptr;
void* observed_output = nullptr;
void* observed_node = nullptr;
void* visible_node = nullptr;
void* expected_root = nullptr;
void* expected_channel = nullptr;
void* expected_database = nullptr;
bool arguments_ok = true;

void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset, &value, sizeof(value));
}
template<class T> T get(const void* base, std::size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(base)+offset, sizeof(value));
    return value;
}
void bytes(std::size_t offset, std::initializer_list<std::uint8_t> values) {
    std::memcpy(image+offset, values.begin(), values.size());
}
void jump(std::size_t offset, void* target) {
    bytes(offset, {0xb8, 0, 0, 0, 0, 0xff, 0xe0});
    put(image+offset, 1, reinterpret_cast<std::uintptr_t>(target));
}
void A2FO_CALL log_message(const char*, const char*) {}
void log_line(const std::string&) {}
void* A2FO_CALL armada_module() { return image; }
void begin(const std::string&) { ++begins; }
void finish(const std::string&, bool) { ++ends; }

extern "C" void** __cdecl native_wire(void*, const void*) {
    ++wire_native_calls;
    return native_layers.data();
}
extern "C" void* __cdecl native_sprite(void*, const char* name, std::uintptr_t zero) {
    arguments_ok &= zero == 0;
    ++lookups;
    lookup_names.emplace_back(name);
    if (std::strcmp(name, "b_fbase") == 0 && provide_build) return build_sprite;
    if (std::strcmp(name, "romulan_icon") == 0 && provide_faction) return faction_sprite;
    return nullptr;
}
extern "C" std::uintptr_t __cdecl native_world(void* self, void* output, void* node) {
    ++world_native_calls;
    observed_self = self;
    observed_output = output;
    observed_node = node;
    return 0x76543210;
}
extern "C" std::uintptr_t __cdecl node_type(void*) { return 0; }
extern "C" void* __cdecl first_animation(void* database) {
    arguments_ok &= database == expected_database;
    return expected_channel;
}
extern "C" void* __cdecl next_animation(void* channel) {
    arguments_ok &= channel == expected_channel;
    return nullptr;
}
extern "C" void* __cdecl find_node(void* root_node, const char* name) {
    arguments_ok &= root_node == expected_root && std::strcmp(name, "hp") == 0;
    ++node_finds;
    return visible_node;
}
extern "C" std::uintptr_t __cdecl evaluate(void* channel, std::uintptr_t argument) {
    arguments_ok &= channel == expected_channel && argument == 0;
    ++evaluations;
    return 1;
}
float __cdecl set_start(float start) {
    put(image+0x3a8e78, 0, start);
    put(image+0x3a8e74, 0, 12.0f);
    return start;
}
void __cdecl first_frame() {
    ++first_frames;
    put(image+0x3a8e74, 0, 0.0f);
}
// Match the module's thiscall_bridge.S: ECX is self, callee removes arguments.
#define BRIDGE0(name, callback) \
    extern "C" __attribute__((naked)) void name() { \
        __asm__ volatile("pushl %ecx\ncall _" #callback "\naddl $4,%esp\nret"); \
    }
#define BRIDGE1(name, callback) \
    extern "C" __attribute__((naked)) void name() { \
        __asm__ volatile("pushl 4(%esp)\npushl %ecx\ncall _" #callback \
                         "\naddl $8,%esp\nret $4"); \
    }
#define BRIDGE2(name, callback) \
    extern "C" __attribute__((naked)) void name() { \
        __asm__ volatile("pushl 8(%esp)\npushl 8(%esp)\npushl %ecx\ncall _" #callback \
                         "\naddl $12,%esp\nret $8"); \
    }
BRIDGE1(wire_bridge, native_wire)
BRIDGE2(sprite_bridge, native_sprite)
BRIDGE2(world_bridge, native_world)
BRIDGE0(type_bridge, node_type)
BRIDGE0(first_bridge, first_animation)
BRIDGE0(next_bridge, next_animation)
BRIDGE1(find_bridge, find_node)
BRIDGE1(evaluate_bridge, evaluate)
#undef BRIDGE0
#undef BRIDGE1
#undef BRIDGE2
bool A2FO_CALL install(void* target, void* replacement, std::size_t size,
                       const std::uint8_t* expected, A2FO_InlineHook* hook) {
    check(std::memcmp(target, expected, size) == 0, "visual hook signature");
    if (target == image+0xf7a60) {
        ++wire_installs;
        wire_hook = reinterpret_cast<WireHook>(replacement);
        if (fail_wire_hook) return false;
        *hook = {target, reinterpret_cast<void*>(&wire_bridge), static_cast<std::uint32_t>(size)};
    } else if (target == image+0x22e470 || target == image+0x22e500) {
        ++world_installs;
        if (target == image+0x22e470) position_hook = reinterpret_cast<WorldHook>(replacement);
        else transform_hook = reinterpret_cast<WorldHook>(replacement);
        if (fail_second_world && target == image+0x22e500) return false;
        *hook = {target, reinterpret_cast<void*>(&world_bridge), static_cast<std::uint32_t>(size)};
    } else check(false, "unexpected visual hook target");
    return true;
}

void seed(const char* mode) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    check(image != nullptr, "synthetic Armada allocation");
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE; dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image+0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp = 0x3c4c76bd;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = std::strcmp(mode, "identity-fail") == 0 ? 0x400000 : 0x403999;
    bytes(0xf7a60, {0x55,0x8b,0xec,0x6a,0xff,0x68,0x2d,0xf9,0x69,0});
    bytes(0x220750, {0x55,0x8b,0xec,0x8b,0x45,0x08,0x53,0x56,0x57});
    bytes(0x22e470, {0x55,0x8b,0xec,0x83,0xec,0x60});
    bytes(0x22e500, {0x55,0x8b,0xec,0x83,0xec,0x60});
    bytes(0x238780, {0x55,0x8b,0xec,0x56,0x57});
    bytes(0x213670, {0x55,0x8b,0xec,0x51,0x8b,0x4d});
    bytes(0x22f1c0, {0x8b,0x41,0x70,0x85,0xc0});
    bytes(0x221080, {0x8b,0x41,0x0c,0x8b,0x49,0x04});
    if (std::strcmp(mode, "signature-fail") == 0) {
        image[0xf7a60] ^= 1;
        image[0x22e470] ^= 1;
    }
}
void stub_calls() {
    jump(0x220750, reinterpret_cast<void*>(&sprite_bridge));
    jump(0x238780, reinterpret_cast<void*>(&find_bridge));
    jump(0x213670, reinterpret_cast<void*>(&set_start));
    jump(0x2136c0, reinterpret_cast<void*>(&first_frame));
    jump(0x22f1c0, reinterpret_cast<void*>(&first_bridge));
    jump(0x221080, reinterpret_cast<void*>(&next_bridge));
}
void write_file(const std::string& path, const char* content) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    check(file != INVALID_HANDLE_VALUE, "fixture file");
    DWORD written = 0;
    const auto size = static_cast<DWORD>(std::strlen(content));
    check(WriteFile(file, content, size, &written, nullptr) && written == size, "fixture write");
    CloseHandle(file);
}
ModulePolicy policy(bool wire, bool animated) {
    ModuleRules rules; rules.has_section = true;
    // No M3D native gateways are provided by this two-feature fixture.
    rules.rejected.push_back("A2FOM3D");
    if (wire) rules.active.push_back("A1Fallbacks");
    if (animated) rules.active.push_back("A2FOAnimatedHardpoints");
    return evaluate_module_rules({rules}, discover_installed_modules(root));
}
void toggle(bool wire, bool animated) {
    std::string error;
    const bool applied = apply_builtin_module_policy({root}, policy(wire, animated), error);
    check(applied, error.c_str());
    check(builtin_module_enabled(BuiltinFeature::a1_fallbacks) == wire &&
          builtin_module_enabled(BuiltinFeature::animated_hardpoints) == animated,
          "independent visual policy gates");
}
void cleanup() {
    if (!root_owned) return;
    DeleteFileA((root+"\\info.ini").c_str());
    DeleteFileA((root+"\\modules\\A1Fallbacks.dll").c_str());
    DeleteFileA((root+"\\modules\\A2FOAnimatedHardpoints.dll").c_str());
    RemoveDirectoryA((root+"\\modules").c_str());
    RemoveDirectoryA(root.c_str());
    // The built-in hardpoint cache may still refer to the synthetic image.
    // Keep it mapped until process exit.
}

void run(const char* mode) {
    const bool legacy = std::strcmp(mode, "legacy") == 0;
    const bool rejected = std::strcmp(mode, "rejected") == 0;
    fail_wire_hook = std::strcmp(mode, "hook-fail") == 0;
    fail_second_world = std::strcmp(mode, "partial") == 0;
    char temp[MAX_PATH]{};
    check(GetTempPathA(sizeof(temp), temp) != 0, "temporary directory");
    root = std::string(temp)+"a2fo-visual-"+std::to_string(GetCurrentProcessId());
    check(CreateDirectoryA(root.c_str(), nullptr) != 0, "private fixture root");
    root_owned = true;
    check(discover_installed_modules(root).size() == static_cast<std::size_t>(BuiltinFeature::count), "built-ins without standalone DLLs");
    check(CreateDirectoryA((root+"\\modules").c_str(), nullptr) != 0 ||
          GetLastError() == ERROR_ALREADY_EXISTS, "fixture modules directory");
    write_file(root+"\\modules\\A1Fallbacks.dll", "stale");
    write_file(root+"\\modules\\A2FOAnimatedHardpoints.dll", "stale");
    if (legacy) write_file(root+"\\info.ini", "[mod]\nName=Fixture\n");
    else if (rejected) write_file(root+"\\info.ini",
        "[modules]\nreject1=A1Fallbacks\nreject2=A2FOAnimatedHardpoints\n");
    else write_file(root+"\\info.ini", "[modules]\n");
    seed(mode);
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &log_message; api.armada_module = &armada_module;
    api.install_inline_hook = &install;
    LoadedModules loaded;
    ModuleRegistrationObserver observer{&begin, &finish};
    check(load_native_modules_from_roots({root}, api, loaded, &log_line, observer),
          "load visual built-ins");
    check(std::all_of(loaded.begin(), loaded.end(), [](const LoadedModule& item) { return item.handle == nullptr; }), "stale standalone visual DLLs ignored");
    check(discover_installed_modules(root).size() == static_cast<std::size_t>(BuiltinFeature::count), "visual inventory deduplicated");
    check(begins == (static_cast<unsigned>(BuiltinFeature::count) - (rejected ? 2u : 0u)) && ends == begins,
          "one preparation transaction per selected built-in");
    finish_builtin_module_preparation(false, false, false, false, false);
    if (rejected) {
        check(wire_installs == 0 && world_installs == 0, "rejected visual modules install no hooks");
        check(!builtin_module_available(BuiltinFeature::a1_fallbacks) &&
              !builtin_module_available(BuiltinFeature::animated_hardpoints),
              "rejected visual modules unavailable");
        return;
    }
    const bool wire_ready = std::strcmp(mode, "signature-fail") != 0 && !fail_wire_hook;
    const bool animated_ready = std::strcmp(mode, "signature-fail") != 0 &&
        std::strcmp(mode, "identity-fail") != 0 && !fail_second_world;
    check(builtin_module_available(BuiltinFeature::a1_fallbacks) == wire_ready &&
          builtin_module_available(BuiltinFeature::animated_hardpoints) == animated_ready,
          "visual availability follows native preparation");
    if (!wire_ready || !animated_ready) {
        std::string error;
        check(!apply_builtin_module_policy({root}, policy(true, true), error) && !error.empty(),
              "unavailable visual activation explained");
        check(!builtin_module_enabled(BuiltinFeature::a1_fallbacks) &&
              !builtin_module_enabled(BuiltinFeature::animated_hardpoints),
              "failed activation does not publish partial state");
        if (fail_second_world) {
            check(position_hook != nullptr && transform_hook != nullptr, "partial hooks captured");
            std::array<std::uint8_t, 0x100> object{}, output{}, logical{};
            check(position_hook(object.data(), nullptr, output.data(), logical.data()) == 0x76543210 &&
                  observed_node == logical.data(), "partial position hook native pass-through");
        }
        return;
    }
    check(wire_installs == 1 && world_installs == 2 && wire_hook &&
          position_hook && transform_hook, "three visual hooks installed once");
    check(builtin_module_enabled(BuiltinFeature::a1_fallbacks) == legacy &&
          builtin_module_enabled(BuiltinFeature::animated_hardpoints) == legacy,
          "managed off and legacy on defaults");
    stub_calls();

    std::array<std::uint8_t,0x120> wire{}, owner{}, klass{}, race{};
    std::array<std::uint8_t,0x100> instance{}, database{}, channel{}, logical{}, visible{}, output{};
    std::array<void*,5> node_vtable{};
    node_vtable[1] = reinterpret_cast<void*>(&type_bridge);
    const char basename[] = "fbase", faction[] = "romulan", node_name[] = "hp";
    put(image+0x365030, 0, database.data());
    put(wire.data(), 0, image+0x2b4f18);
    put(wire.data(), 0x28, owner.data());
    put(owner.data(), 0x40, klass.data());
    put(owner.data(), 0xfc, race.data());
    put(klass.data(), 0x7c, basename);
    std::memcpy(race.data()+0x14, faction, sizeof(faction));
    put(instance.data(), 0x80, database.data());
    put(instance.data(), 0x76, std::uint8_t{1});
    put(instance.data(), 0x18, 3.0f);
    put(database.data(), 0x3c, visible.data());
    put(channel.data(), 0, image+0x2bc42c);
    put(channel.data(), 0x08, node_name);
    put(image+0x2bc42c, 4*sizeof(void*), reinterpret_cast<void*>(&evaluate_bridge));
    put(logical.data(), 0, node_vtable.data());
    put(logical.data(), 0x08, node_name);
    put(image+0x3a8e74, 0, 99.0f);
    put(image+0x3a8e78, 0, 88.0f);
    expected_database = database.data(); expected_channel = channel.data();
    expected_root = visible.data(); visible_node = visible.data();

    auto wire_call = [&](void* widget) { return wire_hook(widget, nullptr, basename); };
    auto world_call = [&](WorldHook hook, void* node) {
        check(hook(instance.data(), nullptr, output.data(), node) == 0x76543210,
              "world native return preserved");
        check(observed_self == instance.data() && observed_output == output.data(),
              "world native self and output preserved");
        check(get<float>(image+0x3a8e74, 0) == 99.0f &&
              get<float>(image+0x3a8e78, 0) == 88.0f,
              "native animation globals restored");
    };
    toggle(false, false);
    check(wire_call(wire.data()) == native_layers.data() && lookups == 0,
          "initially off wireframe does not search sprites");
    world_call(position_hook, logical.data());
    check(observed_node == logical.data() && node_finds == 0 && evaluations == 0,
          "initially off hardpoint passes through without fallback lookup");
    toggle(true, false);
    native_layers[2] = reinterpret_cast<void*>(0x3030);
    check(wire_call(wire.data()) == native_layers.data() && lookups == 0,
          "valid native wireframe preserved while enabled");
    native_layers[2] = nullptr;
    check(wire_call(wire.data())[0] == build_sprite && lookup_names.back() == "b_fbase",
          "missing selected wireframe uses build button");
    provide_build = false;
    check(wire_call(wire.data())[0] == faction_sprite &&
          lookup_names[lookup_names.size()-2] == "b_fbase" &&
          lookup_names.back() == "romulan_icon", "selected owner faction fallback");
    provide_faction = false;
    check(wire_call(wire.data()) == native_layers.data() && native_layers[0] == nullptr,
          "missing both sprites retains native blank");
    put(wire.data(), 0, image+0x2b4994);
    put(wire.data(), 0x3c, klass.data());
    provide_build = true;
    check(wire_call(wire.data())[0] == build_sprite,
          "build queue target class layout uses build sprite");
    provide_build = false; provide_faction = true;
    check(wire_call(wire.data())[0] == faction_sprite,
          "build queue current owner faction fallback");
    toggle(false, true);
    const unsigned prior_lookups = lookups;
    check(wire_call(wire.data()) == native_layers.data() && lookups == prior_lookups,
          "wireframe disabled independently");
    world_call(position_hook, logical.data());
    check(observed_node == visible.data() && node_finds == 1 && evaluations == 1,
          "enabled logical null resolves visible node and evaluates matrix channel");
    world_call(transform_hook, logical.data());
    check(observed_node == visible.data() && evaluations == 1,
          "both world hooks share same-time pose cache");
    world_call(position_hook, visible.data());
    check(observed_node == visible.data() && evaluations == 1,
          "nonnull native node remains unchanged");
    std::string error;
    check(apply_builtin_module_policy({root}, policy(false,true), error), "resave same visual policy");
    world_call(transform_hook, logical.data());
    check(evaluations == 1, "same policy save retains evaluated pose");
    toggle(false, false);
    world_call(position_hook, logical.data());
    check(observed_node == logical.data() && evaluations == 1, "animated off passes through");
    toggle(false, true);
    world_call(position_hook, logical.data());
    check(observed_node == visible.data() && evaluations == 2,
          "off/on at same instance and time reevaluates pose");
    check(arguments_ok && wire_native_calls > 0 && world_native_calls > 0,
          "native resolver and bridge arguments");
    check(wire_installs == 1 && world_installs == 2,
          "live toggles do not reinstall visual hooks");
}
} // namespace

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    try { run(argc > 1 ? argv[1] : "success"); cleanup();
          std::puts("builtin visual native: PASS"); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what()); cleanup(); return 1;
    }
}
