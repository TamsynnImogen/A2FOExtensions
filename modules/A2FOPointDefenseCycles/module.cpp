/*
 * CannonImp-compatible firing cycles for Fleet Operations' two point-defense
 * weapon classes. The module changes timing only: native target selection,
 * interception, hit chance, effects, sounds, and ownership remain in the
 * original simulation paths.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "firing_cycle.hpp"
#include "../../rust/module_runtime_b4_ffi.h"

#include <windows.h>

#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
void a2fo_point_defense_odw_delay_bridge();
void a2fo_point_defense_pdl_timer_gate_bridge();

void* a2fo_point_defense_odw_continue = nullptr;
void* a2fo_point_defense_pdl_timer_ready = nullptr;
void* a2fo_point_defense_pdl_timer_blocked = nullptr;
std::uint32_t __cdecl a2fo_point_defense_select_odw_delay(void* weapon);
std::uint32_t __cdecl a2fo_point_defense_advance_pdl_timer(
    void* weapon, float elapsed_seconds);
}

namespace {

using a2fo::point_defense::CyclePolicy;
using a2fo::point_defense::CycleState;
using a2fo::point_defense::OptionalNumber;
using a2fo::point_defense::PolicyStatus;
using a2fo::point_defense::kMaximumShotDelays;

constexpr char kModuleName[] = "A2FOPointDefenseCycles";
constexpr char kBuildId[] = "safe-prefire-gate-20260812-01";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kPointDefenseBuildClassRva = 0x0026c090;
constexpr std::uintptr_t kPointDefenseSimulateRva = 0x0026c180;
constexpr std::uintptr_t kPointDefenseTimerGateRva = 0x0026c1d7;
constexpr std::uintptr_t kPointDefenseTimerReadyRva = 0x0026c1dd;
constexpr std::uintptr_t kPointDefenseTimerBlockedRva = 0x0026c3c7;
constexpr std::uintptr_t kWeaponResetShotTimerRva = 0x00270dd0;
constexpr std::uintptr_t kWeaponLoadRva = 0x0026ebc0;
constexpr std::uintptr_t kWeaponSaveRva = 0x0026ec50;
constexpr std::uintptr_t kWeaponDestructorRva = 0x0026eed0;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00134df0;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kFileWriterOutFloatRva = 0x0012ee70;
constexpr std::uintptr_t kFileWriterOutIntegerRva = 0x0012ee30;
constexpr std::uintptr_t kFileReaderInFloatRva = 0x0012efb0;
constexpr std::uintptr_t kFileReaderInIntegerRva = 0x0012ef70;

constexpr std::uintptr_t kOrdnanceDefenseBuildClassRva = 0x001f8794;
constexpr std::uintptr_t kOrdnanceDefenseSimulateRva = 0x001f80cc;
constexpr std::uintptr_t kOrdnanceDefenseDelaySiteRva = 0x00134ea4;
constexpr std::uintptr_t kOrdnanceDefenseDelayContinueRva = 0x001f83ee;

constexpr std::size_t kWeaponClassOnWeaponOffset = 0x04;
constexpr std::size_t kShotTimerOnWeaponOffset = 0x28;
constexpr std::size_t kShotDelayOnWeaponClassOffset = 0x24c;

constexpr std::uint8_t kExpectedPointDefenseBuildClass[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedPointDefenseSimulate[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x74};
constexpr std::uint8_t kExpectedPointDefenseTimerGate[] = {
    0xe9, 0x44, 0x58, 0x19, 0x00};
constexpr std::uint8_t kExpectedWeaponResetShotTimer[] = {
    0x55, 0x8b, 0xec, 0x51, 0x56};
constexpr std::uint8_t kExpectedWeaponLoad[] = {
    0x55, 0x8b, 0xec, 0x53, 0x56};
constexpr std::uint8_t kExpectedWeaponSave[] = {
    0x55, 0x8b, 0xec, 0x53, 0x56};
constexpr std::uint8_t kExpectedWeaponDestructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedParameterDbGetFloat[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetString[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedFileWriterOutFloat[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedFileWriterOutInteger[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedFileReaderInFloat[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedFileReaderInInteger[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedOrdnanceDefenseBuildClass[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8};
constexpr std::uint8_t kExpectedOrdnanceDefenseSimulate[] = {
    0x55, 0x8b, 0xec, 0x81, 0xc4, 0x54, 0xff, 0xff, 0xff};
constexpr std::uint8_t kExpectedOrdnanceDefenseDelaySite[] = {
    0x8b, 0x45, 0xfc, 0x89, 0xc2, 0x8b, 0x52, 0x04,
    0xd9, 0x82, 0x4c, 0x02, 0x00, 0x00, 0xd9, 0x58,
    0x28, 0x8b, 0x55, 0xe8, 0xe9, 0x31, 0x35, 0x0c, 0x00};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;

A2FO_InlineHook g_point_defense_build_class_hook{};
A2FO_InlineHook g_point_defense_simulate_hook{};
A2FO_InlineHook g_ordnance_defense_build_class_hook{};
A2FO_InlineHook g_ordnance_defense_simulate_hook{};
A2FO_InlineHook g_weapon_reset_shot_timer_hook{};
A2FO_InlineHook g_weapon_load_hook{};
A2FO_InlineHook g_weapon_save_hook{};
A2FO_InlineHook g_weapon_destructor_hook{};

std::unordered_map<void*, CyclePolicy> g_class_policies;
std::unordered_map<void*, CycleState> g_weapon_states;
bool g_logged_point_defense_gate = false;

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) {
        g_api->log(kModuleName, message);
    }
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(
              reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

template <typename T>
T read_at(const void* address, std::size_t offset, T fallback = T{}) noexcept {
    if (!address) return fallback;
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(address) + offset,
                sizeof(value));
    return value;
}

template <typename T>
void write_at(void* address, std::size_t offset, const T& value) noexcept {
    if (!address) return;
    std::memcpy(static_cast<std::uint8_t*>(address) + offset,
                &value, sizeof(value));
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    void* address = at(module, rva);
    return address && std::memcmp(address, expected, Size) == 0;
}

template <std::size_t Size>
bool install_hook(HMODULE module, std::uintptr_t rva, void* replacement,
                  const std::uint8_t (&expected)[Size],
                  A2FO_InlineHook* hook) noexcept {
    return g_api && g_api->install_inline_hook && hook &&
        g_api->install_inline_hook(at(module, rva), replacement, Size,
                                   expected, hook) && hook->gateway;
}

std::uint32_t float_bits(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

OptionalNumber read_optional_number(void* parameter_db,
                                    const char* key) noexcept {
    OptionalNumber result{};
    if (!parameter_db || !key || !g_armada) return result;
    float value = 0.0f;
    const std::uintptr_t found = a2fo_point_defense_call_thiscall_3(
        at(g_armada, kParameterDbGetFloatRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(&value), 0);
    if ((found & 0xffu) != 0) {
        result.present = true;
        result.valid = std::isfinite(value);
        result.value = value;
        return result;
    }

    // Distinguish a malformed retained command from an absent command. This
    // also accepts quoted numeric values used by some ODF generators.
    std::array<char, 128> text{};
    const std::uintptr_t string_found =
        a2fo_point_defense_call_thiscall_4(
            at(g_armada, kParameterDbGetStringRva), parameter_db,
            reinterpret_cast<std::uintptr_t>(key),
            reinterpret_cast<std::uintptr_t>(text.data()),
            static_cast<std::uintptr_t>(text.size()),
            reinterpret_cast<std::uintptr_t>(""));
    text.back() = '\0';
    if ((string_found & 0xffu) == 0) return result;

    result.present = true;
    char* end = nullptr;
    errno = 0;
    value = std::strtof(text.data(), &end);
    if (end == text.data() || errno == ERANGE || !std::isfinite(value)) {
        return result;
    }
    if (*end == 'f' || *end == 'F') ++end;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return result;
    result.valid = true;
    result.value = value;
    return result;
}

const char* status_description(PolicyStatus status) noexcept {
    switch (status) {
        case PolicyStatus::valid: return "valid";
        case PolicyStatus::missing_delay: return "missing/invalid shotDelay";
        case PolicyStatus::non_contiguous_delays:
            return "shotDelayX entries are not contiguous from shotDelay0";
        case PolicyStatus::invalid_delay:
            return "a shotDelayX entry is malformed or negative";
        case PolicyStatus::invalid_save_point:
            return "saveFireCyclePoint is not a valid sequence index";
        case PolicyStatus::invalid_reset_time:
            return "shotCycleResetTime is malformed or negative";
    }
    return "invalid configuration";
}

struct RegisterContext {
    void* weapon_class;
    void* parameter_db;
    const char* class_label;
    std::array<OptionalNumber, kMaximumShotDelays> numbered{};
    OptionalNumber overflow{}, save_point{}, reset_time{};
    CyclePolicy policy{};
};

std::uint64_t register_policy_op(void* opaque, std::uint32_t operation,
                                 std::uint32_t argument) noexcept {
    auto& context = *static_cast<RegisterContext*>(opaque);
    switch (operation) {
        case 0: {
            char key[32]{};
            std::snprintf(key, sizeof(key), "shotDelay%u", argument);
            context.numbered[argument] = read_optional_number(context.parameter_db, key);
            return context.numbered[argument].present;
        }
        case 1:
            context.overflow = read_optional_number(context.parameter_db, "shotDelay64");
            return context.overflow.present;
        case 2:
            context.save_point = read_optional_number(context.parameter_db, "saveFireCyclePoint");
            return context.save_point.present;
        case 3:
            context.reset_time = read_optional_number(context.parameter_db, "shotCycleResetTime");
            return context.reset_time.present;
        case 4: {
            char message[240]{};
            std::snprintf(message, sizeof(message),
                          "Ignoring %s firing cycle: at most %u numbered delays are supported",
                          context.class_label, static_cast<unsigned>(kMaximumShotDelays));
            log_line(message);
            return 0;
        }
        case 5:
            return static_cast<std::uint32_t>(a2fo::point_defense::build_policy(
                context.numbered.data(), context.numbered.size(),
                read_at<float>(context.weapon_class, kShotDelayOnWeaponClassOffset, -1.0f),
                context.save_point, context.reset_time, &context.policy));
        case 6: {
            char message[300]{};
            std::snprintf(message, sizeof(message),
                          "Ignoring %s firing cycle: %s; native shotDelay retained",
                          context.class_label,
                          status_description(static_cast<PolicyStatus>(argument)));
            log_line(message);
            return 0;
        }
        case 7:
            try { g_class_policies[context.weapon_class] = context.policy; return 1; }
            catch (...) { return 0; }
        case 8:
            log_line("Could not retain a point-defense firing-cycle policy");
            return 0;
        case 9: {
            char message[300]{};
            std::snprintf(message, sizeof(message),
                          "Registered %s firing cycle: %u delay%s, loop %u, idle reset %.3fs%s",
                          context.class_label,
                          static_cast<unsigned>(context.policy.delays.size()),
                          context.policy.delays.size() == 1 ? "" : "s",
                          static_cast<unsigned>(context.policy.save_fire_cycle_point),
                          context.policy.shot_cycle_reset_time,
                          context.policy.uses_numbered_delays
                              ? " (numbered delays override shotDelay)" : "");
            log_line(message);
            return 0;
        }
    }
    return 0;
}

void register_policy(void* weapon_class, void* parameter_db,
                     const char* class_label) noexcept {
    RegisterContext context{weapon_class, parameter_db, class_label};
    a2fo_rs_point_register(g_runtime_ready && weapon_class && parameter_db,
                           &context, &register_policy_op);
}

const CyclePolicy* policy_for_weapon(void* weapon) noexcept {
    if (!g_runtime_ready || !weapon) return nullptr;
    void* weapon_class = read_at<void*>(
        weapon, kWeaponClassOnWeaponOffset, nullptr);
    const auto found = g_class_policies.find(weapon_class);
    return found == g_class_policies.end() ? nullptr : &found->second;
}

CycleState* state_for_weapon(void* weapon,
                             const CyclePolicy& policy) noexcept {
    if (!weapon) return nullptr;
    try {
        CycleState& state = g_weapon_states[weapon];
        a2fo::point_defense::normalize_state(policy, &state);
        return &state;
    } catch (...) {
        return nullptr;
    }
}

struct SelectContext {
    void* weapon;
    const CyclePolicy* policy = nullptr;
    CycleState* state = nullptr;
};
std::uint64_t select_op(void* opaque, std::uint32_t operation,
                        std::uint32_t) noexcept {
    auto& context = *static_cast<SelectContext*>(opaque);
    switch (operation) {
        case 0:
            context.policy = policy_for_weapon(context.weapon);
            return context.policy != nullptr;
        case 1: {
            void* weapon_class = read_at<void*>(context.weapon,
                                                  kWeaponClassOnWeaponOffset, nullptr);
            return float_bits(read_at<float>(weapon_class,
                               kShotDelayOnWeaponClassOffset, 0.0f));
        }
        case 2:
            context.state = state_for_weapon(context.weapon, *context.policy);
            return context.state != nullptr;
        case 3: return float_bits(context.policy->delays.front());
        case 4: return float_bits(a2fo::point_defense::consume_delay(
            *context.policy, context.state));
    }
    return 0;
}
float select_delay(void* weapon) noexcept {
    SelectContext context{weapon};
    const std::uint32_t bits = a2fo_rs_point_select(&context, &select_op);
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

struct UpdateContext {
    void* weapon;
    float elapsed_seconds;
    float timer_before;
    std::uint64_t fire_count_before;
    const CyclePolicy* policy = nullptr;
    CycleState* state = nullptr;
    float timer_after = 0.0f;
};
std::uint64_t update_op(void* opaque, std::uint32_t operation,
                        std::uint32_t argument) noexcept {
    auto& context = *static_cast<UpdateContext*>(opaque);
    switch (operation) {
        case 0:
            context.policy = policy_for_weapon(context.weapon);
            return context.policy != nullptr;
        case 1:
            context.state = state_for_weapon(context.weapon, *context.policy);
            return context.state != nullptr;
        case 2: return context.state->fire_count == context.fire_count_before;
        case 3:
            context.timer_after = read_at<float>(context.weapon,
                kShotTimerOnWeaponOffset, context.timer_before);
            return std::isfinite(context.timer_before) &&
                std::isfinite(context.timer_after) &&
                context.timer_after < context.timer_before;
        case 4:
            a2fo::point_defense::update_idle_reset(*context.policy,
                context.state, argument != 0, context.timer_after,
                context.elapsed_seconds);
            return 0;
    }
    return 0;
}
void update_after_simulation(void* weapon, float elapsed_seconds,
                             float timer_before,
                             std::uint64_t fire_count_before) noexcept {
    UpdateContext context{weapon, elapsed_seconds, timer_before,
                          fire_count_before};
    a2fo_rs_point_update(&context, &update_op);
}

std::uintptr_t __attribute__((fastcall)) point_defense_build_class_hook(
    void* self, void*, void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_point_defense_call_thiscall_1(
        g_point_defense_build_class_hook.gateway, self,
        reinterpret_cast<std::uintptr_t>(parameter_db));
    register_policy(reinterpret_cast<void*>(result), parameter_db,
                    "PointDefenseLaser");
    return result;
}

std::uintptr_t __attribute__((fastcall)) ordnance_defense_build_class_hook(
    void* self, void*, void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_point_defense_call_thiscall_1(
        g_ordnance_defense_build_class_hook.gateway, self,
        reinterpret_cast<std::uintptr_t>(parameter_db));
    register_policy(reinterpret_cast<void*>(result), parameter_db,
                    "OrdnanceDefenseWeapon");
    return result;
}

struct SimulateContext {
    A2FO_InlineHook* hook;
    void* weapon;
    float elapsed_seconds;
    const CyclePolicy* policy = nullptr;
    float timer_before = 0.0f;
    std::uint64_t fire_count_before = 0;
};
std::uint64_t simulate_op(void* opaque, std::uint32_t operation,
                          std::uint32_t) noexcept {
    auto& context = *static_cast<SimulateContext*>(opaque);
    switch (operation) {
        case 0:
            context.policy = policy_for_weapon(context.weapon);
            return context.policy != nullptr;
        case 1:
            context.timer_before = read_at<float>(context.weapon,
                                                   kShotTimerOnWeaponOffset, 0.0f);
            if (context.policy) {
                CycleState* state = state_for_weapon(context.weapon, *context.policy);
                if (state) context.fire_count_before = state->fire_count;
            }
            return 0;
        case 2:
            return a2fo_point_defense_call_thiscall_1(
                context.hook->gateway, context.weapon, float_bits(context.elapsed_seconds));
        case 3:
            update_after_simulation(context.weapon, context.elapsed_seconds,
                                    context.timer_before, context.fire_count_before);
            return 0;
    }
    return 0;
}
std::uintptr_t simulate_with_cycle(A2FO_InlineHook& hook, void* weapon,
                                   float elapsed_seconds) noexcept {
    SimulateContext context{&hook, weapon, elapsed_seconds};
    return a2fo_rs_point_simulate(&context, &simulate_op);
}

std::uintptr_t __attribute__((fastcall)) point_defense_simulate_hook(
    void* weapon, void*, float elapsed_seconds) noexcept {
    return simulate_with_cycle(g_point_defense_simulate_hook, weapon,
                               elapsed_seconds);
}

std::uintptr_t __attribute__((fastcall)) ordnance_defense_simulate_hook(
    void* weapon, void*, float elapsed_seconds) noexcept {
    return simulate_with_cycle(g_ordnance_defense_simulate_hook, weapon,
                               elapsed_seconds);
}

struct ResetContext {
    void* weapon;
    void* weapon_class = nullptr;
    float native_delay = 0.0f;
};
std::uint64_t reset_op(void* opaque, std::uint32_t operation,
                       std::uint32_t) noexcept {
    auto& context = *static_cast<ResetContext*>(opaque);
    switch (operation) {
        case 0:
            context.weapon_class = read_at<void*>(context.weapon,
                                                   kWeaponClassOnWeaponOffset, nullptr);
            return policy_for_weapon(context.weapon) && context.weapon_class;
        case 1: {
            context.native_delay = read_at<float>(context.weapon_class,
                                                   kShotDelayOnWeaponClassOffset, 0.0f);
            const float selected_delay = select_delay(context.weapon);
            write_at(context.weapon_class, kShotDelayOnWeaponClassOffset,
                     selected_delay);
            return 0;
        }
        case 2:
            write_at(context.weapon_class, kShotDelayOnWeaponClassOffset,
                     context.native_delay);
            return 0;
        case 3:
            return a2fo_point_defense_call_thiscall_0(
                g_weapon_reset_shot_timer_hook.gateway, context.weapon);
    }
    return 0;
}
std::uintptr_t __attribute__((fastcall)) weapon_reset_shot_timer_hook(
    void* weapon, void*) noexcept {
    ResetContext context{weapon};
    return a2fo_rs_point_reset(&context, &reset_op);
}

using OutInteger = bool (__cdecl*)(void*, std::int32_t, const char*);
using OutFloat = bool (__cdecl*)(void*, float, const char*);
using InInteger = bool (__cdecl*)(void*, std::int32_t*);
using InFloat = bool (__cdecl*)(void*, float*);

bool save_cycle_state(void* writer, const CycleState& state) noexcept {
    const auto out_integer = reinterpret_cast<OutInteger>(
        at(g_armada, kFileWriterOutIntegerRva));
    const auto out_float = reinterpret_cast<OutFloat>(
        at(g_armada, kFileWriterOutFloatRva));
    return out_integer && out_float &&
        out_integer(writer,
                    static_cast<std::int32_t>(state.next_delay_index),
                    "a2fo_pointDefenseCycleIndex") &&
        out_float(writer, state.idle_reset_remaining,
                  "a2fo_pointDefenseCycleIdle");
}

bool load_cycle_state(void* reader, CycleState* state) noexcept {
    if (!state) return false;
    const auto in_integer = reinterpret_cast<InInteger>(
        at(g_armada, kFileReaderInIntegerRva));
    const auto in_float = reinterpret_cast<InFloat>(
        at(g_armada, kFileReaderInFloatRva));
    std::int32_t index = 0;
    float idle = 0.0f;
    const bool loaded = in_integer && in_float &&
        in_integer(reader, &index) && in_float(reader, &idle);
    if (!loaded) return false;
    state->next_delay_index = index < 0
        ? 0u : static_cast<std::size_t>(index);
    state->idle_reset_remaining = idle;
    state->fire_count = 0;
    return true;
}

struct SerialContext {
    void* weapon;
    void* stream;
    bool loading;
    const CyclePolicy* policy = nullptr;
    CycleState* state = nullptr;
};
std::uint64_t serial_op(void* opaque, std::uint32_t operation,
                        std::uint32_t) noexcept {
    auto& context = *static_cast<SerialContext*>(opaque);
    switch (operation) {
        case 0:
            return a2fo_point_defense_call_thiscall_1(
                context.loading ? g_weapon_load_hook.gateway : g_weapon_save_hook.gateway,
                context.weapon, reinterpret_cast<std::uintptr_t>(context.stream));
        case 1:
            context.policy = policy_for_weapon(context.weapon);
            return context.policy != nullptr;
        case 2:
            context.state = state_for_weapon(context.weapon, *context.policy);
            return context.state != nullptr;
        case 3:
            return context.loading ? load_cycle_state(context.stream, context.state)
                                   : save_cycle_state(context.stream, *context.state);
        case 4:
            *context.state = CycleState{};
            return 0;
        case 5:
            a2fo::point_defense::normalize_state(*context.policy, context.state);
            return 0;
    }
    return 0;
}
std::uintptr_t __attribute__((fastcall)) weapon_save_hook(
    void* weapon, void*, void* writer) noexcept {
    SerialContext context{weapon, writer, false};
    return a2fo_rs_point_save(&context, &serial_op);
}
std::uintptr_t __attribute__((fastcall)) weapon_load_hook(
    void* weapon, void*, void* reader) noexcept {
    SerialContext context{weapon, reader, true};
    return a2fo_rs_point_load(&context, &serial_op);
}

std::uintptr_t __attribute__((fastcall)) weapon_destructor_hook(
    void* weapon, void*) noexcept {
    if (weapon) g_weapon_states.erase(weapon);
    return a2fo_point_defense_call_thiscall_0(
        g_weapon_destructor_hook.gateway, weapon);
}

bool preflight_signatures() noexcept {
    return
        signature_matches(g_armada, kPointDefenseBuildClassRva,
                          kExpectedPointDefenseBuildClass) &&
        signature_matches(g_armada, kPointDefenseSimulateRva,
                          kExpectedPointDefenseSimulate) &&
        signature_matches(g_armada, kPointDefenseTimerGateRva,
                          kExpectedPointDefenseTimerGate) &&
        signature_matches(g_armada, kWeaponResetShotTimerRva,
                          kExpectedWeaponResetShotTimer) &&
        signature_matches(g_armada, kWeaponLoadRva,
                          kExpectedWeaponLoad) &&
        signature_matches(g_armada, kWeaponSaveRva,
                          kExpectedWeaponSave) &&
        signature_matches(g_armada, kWeaponDestructorRva,
                          kExpectedWeaponDestructor) &&
        signature_matches(g_armada, kParameterDbGetFloatRva,
                          kExpectedParameterDbGetFloat) &&
        signature_matches(g_armada, kParameterDbGetStringRva,
                          kExpectedParameterDbGetString) &&
        signature_matches(g_armada, kFileWriterOutFloatRva,
                          kExpectedFileWriterOutFloat) &&
        signature_matches(g_armada, kFileWriterOutIntegerRva,
                          kExpectedFileWriterOutInteger) &&
        signature_matches(g_armada, kFileReaderInFloatRva,
                          kExpectedFileReaderInFloat) &&
        signature_matches(g_armada, kFileReaderInIntegerRva,
                          kExpectedFileReaderInInteger) &&
        signature_matches(g_fleet_ops, kOrdnanceDefenseBuildClassRva,
                          kExpectedOrdnanceDefenseBuildClass) &&
        signature_matches(g_fleet_ops, kOrdnanceDefenseSimulateRva,
                          kExpectedOrdnanceDefenseSimulate) &&
        signature_matches(g_fleet_ops, kOrdnanceDefenseDelaySiteRva,
                          kExpectedOrdnanceDefenseDelaySite);
}

std::uint64_t point_init_op(void* opaque, std::uint32_t operation,
                            std::uint32_t index) noexcept {
    const auto* api = static_cast<const A2FO_ModuleApi*>(opaque);
    try {
    switch (operation) {
        case 0:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
            return g_armada && g_fleet_ops;
        case 1:
            g_runtime_ready = index != 0;
            return 0;
        case 2:
            return preflight_signatures();
        case 3:
            a2fo_point_defense_odw_continue = at(g_fleet_ops, kOrdnanceDefenseDelayContinueRva);
            a2fo_point_defense_pdl_timer_ready = at(g_armada, kPointDefenseTimerReadyRva);
            a2fo_point_defense_pdl_timer_blocked = at(g_armada, kPointDefenseTimerBlockedRva);
            return 0;
        case 4:
            switch (index) {
                case 0: return install_hook(g_armada, kPointDefenseBuildClassRva,
                    reinterpret_cast<void*>(&point_defense_build_class_hook),
                    kExpectedPointDefenseBuildClass, &g_point_defense_build_class_hook);
                case 1: return install_hook(g_fleet_ops, kOrdnanceDefenseBuildClassRva,
                    reinterpret_cast<void*>(&ordnance_defense_build_class_hook),
                    kExpectedOrdnanceDefenseBuildClass, &g_ordnance_defense_build_class_hook);
                case 2: return install_hook(g_armada, kPointDefenseSimulateRva,
                    reinterpret_cast<void*>(&point_defense_simulate_hook),
                    kExpectedPointDefenseSimulate, &g_point_defense_simulate_hook);
                case 3: return install_hook(g_fleet_ops, kOrdnanceDefenseSimulateRva,
                    reinterpret_cast<void*>(&ordnance_defense_simulate_hook),
                    kExpectedOrdnanceDefenseSimulate, &g_ordnance_defense_simulate_hook);
                case 4: return install_hook(g_armada, kWeaponResetShotTimerRva,
                    reinterpret_cast<void*>(&weapon_reset_shot_timer_hook),
                    kExpectedWeaponResetShotTimer, &g_weapon_reset_shot_timer_hook);
                case 5: return install_hook(g_armada, kWeaponLoadRva,
                    reinterpret_cast<void*>(&weapon_load_hook),
                    kExpectedWeaponLoad, &g_weapon_load_hook);
                case 6: return install_hook(g_armada, kWeaponSaveRva,
                    reinterpret_cast<void*>(&weapon_save_hook),
                    kExpectedWeaponSave, &g_weapon_save_hook);
                case 7: return install_hook(g_armada, kWeaponDestructorRva,
                    reinterpret_cast<void*>(&weapon_destructor_hook),
                    kExpectedWeaponDestructor, &g_weapon_destructor_hook);
                case 8: return api->patch_jump(at(g_armada, kPointDefenseTimerGateRva),
                    reinterpret_cast<void*>(&a2fo_point_defense_pdl_timer_gate_bridge),
                    kExpectedPointDefenseTimerGate, sizeof(kExpectedPointDefenseTimerGate));
                case 9: return api->patch_jump(at(g_fleet_ops, kOrdnanceDefenseDelaySiteRva),
                    reinterpret_cast<void*>(&a2fo_point_defense_odw_delay_bridge),
                    kExpectedOrdnanceDefenseDelaySite, sizeof(kExpectedOrdnanceDefenseDelaySite));
            }
            return 0;
        case 5: {
            char message[240]{};
            std::snprintf(message, sizeof(message),
                          "%s initialized: point-defense shotDelay timing and firing cycles",
                          kBuildId);
            log_line(message);
            return 0;
        }
        case 6:
            log_line("Supported point-defense signatures were not found; runtime disabled");
            return 0;
    }
    return 0;
    } catch (...) {
        return 0;
    }
}

}  // namespace

extern "C" std::uint32_t __cdecl
a2fo_point_defense_select_odw_delay(void* weapon) {
    return float_bits(select_delay(weapon));
}

extern "C" std::uint32_t __cdecl
a2fo_point_defense_advance_pdl_timer(void* weapon, float elapsed_seconds) {
    if (!g_logged_point_defense_gate) {
        g_logged_point_defense_gate = true;
        log_line("PointDefenseLaser pre-fire timer gate reached");
    }
    float timer = read_at<float>(weapon, kShotTimerOnWeaponOffset, 0.0f);
    if (std::isfinite(timer)) {
        if (timer < 0.0f) timer = 0.0f;
        if (std::isfinite(elapsed_seconds) && elapsed_seconds > 0.0f) {
            timer -= elapsed_seconds;
        }
    }
    write_at(weapon, kShotTimerOnWeaponOffset, timer);
    return a2fo_rs_point_timer(g_runtime_ready, timer);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook || !api->patch_jump) {
        return false;
    }
    return a2fo_rs_point_init(true, const_cast<A2FO_ModuleApi*>(api),
                              &point_init_op) != 0;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Checked inline hooks are process-lifetime. The loader keeps built-in
    // modules resident, so shutdown deliberately leaves their backing state
    // available to any final engine cleanup calls.
}
