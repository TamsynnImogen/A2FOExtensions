#include "extended_weapon_icons.hpp"
#include "../../rust/extended_icons_ffi.h"

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_identity_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
}

namespace a2fo::craft_identity {
namespace {

constexpr char kModuleName[] = "A2FOCraftIdentity";
constexpr char kSystemsDisplayRectangle[] = "infoSingleSystemsDisplay";

constexpr std::uintptr_t kParameterDbGetRectangleRva = 0x001358f0;
constexpr std::uintptr_t kGuiParameterDbPointerRva = 0x0036502c;
constexpr std::uintptr_t kShipSystemIconConstructorRva = 0x001ed100;
constexpr std::uintptr_t kShipDisplayCleanupRva = 0x001edfa0;
constexpr std::uintptr_t kShipDisplayPostLoadRva = 0x001ee05c;
constexpr std::uintptr_t kDisplaySingleObjectRva = 0x001ee868;
constexpr std::uintptr_t kDisplaySingleBuilderRva = 0x001ee9bc;
constexpr std::uintptr_t kSimulateSingleObjectRva = 0x001eeb14;
constexpr std::uintptr_t kSimulateSingleBuilderRva = 0x001eec14;
constexpr std::uintptr_t kAlwaysGameSimulateRva = 0x001eed14;

constexpr std::size_t kShipSystemIconSize = 0x50;
constexpr std::size_t kSelectedCraftOffset = 0x1e8;
constexpr std::size_t kSystemsDisplayOffset = 0x308;
constexpr std::size_t kIconSelectedCraftOffset = 0x28;
constexpr std::size_t kIconIndexOffset = 0x30;
constexpr std::size_t kIconFlashPhaseOffset = 0x48;
constexpr std::size_t kIconFlashStateOffset = 0x4c;
constexpr std::size_t kSystemsDisplayActiveSpriteOffset = 0x2c;
constexpr std::size_t kDestructorVtableOffset = 0x08;
constexpr std::size_t kSimulateVtableOffset = 0x0c;
constexpr std::size_t kRenderVtableOffset = 0x10;

constexpr std::array<std::uint8_t, 9> kExpectedParameterDbGetRectangle{{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x01, 0x00, 0x00,
}};
constexpr std::array<std::uint8_t, 8> kExpectedShipSystemIconConstructor{{
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xec, 0x56, 0x57,
}};
constexpr std::array<std::uint8_t, 7> kExpectedCleanup{{
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57,
}};
constexpr std::array<std::uint8_t, 9> kExpectedPostLoad{{
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xc4, 0x53, 0x56, 0x57,
}};
constexpr std::array<std::uint8_t, 7> kExpectedDisplayOrSimulate{{
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57,
}};
constexpr std::array<std::uint8_t, 8> kExpectedAlwaysGameSimulate{{
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x89, 0x4d, 0xfc,
}};

struct RawRectangle {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct SidecarIcons {
    std::array<void*, kExtendedWeaponIconCount> icons{};
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_hooks_armed = false;
LONG g_creation_report_count = 0;
std::unordered_map<void*, SidecarIcons> g_sidecars;

A2FO_InlineHook g_cleanup_hook{};
A2FO_InlineHook g_post_load_hook{};
A2FO_InlineHook g_display_single_object_hook{};
A2FO_InlineHook g_display_single_builder_hook{};
A2FO_InlineHook g_simulate_single_object_hook{};
A2FO_InlineHook g_simulate_single_builder_hook{};
A2FO_InlineHook g_always_game_simulate_hook{};

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

bool writable_range(void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xffu;
    const bool writable = protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
    if (!writable) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

bool executable_address(const void* address) noexcept {
    if (!address) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    switch (information.Protect & 0xffu) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

template <typename T>
T read_at(const void* object, std::size_t offset, T fallback = T{}) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            sizeof(T))) {
        return fallback;
    }
    T value{};
    std::memcpy(
        &value, static_cast<const std::uint8_t*>(object) + offset,
        sizeof(value));
    return value;
}

template <typename T>
bool write_at(void* object, std::size_t offset, const T& value) noexcept {
    if (!object) return false;
    auto* destination = static_cast<std::uint8_t*>(object) + offset;
    if (!writable_range(destination, sizeof(value))) return false;
    std::memcpy(destination, &value, sizeof(value));
    return true;
}

template <std::size_t Size>
bool signature_matches(
    HMODULE module, std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, expected.size()) &&
        std::memcmp(address, expected.data(), expected.size()) == 0;
}

void* virtual_method(void* object, std::size_t offset) noexcept {
    void* vtable = read_at<void*>(object, 0, nullptr);
    void* method = read_at<void*>(vtable, offset, nullptr);
    return executable_address(method) ? method : nullptr;
}

void destroy_icon(void* icon) noexcept {
    if (!icon) return;
    void* destructor = virtual_method(icon, kDestructorVtableOffset);
    if (destructor) {
        // Zero runs the scalar destructor without asking Armada to release
        // memory allocated by this DLL's C runtime.
        a2fo_identity_call_thiscall_1(destructor, icon, 0);
    }
    std::free(icon);
}

struct SidecarContext {
    void* ship{};RawRectangle rectangle{};void* systems{};void* craft{};void* icon{};void* method{};
    std::size_t method_offset{};SidecarIcons pending{};
    decltype(g_sidecars)::iterator found{};
};
std::uintptr_t destroy_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<SidecarContext*>(value);
    switch(op){case 0:c.found=g_sidecars.find(c.ship);return c.found!=g_sidecars.end();
    case 1:destroy_icon(c.found->second.icons[i]);return 1;case 2:g_sidecars.erase(c.found);return 1;default:return 0;}
}
void destroy_sidecars(void* ship_display) noexcept {SidecarContext c{};c.ship=ship_display;a2fo_rs_icon_destroy(&c,&destroy_step);}

void* gui_parameter_db() noexcept {
    return read_at<void*>(
        at(g_armada, kGuiParameterDbPointerRva), 0, nullptr);
}

bool read_systems_display_rectangle(RawRectangle* rectangle) noexcept {
    void* parameter_db = gui_parameter_db();
    if (!parameter_db || !rectangle) return false;
    const RawRectangle fallback{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetRectangleRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(kSystemsDisplayRectangle),
        reinterpret_cast<std::uintptr_t>(rectangle),
        reinterpret_cast<std::uintptr_t>(&fallback));
    return (found & 0xffu) != 0;
}

std::uintptr_t create_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<SidecarContext*>(value);
    switch(op){
    case 0:return c.ship && writable_range(c.ship,0x30c);
    case 1:return read_systems_display_rectangle(&c.rectangle);
    case 2:c.systems=read_at<void*>(c.ship,kSystemsDisplayOffset,nullptr);return c.systems!=nullptr;
    case 3:c.icon=std::calloc(1,kShipSystemIconSize);return c.icon!=nullptr;
    case 4:a2fo_identity_call_thiscall_4(at(g_fleet_ops,kShipSystemIconConstructorRva),c.icon,
        reinterpret_cast<std::uintptr_t>(c.ship),reinterpret_cast<std::uintptr_t>(&c.rectangle),i+kNativeWeaponIconLimit,reinterpret_cast<std::uintptr_t>(c.systems));return 1;
    case 5:return read_at<std::size_t>(c.icon,kIconIndexOffset,kExtendedWeaponIconLimit);
    case 6:destroy_icon(c.icon);return 1;
    case 7:destroy_icon(c.pending.icons[i]);return 1;
    case 8:c.pending.icons[i]=c.icon;return 1;
    case 9:try{return g_sidecars.emplace(c.ship,c.pending).second?2:1;}catch(...){return 0;}
    case 10:return InterlockedCompareExchange(&g_creation_report_count,1,0);
    case 11:
        log_line(i==0?"infoSingleSystemsDisplay is unavailable; extended weapon icons were not created":i==1?"Could not allocate extended weapon-icon controls":i==2?"Extended weapon-icon constructor validation failed":i==3?"Could not retain extended weapon-icon controls":"Created native-compatible weapon icon controls for slots 33 through 128");return 1;
    default:return 0;
    }
}
void create_sidecars(void* ship_display) noexcept {SidecarContext c{};c.ship=ship_display;a2fo_rs_icon_create(&c,&create_step);}

