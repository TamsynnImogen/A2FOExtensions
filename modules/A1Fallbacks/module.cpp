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
#include "../../rust/module_runtime_b2_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

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
    try {if (g_api && g_api->log && message) g_api->log(kModuleName, message);}catch(...){}
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

struct ReportContext {const std::string* basename;const std::string* sprite_name;const char* source;};
std::uint64_t report_operation(void* opaque,std::uint32_t op) noexcept {
    const auto& c=*static_cast<ReportContext*>(opaque);
    try {
        switch(op) {
        case 0:return g_logged_fallbacks.size();
        case 1:return g_logged_fallbacks.insert(*c.basename+"\n"+*c.sprite_name).second;
        case 2: {
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "Missing wireframe for '%s'; using %s sprite '%s'",
            c.basename->c_str(), c.source, c.sprite_name->c_str());
        log_line(message);
        return 1;
        }
        default:return 0;
        }
    } catch (...) {
        log_line("Missing wireframe fallback selected");
        return 0;
    }
}
void report_fallback(const std::string& basename,const std::string& sprite_name,const char* source) noexcept {
    ReportContext context{&basename,&sprite_name,source};
    a2fo_rs_a1_report(&context,report_operation);
}

struct ResolveContext {
    void* wireframe;const void* native_basename;void** native_layers=nullptr;
    std::string basename,faction; a1fallbacks::SpriteFallbackNames names;
    void* sprite=nullptr;bool failed=false;
};
std::uint64_t resolve_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<ResolveContext*>(opaque);
    try {switch(op) {
    case 0:c.native_layers=reinterpret_cast<void**>(a1_fallbacks_call_thiscall_1(
        g_wireframe_sprite_set_hook.gateway,c.wireframe,reinterpret_cast<std::uintptr_t>(c.native_basename)));
        return reinterpret_cast<std::uintptr_t>(c.native_layers);
    case 1:
#ifdef A2FO_BUILTIN_FEATURE
        return a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
        return 1;
#endif
    case 2:return g_runtime_ready;
    case 3:return native_wireframe_present(c.native_layers);
    case 4:return checked_text(read_value<const char*>(target_class_for(c.wireframe),kClassBasenameOffset,nullptr),&c.basename);
    case 5:c.faction=owner_faction_name(c.wireframe);c.names=a1fallbacks::sprite_fallback_names(c.basename,c.faction);return 1;
    case 6:c.sprite=find_interface_sprite(c.names.build_button);return reinterpret_cast<std::uintptr_t>(c.sprite);
    case 7:c.sprite=find_interface_sprite(c.names.faction_icon);return reinterpret_cast<std::uintptr_t>(c.sprite);
    case 8:case 9:
        g_fallback_layers.fill(nullptr);g_fallback_layers[0]=c.sprite;
        report_fallback(c.basename,op==8?c.names.build_button:c.names.faction_icon,op==8?"build-button":"faction-icon");
        return reinterpret_cast<std::uintptr_t>(g_fallback_layers.data());
    case 10:return c.failed;
    default:return 0;
    }}catch(...){c.failed=true;log_line("Could not resolve a missing wireframe fallback");return 0;}
}

void** __attribute__((fastcall)) wireframe_sprite_set_hook(
    void* wireframe, void*, const void* native_basename) noexcept {
    ResolveContext context{};context.wireframe=wireframe;context.native_basename=native_basename;
    return reinterpret_cast<void**>(a2fo_rs_a1_dispatch(&context,resolve_operation));
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

std::uint64_t init_operation(void* opaque,std::uint32_t op) noexcept {
    const auto* api=static_cast<const A2FO_ModuleApi*>(opaque);
    try {switch(op) {
    case 0:g_api=api;g_armada=static_cast<HMODULE>(api->armada_module());return 1;
    case 1:return signatures_supported();
    case 2:return api->install_inline_hook(at(kWireframeSpriteSetRva),
        reinterpret_cast<void*>(&wireframe_sprite_set_hook),kExpectedWireframeSpriteSet.size(),
        kExpectedWireframeSpriteSet.data(),&g_wireframe_sprite_set_hook);
    case 3:g_runtime_ready=true;return 1;
    case 4:log_line("Supported wireframe resolver signatures were not found; runtime disabled");return 1;
    case 5:log_line("Could not install the wireframe fallback resolver");return 1;
    case 6:log_line("Wireframe -> build button -> faction icon fallback initialized");return 1;
    default:return 0;
    }}catch(...){return 0;}
}
std::uint64_t shutdown_operation(void*,std::uint32_t op) noexcept {
    switch(op) {
    case 0:g_runtime_ready=false;return 1;
    case 1:g_logged_fallbacks.clear();return 1;
    case 2:g_fallback_layers.fill(nullptr);return 1;
    default:return 0;
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version == A2FO_MODULE_API_VERSION && api->log &&
        api->armada_module && api->install_inline_hook;
    return a2fo_rs_a1_init(valid,const_cast<A2FO_ModuleApi*>(api),init_operation)!=0;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    a2fo_rs_a1_shutdown(nullptr,shutdown_operation);
}

#ifndef A2FO_BUILTIN_FEATURE
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH) g_runtime_ready = false;
    return TRUE;
}
#endif
