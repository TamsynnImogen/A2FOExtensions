// Synthetic, headless built-in ODF Variants lifecycle fixture.
#include "../core/builtin_odf_variants.cpp"
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
std::string root = "C:\\a2fo-odf-variants-fixture";
unsigned registrations = 0;
bool reject_registration = false;
A2FO_RaceLoadedHandler race_callback = nullptr;
A2FO_CraftEventHandler craft_callback = nullptr;
std::vector<std::string> queried_classes;
void* __cdecl native_find(const char* name) {
    queried_classes.emplace_back(name);
    return std::strcmp(name,"ship")==0 ? reinterpret_cast<void*>(0x12340000) : nullptr;
}
const char* __fastcall native_odf_name(void*,void*) {
    static char name[256] = "ship_b";
    return name;
}
void* __fastcall native_transform(void*,void*) { return nullptr; }
void jump(std::uintptr_t rva, void* function) {
    const std::uint8_t bytes[]{0x68,0,0,0,0,0xc3};
    std::memcpy(engine+rva,bytes,sizeof(bytes));
    std::memcpy(engine+rva+1,&function,sizeof(function));
    FlushInstructionCache(GetCurrentProcess(),engine+rva,sizeof(bytes));
}

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void A2FO_CALL log_message(const char*, const char*) {}
void* A2FO_CALL armada_module() { return engine; }
bool A2FO_CALL race_registration(const char*, const char* const* fields,
                                 std::uint32_t count, A2FO_RaceLoadedHandler callback,
                                 void*) {
    ++registrations;
    require(fields && count == 2 && callback &&
            std::strcmp(fields[0], "name") == 0,
            "ODF race registration");
    race_callback = callback;
    return !reject_registration;
}
bool A2FO_CALL craft_registration(const char*, std::uint32_t mask,
                                  A2FO_CraftEventHandler callback, void*) {
    ++registrations;
    require(mask == (A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                     A2FO_CRAFT_EVENT_MASK_CLEANUP |
                     A2FO_CRAFT_EVENT_MASK_POST_LOAD) && callback,
            "ODF craft registration");
    craft_callback = callback;
    return true;
}
void seed(bool bad_signature) {
    engine = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    require(engine != nullptr, "ODF image allocation");
    const auto signature = [](std::uint8_t* image, std::uintptr_t rva,
                              const auto& bytes) {
        std::memcpy(image + rva, bytes.data(), bytes.size());
    };
    signature(engine, kAiMissionGetCurrentRva, kExpectedAiMissionGetCurrent);
    signature(engine, kGameObjectClassFindRva, kExpectedGameObjectClassFind);
    signature(engine, kGameObjectClassConstructRva, kExpectedGameObjectClassConstruct);
    signature(engine, kGameObjectClassGetOdfNameRva, kExpectedGameObjectClassGetOdfName);
    signature(engine, kGameObjectGetTransformRva, kExpectedGameObjectGetTransform);
    signature(engine, kEvolverSwapObjectsRva, kExpectedEvolverSwapObjects);
    if (bad_signature) engine[kGameObjectClassFindRva] ^= 1;
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
            error.empty() ? "apply ODF policy" : error.c_str());
    require(builtin_module_enabled(BuiltinFeature::odf_variants) == enabled,
            "ODF gate state");
}
void run(const std::string& mode) {
    reject_registration = mode == "registration";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.capabilities = A2FO_CAP_RACE_LOADED | A2FO_CAP_CRAFT_EVENTS;
    api.log = log_message; api.armada_module = armada_module;
    api.register_race_loaded_handler = race_registration;
    api.register_craft_event_handler_masked = craft_registration;
    api.register_craft_event_handler = [](const char*, A2FO_CraftEventHandler,
                                          void*) -> bool { return true; };
    set_builtin_module_roots({root});
    const bool prepared = mode != "signature" && mode != "registration";
    require(prepare_builtin_module(BuiltinFeature::odf_variants, api, false) == prepared,
            "ODF preparation");
    require(registrations == (mode == "signature" ? 0u :
                              mode == "registration" ? 1u : 2u),
            "ODF registration sequence");
    finish_builtin_module_preparation(true, true, true, true, true,
                                      true, mode != "dispatch", true);
    const bool available = prepared && mode != "dispatch";
    require(builtin_module_available(BuiltinFeature::odf_variants) == available,
            "ODF dispatcher availability");
    if (!available) {
        ModulePolicy request;
        ModulePolicyEntry entry;
        entry.name = kModuleName; entry.state = ModulePolicyState::active;
        request.entries.push_back(entry);
        std::string error;
        require(!apply_builtin_module_policy({root}, request, error) && !error.empty() &&
                !builtin_module_enabled(BuiltinFeature::odf_variants),
                "unavailable ODF activation rejected");
        return;
    }
    require(race_callback && craft_callback, "ODF callbacks captured");
    std::array<std::uint8_t, 0x300> craft{};
    std::array<std::uint8_t, 0x40> race{};
    const char key[] = "name", value[] = "Borg";
    const A2FO_OdfFieldView fields[] = {{{key, sizeof(key)-1},
                                          {value, sizeof(value)-1}}};
    A2FO_RaceLoadedEvent loaded{sizeof(loaded), race.data(), fields, 1};
    for (bool enabled : {false, true, false, true}) {
        choose(enabled);
        race_callback(&loaded, nullptr);
        require(g_race_suffixes[race.data()] == "_b",
                "ODF Race metadata retained while off");
        g_last_races[craft.data()] = race.data();
        g_base_odfs[craft.data()] = "fixture";
        g_pending_renderer_refresh_crafts.insert(craft.data());
        A2FO_CraftEvent cleanup{sizeof(cleanup), A2FO_CRAFT_EVENT_CLEANUP,
                                craft.data(), 0.0f};
        craft_callback(&cleanup, nullptr);
        require(g_last_races.count(craft.data()) == 0 &&
                g_base_odfs.count(craft.data()) == 0 &&
                g_pending_renderer_refresh_crafts.count(craft.data()) == 0,
                "ODF cleanup while off or on");
    }
    g_last_races[craft.data()] = race.data();
    g_base_odfs[craft.data()] = "fixture";
    g_pending_renderer_refresh_crafts.insert(craft.data());
    choose(false); choose(true);
    require(runtime_enabled(), "ODF activation after final toggle");
    require(g_pending_renderer_refresh_crafts.empty() &&
            g_last_races.count(craft.data()) == 1 &&
            g_base_odfs[craft.data()] == "fixture" &&
            g_race_suffixes[race.data()] == "_b",
            "ODF generation clears pending refresh but preserves metadata");
    // Replay a first simulation observation of an old Borg variant now owned
    // by another race. The native constructor is never called: a synthetic
    // transform getter returns null after the actual class-name resolution.
    jump(kGameObjectClassFindRva,reinterpret_cast<void*>(&native_find));
    jump(kGameObjectClassGetOdfNameRva,reinterpret_cast<void*>(&native_odf_name));
    jump(kGameObjectGetTransformRva,reinterpret_cast<void*>(&native_transform));
    std::array<std::uint8_t,0x40> new_race{}, object_class{};
    g_race_suffixes[new_race.data()]="_r";
    void* cls=object_class.data(); void* owner=new_race.data();
    std::memcpy(craft.data()+kObjectClassOffset,&cls,sizeof(cls));
    std::memcpy(craft.data()+kObjectRaceOffset,&owner,sizeof(owner));
    g_last_races.erase(craft.data()); g_base_odfs.erase(craft.data());
    A2FO_CraftEvent simulate{sizeof(simulate),A2FO_CRAFT_EVENT_SIMULATE_POST,craft.data(),0.1f};
    choose(false); craft_callback(&simulate,nullptr);
    require(queried_classes.empty(),"disabled ODF simulation never resolves variants");
    choose(true); craft_callback(&simulate,nullptr);
    require(g_base_odfs[craft.data()]=="ship" &&
            std::find(queried_classes.begin(),queried_classes.end(),"ship_r")!=queried_classes.end() &&
            std::find(queried_classes.begin(),queried_classes.end(),"ship_b_r")==queried_classes.end(),
            "first simulation infers canonical base without stacking owner suffixes");
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try { run(argv[1]); std::printf("builtin ODF variants %s: PASS\n", argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "builtin ODF variants %s: FAIL: %s\n", argv[1], error.what());
        return 1;
    }
}