bool systems_display_accepts(void* systems_display, void* craft) noexcept {
    // Each native display/simulate gateway has just refreshed this sprite by
    // running SystemsDisplay's craft predicate. Reuse its result rather than
    // repeating the race-name allocation and sprite-database lookup.
    return craft && read_at<void*>(
        systems_display, kSystemsDisplayActiveSpriteOffset, nullptr);
}

std::uintptr_t virtual_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<SidecarContext*>(value);
    switch(op){
    case 0:c.found=g_sidecars.find(c.ship);return c.found!=g_sidecars.end();
    case 1:c.craft=read_at<void*>(c.ship,kSelectedCraftOffset,nullptr);return reinterpret_cast<std::uintptr_t>(c.craft);
    case 2:c.systems=read_at<void*>(c.ship,kSystemsDisplayOffset,nullptr);return 1;
    case 3:return systems_display_accepts(c.systems,c.craft);
    case 4:c.icon=c.found->second.icons[i];return c.icon!=nullptr;
    case 5:return write_at(c.icon,kIconSelectedCraftOffset,c.craft);
    case 6:c.method=virtual_method(c.icon,c.method_offset);return reinterpret_cast<std::uintptr_t>(c.method);
    case 7:a2fo_identity_call_thiscall_0(c.method,c.icon);return 1;
    default:return 0;
    }
}
void run_sidecar_virtuals(void* ship_display,std::size_t method_offset) noexcept {
    SidecarContext c{};c.ship=ship_display;c.method_offset=method_offset;a2fo_rs_icon_virtuals(&c,&virtual_step);
}

