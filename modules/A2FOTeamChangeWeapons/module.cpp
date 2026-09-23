// Ownership-triggered weapon activation. Native ownership changes finish
// first; requests enter the next native weapon simulation pass in event order.
#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <unordered_set>
#include <vector>

extern "C" std::uintptr_t __cdecl a2fo_team_change_thiscall_1(
    void*, void*, std::uintptr_t);
extern "C" std::uintptr_t __cdecl a2fo_team_change_thiscall_2(
    void*, void*, std::uintptr_t, std::uintptr_t);

namespace {
constexpr char kModuleName[] = "A2FOTeamChangeWeapons";
constexpr char kCommand[] = "activateOnTeamChange";
constexpr std::uintptr_t kSwapTeam = 0xd0ea0;
constexpr std::uintptr_t kSwapRaceAndTeam = 0xd0ed0;
constexpr std::uintptr_t kSimulateAll = 0x26eb00;
constexpr std::uintptr_t kWeaponList = 0x3b5564;
constexpr std::uintptr_t kGetCraft = 0x13800;
constexpr std::uintptr_t kTrigger = 0x271290;
constexpr std::uintptr_t kGetTarget = 0x271300;
constexpr std::uint8_t kGetTargetBytes[]{0x8b,0x49,0x38,0x51,0xe8};
constexpr std::uint8_t kSwapBytes[]{0x55,0x8b,0xec,0x56,0x8b,0xf1};
constexpr std::uint8_t kGetCraftBytes[]{0x55,0x8b,0xec,0x8b,0x45,0x08};
constexpr std::uint8_t kTriggerBytes[]{0x55,0x8b,0xec,0x8b,0x45,0x08};
constexpr std::size_t kHandle = 0x28, kTeam = 0xec;
constexpr std::size_t kCarrier = 0x128, kVectorBegin = 0x0c, kVectorEnd = 0x10;
constexpr std::size_t kWeaponClass = 0x04, kWeaponOwner = 0x18;
constexpr std::size_t kMaximumWeapons = 256;

struct Pending {
    std::uint32_t handle;
    void* craft;
    std::int32_t team;
};
const A2FO_ModuleApi* g_api = nullptr;
std::uint8_t* g_armada = nullptr;
bool g_ready = false;
A2FO_InlineHook g_swap{}, g_swap_race{}, g_simulate{};
std::unordered_set<void*> g_classes;
std::vector<Pending> g_pending;
bool g_logged_activation = false;

void log(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}
void* at(std::uintptr_t rva) noexcept { return g_armada ? g_armada+rva : nullptr; }
bool readable(const void* pointer, std::size_t size) noexcept {
    if (!pointer || !size) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(pointer, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto offset = reinterpret_cast<std::uintptr_t>(pointer) -
        reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    return offset < info.RegionSize && size <= info.RegionSize-offset;
}
template<class T> T read(const void* pointer, std::size_t offset, T fallback = {}) noexcept {
    if (!pointer) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(pointer)+offset;
    if (!readable(address,sizeof(T))) return fallback;
    T value; std::memcpy(&value,address,sizeof(value)); return value;
}
template<std::size_t N> bool matches(std::uintptr_t rva, const std::uint8_t (&bytes)[N]) noexcept {
    return readable(at(rva), N) && std::memcmp(at(rva),bytes,N)==0;
}
bool valid_team(std::int32_t team) noexcept { return team >= 0 && team <= 63; }
void* live_craft(std::uint32_t handle) noexcept {
    using GetCraft = void* (__cdecl*)(std::uint32_t);
    return reinterpret_cast<GetCraft>(at(kGetCraft))(handle);
}

// Slots are visited in native order. Never retain native vector iterators or
// Weapon pointers across ownership calls or simulation passes.
std::size_t configured_weapons(void* craft, std::array<void*,kMaximumWeapons>& output) noexcept {
    const auto handle = read<std::uint32_t>(craft,kHandle);
    if (live_craft(handle) != craft || !craft ||
        read<std::uint8_t>(craft,0x27) || read<std::uint8_t>(craft,0x113)) return 0;
    void* carrier = read<void*>(craft,kCarrier);
    auto** begin = read<void**>(carrier,kVectorBegin);
    auto** end = read<void**>(carrier,kVectorEnd);
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    if (!first || last < first || (last-first)%sizeof(void*) ||
        (last-first)/sizeof(void*) > kMaximumWeapons || !readable(begin,last-first)) return 0;
    std::size_t count = 0;
    for (std::size_t i=0; i<(last-first)/sizeof(void*); ++i) {
        void* weapon = begin[i];
        if (weapon && read<std::uint32_t>(weapon,kWeaponOwner) == handle &&
            g_classes.count(read<void*>(weapon,kWeaponClass))) output[count++] = weapon;
    }
    return count;
}

void forget(void* craft) noexcept {
    g_pending.erase(std::remove_if(g_pending.begin(),g_pending.end(),
        [craft](const Pending& item) { return item.craft == craft; }),g_pending.end());
}
void ownership_changed(void* craft, std::int32_t before) {
    if (!g_ready || g_classes.empty()) return;
    const auto after = read<std::int32_t>(craft,kTeam,-1);
    if (!valid_team(before) || !valid_team(after) || before == after) return;
    // Coalesce rapid changes before the next weapon pass to the final owner.
    // Erase even when no configured weapon remains, cancelling stale work.
    forget(craft);
    std::array<void*,kMaximumWeapons> weapons{};
    if (configured_weapons(craft,weapons)) {
        g_pending.push_back({read<std::uint32_t>(craft,kHandle),craft,after});
    }
}
std::uintptr_t __fastcall swap_team_hook(void* craft, void*, std::uintptr_t team) noexcept {
    const auto before = read<std::int32_t>(craft,kTeam,-1);
    const auto result = a2fo_team_change_thiscall_1(g_swap.gateway,craft,team);
    try { ownership_changed(craft,before); }
    catch (...) { log("Could not retain team-change activation"); }
    return result;
}
std::uintptr_t __fastcall swap_race_hook(void* craft, void*, std::uintptr_t race,
                                       std::uintptr_t team) noexcept {
    const auto before = read<std::int32_t>(craft,kTeam,-1);
    const auto result = a2fo_team_change_thiscall_2(g_swap_race.gateway,craft,race,team);
    try { ownership_changed(craft,before); }
    catch (...) { log("Could not retain race/team-change activation"); }
    return result;
}

void dispatch_pending() {
    std::vector<Pending> pending;
    pending.swap(g_pending); // callbacks can enqueue the next pass safely
    for (const auto& item : pending) {
        void* craft = live_craft(item.handle);
        if (craft != item.craft || !craft || read<std::int32_t>(craft,kTeam,-1) != item.team) continue;
        std::array<void*,kMaximumWeapons> weapons{};
        const auto count = configured_weapons(craft,weapons);
        bool requested = false;
        for (std::size_t i=0; i<count; ++i) {
            // +0x2d is the native toggled-on state (also used by self-destruct).
            // Triggering it again would switch it OFF, cancelling a countdown.
            if (read<std::uint8_t>(weapons[i],0x2d)) continue;
            void* target = nullptr;
            const auto* cls = read<void*>(weapons[i],kWeaponClass);
            if (read<std::uint8_t>(cls,0x1de)) { // native needTarget
                // With no stack arguments, fastcall ECX plus ignored EDX is
                // identical to the engine's MSVC thiscall getter ABI.
                using GetTarget = void* (__fastcall*)(void*,void*);
                target = reinterpret_cast<GetTarget>(at(kGetTarget))(weapons[i],nullptr);
            }
            // Use an existing native target when needed; do not invent one or
            // redirect fire at the new owner/capturing unit. NULL still permits
            // the weapon class's targetless or automatic-acquisition behavior.
            // Call the public entry, retaining the core's shared prechecks.
            a2fo_team_change_thiscall_1(at(kTrigger),weapons[i],reinterpret_cast<std::uintptr_t>(target));
            requested = true;
        }
        if (requested && !g_logged_activation) {
            g_logged_activation = true;
            log("Submitted first ownership-change weapon activation; native firing rules retained");
        }
    }
}
void __cdecl simulate_all_hook(float elapsed) noexcept {
    if (g_ready && std::isfinite(elapsed) && elapsed > 0) {
        try { dispatch_pending(); }
        catch (...) { log("Could not dispatch team-change activation"); }
    }
    using Simulate = void (__cdecl*)(float);
    reinterpret_cast<Simulate>(g_simulate.gateway)(elapsed);
}

std::string_view view(A2FO_StringView value) noexcept {
    return value.data ? std::string_view(value.data,value.size) : std::string_view{};
}
bool equal_ci(std::string_view left, std::string_view right) noexcept {
    return left.size() == right.size() && std::equal(left.begin(),left.end(),right.begin(),
        [](unsigned char a, unsigned char b) { return std::tolower(a)==std::tolower(b); });
}
bool enabled_value(std::string_view value) noexcept {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value.remove_prefix(1); value.remove_suffix(1);
    }
    return value == "1" || equal_ci(value,"true");
}

void A2FO_CALL class_loaded(const A2FO_WeaponClassLoadedEvent* event, void*) {
    if (!g_ready || !event || event->struct_size < sizeof(*event) || !event->weapon_class) return;
    // Field views already contain the resolved include chain. An absent or
    // explicit zero command must remove a stale policy for a reloaded class.
    bool enabled = false;
    for (std::uint32_t i=0; event->odf_fields && i<event->odf_field_count; ++i) {
        const auto& field = event->odf_fields[i];
        if (equal_ci(view(field.name),kCommand)) {
            enabled = enabled_value(view(field.value));
        }
    }
    if (enabled) g_classes.insert(event->weapon_class);
    else g_classes.erase(event->weapon_class);
}
void A2FO_CALL craft_event(const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event)) return;
    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP || event->kind == A2FO_CRAFT_EVENT_POST_LOAD) {
        forget(event->craft);
    }
}
} // namespace

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log || !api->armada_module ||
        !api->install_inline_hook ||
        !A2FO_MODULE_API_HAS(api,register_craft_event_handler_masked) ||
        !api->register_craft_event_handler_masked || !api->register_weapon_class_loaded_handler ||
        (api->capabilities & (A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_CRAFT_EVENTS)) !=
            (A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_CRAFT_EVENTS)) return false;
    g_api = api;
    g_armada = static_cast<std::uint8_t*>(api->armada_module());
    // The absolute list operand is relocated with the image. Check the entire
    // instruction using its loaded address, rather than a preferred-base byte.
    std::uint8_t simulate_bytes[]{0x55,0x8b,0xec,0xa1,0,0,0,0};
    const auto list = reinterpret_cast<std::uintptr_t>(at(kWeaponList));
    static_assert(sizeof(list)==4,"Armada modules must be built for x86");
    std::memcpy(simulate_bytes+4,&list,4);
    if (!g_armada || !matches(kSwapTeam,kSwapBytes) || !matches(kSwapRaceAndTeam,kSwapBytes) ||
        !matches(kSimulateAll,simulate_bytes) || !matches(kGetCraft,kGetCraftBytes) ||
        !matches(kTrigger,kTriggerBytes) || !matches(kGetTarget,kGetTargetBytes)) {
        log("Team-change weapons unavailable: native signature mismatch; no hooks installed");
        return false;
    }
    const char* fields[]{kCommand};
    if (!api->register_weapon_class_loaded_handler(kModuleName,fields,1,&class_loaded,nullptr) ||
        !api->register_craft_event_handler_masked(kModuleName,
            A2FO_CRAFT_EVENT_MASK_CLEANUP | A2FO_CRAFT_EVENT_MASK_POST_LOAD,
            &craft_event,nullptr)) return false;
    g_ready = api->install_inline_hook(at(kSimulateAll),reinterpret_cast<void*>(&simulate_all_hook),
                    sizeof(simulate_bytes),simulate_bytes,&g_simulate) &&
        api->install_inline_hook(at(kSwapTeam),reinterpret_cast<void*>(&swap_team_hook),
                    sizeof(kSwapBytes),kSwapBytes,&g_swap) &&
        api->install_inline_hook(at(kSwapRaceAndTeam),reinterpret_cast<void*>(&swap_race_hook),
                    sizeof(kSwapBytes),kSwapBytes,&g_swap_race);
    log(g_ready ? "activateOnTeamChange initialized; native weapon activation on the next weapon pass"
                : "Team-change weapons disabled; partial hooks retained as native pass-throughs");
    return true; // any installed hook must remain resident, even on partial failure
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    g_ready = false;
    g_pending.clear();
    g_classes.clear();
}
