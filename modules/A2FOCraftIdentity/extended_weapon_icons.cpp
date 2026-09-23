#include "extended_weapon_icons.hpp"

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

void destroy_sidecars(void* ship_display) noexcept {
    const auto found = g_sidecars.find(ship_display);
    if (found == g_sidecars.end()) return;
    for (void* icon : found->second.icons) destroy_icon(icon);
    g_sidecars.erase(found);
}

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

void create_sidecars(void* ship_display) noexcept {
    if (!ship_display || !writable_range(ship_display, 0x30c)) return;
    RawRectangle systems_rectangle{};
    if (!read_systems_display_rectangle(&systems_rectangle)) {
        log_line(
            "infoSingleSystemsDisplay is unavailable; extended weapon icons were not created");
        return;
    }
    void* systems_display = read_at<void*>(
        ship_display, kSystemsDisplayOffset, nullptr);
    if (!systems_display) return;

    SidecarIcons sidecars{};
    for (std::size_t sidecar_index = 0;
         sidecar_index < sidecars.icons.size(); ++sidecar_index) {
        void* icon = std::calloc(1, kShipSystemIconSize);
        if (!icon) {
            for (void* allocated : sidecars.icons) destroy_icon(allocated);
            log_line("Could not allocate extended weapon-icon controls");
            return;
        }
        const std::size_t weapon_index =
            sidecar_index + kNativeWeaponIconLimit;
        a2fo_identity_call_thiscall_4(
            at(g_fleet_ops, kShipSystemIconConstructorRva), icon,
            reinterpret_cast<std::uintptr_t>(ship_display),
            reinterpret_cast<std::uintptr_t>(&systems_rectangle),
            static_cast<std::uintptr_t>(weapon_index),
            reinterpret_cast<std::uintptr_t>(systems_display));
        if (read_at<std::size_t>(
                icon, kIconIndexOffset,
                kExtendedWeaponIconLimit) != weapon_index) {
            destroy_icon(icon);
            for (void* allocated : sidecars.icons) destroy_icon(allocated);
            log_line("Extended weapon-icon constructor validation failed");
            return;
        }
        sidecars.icons[sidecar_index] = icon;
    }

    try {
        const auto inserted = g_sidecars.emplace(ship_display, sidecars);
        if (!inserted.second) {
            for (void* icon : sidecars.icons) destroy_icon(icon);
            return;
        }
    } catch (...) {
        for (void* icon : sidecars.icons) destroy_icon(icon);
        log_line("Could not retain extended weapon-icon controls");
        return;
    }

    if (InterlockedCompareExchange(&g_creation_report_count, 1, 0) == 0) {
        log_line(
            "Created native-compatible weapon icon controls for slots 33 through 128");
    }
}

bool systems_display_accepts(void* systems_display, void* craft) noexcept {
    // Each native display/simulate gateway has just refreshed this sprite by
    // running SystemsDisplay's craft predicate. Reuse its result rather than
    // repeating the race-name allocation and sprite-database lookup.
    return craft && read_at<void*>(
        systems_display, kSystemsDisplayActiveSpriteOffset, nullptr);
}

void run_sidecar_virtuals(
    void* ship_display, std::size_t method_offset) noexcept {
    const auto found = g_sidecars.find(ship_display);
    if (found == g_sidecars.end()) return;
    void* craft = read_at<void*>(
        ship_display, kSelectedCraftOffset, nullptr);
    void* systems_display = read_at<void*>(
        ship_display, kSystemsDisplayOffset, nullptr);
    if (!systems_display_accepts(systems_display, craft)) return;

    for (void* icon : found->second.icons) {
        if (!icon || !write_at(icon, kIconSelectedCraftOffset, craft)) {
            continue;
        }
        void* method = virtual_method(icon, method_offset);
        if (method) a2fo_identity_call_thiscall_0(method, icon);
    }
}

void update_sidecar_flash(void* ship_display,
                          std::uintptr_t elapsed_bits) noexcept {
    static_assert(sizeof(std::uintptr_t) == sizeof(float),
                  "Fleet Operations hooks require a 32-bit build");
    const auto found = g_sidecars.find(ship_display);
    if (found == g_sidecars.end()) return;
    float elapsed = 0.0f;
    std::memcpy(&elapsed, &elapsed_bits, sizeof(elapsed));
    for (void* icon : found->second.icons) {
        float phase = read_at<float>(icon, kIconFlashPhaseOffset, 0.0f);
        std::uint8_t state = read_at<std::uint8_t>(
            icon, kIconFlashStateOffset, 0);
        phase += elapsed;
        while (phase > 1.0f) {
            phase -= 1.0f;
            state ^= 1u;
        }
        write_at(icon, kIconFlashPhaseOffset, phase);
        write_at(icon, kIconFlashStateOffset, state);
    }
}

void __attribute__((fastcall)) cleanup_hook(
    void* ship_display, void*) noexcept {
    if (g_hooks_armed) destroy_sidecars(ship_display);
    if (g_cleanup_hook.gateway) {
        a2fo_identity_call_thiscall_0(
            g_cleanup_hook.gateway, ship_display);
    }
}

void __attribute__((fastcall)) post_load_hook(
    void* ship_display, void*) noexcept {
    if (g_hooks_armed) destroy_sidecars(ship_display);
    if (g_post_load_hook.gateway) {
        a2fo_identity_call_thiscall_0(
            g_post_load_hook.gateway, ship_display);
    }
    if (g_hooks_armed) create_sidecars(ship_display);
}

