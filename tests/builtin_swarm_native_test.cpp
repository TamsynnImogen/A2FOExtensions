// Synthetic BeeColony fixture: callbacks call native gateways, never the game.
#include "../core/builtin_swarm_system.cpp"
#include "../core/module_loader.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using namespace a2fo;
using Simulate = void (__attribute__((fastcall)) *)(void*, void*, float);
using Render = void (__attribute__((fastcall)) *)(void*, void*, void*, std::uint32_t);
std::uint8_t* image;
std::string root;
Simulate simulate_hook;
Render render_hook;
unsigned installs, native_simulations, native_renders;
bool fail_render, arguments_ok = true;
void* expected_self;
void* expected_camera;
std::uint32_t expected_flags;
float expected_elapsed;

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void A2FO_CALL logger(const char*, const char*) {}
void* A2FO_CALL armada() { return image; }
extern "C" void __cdecl native_simulate(void* self, std::uintptr_t elapsed_bits) {
    ++native_simulations;
    float elapsed = 0;
    std::memcpy(&elapsed, &elapsed_bits, sizeof(elapsed));
    arguments_ok &= self == expected_self && elapsed == expected_elapsed;
}
extern "C" void __cdecl native_render(void* self, std::uintptr_t camera,
                                        std::uintptr_t flags) {
    ++native_renders;
    arguments_ok &= self == expected_self &&
        camera == reinterpret_cast<std::uintptr_t>(expected_camera) &&
        flags == expected_flags;
}
extern "C" __attribute__((naked)) void native_simulate_bridge() {
    __asm__ volatile("pushl 4(%esp)\npushl %ecx\ncall _native_simulate\n"
                     "addl $8,%esp\nret $4");
}
extern "C" __attribute__((naked)) void native_render_bridge() {
    __asm__ volatile("pushl 8(%esp)\npushl 8(%esp)\npushl %ecx\n"
                     "call _native_render\naddl $12,%esp\nret $8");
}
bool A2FO_CALL install(void* target, void* replacement, std::size_t size,
                       const std::uint8_t* expected, A2FO_InlineHook* hook) {
    check(std::memcmp(target, expected, size) == 0, "swarm hook signature");
    ++installs;
    if (installs == 1) {
        check(target == image + kBeeColonySimulateRva, "simulate hook site");
        simulate_hook = reinterpret_cast<Simulate>(replacement);
    } else {
        check(installs == 2 && target == image + kBeeColonyRenderRva,
              "render hook site");
        render_hook = reinterpret_cast<Render>(replacement);
    }
    if (fail_render && installs == 2) return false;
    *hook = {target, installs == 1 ? reinterpret_cast<void*>(&native_simulate_bridge) :
             reinterpret_cast<void*>(&native_render_bridge),
             static_cast<std::uint32_t>(size)};
    return true;
}
template<std::size_t N> void bytes(std::uintptr_t rva, const std::uint8_t (&signature)[N]) {
    std::memcpy(image + rva, signature, N);
}
void seed(bool signature_failure) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(image, "swarm synthetic image");
    bytes(kBeeColonySimulateRva, kExpectedBeeColonySimulate);
    bytes(kBeeColonyRenderRva, kExpectedBeeColonyRender);
    bytes(kBeeColonyShouldDrawBeeRva, kExpectedBeeColonyShouldDrawBee);
    bytes(kParameterDbConstructorRva, kExpectedParameterDbConstructor);
    bytes(kParameterDbDestructorRva, kExpectedParameterDbDestructor);
    for (auto rva : {kParameterDbGetIntRva, kParameterDbGetFloatRva,
                     kParameterDbGetBoolRva}) bytes(rva, kExpectedParameterDbGetScalar);
    bytes(kParameterDbGetStringRva, kExpectedParameterDbGetString);
    bytes(kParameterDbGetStringVectorRva, kExpectedParameterDbGetStringVector);
    bytes(kSt3dInstanceConstructorRva, kExpectedSt3dInstanceConstructor);
    bytes(kSt3dInstanceDestructorRva, kExpectedSt3dInstanceDestructor);
    bytes(kSt3dInstanceSetAnimationFlagsRva, kExpectedSt3dInstanceSetAnimationFlags);
    bytes(kSt3dInstanceScaleGeometryRva, kExpectedSt3dInstanceScaleGeometry);
    bytes(kSt3dInstanceSetTransformRva, kExpectedSt3dInstanceSetTransform);
    bytes(kSt3dInstanceSetDatabaseRva, kExpectedSt3dInstanceSetDatabase);
    bytes(kSt3dInstanceRenderRva, kExpectedSt3dInstanceRender);
    bytes(kSt3dInstanceTriggerAnimationRva, kExpectedSt3dInstanceTriggerAnimation);
    bytes(kEntityGetTransformRva, kExpectedEntityGetTransform);
    bytes(kEntityGetBoundingSphereRva, kExpectedEntityGetBoundingSphere);
    bytes(kEntityGetWorldTransformRva, kExpectedEntityGetWorldTransform);
    bytes(kGameObjectClassGetHierarchyRootRva, kExpectedClassGetHierarchyRoot);
    bytes(kGameObjectClassGetOdfNameRva, kExpectedClassGetOdfName);
    bytes(kSt3dNodeFindRecursiveRva, kExpectedNodeFindRecursive);
    bytes(kEngineOperatorDeleteRva, kExpectedEngineOperatorDelete);
    if (signature_failure) image[kBeeColonyRenderRva] ^= 1;
}
void policy(bool enabled) {
    ModulePolicy selection;
    selection.managed = true;
    ModulePolicyEntry entry;
    entry.name = "A2FOSwarmSystem";
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    selection.entries.push_back(entry);
    std::string error;
    check(apply_builtin_module_policy({root}, selection, error),
          error.empty() ? "swarm policy" : error.c_str());
}
void call_simulate(void* self) {
    expected_self = self; expected_elapsed = 0.125f;
    const auto previous = native_simulations;
    simulate_hook(self, nullptr, expected_elapsed);
    check(native_simulations == previous + 1 && arguments_ok,
          "swarm simulation preserves native first-call arguments");
}
void call_render(void* self) {
    expected_self = self; expected_camera = image + 0x100;
    expected_flags = 0x50607080;
    const auto previous = native_renders;
    render_hook(self, nullptr, expected_camera, expected_flags);
    check(native_renders == previous + 1 && arguments_ok,
          "swarm rendering preserves native call arguments");
}
void run(const std::string& mode) {
    char temporary[MAX_PATH]{}, folder[MAX_PATH]{};
    check(GetTempPathA(sizeof(temporary), temporary), "temporary path");
    check(GetTempFileNameA(temporary, "bsw", 0, folder), "temporary filename");
    check(DeleteFileA(folder) && CreateDirectoryA(folder, nullptr), "private root");
    root = folder;
    fail_render = mode == "partial";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger; api.armada_module = armada; api.install_inline_hook = install;
    set_builtin_module_roots({root});
    const bool ready = mode == "success";
    check(prepare_builtin_module(BuiltinFeature::swarm_system, api, false) == ready,
          "swarm preparation");
    finish_builtin_module_preparation(false, false, false, false, false);
    check(builtin_module_available(BuiltinFeature::swarm_system) == ready,
          "swarm complete hook availability");
    check(installs == (mode == "signature" ? 0u : 2u), "swarm install count");
    if (!ready) {
        ModulePolicy selection;
        ModulePolicyEntry entry;
        entry.name = "A2FOSwarmSystem";
        entry.state = ModulePolicyState::active;
        selection.entries.push_back(entry);
        std::string error;
        check(!apply_builtin_module_policy({root}, selection, error) && !error.empty(),
              "unavailable swarm activation refused");
        if (simulate_hook) call_simulate(image + kWorkerBeeColonyRva);
        return;
    }
    check(prepare_builtin_module(BuiltinFeature::swarm_system, api, false) &&
          installs == 2, "swarm preparation idempotent");
    void* worker = image + kWorkerBeeColonyRva;
    call_simulate(worker);
    call_render(worker);
    check(g_epoch == 0 && g_swarm_activation.load() == 0,
          "initially disabled swarm leaves runtime idle");
    policy(true);
    check(!runtime_render_enabled(), "render waits for simulation synchronization");
    call_render(worker);
    check(g_epoch == 0, "pre-simulation render stays native only");
    call_simulate(image + 0x100);
    check(runtime_render_enabled() && g_epoch == 0,
          "nonworker simulation synchronizes activation without swarm tick");
    policy(false);
    g_class_policies[image + 0x200] = ClassPolicy{};
    g_hosts.emplace(77, HostRuntime{}); // No constructed ST3D_Instance.
    g_host_discovery_countdown = 7;
    g_cached_object_list = image + 0x300;
    g_cached_object_list_head = image + 0x400;
    check(!runtime_render_enabled(), "disabled render cannot see stale activation");
    call_render(worker);
    check(g_hosts.size() == 1, "render does not destroy native hosts");
    call_simulate(image + 0x100);
    check(g_hosts.empty() && g_class_policies.empty() &&
          g_host_discovery_countdown == 0 && !g_cached_object_list &&
          !g_cached_object_list_head && g_swarm_activation.load() == 0,
          "simulation boundary clears host and discovery caches");
    policy(true);
    check(!runtime_render_enabled(), "new activation waits for simulation");
    call_simulate(image + 0x100);
    check(runtime_render_enabled() && installs == 2,
          "fresh activation synchronizes without repatching");
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "usage: builtin_swarm_native_test.exe success|signature|partial");
        run(argv[1]);
        std::puts("builtin swarm native fixture: PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
