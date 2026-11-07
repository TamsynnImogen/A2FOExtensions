// Headless shield observer fixture. Only an explicit test callback jump is executed.
#include "../core/builtin_always_shields.cpp"
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
const std::string roots = "C:\\a2fo-shields-native-fixture";
unsigned installs = 0, native_simulates = 0, native_publishes = 0, native_stops = 0;
bool fail_second = false;
bool native_arguments_ok = true;
std::int32_t stopped_id = -1;
void* expected_self = nullptr;
std::uintptr_t expected_argument = 0;
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void A2FO_CALL log_message(const char*, const char*) {}
void* A2FO_CALL armada_module() { return image; }
void* A2FO_CALL fleet_module() { return fleet; }
std::uint32_t A2FO_CALL root_count() { return 0; }
const char* A2FO_CALL root_at(std::uint32_t) { return nullptr; }
std::uintptr_t __cdecl native_publish(void* mission, void* object) {
    ++native_publishes;
    native_arguments_ok &= mission == expected_self &&
        object == reinterpret_cast<void*>(expected_argument);
    return 0x12345678u;
}
extern "C" void __cdecl native_simulate(void* self, std::uintptr_t argument) {
    ++native_simulates;
    native_arguments_ok &= self == expected_self && argument == expected_argument;
}
extern "C" __attribute__((naked)) void native_simulate_bridge() {
    __asm__ volatile("pushl 4(%esp)\npushl %ecx\ncall _native_simulate\n"
                     "addl $8,%esp\nret $4");
}
extern "C" void __cdecl native_stop(std::int32_t effect_id) {
    ++native_stops;
    stopped_id = effect_id;
}
void callback_jump(std::uintptr_t rva, void* callback) {
    static_assert(sizeof(callback) == 4, "synthetic native fixture requires i386");
    const std::uint8_t instruction[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    std::memcpy(image + rva, instruction, sizeof(instruction));
    std::memcpy(image + rva + 1, &callback, sizeof(callback));
}
bool A2FO_CALL install_hook(void* target, void* replacement, std::size_t length,
                            const std::uint8_t* signature, A2FO_InlineHook* hook) {
    const std::uintptr_t rvas[]{kStarbaseSimulateRva, kRenderGameObjectsRva,
                                kGameObjectPublishRva};
    require(installs < 3 && target == image + rvas[installs] && replacement &&
            std::memcmp(target, signature, length) == 0, "shield hook signature and site");
    ++installs;
    if (fail_second && installs == 2) return false;
    hook->target = target;
    hook->gateway = installs == 3 ? reinterpret_cast<void*>(&native_publish) :
                    reinterpret_cast<void*>(&native_simulate_bridge);
    hook->length = length;
    return true;
}
bool A2FO_CALL patch_jump(void*, void*, const std::uint8_t*, std::size_t) { return true; }
void seed(bool bad_signature) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x1e0000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(image && fleet, "shield synthetic images");
    std::memcpy(image + kCreateShieldHitRva, kExpectedCreateShieldHit.data(),
                kExpectedCreateShieldHit.size());
    std::memcpy(image + kStopShieldEffectRva, kExpectedStopShieldEffect.data(),
                kExpectedStopShieldEffect.size());
    std::memcpy(image + kStarbaseSimulateRva, kExpectedStarbaseSimulate.data(),
                kExpectedStarbaseSimulate.size());
    std::memcpy(image + kRenderGameObjectsRva, kExpectedRenderGameObjects.data(),
                kExpectedRenderGameObjects.size());
    std::memcpy(image + kGameObjectPublishRva, kExpectedGameObjectPublish.data(),
                kExpectedGameObjectPublish.size());
    if (bad_signature) image[kStarbaseSimulateRva] ^= 1;
}
void choose(bool enabled) {
    ModulePolicy policy;
    policy.managed = true;
    ModulePolicyEntry entry;
    entry.name = "A2FOAlwaysShowShields";
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    policy.entries.push_back(entry);
    std::string error;
    require(apply_builtin_module_policy({roots}, policy, error),
            error.empty() ? "shield policy" : error.c_str());
    require(builtin_module_enabled(BuiltinFeature::always_show_shields) == enabled,
            "shield gate");
}
void run(const std::string& mode) {
    fail_second = mode == "partial";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = log_message; api.armada_module = armada_module;
    api.fleetops_module = fleet_module; api.extension_root_count = root_count;
    api.extension_root = root_at; api.install_inline_hook = install_hook;
    api.patch_jump = patch_jump;
    set_builtin_module_roots({roots});
    const bool ready = mode == "success";
    require(prepare_builtin_module(BuiltinFeature::always_show_shields, api, false) == ready,
            "shield preparation");
    finish_builtin_module_preparation(false, false, false, false, false);
    require(builtin_module_available(BuiltinFeature::always_show_shields) == ready,
            "shield availability");
    require(installs == (mode == "signature" ? 0u : mode == "partial" ? 2u : 3u),
            "shield observer count");
    if (!ready) {
        ModulePolicy request;
        ModulePolicyEntry entry;
        entry.name = "A2FOAlwaysShowShields";
        entry.state = ModulePolicyState::active;
        request.entries.push_back(entry);
        std::string error;
        require(!apply_builtin_module_policy({roots}, request, error) && !error.empty(),
                "unavailable shield activation rejected");
        if (installs) {
            expected_self = image + 0x100;
            const float elapsed = 1.25f;
            std::memcpy(&expected_argument, &elapsed, sizeof(elapsed));
            starbase_simulate_hook(expected_self, nullptr, elapsed);
            require(native_simulates == 1 && native_arguments_ok,
                    "partial shield hook passes through with float bits");
        }
        return;
    }
    std::array<std::uint8_t, 0x220> craft{};
    std::array<std::uint8_t, 0x40> klass{};
    void* class_pointer = klass.data();
    const std::int32_t native_impact_effect = 77;
    std::memcpy(craft.data() + kObjectClassOffset, &class_pointer, sizeof(class_pointer));
    std::memcpy(craft.data() + 0x208, &native_impact_effect, sizeof(native_impact_effect));
    g_enabled_classes.insert(klass.data());
    expected_self = image + 0x100;
    const float elapsed = 0.5f;
    std::memcpy(&expected_argument, &elapsed, sizeof(elapsed));
    choose(true);
    starbase_simulate_hook(expected_self, nullptr, elapsed);
    require(native_simulates == 1 && native_arguments_ok,
            "shield enabled observer forwards native float bits");
    // The checked signature was consumed during preparation. Route only the
    // stop entry to a test callback; the real Armada implementation is absent.
    callback_jump(kStopShieldEffectRva, reinterpret_cast<void*>(&native_stop));
    g_effect_ids[craft.data()] = 12;
    choose(false);
    starbase_simulate_hook(expected_self, nullptr, elapsed);
    require(native_simulates == 2 && native_arguments_ok && native_stops == 1 &&
            stopped_id == 12 && g_effect_ids.empty() &&
            g_enabled_classes.count(klass.data()),
            "game observer stops exactly its tracked effect on disabled generation");
    std::int32_t impact_after = 0;
    std::memcpy(&impact_after, craft.data() + 0x208, sizeof(impact_after));
    require(impact_after == native_impact_effect, "native impact effect untouched");
    choose(true);
    require(runtime_enabled(), "shield reactivation observed before publication");
    g_observed_crafts.insert(craft.data());
    expected_self = image + 0x120;
    expected_argument = reinterpret_cast<std::uintptr_t>(craft.data());
    require(game_object_publish_hook(expected_self, craft.data()) == 0x12345678u &&
            native_publishes == 1 && native_arguments_ok && native_stops == 1 &&
            g_enabled_classes.count(klass.data()),
            "publish native-first result and class retention after activation");
}
}
int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    if (argc != 2) return 2;
    try { run(argv[1]); std::printf("builtin always shields %s: PASS\n", argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "builtin always shields %s: FAIL: %s\n", argv[1], error.what());
        return 1;
    }
}
