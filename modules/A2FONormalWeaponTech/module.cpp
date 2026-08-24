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

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;
volatile LONG g_logged_first_default_weapon = 0;
volatile LONG g_logged_first_listed_allowed = 0;
volatile LONG g_logged_first_listed_blocked = 0;

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

bool normal_weapon_technology_allows(void* weapon) noexcept {
    if (!g_runtime_ready || !weapon) return true;

    void* weapon_class = read_live_at<void*>(
        weapon, kWeaponClassOnWeaponOffset, nullptr);
    if (!weapon_class) return true;

    // Special weapons already have a native technology path. Applying the
    // ordinary-weapon filter as well could change their established rules.
    if (read_live_at<std::uint8_t>(
            weapon_class, kWeaponClassSpecialOffset, 0) != 0) {
        return true;
    }

    const void* project_id_object = read_live_at<const void*>(
        weapon_class, kWeaponClassProjectIdOffset, nullptr);
    const std::uint32_t project_id = read_live_at<std::uint32_t>(
        project_id_object, 0, 0);
    if (project_id == 0 || project_id > 0x7fffffffu) return true;

    void* owner = reinterpret_cast<void*>(
        a2fo_normal_weapon_tech_call_thiscall_0(
            at(g_armada, kWeaponGetOwnerRva), weapon));
    const std::int32_t team_index = read_live_at<std::int32_t>(
        owner, kGameObjectTeamOffset, -1);
    if (!owner || team_index < 0 || team_index > kMaximumTeamIndex) {
        return true;
    }

    void* technology_trees = read_live_at<void*>(
        at(g_fleet_ops, kTeamTechnologyTreesPointerRva), 0, nullptr);
    void* team_tree = read_live_at<void*>(
        technology_trees,
        static_cast<std::size_t>(team_index) * sizeof(void*), nullptr);
    void* technology_items = read_live_at<void*>(
        team_tree, kTechnologyTreeItemsOffset, nullptr);
    if (!team_tree || !technology_items) return true;

    void* technology_item = read_live_at<void*>(
        technology_items,
        static_cast<std::size_t>(project_id - 1) * sizeof(void*), nullptr);
    if (!technology_item) {
        if (InterlockedCompareExchange(
                &g_logged_first_default_weapon, 1, 0) == 0) {
            log_decision("was allowed (unlisted/default 0)",
                         project_id, team_index);
        }
        return true;
    }

    const bool allowed =
        (a2fo_normal_weapon_tech_call_delphi_2(
             at(g_fleet_ops, kTechnologyTreeAllowsProjectRva),
             reinterpret_cast<std::uintptr_t>(team_tree),
             static_cast<std::uintptr_t>(project_id)) &
         0xffu) != 0;
    volatile LONG* first = allowed
        ? &g_logged_first_listed_allowed
        : &g_logged_first_listed_blocked;
    if (InterlockedCompareExchange(first, 1, 0) == 0) {
        log_decision(allowed ? "was allowed" : "was blocked",
                     project_id, team_index);
    }
    return allowed;
}

bool A2FO_CALL weapon_trigger_handler(
    const A2FO_WeaponTriggerEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event)) return false;
    if (event->kind == A2FO_WEAPON_TRIGGER_COMMITTED) return true;
    if (event->kind != A2FO_WEAPON_TRIGGER_PRECHECK) return false;
    return normal_weapon_technology_allows(event->weapon);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops) return false;

    if (!A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler) ||
        !api->register_weapon_trigger_handler ||
        (api->capabilities & A2FO_CAP_WEAPON_TRIGGER_EVENTS) == 0) {
        log_line("Shared weapon-trigger registration is unavailable");
        return false;
    }
    const bool trigger_registered =
        A2FO_MODULE_API_HAS(
            api, register_weapon_trigger_handler_masked) &&
        api->register_weapon_trigger_handler_masked
            ? api->register_weapon_trigger_handler_masked(
                  kModuleName,
                  A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK,
                  &weapon_trigger_handler, nullptr)
            : api->register_weapon_trigger_handler(
                  kModuleName, &weapon_trigger_handler, nullptr);
    if (!trigger_registered) {
        log_line("Shared weapon-trigger registration is unavailable");
        return false;
    }

    g_runtime_ready =
        signature_matches(
            g_armada, kWeaponGetOwnerRva, kExpectedWeaponGetOwner) &&
        signature_matches(
            g_fleet_ops, kTechnologyTreeAllowsProjectRva,
            kExpectedTechnologyTreeAllowsProject);
    if (g_runtime_ready) {
        log_line("Normal-weapon technology-tree filter initialized; unlisted weapons default to 0 (available)");
    } else {
        log_line("Supported weapon or Fleet Operations technology-tree signature was not found; filter disabled");
    }
    return true;
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
