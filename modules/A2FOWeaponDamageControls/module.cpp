/*
 * Optional weapon-ODF controls for shield and hull damage.
 *
 * The commands are retained by WeaponClass pointer, then applied to a private
 * copy of each ordnance DamageInfo immediately before Craft consumes it.  This
 * leaves the shared ordnance data and Armada's native shield-spillover rules
 * untouched.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../A2FODirectionalShields/api.hpp"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "damage_controls.hpp"
#include "../../rust/module_runtime_b4_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_weapon_damage_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_weapon_damage_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_weapon_damage_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_weapon_damage_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
extern void* a2fo_weapon_damage_hull_amount_continue;
void a2fo_weapon_damage_hull_amount_bridge();
void __cdecl a2fo_weapon_damage_scale_hull_amount(float* amount) noexcept;
}

namespace {

using a2fo::weapon_damage_controls::DamagePolicy;
using a2fo::weapon_damage_controls::apply_policy_to_flags;
using a2fo::weapon_damage_controls::damage_scale_for_hit;
using a2fo::weapon_damage_controls::hull_spillover_scale;

constexpr char kModuleName[] = "A2FOWeaponDamageControls";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kCraftDamageRva = 0x000c5bb0;
constexpr std::uintptr_t kParameterDbGetBoolRva = 0x00134f50;
constexpr std::uintptr_t kParameterDbGetLookupRva = 0x00135630;
constexpr std::uintptr_t kLookupConstructorRva = 0x0025cfb0;
constexpr std::uintptr_t kLookupDestructorRva = 0x0025cfd0;
constexpr std::uintptr_t kLookupFindRva = 0x0025d170;
constexpr std::uintptr_t kCraftDamageHullAmountRva = 0x000c5f08;
constexpr std::uintptr_t kCraftDamageHullAmountResumeRva = 0x000c5f12;

constexpr std::size_t kWeaponClassOnWeaponOffset = 0x04;
constexpr std::size_t kWeaponOnOrdnanceOffset = 0x38;
constexpr std::size_t kDamageInfoOnOrdnanceOffset = 0x3c;
constexpr std::size_t kDamageInfoDamageOffset = 0x00;
constexpr std::size_t kDamageInfoFlagsOffset = 0x14;
constexpr std::size_t kDamageInfoSize = 0x20;
constexpr std::size_t kCurrentShieldsOnCraftOffset = 0x1c8;
constexpr std::size_t kObjectClassOnCraftOffset = 0x40;
constexpr std::size_t kProjectIdOnObjectClassOffset = 0x1cc;
constexpr std::size_t kLookupSize = 0x18;
constexpr std::size_t kLookupCountOffset = 0x08;
constexpr std::size_t kLookupValuesOffset = 0x10;
constexpr std::size_t kLookupDefaultOffset = 0x14;

constexpr std::uint8_t kExpectedCraftDamage[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
constexpr std::uint8_t kExpectedCraftDamageHullAmount[] = {
    0x8b, 0x43, 0x04, 0xc7, 0x45, 0xfc, 0x00, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetBool[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetLookup[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedLookupConstructor[] = {
    0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00};
constexpr std::uint8_t kExpectedLookupDestructor[] = {
    0xc7, 0x01, 0x84, 0xce, 0x6b, 0x00};
constexpr std::uint8_t kExpectedLookupFind[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x8b, 0x71, 0x08};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
bool g_runtime_ready = false;
bool g_state_lock_ready = false;
A2FO_InlineHook g_craft_damage_hook{};
CRITICAL_SECTION g_state_lock{};
thread_local bool g_hull_amount_scale_active = false;
thread_local float g_hull_amount_scale = 1.0f;
A2FO_DirectionalShieldsBeginDamageFn g_directional_begin_damage = nullptr;
A2FO_DirectionalShieldsEndDamageFn g_directional_end_damage = nullptr;

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
T read_at(const void* object, std::size_t offset,
          T fallback = T{}) noexcept {
    const auto* address = object
        ? static_cast<const std::uint8_t*>(object) + offset : nullptr;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

// Armada uses this exact 24-byte cLookup object for damageBase, hitChance,
// and the other target-specific ODF tables. Keeping the native object lets
// modifier entries use the engine's own cPrjID matching rules.
class NativeLookup {
public:
    NativeLookup() noexcept {
        if (!g_armada) return;
        a2fo_weapon_damage_call_thiscall_0(
            at(g_armada, kLookupConstructorRva), storage_.data());
        initialized_ = true;
        set_default(1.0f);
    }

    ~NativeLookup() {
        if (!initialized_ || !g_armada) return;
        a2fo_weapon_damage_call_thiscall_0(
            at(g_armada, kLookupDestructorRva), storage_.data());
    }

    NativeLookup(const NativeLookup&) = delete;
    NativeLookup& operator=(const NativeLookup&) = delete;

    void* data() noexcept { return storage_.data(); }
    const void* data() const noexcept { return storage_.data(); }
    bool initialized() const noexcept { return initialized_; }

    float default_value() const noexcept {
        return read_at<float>(
            storage_.data(), kLookupDefaultOffset, 1.0f);
    }

    int entry_count() const noexcept {
        const int count = read_at<int>(
            storage_.data(), kLookupCountOffset, 0);
        return count > 0 ? count : 0;
    }

    void set_default(float value) noexcept {
        std::memcpy(
            storage_.data() + kLookupDefaultOffset, &value, sizeof(value));
    }

    // Preserve the module's established non-negative modifier contract while
    // accepting the same table grammar as Armada's native lookups.
    int sanitize() noexcept {
        int changed = 0;
        float fallback = default_value();
        if (!std::isfinite(fallback) || fallback < 0.0f) {
            fallback = 1.0f;
            set_default(fallback);
            ++changed;
        }

        const int count = entry_count();
        float* values = read_at<float*>(
            storage_.data(), kLookupValuesOffset, nullptr);
        if (!values || !readable_range(
                values, static_cast<std::size_t>(count) * sizeof(float))) {
            return changed;
        }
        for (int index = 0; index < count; ++index) {
            if (!std::isfinite(values[index]) || values[index] < 0.0f) {
                values[index] = 1.0f;
                ++changed;
            }
        }
        return changed;
    }

    float resolve(const void* project_id) const noexcept {
        float value = default_value();
        if (initialized_ && project_id) {
            a2fo_weapon_damage_call_thiscall_2(
                at(g_armada, kLookupFindRva),
                const_cast<void*>(data()),
                reinterpret_cast<std::uintptr_t>(project_id),
                reinterpret_cast<std::uintptr_t>(&value));
        }
        return value;
    }

private:
    alignas(4) std::array<std::uint8_t, kLookupSize> storage_{};
    bool initialized_ = false;
};

struct ClassPolicy {
    DamagePolicy damage{};
    NativeLookup shield_modifier{};
    NativeLookup hull_modifier{};
};

std::unordered_map<void*, std::unique_ptr<ClassPolicy>> g_class_policies;

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

bool read_optional_bool(void* parameter_db, const char* key,
                        bool* value) noexcept {
    if (!parameter_db || !key || !value) return false;
    std::uint8_t parsed = *value ? 1u : 0u;
    const std::uintptr_t found = a2fo_weapon_damage_call_thiscall_3(
        at(g_armada, kParameterDbGetBoolRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(&parsed), parsed ? 1u : 0u);
    if ((found & 0xffu) == 0) return false;
    *value = parsed != 0;
    return true;
}

bool read_optional_lookup(
    void* parameter_db, const char* key, NativeLookup* output,
    const NativeLookup& inherited) noexcept {
    if (!parameter_db || !key || !output || !output->initialized() ||
        !inherited.initialized()) {
        return false;
    }
    const std::uintptr_t found = a2fo_weapon_damage_call_thiscall_3(
        at(g_armada, kParameterDbGetLookupRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output->data()),
        reinterpret_cast<std::uintptr_t>(inherited.data()));
    return (found & 0xffu) != 0;
}

struct RegisterContext {
    void* weapon_class;
    void* parent_class;
    void* parameter_db;
    std::unique_ptr<ClassPolicy> policy;
    const ClassPolicy* inherited_policy = nullptr;
    std::optional<NativeLookup> default_shield;
    std::optional<NativeLookup> default_hull;
    char message[320]{};
};

std::uint64_t register_class_op(void* opaque, std::uint32_t operation,
                                std::uint32_t) noexcept {
    auto& context = *static_cast<RegisterContext*>(opaque);
    switch (operation) {
        case 0:
            try { context.policy = std::make_unique<ClassPolicy>(); }
            catch (...) { log_line("Could not allocate a weapon damage policy"); return 0; }
            if (!context.policy->shield_modifier.initialized() ||
                !context.policy->hull_modifier.initialized()) {
                log_line("Could not initialize Armada target-specific lookups");
                return 0;
            }
            return 1;
        case 1: {
            EnterCriticalSection(&g_state_lock);
            const auto inherited = g_class_policies.find(context.parent_class);
            if (inherited != g_class_policies.end() && inherited->second) {
                context.inherited_policy = inherited->second.get();
                context.policy->damage.can_damage_shields =
                    context.inherited_policy->damage.can_damage_shields;
                context.policy->damage.can_damage_hull =
                    context.inherited_policy->damage.can_damage_hull;
            }
            LeaveCriticalSection(&g_state_lock);
            return context.inherited_policy != nullptr;
        }
        case 2:
            return read_optional_bool(context.parameter_db, "canDamageShields",
                                      &context.policy->damage.can_damage_shields);
        case 3:
            return read_optional_bool(context.parameter_db, "canDamageHull",
                                      &context.policy->damage.can_damage_hull);
        case 4: {
            context.default_shield.emplace();
            context.default_hull.emplace();
            const NativeLookup& inherited = context.inherited_policy
                ? context.inherited_policy->shield_modifier : *context.default_shield;
            return read_optional_lookup(context.parameter_db, "shieldDamageModifier",
                                        &context.policy->shield_modifier, inherited);
        }
        case 5: {
            const NativeLookup& inherited = context.inherited_policy
                ? context.inherited_policy->hull_modifier : *context.default_hull;
            return read_optional_lookup(context.parameter_db, "hullDamageModifier",
                                        &context.policy->hull_modifier, inherited);
        }
        case 6: return context.policy->shield_modifier.sanitize();
        case 7:
            log_line("shieldDamageModifier contains a non-finite or negative value; invalid entries use 1.0");
            return 0;
        case 8: return context.policy->hull_modifier.sanitize();
        case 9:
            log_line("hullDamageModifier contains a non-finite or negative value; invalid entries use 1.0");
            return 0;
        case 10:
            context.policy->damage.shield_damage_modifier =
                context.policy->shield_modifier.default_value();
            context.policy->damage.hull_damage_modifier =
                context.policy->hull_modifier.default_value();
            std::snprintf(context.message, sizeof(context.message),
                "Registered weapon damage policy: shields=%s x%.3g (%d target overrides), hull=%s x%.3g (%d target overrides)",
                context.policy->damage.can_damage_shields ? "yes" : "no",
                static_cast<double>(context.policy->damage.shield_damage_modifier),
                context.policy->shield_modifier.entry_count(),
                context.policy->damage.can_damage_hull ? "yes" : "no",
                static_cast<double>(context.policy->damage.hull_damage_modifier),
                context.policy->hull_modifier.entry_count());
            return 0;
        case 11:
            try {
                EnterCriticalSection(&g_state_lock);
                g_class_policies[context.weapon_class] = std::move(context.policy);
                LeaveCriticalSection(&g_state_lock);
                return 1;
            } catch (...) {
                LeaveCriticalSection(&g_state_lock);
                return 0;
            }
        case 12: log_line("Could not retain a weapon damage policy"); return 0;
        case 13: log_line(context.message); return 0;
    }
    return 0;
}
void register_class_policy(void* weapon_class, void* parent_class,
                           void* parameter_db) noexcept {
    RegisterContext context{};
    context.weapon_class = weapon_class;
    context.parent_class = parent_class;
    context.parameter_db = parameter_db;
    a2fo_rs_damage_register(g_runtime_ready && weapon_class && parameter_db,
                            &context, &register_class_op);
}

bool policy_for_damage_info(const void* damage_info, const void* target_craft,
                            DamagePolicy* output) noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    // The shared detour still routes directional shields while custom weapon
    // modifiers are off. Retain class metadata for later menu activation.
    if (!a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE)) return false;
#endif
    if (!damage_info || !target_craft || !output || !g_state_lock_ready) {
        return false;
    }
    const auto damage_address = reinterpret_cast<std::uintptr_t>(damage_info);
    if (damage_address < kDamageInfoOnOrdnanceOffset) return false;
    const void* ordnance = reinterpret_cast<const void*>(
        damage_address - kDamageInfoOnOrdnanceOffset);
    void* weapon = read_at<void*>(
        ordnance, kWeaponOnOrdnanceOffset, nullptr);
    void* weapon_class = read_at<void*>(
        weapon, kWeaponClassOnWeaponOffset, nullptr);
    if (!weapon_class) return false;
    void* object_class = read_at<void*>(
        target_craft, kObjectClassOnCraftOffset, nullptr);
    void* target_project_id = read_at<void*>(
        object_class, kProjectIdOnObjectClassOffset, nullptr);
    if (target_project_id && !readable_range(
            target_project_id, sizeof(std::uint32_t))) {
        target_project_id = nullptr;
    }

    bool found = false;
    EnterCriticalSection(&g_state_lock);
    const auto policy = g_class_policies.find(weapon_class);
    if (policy != g_class_policies.end() && policy->second) {
        *output = policy->second->damage;
        output->shield_damage_modifier =
            policy->second->shield_modifier.resolve(target_project_id);
        output->hull_damage_modifier =
            policy->second->hull_modifier.resolve(target_project_id);
        found = true;
    }
    LeaveCriticalSection(&g_state_lock);
    return found;
}

void A2FO_CALL weapon_class_loaded_handler(
    const A2FO_WeaponClassLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event)) return;
    register_class_policy(
        event->weapon_class, event->parent_class, event->parameter_db);
}

template <typename Function>
Function exported_function(HMODULE module, const char* name) noexcept {
    FARPROC exported = module && name ? GetProcAddress(module, name) : nullptr;
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(exported),
                  "unexpected function-pointer size");
    std::memcpy(&function, &exported, sizeof(function));
    return function;
}

bool connect_directional_shields() noexcept {
    if (g_directional_begin_damage && g_directional_end_damage) return true;
    HMODULE module = a2fo::module_provider(
        "A2FODirectionalShields.dll",
        {"A2FODirectionalShields_ConnectDamageBridge",
         "A2FODirectionalShields_BeginDamage",
         "A2FODirectionalShields_EndDamage"});
    if (!module) return false;
    const auto connect = exported_function<
        A2FO_DirectionalShieldsConnectDamageBridgeFn>(
            module, "A2FODirectionalShields_ConnectDamageBridge");
    const auto begin = exported_function<
        A2FO_DirectionalShieldsBeginDamageFn>(
            module, "A2FODirectionalShields_BeginDamage");
    const auto end = exported_function<
        A2FO_DirectionalShieldsEndDamageFn>(
            module, "A2FODirectionalShields_EndDamage");
    if (!connect || !begin || !end || !connect()) return false;
    g_directional_begin_damage = begin;
    g_directional_end_damage = end;
    return true;
}

struct DamageContext {
    void* craft;
    void* damage_info;
    void* argument2;
    void* argument3;
    std::uintptr_t argument4;
    A2FO_DirectionalShieldDamageScope directional_scope{};
    DamagePolicy policy{};
    std::array<std::uint8_t, kDamageInfoSize> local_damage{};
    void* native_damage_info = nullptr;
    bool previous_scale_active = false;
    float previous_scale = 1.0f;
};
std::uint64_t damage_hit_op(void* opaque, std::uint32_t operation,
                            std::uint32_t) noexcept {
    auto& context = *static_cast<DamageContext*>(opaque);
    switch (operation) {
        case 0:
            return g_runtime_ready && readable_range(context.damage_info,
                                                       kDamageInfoSize);
        case 1:
            context.directional_scope.struct_size = sizeof(context.directional_scope);
            return g_directional_begin_damage && g_directional_begin_damage(
                context.craft, context.damage_info, &context.directional_scope);
        case 2: {
            const bool found = policy_for_damage_info(
                context.damage_info, context.craft, &context.policy);
            context.native_damage_info = context.damage_info;
            context.previous_scale_active = g_hull_amount_scale_active;
            context.previous_scale = g_hull_amount_scale;
            return found;
        }
        case 3: {
            std::memcpy(context.local_damage.data(), context.damage_info,
                        context.local_damage.size());
            std::uint32_t flags = 0;
            std::memcpy(&flags, context.local_damage.data() + kDamageInfoFlagsOffset,
                        sizeof(flags));
            const float current_shields = read_at<float>(
                context.craft, kCurrentShieldsOnCraftOffset, 0.0f);
            const bool shields_up = current_shields > 0.0f;
            flags = apply_policy_to_flags(flags, context.policy, shields_up);
            std::memcpy(context.local_damage.data() + kDamageInfoFlagsOffset,
                        &flags, sizeof(flags));
            float damage = 0.0f;
            std::memcpy(&damage, context.local_damage.data() + kDamageInfoDamageOffset,
                        sizeof(damage));
            damage *= damage_scale_for_hit(context.policy, shields_up);
            std::memcpy(context.local_damage.data() + kDamageInfoDamageOffset,
                        &damage, sizeof(damage));
            context.native_damage_info = context.local_damage.data();
            g_hull_amount_scale = hull_spillover_scale(context.policy, shields_up);
            g_hull_amount_scale_active = true;
            return 0;
        }
        case 5:
            return a2fo_weapon_damage_call_thiscall_4(
                g_craft_damage_hook.gateway, context.craft,
                reinterpret_cast<std::uintptr_t>(context.native_damage_info
                    ? context.native_damage_info : context.damage_info),
                reinterpret_cast<std::uintptr_t>(context.argument2),
                reinterpret_cast<std::uintptr_t>(context.argument3),
                context.argument4);
        case 6:
            g_hull_amount_scale = context.previous_scale;
            g_hull_amount_scale_active = context.previous_scale_active;
            return 0;
        case 7:
            if (g_directional_end_damage) {
                g_directional_end_damage(&context.directional_scope);
            }
            return 0;
    }
    return 0;
}
std::uintptr_t __attribute__((fastcall)) craft_damage_hook(
    void* craft, void*, void* damage_info, void* argument2,
    void* argument3, std::uintptr_t argument4) noexcept {
    DamageContext context{craft, damage_info, argument2, argument3, argument4};
    return a2fo_rs_damage_hit(&context, &damage_hit_op);
}

bool preflight_signatures() noexcept {
    bool supported = signature_matches(
        g_armada, kCraftDamageRva, kExpectedCraftDamage);
    if (!supported) log_line("Craft::Damage signature is unsupported");

    const auto* bool_getter = static_cast<const std::uint8_t*>(
        at(g_armada, kParameterDbGetBoolRva));
    const bool bool_getter_supported = signature_matches(
            g_armada, kParameterDbGetBoolRva,
            kExpectedParameterDbGetBool) ||
        (readable_range(bool_getter, 5) && bool_getter[0] == 0xe9);
    if (!bool_getter_supported) {
        log_line("ParameterDB::Get(bool) signature is unsupported");
        supported = false;
    }

    const auto* lookup_getter = static_cast<const std::uint8_t*>(
        at(g_armada, kParameterDbGetLookupRva));
    const bool lookup_getter_supported = signature_matches(
            g_armada, kParameterDbGetLookupRva,
            kExpectedParameterDbGetLookup) ||
        (readable_range(lookup_getter, 5) && lookup_getter[0] == 0xe9);
    if (!lookup_getter_supported) {
        log_line("ParameterDB::Get(cLookup) signature is unsupported");
        supported = false;
    }

    if (!signature_matches(
            g_armada, kLookupConstructorRva,
            kExpectedLookupConstructor)) {
        log_line("cLookup constructor signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kLookupDestructorRva,
            kExpectedLookupDestructor)) {
        log_line("cLookup destructor signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kLookupFindRva, kExpectedLookupFind)) {
        log_line("cLookup::Find signature is unsupported");
        supported = false;
    }

    if (!signature_matches(
            g_armada, kCraftDamageHullAmountRva,
            kExpectedCraftDamageHullAmount)) {
        log_line("Craft::Damage hull-amount signature is unsupported");
        supported = false;
    }

    return supported;
}

std::uint64_t damage_init_op(void* opaque, std::uint32_t operation,
                             std::uint32_t argument) noexcept {
    const auto* api = static_cast<const A2FO_ModuleApi*>(opaque);
    try {
    switch (operation) {
        case 0:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            return g_armada != nullptr;
        case 1:
            InitializeCriticalSection(&g_state_lock);
            g_state_lock_ready = true;
            return 0;
        case 2: return preflight_signatures();
        case 3:
            a2fo_weapon_damage_hull_amount_continue = at(
                g_armada, kCraftDamageHullAmountResumeRva);
            return 0;
        case 4:
            return api->patch_jump(
                at(g_armada, kCraftDamageHullAmountRva),
                reinterpret_cast<void*>(&a2fo_weapon_damage_hull_amount_bridge),
                kExpectedCraftDamageHullAmount, sizeof(kExpectedCraftDamageHullAmount));
        case 5:
            return api->install_inline_hook(
                at(g_armada, kCraftDamageRva),
                reinterpret_cast<void*>(&craft_damage_hook),
                sizeof(kExpectedCraftDamage), kExpectedCraftDamage,
                &g_craft_damage_hook) && g_craft_damage_hook.gateway != nullptr;
        case 6: {
            constexpr const char* fields[]{
                "canDamageShields", "canDamageHull",
                "shieldDamageModifier", "hullDamageModifier"};
            return api->register_weapon_class_loaded_handler(
                kModuleName, fields, static_cast<std::uint32_t>(std::size(fields)),
                &weapon_class_loaded_handler, nullptr);
        }
        case 7: g_runtime_ready = argument != 0; return 0;
        case 8: return connect_directional_shields();
        case 9:
            log_line("Weapon damage controls initialized through the shared WeaponClass dispatcher");
            return 0;
        case 10:
            log_line("Directional shields connected to the shared Craft::Damage hook");
            return 0;
        case 11:
            log_line("Weapon damage controls loaded with runtime disabled");
            return 0;
    }
    return 0;
    } catch (...) {
        return 0;
    }
}

}  // namespace

extern "C" {
void* a2fo_weapon_damage_hull_amount_continue = nullptr;
}

extern "C" void __cdecl a2fo_weapon_damage_scale_hull_amount(
    float* amount) noexcept {
    if (!amount || !g_hull_amount_scale_active) return;
    *amount *= g_hull_amount_scale;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook ||
        !api->patch_jump ||
        !A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) ||
        !api->register_weapon_class_loaded_handler ||
        (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) == 0) {
        return false;
    }
    return a2fo_rs_damage_init(true, const_cast<A2FO_ModuleApi*>(api),
                               &damage_init_op) != 0;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Process-lifetime inline hooks and their sidecars intentionally remain.
}

// DirectionalShields calls this when it loads after WeaponDamageControls.
// The ordinary ModuleInit lookup covers the opposite order, making the
// optional bridge independent of info.ini module ordering.
extern "C" __declspec(dllexport)
bool A2FO_CALL
A2FOWeaponDamageControls_RefreshDirectionalShieldsBridge() {
    return a2fo_rs_damage_refresh(g_runtime_ready, nullptr,
        [](void*, std::uint32_t, std::uint32_t) noexcept -> std::uint64_t {
            return connect_directional_shields();
        }) != 0;
}
