// Compile with the frozen or current complete module source under MODULE_SOURCE.
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include MODULE_SOURCE

#ifdef TEST_BUILTIN
namespace a2fo {
std::uint64_t activation = 1;
bool builtin_module_enabled(BuiltinFeature) noexcept { return activation != 0; }
std::uint64_t builtin_module_activation(BuiltinFeature) noexcept { return activation; }
}
#endif

namespace {
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
std::vector<std::string> trace;
std::array<std::string, 2> roots;
unsigned installed = 0;
unsigned fail_install = 0;
bool throw_root_count = false;
unsigned stopped = 0;
bool reject_create = false;
bool clear_ready_on_simulate = false;
bool clear_ready_on_publish = false;
bool mutate_link_on_create = false;
bool deactivate_on_create = false;
void** link_to_mutate = nullptr;
void* list_sentinel = nullptr;
std::string class_name = "child.odf";

void require(bool value, const char* why) {
    if (!value) throw std::runtime_error(why);
}
void A2FO_CALL logger(const char*, const char* message) {
    trace.emplace_back(std::string("log:") + message);
}
void* A2FO_CALL armada() { trace.emplace_back("armada"); return image; }
void* A2FO_CALL fleetops() { trace.emplace_back("fleetops"); return fleet; }
std::uint32_t A2FO_CALL root_count() {
    trace.emplace_back("root-count");
    if (throw_root_count) throw std::bad_alloc();
    return 2;
}
const char* A2FO_CALL root_at(std::uint32_t index) {
    trace.emplace_back("root:" + std::to_string(index));
    return roots.at(index).c_str();
}
extern "C" const char* __attribute__((fastcall)) native_class_name(void*, void*) {
    trace.emplace_back("class-name");
    return class_name == "<null>" ? nullptr : class_name.c_str();
}
extern "C" void __cdecl native_stop(std::int32_t id) {
    ++stopped; trace.emplace_back("stop:" + std::to_string(id));
}
extern "C" std::int32_t __cdecl native_create(
    void*, const void*, std::int32_t, float, std::int32_t) {
    trace.emplace_back("create");
    if (mutate_link_on_create && link_to_mutate) {
        mutate_link_on_create = false;
        *link_to_mutate = list_sentinel;
        trace.emplace_back("list-mutated-in-create");
    }
    if (deactivate_on_create) {
        deactivate_on_create = false;
#ifdef TEST_BUILTIN
        a2fo::activation = 0;
#else
        g_runtime_ready = false;
#endif
        trace.emplace_back("activation-mutated-in-create");
    }
    return reject_create ? -1 : 123;
}
extern "C" void __cdecl native_simulate(void*, std::uintptr_t) {
    trace.emplace_back("gateway-native");
    if (clear_ready_on_simulate) g_runtime_ready = false;
}
extern "C" __attribute__((naked)) void native_simulate_bridge() {
    __asm__ volatile("pushl 4(%esp)\npushl %ecx\ncall _native_simulate\n"
                     "addl $8,%esp\nret $4");
}
std::uintptr_t __cdecl native_publish(void*, void*) {
    trace.emplace_back("publish-native");
    if (clear_ready_on_publish) g_runtime_ready = false;
    return 0x1234;
}
void callback_jump(std::uintptr_t rva, void* callback) {
    const std::uint8_t jump[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    std::memcpy(image + rva, jump, sizeof(jump));
    std::memcpy(image + rva + 1, &callback, sizeof(callback));
}
bool A2FO_CALL install_hook(void* target, void*, std::size_t length,
                           const std::uint8_t* expected, A2FO_InlineHook* hook) {
    const std::uintptr_t sites[]{kStarbaseSimulateRva, kRenderGameObjectsRva,
                                 kGameObjectPublishRva};
    trace.emplace_back("install:" + std::to_string(++installed));
    require(installed <= 3 && target == image + sites[installed - 1] &&
            std::memcmp(target, expected, length) == 0, "hook site/signature");
    if (installed == fail_install) return false;
    hook->target = target;
    hook->gateway = installed == 3 ? reinterpret_cast<void*>(&native_publish)
                                    : reinterpret_cast<void*>(&native_simulate_bridge);
    hook->length = length;
    return true;
}
bool A2FO_CALL patch_jump(void*, void*, const std::uint8_t*, std::size_t) {
    trace.emplace_back("patch"); return true;
}
void seed(bool bad_signature) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x410000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x1e0000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(image && fleet, "synthetic images");
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
void file(const std::string& path, const char* contents) {
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output), "open ODF");
    output << contents;
}
void make_roots() {
    char directory[MAX_PATH]{};
    const DWORD supplied = GetEnvironmentVariableA(
        "A2FO_SHIELDS_ROOT", directory, MAX_PATH);
    const bool host_case_files = supplied != 0 && supplied < MAX_PATH;
    if (!host_case_files) {
        require(GetTempPathA(MAX_PATH, directory) != 0, "temp path");
    }
    const std::string base = host_case_files ? std::string(directory) :
        std::string(directory) + "a2fo-stage8-shields-" +
            std::to_string(GetCurrentProcessId());
    CreateDirectoryA(base.c_str(), nullptr);
    for (std::size_t i = 0; i < roots.size(); ++i) {
        roots[i] = base + "\\root" + std::to_string(i);
        CreateDirectoryA(roots[i].c_str(), nullptr);
        CreateDirectoryA((roots[i] + "\\odf").c_str(), nullptr);
    }
    file(roots[0] + "\\odf\\parent.odf", "alwaysShowShields = no\n");
    file(roots[0] + "\\odf\\child.odf", "#include \"parent.odf\"\n");
    file(roots[1] + "\\odf\\parent.odf", "alwaysShowShields = yes\n");
    file(roots[1] + "\\odf\\unused.odf", "alwaysShowShields = 0\n");
    // The runner creates both case-distinct names on the host filesystem.
    // Win32 CreateFile may alias them on a case-insensitive Wine drive.
    if (!host_case_files) {
        file(roots[1] + "\\odf\\CASE.odf", "alwaysShowShields = yes\n");
        file(roots[1] + "\\odf\\case.odf", "alwaysShowShields = no\n");
    }
    bool upper = false, lower = false;
    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA((roots[1] + "\\odf\\*").c_str(), &data);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            upper |= std::strcmp(data.cFileName, "CASE.odf") == 0;
            lower |= std::strcmp(data.cFileName, "case.odf") == 0;
        } while (FindNextFileA(search, &data));
        FindClose(search);
    }
    trace.emplace_back(std::string("case-entries:") + (upper ? "1" : "0") +
                       ":" + (lower ? "1" : "0"));
    file(roots[1] + "\\odf\\cyclea.odf", "#include \"cycleb.odf\"\n");
    file(roots[1] + "\\odf\\cycleb.odf", "#include \"cyclea.odf\"\n");
}
void run(const std::string& mode) {
    fail_install = mode == "first" ? 1 : mode == "second" ? 2 :
                   mode == "third" ? 3 : 0;
    throw_root_count = mode == "root-throw";
    seed(mode == "signature");
    make_roots();
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger;
    api.armada_module = armada;
    api.fleetops_module = fleetops;
    api.install_inline_hook = install_hook;
    api.patch_jump = patch_jump;
    api.extension_root_count = root_count;
    api.extension_root = root_at;
    trace.emplace_back(std::string("null-init:") + (A2FO_ModuleInit(nullptr) ? "1" : "0"));
    trace.emplace_back(std::string("init:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    trace.emplace_back("ready:" + std::to_string(g_runtime_ready));
    trace.emplace_back("installs:" + std::to_string(installed));
    if (g_runtime_ready && !throw_root_count) {
        trace.emplace_back("indexed:" + std::to_string(g_loose_odf_paths.size()));
        const auto tied = g_loose_odf_paths.find("case.odf");
        trace.emplace_back("case-selected:" +
            (tied == g_loose_odf_paths.end() ? std::string("missing") :
             tied->second.substr(tied->second.find_last_of("\\/") + 1)));
        callback_jump(kGameObjectClassGetOdfNameRva,
                      reinterpret_cast<void*>(&native_class_name));
        callback_jump(kCreateShieldHitRva, reinterpret_cast<void*>(&native_create));
        callback_jump(kStopShieldEffectRva, reinterpret_cast<void*>(&native_stop));
        std::array<std::uint8_t, 0x220> craft{};
        std::array<std::uint8_t, 0x40> klass{};
        void* object_class = klass.data();
        std::memcpy(craft.data() + kObjectClassOffset, &object_class,
                    sizeof(object_class));
        float shield = 10.0f;
        std::memcpy(craft.data() + kCurrentShieldsOffset, &shield, sizeof(shield));
        trace.emplace_back("policy:" + std::to_string(class_policy_enabled(object_class)));
        trace.emplace_back("policy-repeat:" + std::to_string(class_policy_enabled(object_class)));
        trace.emplace_back("null-policy:" + std::to_string(class_policy_enabled(nullptr)));
        std::array<std::uint8_t, 0x40> cycle_class{};
        class_name = "cyclea.odf";
        trace.emplace_back("cycle-policy:" +
            std::to_string(class_policy_enabled(cycle_class.data())));
        std::array<std::uint8_t, 0x40> nameless_class{};
        class_name = "<null>";
        trace.emplace_back("nameless-policy:" +
            std::to_string(class_policy_enabled(nameless_class.data())));
        class_name = "child.odf";
        A2FOAlwaysShowShields_CleanupCraft(nullptr);
        update_craft(craft.data());
        trace.emplace_back("effects:" + std::to_string(g_effect_ids.size()));
        A2FOAlwaysShowShields_CleanupCraft(craft.data());
        trace.emplace_back("cleaned:" + std::to_string(g_effect_ids.size()));
        reject_create = true;
        update_craft(craft.data());
        update_craft(craft.data());
        trace.emplace_back("creation-failure-logs:" +
            std::to_string(g_logged_creation_failures.size()));
        g_effect_ids[craft.data()] = 77;
#ifdef TEST_BUILTIN
        a2fo::activation = 0;
        trace.emplace_back("disabled:" + std::to_string(runtime_enabled()));
        update_craft(reinterpret_cast<void*>(1));
        trace.emplace_back("disabled-poison-safe");
        trace.emplace_back("disabled-effects:" + std::to_string(g_effect_ids.size()));
        a2fo::activation = 1;
        trace.emplace_back("enabled:" + std::to_string(runtime_enabled()));
#endif
        trace.emplace_back("stopped:" + std::to_string(stopped));
        if (mode == "gateway") {
            clear_ready_on_simulate = true;
            starbase_simulate_hook(reinterpret_cast<void*>(1), nullptr, 0.25f);
            trace.emplace_back("simulate-cleared-ready:" +
                std::to_string(!g_runtime_ready));
            clear_ready_on_simulate = false;
            g_runtime_ready = true;
            clear_ready_on_publish = true;
            const auto result = game_object_publish_hook(
                reinterpret_cast<void*>(2), reinterpret_cast<void*>(1));
            trace.emplace_back("publish-cleared-ready:" +
                std::to_string(!g_runtime_ready) + ":" + std::to_string(result));
            clear_ready_on_publish = false;
            g_runtime_ready = true;
        }
        if (mode == "render-list" || mode == "render-activation") {
            std::array<std::uint8_t, 0x220> second_craft{};
            std::memcpy(second_craft.data() + kObjectClassOffset,
                        &object_class, sizeof(object_class));
            std::memcpy(second_craft.data() + kCurrentShieldsOffset,
                        &shield, sizeof(shield));
            std::array<void*, 3> head{}, first{}, second{};
            head[0] = first.data();
            first[0] = second.data();
            first[2] = craft.data();
            second[0] = head.data();
            second[2] = second_craft.data();
            std::array<void*, 2> owner{nullptr, head.data()};
            void* owner_pointer = owner.data();
            std::memcpy(image + kGameObjectListRva,
                        &owner_pointer, sizeof(owner_pointer));
            link_to_mutate = &first[0];
            list_sentinel = head.data();
            mutate_link_on_create = mode == "render-list";
            deactivate_on_create = mode == "render-activation";
            g_cached_list_owner = nullptr;
            g_cached_list_sentinel = nullptr;
            g_global_scan_completed = false;
            g_logged_object_list_scan = false;
            g_effect_ids.clear();
            g_observed_crafts.clear();
            g_logged_creation_failures.clear();
            reject_create = false;
            render_game_objects_hook(reinterpret_cast<void*>(3), nullptr, 17);
            trace.emplace_back("render-effects:" +
                std::to_string(g_effect_ids.size()) + ":observed:" +
                std::to_string(g_observed_crafts.size()) + ":stopped:" +
                std::to_string(stopped));
            link_to_mutate = nullptr;
            list_sentinel = nullptr;
        }
    }
    A2FO_ModuleShutdown();
    trace.emplace_back("shutdown-ready:" + std::to_string(g_runtime_ready));
    for (const auto& item : trace) std::puts(item.c_str());
}
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    if (argc != 2) return 2;
    try { run(argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "fixture: %s\n", error.what());
        return 1;
    }
}
