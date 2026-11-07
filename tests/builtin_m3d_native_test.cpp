// Synthetic native resolver fixture. The game image is data, never executed.
#include "../core/builtin_m3d.cpp"
#include "../core/module_loader.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using namespace a2fo;
using Resolver = void* (__attribute__((fastcall)) *)(void*, void*, const char*, float);
std::uint8_t* armada_image;
std::uint8_t* fleet_image;
std::string root;
Resolver resolver, no_fallback_resolver;
unsigned installs, native_calls;
bool fail_second, arguments_ok = true;
void* expected_self;
const char* expected_name;
float expected_scale;
void* native_result = reinterpret_cast<void*>(0x12345678);

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void A2FO_CALL logger(const char*, const char*) {}
void* A2FO_CALL armada() { return armada_image; }
void* A2FO_CALL fleetops() { return fleet_image; }
std::uint32_t A2FO_CALL roots() { return 1; }
const char* A2FO_CALL root_at(std::uint32_t index) { return index==0 ? root.c_str() : nullptr; }
const char* files[]{"legacy.m3d","legacy.sod","legacy.sod.a2fo-m3d","authored.sod","authored.m3d"};
void file_fixture(bool create) {
    for (const auto* name : files) {
        const auto path=root+"\\sod\\"+name;
        HANDLE file=CreateFileA(path.c_str(),create?GENERIC_WRITE:GENERIC_READ,0,nullptr,
                                create?CREATE_NEW:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(file!=INVALID_HANDLE_VALUE,"M3D source/cache fixture preserved");
        DWORD count{}; char bytes[4]{};
        if (create) check(WriteFile(file,"keep",4,&count,nullptr)&&count==4,"write private source/cache");
        else check(ReadFile(file,bytes,4,&count,nullptr)&&count==4&&!std::memcmp(bytes,"keep",4),
                   "M3D preparation preserves cache and authored file bytes");
        CloseHandle(file);
    }
}
void cleanup() {
    if(root.empty()) return;
    for (const auto* name:files) DeleteFileA((root+"\\sod\\"+name).c_str());
    RemoveDirectoryA((root+"\\sod").c_str()); RemoveDirectoryA(root.c_str());
}
extern "C" void* __cdecl native_loader(void* self, const char* name, float scale) {
    ++native_calls;
    arguments_ok &= self == expected_self && name == expected_name && scale == expected_scale;
    return native_result;
}
extern "C" __attribute__((naked)) void native_loader_bridge() {
    __asm__ volatile("pushl 8(%esp)\npushl 8(%esp)\npushl %ecx\n"
                     "call _native_loader\naddl $12,%esp\nret $8");
}
bool A2FO_CALL install(void* target, void* replacement, std::size_t size,
                       const std::uint8_t* signature, A2FO_InlineHook* hook) {
    check(std::memcmp(target, signature, size) == 0, "M3D installed signature");
    ++installs;
    check(target == fleet_image + (installs == 1 ? kFoLoadSodHandlerRva :
                  kFoLoadSodNoFallbackHandlerRva), "M3D resolver target");
    if (installs == 1) resolver = reinterpret_cast<Resolver>(replacement);
    else no_fallback_resolver = reinterpret_cast<Resolver>(replacement);
    if (fail_second && installs == 2) return false;
    *hook = {target, reinterpret_cast<void*>(&native_loader_bridge),
             static_cast<std::uint32_t>(size)};
    return true;
}
void policy(bool enabled) {
    ModulePolicy selection;
    selection.managed = true;
    ModulePolicyEntry entry;
    entry.name = "A2FOM3D";
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    selection.entries.push_back(entry);
    std::string error;
    check(apply_builtin_module_policy({root}, selection, error),
          error.empty() ? "M3D policy" : error.c_str());
    check(builtin_module_enabled(BuiltinFeature::m3d) == enabled, "M3D gate");
}
void seed(bool signature_failure) {
    armada_image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    fleet_image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x1e0000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(armada_image && fleet_image, "M3D synthetic images");
    std::memcpy(armada_image + kMeshUpdateRva, kExpectedMeshUpdate,
                sizeof(kExpectedMeshUpdate));
    std::memcpy(fleet_image + kFoLoadSodHandlerRva, kExpectedFoLoadSodHandler,
                sizeof(kExpectedFoLoadSodHandler));
    std::memcpy(fleet_image + kFoLoadSodNoFallbackHandlerRva,
                kExpectedFoLoadSodNoFallbackHandler,
                sizeof(kExpectedFoLoadSodNoFallbackHandler));
    if (signature_failure) fleet_image[kFoLoadSodNoFallbackHandlerRva] ^= 1;
}
void call(Resolver hook, const char* name, float scale, void* expected) {
    expected_self = armada_image + 0x100;
    expected_name = name;
    expected_scale = scale;
    check(hook(expected_self, nullptr, name, scale) == expected,
          "M3D resolver result");
    check(arguments_ok, "M3D native arguments");
}
void run(const std::string& mode) {
    char temporary[MAX_PATH]{}, folder[MAX_PATH]{};
    check(GetTempPathA(sizeof(temporary), temporary), "temporary path");
    check(GetTempFileNameA(temporary, "bmd", 0, folder), "temporary filename");
    check(DeleteFileA(folder) && CreateDirectoryA(folder, nullptr), "private root");
    root = folder;
    check(CreateDirectoryA((root+"\\sod").c_str(),nullptr),"private source directory");
    file_fixture(true);
    fail_second = mode == "partial";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger; api.armada_module = armada; api.fleetops_module = fleetops;
    api.extension_root_count = roots; api.extension_root = root_at;
    api.install_inline_hook = install;
    set_builtin_module_roots({root});
    const bool ready = mode == "success";
    check(prepare_builtin_module(BuiltinFeature::m3d, api, false) == ready,
          "M3D preparation");
    finish_builtin_module_preparation(false, false, false, false, false);
    check(builtin_module_available(BuiltinFeature::m3d) == ready,
          "M3D complete native availability");
    check(installs == (mode == "signature" ? 0u : 2u), "M3D hook count");
    file_fixture(false);
    if(mode!="signature") check(g_sources.count("legacy") && !g_sources.count("authored"),
        "legacy generated cache ignored without deletion; native authored SOD wins");
    if (!ready) {
        ModulePolicy selection;
        ModulePolicyEntry entry;
        entry.name = "A2FOM3D";
        entry.state = ModulePolicyState::active;
        selection.entries.push_back(entry);
        std::string error;
        check(!apply_builtin_module_policy({root}, selection, error) && !error.empty(),
              "unavailable M3D cannot activate");
        if (resolver) call(resolver, "ship.m3d", 1.25f, native_result);
        cleanup();
        return;
    }
    check(prepare_builtin_module(BuiltinFeature::m3d, api, false) && installs == 2,
          "M3D preparation idempotent");
    const char name[] = "ship.m3d";
    const std::string key = model_key(name);
    const std::string cache = runtime_cache_key(key, 1.25f);
    std::array<std::uint8_t, 0x80> geometry{}, resource{};
    void* resource_pointer = resource.data();
    std::memcpy(geometry.data() + 0x40, &resource_pointer, sizeof(resource_pointer));
    g_sources[key] = {"synthetic.m3d", 0};
    g_loaded[cache] = geometry.data();
    policy(false);
    call(resolver, name, 1.25f, native_result);
    check(g_sources.count(key) && g_loaded.count(cache) &&
          *reinterpret_cast<std::uint32_t*>(resource.data() + 0x10) == 0,
          "M3D disabled resolver retains index and cached geometry");
    policy(true);
    call(resolver, name, 1.25f, geometry.data());
    check(*reinterpret_cast<std::uint32_t*>(resource.data() + 0x10) == 1 &&
          resource[0x14] == 0, "enabled cached M3D finalized");
    call(no_fallback_resolver, name, 1.25f, geometry.data());
    check(*reinterpret_cast<std::uint32_t*>(resource.data() + 0x10) == 2 &&
          resource[0x14] == 1, "no-fallback resource mode preserved");
    policy(false); policy(true); policy(false);
    call(no_fallback_resolver, name, 1.25f, native_result);
    check(g_sources.count(key) && g_loaded.count(cache) && installs == 2,
          "M3D off/on keeps native hooks and parsed asset cache");
    cleanup();
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "usage: builtin_m3d_native_test.exe success|signature|partial");
        run(argv[1]);
        std::puts("builtin M3D native fixture: PASS");
        return 0;
    } catch (const std::exception& error) {
        cleanup();
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
