// Fire-arc built-in lifecycle with synthetic signatures and native-call stubs.
#include "../core/builtin_fire_arcs.cpp"
#include "../core/builtin_modules.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
using namespace a2fo;
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
const std::string roots = "C:\\a2fo-fire-arcs-native-fixture";
unsigned installs = 0, native_calls = 0, native_ui_calls = 0;
bool fail_second = false, native_result = true;
bool native_arguments_ok = true;
void* expected_weapon = nullptr;
std::uintptr_t expected_context = 0, expected_target = 0, expected_distance = 0;
A2FO_WeaponClassLoadedHandler class_callback = nullptr;
A2FO_WeaponTriggerHandler trigger_callback = nullptr;
void require(bool okay, const char* message) { if (!okay) throw std::runtime_error(message); }
void A2FO_CALL log_message(const char*, const char*) {}
void* A2FO_CALL armada_module() { return image; }
void* A2FO_CALL fleet_module() { return fleet; }
std::uint32_t A2FO_CALL root_count() { return 0; }
const char* A2FO_CALL root_at(std::uint32_t) { return nullptr; }
bool toggle_in_native = false;
unsigned geometry_calls = 0;
void choose(bool enabled);
Matrix34 owner_matrix{{1,0,0, 0,1,0, 0,0,1, 0,0,0}};
void* __fastcall native_owner(void* weapon, void*) {
    ++geometry_calls;
    native_arguments_ok &= weapon == expected_weapon;
    return image + 0x380000;
}
void* __fastcall native_transform(void* owner, void*) {
    native_arguments_ok &= owner == image + 0x380000;
    return &owner_matrix;
}
void jump(std::uintptr_t rva, void* destination) {
    const std::uint8_t code[]{0xb8,0,0,0,0,0xff,0xe0};
    std::memcpy(image+rva,code,sizeof(code));
    std::memcpy(image+rva+1,&destination,sizeof(destination));
}
extern "C" std::uintptr_t __cdecl native_can_fire(
    void* weapon, std::uintptr_t context, std::uintptr_t target,
    std::uintptr_t distance) {
    ++native_calls;
    native_arguments_ok &= weapon == expected_weapon && context == expected_context &&
        target == expected_target && distance == expected_distance;
    if(toggle_in_native) {toggle_in_native=false;choose(false);choose(true);}
    return native_result;
}
extern "C" __attribute__((naked)) void native_can_fire_bridge() {
    __asm__ volatile("pushl 12(%esp)\npushl 12(%esp)\npushl 12(%esp)\n"
                     "pushl %ecx\ncall _native_can_fire\naddl $16,%esp\nret $12");
}
extern "C" void __cdecl native_ui(void* icon) {
    ++native_ui_calls;
    native_arguments_ok &= icon == expected_weapon;
}
extern "C" __attribute__((naked)) void native_ui_bridge() {
    __asm__ volatile("pushl %ecx\ncall _native_ui\naddl $4,%esp\nret");
}
bool A2FO_CALL register_class(const char*, const char* const*, std::uint32_t,
                              A2FO_WeaponClassLoadedHandler callback, void*) {
    class_callback = callback; return true;
}
bool A2FO_CALL register_trigger(const char*, std::uint32_t mask,
                                A2FO_WeaponTriggerHandler callback, void*) {
    require(mask == A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK,
            "fire-arc trigger registration mask");
    trigger_callback = callback; return true;
}
bool A2FO_CALL patch_jump(void*, void*, const std::uint8_t*, std::size_t) { return true; }
bool A2FO_CALL install_hook(void* target, void* replacement, std::size_t length,
                            const std::uint8_t* signature, A2FO_InlineHook* hook) {
    require(installs < 2 && replacement &&
            target == (installs == 0 ? fleet + kFoShipSystemIconRenderRva :
                                      image + kWeaponCanFireAtRva) &&
            std::memcmp(target, signature, length) == 0,
            "fire-arc hook signature and site");
    ++installs;
    if (fail_second && installs == 2) return false;
    hook->target = target;
    hook->gateway = installs == 1 ? reinterpret_cast<void*>(&native_ui_bridge) :
                                    reinterpret_cast<void*>(&native_can_fire_bridge);
    hook->length = length;
    return true;
}
template<std::size_t N> void signature(std::uint8_t* base, std::uintptr_t rva,
                                       const std::uint8_t (&bytes)[N]) {
    std::memcpy(base + rva, bytes, N);
}
void seed(bool bad_signature) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x330000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(image && fleet, "fire-arc synthetic images");
    signature(image, kWeaponCanFireAtRva, kExpectedWeaponCanFireAt);
    signature(image, kEntityGetWorldTransformRva, kExpectedEntityGetWorldTransform);
    signature(image, kDisplayInterfaceDrawLineRva, kExpectedDisplayInterfaceDrawLine);
    signature(image, kStandardComponentIsMouseOverRva, kExpectedStandardComponentIsMouseOver);
    signature(image, kWeaponGetTargetRva, kExpectedWeaponGetTarget);
    signature(image, kParameterDbGetColorRva, kExpectedParameterDbGetColor);
    signature(fleet, kFoShipSystemIconRenderRva, kExpectedFoShipSystemIconRender);
    void* native_render = image + kSystemIconRenderRva;
    std::memcpy(image + kSystemIconVtableRva + kSystemIconRenderVtableOffset,
                &native_render, sizeof(native_render));
    if (bad_signature) image[kWeaponGetTargetRva] ^= 1;
}
void choose(bool enabled) {
    ModulePolicy policy;
    policy.managed = true;
    ModulePolicyEntry entry;
    entry.name = "A2FOFireArcs";
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    policy.entries.push_back(entry);
    std::string error;
    require(apply_builtin_module_policy({roots}, policy, error),
            error.empty() ? "fire-arc policy" : error.c_str());
    require(builtin_module_enabled(BuiltinFeature::fire_arcs) == enabled,
            "fire-arc gate");
}
void run(const std::string& mode) {
    fail_second = mode == "partial";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.capabilities = A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_WEAPON_TRIGGER_EVENTS;
    api.log = log_message; api.armada_module = armada_module;
    api.fleetops_module = fleet_module; api.extension_root_count = root_count;
    api.extension_root = root_at; api.install_inline_hook = install_hook;
    api.patch_jump = patch_jump;
    api.register_weapon_class_loaded_handler = register_class;
    api.register_weapon_trigger_handler_masked = register_trigger;
    api.register_weapon_trigger_handler = +[](const char*, A2FO_WeaponTriggerHandler callback,
                                               void*) -> bool { trigger_callback = callback; return true; };
    set_builtin_module_roots({roots});
    const bool prepared = mode != "signature" && mode != "partial";
    require(prepare_builtin_module(BuiltinFeature::fire_arcs, api, false) == prepared,
            "fire-arc preparation");
    finish_builtin_module_preparation(false, mode != "dispatch", mode != "dispatch",
        false, false);
    const bool available = prepared && mode != "dispatch";
    require(builtin_module_available(BuiltinFeature::fire_arcs) == available,
            "fire-arc dispatch availability");
    require(installs == (mode == "signature" ? 0u : mode == "partial" ? 2u : 2u),
            "fire-arc hook count");
    if (!available) {
        ModulePolicy request;
        ModulePolicyEntry entry;
        entry.name = "A2FOFireArcs"; entry.state = ModulePolicyState::active;
        request.entries.push_back(entry);
        std::string error;
        require(!apply_builtin_module_policy({roots}, request, error) && !error.empty(),
                "unavailable fire-arc activation rejected");
        if (g_weapon_can_fire_at_original) {
            expected_weapon = image + 0x100;
            expected_context = reinterpret_cast<std::uintptr_t>(image + 0x120);
            expected_target = reinterpret_cast<std::uintptr_t>(image + 0x140);
            expected_distance = reinterpret_cast<std::uintptr_t>(image + 0x160);
            require(weapon_can_fire_at_hook(expected_weapon, nullptr,
                image + 0x120, image + 0x140, image + 0x160) == native_result,
                "failed fire-arc hook passes native result");
            require(native_arguments_ok, "failed fire-arc native arguments");
        }
        if (g_ship_system_icon_render_original) {
            expected_weapon = image + 0x100;
            ship_system_icon_render_hook(expected_weapon, nullptr);
            require(native_ui_calls == 1 && native_arguments_ok,
                    "partial fire-arc UI hook forwards native");
        }
        return;
    }
    require(class_callback && trigger_callback, "fire-arc callbacks retained");
    std::array<std::uint8_t, 0x300> weapon{};
    std::array<std::uint8_t, 0x300> klass{};
    void* klass_pointer = klass.data();
    std::memcpy(weapon.data() + kWeaponClassOnWeaponOffset,
                &klass_pointer, sizeof(klass_pointer));
    ClassArcPolicy policy{};
    policy.arc.mode = ArcMode::cone;
    g_class_arcs[klass.data()] = policy;
    expected_weapon = weapon.data();
    expected_context = reinterpret_cast<std::uintptr_t>(image + 0x120);
    expected_target = reinterpret_cast<std::uintptr_t>(image + 0x140);
    expected_distance = reinterpret_cast<std::uintptr_t>(image + 0x160);
    choose(false);
    require(configured_arc(expected_weapon, nullptr) == nullptr,
            "disabled fire arc does not select custom policy");
    require(weapon_can_fire_at_hook(expected_weapon, nullptr,
            reinterpret_cast<void*>(expected_context),
            reinterpret_cast<void*>(expected_target),
            reinterpret_cast<void*>(expected_distance)) == native_result &&
            native_calls == 1 && native_arguments_ok,
            "fire-arc off forwards native decision and arguments");
    native_result = false;
    require(!weapon_can_fire_at_hook(expected_weapon, nullptr,
            reinterpret_cast<void*>(expected_context),
            reinterpret_cast<void*>(expected_target),
            reinterpret_cast<void*>(expected_distance)) && native_calls == 2 &&
            native_arguments_ok,
            "fire-arc off preserves native rejection");
    native_result = true;
    require(g_class_arcs.count(klass.data()), "weapon class arc metadata retained off");
    choose(true);
    require(runtime_enabled() && configured_arc(expected_weapon, nullptr) &&
            g_class_arcs.count(klass.data()),
            "fire-arc activation reuses class metadata");
    jump(kWeaponGetOwnerRva,reinterpret_cast<void*>(&native_owner));
    jump(kEntityGetTransformRva,reinterpret_cast<void*>(&native_transform));
    std::array<std::uint8_t,0x100> target{};
    float position[3]{0,0,-10};
    std::memcpy(target.data()+kPositionOnGameObjectOffset,position,sizeof(position));
    expected_target=reinterpret_cast<std::uintptr_t>(target.data());
    g_class_arcs[klass.data()].arc.cone_angle_degrees=60;
    klass[kRestrictFireArcOnWeaponClassOffset]=1;
    require(!weapon_can_fire_at_hook(expected_weapon,nullptr,
            reinterpret_cast<void*>(expected_context),target.data(),
            reinterpret_cast<void*>(expected_distance)) && geometry_calls==1 &&
            klass[kRestrictFireArcOnWeaponClassOffset]==1 && native_arguments_ok,
            "active arc denies rear target and restores stock restriction");
    position[2]=10;
    std::memcpy(target.data()+kPositionOnGameObjectOffset,position,sizeof(position));
    require(weapon_can_fire_at_hook(expected_weapon,nullptr,
            reinterpret_cast<void*>(expected_context),target.data(),
            reinterpret_cast<void*>(expected_distance)) && geometry_calls==2,
            "active arc admits forward target");
    position[2]=-10;
    std::memcpy(target.data()+kPositionOnGameObjectOffset,position,sizeof(position));
    toggle_in_native=true;
    require(weapon_can_fire_at_hook(expected_weapon,nullptr,
            reinterpret_cast<void*>(expected_context),target.data(),
            reinterpret_cast<void*>(expected_distance)) && geometry_calls==2 &&
            klass[kRestrictFireArcOnWeaponClassOffset]==1,
            "generation change in native call discards stale directional decision");
    choose(false);
    A2FO_WeaponTriggerEvent trigger{sizeof(trigger), A2FO_WEAPON_TRIGGER_PRECHECK,
                                    weapon.data(), nullptr};
    require(trigger_callback(&trigger, nullptr), "fire-arc off permits native trigger");
}
}
int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    if (argc != 2) return 2;
    try { run(argv[1]); std::printf("builtin fire arcs %s: PASS\n", argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "builtin fire arcs %s: FAIL: %s\n", argv[1], error.what());
        return 1;
    }
}
