// Full frozen/current FireArcs module fixture with synthetic native image.
#include <windows.h>
#include <float.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/builtin_modules.hpp"
#include MODULE_SOURCE

namespace {
std::vector<std::string> trace;
std::string mode;
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
std::array<std::uint8_t, 0x300> weapon{}, klass{}, target{}, icon{},
    craft{}, weapon_system{};
Matrix34 owner_matrix{{1,0,0, 0,1,0, 0,0,1, 0,0,0}};
std::unordered_map<std::string, std::string> fields;
A2FO_WeaponClassLoadedHandler class_callback = nullptr;
A2FO_WeaponTriggerHandler trigger_callback = nullptr;
unsigned hook_count = 0, native_count = 0;
bool native_result = true, enabled = true;
std::uint64_t activation = 1;
const char* fixture_roots[2] = {
    "build\\rust-migration-stage8\\runtime-a-fire-arcs-work\\root0",
    "build\\rust-migration-stage8\\runtime-a-fire-arcs-work\\root1"};

void record(const std::string& value) { trace.push_back(value); }
std::uint32_t bits(float value) {
    std::uint32_t output = 0;
    std::memcpy(&output, &value, sizeof(output));
    return output;
}
void A2FO_CALL logger(const char*, const char* value) {
    record(std::string("log:") + value);
}
void* A2FO_CALL armada() {
    record("armada");
    if (mode == "init-module-throw") throw std::runtime_error("armada throw");
    return mode == "init-no-module" ? nullptr : image;
}
void* A2FO_CALL fleetops() { record("fleetops"); return fleet; }
std::uint32_t A2FO_CALL root_count() {
    record("root-count");
    return mode == "root-too-many" ? 4097 :
        mode == "root-overlay" || mode == "root-disabled" ? 2 : 0;
}
const char* A2FO_CALL root_at(std::uint32_t index) {
    record("root:" + std::to_string(index));
    return fixture_roots[index];
}
bool A2FO_CALL register_class(const char*, const char* const*, std::uint32_t,
        A2FO_WeaponClassLoadedHandler callback, void*) {
    record("register-class");
    class_callback = callback;
    if (mode == "init-class-throw") throw std::runtime_error("class throw");
    return mode != "init-class-false";
}
bool A2FO_CALL register_trigger(const char*, A2FO_WeaponTriggerHandler callback,
                                void*) {
    record("register-trigger");
    trigger_callback = callback;
    if (mode == "init-legacy-trigger-throw")
        throw std::runtime_error("trigger throw");
    return mode != "init-trigger-false";
}
bool A2FO_CALL register_masked(const char*, std::uint32_t mask,
        A2FO_WeaponTriggerHandler callback, void*) {
    record("register-masked:" + std::to_string(mask));
    trigger_callback = callback;
    if (mode == "init-trigger-throw")
        throw std::runtime_error("trigger throw");
    return mode != "init-trigger-false";
}
bool A2FO_CALL install_hook(void* site, void*, std::size_t,
        const std::uint8_t*, A2FO_InlineHook* output) {
    ++hook_count;
    record("hook:" + std::to_string(hook_count) + ":" +
           std::to_string(site == fleet + kFoShipSystemIconRenderRva));
    if (mode == "init-hook-throw" && hook_count == 2)
        throw std::runtime_error("hook throw");
    if (mode == "init-hook-first-false" && hook_count == 1) return false;
    if (mode == "init-hook-third-false" && hook_count == 2) return false;
    output->gateway = reinterpret_cast<void*>(
        hook_count == 1 ? 0x10001 : 0x10002);
    return true;
}
bool A2FO_CALL patch_jump(void*, void*, const std::uint8_t*, std::size_t) {
    record("patch-jump"); return true;
}
template<std::size_t N>
void signature(std::uint8_t* base, std::uintptr_t rva,
               const std::uint8_t (&expected)[N]) {
    std::memcpy(base + rva, expected, N);
}
void seed_image() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x330000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!image || !fleet) throw std::runtime_error("VirtualAlloc");
    signature(image, kWeaponCanFireAtRva, kExpectedWeaponCanFireAt);
    signature(image, kEntityGetWorldTransformRva, kExpectedEntityGetWorldTransform);
    signature(image, kDisplayInterfaceDrawLineRva, kExpectedDisplayInterfaceDrawLine);
    signature(image, kStandardComponentIsMouseOverRva, kExpectedStandardComponentIsMouseOver);
    signature(image, kWeaponGetTargetRva, kExpectedWeaponGetTarget);
    signature(image, kParameterDbGetColorRva, kExpectedParameterDbGetColor);
    signature(fleet, kFoShipSystemIconRenderRva, kExpectedFoShipSystemIconRender);
    void* render = image + kSystemIconRenderRva;
    std::memcpy(image + kSystemIconVtableRva + kSystemIconRenderVtableOffset,
                &render, sizeof(render));
    if (mode == "init-signature") image[kWeaponGetTargetRva] ^= 1;
}
void write_roots() {
    CreateDirectoryA("build\\rust-migration-stage8\\runtime-a-fire-arcs-work", nullptr);
    for (const char* root : fixture_roots) CreateDirectoryA(root, nullptr);
    if (mode == "root-overlay" || mode == "root-disabled") {
        std::ofstream first{std::string(fixture_roots[0]) + "\\RTS_CFG.h"};
        first << "firearc = 0\n";
        std::ofstream second{std::string(fixture_roots[1]) + "\\RTS_CFG.h"};
        second << "firearc = " << (mode == "root-overlay" ? "1" : "0") << "\n";
    }
}
A2FO_ModuleApi make_api() {
    A2FO_ModuleApi api{};
    api.struct_size = mode == "init-short" ? 0 : sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.capabilities = A2FO_CAP_WEAPON_CLASS_LOADED |
                       A2FO_CAP_WEAPON_TRIGGER_EVENTS;
    api.log = logger;
    api.armada_module = armada;
    api.fleetops_module = fleetops;
    api.extension_root_count = root_count;
    api.extension_root = root_at;
    api.register_weapon_class_loaded_handler = register_class;
    api.register_weapon_trigger_handler = register_trigger;
    api.register_weapon_trigger_handler_masked = register_masked;
    api.install_inline_hook = install_hook;
    api.patch_jump = patch_jump;
    if (mode == "init-events-missing") api.capabilities = 0;
    if (mode == "init-legacy-trigger" ||
        mode == "init-legacy-trigger-throw") api.struct_size =
        offsetof(A2FO_ModuleApi, register_weapon_trigger_handler_masked);
    return api;
}
void dump_state() {
    record("ready:" + std::to_string(g_runtime_ready));
    record("hooks:" + std::to_string(hook_count));
    record("classes:" + std::to_string(g_class_arcs.size()));
    record("class-callback:" + std::to_string(class_callback != nullptr));
    record("trigger-callback:" + std::to_string(trigger_callback != nullptr));
}
void* parameter_db = reinterpret_cast<void*>(0x1133);
void class_case() {
    fields = {{"wpnName", "Test Weapon"}, {"classLabel", "CannonImp"}};
    if (mode == "class-box") {
        fields["fireArcCenter"] = "15";
        fields["fireArcWidth"] = "60";
        fields["fireArcPitchAngle"] = "80";
    } else if (mode == "class-alias") {
        fields["fireArcYaw"] = "45";
        fields["fireArcYawAngle"] = "20";
        fields["fireArcMode"] = " BOX ";
    } else if (mode == "class-invalid") {
        fields["fireArcAngle"] = "bad";
    } else if (mode == "class-mode-invalid") {
        fields["fireArcAngle"] = "45";
        fields["fireArcMode"] = "strange";
    } else if (mode == "class-no-coverage") {
        fields["fireArcCenter"] = "25";
    } else if (mode == "class-box-angle") {
        fields["fireArcMode"] = "box";
        fields["fireArcWidth"] = "55";
        fields["fireArcAngle"] = "20";
    } else if (mode == "class-cone-width") {
        fields["fireArcMode"] = "cone";
        fields["fireArcAngle"] = "70";
        fields["fireArcWidth"] = "25";
    } else if (mode.rfind("class-pc", 0) == 0) {
        fields["fireArcCenter"] = "359.99997";
        fields["fireArcWidth"] = "180.000015";
        fields["fireArcPitch"] = "89.99999";
    } else {
        fields["fireArcAngle"] = "60";
    }
    A2FO_WeaponClassLoadedEvent event{};
    event.struct_size = sizeof(event);
    event.weapon_class = klass.data();
    event.parameter_db = parameter_db;
    class_callback(&event, nullptr);
    auto found = g_class_arcs.find(klass.data());
    record("stored:" + std::to_string(found != g_class_arcs.end()));
    if (found != g_class_arcs.end()) {
        const ArcConfig& arc = found->second.arc;
        record("arc:" + std::to_string(static_cast<int>(arc.mode)) + ":" +
            std::to_string(bits(arc.yaw_degrees)) + ":" +
            std::to_string(bits(arc.pitch_degrees)) + ":" +
            std::to_string(bits(arc.yaw_angle_degrees)) + ":" +
            std::to_string(bits(arc.pitch_angle_degrees)) + ":" +
            std::to_string(bits(arc.cone_angle_degrees)));
        record("cannon:" + std::to_string(found->second.cannon_imp));
    }
}
void trigger_case() {
    fields = {{"fireArcAngle", "60"}, {"classLabel", "CannonImp"}};
    A2FO_WeaponClassLoadedEvent loaded{};
    loaded.struct_size = sizeof(loaded);
    loaded.weapon_class = klass.data();
    loaded.parameter_db = parameter_db;
    class_callback(&loaded, nullptr);
    void* class_pointer = klass.data();
    std::memcpy(weapon.data() + kWeaponClassOnWeaponOffset,
                &class_pointer, sizeof(class_pointer));
    float position[3]{0, 0, mode == "trigger-reject" ? -10.0f : 10.0f};
    std::memcpy(target.data() + kPositionOnGameObjectOffset,
                position, sizeof(position));
    if (mode == "trigger-auto")
        klass[kUsePrimaryTargetOnCannonImpClassOffset] = 0;
    else klass[kUsePrimaryTargetOnCannonImpClassOffset] = 1;
    A2FO_WeaponTriggerEvent event{sizeof(event), A2FO_WEAPON_TRIGGER_PRECHECK,
                                   weapon.data(), target.data()};
    if (mode == "trigger-short") event.struct_size = 0;
    if (mode == "trigger-committed") event.kind = A2FO_WEAPON_TRIGGER_COMMITTED;
    if (mode == "trigger-disabled") enabled = false;
    record("trigger:" + std::to_string(trigger_callback(&event, nullptr)));
    record("allow:" + std::to_string(A2FOFireArcs_AllowWeaponTrigger(
        weapon.data(), target.data())));
}
void prepare_weapon() {
    fields = {{"fireArcAngle", "60"}, {"wpnName", "Visible Weapon"}};
    A2FO_WeaponClassLoadedEvent loaded{};
    loaded.struct_size = sizeof(loaded);
    loaded.weapon_class = klass.data();
    loaded.parameter_db = parameter_db;
    class_callback(&loaded, nullptr);
    void* class_pointer = klass.data();
    std::memcpy(weapon.data() + kWeaponClassOnWeaponOffset,
                &class_pointer, sizeof(class_pointer));
    float position[3]{0, 0,
        mode == "canfire-reject" ? -10.0f : 10.0f};
    std::memcpy(target.data() + kPositionOnGameObjectOffset,
                position, sizeof(position));
    klass[kRestrictFireArcOnWeaponClassOffset] = 1;
}
void canfire_case() {
    prepare_weapon();
    if (mode == "canfire-native-false") native_result = false;
    if (mode == "canfire-no-policy") g_class_arcs.clear();
    if (mode == "canfire-disabled") enabled = false;
    const bool result = weapon_can_fire_at_hook(
        weapon.data(), nullptr, craft.data(), target.data(), icon.data());
    record("can-fire:" + std::to_string(result));
    record("native-count:" + std::to_string(native_count));
    record("restriction:" + std::to_string(
        klass[kRestrictFireArcOnWeaponClassOffset]));
}
void __cdecl mock_draw_line(const float* start, const float* end,
                            const float* colour) {
    record("draw:" + std::to_string(bits(start[2])) + ":" +
           std::to_string(bits(end[2])) + ":" +
           std::to_string(bits(colour[0])));
}
void ui_case() {
    prepare_weapon();
    const std::uint8_t jump[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    std::memcpy(image + kDisplayInterfaceDrawLineRva, jump, sizeof(jump));
    void* draw = reinterpret_cast<void*>(&mock_draw_line);
    std::memcpy(image + kDisplayInterfaceDrawLineRva + 1,
                &draw, sizeof(draw));
    void* db = parameter_db;
    std::memcpy(image + kGuiParameterDbPointerRva, &db, sizeof(db));
    void* system_pointer = weapon_system.data();
    std::memcpy(craft.data() + kWeaponSystemOnCraftOffset,
                &system_pointer, sizeof(system_pointer));
    static void* weapons[1];
    weapons[0] = weapon.data();
    void** begin = weapons;
    void** end = weapons + 1;
    std::memcpy(weapon_system.data() + kWeaponVectorBeginOffset,
                &begin, sizeof(begin));
    std::memcpy(weapon_system.data() + kWeaponVectorEndOffset,
                &end, sizeof(end));
    void* craft_pointer = craft.data();
    std::memcpy(icon.data() + kCraftOnShipSystemIconOffset,
                &craft_pointer, sizeof(craft_pointer));
    const std::int32_t index = mode == "ui-invalid-index" ? -1 : 0;
    std::memcpy(icon.data() + kWeaponIndexOnShipSystemIconOffset,
                &index, sizeof(index));
    const std::int32_t system_index = kWeaponsSystemIndex;
    std::memcpy(icon.data() + kSystemIndexOnSystemIconOffset,
                &system_index, sizeof(system_index));
    if (mode == "ui-disabled") enabled = false;
    if (mode == "ui-system")
        system_icon_render_hook(icon.data(), nullptr);
    else
        ship_system_icon_render_hook(icon.data(), nullptr);
    record("ui-loaded:" + std::to_string(g_ui_colours_loaded));
}
void run() {
    if (mode.find("pc24") != std::string::npos)
        _controlfp(_PC_24, _MCW_PC);
    else if (mode.find("pc53") != std::string::npos)
        _controlfp(_PC_53, _MCW_PC);
    else if (mode.find("pc64") != std::string::npos)
        _controlfp(_PC_64, _MCW_PC);
    seed_image();
    write_roots();
    A2FO_ModuleApi api = make_api();
    try {
        record("init:" + std::to_string(A2FO_ModuleInit(&api)));
    } catch (const std::exception& error) {
        record(std::string("throw:") + error.what());
    }
    dump_state();
    if (!g_runtime_ready) return;
    if (mode.rfind("class-", 0) == 0) class_case();
    if (mode.rfind("trigger-", 0) == 0) trigger_case();
    if (mode.rfind("canfire-", 0) == 0) canfire_case();
    if (mode.rfind("ui-", 0) == 0) ui_case();
}
}

