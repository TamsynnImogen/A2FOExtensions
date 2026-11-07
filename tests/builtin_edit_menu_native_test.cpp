// Synthetic editor update fixture; no keyboard, mouse, window or game loop.
#include "../core/builtin_edit_menu.cpp"
#include "../core/module_loader.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using namespace a2fo;
using Update = void (__attribute__((fastcall)) *)(void*, void*);
std::uint8_t* image;
std::string root;
Update update;
unsigned installs, native_updates;
bool fail_hook, arguments_ok = true, toggle_in_native, original_visible;
void* expected_menu;
NativeRootMenu* native_root;
NativeRootMenu original_root{};

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void A2FO_CALL logger(const char*, const char*) {}
void* A2FO_CALL armada() { return image; }
std::uint32_t A2FO_CALL roots() { return 0; }
const char* A2FO_CALL root_at(std::uint32_t) { return nullptr; }
void policy(bool enabled) {
    ModulePolicy selection;
    selection.managed = true;
    ModulePolicyEntry entry;
    entry.name = "A2FOEditMenu";
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    selection.entries.push_back(entry);
    std::string error;
    check(apply_builtin_module_policy({root}, selection, error),
          error.empty() ? "EditMenu policy" : error.c_str());
}
extern "C" void __cdecl native_update(void* self) {
    ++native_updates;
    arguments_ok &= self == expected_menu;
    original_visible = native_root && std::memcmp(native_root, &original_root,
                                                  sizeof(original_root)) == 0;
    if (toggle_in_native) { toggle_in_native = false; policy(false); policy(true); }
    const std::int32_t level = 2;
    std::memcpy(static_cast<std::uint8_t*>(self) + kEditMenuLevelOffset,
                &level, sizeof(level));
}
extern "C" __attribute__((naked)) void native_update_bridge() {
    __asm__ volatile("pushl %ecx\ncall _native_update\naddl $4,%esp\nret");
}
bool A2FO_CALL install(void* target, void* replacement, std::size_t size,
                       const std::uint8_t* expected, A2FO_InlineHook* hook) {
    check(target == image + kEditMenuUpdateRva &&
          std::memcmp(target, expected, size) == 0, "EditMenu hook target and signature");
    ++installs;
    update = reinterpret_cast<Update>(replacement);
    if (fail_hook) return false;
    *hook = {target, reinterpret_cast<void*>(&native_update_bridge),
             static_cast<std::uint32_t>(size)};
    return true;
}
void seed(bool signature_failure) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(image, "EditMenu synthetic image");
    std::memcpy(image + kEditMenuUpdateRva, kExpectedEditMenuUpdate.data(),
                kExpectedEditMenuUpdate.size());
    if (signature_failure) image[kEditMenuUpdateRva] ^= 1;
    native_root = reinterpret_cast<NativeRootMenu*>(image + kEditMenuRootSlotsRva);
    original_root.source_name = const_cast<char*>("original.odf");
    original_root.title = const_cast<char*>("Original");
    *native_root = original_root;
}
void call(std::array<std::uint8_t, 0x40>& menu) {
    expected_menu = menu.data();
    const unsigned before = native_updates;
    update(menu.data(), nullptr);
    check(native_updates == before + 1 && arguments_ok,
          "EditMenu native Update called exactly once with self");
}
void install_scratch(std::array<std::uint8_t, 0x40>& menu) {
    g_saved_root = original_root;
    *native_root = NativeRootMenu{}; // Null strings need no native allocator cleanup.
    g_scratch_installed = true;
    g_active_edit_menu = menu.data();
    g_active_root = 0;
    g_current_node = "synthetic.odf";
    g_breadcrumbs = {"original.odf"};
}
void run(const std::string& mode) {
    char temporary[MAX_PATH]{}, folder[MAX_PATH]{};
    check(GetTempPathA(sizeof(temporary), temporary), "temporary path");
    check(GetTempFileNameA(temporary, "bem", 0, folder), "temporary filename");
    check(DeleteFileA(folder) && CreateDirectoryA(folder, nullptr), "private root");
    root = folder;
    fail_hook = mode == "hook-fail";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger; api.armada_module = armada;
    api.extension_root_count = roots; api.extension_root = root_at;
    api.install_inline_hook = install;
    set_builtin_module_roots({root});
    const bool ready = mode == "success";
    check(prepare_builtin_module(BuiltinFeature::edit_menu, api, false) == ready,
          "EditMenu preparation");
    finish_builtin_module_preparation(false, false, false, false, false);
    check(builtin_module_available(BuiltinFeature::edit_menu) == ready,
          "EditMenu native availability");
    check(installs == (mode == "signature" ? 0u : 1u), "EditMenu hook count");
    std::array<std::uint8_t, 0x40> menu{};
    std::int32_t level = 1;
    std::memcpy(menu.data() + kEditMenuLevelOffset, &level, sizeof(level));
    if (!ready) {
        ModulePolicy selection;
        ModulePolicyEntry entry;
        entry.name = "A2FOEditMenu";
        entry.state = ModulePolicyState::active;
        selection.entries.push_back(entry);
        std::string error;
        check(!apply_builtin_module_policy({root}, selection, error) && !error.empty(),
              "unavailable EditMenu activation refused");
        if (update && g_edit_menu_update_hook.gateway) call(menu);
        return;
    }
    check(prepare_builtin_module(BuiltinFeature::edit_menu, api, false) &&
          installs == 1, "EditMenu preparation idempotent");
    call(menu);
    check(original_visible && !g_scratch_installed &&
          *reinterpret_cast<std::int32_t*>(menu.data() + kEditMenuLevelOffset) == 2,
          "disabled Update runs native and does not navigate");
    policy(true);
    install_scratch(menu);
    call(menu);
    check(original_visible && !g_scratch_installed &&
          *reinterpret_cast<std::int32_t*>(menu.data() + kEditMenuLevelOffset) == 2,
          "activation change restores retained scratch before native Update");
    install_scratch(menu);
    policy(false);
    call(menu);
    check(original_visible && !g_scratch_installed,
          "disabling restores scratch before native Update");
    policy(true);
    call(menu); // Synchronize activation before an in-flight change.
    level = 1;
    std::memcpy(menu.data() + kEditMenuLevelOffset, &level, sizeof(level));
    toggle_in_native = true;
    call(menu);
    check(!g_scratch_installed && g_breadcrumbs.empty() && installs == 1,
          "off/on inside native Update invalidates pending custom navigation");
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "usage: builtin_edit_menu_native_test.exe success|signature|hook-fail");
        run(argv[1]);
        std::puts("builtin EditMenu native fixture: PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
