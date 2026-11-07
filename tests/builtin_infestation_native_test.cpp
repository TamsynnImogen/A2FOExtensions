// Synthetic, headless built-in Infestation lifecycle fixture.
#include "../core/builtin_infestation.cpp"
#include "../core/builtin_modules.hpp"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
using namespace a2fo;
std::uint8_t* engine = nullptr;
std::string root = "C:\\a2fo-infestation-fixture";
unsigned registrations = 0;
bool reject_registration = false;
A2FO_GameObjectClassLoadedHandler class_callback = nullptr;
A2FO_WeaponClassLoadedHandler weapon_callback = nullptr;
A2FO_WeaponTriggerHandler trigger_callback = nullptr;
A2FO_CraftEventHandler craft_callback = nullptr;

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void A2FO_CALL log_message(const char*, const char*) {}
void* A2FO_CALL armada_module() { return engine; }
bool A2FO_CALL class_registration(const char*, const char* const* fields,
                                  std::uint32_t count,
                                  A2FO_GameObjectClassLoadedHandler callback, void*) {
    ++registrations;
    require(fields && count == kObjectFields.size() && callback,
            "infestation class registration");
    class_callback = callback; return !reject_registration;
}
bool A2FO_CALL weapon_registration(const char*, const char* const* fields,
                                   std::uint32_t count,
                                   A2FO_WeaponClassLoadedHandler callback, void*) {
    ++registrations;
    require(fields && count == kWeaponFields.size() && callback,
            "infestation weapon registration");
    weapon_callback = callback; return true;
}
bool A2FO_CALL trigger_registration(const char*, std::uint32_t mask,
                                    A2FO_WeaponTriggerHandler callback, void*) {
    ++registrations;
    require(mask == A2FO_WEAPON_TRIGGER_EVENT_MASK_ALL && callback,
            "infestation trigger registration");
    trigger_callback = callback; return true;
}
bool A2FO_CALL craft_registration(const char*, std::uint32_t mask,
                                  A2FO_CraftEventHandler callback, void*) {
    ++registrations;
    require(mask == (A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                     A2FO_CRAFT_EVENT_MASK_CLEANUP |
                     A2FO_CRAFT_EVENT_MASK_POST_LOAD) && callback,
            "infestation craft registration");
    craft_callback = callback; return true;
}
template<std::size_t N> void signature(std::uintptr_t rva,
                                       const std::uint8_t (&bytes)[N]) {
    std::memcpy(engine + rva, bytes, N);
}
void seed(bool bad_signature) {
    engine = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    require(engine != nullptr, "infestation image allocation");
    signature(kAiMissionGetCurrentRva, kExpectedAiMissionGetCurrent);
    signature(kCraftGetRva, kExpectedCraftGet);
    signature(kExplosionClassFindRva, kExpectedExplosionClassFind);
    signature(kCraftDamageRva, kExpectedCraftDamage);
    signature(kGameObjectClassFindRva, kExpectedGameObjectClassFind);
    signature(kGameObjectClassConstructRva, kExpectedGameObjectClassConstruct);
    signature(kEntityGetTransformRva, kExpectedEntityGetTransform);
    signature(kEntityGetRva, kExpectedEntityGet);
    signature(kParameterDbGetProjectIdRva, kExpectedParameterDbGetProjectId);
    signature(kDequeueCommandVectorRva, kExpectedDequeueCommandVector);
    signature(kWeaponGetOwnerRva, kExpectedWeaponGetOwner);
    if (bad_signature) engine[kCraftGetRva] ^= 1;
}
void choose(bool enabled) {
    ModulePolicy policy;
    policy.managed = true;
    ModulePolicyEntry entry;
    entry.name = kModuleName;
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    policy.entries.push_back(entry);
    std::string error;
    require(apply_builtin_module_policy({root}, policy, error),
            error.empty() ? "apply infestation policy" : error.c_str());
    require(builtin_module_enabled(BuiltinFeature::infestation) == enabled,
            "infestation gate state");
}
void run(const std::string& mode) {
    reject_registration = mode == "registration";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.capabilities = A2FO_CAP_GAME_OBJECT_CLASS_LOADED |
        A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_WEAPON_TRIGGER_EVENTS |
        A2FO_CAP_CRAFT_EVENTS;
    api.log = log_message; api.armada_module = armada_module;
    api.register_game_object_class_loaded_handler = class_registration;
    api.register_weapon_class_loaded_handler = weapon_registration;
    api.register_weapon_trigger_handler_masked = trigger_registration;
    api.register_craft_event_handler_masked = craft_registration;
    set_builtin_module_roots({root});
    const bool prepared = mode != "signature" && mode != "registration";
    require(prepare_builtin_module(BuiltinFeature::infestation, api, false) == prepared,
            "infestation preparation");
    require(registrations == (mode == "signature" ? 0u : 4u),
            "infestation registration sequence");
    finish_builtin_module_preparation(true, true, mode != "dispatch", true,
        mode != "dispatch", mode != "dispatch", true, true);
    const bool available = prepared && mode != "dispatch";
    require(builtin_module_available(BuiltinFeature::infestation) == available,
            "infestation dispatcher availability");
    if (!available) {
        ModulePolicy request;
        ModulePolicyEntry entry;
        entry.name = kModuleName; entry.state = ModulePolicyState::active;
        request.entries.push_back(entry);
        std::string error;
        require(!apply_builtin_module_policy({root}, request, error) && !error.empty() &&
                !builtin_module_enabled(BuiltinFeature::infestation),
                "unavailable infestation activation rejected");
        return;
    }
    require(class_callback && weapon_callback && trigger_callback && craft_callback,
            "infestation callbacks captured");
    std::array<std::uint8_t, 0x40> weapon{};
    std::array<std::uint8_t, 0x300> craft{};
    std::array<std::uint8_t, 0x20> klass{};
    void* klass_pointer = klass.data();
    const std::uint32_t handle = 42;
    std::memcpy(weapon.data() + kWeaponClassOffset, &klass_pointer, sizeof(klass_pointer));
    std::memcpy(craft.data() + kObjectHandleOffset, &handle, sizeof(handle));
    const char key[] = "noxterInfestation", value[] = "1";
    const A2FO_OdfFieldView fields[] = {{{key, sizeof(key)-1},
                                          {value, sizeof(value)-1}}};
    A2FO_WeaponClassLoadedEvent loaded{sizeof(loaded), klass.data(), nullptr,
                                       nullptr, fields, 1};
    A2FO_WeaponTriggerEvent precheck{sizeof(precheck),
        A2FO_WEAPON_TRIGGER_PRECHECK, weapon.data(), nullptr};
    for (bool enabled : {false, true, false, true}) {
        choose(enabled);
        weapon_callback(&loaded, nullptr);
        require(g_weapon_policies.count(klass.data()) == 1,
                "infestation metadata captured while off");
        require(trigger_callback(&precheck, nullptr) == !enabled,
                "infestation precheck gate with invalid target");
        Infection infection{};
        infection.target_handle = handle;
        g_infections[handle] = infection;
        g_larva_routes[handle] = LarvaRoute{};
        A2FO_CraftEvent cleanup{sizeof(cleanup), A2FO_CRAFT_EVENT_CLEANUP,
                                craft.data(), 0.0f};
        craft_callback(&cleanup, nullptr);
        require(g_infections.empty() && g_larva_routes.empty(),
                "infestation cleanup while off or on");
    }
    g_infections[handle] = Infection{};
    g_larva_routes[handle] = LarvaRoute{};
    choose(false); choose(true);
    require(runtime_enabled(), "infestation activation after final toggle");
    require(g_infections.empty() && g_larva_routes.empty() &&
            g_weapon_policies.count(klass.data()) == 1,
            "infestation generation clears only transient state");
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try { run(argv[1]); std::printf("builtin infestation %s: PASS\n", argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "builtin infestation %s: FAIL: %s\n", argv[1], error.what());
        return 1;
    }
}