void __attribute__((fastcall)) display_single_object_hook(
    void* ship_display, void*) noexcept {
    if (g_display_single_object_hook.gateway) {
        a2fo_identity_call_thiscall_0(
            g_display_single_object_hook.gateway, ship_display);
    }
    if (g_hooks_armed) {
        run_sidecar_virtuals(ship_display, kRenderVtableOffset);
    }
}

void __attribute__((fastcall)) display_single_builder_hook(
    void* ship_display, void*) noexcept {
    if (g_display_single_builder_hook.gateway) {
        a2fo_identity_call_thiscall_0(
            g_display_single_builder_hook.gateway, ship_display);
    }
    if (g_hooks_armed) {
        run_sidecar_virtuals(ship_display, kRenderVtableOffset);
    }
}

void __attribute__((fastcall)) simulate_single_object_hook(
    void* ship_display, void*, std::uintptr_t elapsed_bits) noexcept {
    if (g_simulate_single_object_hook.gateway) {
        a2fo_identity_call_thiscall_1(
            g_simulate_single_object_hook.gateway, ship_display,
            elapsed_bits);
    }
    if (g_hooks_armed) {
        run_sidecar_virtuals(ship_display, kSimulateVtableOffset);
    }
}

void __attribute__((fastcall)) simulate_single_builder_hook(
    void* ship_display, void*, std::uintptr_t elapsed_bits) noexcept {
    if (g_simulate_single_builder_hook.gateway) {
        a2fo_identity_call_thiscall_1(
            g_simulate_single_builder_hook.gateway, ship_display,
            elapsed_bits);
    }
    if (g_hooks_armed) {
        run_sidecar_virtuals(ship_display, kSimulateVtableOffset);
    }
}

void __attribute__((fastcall)) always_game_simulate_hook(
    void* ship_display, void*, std::uintptr_t elapsed_bits) noexcept {
    if (g_always_game_simulate_hook.gateway) {
        a2fo_identity_call_thiscall_1(
            g_always_game_simulate_hook.gateway, ship_display,
            elapsed_bits);
    }
    if (g_hooks_armed) update_sidecar_flash(ship_display, elapsed_bits);
}

bool preflight_signatures() noexcept {
    bool supported = signature_matches(
        g_armada, kParameterDbGetRectangleRva,
        kExpectedParameterDbGetRectangle);
    supported = signature_matches(
        g_fleet_ops, kShipSystemIconConstructorRva,
        kExpectedShipSystemIconConstructor) && supported;
    supported = signature_matches(
        g_fleet_ops, kShipDisplayCleanupRva,
        kExpectedCleanup) && supported;
    supported = signature_matches(
        g_fleet_ops, kShipDisplayPostLoadRva,
        kExpectedPostLoad) && supported;
    supported = signature_matches(
        g_fleet_ops, kDisplaySingleObjectRva,
        kExpectedDisplayOrSimulate) && supported;
    supported = signature_matches(
        g_fleet_ops, kDisplaySingleBuilderRva,
        kExpectedDisplayOrSimulate) && supported;
    supported = signature_matches(
        g_fleet_ops, kSimulateSingleObjectRva,
        kExpectedDisplayOrSimulate) && supported;
    supported = signature_matches(
        g_fleet_ops, kSimulateSingleBuilderRva,
        kExpectedDisplayOrSimulate) && supported;
    supported = signature_matches(
        g_fleet_ops, kAlwaysGameSimulateRva,
        kExpectedAlwaysGameSimulate) && supported;
    return supported;
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

}  // namespace

bool install_extended_weapon_icons(
    const A2FO_ModuleApi* api,
    void* armada_module,
    void* fleetops_module) noexcept {
    if (!api || !api->install_inline_hook || !armada_module ||
        !fleetops_module) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(armada_module);
    g_fleet_ops = static_cast<HMODULE>(fleetops_module);
    if (!preflight_signatures()) {
        log_line(
            "Extended weapon-icon signatures were not found; slots above 32 remain native-only");
        return false;
    }

    bool installed = install_hook(
        kShipDisplayCleanupRva,
        reinterpret_cast<void*>(&cleanup_hook), kExpectedCleanup,
        &g_cleanup_hook);
    installed = install_hook(
        kShipDisplayPostLoadRva,
        reinterpret_cast<void*>(&post_load_hook), kExpectedPostLoad,
        &g_post_load_hook) && installed;
    installed = install_hook(
        kDisplaySingleObjectRva,
        reinterpret_cast<void*>(&display_single_object_hook),
        kExpectedDisplayOrSimulate,
        &g_display_single_object_hook) && installed;
    installed = install_hook(
        kDisplaySingleBuilderRva,
        reinterpret_cast<void*>(&display_single_builder_hook),
        kExpectedDisplayOrSimulate,
        &g_display_single_builder_hook) && installed;
    installed = install_hook(
        kSimulateSingleObjectRva,
        reinterpret_cast<void*>(&simulate_single_object_hook),
        kExpectedDisplayOrSimulate,
        &g_simulate_single_object_hook) && installed;
    installed = install_hook(
        kSimulateSingleBuilderRva,
        reinterpret_cast<void*>(&simulate_single_builder_hook),
        kExpectedDisplayOrSimulate,
        &g_simulate_single_builder_hook) && installed;
    installed = install_hook(
        kAlwaysGameSimulateRva,
        reinterpret_cast<void*>(&always_game_simulate_hook),
        kExpectedAlwaysGameSimulate,
        &g_always_game_simulate_hook) && installed;

    g_hooks_armed = installed;
    if (installed) {
        log_line(
            "weaponXiconpos capacity extended from 32 to 128 selected-panel controls");
    } else {
        log_line(
            "An extended weapon-icon lifecycle hook failed; partial hooks remain pass-through");
    }
    return installed;
}

}  // namespace a2fo::craft_identity
