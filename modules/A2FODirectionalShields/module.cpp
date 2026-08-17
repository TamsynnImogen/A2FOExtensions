/*
 * Optional four-facing Craft shields.
 *
 * A2FOWeaponDamageControls remains the single Craft::Damage hook owner and
 * calls the bounded BeginDamage/EndDamage exports below. Only classes with
 * directionalShields = 1 receive sidecar state or altered damage routing.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "api.hpp"
#include "directional_shields.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

extern "C" std::uintptr_t __cdecl
a2fo_directional_shields_call_thiscall_0(void* function, void* self);

namespace {

using a2fo::directional_shields::Facing;
using a2fo::directional_shields::Matrix34;
using a2fo::directional_shields::ShieldPolicy;
using a2fo::directional_shields::ShieldStores;
using CreateShieldHit = std::int32_t (__cdecl *)(
    void* object, const void* shield_matrix, std::int32_t shield_type,
    float duration, std::int32_t flags);
using UpdateShieldEffect = void (__cdecl *)(
    std::int32_t effect_id, const void* shield_matrix);
using StopShieldEffect = void (__cdecl *)(std::int32_t effect_id);

constexpr char kModuleName[] = "A2FODirectionalShields";
constexpr char kWeaponDamageControlsModuleName[] =
    "A2FOWeaponDamageControls.dll";
constexpr char kRefreshDamageBridgeExport[] =
    "A2FOWeaponDamageControls_RefreshDirectionalShieldsBridge";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs and object fields.
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;
constexpr std::uintptr_t kCreateShieldHitRva = 0x000743b0;
constexpr std::uintptr_t kStopShieldEffectRva = 0x00074770;
constexpr std::uintptr_t kUpdateShieldEffectRva = 0x000747d0;
constexpr std::uintptr_t kShieldEffectMapHeadRva = 0x00336dec;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kCurrentShieldsOffset = 0x1c8;
constexpr std::size_t kMaximumShieldsOffset = 0x1cc;
constexpr std::size_t kNativeShieldEffectIdOffset = 0x208;
constexpr std::size_t kMaximumShieldsOnCraftClassOffset = 0x208;
constexpr std::size_t kShieldEffectNodeKeyOffset = 0x0c;
constexpr std::size_t kShieldEffectNodeValueOffset = 0x14;
constexpr std::size_t kShieldEffectStrengthOffset = 0xc0;
constexpr std::size_t kWeaponOnOrdnanceOffset = 0x38;
constexpr std::size_t kDamageInfoOnOrdnanceOffset = 0x3c;
constexpr std::size_t kDamageInfoSize = 0x20;

constexpr std::array<std::uint8_t, 7> kExpectedEntityGetTransform{{
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3}};
constexpr std::array<std::uint8_t, 5> kExpectedWeaponGetOwner{{
    0x8b, 0x49, 0x18, 0x51, 0xe8}};
constexpr std::array<std::uint8_t, 10> kExpectedCreateShieldHit{{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0x87, 0xb5, 0x69, 0x00}};
constexpr std::array<std::uint8_t, 9> kExpectedShieldEffectLookup{{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8d, 0x45, 0x08}};

constexpr std::array<const char*, 6> kCraftFields{{
    "directionalShields",
    "forwardShieldStrength",
    "aftShieldStrength",
    "portShieldStrength",
    "starboardShieldStrength",
    "maxShields",
}};

struct CraftState {
    ShieldStores stores{};
    Facing pending_effect_facing = Facing::forward;
    bool pending_effect = false;
};

struct ImpactEffectState {
    void* craft = nullptr;
    Facing facing = Facing::forward;
};

struct ActiveDamageEffectScope {
    A2FO_DirectionalShieldDamageScope* scope = nullptr;
    void* craft = nullptr;
    Facing facing = Facing::forward;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
bool g_runtime_ready = false;
bool g_damage_bridge_connected = false;
A2FO_InlineHook g_create_shield_hit_hook{};
A2FO_InlineHook g_stop_shield_effect_hook{};
A2FO_InlineHook g_update_shield_effect_hook{};
std::unordered_map<void*, ShieldPolicy> g_class_policies;
std::unordered_map<void*, CraftState> g_craft_states;
std::unordered_map<std::int32_t, ImpactEffectState> g_impact_effects;
std::array<ActiveDamageEffectScope, 8> g_active_damage_effect_scopes{};
std::size_t g_active_damage_effect_scope_count = 0;

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void* at(std::uintptr_t rva) noexcept {
    return g_armada
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(g_armada) + rva)
        : nullptr;
}

void push_active_damage_effect_scope(
    A2FO_DirectionalShieldDamageScope* scope, void* craft,
    Facing facing) noexcept {
    if (!scope || !craft || g_active_damage_effect_scope_count >=
            g_active_damage_effect_scopes.size()) {
        return;
    }
    g_active_damage_effect_scopes[g_active_damage_effect_scope_count++] =
        {scope, craft, facing};
}

void remove_active_damage_effect_scope(
    A2FO_DirectionalShieldDamageScope* scope) noexcept {
    if (!scope) return;
    for (std::size_t index = g_active_damage_effect_scope_count;
         index > 0; --index) {
        const std::size_t candidate = index - 1;
        if (g_active_damage_effect_scopes[candidate].scope != scope) continue;
        for (std::size_t move = candidate + 1;
             move < g_active_damage_effect_scope_count; ++move) {
            g_active_damage_effect_scopes[move - 1] =
                g_active_damage_effect_scopes[move];
        }
        --g_active_damage_effect_scope_count;
        g_active_damage_effect_scopes[g_active_damage_effect_scope_count] = {};
        return;
    }
}

bool active_damage_effect_facing(void* craft, Facing* facing) noexcept {
    if (!craft || !facing) return false;
    for (std::size_t index = g_active_damage_effect_scope_count;
         index > 0; --index) {
        const ActiveDamageEffectScope& active =
            g_active_damage_effect_scopes[index - 1];
        if (active.craft == craft) {
            *facing = active.facing;
            return true;
        }
    }
    return false;
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
T read_at(const void* object, std::size_t offset,
          T fallback = T{}) noexcept {
    const auto* address = object
        ? static_cast<const std::uint8_t*>(object) + offset : nullptr;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) noexcept {
    const void* address = at(rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected.data(), Size) == 0;
}

bool write_current_shields(void* craft, float value) noexcept {
    auto* address = craft
        ? static_cast<std::uint8_t*>(craft) + kCurrentShieldsOffset
        : nullptr;
    if (!readable_range(address, sizeof(value)) || !std::isfinite(value)) {
        return false;
    }
    std::memcpy(address, &value, sizeof(value));
    return true;
}

void* find_shield_effect(std::int32_t effect_id) noexcept {
    // Armada stores active ShieldEffect pointers in an MSVC red-black tree.
    // The head node's parent is the root; keys and values are at +0x0c/+0x14.
    void* head = read_at<void*>(at(kShieldEffectMapHeadRva), 0, nullptr);
    if (!head || !readable_range(head, 0x18)) return nullptr;
    void* node = read_at<void*>(head, 0x04, head);
    for (unsigned depth = 0; node && node != head && depth < 128; ++depth) {
        if (!readable_range(node, 0x18)) return nullptr;
        const std::int32_t key = read_at<std::int32_t>(
            node, kShieldEffectNodeKeyOffset, -1);
        if (effect_id == key) {
            return read_at<void*>(
                node, kShieldEffectNodeValueOffset, nullptr);
        }
        node = read_at<void*>(node, effect_id < key ? 0x00 : 0x08, head);
    }
    return nullptr;
}

float effect_facing_strength(const ShieldPolicy& policy,
                             const CraftState& state,
                             Facing facing) noexcept {
    const std::size_t index =
        a2fo::directional_shields::facing_index(facing);
    const float maximum = policy.maximum[index];
    if (!std::isfinite(maximum) || maximum <= 0.0f) return 0.0f;
    const float current = std::isfinite(state.stores.current[index])
        ? state.stores.current[index] : 0.0f;
    return std::max(0.0f, std::min(current / maximum, 1.0f));
}

bool write_effect_facing_strength(std::int32_t effect_id,
                                  const ShieldPolicy& policy,
                                  const CraftState& state,
                                  Facing facing) noexcept {
    void* effect = find_shield_effect(effect_id);
    auto* address = effect
        ? static_cast<std::uint8_t*>(effect) + kShieldEffectStrengthOffset
        : nullptr;
    if (!readable_range(address, sizeof(float))) return false;
    const float strength = effect_facing_strength(policy, state, facing);
    std::memcpy(address, &strength, sizeof(strength));
    return true;
}

void clear_native_collapse_effect(void* craft,
                                  float facing_remaining) noexcept {
    auto* effect_address = craft
        ? static_cast<std::uint8_t*>(craft) +
              kNativeShieldEffectIdOffset
        : nullptr;
    if (!readable_range(effect_address, sizeof(std::int32_t))) return;
    std::int32_t effect_id = -1;
    std::memcpy(&effect_id, effect_address, sizeof(effect_id));
    if (!a2fo::directional_shields::should_clear_native_collapse_effect(
            facing_remaining, effect_id)) {
        return;
    }
    reinterpret_cast<StopShieldEffect>(at(kStopShieldEffectRva))(effect_id);
    constexpr std::int32_t kNoEffect = -1;
    std::memcpy(effect_address, &kNoEffect, sizeof(kNoEffect));
}

bool write_class_maximum_shields(void* object_class, float value) noexcept {
    auto* address = object_class
        ? static_cast<std::uint8_t*>(object_class) +
              kMaximumShieldsOnCraftClassOffset
        : nullptr;
    if (!readable_range(address, sizeof(value)) || !std::isfinite(value) ||
        value <= 0.0f) {
        return false;
    }
    std::memcpy(address, &value, sizeof(value));
    return true;
}

bool request_damage_bridge_connection() noexcept {
    HMODULE module = GetModuleHandleA(kWeaponDamageControlsModuleName);
    FARPROC exported = module
        ? GetProcAddress(module, kRefreshDamageBridgeExport) : nullptr;
    using RefreshFn = bool (A2FO_CALL*)();
    RefreshFn refresh = nullptr;
    static_assert(sizeof(refresh) == sizeof(exported),
                  "unexpected function-pointer size");
    std::memcpy(&refresh, &exported, sizeof(refresh));
    return refresh && refresh();
}

bool field_value(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, std::string* value) {
    if (!fields || !name || !value) return false;
    const std::size_t name_size = std::strlen(name);
    for (std::uint32_t index = 0; index < count; ++index) {
        const A2FO_OdfFieldView& field = fields[index];
        if (!field.name.data || field.name.size != name_size ||
            _strnicmp(field.name.data, name, name_size) != 0 ||
            (!field.value.data && field.value.size != 0)) {
            continue;
        }
        value->assign(field.value.data ? field.value.data : "",
                      field.value.size);
        return true;
    }
    return false;
}

bool parse_float(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, float* value) noexcept {
    if (!value) return false;
    try {
        std::string text;
        if (!field_value(fields, count, name, &text)) return false;
        char* end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || !std::isfinite(parsed)) return false;
        while (*end == ' ' || *end == '\t' || *end == '\r' ||
               *end == '\n') ++end;
        if (*end == 'f' || *end == 'F') ++end;
        while (*end == ' ' || *end == '\t' || *end == '\r' ||
               *end == '\n') ++end;
        if (*end != '\0' || parsed < 0.0f) return false;
        *value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_bool(const A2FO_OdfFieldView* fields, std::uint32_t count,
                const char* name, bool* value) noexcept {
    if (!value) return false;
    float numeric = 0.0f;
    if (parse_float(fields, count, name, &numeric)) {
        *value = numeric != 0.0f;
        return true;
    }
    try {
        std::string text;
        if (!field_value(fields, count, name, &text)) return false;
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        if (text == "true" || text == "yes" || text == "on") {
            *value = true;
            return true;
        }
        if (text == "false" || text == "no" || text == "off") {
            *value = false;
            return true;
        }
    } catch (...) {
    }
    return false;
}

std::string source_name(const A2FO_GameObjectClassLoadedEvent* event) {
    if (!event || !event->source_odf.data) return "<unknown ODF>";
    return std::string(event->source_odf.data, event->source_odf.size);
}

void A2FO_CALL craft_class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!g_runtime_ready || !event ||
        event->struct_size < sizeof(*event) || !event->object_class) {
        return;
    }
    bool enabled = false;
    if (!parse_bool(event->odf_fields, event->odf_field_count,
                    "directionalShields", &enabled) || !enabled) {
        return;
    }

    ShieldPolicy policy{};
    const bool complete =
        parse_float(event->odf_fields, event->odf_field_count,
                    "forwardShieldStrength", &policy.maximum[0]) &&
        parse_float(event->odf_fields, event->odf_field_count,
                    "aftShieldStrength", &policy.maximum[1]) &&
        parse_float(event->odf_fields, event->odf_field_count,
                    "portShieldStrength", &policy.maximum[2]) &&
        parse_float(event->odf_fields, event->odf_field_count,
                    "starboardShieldStrength", &policy.maximum[3]);
    policy = a2fo::directional_shields::normalize_policy(policy);
    if (!complete || !a2fo::directional_shields::valid_policy(policy)) {
        const std::string message =
            "Ignored directionalShields on " + source_name(event) +
            "; all four facing strengths must be positive";
        log_line(message.c_str());
        return;
    }

    const float total = a2fo::directional_shields::total_capacity(policy);
    std::string native_maximum_text;
    const bool native_maximum_declared = field_value(
        event->odf_fields, event->odf_field_count,
        "maxShields", &native_maximum_text);
    float native_maximum = 0.0f;
    if (native_maximum_declared &&
        (!parse_float(event->odf_fields, event->odf_field_count,
                      "maxShields", &native_maximum) ||
         native_maximum <= 0.0f)) {
        const std::string message =
            "Ignored directionalShields on " + source_name(event) +
            "; explicit maxShields must be positive";
        log_line(message.c_str());
        return;
    }
    const float comparison_scale = std::max(total, native_maximum);
    const float comparison_tolerance =
        std::max(0.01f, comparison_scale * 0.0001f);
    if (native_maximum_declared &&
        std::fabs(native_maximum - total) > comparison_tolerance) {
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "Ignored directionalShields on %s; facing total %.3g does not match explicit maxShields %.3g",
            source_name(event).c_str(), static_cast<double>(total),
            static_cast<double>(native_maximum));
        log_line(message);
        return;
    }

    // A2 parses maxShields into CraftClass+0x208. A1-era ODFs may omit that
    // command, so only that compatibility case needs the directional total
    // installed as the native shield ceiling. maxHealth/healthRate are hull
    // fields and are deliberately left untouched in both cases.
    if (!native_maximum_declared &&
        !write_class_maximum_shields(event->object_class, total)) {
        const std::string message =
            "Could not install the directional shield maximum for " +
            source_name(event) + "; native shield behaviour retained";
        log_line(message.c_str());
        return;
    }

    try {
        g_class_policies[event->object_class] = policy;
    } catch (...) {
        log_line("Could not retain a directional-shield class policy");
        return;
    }

    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "Registered directional shields for %s: forward %.3g, aft %.3g, port %.3g, starboard %.3g (total %.3g; native maximum %s)",
        source_name(event).c_str(),
        static_cast<double>(policy.maximum[0]),
        static_cast<double>(policy.maximum[1]),
        static_cast<double>(policy.maximum[2]),
        static_cast<double>(policy.maximum[3]),
        static_cast<double>(total),
        native_maximum_declared ? "from maxShields" : "derived for A1 ODF");
    log_line(message);
}

const ShieldPolicy* policy_for_craft(const void* craft) noexcept {
    const void* object_class = read_at<const void*>(
        craft, kObjectClassOffset, nullptr);
    const auto found = g_class_policies.find(const_cast<void*>(object_class));
    return found == g_class_policies.end() ? nullptr : &found->second;
}

CraftState* state_for_craft(void* craft, bool create) noexcept {
    if (!craft) return nullptr;
    const auto existing = g_craft_states.find(craft);
    if (existing != g_craft_states.end()) return &existing->second;
    if (!create) return nullptr;
    const ShieldPolicy* policy = policy_for_craft(craft);
    if (!policy) return nullptr;

    CraftState state{};
    const float native_total = read_at<float>(
        craft, kCurrentShieldsOffset, 0.0f);
    a2fo::directional_shields::initialize_from_total(
        &state.stores, *policy, native_total);
    try {
        const auto inserted = g_craft_states.emplace(craft, state);
        if (!inserted.second) return nullptr;
        write_current_shields(
            craft,
            a2fo::directional_shields::total_current(
                inserted.first->second.stores));
        return &inserted.first->second;
    } catch (...) {
        return nullptr;
    }
}

void reconcile_craft(void* craft, CraftState* state,
                     const ShieldPolicy& policy) noexcept {
    if (!craft || !state) return;
    const float native_total = read_at<float>(
        craft, kCurrentShieldsOffset, 0.0f);
    a2fo::directional_shields::reconcile_total(
        &state->stores, policy, native_total);
    write_current_shields(
        craft,
        a2fo::directional_shields::total_current(state->stores));
}

bool resolve_effect_facing(void* craft, const void* shield_matrix,
                           Facing* facing) noexcept {
    if (!craft || !facing ||
        !readable_range(shield_matrix, sizeof(Matrix34))) {
        return false;
    }
    Matrix34 hit_copy{};
    std::memcpy(&hit_copy, shield_matrix, sizeof(hit_copy));
    return a2fo::directional_shields::select_local_effect_facing(
        hit_copy, facing);
}

void forget_impact_effects_for_craft(void* craft) noexcept {
    for (auto effect = g_impact_effects.begin();
         effect != g_impact_effects.end();) {
        if (effect->second.craft == craft) {
            effect = g_impact_effects.erase(effect);
        } else {
            ++effect;
        }
    }
    for (std::size_t index = g_active_damage_effect_scope_count;
         index > 0; --index) {
        if (g_active_damage_effect_scopes[index - 1].craft == craft) {
            remove_active_damage_effect_scope(
                g_active_damage_effect_scopes[index - 1].scope);
        }
    }
}

std::size_t clear_tracked_impact_effects(void* craft, Facing facing,
                                         float facing_remaining) noexcept {
    if (!a2fo::directional_shields::should_suppress_native_impact_effect(
            0, true, facing_remaining)) {
        return 0;
    }
    const auto stop = reinterpret_cast<StopShieldEffect>(
        g_stop_shield_effect_hook.gateway);
    std::size_t stopped = 0;
    for (auto effect = g_impact_effects.begin();
         effect != g_impact_effects.end();) {
        if (effect->second.craft == craft &&
            effect->second.facing == facing) {
            if (stop) stop(effect->first);
            effect = g_impact_effects.erase(effect);
            ++stopped;
        } else {
            ++effect;
        }
    }
    return stopped;
}

template <typename Callback>
auto with_effect_facing_value(
    void* craft, const ShieldPolicy& policy, const CraftState& state,
    Facing facing, Callback&& callback) noexcept -> decltype(callback()) {
    const float native_before = read_at<float>(
        craft, kCurrentShieldsOffset, 0.0f);
    float presented =
        a2fo::directional_shields::effect_aggregate_for_facing(
            policy, state.stores, facing);
    // ShieldEffect divides the temporary current value by Craft+0x1cc. Scale
    // the face ratio to that live ceiling so A2 ODFs whose maxShields differs
    // from the four configured arc maxima still receive an exact 0..1 colour
    // value.
    const float configured_maximum =
        a2fo::directional_shields::total_capacity(policy);
    const float native_maximum = read_at<float>(
        craft, kMaximumShieldsOffset, configured_maximum);
    if (configured_maximum > 0.0f && std::isfinite(native_maximum) &&
        native_maximum > 0.0f) {
        presented *= native_maximum / configured_maximum;
    }
    const bool replaced = write_current_shields(craft, presented);
    const auto result = callback();
    if (replaced) write_current_shields(craft, native_before);
    return result;
}

bool resolve_hit_facing(void* craft, const void* source_damage_info,
                        Facing* facing) noexcept {
    if (!craft || !source_damage_info || !facing ||
        !readable_range(source_damage_info, kDamageInfoSize)) {
        return false;
    }
    const auto damage_address =
        reinterpret_cast<std::uintptr_t>(source_damage_info);
    if (damage_address < kDamageInfoOnOrdnanceOffset) return false;
    const void* ordnance = reinterpret_cast<const void*>(
        damage_address - kDamageInfoOnOrdnanceOffset);
    void* weapon = read_at<void*>(
        ordnance, kWeaponOnOrdnanceOffset, nullptr);
    if (!weapon || !readable_range(weapon, 0x1c)) return false;
    void* attacker = reinterpret_cast<void*>(
        a2fo_directional_shields_call_thiscall_0(
            at(kWeaponGetOwnerRva), weapon));
    if (!attacker) return false;

    const auto* target_transform = reinterpret_cast<const Matrix34*>(
        a2fo_directional_shields_call_thiscall_0(
            at(kEntityGetTransformRva), craft));
    const auto* attacker_transform = reinterpret_cast<const Matrix34*>(
        a2fo_directional_shields_call_thiscall_0(
            at(kEntityGetTransformRva), attacker));
    if (!readable_range(target_transform, sizeof(Matrix34)) ||
        !readable_range(attacker_transform, sizeof(Matrix34))) {
        return false;
    }
    Matrix34 target_copy{};
    Matrix34 attacker_copy{};
    std::memcpy(&target_copy, target_transform, sizeof(target_copy));
    std::memcpy(&attacker_copy, attacker_transform, sizeof(attacker_copy));
    return a2fo::directional_shields::select_facing(
        target_copy, &attacker_copy.values[9], facing);
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) || !event->craft) {
        return;
    }
    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP) {
        forget_impact_effects_for_craft(event->craft);
        g_craft_states.erase(event->craft);
        return;
    }
    if (!g_runtime_ready || !g_damage_bridge_connected) return;
    const ShieldPolicy* policy = policy_for_craft(event->craft);
    if (!policy) return;
    if (event->kind == A2FO_CRAFT_EVENT_POST_LOAD) {
        g_craft_states.erase(event->craft);
        state_for_craft(event->craft, true);
        return;
    }
    if (event->kind != A2FO_CRAFT_EVENT_SIMULATE_PRE &&
        event->kind != A2FO_CRAFT_EVENT_SIMULATE_POST) {
        return;
    }
    CraftState* state = state_for_craft(event->craft, true);
    reconcile_craft(event->craft, state, *policy);
}

bool begin_damage(void* craft, const void* source_damage_info,
                  A2FO_DirectionalShieldDamageScope* scope) noexcept {
    if (!scope || scope->struct_size < sizeof(*scope)) return false;
    scope->active = 0;
    scope->craft = nullptr;
    scope->facing = 0;
    if (!g_runtime_ready || !g_damage_bridge_connected || !craft) {
        return false;
    }
    const ShieldPolicy* policy = policy_for_craft(craft);
    if (!policy) return false;
    Facing facing = Facing::forward;
    if (!resolve_hit_facing(craft, source_damage_info, &facing)) {
        return false;
    }
    CraftState* state = state_for_craft(craft, true);
    if (!state) return false;
    reconcile_craft(craft, state, *policy);
    // A normal type-0 hit flare is created just after Craft::Damage returns.
    // Discard any unconsumed result from an earlier damage call before
    // recording this hit's facing in EndDamage.
    state->pending_effect = false;
    const std::size_t index = a2fo::directional_shields::facing_index(facing);
    if (!write_current_shields(craft, state->stores.current[index])) {
        return false;
    }
    scope->active = 1;
    scope->craft = craft;
    scope->facing = static_cast<std::uint32_t>(facing);
    push_active_damage_effect_scope(scope, craft, facing);
    return true;
}

void end_damage(A2FO_DirectionalShieldDamageScope* scope) noexcept {
    if (!scope || scope->struct_size < sizeof(*scope) || !scope->active ||
        !scope->craft || scope->facing >=
            a2fo::directional_shields::kFacingCount) {
        return;
    }
    const ShieldPolicy* policy = policy_for_craft(scope->craft);
    CraftState* state = state_for_craft(scope->craft, false);
    if (policy && state) {
        const std::size_t index = scope->facing;
        float remaining = read_at<float>(
            scope->craft, kCurrentShieldsOffset,
            state->stores.current[index]);
        if (!std::isfinite(remaining)) remaining = 0.0f;
        state->stores.current[index] = std::max(
            0.0f, std::min(remaining, policy->maximum[index]));
        // The ordinary type-0 hit effect is emitted by the caller after the
        // shared Craft::Damage hook has returned, whereas the type-1 collapse
        // effect is emitted inside Craft::Damage. Preserve the completed hit
        // facing until that immediately following type-0 effect consumes it.
        state->pending_effect_facing =
            static_cast<Facing>(scope->facing);
        state->pending_effect = true;
        // Craft::Damage creates its type-1 collapse effect whenever the
        // temporary native value reaches zero. With directional shields that
        // temporary value is only the struck facing, so remove the native
        // effect immediately while that facing is empty. Other facings and
        // A2FOAlwaysShowShields' separately owned effect remain untouched.
        clear_native_collapse_effect(
            scope->craft, state->stores.current[index]);
        clear_tracked_impact_effects(
            scope->craft, static_cast<Facing>(scope->facing),
            state->stores.current[index]);
        write_current_shields(
            scope->craft,
            a2fo::directional_shields::total_current(state->stores));
    }
    remove_active_damage_effect_scope(scope);
    scope->active = 0;
}

float shield_value(void* craft, std::uint32_t facing,
                   bool maximum) noexcept {
    if (!g_runtime_ready || !g_damage_bridge_connected || !craft ||
        facing >= a2fo::directional_shields::kFacingCount) {
        return 0.0f;
    }
    const ShieldPolicy* policy = policy_for_craft(craft);
    if (!policy) return 0.0f;
    if (maximum) return policy->maximum[facing];
    CraftState* state = state_for_craft(craft, true);
    return state ? state->stores.current[facing] : 0.0f;
}

std::int32_t __cdecl create_shield_hit_hook(
    void* craft, const void* shield_matrix, std::int32_t shield_type,
    float duration, std::int32_t flags) noexcept {
    // This callback can run from inside Craft::Damage while the shared damage
    // bridge has temporarily exposed only the struck facing through the
    // native aggregate field. Never reconcile sidecar gameplay state here:
    // doing so would mistake that temporary value for a real aggregate change
    // and drain every other facing when one arc reaches zero.
    const bool visual_runtime =
        g_runtime_ready && g_damage_bridge_connected && craft;
    const ShieldPolicy* policy = visual_runtime
        ? policy_for_craft(craft) : nullptr;
    CraftState* state = policy ? state_for_craft(craft, false) : nullptr;
    Facing facing = Facing::forward;
    bool resolved = false;
    if (policy && active_damage_effect_facing(craft, &facing)) {
        resolved = true;
    } else if (shield_type == 0 && state && state->pending_effect) {
        facing = state->pending_effect_facing;
        state->pending_effect = false;
        resolved = true;
    } else if (policy && resolve_effect_facing(
                   craft, shield_matrix, &facing)) {
        resolved = true;
    }
    if (shield_type == 0 && policy && state) {
            const float remaining = resolved
                ? state->stores.current[
                      a2fo::directional_shields::facing_index(facing)]
                : 0.0f;
            if (a2fo::directional_shields::
                    should_suppress_native_impact_effect(
                        shield_type, resolved, remaining)) {
                return -1;
            }
            if (resolved) {
                const auto original = reinterpret_cast<CreateShieldHit>(
                    g_create_shield_hit_hook.gateway);
                const std::int32_t effect_id = original
                    ? with_effect_facing_value(
                          craft, *policy, *state, facing,
                          [&]() noexcept {
                              return original(
                                  craft, shield_matrix, shield_type,
                                  duration, flags);
                          })
                    : -1;
                if (effect_id >= 0) {
                    write_effect_facing_strength(
                        effect_id, *policy, *state, facing);
                    try {
                        g_impact_effects[effect_id] = {craft, facing};
                    } catch (...) {
                    }
                }
                return effect_id;
            }
    }
    const auto original = reinterpret_cast<CreateShieldHit>(
        g_create_shield_hit_hook.gateway);
    return original
        ? original(craft, shield_matrix, shield_type, duration, flags)
        : -1;
}

void __cdecl stop_shield_effect_hook(std::int32_t effect_id) noexcept {
    g_impact_effects.erase(effect_id);
    const auto original = reinterpret_cast<StopShieldEffect>(
        g_stop_shield_effect_hook.gateway);
    if (original) original(effect_id);
}

void __cdecl update_shield_effect_hook(
    std::int32_t effect_id, const void* shield_matrix) noexcept {
    const auto original = reinterpret_cast<UpdateShieldEffect>(
        g_update_shield_effect_hook.gateway);
    const auto tracked = g_impact_effects.find(effect_id);
    if (!original || tracked == g_impact_effects.end() ||
        !g_runtime_ready || !g_damage_bridge_connected) {
        if (original) original(effect_id, shield_matrix);
        return;
    }
    void* craft = tracked->second.craft;
    Facing facing = tracked->second.facing;
    Facing updated_facing = facing;
    if (resolve_effect_facing(craft, shield_matrix, &updated_facing)) {
        facing = updated_facing;
        tracked->second.facing = updated_facing;
    }
    const ShieldPolicy* policy = policy_for_craft(craft);
    CraftState* state = policy ? state_for_craft(craft, false) : nullptr;
    if (!policy || !state) {
        g_impact_effects.erase(tracked);
        original(effect_id, shield_matrix);
        return;
    }
    const float remaining = state->stores.current[
        a2fo::directional_shields::facing_index(facing)];
    if (a2fo::directional_shields::should_suppress_native_impact_effect(
            0, true, remaining)) {
        const auto stop = reinterpret_cast<StopShieldEffect>(
            g_stop_shield_effect_hook.gateway);
        if (stop) stop(effect_id);
        g_impact_effects.erase(tracked);
        return;
    }
    original(effect_id, shield_matrix);
    write_effect_facing_strength(effect_id, *policy, *state, facing);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook ||
        !A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) ||
        !api->register_game_object_class_loaded_handler ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler) ||
        !api->register_craft_event_handler ||
        (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada ||
        !signature_matches(kEntityGetTransformRva,
                           kExpectedEntityGetTransform) ||
        !signature_matches(kWeaponGetOwnerRva, kExpectedWeaponGetOwner) ||
        !signature_matches(kCreateShieldHitRva,
                           kExpectedCreateShieldHit) ||
        !signature_matches(kStopShieldEffectRva,
                           kExpectedShieldEffectLookup) ||
        !signature_matches(kUpdateShieldEffectRva,
                           kExpectedShieldEffectLookup)) {
        return false;
    }
    if (!api->install_inline_hook(
            at(kCreateShieldHitRva),
            reinterpret_cast<void*>(&create_shield_hit_hook),
            kExpectedCreateShieldHit.size(),
            kExpectedCreateShieldHit.data(),
            &g_create_shield_hit_hook) ||
        !g_create_shield_hit_hook.gateway) {
        return false;
    }
    if (!api->install_inline_hook(
            at(kStopShieldEffectRva),
            reinterpret_cast<void*>(&stop_shield_effect_hook),
            kExpectedShieldEffectLookup.size(),
            kExpectedShieldEffectLookup.data(),
            &g_stop_shield_effect_hook) ||
        !g_stop_shield_effect_hook.gateway) {
        return true;
    }
    if (!api->install_inline_hook(
            at(kUpdateShieldEffectRva),
            reinterpret_cast<void*>(&update_shield_effect_hook),
            kExpectedShieldEffectLookup.size(),
            kExpectedShieldEffectLookup.data(),
            &g_update_shield_effect_hook) ||
        !g_update_shield_effect_hook.gateway) {
        return true;
    }
    g_runtime_ready = true;
    const bool registered =
        api->register_game_object_class_loaded_handler(
            kModuleName, kCraftFields.data(),
            static_cast<std::uint32_t>(kCraftFields.size()),
            &craft_class_loaded_handler, nullptr) &&
        api->register_craft_event_handler(
            kModuleName, &craft_event_handler, nullptr);
    if (!registered) {
        g_runtime_ready = false;
        log_line("Could not register directional-shield dispatchers; installed effect hooks remain pass-through");
        return true;
    }
    log_line("Optional four-facing shield policies registered");
    if (!request_damage_bridge_connection()) {
        log_line("Awaiting the shared Craft::Damage bridge");
    }
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Shared dispatchers and the Craft::Damage hook are process-lifetime.
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_ConnectDamageBridge() {
    if (!g_runtime_ready) return false;
    if (g_damage_bridge_connected) return true;
    g_damage_bridge_connected = true;
    log_line("Four-facing damage routing connected to Craft::Damage");
    return true;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_BeginDamage(
    void* craft, const void* source_damage_info,
    A2FO_DirectionalShieldDamageScope* scope) {
    return begin_damage(craft, source_damage_info, scope);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FODirectionalShields_EndDamage(
    A2FO_DirectionalShieldDamageScope* scope) {
    end_damage(scope);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_IsEnabled(void* craft) {
    return g_runtime_ready && g_damage_bridge_connected &&
        policy_for_craft(craft) != nullptr;
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FODirectionalShields_GetCurrent(
    void* craft, std::uint32_t facing) {
    return shield_value(craft, facing, false);
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FODirectionalShields_GetMaximum(
    void* craft, std::uint32_t facing) {
    return shield_value(craft, facing, true);
}
