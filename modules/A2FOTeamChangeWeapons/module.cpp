// Ownership-triggered weapon activation. Native ownership changes finish
// first; requests enter the next native weapon simulation pass in event order.
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
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
    std::uint64_t activation;
};
const A2FO_ModuleApi* g_api = nullptr;
std::uint8_t* g_armada = nullptr;
bool g_ready = false;
A2FO_InlineHook g_swap{}, g_swap_race{}, g_simulate{};
std::unordered_set<void*> g_classes;
std::vector<Pending> g_pending;
bool g_logged_activation = false;
std::exception_ptr g_team_error;
std::array<std::uint8_t,8> g_simulate_bytes{};
struct TeamPassContext { std::vector<Pending>* snapshot; };

std::uint64_t team_native(void* context, std::uint32_t operation,
                           std::uintptr_t first, std::uintptr_t second,
                           std::uintptr_t, A2foRsTeamPending* pending) noexcept;
std::uint64_t live_activation() noexcept {
    return a2fo_rs_runtime_team_activation(g_ready, &team_native, nullptr);
}

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
void* live_craft(std::uint32_t handle) noexcept {
    using GetCraft = void* (__cdecl*)(std::uint32_t);
    return reinterpret_cast<GetCraft>(at(kGetCraft))(handle);
}
void forget(void* craft) noexcept {
    g_pending.erase(std::remove_if(g_pending.begin(),g_pending.end(),
        [craft](const Pending& item) { return item.craft == craft; }),g_pending.end());
}
std::uint64_t team_native(void* context, std::uint32_t operation,
                           std::uintptr_t first, std::uintptr_t second,
                           std::uintptr_t, A2foRsTeamPending* pending) noexcept {
    if (g_team_error) return 0;
    try {
        auto* pass = static_cast<TeamPassContext*>(context);
        switch (operation) {
        case 0:
#ifdef A2FO_BUILTIN_FEATURE
            return a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE);
#else
            return 1;
#endif
        case 1: return static_cast<std::uintptr_t>(read<std::int32_t>(
            reinterpret_cast<void*>(first),kTeam,-1));
        case 2: return read<std::uint32_t>(reinterpret_cast<void*>(first),kHandle);
        case 3: return reinterpret_cast<std::uintptr_t>(live_craft(
            static_cast<std::uint32_t>(first)));
        case 4: return read<std::uint8_t>(reinterpret_cast<void*>(first),0x27) ||
                       read<std::uint8_t>(reinterpret_cast<void*>(first),0x113);
        case 5: return reinterpret_cast<std::uintptr_t>(read<void*>(
            reinterpret_cast<void*>(first),kCarrier));
        case 6: return reinterpret_cast<std::uintptr_t>(read<void**>(
            reinterpret_cast<void*>(first),kVectorBegin));
        case 7: return reinterpret_cast<std::uintptr_t>(read<void**>(
            reinterpret_cast<void*>(first),kVectorEnd));
        case 8: return readable(reinterpret_cast<void*>(first),second);
        case 9: return reinterpret_cast<std::uintptr_t>(
            reinterpret_cast<void**>(first)[second]);
        case 10: return read<std::uint32_t>(reinterpret_cast<void*>(first),kWeaponOwner);
        case 11: return reinterpret_cast<std::uintptr_t>(read<void*>(
            reinterpret_cast<void*>(first),kWeaponClass));
        case 12: return g_classes.count(reinterpret_cast<void*>(first));
        case 13: forget(reinterpret_cast<void*>(first)); return 0;
        case 14:
            if (pending) g_pending.push_back({pending->handle,
                reinterpret_cast<void*>(pending->craft),pending->team,
                pending->activation});
            return 0;
        case 15:
            if (pass && pass->snapshot) pass->snapshot->swap(g_pending);
            return 0;
        case 16: return pass && pass->snapshot ? pass->snapshot->size() : 0;
        case 17:
            if (pass && pass->snapshot && pending && first < pass->snapshot->size()) {
                const auto& item = (*pass->snapshot)[first];
                *pending = {item.handle,reinterpret_cast<std::uintptr_t>(item.craft),
                            item.team,item.activation};
            }
            return 0;
        case 18: return read<std::uint8_t>(reinterpret_cast<void*>(first),0x2d);
        case 19: return read<std::uint8_t>(reinterpret_cast<void*>(first),0x1de);
        case 20: {
            using GetTarget = void* (__fastcall*)(void*,void*);
            return reinterpret_cast<std::uintptr_t>(
                reinterpret_cast<GetTarget>(at(kGetTarget))(
                    reinterpret_cast<void*>(first),nullptr));
        }
        case 21:
            return a2fo_team_change_thiscall_1(at(kTrigger),
                reinterpret_cast<void*>(first),second);
        case 22: return g_classes.empty();
        case 23:
            if (second) g_classes.insert(reinterpret_cast<void*>(first));
            else g_classes.erase(reinterpret_cast<void*>(first));
            return 0;
        case 26:
            if (g_logged_activation) return 0;
            g_logged_activation = true;
            return 1;
        case 27: g_pending.clear(); g_classes.clear(); return 0;
        case 28: g_ready = false; return 0;
        case 29:
            log("Submitted first ownership-change weapon activation; native firing rules retained");
            return 0;
        default: return 0;
        }
    } catch (...) {
        g_team_error = std::current_exception();
        return 0;
    }
}
void ownership_changed(void* craft, std::int32_t before, std::uint64_t activation) {
    g_team_error = nullptr;
    a2fo_rs_runtime_team_ownership(reinterpret_cast<std::uintptr_t>(craft),
        before,activation,g_ready,&team_native,nullptr);
    if (g_team_error) std::rethrow_exception(g_team_error);
}
std::uintptr_t __fastcall swap_team_hook(void* craft, void*, std::uintptr_t team) noexcept {
    const auto activation = live_activation();
    const auto before = a2fo_rs_runtime_team_before(activation,
        reinterpret_cast<std::uintptr_t>(craft), &team_native, nullptr);
    const auto result = a2fo_team_change_thiscall_1(g_swap.gateway,craft,team);
    try { ownership_changed(craft,before,activation); }
    catch (...) { log("Could not retain team-change activation"); }
    return result;
}
std::uintptr_t __fastcall swap_race_hook(void* craft, void*, std::uintptr_t race,
                                       std::uintptr_t team) noexcept {
    const auto activation = live_activation();
    const auto before = a2fo_rs_runtime_team_before(activation,
        reinterpret_cast<std::uintptr_t>(craft), &team_native, nullptr);
    const auto result = a2fo_team_change_thiscall_2(g_swap_race.gateway,craft,race,team);
    try { ownership_changed(craft,before,activation); }
    catch (...) { log("Could not retain race/team-change activation"); }
    return result;
}