namespace a2fo {
bool builtin_module_enabled(BuiltinFeature) noexcept { return enabled; }
std::uint64_t builtin_module_activation(BuiltinFeature) noexcept {
    return enabled ? activation : 0;
}
}

extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_0(
        void* function, void* self) {
    if (function == image + kWeaponGetOwnerRva) {
        record("native-owner"); return reinterpret_cast<std::uintptr_t>(craft.data());
    }
    if (function == image + kEntityGetTransformRva) {
        record("native-transform");
        return reinterpret_cast<std::uintptr_t>(&owner_matrix);
    }
    if (function == image + kWeaponGetTargetRva) {
        record("native-target"); return reinterpret_cast<std::uintptr_t>(target.data());
    }
    if (function == image + kStandardComponentIsMouseOverRva) {
        record("native-hover"); return 1;
    }
    if (function == reinterpret_cast<void*>(0x10001) ||
        function == g_system_icon_render_original) {
        record("native-render:" + std::to_string(self == icon.data()));
        return 1;
    }
    record("native-0-unknown"); return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_2(
        void*, void*, std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_3(
        void* function, void* self, std::uintptr_t one, std::uintptr_t two,
        std::uintptr_t) {
    if (function == image + kParameterDbGetFloatRva) {
        const char* key = reinterpret_cast<const char*>(one);
        record(std::string("float:") + key);
        return 0;
    }
    if (function == image + kParameterDbGetColorRva) {
        record(std::string("color:") + reinterpret_cast<const char*>(one));
        return 0;
    }
    if (function == reinterpret_cast<void*>(0x10002)) {
        ++native_count;
        record("native-can-fire:" + std::to_string(self == weapon.data()) +
               ":" + std::to_string(
                   klass[kRestrictFireArcOnWeaponClassOffset]));
        if (mode == "canfire-mutate") ++activation;
        return native_result;
    }
    (void)two;
    record("native-3-unknown"); return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_4(
        void* function, void*, std::uintptr_t key_address,
        std::uintptr_t value_address, std::uintptr_t capacity,
        std::uintptr_t) {
    if (function != image + kParameterDbGetStringRva) return 0;
    const std::string key = reinterpret_cast<const char*>(key_address);
    record("string:" + key);
    auto found = fields.find(key);
    if (found == fields.end()) return 0;
    std::snprintf(reinterpret_cast<char*>(value_address), capacity,
                  "%s", found->second.c_str());
    return 1;
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    if (argc != 2) return 2;
    mode = argv[1];
    std::set_terminate([] {
        for (const auto& line : trace) std::puts(line.c_str());
        try {
            auto exception = std::current_exception();
            if (exception) std::rethrow_exception(exception);
            std::puts("terminate:none");
        } catch (const std::exception& error) {
            std::printf("terminate:%s\n", error.what());
        } catch (...) {
            std::puts("terminate:other");
        }
        std::fflush(stdout);
        std::_Exit(0);
    });
    try {
        run();
        for (const auto& line : trace) std::puts(line.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fixture: %s\n", error.what());
        return 1;
    }
}
