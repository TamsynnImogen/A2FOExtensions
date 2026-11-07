// Synthetic, headless built-in Breeders lifecycle fixture. No engine entry point.
#include "../core/builtin_breeders.cpp"
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
std::uint8_t* fleet = nullptr;
std::string root = "C:\\a2fo-breeders-fixture";
unsigned registrations = 0;
bool reject_registration = false;
A2FO_GameObjectClassLoadedHandler class_callback = nullptr;
A2FO_ProducerEventHandler producer_callback = nullptr;
A2FO_CraftEventHandler craft_callback = nullptr;

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void A2FO_CALL log_message(const char*, const char*) {}
void* A2FO_CALL armada_module() { return engine; }
void* A2FO_CALL fleetops_module() { return fleet; }
bool A2FO_CALL alias(const char* owner, const char* source, const char* target) {
    ++registrations;
    require(std::strcmp(owner, kModuleName) == 0 &&
            std::strcmp(source, "breeder") == 0 && std::strcmp(target, "shipyard") == 0,
            "breeder classlabel alias");
    return !reject_registration;
}
bool A2FO_CALL defaults(const char* owner, const char* label,
                        const A2FO_ClasslabelOdfDefault* values, std::uint32_t count) {
    ++registrations;
    require(std::strcmp(owner, kModuleName) == 0 &&
            std::strcmp(label, "breeder") == 0 && values && count == 5,
            "breeder default registration");
    return true;
}
bool A2FO_CALL class_registration(const char*, const char* const* fields,
                                  std::uint32_t count,
                                  A2FO_GameObjectClassLoadedHandler callback, void*) {
    ++registrations;
    require(fields && count == kRequiredFields.size() && callback,
            "breeder class registration");
    class_callback = callback;
    return true;
}
bool A2FO_CALL producer_registration(const char*, A2FO_ProducerEventHandler callback,
                                     void*) {
    ++registrations;
    require(callback != nullptr, "breeder producer registration");
    producer_callback = callback;
    return true;
}
bool A2FO_CALL craft_registration(const char*, std::uint32_t mask,
                                  A2FO_CraftEventHandler callback, void*) {
    ++registrations;
    require(mask == (A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                     A2FO_CRAFT_EVENT_MASK_CLEANUP |
                     A2FO_CRAFT_EVENT_MASK_POST_LOAD) && callback,
            "breeder craft registration");
    craft_callback = callback;
    return true;
}
template<std::size_t N> void signature(std::uint8_t* base, std::uintptr_t rva,
                                       const std::uint8_t (&bytes)[N]) {
    std::memcpy(base + rva, bytes, N);
}
void seed(bool bad_signature) {
    engine = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x130000,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    require(engine && fleet, "breeder fixture allocation");
    signature(engine, kAiMissionGetCurrentRva, kExpectedAiMissionGetCurrent);
    signature(engine, kGameObjectClassFindRva, kExpectedGameObjectClassFind);
    signature(engine, kGameObjectClassConstructRva, kExpectedGameObjectClassConstruct);
    signature(engine, kEntityGetTransformRva, kExpectedEntityGetTransform);
    signature(engine, kEntityGetRva, kExpectedEntityGet);
    signature(engine, kDequeueCommandVectorRva, kExpectedDequeueCommandVector);
    signature(engine, kEvolverSwapObjectsRva, kExpectedEvolverSwapObjects);
    signature(fleet, kFoProducerPushCheckedRva, kExpectedFoPushChecked);
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
            error.empty() ? "apply breeder policy" : error.c_str());
    require(builtin_module_enabled(BuiltinFeature::breeders) == enabled,
            "breeder gate state");
}
void run(const std::string& mode) {
    reject_registration = mode == "registration";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.capabilities = A2FO_CAP_CLASSLABEL_ODF_DEFAULTS |
        A2FO_CAP_GAME_OBJECT_CLASS_LOADED | A2FO_CAP_PRODUCER_EVENTS |
        A2FO_CAP_CRAFT_EVENTS;
    api.log = log_message; api.armada_module = armada_module;
    api.fleetops_module = fleetops_module;
    api.register_classlabel_alias = alias;
    api.register_classlabel_odf_defaults = defaults;
    api.register_game_object_class_loaded_handler = class_registration;
    api.register_producer_event_handler = producer_registration;
    api.register_craft_event_handler_masked = craft_registration;
    // Older API slot remains required even when the masked registration is used.
    api.register_craft_event_handler = [](const char*, A2FO_CraftEventHandler, void*) -> bool { return true; };
    set_builtin_module_roots({root});
    const bool prepared = mode != "signature" && mode != "registration";
    require(prepare_builtin_module(BuiltinFeature::breeders, api, false) == prepared,
            "breeder preparation");
    require(registrations == (mode == "signature" ? 0u : 5u),
            "breeder registration sequence");
    finish_builtin_module_preparation(true, true, true, true, true,
        mode != "dispatch", true, mode != "dispatch");
    const bool available = prepared && mode != "dispatch";
    require(builtin_module_available(BuiltinFeature::breeders) == available,
            "breeder dispatcher availability");
    if (!available) {
        ModulePolicy request;
        ModulePolicyEntry entry;
        entry.name = kModuleName; entry.state = ModulePolicyState::active;
        request.entries.push_back(entry);
        std::string error;
        require(!apply_builtin_module_policy({root}, request, error) && !error.empty() &&
                !builtin_module_enabled(BuiltinFeature::breeders),
                "unavailable breeder activation rejected");
        return;
    }
    require(class_callback && producer_callback && craft_callback,
            "breeder callbacks captured");
    std::array<std::uint8_t, 0x300> producer{};
    std::array<std::uint8_t, 0x100> klass{};
    const std::uint32_t handle = 42;
    std::memcpy(producer.data() + kObjectHandleOffset, &handle, sizeof(handle));
    void* producer_class = klass.data();
    std::memcpy(producer.data() + kObjectClassOffset, &producer_class, sizeof(producer_class));
    const char breeder_key[] = "breederClass", yes[] = "1";
    const char produces_key[] = "breederProduces", target[] = "target.odf";
    const A2FO_OdfFieldView fields[] = {
        {{breeder_key, sizeof(breeder_key)-1}, {yes, sizeof(yes)-1}},
        {{produces_key, sizeof(produces_key)-1}, {target, sizeof(target)-1}}};
    A2FO_GameObjectClassLoadedEvent loaded{sizeof(loaded), klass.data(), {}, fields, 2};
    A2FO_ProducerEvent admit{sizeof(admit), A2FO_PRODUCER_EVENT_ADMIT,
                             producer.data(), reinterpret_cast<void*>(0x1234)};
    const auto make_craft = [&](std::uint32_t kind) {
        A2FO_CraftEvent event{sizeof(A2FO_CraftEvent), kind, producer.data(), 0.1f};
        craft_callback(&event, nullptr);
    };
    for (bool enabled : {false, true, false, true}) {
        choose(enabled);
        class_callback(&loaded, nullptr);
        require(g_class_policies.count(klass.data()) == 1,
                "breeder class metadata captured while off");
        require(producer_callback(&admit, nullptr) == !enabled,
                "committed breeder admission follows gate");
        g_waiting[42] = WaitingSeed{};
        g_production_retry[42] = 1.0f;
        make_craft(A2FO_CRAFT_EVENT_CLEANUP);
        // Cleanup is safe for synthetic craft and leaves no queued work.
        require(g_production_retry.count(42) == 0,
                "breeder cleanup drops transient retry");
    }
    choose(false);
    choose(true);
    require(runtime_enabled(), "breeder activation after final toggle");
    require(g_waiting.empty() && g_production_retry.empty() &&
            g_class_policies.count(klass.data()) == 1,
            "breeder generation clears only transient state");
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try { run(argv[1]); std::printf("builtin breeders %s: PASS\n", argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "builtin breeders %s: FAIL: %s\n", argv[1], error.what());
        return 1;
    }
}