void dispatch_pending(float elapsed) {
    std::vector<Pending> snapshot;
    TeamPassContext context{&snapshot};
    g_team_error = nullptr;
    a2fo_rs_runtime_team_simulate(g_ready,elapsed,&team_native,&context);
    if (g_team_error) std::rethrow_exception(g_team_error);
}
void __cdecl simulate_all_hook(float elapsed) noexcept {
    try { dispatch_pending(elapsed); }
    catch (...) { log("Could not dispatch team-change activation"); }
    using Simulate = void (__cdecl*)(float);
    reinterpret_cast<Simulate>(g_simulate.gateway)(elapsed);
}

std::uint8_t team_field(void* context, std::uint32_t index,
                        A2foRsRuntimeField* out) noexcept {
    const auto* event = static_cast<const A2FO_WeaponClassLoadedEvent*>(context);
    if (!event || !event->odf_fields || index >= event->odf_field_count || !out) return 0;
    const auto& field = event->odf_fields[index];
    *out = {{reinterpret_cast<const std::uint8_t*>(field.name.data),field.name.size},
            {reinterpret_cast<const std::uint8_t*>(field.value.data),field.value.size},
            static_cast<std::uint8_t>(field.value.data || field.value.size == 0)};
    return 1;
}
void A2FO_CALL class_loaded(const A2FO_WeaponClassLoadedEvent* event, void*) {
    g_team_error = nullptr;
    const bool valid = g_ready && event && event->struct_size >= sizeof(*event);
    a2fo_rs_runtime_team_class_loaded(g_ready,
        valid,
        reinterpret_cast<std::uintptr_t>(valid ? event->weapon_class : nullptr),
        valid && event->odf_fields ? event->odf_field_count : 0,
        &team_field,&team_native,const_cast<A2FO_WeaponClassLoadedEvent*>(event));
    if (g_team_error) std::rethrow_exception(g_team_error);
}
void A2FO_CALL craft_event(const A2FO_CraftEvent* event, void*) {
    g_team_error = nullptr;
    const bool valid = event && event->struct_size >= sizeof(*event);
    a2fo_rs_runtime_team_craft_event(valid,
        valid ? event->kind : 0,
        reinterpret_cast<std::uintptr_t>(valid ? event->craft : nullptr),
        &team_native,nullptr);
    if (g_team_error) std::rethrow_exception(g_team_error);
}
} // namespace

