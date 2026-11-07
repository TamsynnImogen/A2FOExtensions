// Inventory, policy and loaded Producer-provider checks; no game or input.
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace a2fo;
const char* added[]{"A2FOFireArcs", "A2FOAnimations", "A2FOAlwaysShowShields", "A2FOStationRotation", "A2FORadarDiagnostics", "A2FORecoveryTests"};
unsigned begins = 0, finishes = 0;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void begin(const std::string&) { ++begins; }
void finish(const std::string&, bool) { ++finishes; }
void logger(const std::string&) {}
void write(const std::string& path, const std::string& contents) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    check(file != INVALID_HANDLE_VALUE, "fixture file");
    DWORD count{};
    check(WriteFile(file, contents.data(), contents.size(), &count, nullptr) &&
          count == contents.size(), "write fixture file"); CloseHandle(file);
}
void run(const std::string& mode) {
    if (mode == "provider") {
        check(!producer_dispatch_available({}), "no provider is unavailable");
        HMODULE module = LoadLibraryA("build/fixtures/builtin-producer/A2FOFeaturePack.dll");
        check(module != nullptr, "load console provider fixture");
        FARPROC address = GetProcAddress(module, "FixtureSetReady");
        void (__cdecl *set)(bool) = nullptr;
        static_assert(sizeof(set) == sizeof(address)); std::memcpy(&set, &address, sizeof(set));
        check(set != nullptr, "fixture setter");
        const LoadedModule provider{module, nullptr, "C:/Fixture/modules/A2FOFeaturePack.dll"};
        check(!producer_dispatch_available({provider}), "failed queue hooks unavailable");
        set(true);
        check(producer_dispatch_available({provider}), "confirmed Producer provider ready");
        check(!producer_dispatch_available({{module,nullptr,"C:/Fixture/Other.dll"}}),
              "other owner cannot supply Producer capability");
        set(false);
        check(!producer_dispatch_available({provider}), "provider negative response honored");
        HMODULE older = LoadLibraryExA("build/modules/A2FOInstantActionSettings.dll", nullptr,
                                       DONT_RESOLVE_DLL_REFERENCES);
        check(older != nullptr, "data map library without query export");
        check(!producer_dispatch_available({{older,nullptr,"A2FOFeaturePack.dll"}}),
              "old provider without capability export unavailable");
        FreeLibrary(older); FreeLibrary(module); return;
    }
    char temp[MAX_PATH]{}, directory[MAX_PATH]{};
    check(GetTempPathA(sizeof(temp),temp) && GetTempFileNameA(temp,"b7t",0,directory),
          "fixture directory name");
    check(DeleteFileA(directory) && CreateDirectoryA(directory,nullptr), "fixture root");
    const std::string root = directory;
    check(CreateDirectoryA((root+"\\modules").c_str(),nullptr), "modules directory");
    for (const auto* name : added) write(root+"\\modules\\"+name+".dll", "stale");
    std::vector<std::string> diagnostics;
    const auto inventory = discover_installed_modules(root, &diagnostics);
    check(inventory.size() == static_cast<std::size_t>(BuiltinFeature::count) && diagnostics.size() == 6, "all builtins with six stale DLLs deduplicated");
    for (const auto* name : added) {
        BuiltinFeature feature;
        check(find_builtin_feature(std::string(name)+".dll",feature), "new canonical identity");
    }
    ModuleRules parent, child;
    parent.has_section = child.has_section = true;
    const auto managed = evaluate_module_rules({child}, inventory);
    bool compatibility_active = false;
    for (const auto& entry : managed.entries)
        if (entry.name == "A2FOM3D") compatibility_active = entry.state == ModulePolicyState::active;
    check(compatibility_active, "existing managed-policy M3D compatibility default retained");
    child.rejected.push_back("A2FOM3D");
    const auto disabled = evaluate_module_rules({child}, inventory);
    bool compatibility_rejected = false;
    for (const auto& entry : disabled.entries)
        if (entry.name == "A2FOM3D") compatibility_rejected = entry.state == ModulePolicyState::rejected;
    check(compatibility_rejected, "explicit M3D rejection overrides compatibility default");
    child.rejected.clear();
    for (const auto* name : added) parent.required.push_back(name);
    const auto required = evaluate_module_rules({parent,child}, inventory);
    check(required.valid, "inherited required policy valid");
    unsigned locked = 0;
    for (const auto& entry : required.entries) if (entry.state == ModulePolicyState::required) ++locked;
    check(locked == 6, "six inherited requirements retained");
    child.rejected.push_back("A2FOFireArcs");
    check(!evaluate_module_rules({parent,child},inventory).valid, "required/rejected conflict retained");
    if (mode == "rejected") {
        std::string config = "[modules]\n";
        for (int i=0; i<static_cast<int>(BuiltinFeature::count); ++i)
            config += "reject"+std::to_string(i)+"="+canonical_builtin_name(static_cast<BuiltinFeature>(i))+"\n";
        write(root+"\\info.ini",config);
        A2FO_ModuleApi api{}; api.struct_size=sizeof(api); api.api_version=A2FO_MODULE_API_VERSION;
        LoadedModules loaded;
        check(load_native_modules_from_roots({root},api,loaded,logger,{begin,finish}), "all rejected loader");
        check(begins==0 && finishes==0 && loaded.empty(), "rejected providers never prepared or loaded");
        finish_builtin_module_preparation(false,false,false,false,false,false,false,false);
        ModulePolicy requested; ModulePolicyEntry entry;
        entry.name="A2FOFireArcs"; entry.state=ModulePolicyState::active; requested.entries.push_back(entry);
        std::string error;
        check(!apply_builtin_module_policy({root},requested,error) && !error.empty(), "unavailable selection rejected");
        check(apply_builtin_module_policy({root+"\\Other"},requested,error), "other mod selection saved only");
        check(DeleteFileA((root+"\\info.ini").c_str()), "remove policy");
    }
    for (const auto* name : added) check(DeleteFileA((root+"\\modules\\"+name+".dll").c_str()), "remove stale fixture");
    check(RemoveDirectoryA((root+"\\modules").c_str()) && RemoveDirectoryA(root.c_str()), "remove fixture root");
}
}
int main(int argc, char** argv) {
    if (argc!=2) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try { run(argv[1]); std::printf("builtin batch8 %s: PASS\n",argv[1]); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
}