void update_sidecar_flash(void* ship_display,
                          std::uintptr_t elapsed_bits) noexcept {
    static_assert(sizeof(std::uintptr_t) == sizeof(float),
                  "Fleet Operations hooks require a 32-bit build");
    const auto found = g_sidecars.find(ship_display);
    if (found == g_sidecars.end()) return;
    for (void* icon : found->second.icons) {
        float phase = read_at<float>(icon, kIconFlashPhaseOffset, 0.0f);
        std::uint8_t state = read_at<std::uint8_t>(
            icon, kIconFlashStateOffset, 0);
        a2fo_rs_icon_flash(phase,static_cast<std::uint32_t>(elapsed_bits),state,&phase,&state);
        write_at(icon, kIconFlashPhaseOffset, phase);
        write_at(icon, kIconFlashStateOffset, state);
    }
}

struct HookContext {void* ship;std::uintptr_t elapsed;unsigned kind;};
A2FO_InlineHook& lifecycle_hook(unsigned kind) noexcept {
    A2FO_InlineHook* hooks[]{&g_cleanup_hook,&g_post_load_hook,&g_display_single_object_hook,&g_display_single_builder_hook,&g_simulate_single_object_hook,&g_simulate_single_builder_hook,&g_always_game_simulate_hook};return *hooks[kind];
}
std::uintptr_t hook_step(void* value,std::uint32_t op,std::uint32_t) noexcept {
    auto& c=*static_cast<HookContext*>(value);auto& hook=lifecycle_hook(c.kind);
    switch(op){
    case 0:destroy_sidecars(c.ship);return 1;
    case 1:return hook.gateway!=nullptr;
    case 2:if(c.kind>=4)a2fo_identity_call_thiscall_1(hook.gateway,c.ship,c.elapsed);else a2fo_identity_call_thiscall_0(hook.gateway,c.ship);return 1;
    case 3:create_sidecars(c.ship);return 1;
    case 4:run_sidecar_virtuals(c.ship,c.kind>=4?kSimulateVtableOffset:kRenderVtableOffset);return 1;
    case 5:update_sidecar_flash(c.ship,c.elapsed);return 1;
    case 6:return g_hooks_armed;
    default:return 0;
    }
}
void dispatch_hook(unsigned kind,void* ship,std::uintptr_t elapsed=0) noexcept {HookContext c{ship,elapsed,kind};a2fo_rs_icon_hook(kind,&c,&hook_step);}
void __attribute__((fastcall)) cleanup_hook(void* ship,void*) noexcept {dispatch_hook(0,ship);}
void __attribute__((fastcall)) post_load_hook(void* ship,void*) noexcept {dispatch_hook(1,ship);}
void __attribute__((fastcall)) display_single_object_hook(void* ship,void*) noexcept {dispatch_hook(2,ship);}
void __attribute__((fastcall)) display_single_builder_hook(void* ship,void*) noexcept {dispatch_hook(3,ship);}
void __attribute__((fastcall)) simulate_single_object_hook(void* ship,void*,std::uintptr_t elapsed) noexcept {dispatch_hook(4,ship,elapsed);}
void __attribute__((fastcall)) simulate_single_builder_hook(void* ship,void*,std::uintptr_t elapsed) noexcept {dispatch_hook(5,ship,elapsed);}
void __attribute__((fastcall)) always_game_simulate_hook(void* ship,void*,std::uintptr_t elapsed) noexcept {dispatch_hook(6,ship,elapsed);}

