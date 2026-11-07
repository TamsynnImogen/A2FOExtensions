/*
 * Indexed, ODF-driven hull-mounted turrets for Fleet Operations.
 *
 * Parent craft declare turretN/turretHardpointN pairs. The referenced ODF
 * uses the semantic "turret" classlabel, which is deliberately hosted by the
 * native SensorArray class so every linked turret retains Armada's ordinary
 * weapon, target, damage, render, and save machinery.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "turret_combat.hpp"
#include "turret_math.hpp"
#include "../../rust/stage8_a_turrets_ffi.h"

#include <windows.h>

#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_turret_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_turret_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_turret_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_turret_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
}

namespace {

using a2fo::turrets::AimAngles;
using a2fo::turrets::AimLimits;
using a2fo::turrets::Matrix34;

using ShieldClassObserver = void (A2FO_CALL *)(
    void* object_class, void* parameter_db);

constexpr const char* kModuleName = "A2FOTurrets";
constexpr std::size_t kMaximumTurrets = 64;
constexpr char kLinkedLabelPrefix[] = "A2FOT:";
constexpr char kAlwaysShowShieldsModuleName[] =
    "A2FOAlwaysShowShields.dll";
constexpr char kShieldClassObserverExport[] =
    "A2FOAlwaysShowShields_RegisterClass";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs. These are checked before a
// single hook is installed so an unsupported executable cannot leave a
// partially active runtime.
constexpr std::uintptr_t kGameObjectClassConstructorRva = 0x000cc480;
constexpr std::uintptr_t kWeaponSetTargetRva = 0x00271340;
constexpr std::uintptr_t kFoGameObjectClassConstructorHandlerRva =
    0x0010bd80;

constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x000cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x000cd390;
constexpr std::uintptr_t kGameObjectClassGetHierarchyRootRva = 0x000cd940;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetWorldTransformRva = 0x000cff90;
constexpr std::uintptr_t kEntityGetRva = 0x000cfff0;
constexpr std::uintptr_t kGameObjectSwapTeamRva = 0x000d0ea0;
constexpr std::uintptr_t kGameObjectSwapRaceAndTeamRva = 0x000d0ed0;
constexpr std::uintptr_t kGameObjectSetTransformRva = 0x000d4ce0;
constexpr std::uintptr_t kCraftDoExpireRva = 0x000caae0;
constexpr std::uintptr_t kCraftGetAlertStatusRva = 0x000c9a20;
constexpr std::uintptr_t kCraftSetAlertStatusRva = 0x000c9a50;
constexpr std::uintptr_t kCraftGetSpecialWeaponAutonomyRva = 0x000c9ae0;
constexpr std::uintptr_t kCraftSetSpecialWeaponAutonomyRva = 0x000c9b10;
constexpr std::uintptr_t kGameObjectGetCurrentCommandRva = 0x000d19c0;
constexpr std::uintptr_t kGameObjectSetTargetlessCommandRva = 0x000d1a40;
constexpr std::uintptr_t kGameObjectSetObjectCommandRva = 0x000d1af0;
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kSt3dNodeFindRecursiveRva = 0x00238780;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;

constexpr std::size_t kObjectExpiredOffset = 0x27;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectLabelOffset = 0x48;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kObjectRaceOffset = 0xfc;
constexpr std::size_t kCommandTargetFormOffset = 0x00;
constexpr std::size_t kCommandIdOffset = 0x04;
constexpr std::size_t kCommandTargetHandleOffset = 0x08;
constexpr std::size_t kCommandExtraOffset = 0x10;
constexpr std::size_t kCommandFlagsOffset = 0x28;

constexpr std::uint8_t kExpectedGameObjectClassConstructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedWeaponSetTarget[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedCraftGetAlertStatus[] = {
    0x8b, 0x41, 0x44, 0x6a, 0x00};
constexpr std::uint8_t kExpectedCraftSetAlertStatus[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x41, 0x44};
constexpr std::uint8_t kExpectedCraftGetSpecialWeaponAutonomy[] = {
    0x8b, 0x41, 0x44, 0x6a, 0x00};
constexpr std::uint8_t kExpectedCraftSetSpecialWeaponAutonomy[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x41, 0x44};
constexpr std::uint8_t kExpectedGameObjectGetCurrentCommand[] = {
    0x8d, 0x41, 0x4c, 0xc3};
constexpr std::uint8_t kExpectedGameObjectSetTargetlessCommand[] = {
    0x55, 0x8b, 0xec, 0x64, 0xa1, 0x00};
constexpr std::uint8_t kExpectedGameObjectSetObjectCommand[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x14};
constexpr std::uint8_t kExpectedFoGameObjectClassConstructorHandler[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe8, 0x53};

constexpr std::array<A2FO_ClasslabelOdfDefault, 7> kTurretDefaults{{
    {"ignoreInterface", "1"},
    {"avoidMe", "0"},
    {"avoidanceClass", "0"},
    {"mapIcon", "mapicon_empty"},
    {"footprintBuffer", "0.0f"},
    {"createFootprint", "0"},
    {"destroyFootprint", "0"},
}};

enum class MountStatus {
    pending,
    active,
    destroyed,
    failed,
};

struct MountClassConfig {
    std::uint32_t index = 0;
    std::string turret_odf;
    std::string hardpoint;
};

struct ParentClassConfig {
    std::string odf_name;
    std::vector<MountClassConfig> mounts;
};

struct TurretClassConfig {
    std::string odf_name;
    AimLimits limits{};
    AimAngles rest{};
    bool return_to_rest = true;
};

struct MountRuntime {
    MountClassConfig config;
    void* hardpoint_node = nullptr;
    bool hardpoint_resolved = false;
    std::uint32_t child_handle = 0;
    MountStatus status = MountStatus::pending;
};

struct ParentRuntime {
    void* object_class = nullptr;
    std::vector<MountRuntime> mounts;
};

struct ChildRuntime {
    std::uint32_t parent_handle = 0;
    std::uint32_t mount_index = 0;
    // An explicit Attack command is inherited from the host for as long as it
    // remains current. Zero means the turret may use native target selection.
    std::uint32_t ordered_target_handle = 0;
    AimAngles current{};
};

struct AttackOrder {
    std::uint32_t target_handle = 0;
    void* target = nullptr;
    std::uint32_t extra = 0;
    std::uint8_t flags = 0;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
ShieldClassObserver g_shield_class_observer = nullptr;
bool g_runtime_ready = false;
bool g_chained_fo_class_constructor = false;
void* g_class_constructor_original = nullptr;
A2FO_InlineHook g_class_constructor_hook{};
A2FO_InlineHook g_weapon_set_target_hook{};

std::unordered_map<void*, ParentClassConfig> g_parent_classes;
std::unordered_map<void*, TurretClassConfig> g_turret_classes;
std::unordered_map<std::uint32_t, ParentRuntime> g_parents;
std::unordered_map<std::uint32_t, ChildRuntime> g_children;
std::unordered_map<std::uint32_t, std::uint32_t> g_targets;

void prepare_linked_turret_for_simulation(
    void* child, std::uint32_t child_handle) noexcept;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

struct TurretShieldObserverContext {
    HMODULE provider = nullptr;
    FARPROC class_export = nullptr;
};

std::uintptr_t turret_shield_observer_step(void* opaque, std::uint32_t op,
                                         std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretShieldObserverContext*>(opaque);
    switch (op) {
    case 0:
        context.provider = GetModuleHandleA("A2FOExtensions.dll");
        return reinterpret_cast<std::uintptr_t>(context.provider);
    case 1:
        context.class_export = GetProcAddress(
            context.provider, kShieldClassObserverExport);
        return reinterpret_cast<std::uintptr_t>(context.class_export);
    case 2:
        context.provider = GetModuleHandleA(kAlwaysShowShieldsModuleName);
        return reinterpret_cast<std::uintptr_t>(context.provider);
    case 3:
        context.class_export = GetProcAddress(
            context.provider, kShieldClassObserverExport);
        return reinterpret_cast<std::uintptr_t>(context.class_export);
    case 4:
        context.class_export = reinterpret_cast<FARPROC>(arg);
    static_assert(sizeof(context.class_export) ==
                  sizeof(g_shield_class_observer),
                  "unexpected function-pointer size");
    std::memcpy(&g_shield_class_observer, &context.class_export,
                sizeof(g_shield_class_observer));
        return 1;
    case 5: return g_shield_class_observer != nullptr;
    case 6: g_shield_class_observer = nullptr; return 1;
    case 7:
        log_line(
            "Shield visibility callbacks linked through "
            "A2FOAlwaysShowShields");
        return 1;
    default: return 0;
    }
}

void resolve_shield_observers() noexcept {
    TurretShieldObserverContext context{};
    a2fo_rs_stage8_turrets_resolve_shield_observers(
        &turret_shield_observer_step, &context);
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
T read_at(const void* object, std::size_t offset, T fallback = T{}) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            sizeof(T))) {
        return fallback;
    }
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
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

std::uint32_t object_handle(const void* object) noexcept {
    return read_at<std::uint32_t>(object, kObjectHandleOffset, 0);
}

void* object_class(const void* object) noexcept {
    return read_at<void*>(object, kObjectClassOffset, nullptr);
}

bool object_expired(const void* object) noexcept {
    return read_at<std::uint8_t>(object, kObjectExpiredOffset, 1) != 0;
}

void* find_entity(std::uint32_t handle) noexcept {
    if (!g_armada || handle == 0) return nullptr;
    using EntityGetFn = void* (__cdecl*)(std::uint32_t);
    const auto get = reinterpret_cast<EntityGetFn>(
        at(g_armada, kEntityGetRva));
    return get ? get(handle) : nullptr;
}

void copy_class_odf_name(void* object_class_pointer, char* output,
                         std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!object_class_pointer || !g_armada) return;
    const char* name = reinterpret_cast<const char*>(
        a2fo_turret_call_thiscall_0(
            at(g_armada, kGameObjectClassGetOdfNameRva),
            object_class_pointer));
    if (!name) return;
    std::snprintf(output, output_size, "%s", name);
}

void trim_string(std::string* value) {
    if (!value) return;
    std::size_t begin = 0;
    std::size_t end = value->size();
    const auto ctype = +[](void*, std::uint32_t op,
                           std::uintptr_t byte) noexcept -> std::uintptr_t {
        const auto character = static_cast<unsigned char>(byte);
        return op == 0 ? std::isspace(character) != 0
                       : static_cast<std::uintptr_t>(std::tolower(character));
    };
    a2fo_rs_stage8_turrets_trim(
        reinterpret_cast<const std::uint8_t*>(value->data()),
        value->size(), ctype, nullptr, &begin, &end);
    *value = value->substr(begin, end - begin);
}

struct TurretNormalizeOdfContext {
    std::string* value;
    std::exception_ptr exception{};
};

std::uintptr_t turret_normalize_odf_step(void* opaque, std::uint32_t op,
                                       std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretNormalizeOdfContext*>(opaque);
    if (context.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
    switch (op) {
    case 0: return context.value != nullptr;
    case 1: trim_string(context.value); return 1;
    case 2: {
        const auto ctype = +[](void*, std::uint32_t,
                               std::uintptr_t byte) noexcept -> std::uintptr_t {
            return static_cast<std::uintptr_t>(
                std::tolower(static_cast<unsigned char>(byte)));
        };
        return a2fo_rs_stage8_turrets_odf_suffix(
            reinterpret_cast<const std::uint8_t*>(context.value->data()),
            context.value->size(), ctype, nullptr);
    }
    case 3: context.value->resize(context.value->size() - 4); return 1;
    default: return 0;
    }
    } catch (...) {
        context.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

void normalize_odf_basename(std::string* value) {
    TurretNormalizeOdfContext context{value};
    a2fo_rs_stage8_turrets_normalize_odf(
        &turret_normalize_odf_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

struct TurretParameterStringContext {
    void* parameter_db;
    const char* key;
    std::string* output;
    std::array<char, 260> value{};
};

std::uintptr_t turret_parameter_string_step(void* opaque, std::uint32_t op,
                                          std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretParameterStringContext*>(opaque);
    switch (op) {
    case 0: return context.parameter_db != nullptr;
    case 1: return context.key != nullptr;
    case 2: return *context.key != '\0';
    case 3: return context.output != nullptr;
    case 4: return g_armada != nullptr;
    case 5:
        return a2fo_turret_call_thiscall_4(
            at(g_armada, kParameterDbGetStringRva), context.parameter_db,
            reinterpret_cast<std::uintptr_t>(context.key),
            reinterpret_cast<std::uintptr_t>(context.value.data()),
            static_cast<std::uintptr_t>(context.value.size()),
            reinterpret_cast<std::uintptr_t>(""));
    case 6: context.value.back() = '\0'; return 1;
    case 7: return context.value[0] != '\0';
    case 8:
        try {
            *context.output = context.value.data();
            return 1;
        } catch (...) { return 0; }
    case 9:
        try { trim_string(context.output); return 1; }
        catch (...) { return 0; }
    case 10: return !context.output->empty();
    default: return 0;
    }
}

bool read_parameter_string(void* parameter_db, const char* key,
                           std::string* output) noexcept {
    TurretParameterStringContext context{parameter_db, key, output, {}};
    return a2fo_rs_stage8_turrets_read_parameter_string(
        &turret_parameter_string_step, &context) != 0;
}

struct TurretFloatContext {
    const std::string* text;
    float* output;
    char* end = nullptr;
    float parsed = 0.0f;
};

std::uintptr_t turret_float_step(void* opaque, std::uint32_t op,
                               std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretFloatContext*>(opaque);
    switch (op) {
    case 0: return context.output != nullptr;
    case 1: return !context.text->empty();
    case 2:
        errno = 0;
        context.parsed = std::strtof(context.text->c_str(), &context.end);
        return 1;
    case 3: return context.end != context.text->c_str();
    case 4: return errno != ERANGE;
    case 5: return std::isfinite(context.parsed);
    case 6: return static_cast<unsigned char>(*context.end);
    case 7: return static_cast<unsigned char>(*context.end);
    case 8: return std::isspace(static_cast<unsigned char>(arg)) != 0;
    case 9: ++context.end; return 1;
    case 10: *context.output = context.parsed; return 1;
    default: return 0;
    }
}

bool parse_float(const std::string& text, float* output) noexcept {
    TurretFloatContext context{&text, output};
    return a2fo_rs_stage8_turrets_parse_float(
        &turret_float_step, &context) != 0;
}

struct TurretParameterFloatContext {
    void* parameter_db;
    const char* key;
    float* output;
    std::string value;
};

std::uintptr_t turret_parameter_float_step(void* opaque, std::uint32_t op,
                                         std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretParameterFloatContext*>(opaque);
    switch (op) {
    case 0: return read_parameter_string(context.parameter_db, context.key,
        &context.value);
    case 1: return parse_float(context.value, context.output);
    default: return 0;
    }
}

bool read_parameter_float(void* parameter_db, const char* key,
                          float* output) noexcept {
    TurretParameterFloatContext context{parameter_db, key, output, {}};
    return a2fo_rs_stage8_turrets_read_parameter_float(
        &turret_parameter_float_step, &context) != 0;
}

struct TurretParameterBoolContext {
    void* parameter_db;
    const char* key;
    bool* output;
    std::string value;
};

std::uintptr_t turret_parameter_bool_step(void* opaque, std::uint32_t op,
                                        std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretParameterBoolContext*>(opaque);
    switch (op) {
    case 0: return context.output != nullptr;
    case 1:
        return read_parameter_string(
            context.parameter_db, context.key, &context.value);
    case 2: return context.value.size();
    case 3: {
        char& character = context.value[arg];
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
        return static_cast<unsigned char>(character);
    }
    case 4: *context.output = arg != 0; return 1;
    default: return 0;
    }
}

bool read_parameter_bool(void* parameter_db, const char* key,
                         bool* output) noexcept {
    TurretParameterBoolContext context{parameter_db, key, output, {}};
    return a2fo_rs_stage8_turrets_read_parameter_bool(
        &turret_parameter_bool_step, &context) != 0;
}

struct TurretSemanticContext {
    void* parameter_db;
    std::array<char, 64> label{};
};

std::uintptr_t turret_semantic_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretSemanticContext*>(opaque);
    switch (op) {
    case 0:
        return g_api && g_api->get_original_classlabel &&
            context.parameter_db;
    case 1:
        return g_api->get_original_classlabel(
            context.parameter_db, context.label.data(),
            static_cast<std::uint32_t>(context.label.size()));
    case 2: {
        char& character = context.label[arg];
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
        return 1;
    }
    case 3: return static_cast<unsigned char>(context.label[arg]);
    default: return 0;
    }
}

bool is_semantic_turret(void* parameter_db) noexcept {
    TurretSemanticContext context{parameter_db, {}};
    return a2fo_rs_stage8_turrets_is_semantic_turret(
        &turret_semantic_step, &context) != 0;
}

void normalize_turret_config(TurretClassConfig* config) noexcept {
    if (!config) return;
    A2FO_Stage8TurretAim aim{
        config->limits.yaw_min_degrees,
        config->limits.yaw_max_degrees,
        config->limits.pitch_min_degrees,
        config->limits.pitch_max_degrees,
        config->limits.yaw_rate_degrees,
        config->limits.pitch_rate_degrees,
        config->rest.yaw_degrees,
        config->rest.pitch_degrees};
    a2fo_rs_stage8_turrets_normalize(&aim);
    config->limits.yaw_min_degrees = aim.yaw_min;
    config->limits.yaw_max_degrees = aim.yaw_max;
    config->limits.pitch_min_degrees = aim.pitch_min;
    config->limits.pitch_max_degrees = aim.pitch_max;
    config->limits.yaw_rate_degrees = aim.yaw_rate;
    config->limits.pitch_rate_degrees = aim.pitch_rate;
    config->rest.yaw_degrees = aim.rest_yaw;
    config->rest.pitch_degrees = aim.rest_pitch;
}

struct TurretClassContext {
    void* class_object = nullptr;
    void* parameter_db = nullptr;
    char odf_name[128]{};
    TurretClassConfig config{};
    ParentClassConfig parent{};
    std::size_t index = 0;
    char turret_key[32]{};
    char hardpoint_key[40]{};
    std::string turret_odf;
    std::string hardpoint;
    bool has_turret = false;
    bool has_hardpoint = false;
    std::exception_ptr exception{};
};

std::uintptr_t turret_class_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t arg) noexcept {
    auto& c = *static_cast<TurretClassContext*>(opaque);
    if (c.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
        switch (op) {
        case 0: return g_runtime_ready && c.class_object && c.parameter_db;
        case 1:
            copy_class_odf_name(c.class_object, c.odf_name,
                                sizeof(c.odf_name));
            return 1;
        case 2: return is_semantic_turret(c.parameter_db);
        case 3: c.config.odf_name = c.odf_name; return 1;
        case 4:
            switch (arg) {
            case 0: read_parameter_float(c.parameter_db, "turretYawMin",
                &c.config.limits.yaw_min_degrees); break;
            case 1: read_parameter_float(c.parameter_db, "turretYawMax",
                &c.config.limits.yaw_max_degrees); break;
            case 2: read_parameter_float(c.parameter_db, "turretPitchMin",
                &c.config.limits.pitch_min_degrees); break;
            case 3: read_parameter_float(c.parameter_db, "turretPitchMax",
                &c.config.limits.pitch_max_degrees); break;
            case 4: read_parameter_float(c.parameter_db, "turretYawRate",
                &c.config.limits.yaw_rate_degrees); break;
            case 5: read_parameter_float(c.parameter_db, "turretPitchRate",
                &c.config.limits.pitch_rate_degrees); break;
            case 6: read_parameter_float(c.parameter_db, "turretRestYaw",
                &c.config.rest.yaw_degrees); break;
            case 7: read_parameter_float(c.parameter_db, "turretRestPitch",
                &c.config.rest.pitch_degrees); break;
            case 8: read_parameter_bool(c.parameter_db, "turretReturnToRest",
                &c.config.return_to_rest); break;
            default: return 0;
            }
            return 1;
        case 5: normalize_turret_config(&c.config); return 1;
        case 6:
            try { g_turret_classes[c.class_object] = c.config; }
            catch (...) {
                log_line("Could not retain a turret class policy");
                return 0;
            }
            return 1;
        case 7: {
            char message[384]{};
            std::snprintf(message, sizeof(message),
                "Registered turret ODF '%s': yaw %.1f..%.1f @ %.1f deg/s, "
                "pitch %.1f..%.1f @ %.1f deg/s",
                c.odf_name[0] ? c.odf_name : "<unknown>",
                c.config.limits.yaw_min_degrees,
                c.config.limits.yaw_max_degrees,
                c.config.limits.yaw_rate_degrees,
                c.config.limits.pitch_min_degrees,
                c.config.limits.pitch_max_degrees,
                c.config.limits.pitch_rate_degrees);
            log_line(message);
            return 1;
        }
        case 8: c.parent.odf_name = c.odf_name; return 1;
        case 9:
            c.index = arg;
            std::snprintf(c.turret_key, sizeof(c.turret_key), "turret%lu",
                          static_cast<unsigned long>(c.index));
            std::snprintf(c.hardpoint_key, sizeof(c.hardpoint_key),
                          "turretHardpoint%lu",
                          static_cast<unsigned long>(c.index));
            c.turret_odf.clear();
            c.hardpoint.clear();
            return 1;
        case 10:
            c.has_turret = read_parameter_string(
                c.parameter_db, c.turret_key, &c.turret_odf);
            return c.has_turret;
        case 11:
            c.has_hardpoint = read_parameter_string(
                c.parameter_db, c.hardpoint_key, &c.hardpoint);
            return c.has_hardpoint;
        case 12: {
            char message[320]{};
            std::snprintf(message, sizeof(message),
                "Skipping incomplete turret pair %lu on '%s' "
                "(%s is missing)",
                static_cast<unsigned long>(c.index),
                c.odf_name[0] ? c.odf_name : "<unknown>",
                c.has_turret ? c.hardpoint_key : c.turret_key);
            log_line(message);
            return 1;
        }
        case 13: normalize_odf_basename(&c.turret_odf); return 1;
        case 14: trim_string(&c.hardpoint); return 1;
        case 15: return !c.turret_odf.empty() && !c.hardpoint.empty();
        case 16:
            try {
                c.parent.mounts.push_back(MountClassConfig{
                    static_cast<std::uint32_t>(c.index),
                    std::move(c.turret_odf), std::move(c.hardpoint)});
            } catch (...) {
                log_line("Could not retain an indexed turret pair");
                return 0;
            }
            return 1;
        case 17: return c.parent.mounts.size();
        case 18:
            try {
                const std::size_t count = c.parent.mounts.size();
                g_parent_classes[c.class_object] = std::move(c.parent);
                char message[256]{};
                std::snprintf(message, sizeof(message),
                    "Registered %lu hull turret mount%s on '%s'",
                    static_cast<unsigned long>(count),
                    count == 1 ? "" : "s",
                    c.odf_name[0] ? c.odf_name : "<unknown>");
                log_line(message);
            } catch (...) {
                log_line("Could not retain a parent turret policy");
            }
            return 1;
        case 19: {
            std::string{}.swap(c.turret_odf);
            std::string{}.swap(c.hardpoint);
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        c.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

void register_class_policy(void* class_object, void* parameter_db) noexcept {
    TurretClassContext context{};
    context.class_object = class_object;
    context.parameter_db = parameter_db;
    a2fo_rs_stage8_turrets_register_class(&turret_class_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

struct TurretClassConstructContext {
    void* self;
    void* parent_class;
    void* parameter_db;
    std::uintptr_t result = 0;
    std::exception_ptr exception{};
};

std::uintptr_t turret_class_construct_step(void* opaque, std::uint32_t op,
                                         std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretClassConstructContext*>(opaque);
    switch (op) {
    case 0:
        context.result = a2fo_turret_call_thiscall_2(
            g_class_constructor_original, context.self,
            reinterpret_cast<std::uintptr_t>(context.parent_class),
            reinterpret_cast<std::uintptr_t>(context.parameter_db));
        return 1;
    case 1:
        try {
            register_class_policy(context.self, context.parameter_db);
            return 1;
        } catch (...) {
            context.exception = std::current_exception();
            return std::numeric_limits<std::uintptr_t>::max();
        }
    case 2:
        if (g_shield_class_observer) {
            g_shield_class_observer(context.self, context.parameter_db);
        }
        return 1;
    default: return 0;
    }
}

std::uintptr_t __attribute__((fastcall)) game_object_class_constructor_hook(
    void* self, void*, void* parent_class, void* parameter_db) noexcept {
    TurretClassConstructContext context{self, parent_class, parameter_db};
    a2fo_rs_stage8_turrets_class_construct(
        &turret_class_construct_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    return context.result;
}

struct TurretParentContext {
    void* object = nullptr;
    void* object_class = nullptr;
    const ParentClassConfig* configured = nullptr;
    ParentRuntime runtime{};
    ParentRuntime* result = nullptr;
};

std::uintptr_t turret_parent_step(void* opaque, std::uint32_t op,
                                std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretParentContext*>(opaque);
    switch (op) {
    case 0: return context.object && !object_expired(context.object);
    case 1: return object_handle(context.object);
    case 2: {
        const auto found = g_parents.find(static_cast<std::uint32_t>(arg));
        if (found == g_parents.end()) return 0;
        context.result = &found->second;
        return 1;
    }
    case 3:
        context.object_class = object_class(context.object);
        return 1;
    case 4: {
        const auto found = g_parent_classes.find(context.object_class);
        if (found == g_parent_classes.end()) return 0;
        context.configured = &found->second;
        context.runtime.object_class = context.object_class;
        return 1;
    }
    case 5: return context.configured->mounts.size();
    case 6:
        try {
            context.runtime.mounts.reserve(arg);
            return 1;
        } catch (...) { return 0; }
    case 7:
        try {
            context.runtime.mounts.push_back(
                MountRuntime{context.configured->mounts[arg]});
            return 1;
        } catch (...) { return 0; }
    case 8:
        try {
            const auto inserted = g_parents.emplace(
                static_cast<std::uint32_t>(arg), std::move(context.runtime));
            context.result = inserted.second ? &inserted.first->second : nullptr;
            return 1;
        } catch (...) { return 0; }
    case 9:
        log_line("Could not allocate per-object turret state");
        return 1;
    default: return 0;
    }
}

ParentRuntime* ensure_parent_runtime(void* parent) noexcept {
    TurretParentContext context{};
    context.object = parent;
    a2fo_rs_stage8_turrets_ensure_parent(&turret_parent_step, &context);
    return context.result;
}

struct TurretMountContext {
    ParentRuntime* parent = nullptr;
    std::uint32_t requested = 0;
    MountRuntime* result = nullptr;
};

std::uintptr_t turret_mount_step(void* opaque, std::uint32_t op,
                               std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretMountContext*>(opaque);
    switch (op) {
    case 0: return context.parent ? context.parent->mounts.size() : 0;
    case 1:
        return context.parent->mounts[arg].config.index == context.requested;
    case 2:
        context.result = &context.parent->mounts[arg];
        return 1;
    default: return 0;
    }
}

MountRuntime* find_mount(ParentRuntime* parent,
                         std::uint32_t index) noexcept {
    TurretMountContext context{parent, index, nullptr};
    a2fo_rs_stage8_turrets_find_mount(&turret_mount_step, &context);
    return context.result;
}

struct TurretHardpointContext {
    void* parent;
    MountRuntime* mount;
};

std::uintptr_t turret_hardpoint_step(void* opaque, std::uint32_t op,
                                   std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretHardpointContext*>(opaque);
    switch (op) {
    case 0: return context.parent && context.mount && g_armada;
    case 1: return context.mount->hardpoint_resolved;
    case 2: return context.mount->hardpoint_node != nullptr;
    case 3: context.mount->hardpoint_resolved = true; return 1;
    case 4: {
        void* class_pointer = object_class(context.parent);
        return a2fo_turret_call_thiscall_0(
            at(g_armada, kGameObjectClassGetHierarchyRootRva), class_pointer);
    }
    case 5:
        context.mount->hardpoint_node = reinterpret_cast<void*>(
            a2fo_turret_call_thiscall_1(
                at(g_armada, kSt3dNodeFindRecursiveRva),
                reinterpret_cast<void*>(arg),
                reinterpret_cast<std::uintptr_t>(
                    context.mount->config.hardpoint.c_str())));
        return 1;
    case 6: {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Turret %lu disabled: hardpoint '%s' was not found",
            static_cast<unsigned long>(context.mount->config.index),
            context.mount->config.hardpoint.c_str());
        log_line(message);
        context.mount->status = MountStatus::failed;
        return 1;
    }
    default: return 0;
    }
}

bool resolve_hardpoint(void* parent, MountRuntime* mount) noexcept {
    TurretHardpointContext context{parent, mount};
    return a2fo_rs_stage8_turrets_resolve_hardpoint(
        &turret_hardpoint_step, &context) != 0;
}

struct TurretTransformContext {
    void* parent;
    MountRuntime* mount;
    Matrix34* output;
};

std::uintptr_t turret_transform_step(void* opaque, std::uint32_t op,
                                   std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretTransformContext*>(opaque);
    switch (op) {
    case 0: return context.parent != nullptr;
    case 1: return context.mount != nullptr;
    case 2: return context.output != nullptr;
    case 3: return resolve_hardpoint(context.parent, context.mount);
    case 4:
        return a2fo_turret_call_thiscall_2(
            at(g_armada, kEntityGetWorldTransformRva), context.parent,
            reinterpret_cast<std::uintptr_t>(context.output),
            reinterpret_cast<std::uintptr_t>(context.mount->hardpoint_node));
    case 5: return reinterpret_cast<std::uintptr_t>(context.output);
    default: return 0;
    }
}

bool mount_world_transform(void* parent, MountRuntime* mount,
                           Matrix34* output) noexcept {
    TurretTransformContext context{parent, mount, output};
    return a2fo_rs_stage8_turrets_mount_world_transform(
        &turret_transform_step, &context) != 0;
}

struct TurretMissionContext {
    void* object;
    void* (__cdecl* get_current)() = nullptr;
    void* mission = nullptr;
    void* vtable = nullptr;
    void* add_object = nullptr;
};

std::uintptr_t turret_mission_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretMissionContext*>(opaque);
    switch (op) {
    case 0: return context.object && g_armada;
    case 1:
        context.get_current = reinterpret_cast<void* (__cdecl*)()>(
            at(g_armada, kAiMissionGetCurrentRva));
        return 1;
    case 2:
        context.mission = context.get_current
            ? context.get_current() : nullptr;
        return 1;
    case 3:
        context.vtable = read_at<void*>(context.mission, 0, nullptr);
        return 1;
    case 4:
        context.add_object = read_at<void*>(
            context.vtable, 0x18, nullptr);
        return 1;
    case 5: return context.mission && context.add_object;
    case 6:
        a2fo_turret_call_thiscall_1(context.add_object, context.mission,
            reinterpret_cast<std::uintptr_t>(context.object));
        return 1;
    default: return 0;
    }
}

void add_to_current_mission(void* object) noexcept {
    TurretMissionContext context{object};
    a2fo_rs_stage8_turrets_add_to_mission(&turret_mission_step, &context);
}

struct TurretExpireContext { void* craft; };

std::uintptr_t turret_expire_step(void* opaque, std::uint32_t op,
                                std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretExpireContext*>(opaque);
    switch (op) {
    case 0: return context.craft != nullptr;
    case 1: return object_expired(context.craft);
    case 2: return g_armada != nullptr;
    case 3:
        a2fo_turret_call_thiscall_0(
            at(g_armada, kCraftDoExpireRva), context.craft);
        return 1;
    default: return 0;
    }
}

void expire_craft(void* craft) noexcept {
    TurretExpireContext context{craft};
    a2fo_rs_stage8_turrets_expire_craft(&turret_expire_step, &context);
}

struct TurretConstructContext {
    const MountRuntime* mount;
    const Matrix34* transform;
    std::int32_t team;
    std::uint32_t parent_handle;
    void* turret_class = nullptr;
    char label[64]{};
    void* child = nullptr;
};

std::uintptr_t turret_construct_step(void* opaque, std::uint32_t op,
                                   std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretConstructContext*>(opaque);
    switch (op) {
    case 0:
        return g_armada && !context.mount->config.turret_odf.empty();
    case 1: {
        using FindClassFn = void* (__cdecl*)(const char*);
        const auto find_class = reinterpret_cast<FindClassFn>(
            at(g_armada, kGameObjectClassFindRva));
        context.turret_class = find_class
            ? find_class(context.mount->config.turret_odf.c_str()) : nullptr;
        return context.turret_class != nullptr;
    }
    case 2:
        std::snprintf(context.label, sizeof(context.label), "%s%08lX:%lu",
            kLinkedLabelPrefix,
            static_cast<unsigned long>(context.parent_handle),
            static_cast<unsigned long>(context.mount->config.index));
        return 1;
    case 3:
        context.child = reinterpret_cast<void*>(a2fo_turret_call_thiscall_4(
            at(g_armada, kGameObjectClassConstructRva), context.turret_class,
            reinterpret_cast<std::uintptr_t>(context.transform),
            static_cast<std::uintptr_t>(context.team), 0,
            reinterpret_cast<std::uintptr_t>(context.label)));
        return context.child != nullptr;
    case 4: add_to_current_mission(context.child); return 1;
    default: return 0;
    }
}

void* construct_turret(const MountRuntime& mount,
                       const Matrix34& transform, std::int32_t team,
                       std::uint32_t parent_handle) noexcept {
    TurretConstructContext context{&mount, &transform, team, parent_handle};
    a2fo_rs_stage8_turrets_construct_turret(
        &turret_construct_step, &context);
    return context.child;
}

void log_spawn_failure(void* parent, const MountRuntime& mount,
                       const char* reason) noexcept {
    char parent_name[128]{};
    copy_class_odf_name(object_class(parent), parent_name,
                        sizeof(parent_name));
    char message[384]{};
    std::snprintf(
        message, sizeof(message),
        "Turret %lu on '%s' was not created from '%s': %s",
        static_cast<unsigned long>(mount.config.index),
        parent_name[0] ? parent_name : "<unknown>",
        mount.config.turret_odf.c_str(), reason ? reason : "unknown error");
    log_line(message);
}

struct TurretSpawnContext {
    void* parent;
    ParentRuntime* runtime = nullptr;
    std::vector<MountRuntime>::iterator cursor{};
    std::vector<MountRuntime>::iterator end{};
    MountRuntime* mount = nullptr;
    std::uint32_t parent_handle = 0;
    std::int32_t team = 0;
    Matrix34 transform{};
    void* child = nullptr;
    std::uint32_t child_handle = 0;
    const TurretClassConfig* turret_config = nullptr;
    ChildRuntime child_runtime{};
};

std::uintptr_t turret_spawn_step(void* opaque, std::uint32_t op,
                               std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretSpawnContext*>(opaque);
    switch (op) {
    case 0:
        context.runtime = ensure_parent_runtime(context.parent);
        if (!context.runtime) return 0;
        context.cursor = context.runtime->mounts.begin();
        context.end = context.runtime->mounts.end();
        return 1;
    case 1: context.parent_handle = object_handle(context.parent); return 1;
    case 2:
        context.team = read_at<std::int32_t>(
            context.parent, kObjectTeamOffset, 0);
        return 1;
    case 3:
        if (context.cursor == context.end) return 0;
        context.mount = &*context.cursor;
        return 1;
    case 4: return static_cast<std::uint32_t>(context.mount->status);
    case 6: return find_entity(context.mount->child_handle) != nullptr;
    case 7:
        context.mount->status = MountStatus::destroyed;
        context.mount->child_handle = 0;
        return 1;
    case 8: ++context.cursor; return 1;
    case 9:
        context.transform = {};
        return mount_world_transform(
            context.parent, context.mount, &context.transform);
    case 10: context.mount->status = MountStatus::failed; return 1;
    case 11:
        context.child = construct_turret(*context.mount, context.transform,
            context.team, context.parent_handle);
        return context.child != nullptr;
    case 12:
        log_spawn_failure(context.parent, *context.mount,
            "ODF class was not found or construction failed");
        return 1;
    case 13:
        context.child_handle = object_handle(context.child);
        return context.child_handle;
    case 14: {
        const auto found = g_turret_classes.find(object_class(context.child));
        context.turret_config = found == g_turret_classes.end()
            ? nullptr : &found->second;
        return context.turret_config != nullptr;
    }
    case 15:
        log_spawn_failure(context.parent, *context.mount,
            "referenced ODF must use classLabel = \"turret\"");
        return 1;
    case 16: expire_craft(context.child); return 1;
    case 17:
        context.child_runtime = {};
        context.child_runtime.parent_handle = context.parent_handle;
        context.child_runtime.mount_index = context.mount->config.index;
        context.child_runtime.current = context.turret_config->rest;
        return 1;
    case 18:
        try {
            g_children[context.child_handle] = context.child_runtime;
            context.mount->child_handle = context.child_handle;
            context.mount->status = MountStatus::active;
            return 1;
        } catch (...) { return 0; }
    case 19:
        log_spawn_failure(context.parent, *context.mount,
            "runtime state allocation failed");
        return 1;
    case 20:
        // Prevent an independent combat tick for the newly constructed child.
        prepare_linked_turret_for_simulation(context.child, context.child_handle);
        return 1;
    case 21: {
        const Matrix34 initial = a2fo::turrets::compose_turret_transform(
            context.transform, context.child_runtime.current);
        a2fo_turret_call_thiscall_1(
            at(g_armada, kGameObjectSetTransformRva), context.child,
            reinterpret_cast<std::uintptr_t>(&initial));
        return 1;
    }
    default: return 0;
    }
}

void spawn_pending_turrets(void* parent) noexcept {
    TurretSpawnContext context{};
    context.parent = parent;
    a2fo_rs_stage8_turrets_spawn_pending(&turret_spawn_step, &context,
        static_cast<std::uint32_t>(MountStatus::active),
        static_cast<std::uint32_t>(MountStatus::pending));
}

struct TurretLabelContext {
    const char* label;
    std::uint32_t* parent_handle;
    std::uint32_t* mount_index;
    unsigned long parsed_parent = 0;
    unsigned long parsed_index = 0;
    char trailing = '\0';
};

std::uintptr_t turret_label_step(void* opaque, std::uint32_t op,
                               std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretLabelContext*>(opaque);
    switch (op) {
    case 0:
        return context.label && context.parent_handle && context.mount_index;
    case 1:
        return std::strncmp(context.label, kLinkedLabelPrefix,
            sizeof(kLinkedLabelPrefix) - 1) == 0;
    case 2:
        return std::sscanf(context.label + sizeof(kLinkedLabelPrefix) - 1,
            "%8lx:%lu%c", &context.parsed_parent,
            &context.parsed_index, &context.trailing);
    case 3: return context.parsed_parent;
    case 4: return context.parsed_index;
    case 5:
        *context.parent_handle = static_cast<std::uint32_t>(
            context.parsed_parent);
        *context.mount_index = static_cast<std::uint32_t>(
            context.parsed_index);
        return 1;
    default: return 0;
    }
}

bool parse_linked_label(const char* label, std::uint32_t* parent_handle,
                        std::uint32_t* mount_index) noexcept {
    TurretLabelContext context{label, parent_handle, mount_index};
    return a2fo_rs_stage8_turrets_parse_label(
        &turret_label_step, &context, kMaximumTurrets) != 0;
}

struct TurretLoadedContext {
    void* child;
    const char* label = nullptr;
    std::uint32_t parent_handle = 0;
    std::uint32_t mount_index = 0;
    std::uint32_t child_handle = 0;
    ChildRuntime runtime{};
    void* parent = nullptr;
    ParentRuntime* parent_runtime = nullptr;
    MountRuntime* mount = nullptr;
};

std::uintptr_t turret_loaded_step(void* opaque, std::uint32_t op,
                                std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretLoadedContext*>(opaque);
    switch (op) {
    case 0: return context.child != nullptr;
    case 1: return g_turret_classes.find(object_class(context.child)) !=
        g_turret_classes.end();
    case 2:
        context.label = read_at<const char*>(
            context.child, kObjectLabelOffset, nullptr);
        return 1;
    case 3:
        return parse_linked_label(context.label, &context.parent_handle,
            &context.mount_index);
    case 4:
        if (context.child_handle == 0) {
            context.child_handle = object_handle(context.child);
        }
        return context.child_handle;
    case 5: {
        context.runtime = {};
        context.runtime.parent_handle = context.parent_handle;
        context.runtime.mount_index = context.mount_index;
        const auto found = g_turret_classes.find(object_class(context.child));
        if (found != g_turret_classes.end()) {
            context.runtime.current = found->second.rest;
        }
        return 1;
    }
    case 6:
        context.parent = find_entity(context.parent_handle);
        return context.parent != nullptr;
    case 7:
        try {
            g_children[context.child_handle] = context.runtime;
            return 1;
        } catch (...) { return 0; }
    case 8:
        log_line("Could not restore a loaded turret relationship");
        return 1;
    case 9: expire_craft(context.child); return 1;
    case 10:
        context.parent_runtime = ensure_parent_runtime(context.parent);
        return 1;
    case 11:
        context.mount = find_mount(context.parent_runtime,
            context.mount_index);
        return context.mount != nullptr;
    case 12:
        log_line("Loaded turret could not resolve its parent mount; expiring it");
        return 1;
    case 13: g_children.erase(context.child_handle); return 1;
    case 14: return static_cast<std::uint32_t>(context.mount->status);
    case 15: return context.mount->child_handle;
    case 16:
        log_line("Duplicate loaded turret detected; expiring the duplicate");
        return 1;
    case 17:
        context.mount->child_handle = context.child_handle;
        context.mount->status = MountStatus::active;
        return 1;
    default: return 0;
    }
}

void attach_loaded_turret(void* child) noexcept {
    TurretLoadedContext context{};
    context.child = child;
    a2fo_rs_stage8_turrets_attach_loaded(&turret_loaded_step, &context,
        static_cast<std::uint32_t>(MountStatus::active));
}

struct TurretOwnershipContext {
    void* child;
    void* parent;
    std::int32_t parent_team = 0;
    void* parent_race = nullptr;
};

std::uintptr_t turret_ownership_step(void* opaque, std::uint32_t op,
                                   std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretOwnershipContext*>(opaque);
    switch (op) {
    case 0: return context.child && context.parent && g_armada;
    case 1:
        context.parent_team = read_at<std::int32_t>(
            context.parent, kObjectTeamOffset, 0);
        return static_cast<std::uint32_t>(context.parent_team);
    case 2:
        return static_cast<std::uint32_t>(read_at<std::int32_t>(
            context.child, kObjectTeamOffset, 0));
    case 3:
        context.parent_race = read_at<void*>(
            context.parent, kObjectRaceOffset, nullptr);
        return reinterpret_cast<std::uintptr_t>(context.parent_race);
    case 4:
        return reinterpret_cast<std::uintptr_t>(read_at<void*>(
            context.child, kObjectRaceOffset, nullptr));
    case 5:
        a2fo_turret_call_thiscall_2(
            at(g_armada, kGameObjectSwapRaceAndTeamRva), context.child,
            reinterpret_cast<std::uintptr_t>(context.parent_race),
            static_cast<std::uintptr_t>(context.parent_team));
        return 1;
    case 6:
        a2fo_turret_call_thiscall_1(
            at(g_armada, kGameObjectSwapTeamRva), context.child,
            static_cast<std::uintptr_t>(context.parent_team));
        return 1;
    default: return 0;
    }
}

void sync_child_ownership(void* child, void* parent) noexcept {
    TurretOwnershipContext context{child, parent};
    a2fo_rs_stage8_turrets_sync_ownership(
        &turret_ownership_step, &context);
}

std::int32_t get_craft_policy(
    void* craft, std::uintptr_t getter_rva) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(a2fo_turret_call_thiscall_0(
            at(g_armada, getter_rva), craft)));
}

struct TurretCombatPolicyContext {
    void* child;
    void* parent;
    std::int32_t parent_alert = 0;
    std::int32_t parent_autonomy = 0;
};

std::uintptr_t turret_combat_policy_step(void* opaque, std::uint32_t op,
                                       std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretCombatPolicyContext*>(opaque);
    switch (op) {
    case 0: return context.child && context.parent && g_armada;
    case 1:
        context.parent_alert = get_craft_policy(
            context.parent, kCraftGetAlertStatusRva);
        return static_cast<std::uint32_t>(context.parent_alert);
    case 2:
        return static_cast<std::uint32_t>(get_craft_policy(
            context.child, kCraftGetAlertStatusRva));
    case 3:
        a2fo_turret_call_thiscall_1(
            at(g_armada, kCraftSetAlertStatusRva), context.child,
            static_cast<std::uint32_t>(context.parent_alert));
        return 1;
    case 4:
        context.parent_autonomy = get_craft_policy(
            context.parent, kCraftGetSpecialWeaponAutonomyRva);
        return static_cast<std::uint32_t>(context.parent_autonomy);
    case 5:
        return static_cast<std::uint32_t>(get_craft_policy(
            context.child, kCraftGetSpecialWeaponAutonomyRva));
    case 6:
        a2fo_turret_call_thiscall_1(
            at(g_armada, kCraftSetSpecialWeaponAutonomyRva), context.child,
            static_cast<std::uint32_t>(context.parent_autonomy));
        return 1;
    default: return 0;
    }
}

void sync_child_combat_policy(void* child, void* parent) noexcept {
    TurretCombatPolicyContext context{child, parent};
    a2fo_rs_stage8_turrets_sync_combat_policy(
        &turret_combat_policy_step, &context);
}

struct TurretAttackOrderContext {
    void* parent;
    AttackOrder order{};
    const void* command = nullptr;
    std::uint32_t command_id = 0;
    std::uint32_t target_form = 0;
    std::uint32_t target_handle = 0;
    void* target = nullptr;
};

std::uintptr_t turret_attack_order_step(void* opaque, std::uint32_t op,
                                      std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretAttackOrderContext*>(opaque);
    switch (op) {
    case 0: return context.parent && g_armada;
    case 1:
        context.command = reinterpret_cast<const void*>(
            a2fo_turret_call_thiscall_0(
                at(g_armada, kGameObjectGetCurrentCommandRva),
                context.parent));
        return 1;
    case 2:
        context.command_id = read_at<std::uint32_t>(
            context.command, kCommandIdOffset, 0);
        return 1;
    case 3:
        context.target_form = read_at<std::uint32_t>(
            context.command, kCommandTargetFormOffset, 0);
        return 1;
    case 4:
        context.target_handle = read_at<std::uint32_t>(
            context.command, kCommandTargetHandleOffset, 0);
        return 1;
    case 5:
        context.target = find_entity(context.target_handle);
        return reinterpret_cast<std::uintptr_t>(context.target);
    case 6: return !object_expired(context.target);
    case 7:
        context.order.target_handle =
            a2fo::turrets::attack_order_target_handle(
                context.command_id, context.target_form,
                context.target_handle, arg != 0);
        return context.order.target_handle;
    case 8: context.order.target = context.target; return 1;
    case 9:
        context.order.extra = read_at<std::uint32_t>(
            context.command, kCommandExtraOffset, 0);
        return 1;
    case 10:
        context.order.flags = read_at<std::uint8_t>(
            context.command, kCommandFlagsOffset, 0);
        return 1;
    default: return 0;
    }
}

AttackOrder current_attack_order(void* parent) noexcept {
    TurretAttackOrderContext context{};
    context.parent = parent;
    a2fo_rs_stage8_turrets_current_attack_order(
        &turret_attack_order_step, &context);
    return context.order;
}

struct TurretChildAttackContext {
    void* child;
    const AttackOrder* order;
    const void* command = nullptr;
};

std::uintptr_t turret_child_attack_step(void* opaque, std::uint32_t op,
                                      std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretChildAttackContext*>(opaque);
    switch (op) {
    case 0:
        context.command = reinterpret_cast<const void*>(
            a2fo_turret_call_thiscall_0(
                at(g_armada, kGameObjectGetCurrentCommandRva),
                context.child));
        return 1;
    case 1: return read_at<std::uint32_t>(
        context.command, kCommandIdOffset, 0);
    case 2: return read_at<std::uint32_t>(
        context.command, kCommandTargetFormOffset, 0);
    case 3: return read_at<std::uint32_t>(
        context.command, kCommandTargetHandleOffset, 0) ==
        context.order->target_handle;
    case 4: return read_at<std::uint32_t>(
        context.command, kCommandExtraOffset, 0) == context.order->extra;
    case 5: return read_at<std::uint8_t>(
        context.command, kCommandFlagsOffset, 0) == context.order->flags;
    default: return 0;
    }
}

bool child_has_attack_order(
    void* child, const AttackOrder& order) noexcept {
    TurretChildAttackContext context{child, &order};
    return a2fo_rs_stage8_turrets_child_has_attack_order(
        &turret_child_attack_step, &context,
        a2fo::turrets::kAttackCommand,
        a2fo::turrets::kObjectCommandTargetForm) != 0;
}

struct TurretMirrorOrderContext {
    void* child;
    const AttackOrder* order;
    std::uint32_t previous_target_handle;
};

std::uintptr_t turret_mirror_order_step(void* opaque, std::uint32_t op,
                                      std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretMirrorOrderContext*>(opaque);
    switch (op) {
    case 0: return context.order->target_handle;
    case 1: return reinterpret_cast<std::uintptr_t>(context.order->target);
    case 2: return child_has_attack_order(context.child, *context.order);
    case 3:
        a2fo_turret_call_thiscall_4(
            at(g_armada, kGameObjectSetObjectCommandRva), context.child,
            a2fo::turrets::kAttackCommand,
            reinterpret_cast<std::uintptr_t>(context.order->target),
            context.order->extra, context.order->flags);
        return 1;
    case 4: return context.previous_target_handle;
    case 5:
        a2fo_turret_call_thiscall_4(
            at(g_armada, kGameObjectSetTargetlessCommandRva), context.child,
            0, 0, 0, 0);
        return 1;
    default: return 0;
    }
}

void mirror_attack_order(
    void* child, const AttackOrder& order,
    std::uint32_t previous_target_handle) noexcept {
    TurretMirrorOrderContext context{child, &order, previous_target_handle};
    a2fo_rs_stage8_turrets_mirror_attack_order(
        &turret_mirror_order_step, &context);
}

struct TurretInheritedOrderContext {
    void* child;
    std::uint32_t child_handle;
    ChildRuntime* child_runtime;
    void* parent;
    std::uint32_t visual_handle = 0;
    AttackOrder order{};
    a2fo::turrets::OrderTargetState next{};
};

std::uintptr_t turret_inherited_order_step(void* opaque, std::uint32_t op,
                                         std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretInheritedOrderContext*>(opaque);
    switch (op) {
    case 0: return context.child_runtime != nullptr;
    case 1: {
        const auto found = g_targets.find(context.child_handle);
        context.visual_handle = found == g_targets.end() ? 0 : found->second;
        return 1;
    }
    case 2: context.order = current_attack_order(context.parent); return 1;
    case 3:
        mirror_attack_order(context.child, context.order,
            context.child_runtime->ordered_target_handle);
        return 1;
    case 4:
        context.next = a2fo::turrets::update_order_target(
            context.child_runtime->ordered_target_handle,
            context.order.target_handle, context.visual_handle);
        return 1;
    case 5:
        context.child_runtime->ordered_target_handle =
            context.next.ordered_target_handle;
        return 1;
    case 6:
        return context.next.visual_target_handle == context.visual_handle;
    case 7:
        try {
            if (context.next.visual_target_handle != 0) {
                g_targets[context.child_handle] =
                    context.next.visual_target_handle;
            } else {
                g_targets.erase(context.child_handle);
            }
            return 1;
        } catch (...) { return 0; }
    case 8: g_targets.erase(context.child_handle); return 1;
    default: return 0;
    }
}

void update_inherited_order_target(
    void* child, std::uint32_t child_handle,
    ChildRuntime* child_runtime, void* parent) noexcept {
    TurretInheritedOrderContext context{
        child, child_handle, child_runtime, parent};
    a2fo_rs_stage8_turrets_update_inherited_order(
        &turret_inherited_order_step, &context);
}

struct TurretPrepareContext {
    void* child;
    std::uint32_t child_handle;
    ChildRuntime* child_runtime = nullptr;
    void* parent = nullptr;
};

std::uintptr_t turret_prepare_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretPrepareContext*>(opaque);
    switch (op) {
    case 0: {
        const auto found = g_children.find(context.child_handle);
        if (found == g_children.end()) return 0;
        context.child_runtime = &found->second;
        return 1;
    }
    case 1:
        context.parent = find_entity(context.child_runtime->parent_handle);
        return context.parent != nullptr;
    case 2: return object_expired(context.parent);
    case 3: sync_child_ownership(context.child, context.parent); return 1;
    case 4: sync_child_combat_policy(context.child, context.parent); return 1;
    case 5:
        update_inherited_order_target(context.child, context.child_handle,
            context.child_runtime, context.parent);
        return 1;
    default: return 0;
    }
}

void prepare_linked_turret_for_simulation(
    void* child, std::uint32_t child_handle) noexcept {
    TurretPrepareContext context{child, child_handle};
    a2fo_rs_stage8_turrets_prepare_linked(&turret_prepare_step, &context);
}

struct TurretUpdateContext {
    void* child;
    std::uint32_t child_handle;
    float elapsed_seconds;
    ChildRuntime* child_runtime = nullptr;
    void* parent = nullptr;
    ParentRuntime* parent_runtime = nullptr;
    MountRuntime* mount = nullptr;
    Matrix34 mount_transform{};
    const TurretClassConfig* config = nullptr;
    AimAngles desired{};
    void* target = nullptr;
    const Matrix34* target_transform = nullptr;
};

std::uintptr_t turret_update_step(void* opaque, std::uint32_t op,
                                std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretUpdateContext*>(opaque);
    switch (op) {
    case 0: {
        const auto found = g_children.find(context.child_handle);
        if (found == g_children.end()) return 0;
        context.child_runtime = &found->second;
        return 1;
    }
    case 1:
        context.parent = find_entity(context.child_runtime->parent_handle);
        return context.parent != nullptr;
    case 2: return object_expired(context.parent);
    case 3:
        context.parent_runtime = ensure_parent_runtime(context.parent);
        return 1;
    case 4:
        context.mount = find_mount(context.parent_runtime,
            context.child_runtime->mount_index);
        return 1;
    case 5:
        return context.mount && context.mount->status == MountStatus::active &&
            context.mount->child_handle != 0 &&
            context.mount->child_handle != context.child_handle;
    case 6:
        if (context.mount) {
            context.mount->child_handle = context.child_handle;
            context.mount->status = MountStatus::active;
        }
        return 1;
    case 7:
        context.mount_transform = {};
        return context.mount && mount_world_transform(context.parent,
            context.mount, &context.mount_transform);
    case 8: sync_child_ownership(context.child, context.parent); return 1;
    case 9: {
        const auto found = g_turret_classes.find(read_live_at<void*>(
            context.child, kObjectClassOffset, nullptr));
        context.config = found == g_turret_classes.end()
            ? nullptr : &found->second;
        return context.config != nullptr;
    }
    case 10: context.desired = context.child_runtime->current; return 1;
    case 11: {
        const auto found = g_targets.find(context.child_handle);
        context.target = found == g_targets.end()
            ? nullptr : find_entity(found->second);
        return reinterpret_cast<std::uintptr_t>(context.target);
    }
    case 12: return object_expired(context.target);
    case 13:
        context.target_transform = reinterpret_cast<const Matrix34*>(
            a2fo_turret_call_thiscall_0(
                at(g_armada, kEntityGetTransformRva), context.target));
        return context.target_transform != nullptr;
    case 14: {
        const float position[3]{
            context.target_transform->values[9],
            context.target_transform->values[10],
            context.target_transform->values[11],
        };
        context.desired = a2fo::turrets::calculate_aim_angles(
            context.mount_transform, position, context.config->limits,
            context.child_runtime->current);
        return 1;
    }
    case 15: return context.config->return_to_rest;
    case 16: context.desired = context.config->rest; return 1;
    case 17:
        context.child_runtime->current = a2fo::turrets::advance_aim_angles(
            context.child_runtime->current, context.desired,
            context.config->limits, context.elapsed_seconds);
        return 1;
    case 18: {
        const Matrix34 transform = a2fo::turrets::compose_turret_transform(
            context.mount_transform, context.child_runtime->current);
        a2fo_turret_call_thiscall_1(
            at(g_armada, kGameObjectSetTransformRva), context.child,
            reinterpret_cast<std::uintptr_t>(&transform));
        return 1;
    }
    case 19:
        log_line("Duplicate loaded turret detected; expiring the duplicate");
        return 1;
    case 20: expire_craft(context.child); return 1;
    default: return 0;
    }
}

void update_linked_turret(void* child, std::uint32_t child_handle,
                          float elapsed_seconds) noexcept {
    TurretUpdateContext context{child, child_handle, elapsed_seconds};
    a2fo_rs_stage8_turrets_update_linked(&turret_update_step, &context);
}

struct TurretUnlinkContext {
    std::uint32_t child_handle;
    ChildRuntime* child = nullptr;
    ParentRuntime* parent = nullptr;
    MountRuntime* mount = nullptr;
};

std::uintptr_t turret_unlink_step(void* opaque, std::uint32_t op,
                                std::uintptr_t) noexcept {
    auto& context = *static_cast<TurretUnlinkContext*>(opaque);
    switch (op) {
    case 0: {
        const auto found = g_children.find(context.child_handle);
        if (found == g_children.end()) return 0;
        context.child = &found->second;
        return 1;
    }
    case 1: {
        const auto found = g_parents.find(context.child->parent_handle);
        if (found == g_parents.end()) return 0;
        context.parent = &found->second;
        return 1;
    }
    case 2:
        context.mount = find_mount(context.parent,
            context.child->mount_index);
        return context.mount != nullptr;
    case 3: return context.mount->child_handle == context.child_handle;
    case 4:
        context.mount->child_handle = 0;
        context.mount->status = MountStatus::destroyed;
        return 1;
    case 5: g_targets.erase(context.child_handle); return 1;
    case 6: g_children.erase(context.child_handle); return 1;
    default: return 0;
    }
}

void unlink_child(std::uint32_t child_handle) noexcept {
    TurretUnlinkContext context{child_handle, nullptr, nullptr, nullptr};
    a2fo_rs_stage8_turrets_unlink_child(&turret_unlink_step, &context);
}

struct TurretCleanupContext {
    std::uint32_t parent_handle;
    ParentRuntime* parent = nullptr;
    std::vector<std::uint32_t> children;
};

std::uintptr_t turret_cleanup_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretCleanupContext*>(opaque);
    switch (op) {
    case 0: {
        const auto found = g_parents.find(context.parent_handle);
        if (found == g_parents.end()) return 0;
        context.parent = &found->second;
        return 1;
    }
    case 1: return context.parent->mounts.size();
    case 2: return context.parent->mounts[arg].child_handle;
    case 3:
        try {
            context.children.push_back(static_cast<std::uint32_t>(arg));
            return 1;
        } catch (...) { return 0; }
    case 4: g_parents.erase(context.parent_handle); return 1;
    case 5: return context.children.size();
    case 6: return context.children[arg];
    case 7:
        g_children.erase(static_cast<std::uint32_t>(arg));
        g_targets.erase(static_cast<std::uint32_t>(arg));
        return 1;
    case 8: expire_craft(find_entity(static_cast<std::uint32_t>(arg))); return 1;
    default: return 0;
    }
}

void cleanup_parent(std::uint32_t parent_handle) noexcept {
    TurretCleanupContext context{parent_handle, nullptr, {}};
    a2fo_rs_stage8_turrets_cleanup_parent(&turret_cleanup_step, &context);
}

struct TurretCraftEventContext {
    const A2FO_CraftEvent* event;
    void* craft = nullptr;
};

std::uintptr_t turret_craft_event_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretCraftEventContext*>(opaque);
    switch (op) {
    case 0: return context.event != nullptr;
    case 1: return context.event->struct_size >= sizeof(*context.event);
    case 2: return g_runtime_ready;
    case 3:
        context.craft = context.event->craft;
        return context.craft != nullptr;
    case 4: return context.event->kind;
    case 5: return object_handle(context.craft);
    case 6: cleanup_parent(static_cast<std::uint32_t>(arg)); return 1;
    case 7: unlink_child(static_cast<std::uint32_t>(arg)); return 1;
    case 8: return !g_parent_classes.empty() || !g_turret_classes.empty();
    case 9: attach_loaded_turret(context.craft); return 1;
    case 10: return !g_children.empty() || !g_parent_classes.empty();
    case 11:
        return read_live_at<std::uint8_t>(
            context.craft, kObjectExpiredOffset, 1) == 0;
    case 12:
        return read_live_at<std::uint32_t>(
            context.craft, kObjectHandleOffset, 0);
    case 13:
        return g_children.find(static_cast<std::uint32_t>(arg)) !=
            g_children.end();
    case 14:
        prepare_linked_turret_for_simulation(
            context.craft, static_cast<std::uint32_t>(arg));
        return 1;
    case 15:
        update_linked_turret(context.craft,
            static_cast<std::uint32_t>(arg), context.event->elapsed_seconds);
        return 1;
    case 16:
        return g_parent_classes.find(read_live_at<void*>(
            context.craft, kObjectClassOffset, nullptr)) !=
            g_parent_classes.end();
    case 17: spawn_pending_turrets(context.craft); return 1;
    default: return 0;
    }
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    TurretCraftEventContext context{event, nullptr};
    a2fo_rs_stage8_turrets_craft_event(
        &turret_craft_event_step, &context, A2FO_CRAFT_EVENT_CLEANUP,
        A2FO_CRAFT_EVENT_POST_LOAD, A2FO_CRAFT_EVENT_SIMULATE_PRE,
        A2FO_CRAFT_EVENT_SIMULATE_POST);
}

struct TurretOwnerContext {
    void* weapon = nullptr;
    void* owner = nullptr;
};

std::uintptr_t turret_owner_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& c = *static_cast<TurretOwnerContext*>(opaque);
    switch (op) {
    case 0: return g_runtime_ready && c.weapon && g_armada;
    case 1:
        c.owner = reinterpret_cast<void*>(a2fo_turret_call_thiscall_0(
            at(g_armada, kWeaponGetOwnerRva), c.weapon));
        return 1;
    default: return 0;
    }
}

void* weapon_owner(void* weapon) noexcept {
    TurretOwnerContext context{weapon};
    a2fo_rs_stage8_turrets_weapon_owner(&turret_owner_step, &context);
    return context.owner;
}

struct TurretTrackContext {
    void* owner = nullptr;
    const void* target = nullptr;
    std::uint32_t owner_handle = 0;
    std::uint32_t target_handle = 0;
};

std::uintptr_t turret_track_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& c = *static_cast<TurretTrackContext*>(opaque);
    switch (op) {
    case 0:
        return c.owner && g_turret_classes.find(object_class(c.owner)) !=
            g_turret_classes.end();
    case 1: c.owner_handle = object_handle(c.owner); return 1;
    case 2: return c.owner_handle != 0;
    case 3: c.target_handle = object_handle(c.target); return 1;
    case 4: return c.target_handle != 0;
    case 5:
        try { g_targets[c.owner_handle] = c.target_handle; }
        catch (...) { g_targets.erase(c.owner_handle); }
        return 1;
    case 6: g_targets.erase(c.owner_handle); return 1;
    default: return 0;
    }
}

void track_weapon_target_for_owner(
    void* owner, const void* target) noexcept {
    TurretTrackContext context{owner, target};
    a2fo_rs_stage8_turrets_track_target(&turret_track_step, &context);
}

struct TurretInheritedContext {
    void* owner = nullptr;
    std::uint32_t owner_handle = 0;
    std::unordered_map<std::uint32_t, ChildRuntime>::iterator child{};
    void* target = nullptr;
    const void* result = nullptr;
};

std::uintptr_t turret_inherited_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t) noexcept {
    auto& c = *static_cast<TurretInheritedContext*>(opaque);
    switch (op) {
    case 0: c.owner_handle = object_handle(c.owner); return 1;
    case 1:
        c.child = g_children.find(c.owner_handle);
        return c.child != g_children.end();
    case 2: return c.child->second.ordered_target_handle != 0;
    case 3: c.target = find_entity(c.child->second.ordered_target_handle); return 1;
    case 4: return c.target != nullptr;
    case 5: return object_expired(c.target);
    case 6: c.result = c.target; return 1;
    default: return 0;
    }
}

const void* inherited_order_target_for_owner(void* owner) noexcept {
    TurretInheritedContext context{};
    context.owner = owner;
    a2fo_rs_stage8_turrets_inherited_target(
        &turret_inherited_step, &context);
    return context.result;
}

struct TurretWeaponContext {
    const A2FO_WeaponTriggerEvent* event = nullptr;
    void* weapon = nullptr;
    const void* target = nullptr;
    void* owner = nullptr;
    const void* ordered = nullptr;
    const void* effective = nullptr;
    A2FO_Stage8TurretTargetHandles handles{};
    std::exception_ptr exception{};
};

std::uintptr_t turret_weapon_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& c = *static_cast<TurretWeaponContext*>(opaque);
    if (c.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
        switch (op) {
        case 0:
            return c.event && c.event->struct_size >= sizeof(*c.event);
        case 1: return c.event->kind;
        case 2: c.weapon = c.event->weapon; return 1;
        case 3: c.target = c.event->target; return 1;
        case 4: c.owner = weapon_owner(c.weapon); return 1;
        case 5:
            c.ordered = inherited_order_target_for_owner(c.owner);
            return c.ordered != nullptr;
        case 6: c.handles.target = object_handle(c.target); return 1;
        case 7: c.handles.ordered = object_handle(c.ordered); return 1;
        case 9:
            track_weapon_target_for_owner(c.owner, c.ordered);
            return 1;
        case 10:
            track_weapon_target_for_owner(c.owner, c.target);
            return 1;
        default: return 0;
        }
    } catch (...) {
        c.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

bool A2FO_CALL weapon_trigger_handler(
    const A2FO_WeaponTriggerEvent* event, void*) {
    TurretWeaponContext context{};
    context.event = event;
    const bool result = a2fo_rs_stage8_turrets_weapon_trigger(
        &turret_weapon_step, &context, &context.handles,
        A2FO_WEAPON_TRIGGER_COMMITTED, A2FO_WEAPON_TRIGGER_PRECHECK) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

struct TurretSetTargetContext {
    void* weapon = nullptr;
    const void* target = nullptr;
    void* owner = nullptr;
    const void* ordered = nullptr;
    const void* effective = nullptr;
};

std::uintptr_t turret_set_target_step(void* opaque, std::uint32_t op,
                                      std::uintptr_t arg) noexcept {
    auto& c = *static_cast<TurretSetTargetContext*>(opaque);
    try {
        switch (op) {
        case 0: c.owner = weapon_owner(c.weapon); return 1;
        case 1:
            c.ordered = inherited_order_target_for_owner(c.owner);
            return c.ordered != nullptr;
        case 2: c.effective = arg != 0 ? c.ordered : c.target; return 1;
        case 3:
            a2fo_turret_call_thiscall_1(
                g_weapon_set_target_hook.gateway, c.weapon,
                reinterpret_cast<std::uintptr_t>(c.effective));
            return 1;
        case 4:
            track_weapon_target_for_owner(c.owner, c.effective);
            return 1;
        default: return 0;
        }
    } catch (...) {
        std::terminate();
    }
}

void __attribute__((fastcall)) weapon_set_target_hook(
    void* weapon, void*, const void* target) noexcept {
    TurretSetTargetContext context{weapon, target};
    a2fo_rs_stage8_turrets_set_target(&turret_set_target_step, &context);
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(g_armada, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

template <std::size_t Size>
bool checked_signature(const char* name, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    if (signature_matches(rva, expected)) return true;
    char actual[Size * 3 + 1]{};
    const auto* address = static_cast<const std::uint8_t*>(at(g_armada, rva));
    if (readable_range(address, Size)) {
        std::size_t used = 0;
        for (std::size_t index = 0; index < Size && used < sizeof(actual);
             ++index) {
            const int written = std::snprintf(
                actual + used, sizeof(actual) - used,
                index == 0 ? "%02X" : " %02X", address[index]);
            if (written <= 0) break;
            used += static_cast<std::size_t>(written);
        }
    } else {
        std::snprintf(actual, sizeof(actual), "<unreadable>");
    }
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "%s signature mismatch at Armada RVA 0x%08lX: %s",
                  name ? name : "Turret dependency",
                  static_cast<unsigned long>(rva), actual);
    log_line(message);
    return false;
}

void* existing_detour_destination(const void* site,
                                  std::size_t* patch_length) noexcept {
    if (patch_length) *patch_length = 0;
    if (!site || !readable_range(site, 5)) return nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(site);
    if (bytes[0] == 0xe9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        if (patch_length) *patch_length = 5;
        return const_cast<std::uint8_t*>(bytes + 5 + displacement);
    }
    // Fleet Operations' runtime hooker uses an absolute six-byte
    // `push handler; ret` transfer so its hooks do not depend on module
    // placement within rel32 range.
    if (readable_range(site, 6) && bytes[0] == 0x68 && bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

template <std::size_t HandlerSize>
bool supported_fleet_ops_detour(
    std::uintptr_t armada_rva, std::uintptr_t handler_rva,
    const std::uint8_t (&handler_signature)[HandlerSize],
    void** destination, std::size_t* patch_length) noexcept {
    void* resolved = existing_detour_destination(
        at(g_armada, armada_rva), patch_length);
    void* expected = at(g_fleet_ops, handler_rva);
    const bool supported = resolved == expected &&
        readable_range(expected, HandlerSize) &&
        std::memcmp(expected, handler_signature, HandlerSize) == 0;
    if (destination) *destination = supported ? resolved : nullptr;
    return supported;
}

bool class_constructor_site_supported() noexcept {
    if (signature_matches(kGameObjectClassConstructorRva,
                          kExpectedGameObjectClassConstructor)) {
        return true;
    }
    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (supported_fleet_ops_detour(
            kGameObjectClassConstructorRva,
            kFoGameObjectClassConstructorHandlerRva,
            kExpectedFoGameObjectClassConstructorHandler,
            &destination, &patch_length)) {
        return true;
    }
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "GameObjectClass constructor has neither the stock prologue nor "
        "Fleet Ops' supported detour (destination=%p, expected=%p)",
        existing_detour_destination(
            at(g_armada, kGameObjectClassConstructorRva), nullptr),
        at(g_fleet_ops, kFoGameObjectClassConstructorHandlerRva));
    log_line(message);
    return false;
}

bool preflight_signature_index(std::size_t index) noexcept {
    switch (index) {
    case 0: return class_constructor_site_supported();
    case 1: return checked_signature("Weapon::SetTarget",
        kWeaponSetTargetRva, kExpectedWeaponSetTarget);
    case 2: return checked_signature("Craft::GetAlertStatus",
        kCraftGetAlertStatusRva, kExpectedCraftGetAlertStatus);
    case 3: return checked_signature("Craft::SetAlertStatus",
        kCraftSetAlertStatusRva, kExpectedCraftSetAlertStatus);
    case 4: return checked_signature("Craft::GetSpecialWeaponAutonomy",
        kCraftGetSpecialWeaponAutonomyRva,
        kExpectedCraftGetSpecialWeaponAutonomy);
    case 5: return checked_signature("Craft::SetSpecialWeaponAutonomy",
        kCraftSetSpecialWeaponAutonomyRva,
        kExpectedCraftSetSpecialWeaponAutonomy);
    case 6: return checked_signature("GameObject::GetCurrentCommand",
        kGameObjectGetCurrentCommandRva,
        kExpectedGameObjectGetCurrentCommand);
    case 7: return checked_signature("GameObject::SetCommand(targetless)",
        kGameObjectSetTargetlessCommandRva,
        kExpectedGameObjectSetTargetlessCommand);
    case 8: return checked_signature("GameObject::SetCommand(object)",
        kGameObjectSetObjectCommandRva,
        kExpectedGameObjectSetObjectCommand);
    default: return false;
    }
}

struct TurretInitContext {
    const A2FO_ModuleApi* api = nullptr;
    void* site = nullptr;
    void* destination = nullptr;
    std::size_t patch_length = 0;
    std::exception_ptr exception{};
    bool installing = false;
};

std::uintptr_t turret_init_step(void* opaque, std::uint32_t op,
                                std::uintptr_t arg) noexcept {
    auto& context = *static_cast<TurretInitContext*>(opaque);
    if (context.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
        const A2FO_ModuleApi* api = context.api;
        switch (op) {
        case 0:
            return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                api->armada_module && api->fleetops_module &&
                api->install_inline_hook && api->patch_jump &&
                api->register_classlabel_alias &&
                A2FO_MODULE_API_HAS(api, get_original_classlabel) &&
                api->get_original_classlabel &&
                A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) &&
                api->register_classlabel_odf_defaults &&
                (api->capabilities & A2FO_CAP_ORIGINAL_CLASSLABEL) != 0 &&
                (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) != 0 &&
                A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler) &&
                api->register_weapon_trigger_handler &&
                A2FO_MODULE_API_HAS(api, register_craft_event_handler) &&
                api->register_craft_event_handler &&
                (api->capabilities & A2FO_CAP_WEAPON_TRIGGER_EVENTS) != 0 &&
                (api->capabilities & A2FO_CAP_CRAFT_EVENTS) != 0;
        case 1: g_api = api; return 1;
        case 2:
            g_armada = static_cast<HMODULE>(api->armada_module());
            return 1;
        case 3:
            g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
            return 1;
        case 4: return g_armada && g_fleet_ops;
        case 5: resolve_shield_observers(); return 1;
        case 6: return api->register_classlabel_alias(
            kModuleName, "turret", "sensor");
        case 7: return api->register_classlabel_odf_defaults(
            kModuleName, "turret", kTurretDefaults.data(),
            static_cast<std::uint32_t>(kTurretDefaults.size()));
        case 8:
            return A2FO_MODULE_API_HAS(
                api, register_weapon_trigger_handler_masked) &&
                api->register_weapon_trigger_handler_masked;
        case 9: return api->register_weapon_trigger_handler_masked(
            kModuleName, A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK,
            &weapon_trigger_handler, nullptr);
        case 10: return api->register_weapon_trigger_handler(
            kModuleName, &weapon_trigger_handler, nullptr);
        case 11: return api->register_craft_event_handler(
            kModuleName, &craft_event_handler, nullptr);
        case 12: log_line("Turret semantic policy registration failed"); return 1;
        case 13: return preflight_signature_index(arg);
        case 14:
            log_line("Supported ArmadaL signatures were not found; runtime disabled");
            return 1;
        case 15:
            context.site = at(g_armada, kGameObjectClassConstructorRva);
            return signature_matches(kGameObjectClassConstructorRva,
                                     kExpectedGameObjectClassConstructor);
        case 16:
            context.installing = true;
            return api->install_inline_hook(
                context.site,
                reinterpret_cast<void*>(&game_object_class_constructor_hook),
                sizeof(kExpectedGameObjectClassConstructor),
                kExpectedGameObjectClassConstructor,
                &g_class_constructor_hook);
        case 17:
            g_class_constructor_original = g_class_constructor_hook.gateway;
            g_chained_fo_class_constructor = false;
            return g_class_constructor_original != nullptr;
        case 18:
            return supported_fleet_ops_detour(
                kGameObjectClassConstructorRva,
                kFoGameObjectClassConstructorHandlerRva,
                kExpectedFoGameObjectClassConstructorHandler,
                &context.destination, &context.patch_length) &&
                context.patch_length <= 6;
        case 19: {
            std::uint8_t expected_detour[6]{};
            std::memcpy(expected_detour, context.site, context.patch_length);
            context.installing = true;
            return api->patch_jump(
                context.site,
                reinterpret_cast<void*>(&game_object_class_constructor_hook),
                expected_detour, context.patch_length);
        }
        case 20:
            g_class_constructor_original = context.destination;
            g_chained_fo_class_constructor = true;
            return 1;
        case 21:
            context.installing = true;
            return api->install_inline_hook(
                at(g_armada, kWeaponSetTargetRva),
                reinterpret_cast<void*>(&weapon_set_target_hook),
                sizeof(kExpectedWeaponSetTarget), kExpectedWeaponSetTarget,
                &g_weapon_set_target_hook);
        case 22:
            log_line("A turret runtime hook could not be installed; hooks fail closed");
            return 1;
        case 23:
            g_runtime_ready = arg != 0;
            if (g_runtime_ready) {
                if (g_chained_fo_class_constructor) {
                    log_line(
                        "Indexed hull-mounted turret runtime initialized and chained "
                        "through Fleet Operations class construction; shared Craft "
                        "and weapon events active");
                } else {
                    log_line("Indexed hull-mounted turret runtime initialized");
                }
            } else {
                log_line("Turret classlabel loaded with linked-mount runtime disabled");
            }
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

// install_runtime_hooks was noexcept in the original module.
void rethrow_install_exception(std::exception_ptr error) noexcept {
    std::rethrow_exception(error);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    TurretInitContext context{};
    context.api = api;
    const bool accepted = a2fo_rs_stage8_turrets_init(
        &turret_init_step, &context) != 0;
    if (context.exception) {
        if (context.installing) rethrow_install_exception(context.exception);
        std::rethrow_exception(context.exception);
    }
    // Earlier successful patches remain process-lifetime even after a later
    // hook fails; the disabled runtime still stays resident.
    return accepted;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Inline hooks are process-lifetime patches owned by the core. Runtime
    // module shutdown is therefore intentionally non-destructive.
}
