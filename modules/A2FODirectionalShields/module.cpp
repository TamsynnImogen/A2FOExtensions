#include "../../rust/stage10_ui_completion_ffi.h"
/*
 * Optional directional Craft shields.
 *
 * A2FOWeaponDamageControls remains the single Craft::Damage hook owner and
 * calls the bounded BeginDamage/EndDamage exports below. Only classes with
 * directionalShields modes 1+ receive sidecar state or altered damage routing.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "api.hpp"
#include "directional_shields.hpp"
#include "../../sdk/include/a2fo_shield_values.hpp"
#include "../../rust/stage8_a_directional_ffi.h"
#include "../../rust/stage9_directional_parse_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <unordered_map>

extern "C" std::uintptr_t __cdecl
a2fo_directional_shields_call_thiscall_0(void* function, void* self);

namespace {

using a2fo::directional_shields::Facing;
using a2fo::directional_shields::Matrix34;
using a2fo::directional_shields::Mode;
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

constexpr std::array<const char*, 4> kShieldStrengthFields{{
    "forwardShieldStrength",
    "aftShieldStrength",
    "portShieldStrength",
    "starboardShieldStrength",
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
std::unordered_map<void*, ShieldPolicy> g_craft_policy_overrides;
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
    struct Context {
        A2FO_DirectionalShieldDamageScope* scope;
        void* craft;
        Facing facing;
    } context{scope, craft, facing};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        const auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_active_damage_effect_scope_count;
        case 1: return g_active_damage_effect_scopes.size();
        case 2:
            g_active_damage_effect_scopes[
                g_active_damage_effect_scope_count++] =
                    {value.scope, value.craft, value.facing};
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_directional_push_scope(
        reinterpret_cast<std::uintptr_t>(scope),
        reinterpret_cast<std::uintptr_t>(craft), step, &context);
}

void remove_active_damage_effect_scope(
    A2FO_DirectionalShieldDamageScope* scope) noexcept {
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        switch (operation) {
        case 0: return g_active_damage_effect_scope_count;
        case 1: return reinterpret_cast<std::uintptr_t>(
            g_active_damage_effect_scopes[argument].scope);
        case 2:
            g_active_damage_effect_scopes[argument - 1] =
                g_active_damage_effect_scopes[argument];
            return 0;
        case 3:
            --g_active_damage_effect_scope_count;
            g_active_damage_effect_scopes[
                g_active_damage_effect_scope_count] = {};
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_directional_remove_scope(
        reinterpret_cast<std::uintptr_t>(scope), step, nullptr);
}

bool active_damage_effect_facing(void* craft, Facing* facing) noexcept {
    struct Context {Facing* facing;} context{facing};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_active_damage_effect_scope_count;
        case 1: return reinterpret_cast<std::uintptr_t>(
            g_active_damage_effect_scopes[argument].craft);
        case 2:
            *value.facing = g_active_damage_effect_scopes[argument].facing;
            return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_active_facing(
        reinterpret_cast<std::uintptr_t>(craft),
        reinterpret_cast<std::uintptr_t>(facing),
        step, &context) != 0;
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
    struct Context {void* head;void* node;};
    Context context{head,read_at<void*>(head,0x04,head)};
    const auto step=+[](void* opaque,std::uint32_t operation,std::uintptr_t direction) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(operation){
        case 0:return c.node && c.node!=c.head && readable_range(c.node,0x18);
        case 1:return static_cast<std::uint32_t>(read_at<std::int32_t>(c.node,kShieldEffectNodeKeyOffset,-1));
        case 2:return reinterpret_cast<std::uintptr_t>(read_at<void*>(c.node,kShieldEffectNodeValueOffset,nullptr));
        case 3:c.node=read_at<void*>(c.node,direction==0?0x00:0x08,c.head);return 0;
        default:return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_stage10_ui_effect_tree(&context,step,effect_id));
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
    // New cores own the damage bridge. Keep compatibility with older cores
    // that still load the standalone provider, without selecting a stale DLL
    // when a present core bridge is deliberately unprepared/rejected.
    HMODULE module = GetModuleHandleA("A2FOExtensions.dll");
    FARPROC exported = module
        ? GetProcAddress(module, kRefreshDamageBridgeExport) : nullptr;
    if (!exported) {
        module = GetModuleHandleA(kWeaponDamageControlsModuleName);
        exported = module ? GetProcAddress(module, kRefreshDamageBridgeExport) : nullptr;
    }
    using RefreshFn = bool (A2FO_CALL*)();
    RefreshFn refresh = nullptr;
    static_assert(sizeof(refresh) == sizeof(exported),
                  "unexpected function-pointer size");
    std::memcpy(&refresh, &exported, sizeof(refresh));
    return refresh && refresh();
}

bool field_value(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, std::string* value) {
    struct Context {
        const A2FO_OdfFieldView* fields;
        const char* name;
        std::string* value;
        const A2FO_OdfFieldView* field{};
        std::size_t name_size{};
        std::exception_ptr failure{};
    } context{fields, name, value};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uint32_t index, std::uint64_t* result) noexcept
            -> std::uint8_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            switch (operation) {
                case 0: *result = state.fields != nullptr; return 1;
                case 1: *result = state.name != nullptr; return 1;
                case 2: *result = state.value != nullptr; return 1;
                case 3:
                    state.name_size = std::strlen(state.name);
                    *result = state.name_size;
                    return 1;
                case 4:
                    state.field = &state.fields[index];
                    *result = 0;
                    return 1;
                case 5: *result = state.field->name.data != nullptr; return 1;
                case 6: *result = state.field->name.size == state.name_size; return 1;
                case 7:
                    *result = _strnicmp(state.field->name.data, state.name,
                                        state.name_size) == 0;
                    return 1;
                case 8:
                    *result = state.field->value.data != nullptr ||
                              state.field->value.size == 0;
                    return 1;
                case 9:
                    state.value->assign(state.field->value.data
                                            ? state.field->value.data : "",
                                        state.field->value.size);
                    *result = 1;
                    return 1;
                default: *result = 0; return 1;
            }
        } catch (...) {
            state.failure = std::current_exception();
            return 0;
        }
    };
    const bool result = a2fo_rs_stage9_directional_field_value(
        &context, step, count);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
}

bool parse_float(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, float* value) noexcept {
    if (!value) return false;
    try {
        std::string text;
        if (!field_value(fields, count, name, &text)) return false;
        char* end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        std::uint32_t parsed_bits = 0;
        static_assert(sizeof(parsed_bits) == sizeof(parsed),
                      "unexpected float width");
        std::memcpy(&parsed_bits, &parsed, sizeof(parsed_bits));
        const std::size_t used = static_cast<std::size_t>(end - text.c_str());
        if (!a2fo_rs_stage9_directional_float_suffix(
                reinterpret_cast<const std::uint8_t*>(end),
                text.size() - used + 1, end != text.c_str(), parsed_bits)) {
            return false;
        }
        *value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

std::string source_name(const A2FO_GameObjectClassLoadedEvent* event) {
    if (!event || !event->source_odf.data) return "<unknown ODF>";
    return std::string(event->source_odf.data, event->source_odf.size);
}

void A2FO_CALL craft_class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    struct Context {
        const A2FO_GameObjectClassLoadedEvent* event;
        bool retained = false;
    } context{event};
    const auto admit = [](void* opaque, std::uint32_t operation) noexcept
            -> std::uintptr_t {
        const auto* value = static_cast<Context*>(opaque)->event;
        switch (operation) {
        case 0: return g_runtime_ready;
        case 1: return value->struct_size;
        case 2: return reinterpret_cast<std::uintptr_t>(value->object_class);
        case 3: return value->odf_fields ? value->odf_field_count : 0;
        default: return 0;
        }
    };
    const auto field = [](void* opaque, std::uint32_t index,
                          A2foStage8DirectionalBytes* name,
                          A2foStage8DirectionalBytes* value) noexcept -> std::uint8_t {
        const auto* event = static_cast<Context*>(opaque)->event;
        if (!event->odf_fields || index >= event->odf_field_count) return 0;
        const auto& source = event->odf_fields[index];
        if (!source.name.data || (!source.value.data && source.value.size != 0)) {
            return 0;
        }
        *name = {reinterpret_cast<const std::uint8_t*>(source.name.data),
                 source.name.size};
        *value = {reinterpret_cast<const std::uint8_t*>(source.value.data),
                  source.value.size};
        return 1;
    };
    const auto equal = [](void*, A2foStage8DirectionalBytes left,
                          A2foStage8DirectionalBytes right) noexcept -> std::uint8_t {
        return _strnicmp(reinterpret_cast<const char*>(left.data),
                         reinterpret_cast<const char*>(right.data), left.size) == 0;
    };
    const auto lower = [](void*, std::uint8_t value) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(std::tolower(value));
    };
    const auto parse = [](void* opaque, std::uint32_t field_index,
                          float* value) noexcept -> std::uint8_t {
        const auto* event = static_cast<Context*>(opaque)->event;
        return field_index < kCraftFields.size() && parse_float(
            event->odf_fields, event->odf_field_count,
            kCraftFields[field_index], value);
    };
    const auto numeric = [](void*, std::uint32_t operation,
            A2foStage8DirectionalClassResult* value) noexcept -> std::uint8_t {
        switch (operation) {
        case 0: value->explicit_total += value->current_value; return 0;
        case 1: {
            const float comparison_scale = std::max(
                value->native_maximum, value->explicit_total);
            const float comparison_tolerance = std::max(
                0.01f, comparison_scale * 0.0001f);
            const float remaining = value->native_maximum - value->explicit_total;
            if (!std::isfinite(remaining) || remaining <= comparison_tolerance) {
                return 1;
            }
            value->share = remaining / static_cast<float>(value->automatic_count);
            return !std::isfinite(value->share) || value->share <= 0.0f ? 2 : 0;
        }
        case 2: {
            const float comparison_scale = std::max(
                value->total, value->native_maximum);
            const float comparison_tolerance = std::max(
                0.01f, comparison_scale * 0.0001f);
            return std::fabs(value->native_maximum - value->total) >
                comparison_tolerance;
        }
        case 3: return std::floor(value->current_value) == value->current_value;
        default: return 0;
        }
    };
    const auto action = [](void* opaque, std::uint32_t operation,
            const A2foStage8DirectionalClassResult* value) noexcept -> std::uint8_t {
        auto& state = *static_cast<Context*>(opaque);
        if (operation == 0) {
            return write_class_maximum_shields(
                state.event->object_class, value->total);
        }
        if (operation != 1) return 0;
        ShieldPolicy policy{};
        policy.mode = static_cast<Mode>(value->mode);
        std::copy_n(value->maximum, 4, policy.maximum.begin());
        try {
            g_class_policies[state.event->object_class] = policy;
            state.retained = true;
            return 1;
        } catch (...) {
            return 0;
        }
    };
    A2foStage8DirectionalClassResult result{};
    const std::uint32_t outcome = a2fo_rs_stage8_directional_class(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event),
        admit, field, equal, lower, parse, numeric, action, &context, &result);
    switch (outcome) {
    case 0: return;
    case 1: {
        const std::string message =
            "Ignored directionalShields on " + source_name(event) +
            "; expected mode 0..5";
        log_line(message.c_str());
        return;
    }
    case 2: {
        char message[320]{};
        std::snprintf(message, sizeof(message),
            "Ignored directionalShields mode %u on %s; this build currently supports 1 (quad), 2 (tri forward), 3 (tri rear), 4 (forward/rear hemispheres), and 5 (port/starboard hemispheres)",
            result.mode, source_name(event).c_str());
        log_line(message);
        return;
    }
    case 3: {
        const std::string message =
            "Ignored directionalShields on " + source_name(event) +
            "; invalid " + kShieldStrengthFields[result.offending_index];
        log_line(message.c_str());
        return;
    }
    case 4: {
        const std::string message =
            "Ignored directionalShields on " + source_name(event) +
            "; explicit maxShields must be positive";
        log_line(message.c_str());
        return;
    }
    case 5: {
        const std::string message =
            "Ignored directionalShields on " + source_name(event) +
            "; maxShields is required when an active facing strength is omitted or zero";
        log_line(message.c_str());
        return;
    }
    case 6: {
        char message[384]{};
        std::snprintf(message, sizeof(message),
            "Ignored directionalShields on %s; explicit facing total %.3g leaves no positive capacity for %lu automatic facing%s within maxShields %.3g",
            source_name(event).c_str(), static_cast<double>(result.explicit_total),
            static_cast<unsigned long>(result.automatic_count),
            result.automatic_count == 1 ? "" : "s",
            static_cast<double>(result.native_maximum));
        log_line(message);
        return;
    }
    case 7: {
        const std::string message =
            "Ignored directionalShields on " + source_name(event) +
            "; every active facing must have positive capacity";
        log_line(message.c_str());
        return;
    }
    case 8: {
        char message[384]{};
        std::snprintf(message, sizeof(message),
            "Ignored directionalShields on %s; facing total %.3g does not match explicit maxShields %.3g",
            source_name(event).c_str(), static_cast<double>(result.total),
            static_cast<double>(result.native_maximum));
        log_line(message);
        return;
    }
    case 9: {
        const std::string message =
            "Could not install the directional shield maximum for " +
            source_name(event) + "; native shield behaviour retained";
        log_line(message.c_str());
        return;
    }
    case 10:
        log_line("Could not retain a directional-shield class policy");
        return;
    case 11: {
        char message[384]{};
        std::snprintf(message, sizeof(message),
            "Registered directional shields mode %u for %s: forward %.3g, aft %.3g, port %.3g, starboard %.3g (total %.3g; native maximum %s)",
            result.mode, source_name(event).c_str(),
            static_cast<double>(result.maximum[0]),
            static_cast<double>(result.maximum[1]),
            static_cast<double>(result.maximum[2]),
            static_cast<double>(result.maximum[3]),
            static_cast<double>(result.total),
            result.native_maximum_declared ? "from maxShields" : "derived for A1 ODF");
        log_line(message);
        return;
    }
    default: return;
    }
}

const ShieldPolicy* policy_for_craft(const void* craft) noexcept {
    const void* object_class = read_at<const void*>(
        craft, kObjectClassOffset, nullptr);
    const auto found = g_class_policies.find(const_cast<void*>(object_class));
    if (found == g_class_policies.end()) return nullptr;
    const auto override = g_craft_policy_overrides.find(const_cast<void*>(craft));
    return override == g_craft_policy_overrides.end()
        ? &found->second : &override->second;
}

CraftState* state_for_craft(void* craft, bool create) noexcept {
    struct Context {
        void* craft;
        const ShieldPolicy* policy = nullptr;
        CraftState state{};
        CraftState* inserted = nullptr;
    } context{craft};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: {
            const auto found = g_craft_states.find(value.craft);
            return found == g_craft_states.end() ? 0 :
                reinterpret_cast<std::uintptr_t>(&found->second);
        }
        case 1:
            value.policy = policy_for_craft(value.craft);
            return reinterpret_cast<std::uintptr_t>(value.policy);
        case 2: {
            const float native_total = read_at<float>(
                value.craft, kCurrentShieldsOffset, 0.0f);
            a2fo::directional_shields::initialize_from_total(
                &value.state.stores, *value.policy, native_total);
            return 0;
        }
        case 3:
            try {
                const auto result = g_craft_states.emplace(
                    value.craft, value.state);
                value.inserted = result.second ? &result.first->second : nullptr;
                return reinterpret_cast<std::uintptr_t>(value.inserted);
            } catch (...) {
                return 0;
            }
        case 4:
            write_current_shields(value.craft,
                a2fo::directional_shields::total_current(
                    value.inserted->stores));
            return 0;
        default: return 0;
        }
    };
    return reinterpret_cast<CraftState*>(
        a2fo_rs_stage8_directional_state_for_craft(
            reinterpret_cast<std::uintptr_t>(craft), create ? 1 : 0,
            step, &context));
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
                           Mode mode, Facing* facing) noexcept {
    if (!craft || !facing ||
        !readable_range(shield_matrix, sizeof(Matrix34))) {
        return false;
    }
    Matrix34 hit_copy{};
    std::memcpy(&hit_copy, shield_matrix, sizeof(hit_copy));
    return a2fo::directional_shields::select_local_effect_facing(
        mode, hit_copy, facing);
}

void forget_impact_effects_for_craft(void* craft) noexcept {
    struct Context {
        void* craft;
        decltype(g_impact_effects)::iterator effect{};
    } context{craft};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: value.effect = g_impact_effects.begin(); return 0;
        case 1: return value.effect != g_impact_effects.end();
        case 2: return value.effect->second.craft == value.craft;
        case 3: value.effect = g_impact_effects.erase(value.effect); return 0;
        case 4: ++value.effect; return 0;
        case 5: return g_active_damage_effect_scope_count;
        case 6: return g_active_damage_effect_scopes[argument].craft == value.craft;
        case 7:
            remove_active_damage_effect_scope(
                g_active_damage_effect_scopes[argument].scope);
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_directional_forget_effects(step, &context);
}

std::size_t clear_tracked_impact_effects(void* craft, Facing facing,
                                         float facing_remaining) noexcept {
    struct Context {
        void* craft;
        Facing facing;
        StopShieldEffect stop = nullptr;
        decltype(g_impact_effects)::iterator effect{};
    } context{craft, facing};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0:
            value.stop = reinterpret_cast<StopShieldEffect>(
                g_stop_shield_effect_hook.gateway);
            value.effect = g_impact_effects.begin();
            return 0;
        case 1: return value.effect != g_impact_effects.end();
        case 2: return value.effect->second.craft == value.craft &&
                       value.effect->second.facing == value.facing;
        case 3:
            if (value.stop) value.stop(value.effect->first);
            value.effect = g_impact_effects.erase(value.effect);
            return 0;
        case 4: ++value.effect; return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_clear_tracked(
        facing_remaining, step, &context);
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
                        Mode mode, Facing* facing) noexcept {
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
        mode, target_copy, &attacker_copy.values[9], facing);
}

bool set_editor_state(void* craft, const A2FO_DirectionalShieldState* values) noexcept;

bool restore_editor_shields(void* craft) noexcept {
    struct Context {
        void* craft;
        A2FO_CraftIdentityTakeLoadedShieldsFn take = nullptr;
        A2FO_DirectionalShieldState values{};
    } context{craft};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: {
            HMODULE identity = a2fo::startup_module_provider(
                "A2FOCraftIdentity.dll",
                {"A2FOCraftIdentity_TakeLoadedDirectionalShields"});
            FARPROC exported = identity ? GetProcAddress(
                identity, "A2FOCraftIdentity_TakeLoadedDirectionalShields") : nullptr;
            static_assert(sizeof(value.take) == sizeof(exported),
                          "function pointer size");
            std::memcpy(&value.take, &exported, sizeof(value.take));
            value.values.struct_size = sizeof(value.values);
            return value.take != nullptr;
        }
        case 1: return value.take(value.craft, &value.values);
        case 2: return set_editor_state(value.craft, &value.values);
        case 3:
            log_line("Could not restore saved per-object directional shields");
            return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_restore(
        reinterpret_cast<std::uintptr_t>(craft), step, &context) != 0;
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    struct Context {
        const A2FO_CraftEvent* event;
        const ShieldPolicy* policy = nullptr;
    } context{event};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        void* craft = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: return state.event->struct_size;
        case 1: return reinterpret_cast<std::uintptr_t>(state.event->craft);
        case 2: return state.event->kind;
        case 3: forget_impact_effects_for_craft(craft); return 0;
        case 4: g_craft_states.erase(craft); return 0;
        case 5: g_craft_policy_overrides.erase(craft); return 0;
        case 6: return g_runtime_ready;
        case 7: return g_damage_bridge_connected;
        case 8: return !g_class_policies.empty();
        case 9: return restore_editor_shields(craft);
        case 10: state_for_craft(craft, true); return 0;
        case 11:
            state.policy = policy_for_craft(craft);
            return state.policy != nullptr;
        case 12: return reinterpret_cast<std::uintptr_t>(
            state_for_craft(craft, true));
        case 13:
            reconcile_craft(state.event->craft,
                reinterpret_cast<CraftState*>(argument), *state.policy);
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_directional_craft_event(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event),
        A2FO_CRAFT_EVENT_CLEANUP, A2FO_CRAFT_EVENT_POST_LOAD,
        A2FO_CRAFT_EVENT_SIMULATE_PRE, A2FO_CRAFT_EVENT_SIMULATE_POST,
        step, &context);
}

bool begin_damage(void* craft, const void* source_damage_info,
                  A2FO_DirectionalShieldDamageScope* scope) noexcept {
    struct Context {
        void* craft;
        const void* source_damage_info;
        A2FO_DirectionalShieldDamageScope* scope;
        const ShieldPolicy* policy = nullptr;
        CraftState* state = nullptr;
        Facing facing = Facing::forward;
    } context{craft, source_damage_info, scope};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return value.scope->struct_size;
        case 1:
            value.scope->active = 0;
            value.scope->craft = nullptr;
            value.scope->facing = 0;
            return 0;
        case 2: return g_runtime_ready;
        case 3: return g_damage_bridge_connected;
        case 4:
            value.policy = policy_for_craft(value.craft);
            return reinterpret_cast<std::uintptr_t>(value.policy);
        case 5: return resolve_hit_facing(
            value.craft, value.source_damage_info,
            value.policy->mode, &value.facing);
        case 6:
            value.state = state_for_craft(value.craft, true);
            return reinterpret_cast<std::uintptr_t>(value.state);
        case 7: reconcile_craft(value.craft, value.state, *value.policy); return 0;
        case 8: value.state->pending_effect = false; return 0;
        case 9: return write_current_shields(
            value.craft, value.state->stores.current[
                a2fo::directional_shields::facing_index(value.facing)]);
        case 10:
            value.scope->active = 1;
            value.scope->craft = value.craft;
            value.scope->facing = static_cast<std::uint32_t>(value.facing);
            return 0;
        case 11:
            push_active_damage_effect_scope(
                value.scope, value.craft, value.facing);
            return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_begin_damage(
        reinterpret_cast<std::uintptr_t>(craft),
        reinterpret_cast<std::uintptr_t>(scope), sizeof(*scope),
        step, &context) != 0;
}

void end_damage(A2FO_DirectionalShieldDamageScope* scope) noexcept {
    struct Context {
        A2FO_DirectionalShieldDamageScope* scope;
        const ShieldPolicy* policy = nullptr;
        CraftState* state = nullptr;
    } context{scope};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        auto* scope = value.scope;
        switch (operation) {
        case 0: return scope->struct_size;
        case 1: return scope->active;
        case 2: return reinterpret_cast<std::uintptr_t>(scope->craft);
        case 3: return scope->facing;
        case 4:
            value.policy = policy_for_craft(scope->craft);
            return reinterpret_cast<std::uintptr_t>(value.policy);
        case 5:
            value.state = state_for_craft(scope->craft, false);
            return reinterpret_cast<std::uintptr_t>(value.state);
        case 6: {
            const std::size_t index = scope->facing;
            float remaining = read_at<float>(
                scope->craft, kCurrentShieldsOffset,
                value.state->stores.current[index]);
            if (!std::isfinite(remaining)) remaining = 0.0f;
            value.state->stores.current[index] = std::max(
                0.0f, std::min(remaining, value.policy->maximum[index]));
            return 0;
        }
        case 7:
            value.state->pending_effect_facing =
                static_cast<Facing>(scope->facing);
            value.state->pending_effect = true;
            return 0;
        case 8:
            clear_native_collapse_effect(
                scope->craft, value.state->stores.current[scope->facing]);
            return 0;
        case 9:
            clear_tracked_impact_effects(
                scope->craft, static_cast<Facing>(scope->facing),
                value.state->stores.current[scope->facing]);
            return 0;
        case 10:
            write_current_shields(scope->craft,
                a2fo::directional_shields::total_current(value.state->stores));
            return 0;
        case 11: remove_active_damage_effect_scope(scope); return 0;
        case 12: scope->active = 0; return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_directional_end_damage(
        reinterpret_cast<std::uintptr_t>(scope), sizeof(*scope),
        step, &context);
}

bool set_editor_state(void* craft, const A2FO_DirectionalShieldState* values) noexcept {
    struct Context {
        void* craft;
        const A2FO_DirectionalShieldState* values;
        const ShieldPolicy* existing_policy = nullptr;
        ShieldPolicy policy{};
        CraftState* state = nullptr;
    } context{craft, values};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0:
            value.existing_policy = policy_for_craft(value.craft);
            return reinterpret_cast<std::uintptr_t>(value.existing_policy);
        case 1: return g_runtime_ready;
        case 2: return g_damage_bridge_connected;
        case 3: return value.values->struct_size;
        case 4: return g_active_damage_effect_scope_count;
        case 5: return a2fo::valid_shield_values(
            value.values->current, value.values->maximum);
        case 6: return readable_range(
            static_cast<std::uint8_t*>(value.craft) + kCurrentShieldsOffset,
            sizeof(float) * 2);
        case 7:
            value.policy.mode = value.existing_policy->mode;
            std::copy_n(value.values->maximum, 4, value.policy.maximum.begin());
            value.policy = a2fo::directional_shields::normalize_policy(value.policy);
            return a2fo::directional_shields::valid_policy(value.policy);
        case 8: return static_cast<std::uint32_t>(value.policy.mode);
        case 9: {
            const float current = value.values->current[argument];
            std::uint32_t bits = 0;
            std::memcpy(&bits, &current, sizeof(bits));
            return bits;
        }
        case 10:
            value.state = state_for_craft(value.craft, true);
            return reinterpret_cast<std::uintptr_t>(value.state);
        case 11:
            try {
                g_craft_policy_overrides.insert_or_assign(
                    value.craft, value.policy);
                return 1;
            } catch (...) {
                return 0;
            }
        case 12:
            std::copy_n(value.values->current, 4,
                        value.state->stores.current.begin());
            value.state->pending_effect = false;
            return 0;
        case 13: {
            const float maximum =
                a2fo::directional_shields::total_capacity(value.policy);
            const float current =
                a2fo::directional_shields::total_current(value.state->stores);
            std::memcpy(static_cast<std::uint8_t*>(value.craft) +
                kMaximumShieldsOffset, &maximum, sizeof(maximum));
            write_current_shields(value.craft, current);
            return 0;
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_set_state(
        reinterpret_cast<std::uintptr_t>(craft),
        reinterpret_cast<std::uintptr_t>(values), sizeof(*values),
        step, &context) != 0;
}

std::uint32_t query_value(void* craft, std::uint32_t facing,
                          std::uint32_t kind) noexcept {
    struct Context {
        void* craft;
        const ShieldPolicy* policy = nullptr;
        CraftState* state = nullptr;
    } context{craft};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_runtime_ready;
        case 1: return g_damage_bridge_connected;
        case 2:
            value.policy = policy_for_craft(value.craft);
            return reinterpret_cast<std::uintptr_t>(value.policy);
        case 3: return static_cast<std::uint32_t>(value.policy->mode);
        case 4: {
            const float maximum = value.policy->maximum[argument];
            std::uint32_t bits = 0;
            std::memcpy(&bits, &maximum, sizeof(bits));
            return bits;
        }
        case 5:
            value.state = state_for_craft(value.craft, true);
            return reinterpret_cast<std::uintptr_t>(value.state);
        case 6: {
            const float current = value.state->stores.current[argument];
            std::uint32_t bits = 0;
            std::memcpy(&bits, &current, sizeof(bits));
            return bits;
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_query(
        reinterpret_cast<std::uintptr_t>(craft), facing, kind,
        step, &context);
}

float shield_value(void* craft, std::uint32_t facing,
                   bool maximum) noexcept {
    const std::uint32_t bits = query_value(craft, facing, maximum ? 3u : 2u);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::int32_t __cdecl create_shield_hit_hook(
    void* craft, const void* shield_matrix, std::int32_t shield_type,
    float duration, std::int32_t flags) noexcept {
    struct Context {
        void* craft;
        const void* shield_matrix;
        std::int32_t shield_type;
        float duration;
        std::int32_t flags;
        const ShieldPolicy* policy = nullptr;
        CraftState* state = nullptr;
        Facing facing = Facing::forward;
    } context{craft, shield_matrix, shield_type, duration, flags};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_runtime_ready && g_damage_bridge_connected && value.craft;
        case 1:
            value.policy = policy_for_craft(value.craft);
            return reinterpret_cast<std::uintptr_t>(value.policy);
        case 2:
            value.state = state_for_craft(value.craft, false);
            return reinterpret_cast<std::uintptr_t>(value.state);
        case 3: return active_damage_effect_facing(value.craft, &value.facing);
        case 4: return static_cast<std::uint32_t>(value.facing);
        case 5: return value.state->pending_effect;
        case 6:
            value.facing = value.state->pending_effect_facing;
            value.state->pending_effect = false;
            return static_cast<std::uint32_t>(value.facing);
        case 7: return resolve_effect_facing(
            value.craft, value.shield_matrix, value.policy->mode, &value.facing);
        case 8: {
            const float remaining = value.state->stores.current[
                a2fo::directional_shields::facing_index(
                    static_cast<Facing>(argument))];
            std::uint32_t bits = 0;
            std::memcpy(&bits, &remaining, sizeof(bits));
            return bits;
        }
        case 9: {
            const auto original = reinterpret_cast<CreateShieldHit>(
                g_create_shield_hit_hook.gateway);
            const std::int32_t effect_id = original
                ? with_effect_facing_value(
                    value.craft, *value.policy, *value.state, value.facing,
                    [&]() noexcept {
                        return original(value.craft, value.shield_matrix,
                                        value.shield_type, value.duration, value.flags);
                    })
                : -1;
            return static_cast<std::uint32_t>(effect_id);
        }
        case 10: {
            const auto effect_id = static_cast<std::int32_t>(argument);
            write_effect_facing_strength(
                effect_id, *value.policy, *value.state, value.facing);
            try {
                g_impact_effects[effect_id] = {value.craft, value.facing};
            } catch (...) {
            }
            return 0;
        }
        case 11: {
            const auto original = reinterpret_cast<CreateShieldHit>(
                g_create_shield_hit_hook.gateway);
            const std::int32_t effect_id = original
                ? original(value.craft, value.shield_matrix,
                           value.shield_type, value.duration, value.flags)
                : -1;
            return static_cast<std::uint32_t>(effect_id);
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_create_hit(shield_type, step, &context);
}

void __cdecl stop_shield_effect_hook(std::int32_t effect_id) noexcept {
    const auto step = [](void* opaque, std::uint32_t operation) noexcept
            -> std::uintptr_t {
        const auto id = *static_cast<std::int32_t*>(opaque);
        switch (operation) {
        case 0: g_impact_effects.erase(id); return 0;
        case 1: return g_stop_shield_effect_hook.gateway != nullptr;
        case 2:
            reinterpret_cast<StopShieldEffect>(
                g_stop_shield_effect_hook.gateway)(id);
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_directional_stop_effect(step, &effect_id);
}

void __cdecl update_shield_effect_hook(
    std::int32_t effect_id, const void* shield_matrix) noexcept {
    struct Context {
        std::int32_t effect_id;
        const void* shield_matrix;
        UpdateShieldEffect original = nullptr;
        decltype(g_impact_effects)::iterator tracked{};
        void* craft = nullptr;
        Facing facing = Facing::forward;
        const ShieldPolicy* policy = nullptr;
        CraftState* state = nullptr;
    } context{effect_id, shield_matrix};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0:
            value.original = reinterpret_cast<UpdateShieldEffect>(
                g_update_shield_effect_hook.gateway);
            return value.original != nullptr;
        case 1:
            value.tracked = g_impact_effects.find(value.effect_id);
            return value.tracked != g_impact_effects.end();
        case 2: return g_runtime_ready;
        case 3: return g_damage_bridge_connected;
        case 4:
            value.craft = value.tracked->second.craft;
            return reinterpret_cast<std::uintptr_t>(value.craft);
        case 5:
            value.facing = value.tracked->second.facing;
            return static_cast<std::uint32_t>(value.facing);
        case 6:
            value.policy = policy_for_craft(value.craft);
            return reinterpret_cast<std::uintptr_t>(value.policy);
        case 7: {
            Facing updated_facing = value.facing;
            if (resolve_effect_facing(value.craft, value.shield_matrix,
                                      value.policy->mode, &updated_facing)) {
                value.facing = updated_facing;
                value.tracked->second.facing = updated_facing;
            }
            return 0;
        }
        case 8:
            value.state = state_for_craft(value.craft, false);
            return reinterpret_cast<std::uintptr_t>(value.state);
        case 9: {
            const float remaining = value.state->stores.current[
                a2fo::directional_shields::facing_index(value.facing)];
            std::uint32_t bits = 0;
            std::memcpy(&bits, &remaining, sizeof(bits));
            return bits;
        }
        case 10: {
            const auto stop = reinterpret_cast<StopShieldEffect>(
                g_stop_shield_effect_hook.gateway);
            if (stop) stop(value.effect_id);
            return 0;
        }
        case 11: g_impact_effects.erase(value.tracked); return 0;
        case 12: value.original(value.effect_id, value.shield_matrix); return 0;
        case 13:
            write_effect_facing_strength(
                value.effect_id, *value.policy, *value.state, value.facing);
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_directional_update_effect(step, &context);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    struct Context {
        const A2FO_ModuleApi* api;
        std::exception_ptr error;
    } context{api, nullptr};
    const auto step = [](void* opaque, std::uint32_t operation) noexcept
            -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        const auto* api = state.api;
        if (state.error) return UINTPTR_MAX;
        try {
            switch (operation) {
            case 0:
                return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                    api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                    api->armada_module && api->install_inline_hook &&
                    A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) &&
                    api->register_game_object_class_loaded_handler &&
                    A2FO_MODULE_API_HAS(api, register_craft_event_handler) &&
                    api->register_craft_event_handler &&
                    (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) != 0 &&
                    (api->capabilities & A2FO_CAP_CRAFT_EVENTS) != 0;
            case 1:
                g_api = api;
                g_armada = static_cast<HMODULE>(api->armada_module());
                return g_armada != nullptr;
            case 2: return signature_matches(kEntityGetTransformRva,
                                              kExpectedEntityGetTransform);
            case 3: return signature_matches(kWeaponGetOwnerRva,
                                              kExpectedWeaponGetOwner);
            case 4: return signature_matches(kCreateShieldHitRva,
                                              kExpectedCreateShieldHit);
            case 5: return signature_matches(kStopShieldEffectRva,
                                              kExpectedShieldEffectLookup);
            case 6: return signature_matches(kUpdateShieldEffectRva,
                                              kExpectedShieldEffectLookup);
            case 7: return api->install_inline_hook(
                at(kCreateShieldHitRva),
                reinterpret_cast<void*>(&create_shield_hit_hook),
                kExpectedCreateShieldHit.size(), kExpectedCreateShieldHit.data(),
                &g_create_shield_hit_hook);
            case 8: return g_create_shield_hit_hook.gateway != nullptr;
            case 9: return api->install_inline_hook(
                at(kStopShieldEffectRva),
                reinterpret_cast<void*>(&stop_shield_effect_hook),
                kExpectedShieldEffectLookup.size(), kExpectedShieldEffectLookup.data(),
                &g_stop_shield_effect_hook);
            case 10: return g_stop_shield_effect_hook.gateway != nullptr;
            case 11: return api->install_inline_hook(
                at(kUpdateShieldEffectRva),
                reinterpret_cast<void*>(&update_shield_effect_hook),
                kExpectedShieldEffectLookup.size(), kExpectedShieldEffectLookup.data(),
                &g_update_shield_effect_hook);
            case 12: return g_update_shield_effect_hook.gateway != nullptr;
            case 13: g_runtime_ready = true; return 0;
            case 14: return api->register_game_object_class_loaded_handler(
                kModuleName, kCraftFields.data(),
                static_cast<std::uint32_t>(kCraftFields.size()),
                &craft_class_loaded_handler, nullptr);
            case 15: return api->register_craft_event_handler(
                kModuleName, &craft_event_handler, nullptr);
            case 16:
                g_runtime_ready = false;
                log_line("Could not register directional-shield dispatchers; installed effect hooks remain pass-through");
                return 0;
            case 17: log_line("Optional directional-shield policies registered"); return 0;
            case 18: return request_damage_bridge_connection();
            case 19: log_line("Awaiting the shared Craft::Damage bridge"); return 0;
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool result = a2fo_rs_stage8_directional_init(step, &context) != 0;
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Shared dispatchers and the Craft::Damage hook are process-lifetime.
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_ConnectDamageBridge() {
    const auto step = [](void*, std::uint32_t operation) noexcept
            -> std::uintptr_t {
        switch (operation) {
        case 0: return g_runtime_ready;
        case 1: return g_damage_bridge_connected;
        case 2: g_damage_bridge_connected = true; return 0;
        case 3:
            log_line("Directional-shield damage routing connected to Craft::Damage");
            return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_connect(step, nullptr) != 0;
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
    return query_value(craft, 0, 0) != 0;
}

extern "C" __declspec(dllexport)
std::uint32_t A2FO_CALL A2FODirectionalShields_GetMode(void* craft) {
    return query_value(craft, 0, 1);
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

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_GetState(
    void* craft, A2FO_DirectionalShieldState* values) {
    struct Context {
        void* craft;
        A2FO_DirectionalShieldState* values;
        const ShieldPolicy* policy = nullptr;
        CraftState* state = nullptr;
    } context{craft, values};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& value = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return value.values->struct_size;
        case 1: return g_runtime_ready;
        case 2: return g_damage_bridge_connected;
        case 3: return g_active_damage_effect_scope_count;
        case 4:
            value.policy = policy_for_craft(value.craft);
            return reinterpret_cast<std::uintptr_t>(value.policy);
        case 5:
            value.state = state_for_craft(value.craft, true);
            return reinterpret_cast<std::uintptr_t>(value.state);
        case 6:
            reconcile_craft(value.craft, value.state, *value.policy);
            return 0;
        case 7:
            std::copy_n(value.state->stores.current.begin(), 4,
                        value.values->current);
            std::copy_n(value.policy->maximum.begin(), 4,
                        value.values->maximum);
            return 0;
        case 8: return a2fo::valid_shield_values(
            value.values->current, value.values->maximum);
        default: return 0;
        }
    };
    return a2fo_rs_stage8_directional_get_state(
        reinterpret_cast<std::uintptr_t>(craft),
        reinterpret_cast<std::uintptr_t>(values), sizeof(*values),
        step, &context) != 0;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_SetState(
    void* craft, const A2FO_DirectionalShieldState* values) {
    return set_editor_state(craft, values);
}
