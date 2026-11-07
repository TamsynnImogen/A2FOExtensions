// Synthetic i386/Wine fixture for optional built-ins. No game process or UI.
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" std::uintptr_t __cdecl native_build_check(std::uintptr_t);
extern "C" std::uintptr_t __cdecl native_owner_check(void*);
extern "C" std::uintptr_t __cdecl native_find_check(void*, std::uintptr_t,
                                                      std::uintptr_t);
// Match the project's documented bridge.S ABI: ECX is this and the callee
// removes respectively three, zero, or two stack arguments.
extern "C" __attribute__((naked)) void native_build_bridge() {
    __asm__ volatile("pushl %ebp\nmovl %esp, %ebp\npushl 16(%ebp)\n"
                     "call _native_build_check\naddl $4, %esp\nleave\nret $12");
}
extern "C" __attribute__((naked)) void native_owner_bridge() {
    __asm__ volatile("pushl %ecx\ncall _native_owner_check\n"
                     "addl $4, %esp\nret");
}
extern "C" __attribute__((naked)) void native_find_bridge() {
    __asm__ volatile("pushl %ebp\nmovl %esp, %ebp\npushl 12(%ebp)\n"
                     "pushl 8(%ebp)\npushl %ecx\ncall _native_find_check\n"
                     "addl $12, %esp\nleave\nret $8");
}