namespace {
std::uint8_t team_init_step(void*, std::uint32_t operation) noexcept {
    if (g_team_error) return 0;
    try {
        switch (operation) {
        case 0:
            g_armada = static_cast<std::uint8_t*>(g_api->armada_module());
            return g_armada != nullptr;
        case 1: {
            g_simulate_bytes = {0x55,0x8b,0xec,0xa1,0,0,0,0};
            const auto list = reinterpret_cast<std::uintptr_t>(at(kWeaponList));
            static_assert(sizeof(list)==4,"Armada modules must be built for x86");
            std::memcpy(g_simulate_bytes.data()+4,&list,4);
            return matches(kSwapTeam,kSwapBytes) && matches(kSwapRaceAndTeam,kSwapBytes) &&
                readable(at(kSimulateAll),g_simulate_bytes.size()) &&
                std::memcmp(at(kSimulateAll),g_simulate_bytes.data(),g_simulate_bytes.size())==0 &&
                matches(kGetCraft,kGetCraftBytes) && matches(kTrigger,kTriggerBytes) &&
                matches(kGetTarget,kGetTargetBytes);
        }
        case 2: {
            const char* fields[]{kCommand};
            return g_api->register_weapon_class_loaded_handler(
                kModuleName,fields,1,&class_loaded,nullptr);
        }
        case 3:
            return g_api->register_craft_event_handler_masked(kModuleName,
                A2FO_CRAFT_EVENT_MASK_CLEANUP | A2FO_CRAFT_EVENT_MASK_POST_LOAD,
                &craft_event,nullptr);
        case 4:
            return g_api->install_inline_hook(at(kSimulateAll),
                reinterpret_cast<void*>(&simulate_all_hook),g_simulate_bytes.size(),
                g_simulate_bytes.data(),&g_simulate);
        case 5:
            return g_api->install_inline_hook(at(kSwapTeam),
                reinterpret_cast<void*>(&swap_team_hook),sizeof(kSwapBytes),
                kSwapBytes,&g_swap);
        case 6:
            return g_api->install_inline_hook(at(kSwapRaceAndTeam),
                reinterpret_cast<void*>(&swap_race_hook),sizeof(kSwapBytes),
                kSwapBytes,&g_swap_race);
        default: return 0;
        }
    } catch (...) { g_team_error = std::current_exception(); return 0; }
}
void team_init_log(void*, std::uint32_t code) noexcept {
    if (g_team_error) return;
    log(code == 0 ? "Team-change weapons unavailable: native signature mismatch; no hooks installed" :
        code == 1 ? "activateOnTeamChange initialized; native weapon activation on the next weapon pass" :
                    "Team-change weapons disabled; partial hooks retained as native pass-throughs");
}
} // namespace

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid = api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version == A2FO_MODULE_API_VERSION && api->log && api->armada_module &&
        api->install_inline_hook &&
        A2FO_MODULE_API_HAS(api,register_craft_event_handler_masked) &&
        api->register_craft_event_handler_masked && api->register_weapon_class_loaded_handler &&
        (api->capabilities & (A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_CRAFT_EVENTS)) ==
            (A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_CRAFT_EVENTS);
    if (!valid) return false;
    g_api = api;
    g_team_error = nullptr;
    std::uint8_t ready = 0;
    const bool result = a2fo_rs_runtime_team_init(valid,&team_init_step,
        &team_init_log,nullptr,&ready) != 0;
    if (g_team_error) std::rethrow_exception(g_team_error);
    if (result) g_ready = ready != 0;
    return result;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    g_team_error = nullptr;
    a2fo_rs_runtime_team_shutdown(&team_native,nullptr);
    if (g_team_error) std::rethrow_exception(g_team_error);
}
