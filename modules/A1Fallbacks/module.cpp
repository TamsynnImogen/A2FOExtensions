/*
 * Missing wireframe fallbacks for Armada 1-era GUI assets.
 *
 * Armada's shared wireframe cache is used by the selected-object display and
 * its BuildQueueIcon subclass. Preserve every native one-to-five-layer result.
 * When the complete set is absent, return one interface sprite using the
 * object's build-button key and then its current owner's faction icon.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "fallback_policy.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

extern "C" {
std::uintptr_t __cdecl a1_fallbacks_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a1_fallbacks_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
}

namespace {

constexpr char kModuleName[] = "A1Fallbacks";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kWireframeSpriteSetRva = 0x000f7a60;
constexpr std::uintptr_t kInterfaceSpriteDatabaseGetRva = 0x00220750;
constexpr std::uintptr_t kInterfaceSpriteDatabasePointerRva = 0x00365030;
constexpr std::uintptr_t kWireframeIconVtableRva = 0x002b4f18;
constexpr std::uintptr_t kBuildQueueIconVtableRva = 0x002b4994;

constexpr std::size_t kWireframeOwnerOffset = 0x28;
constexpr std::size_t kBuildQueueTargetClassOffset = 0x3c;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectRaceOffset = 0xfc;
constexpr std::size_t kClassBasenameOffset = 0x7c;
constexpr std::size_t kRaceNameOffset = 0x14;
constexpr std::size_t kWireframeLayerCount = 5;
constexpr std::size_t kMaximumLoggedFallbacks = 64;

constexpr std::array<std::uint8_t, 10> kExpectedWireframeSpriteSet{{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0x2d, 0xf9, 0x69, 0x00}};
constexpr std::array<std::uint8_t, 9> kExpectedInterfaceSpriteDatabaseGet{{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x53, 0x56, 0x57}};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
A2FO_InlineHook g_wireframe_sprite_set_hook{};
bool g_runtime_ready = false;
thread_local std::array<void*, kWireframeLayerCount> g_fallback_layers{};
std::unordered_set<std::string> g_logged_fallbacks;

void* at(std::uintptr_t rva) noexcept {
    return g_armada
        ? static_cast<void*>(
              reinterpret_cast<std::uint8_t*>(g_armada) + rva)
        : nullptr;
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || size == 0) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (begin + size < begin) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto region_begin = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    const auto region_end = region_begin + information.RegionSize;
    return begin >= region_begin && begin + size <= region_end;
}

template <typename T>
T read_value(const void* base, std::size_t offset,
             T fallback = T{}) noexcept {
    if (!base) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(base) + offset;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

bool checked_text(const char* text, std::string* result) {
    if (!text || !result) return false;
    for (std::size_t length = 0;
         length <= a1fallbacks::kMaximumSpriteNameLength; ++length) {
        if (!readable_range(text + length, sizeof(char))) return false;
        if (text[length] == '\0') {
            if (length == 0) return false;
            result->assign(text, length);
            return true;
        }
    }
    return false;
}

bool native_wireframe_present(void** layers) noexcept {
    if (!readable_range(
            layers, kWireframeLayerCount * sizeof(void*))) {
        return false;
    }
    for (std::size_t index = 0; index < kWireframeLayerCount; ++index) {
        if (layers[index]) return true;
    }
    return false;
}

void* target_class_for(void* wireframe) noexcept {
    void* const vtable = read_value<void*>(wireframe, 0, nullptr);
    if (vtable == at(kBuildQueueIconVtableRva)) {
        return read_value<void*>(
            wireframe, kBuildQueueTargetClassOffset, nullptr);
    }
    if (vtable != at(kWireframeIconVtableRva)) return nullptr;
    void* const owner = read_value<void*>(
        wireframe, kWireframeOwnerOffset, nullptr);
    return read_value<void*>(owner, kObjectClassOffset, nullptr);
}

void* find_interface_sprite(const std::string& name) noexcept {
    if (name.empty()) return nullptr;
    void* const database = read_value<void*>(
        at(kInterfaceSpriteDatabasePointerRva), 0, nullptr);
    if (!database) return nullptr;
    return reinterpret_cast<void*>(a1_fallbacks_call_thiscall_2(
        at(kInterfaceSpriteDatabaseGetRva), database,
        reinterpret_cast<std::uintptr_t>(name.c_str()), 0));
}

std::string owner_faction_name(void* wireframe) {
    void* const owner = read_value<void*>(
        wireframe, kWireframeOwnerOffset, nullptr);
    void* const race = read_value<void*>(owner, kObjectRaceOffset, nullptr);
    std::string faction;
    if (!race) return faction;
    checked_text(
        static_cast<const char*>(race) + kRaceNameOffset, &faction);
    return faction;
}

void report_fallback(
    const std::string& basename, const std::string& sprite_name,
    const char* source) noexcept {
    if (g_logged_fallbacks.size() >= kMaximumLoggedFallbacks) return;
    try {
        const std::string key = basename + "\n" + sprite_name;
        if (!g_logged_fallbacks.insert(key).second) return;
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "Missing wireframe for '%s'; using %s sprite '%s'",
            basename.c_str(), source, sprite_name.c_str());
        log_line(message);
    } catch (...) {
        log_line("Missing wireframe fallback selected");
    }
}

void** __attribute__((fastcall)) wireframe_sprite_set_hook(
    void* wireframe, void*, const void* native_basename) noexcept {
    void** const native_layers = reinterpret_cast<void**>(
        a1_fallbacks_call_thiscall_1(
            g_wireframe_sprite_set_hook.gateway, wireframe,
            reinterpret_cast<std::uintptr_t>(native_basename)));
    if (!g_runtime_ready || native_wireframe_present(native_layers)) {
        return native_layers;
    }

    try {
        void* const object_class = target_class_for(wireframe);
        std::string basename;
        if (!checked_text(
                read_value<const char*>(
                    object_class, kClassBasenameOffset, nullptr),
                &basename)) {
            return native_layers;
        }
        const std::string faction = owner_faction_name(wireframe);
        const auto names = a1fallbacks::sprite_fallback_names(
            basename, faction);

        void* sprite = find_interface_sprite(names.build_button);
        const char* source = "build-button";
        const std::string* selected_name = &names.build_button;
        if (!sprite) {
            sprite = find_interface_sprite(names.faction_icon);
            source = "faction-icon";
            selected_name = &names.faction_icon;
        }
        if (!sprite) return native_layers;

        g_fallback_layers.fill(nullptr);
        g_fallback_layers[0] = sprite;
        report_fallback(basename, *selected_name, source);
        return g_fallback_layers.data();
    } catch (...) {
        log_line("Could not resolve a missing wireframe fallback");
        return native_layers;
    }
}

bool signatures_supported() noexcept {
    return g_armada &&
        readable_range(
            at(kWireframeSpriteSetRva),
            kExpectedWireframeSpriteSet.size()) &&
        std::memcmp(
            at(kWireframeSpriteSetRva),
            kExpectedWireframeSpriteSet.data(),
            kExpectedWireframeSpriteSet.size()) == 0 &&
        readable_range(
            at(kInterfaceSpriteDatabaseGetRva),
            kExpectedInterfaceSpriteDatabaseGet.size()) &&
        std::memcmp(
            at(kInterfaceSpriteDatabaseGetRva),
            kExpectedInterfaceSpriteDatabaseGet.data(),
            kExpectedInterfaceSpriteDatabaseGet.size()) == 0 &&
        readable_range(
            at(kInterfaceSpriteDatabasePointerRva), sizeof(void*));
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!signatures_supported()) {
        log_line("Supported wireframe resolver signatures were not found; "
                 "runtime disabled");
        return true;
    }
    if (!api->install_inline_hook(
            at(kWireframeSpriteSetRva),
            reinterpret_cast<void*>(&wireframe_sprite_set_hook),
            kExpectedWireframeSpriteSet.size(),
            kExpectedWireframeSpriteSet.data(),
            &g_wireframe_sprite_set_hook)) {
        log_line("Could not install the wireframe fallback resolver");
        return true;
    }
    g_runtime_ready = true;
    log_line("Wireframe -> build button -> faction icon fallback initialized");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
    g_logged_fallbacks.clear();
    g_fallback_layers.fill(nullptr);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH) g_runtime_ready = false;
    return TRUE;
}
