/*
 * Fleet Operations technology-tree enforcement for ordinary weapons.
 *
 * Stock Armada/Fleet Ops applies technology entries to special weapons, but
 * ordinary cannon, phaser, pulse, and torpedo WeaponClasses do not consult the
 * owning team's tree before triggering. This module exports a fail-open
 * trigger filter registered with the core's shared Weapon::Trigger event.
 * Unlisted weapon ODFs are intentionally equivalent to a `0` entry.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a_ffi.h"
#include "../../rust/runtime_normal_state_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

extern "C" {
std::uintptr_t __cdecl a2fo_normal_weapon_tech_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_normal_weapon_tech_call_delphi_2(
    void* function, std::uintptr_t argument_eax,
    std::uintptr_t argument_edx);
}

namespace {

constexpr char kModuleName[] = "A2FONormalWeaponTech";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs and layouts.
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;
constexpr std::size_t kWeaponClassOnWeaponOffset = 0x04;
constexpr std::size_t kWeaponClassSpecialOffset = 0x1b4;
constexpr std::size_t kWeaponClassProjectIdOffset = 0x208;
constexpr std::size_t kGameObjectTeamOffset = 0xec;

// FleetOpsHook.dll's team technology-tree array and native recursive
// availability evaluator. The evaluator refreshes the item's current state,
// including its recursive requirements, and uses Delphi's register ABI:
//   EAX = team tree, EDX = project ID.
constexpr std::uintptr_t kTeamTechnologyTreesPointerRva = 0x00212f08;
constexpr std::uintptr_t kTechnologyTreeAllowsProjectRva = 0x00120680;
constexpr std::size_t kTechnologyTreeItemsOffset = 0x0c;
constexpr std::int32_t kMaximumTeamIndex = 63;

constexpr std::uint8_t kExpectedWeaponGetOwner[] = {
    0x8b, 0x49, 0x18, 0x51, 0xe8};
constexpr std::uint8_t kExpectedTechnologyTreeAllowsProject[] = {
    0x53, 0x51, 0x89, 0x14, 0x24, 0x33, 0xd2, 0x8b,
    0x40, 0x0c, 0x8b, 0x0c, 0x24, 0x8b, 0x5c, 0x88};

A2foRsNormalState& checked_normal_runtime_state() noexcept {
    if (!a2fo_normal_state_layout_matches()) std::terminate();
    return *a2fo_rs_normal_state();
}
auto& g_state = checked_normal_runtime_state();
auto& g_api = g_state.api;
auto& g_armada = g_state.armada;
auto& g_fleet_ops = g_state.fleet_ops;
auto& g_runtime_ready = g_state.runtime_ready;
auto& g_logged_first_default_weapon = g_state.logged_first_default_weapon;
auto& g_logged_first_listed_allowed = g_state.logged_first_listed_allowed;
auto& g_logged_first_listed_blocked = g_state.logged_first_listed_blocked;

struct NormalInitContext { std::exception_ptr error; };

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(
              reinterpret_cast<std::uint8_t*>(module) + rva)
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

template <typename T>
T read_live_at(const void* object, std::size_t offset,
               T fallback = T{}) noexcept {
    if (!object) return fallback;
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

void log_decision(const char* decision, std::uint32_t project_id,
                  std::int32_t team_index) noexcept {
    char message[220]{};
    std::snprintf(
        message, sizeof(message),
        "Normal weapon project ID %lu on team %ld %s by its technology-tree entry",
        static_cast<unsigned long>(project_id),
        static_cast<long>(team_index), decision ? decision : "was evaluated");
    log_line(message);
}

std::uintptr_t normal_read(void*, std::uint32_t operation,
                           std::uintptr_t first,
                           std::uintptr_t second) noexcept {
    switch (operation) {
    case 0:
        return reinterpret_cast<std::uintptr_t>(read_live_at<void*>(
            reinterpret_cast<void*>(first), kWeaponClassOnWeaponOffset, nullptr));
    case 1:
        return read_live_at<std::uint8_t>(reinterpret_cast<void*>(first),
                                          kWeaponClassSpecialOffset, 0);
    case 2:
        return reinterpret_cast<std::uintptr_t>(read_live_at<const void*>(
            reinterpret_cast<void*>(first), kWeaponClassProjectIdOffset, nullptr));
    case 3:
        return read_live_at<std::uint32_t>(reinterpret_cast<void*>(first), 0, 0);
    case 4:
        return a2fo_normal_weapon_tech_call_thiscall_0(
            at(g_armada, kWeaponGetOwnerRva), reinterpret_cast<void*>(first));
    case 5:
        return static_cast<std::uintptr_t>(read_live_at<std::int32_t>(
            reinterpret_cast<void*>(first), kGameObjectTeamOffset, -1));
    case 6:
        return reinterpret_cast<std::uintptr_t>(read_live_at<void*>(
            at(g_fleet_ops, kTeamTechnologyTreesPointerRva), 0, nullptr));
    case 7:
        return reinterpret_cast<std::uintptr_t>(read_live_at<void*>(
            reinterpret_cast<void*>(first),
            static_cast<std::size_t>(second) * sizeof(void*), nullptr));
    case 8:
        return reinterpret_cast<std::uintptr_t>(read_live_at<void*>(
            reinterpret_cast<void*>(first), kTechnologyTreeItemsOffset, nullptr));
    case 9:
        return reinterpret_cast<std::uintptr_t>(read_live_at<void*>(
            reinterpret_cast<void*>(first),
            static_cast<std::size_t>(second) * sizeof(void*), nullptr));
    case 10:
        return a2fo_normal_weapon_tech_call_delphi_2(
            at(g_fleet_ops, kTechnologyTreeAllowsProjectRva), first, second);
    default:
        return 0;
    }
}

void normal_log(void*, std::uint32_t kind, std::uint32_t project,
                std::int32_t team) noexcept {
    volatile LONG* first = kind == 0 ? &g_logged_first_default_weapon :
        kind == 1 ? &g_logged_first_listed_allowed :
                    &g_logged_first_listed_blocked;
    if (InterlockedCompareExchange(first, 1, 0) == 0) {
        log_decision(kind == 0 ? "was allowed (unlisted/default 0)" :
                     kind == 1 ? "was allowed" : "was blocked", project, team);
    }
}

bool normal_weapon_technology_allows(void* weapon) noexcept {
    const bool enabled =
#ifdef A2FO_BUILTIN_FEATURE
        a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
        true;
#endif
    return a2fo_rs_runtime_normal_allows(enabled, g_runtime_ready,
        reinterpret_cast<std::uintptr_t>(weapon), &normal_read, &normal_log,
        nullptr) != 0;
}

bool A2FO_CALL weapon_trigger_handler(
    const A2FO_WeaponTriggerEvent* event, void*) {
    const bool valid = event && event->struct_size >= sizeof(*event);
    const bool enabled =
#ifdef A2FO_BUILTIN_FEATURE
        valid && a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
        true;
#endif
    return a2fo_rs_runtime_normal_event(
        valid,
        valid ? event->kind : 0,
        reinterpret_cast<std::uintptr_t>(valid ? event->weapon : nullptr),
        enabled, g_runtime_ready, &normal_read, &normal_log, nullptr) != 0;
}

std::uint8_t normal_init_step(void* context, std::uint32_t operation) noexcept {
    auto& init = *static_cast<NormalInitContext*>(context);
    if (init.error) return 0;
    try {
        switch (operation) {
        case 0:
            g_armada = static_cast<HMODULE>(g_api->armada_module());
            g_fleet_ops = static_cast<HMODULE>(g_api->fleetops_module());
            return g_armada && g_fleet_ops;
        case 1:
            return A2FO_MODULE_API_HAS(g_api, register_weapon_trigger_handler) &&
                g_api->register_weapon_trigger_handler &&
                (g_api->capabilities & A2FO_CAP_WEAPON_TRIGGER_EVENTS) != 0;
        case 2:
            return A2FO_MODULE_API_HAS(
                       g_api, register_weapon_trigger_handler_masked) &&
                    g_api->register_weapon_trigger_handler_masked
                ? g_api->register_weapon_trigger_handler_masked(
                      kModuleName, A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK,
                      &weapon_trigger_handler, nullptr)
                : g_api->register_weapon_trigger_handler(
                      kModuleName, &weapon_trigger_handler, nullptr);
        case 3:
            g_runtime_ready = signature_matches(
                g_armada, kWeaponGetOwnerRva, kExpectedWeaponGetOwner) &&
                signature_matches(g_fleet_ops, kTechnologyTreeAllowsProjectRva,
                                  kExpectedTechnologyTreeAllowsProject);
            return g_runtime_ready;
        default: return 0;
        }
    } catch (...) {
        init.error = std::current_exception();
        return 0;
    }
}
void normal_init_log(void* context, std::uint32_t code) noexcept {
    if (static_cast<NormalInitContext*>(context)->error) return;
    log_line(code == 0 ? "Shared weapon-trigger registration is unavailable" :
             code == 1 ? "Normal-weapon technology-tree filter initialized; unlisted weapons default to 0 (available)" :
                         "Supported weapon or Fleet Operations technology-tree signature was not found; filter disabled");
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid = api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version == A2FO_MODULE_API_VERSION && api->log &&
        api->armada_module && api->fleetops_module;
    if (!valid) return false;
    g_api = api;
    NormalInitContext context{};
    const bool result = a2fo_rs_runtime_normal_init(
        valid, &normal_init_step, &normal_init_log, &context) != 0;
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FONormalWeaponTech_AllowWeaponTrigger(
    void* weapon, const void*) {
    return normal_weapon_technology_allows(weapon);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
}