bool signature_at(unsigned index) noexcept {
    switch(index){
    case 0:return signature_matches(g_armada,kParameterDbGetRectangleRva,kExpectedParameterDbGetRectangle);
    case 1:return signature_matches(g_fleet_ops,kShipSystemIconConstructorRva,kExpectedShipSystemIconConstructor);
    case 2:return signature_matches(g_fleet_ops,kShipDisplayCleanupRva,kExpectedCleanup);
    case 3:return signature_matches(g_fleet_ops,kShipDisplayPostLoadRva,kExpectedPostLoad);
    case 4:return signature_matches(g_fleet_ops,kDisplaySingleObjectRva,kExpectedDisplayOrSimulate);
    case 5:return signature_matches(g_fleet_ops,kDisplaySingleBuilderRva,kExpectedDisplayOrSimulate);
    case 6:return signature_matches(g_fleet_ops,kSimulateSingleObjectRva,kExpectedDisplayOrSimulate);
    case 7:return signature_matches(g_fleet_ops,kSimulateSingleBuilderRva,kExpectedDisplayOrSimulate);
    case 8:return signature_matches(g_fleet_ops,kAlwaysGameSimulateRva,kExpectedAlwaysGameSimulate);
    default:return false;
    }
}

template <std::size_t Size>
bool install_hook(
    std::uintptr_t rva, void* replacement,
    const std::array<std::uint8_t, Size>& expected,
    A2FO_InlineHook* hook) noexcept {
    return g_api->install_inline_hook(
        at(g_fleet_ops, rva), replacement, expected.size(),
        expected.data(), hook) && hook->gateway;
}

struct InstallContext {const A2FO_ModuleApi* api;void* armada;void* fleetops;};
std::uintptr_t install_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<InstallContext*>(value);
    switch(op){
    case 0:return c.api && c.api->install_inline_hook && c.armada && c.fleetops;
    case 1:g_api=c.api;g_armada=static_cast<HMODULE>(c.armada);g_fleet_ops=static_cast<HMODULE>(c.fleetops);return 1;
    case 2:return signature_at(i);
    case 3:
        switch(i){
        case 0:return install_hook(kShipDisplayCleanupRva,reinterpret_cast<void*>(&cleanup_hook),kExpectedCleanup,&g_cleanup_hook);
        case 1:return install_hook(kShipDisplayPostLoadRva,reinterpret_cast<void*>(&post_load_hook),kExpectedPostLoad,&g_post_load_hook);
        case 2:return install_hook(kDisplaySingleObjectRva,reinterpret_cast<void*>(&display_single_object_hook),kExpectedDisplayOrSimulate,&g_display_single_object_hook);
        case 3:return install_hook(kDisplaySingleBuilderRva,reinterpret_cast<void*>(&display_single_builder_hook),kExpectedDisplayOrSimulate,&g_display_single_builder_hook);
        case 4:return install_hook(kSimulateSingleObjectRva,reinterpret_cast<void*>(&simulate_single_object_hook),kExpectedDisplayOrSimulate,&g_simulate_single_object_hook);
        case 5:return install_hook(kSimulateSingleBuilderRva,reinterpret_cast<void*>(&simulate_single_builder_hook),kExpectedDisplayOrSimulate,&g_simulate_single_builder_hook);
        case 6:return install_hook(kAlwaysGameSimulateRva,reinterpret_cast<void*>(&always_game_simulate_hook),kExpectedAlwaysGameSimulate,&g_always_game_simulate_hook);
        default:return 0;
        }
    case 4:g_hooks_armed=i!=0;return 1;
    case 5:log_line(i==0?"Extended weapon-icon signatures were not found; slots above 32 remain native-only":i==1?"weaponXiconpos capacity extended from 32 to 128 selected-panel controls":"An extended weapon-icon lifecycle hook failed; partial hooks remain pass-through");return 1;
    default:return 0;
    }
}
}  // namespace
bool install_extended_weapon_icons(const A2FO_ModuleApi* api,void* armada,void* fleetops) noexcept {InstallContext c{api,armada,fleetops};return a2fo_rs_icon_install(&c,&install_step)!=0;}
}  // namespace a2fo::craft_identity