namespace {
using namespace a2fo;
using BuildHook = std::uintptr_t (__attribute__((fastcall)) *)(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t);
std::uint8_t* image = nullptr;
std::string root;
bool owns_root = false;
bool reject_info_registration = false, reject_weapon_registration = false;
bool reject_hook = false;
unsigned begins = 0, ends = 0, info_regs = 0, weapon_regs = 0;
unsigned hook_calls = 0, builds = 0, particles = 0, lookups = 0;
A2FO_InfoIniDefaultsHandler defaults = nullptr;
A2FO_WeaponClassLoadedHandler class_loaded = nullptr;
BuildHook build_hook = nullptr;
void* expected_owner = nullptr;
void* expected_database = nullptr;
void* expected_weapon = nullptr;
bool native_args_ok = true;

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
template<class T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}
void bytes(std::size_t rva, std::initializer_list<std::uint8_t> values) {
    std::memcpy(image + rva, values.begin(), values.size());
}
void jump(std::size_t rva, void* target) {
    bytes(rva, {0xb8, 0, 0, 0, 0, 0xff, 0xe0});
    put(image + rva, 1, reinterpret_cast<std::uintptr_t>(target));
}
void A2FO_CALL logger(const char*, const char*) {}
void log_line(const std::string&) {}
void* A2FO_CALL armada_module() { return image; }
const char* A2FO_CALL root_directory() { return root.c_str(); }
std::uint32_t A2FO_CALL root_count() { return 1; }
const char* A2FO_CALL extension_root(std::uint32_t i) {
    return i == 0 ? root.c_str() : nullptr;
}
bool A2FO_CALL register_info(const char* owner, A2FO_InfoIniDefaultsHandler handler,
                             void*) {
    check(std::strcmp(owner, "A2FOInfoIni") == 0 && handler, "info registration");
    ++info_regs;
    defaults = handler;
    return !reject_info_registration;
}
bool A2FO_CALL register_weapon(const char* owner, const char* const* fields,
                               std::uint32_t count,
                               A2FO_WeaponClassLoadedHandler handler, void*) {
    check(std::strcmp(owner, "A2FOMuzzleFlashes") == 0 && handler && count == 4,
          "weapon-class registration");
    check(std::strcmp(fields[0], "muzzleFlashSprite") == 0 &&
          std::strcmp(fields[3], "muzzleFlashColor") == 0, "weapon fields");
    ++weapon_regs;
    class_loaded = handler;
    return !reject_weapon_registration;
}
void __cdecl native_particle(const char* sprite, const float* colour,
                             void* owner, void* node, float size, float duration) {
    check(std::strcmp(sprite, "spark") == 0 && owner == expected_owner && node &&
          colour[0] == 0.2f && colour[1] == 0.4f && colour[2] == 0.6f &&
          size == 9.0f && duration > 0.24f && duration < 0.26f,
          "particle payload");
    ++particles;
}
bool A2FO_CALL install(void* target, void* replacement, std::size_t size,
                       const std::uint8_t* expected, A2FO_InlineHook* hook) {
    check(target == image + 0x18aba0 && size == 5 &&
          std::memcmp(target, expected, size) == 0, "ordnance hook signature");
    ++hook_calls;
    if (reject_hook) return false;
    build_hook = reinterpret_cast<BuildHook>(replacement);
    *hook = {target, reinterpret_cast<void*>(&native_build_bridge), size};
    return true;
}
void begin(const std::string&) { ++begins; }
void finish(const std::string&, bool) { ++ends; }
void seed(bool bad_signature) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x400000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    check(image, "synthetic Armada image");
    bytes(0x18aba0, {0x55, 0x8b, 0xec, 0x8b, 0x01});
    bytes(0x271050, {0x8b, 0x49, 0x18, 0x51, 0xe8});
    bytes(0x733c0, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x34});
    bytes(0x220750, {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08});
    if (bad_signature) image[0x18aba0] ^= 1;
}
void write_file(const std::string& path, const char* contents) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(file != INVALID_HANDLE_VALUE, "create fixture file");
    DWORD written = 0;
    check(WriteFile(file, contents, static_cast<DWORD>(std::strlen(contents)),
                    &written, nullptr) && written == std::strlen(contents),
          "write fixture file");
    CloseHandle(file);
}
ModulePolicy policy(bool enabled) {
    ModuleRules rules;
    rules.has_section = true;
    rules.rejected.push_back("A2FOM3D");
    if (enabled) rules.active = {"A2FOInfoIni", "A2FOMuzzleFlashes"};
    return evaluate_module_rules({rules}, discover_installed_modules(root));
}
void toggle(bool enabled) {
    std::string error;
    const bool applied = apply_builtin_module_policy({root}, policy(enabled), error);
    check(applied, error.c_str());
    check(builtin_module_enabled(BuiltinFeature::info_ini) == enabled &&
          builtin_module_enabled(BuiltinFeature::muzzle_flashes) == enabled,
          "both live gates follow policy");
}
void cleanup() {
    if (image) VirtualFree(image, 0, MEM_RELEASE);
    if (!owns_root) return;
    DeleteFileA((root + "\\info.ini").c_str());
    DeleteFileA((root + "\\modules\\A2FOInfoIni.dll").c_str());
    DeleteFileA((root + "\\modules\\A2FOMuzzleFlashes.dll").c_str());
    RemoveDirectoryA((root + "\\modules").c_str());
    RemoveDirectoryA(root.c_str());
}
std::string fixture_info(bool legacy, bool rejected) {
    const std::string mod_section = "[mod]\nDefaultGameSpeed=4\nSettingsDirectory=FixtureSettings\n";
    if (legacy) return mod_section;
    std::string contents = rejected ? "[modules]\n" : mod_section + "[modules]\n";
    unsigned reject_number = 0;
    for (unsigned index = 0; index < static_cast<unsigned>(BuiltinFeature::count); ++index) {
        const auto feature = static_cast<BuiltinFeature>(index);
        if (!rejected && (feature == BuiltinFeature::info_ini ||
                          feature == BuiltinFeature::muzzle_flashes)) continue;
        contents += "reject" + std::to_string(++reject_number) + "=" +
                    canonical_builtin_name(feature) + "\n";
    }
    return contents;
}
void run(const char* mode) {
    const bool legacy = std::strcmp(mode, "legacy") == 0;
    const bool rejected = std::strcmp(mode, "rejected") == 0;
    const bool dispatch_fail = std::strcmp(mode, "dispatch-fail") == 0;
    const bool info_dispatch_fail = dispatch_fail || std::strcmp(mode, "info-dispatch-fail") == 0;
    const bool muzzle_dispatch_fail = dispatch_fail || std::strcmp(mode, "muzzle-dispatch-fail") == 0;
    const bool bad_signature = std::strcmp(mode, "signature-fail") == 0;
    reject_hook = std::strcmp(mode, "hook-fail") == 0;
    reject_info_registration = std::strcmp(mode, "info-registration-fail") == 0;
    reject_weapon_registration = std::strcmp(mode, "weapon-registration-fail") == 0;
    check(std::strcmp(mode, "success") == 0 || legacy || rejected || info_dispatch_fail || muzzle_dispatch_fail ||
          bad_signature || reject_hook || reject_info_registration ||
          reject_weapon_registration, "known fixture mode");
    char temp[MAX_PATH]{};
    check(GetTempPathA(sizeof(temp), temp) != 0, "temporary directory");
    root = std::string(temp) + "a2fo-builtin-optional-" +
           std::to_string(GetCurrentProcessId());
    check(CreateDirectoryA(root.c_str(), nullptr) != 0, "private fixture root");
    owns_root = true;
    check(discover_installed_modules(root).size() == static_cast<std::size_t>(BuiltinFeature::count),
          "built-ins without standalone files");
    write_file(root + "\\modules\\A2FOInfoIni.dll", "invalid leftover");
    write_file(root + "\\modules\\A2FOMuzzleFlashes.dll", "invalid leftover");
    std::vector<std::string> diagnostics;
    check(discover_installed_modules(root, &diagnostics).size() == static_cast<std::size_t>(BuiltinFeature::count) &&
          diagnostics.size() == 2, "duplicate DLLs ignored");
    const std::string info = fixture_info(legacy, rejected);
    write_file(root + "\\info.ini", info.c_str());
    seed(bad_signature);
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.api_revision = 14;
    api.capabilities = A2FO_CAP_INFO_INI_DEFAULTS | A2FO_CAP_WEAPON_CLASS_LOADED;
    api.log = &logger;
    api.root_directory = &root_directory;
    api.extension_root_count = &root_count;
    api.extension_root = &extension_root;
    api.armada_module = &armada_module;
    api.install_inline_hook = &install;
    api.register_info_ini_defaults_handler = &register_info;
    api.register_weapon_class_loaded_handler = &register_weapon;
    LoadedModules loaded;
    check(load_native_modules_from_roots(
        {root}, api, loaded, &log_line, {&begin, &finish}), "builtin loader");
    check(std::all_of(loaded.begin(), loaded.end(),
                      [](const LoadedModule& module) { return module.handle == nullptr; }),
          "duplicate DLLs never loaded");
    finish_builtin_module_preparation(true, true, !muzzle_dispatch_fail, !info_dispatch_fail, false);
    if (rejected) {
        check(begins == 0 && ends == 0 && info_regs == 0 && weapon_regs == 0 &&
              hook_calls == 0, "rejected built-ins never prepared");
        check(!builtin_module_available(BuiltinFeature::info_ini) &&
              !builtin_module_available(BuiltinFeature::muzzle_flashes),
              "rejected unavailable");
        return;
    }
    check(begins == (legacy ? static_cast<unsigned>(BuiltinFeature::count) : 2u) && ends == begins, "one preparation transaction per eligible builtin");
    check(info_regs == 1 && weapon_regs == (bad_signature ? 0u : 1u), "callbacks registered after signature validation");
    check(builtin_module_enabled(BuiltinFeature::info_ini) == legacy &&
          builtin_module_enabled(BuiltinFeature::muzzle_flashes) == legacy,
          "managed inactive versus legacy load-all defaults");
    const bool info_available = !info_dispatch_fail && !reject_info_registration;
    const bool muzzle_available = !muzzle_dispatch_fail && !bad_signature &&
                                  !reject_hook && !reject_weapon_registration;
    check(builtin_module_available(BuiltinFeature::info_ini) == info_available &&
          builtin_module_available(BuiltinFeature::muzzle_flashes) == muzzle_available,
          "availability follows registration, hook and dispatcher readiness");
    if (!info_available || !muzzle_available) {
        std::string error;
        check(!apply_builtin_module_policy({root}, policy(true), error) &&
              !error.empty(), "failed activation explains unavailable feature");
        check(!builtin_module_enabled(BuiltinFeature::info_ini) &&
              !builtin_module_enabled(BuiltinFeature::muzzle_flashes),
              "failed activation is atomic");
        return;
    }
    check(defaults && class_loaded && build_hook && hook_calls == 1,
          "callbacks and native detour captured once");
    std::array<std::uint8_t, 64> klass{}, weapon{}, owner{}, node{}, database{};
    std::array<std::uint8_t, 0x80> storm{};
    expected_weapon = weapon.data();
    expected_owner = owner.data();
    expected_database = database.data();
    put(weapon.data(), 4, klass.data());
    put(image + 0x3ad508, 0, storm.data());
    put(storm.data(), 0x44, database.data());
    jump(0x271050, reinterpret_cast<void*>(&native_owner_bridge));
    jump(0x220750, reinterpret_cast<void*>(&native_find_bridge));
    jump(0x733c0, reinterpret_cast<void*>(&native_particle));
    const A2FO_OdfFieldView fields[]{
        {{"muzzleFlashSprite", 17}, {"spark", 5}},
        {{"muzzleFlashSize", 15}, {"9", 1}},
        {{"muzzleFlashDuration", 19}, {"0.25", 4}},
        {{"muzzleFlashColor", 16}, {"0.2 0.4 0.6", 11}},
    };
    A2FO_WeaponClassLoadedEvent event{};
    event.struct_size = sizeof(event);
    event.weapon_class = klass.data();
    event.odf_fields = fields;
    event.odf_field_count = 4;
    toggle(false);
    class_loaded(&event, nullptr); // Metadata arrives before the menu enables it.
    for (bool enabled : {false, true, false, true}) {
        toggle(enabled);
        char directory[256] = "untouched";
        std::uint32_t has_speed = 77;
        std::int32_t speed = 77;
        const bool supplied = defaults("C:\\FleetOps\\Config\\mods\\fixture",
                                       directory, sizeof(directory), &has_speed,
                                       &speed, nullptr);
        check(supplied == enabled, "info defaults live gate");
        if (enabled) {
            check(has_speed == 1 && speed == 4 &&
                  std::strstr(directory, "FixtureSettings") != nullptr,
                  "enabled info defaults payload");
        } else {
            check(has_speed == 77 && speed == 77 &&
                  std::strcmp(directory, "untouched") == 0,
                  "disabled provider does not alter outputs");
        }
        const unsigned before = particles;
        check(build_hook(klass.data(), nullptr,
                         reinterpret_cast<std::uintptr_t>(node.data()), 0,
                         reinterpret_cast<std::uintptr_t>(weapon.data())) ==
              0x12345678, "native ordnance result retained");
        check(particles == before + (enabled ? 1u : 0u),
              "flash emitted only while enabled");
    }
    check(native_args_ok && builds == 4 && particles == 2 && lookups == 2 &&
          info_regs == 1 && weapon_regs == 1 && hook_calls == 1,
          "toggle never re-registers or re-hooks");
}
}

extern "C" std::uintptr_t __cdecl native_build_check(std::uintptr_t weapon) {
    native_args_ok &= reinterpret_cast<void*>(weapon) == expected_weapon;
    ++builds;
    return 0x12345678;
}
extern "C" std::uintptr_t __cdecl native_owner_check(void* weapon) {
    native_args_ok &= weapon == expected_weapon;
    return reinterpret_cast<std::uintptr_t>(expected_owner);
}
extern "C" std::uintptr_t __cdecl native_find_check(
    void* database, std::uintptr_t sprite, std::uintptr_t zero) {
    native_args_ok &= database == expected_database && zero == 0 && sprite &&
                      std::strcmp(reinterpret_cast<const char*>(sprite), "spark") == 0;
    ++lookups;
    return 0x1234;
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    try {
        run(argc > 1 ? argv[1] : "success");
        cleanup();
        std::puts("builtin optional native: PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        cleanup();
        return 1;
    }
}
