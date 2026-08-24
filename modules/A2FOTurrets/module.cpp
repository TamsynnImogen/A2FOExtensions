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

#include <windows.h>

#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void resolve_shield_observers() noexcept {
    HMODULE shields = GetModuleHandleA(kAlwaysShowShieldsModuleName);
    FARPROC class_export = shields
        ? GetProcAddress(shields, kShieldClassObserverExport)
        : nullptr;
    static_assert(sizeof(class_export) == sizeof(g_shield_class_observer),
                  "unexpected function-pointer size");
    std::memcpy(&g_shield_class_observer, &class_export,
                sizeof(g_shield_class_observer));
    if (!g_shield_class_observer) {
        g_shield_class_observer = nullptr;
        return;
    }
    log_line(
        "Shield visibility callbacks linked through "
        "A2FOAlwaysShowShields");
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
    while (begin < value->size() &&
           std::isspace(static_cast<unsigned char>((*value)[begin]))) {
        ++begin;
    }
    std::size_t end = value->size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>((*value)[end - 1]))) {
        --end;
    }
    *value = value->substr(begin, end - begin);
}

void normalize_odf_basename(std::string* value) {
    if (!value) return;
    trim_string(value);
    if (value->size() >= 4) {
        const std::size_t offset = value->size() - 4;
        if ((*value)[offset] == '.' &&
            std::tolower(static_cast<unsigned char>((*value)[offset + 1])) == 'o' &&
            std::tolower(static_cast<unsigned char>((*value)[offset + 2])) == 'd' &&
            std::tolower(static_cast<unsigned char>((*value)[offset + 3])) == 'f') {
            value->resize(offset);
        }
    }
}

bool read_parameter_string(void* parameter_db, const char* key,
                           std::string* output) noexcept {
    if (!parameter_db || !key || !*key || !output || !g_armada) {
        return false;
    }
    std::array<char, 260> value{};
    const std::uintptr_t found = a2fo_turret_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if ((found & 0xffu) == 0 || value[0] == '\0') return false;
    try {
        *output = value.data();
        trim_string(output);
        return !output->empty();
    } catch (...) {
        return false;
    }
}

bool parse_float(const std::string& text, float* output) noexcept {
    if (!output || text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }
    if (*end == 'f' || *end == 'F') ++end;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;
    *output = parsed;
    return true;
}

bool read_parameter_float(void* parameter_db, const char* key,
                          float* output) noexcept {
    std::string value;
    return read_parameter_string(parameter_db, key, &value) &&
        parse_float(value, output);
}

bool read_parameter_bool(void* parameter_db, const char* key,
                         bool* output) noexcept {
    if (!output) return false;
    std::string value;
    if (!read_parameter_string(parameter_db, key, &value)) return false;
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    if (value == "1" || value == "true" || value == "yes") {
        *output = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "no") {
        *output = false;
        return true;
    }
    return false;
}

bool is_semantic_turret(void* parameter_db) noexcept {
    if (!g_api || !g_api->get_original_classlabel || !parameter_db) {
        return false;
    }
    std::array<char, 64> label{};
    if (!g_api->get_original_classlabel(
            parameter_db, label.data(),
            static_cast<std::uint32_t>(label.size()))) {
        return false;
    }
    for (char& character : label) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return std::strcmp(label.data(), "turret") == 0;
}

void normalize_turret_config(TurretClassConfig* config) noexcept {
    if (!config) return;
    AimLimits& limits = config->limits;
    limits.yaw_min_degrees = a2fo::turrets::clamp_value(
        limits.yaw_min_degrees, -180.0f, 180.0f);
    limits.yaw_max_degrees = a2fo::turrets::clamp_value(
        limits.yaw_max_degrees, -180.0f, 180.0f);
    if (limits.yaw_min_degrees > limits.yaw_max_degrees) {
        std::swap(limits.yaw_min_degrees, limits.yaw_max_degrees);
    }
    limits.pitch_min_degrees = a2fo::turrets::clamp_value(
        limits.pitch_min_degrees, -90.0f, 90.0f);
    limits.pitch_max_degrees = a2fo::turrets::clamp_value(
        limits.pitch_max_degrees, -90.0f, 90.0f);
    if (limits.pitch_min_degrees > limits.pitch_max_degrees) {
        std::swap(limits.pitch_min_degrees, limits.pitch_max_degrees);
    }
    limits.yaw_rate_degrees = a2fo::turrets::clamp_value(
        limits.yaw_rate_degrees, 0.0f, 3600.0f);
    limits.pitch_rate_degrees = a2fo::turrets::clamp_value(
        limits.pitch_rate_degrees, 0.0f, 3600.0f);
    config->rest.yaw_degrees = a2fo::turrets::clamp_value(
        config->rest.yaw_degrees, limits.yaw_min_degrees,
        limits.yaw_max_degrees);
    config->rest.pitch_degrees = a2fo::turrets::clamp_value(
        config->rest.pitch_degrees, limits.pitch_min_degrees,
        limits.pitch_max_degrees);
}

void register_class_policy(void* class_object, void* parameter_db) noexcept {
    if (!g_runtime_ready || !class_object || !parameter_db) return;

    char odf_name[128]{};
    copy_class_odf_name(class_object, odf_name, sizeof(odf_name));
    if (is_semantic_turret(parameter_db)) {
        TurretClassConfig config{};
        config.odf_name = odf_name;
        read_parameter_float(parameter_db, "turretYawMin",
                             &config.limits.yaw_min_degrees);
        read_parameter_float(parameter_db, "turretYawMax",
                             &config.limits.yaw_max_degrees);
        read_parameter_float(parameter_db, "turretPitchMin",
                             &config.limits.pitch_min_degrees);
        read_parameter_float(parameter_db, "turretPitchMax",
                             &config.limits.pitch_max_degrees);
        read_parameter_float(parameter_db, "turretYawRate",
                             &config.limits.yaw_rate_degrees);
        read_parameter_float(parameter_db, "turretPitchRate",
                             &config.limits.pitch_rate_degrees);
        read_parameter_float(parameter_db, "turretRestYaw",
                             &config.rest.yaw_degrees);
        read_parameter_float(parameter_db, "turretRestPitch",
                             &config.rest.pitch_degrees);
        read_parameter_bool(parameter_db, "turretReturnToRest",
                            &config.return_to_rest);
        normalize_turret_config(&config);
        try {
            g_turret_classes[class_object] = config;
        } catch (...) {
            log_line("Could not retain a turret class policy");
            return;
        }
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "Registered turret ODF '%s': yaw %.1f..%.1f @ %.1f deg/s, "
            "pitch %.1f..%.1f @ %.1f deg/s",
            odf_name[0] ? odf_name : "<unknown>",
            config.limits.yaw_min_degrees,
            config.limits.yaw_max_degrees,
            config.limits.yaw_rate_degrees,
            config.limits.pitch_min_degrees,
            config.limits.pitch_max_degrees,
            config.limits.pitch_rate_degrees);
        log_line(message);
    }

    ParentClassConfig parent{};
    parent.odf_name = odf_name;
    for (std::size_t index = 0; index <= kMaximumTurrets; ++index) {
        char turret_key[32]{};
        char hardpoint_key[40]{};
        std::snprintf(turret_key, sizeof(turret_key), "turret%lu",
                      static_cast<unsigned long>(index));
        std::snprintf(hardpoint_key, sizeof(hardpoint_key),
                      "turretHardpoint%lu",
                      static_cast<unsigned long>(index));
        std::string turret_odf;
        std::string hardpoint;
        const bool has_turret = read_parameter_string(
            parameter_db, turret_key, &turret_odf);
        const bool has_hardpoint = read_parameter_string(
            parameter_db, hardpoint_key, &hardpoint);
        if (!has_turret && !has_hardpoint) continue;
        if (!has_turret || !has_hardpoint) {
            char message[320]{};
            std::snprintf(
                message, sizeof(message),
                "Skipping incomplete turret pair %lu on '%s' "
                "(%s is missing)",
                static_cast<unsigned long>(index),
                odf_name[0] ? odf_name : "<unknown>",
                has_turret ? hardpoint_key : turret_key);
            log_line(message);
            continue;
        }
        normalize_odf_basename(&turret_odf);
        trim_string(&hardpoint);
        if (turret_odf.empty() || hardpoint.empty()) continue;
        try {
            parent.mounts.push_back(MountClassConfig{
                static_cast<std::uint32_t>(index),
                std::move(turret_odf), std::move(hardpoint)});
        } catch (...) {
            log_line("Could not retain an indexed turret pair");
            return;
        }
    }
    if (parent.mounts.empty()) return;
    try {
        const std::size_t count = parent.mounts.size();
        g_parent_classes[class_object] = std::move(parent);
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Registered %lu hull turret mount%s on '%s'",
            static_cast<unsigned long>(count), count == 1 ? "" : "s",
            odf_name[0] ? odf_name : "<unknown>");
        log_line(message);
    } catch (...) {
        log_line("Could not retain a parent turret policy");
    }
}

std::uintptr_t __attribute__((fastcall)) game_object_class_constructor_hook(
    void* self, void*, void* parent_class, void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_turret_call_thiscall_2(
        g_class_constructor_original, self,
        reinterpret_cast<std::uintptr_t>(parent_class),
        reinterpret_cast<std::uintptr_t>(parameter_db));
    register_class_policy(self, parameter_db);
    if (g_shield_class_observer) {
        g_shield_class_observer(self, parameter_db);
    }
    return result;
}

ParentRuntime* ensure_parent_runtime(void* parent) noexcept {
    if (!parent || object_expired(parent)) return nullptr;
    const std::uint32_t handle = object_handle(parent);
    if (handle == 0) return nullptr;
    const auto present = g_parents.find(handle);
    if (present != g_parents.end()) return &present->second;

    void* class_pointer = object_class(parent);
    const auto configured = g_parent_classes.find(class_pointer);
    if (configured == g_parent_classes.end()) return nullptr;
    ParentRuntime runtime{};
    runtime.object_class = class_pointer;
    try {
        runtime.mounts.reserve(configured->second.mounts.size());
        for (const MountClassConfig& config : configured->second.mounts) {
            runtime.mounts.push_back(MountRuntime{config});
        }
        const auto inserted = g_parents.emplace(handle, std::move(runtime));
        return inserted.second ? &inserted.first->second : nullptr;
    } catch (...) {
        log_line("Could not allocate per-object turret state");
        return nullptr;
    }
}

MountRuntime* find_mount(ParentRuntime* parent,
                         std::uint32_t index) noexcept {
    if (!parent) return nullptr;
    for (MountRuntime& mount : parent->mounts) {
        if (mount.config.index == index) return &mount;
    }
    return nullptr;
}

bool resolve_hardpoint(void* parent, MountRuntime* mount) noexcept {
    if (!parent || !mount || !g_armada) return false;
    if (mount->hardpoint_resolved) return mount->hardpoint_node != nullptr;
    mount->hardpoint_resolved = true;
    void* class_pointer = object_class(parent);
    void* root = reinterpret_cast<void*>(a2fo_turret_call_thiscall_0(
        at(g_armada, kGameObjectClassGetHierarchyRootRva), class_pointer));
    if (root) {
        mount->hardpoint_node = reinterpret_cast<void*>(
            a2fo_turret_call_thiscall_1(
                at(g_armada, kSt3dNodeFindRecursiveRva), root,
                reinterpret_cast<std::uintptr_t>(
                    mount->config.hardpoint.c_str())));
    }
    if (!mount->hardpoint_node) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Turret %lu disabled: hardpoint '%s' was not found",
            static_cast<unsigned long>(mount->config.index),
            mount->config.hardpoint.c_str());
        log_line(message);
        mount->status = MountStatus::failed;
        return false;
    }
    return true;
}

bool mount_world_transform(void* parent, MountRuntime* mount,
                           Matrix34* output) noexcept {
    if (!parent || !mount || !output || !resolve_hardpoint(parent, mount)) {
        return false;
    }
    const std::uintptr_t result = a2fo_turret_call_thiscall_2(
        at(g_armada, kEntityGetWorldTransformRva), parent,
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(mount->hardpoint_node));
    return result == reinterpret_cast<std::uintptr_t>(output);
}

void add_to_current_mission(void* object) noexcept {
    if (!object || !g_armada) return;
    using GetCurrentFn = void* (__cdecl*)();
    const auto get_current = reinterpret_cast<GetCurrentFn>(
        at(g_armada, kAiMissionGetCurrentRva));
    void* mission = get_current ? get_current() : nullptr;
    void* vtable = read_at<void*>(mission, 0, nullptr);
    void* add_object = read_at<void*>(vtable, 0x18, nullptr);
    if (mission && add_object) {
        a2fo_turret_call_thiscall_1(
            add_object, mission, reinterpret_cast<std::uintptr_t>(object));
    }
}

void expire_craft(void* craft) noexcept {
    if (!craft || object_expired(craft) || !g_armada) return;
    a2fo_turret_call_thiscall_0(
        at(g_armada, kCraftDoExpireRva), craft);
}

void* construct_turret(const MountRuntime& mount,
                       const Matrix34& transform, std::int32_t team,
                       std::uint32_t parent_handle) noexcept {
    if (!g_armada || mount.config.turret_odf.empty()) return nullptr;
    using FindClassFn = void* (__cdecl*)(const char*);
    const auto find_class = reinterpret_cast<FindClassFn>(
        at(g_armada, kGameObjectClassFindRva));
    void* turret_class = find_class
        ? find_class(mount.config.turret_odf.c_str()) : nullptr;
    if (!turret_class) return nullptr;

    char label[64]{};
    std::snprintf(label, sizeof(label), "%s%08lX:%lu",
                  kLinkedLabelPrefix,
                  static_cast<unsigned long>(parent_handle),
                  static_cast<unsigned long>(mount.config.index));
    void* child = reinterpret_cast<void*>(a2fo_turret_call_thiscall_4(
        at(g_armada, kGameObjectClassConstructRva), turret_class,
        reinterpret_cast<std::uintptr_t>(&transform),
        static_cast<std::uintptr_t>(team), 0,
        reinterpret_cast<std::uintptr_t>(label)));
    if (child) add_to_current_mission(child);
    return child;
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

void spawn_pending_turrets(void* parent) noexcept {
    ParentRuntime* runtime = ensure_parent_runtime(parent);
    if (!runtime) return;
    const std::uint32_t parent_handle = object_handle(parent);
    const std::int32_t team = read_at<std::int32_t>(
        parent, kObjectTeamOffset, 0);
    for (MountRuntime& mount : runtime->mounts) {
        if (mount.status == MountStatus::active) {
            if (!find_entity(mount.child_handle)) {
                mount.status = MountStatus::destroyed;
                mount.child_handle = 0;
            }
            continue;
        }
        if (mount.status != MountStatus::pending) continue;
        Matrix34 transform{};
        if (!mount_world_transform(parent, &mount, &transform)) continue;

        mount.status = MountStatus::failed;
        void* child = construct_turret(
            mount, transform, team, parent_handle);
        if (!child) {
            log_spawn_failure(parent, mount,
                              "ODF class was not found or construction failed");
            continue;
        }
        const std::uint32_t child_handle = object_handle(child);
        const auto turret_config = g_turret_classes.find(object_class(child));
        if (child_handle == 0 || turret_config == g_turret_classes.end()) {
            log_spawn_failure(
                parent, mount,
                "referenced ODF must use classLabel = \"turret\"");
            expire_craft(child);
            continue;
        }

        ChildRuntime child_runtime{};
        child_runtime.parent_handle = parent_handle;
        child_runtime.mount_index = mount.config.index;
        child_runtime.current = turret_config->second.rest;
        try {
            g_children[child_handle] = child_runtime;
            mount.child_handle = child_handle;
            mount.status = MountStatus::active;
        } catch (...) {
            log_spawn_failure(parent, mount, "runtime state allocation failed");
            expire_craft(child);
            continue;
        }

        // Do not allow a newly constructed child one independent combat tick.
        prepare_linked_turret_for_simulation(child, child_handle);

        const Matrix34 initial = a2fo::turrets::compose_turret_transform(
            transform, child_runtime.current);
        a2fo_turret_call_thiscall_1(
            at(g_armada, kGameObjectSetTransformRva), child,
            reinterpret_cast<std::uintptr_t>(&initial));
    }
}

bool parse_linked_label(const char* label, std::uint32_t* parent_handle,
                        std::uint32_t* mount_index) noexcept {
    if (!label || !parent_handle || !mount_index ||
        std::strncmp(label, kLinkedLabelPrefix,
                     sizeof(kLinkedLabelPrefix) - 1) != 0) {
        return false;
    }
    unsigned long parsed_parent = 0;
    unsigned long parsed_index = 0;
    char trailing = '\0';
    const int converted = std::sscanf(
        label + sizeof(kLinkedLabelPrefix) - 1,
        "%8lx:%lu%c", &parsed_parent, &parsed_index, &trailing);
    if (converted != 2 || parsed_parent == 0 ||
        parsed_index > kMaximumTurrets ||
        parsed_parent > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    *parent_handle = static_cast<std::uint32_t>(parsed_parent);
    *mount_index = static_cast<std::uint32_t>(parsed_index);
    return true;
}

void attach_loaded_turret(void* child) noexcept {
    if (!child || g_turret_classes.find(object_class(child)) ==
            g_turret_classes.end()) {
        return;
    }
    const char* label = read_at<const char*>(
        child, kObjectLabelOffset, nullptr);
    std::uint32_t parent_handle = 0;
    std::uint32_t mount_index = 0;
    if (!parse_linked_label(label, &parent_handle, &mount_index)) return;
    const std::uint32_t child_handle = object_handle(child);
    if (child_handle == 0) return;

    ChildRuntime runtime{};
    runtime.parent_handle = parent_handle;
    runtime.mount_index = mount_index;
    const auto config = g_turret_classes.find(object_class(child));
    if (config != g_turret_classes.end()) runtime.current = config->second.rest;

    void* parent = find_entity(parent_handle);
    try {
        g_children[child_handle] = runtime;
    } catch (...) {
        log_line("Could not restore a loaded turret relationship");
        expire_craft(child);
        return;
    }

    // Object PostLoad order is not a public contract. Keep the parsed child
    // relationship even when its parent has not been published yet; the first
    // Craft::Simulate reconnects it after the whole object table is restored.
    if (!parent) return;
    ParentRuntime* parent_runtime = ensure_parent_runtime(parent);
    MountRuntime* mount = find_mount(parent_runtime, mount_index);
    if (!mount) {
        log_line("Loaded turret could not resolve its parent mount; expiring it");
        g_children.erase(child_handle);
        expire_craft(child);
        return;
    }
    if (mount->status == MountStatus::active &&
        mount->child_handle != child_handle) {
        log_line("Duplicate loaded turret detected; expiring the duplicate");
        g_children.erase(child_handle);
        expire_craft(child);
        return;
    }
    mount->child_handle = child_handle;
    mount->status = MountStatus::active;
}

void sync_child_ownership(void* child, void* parent) noexcept {
    if (!child || !parent || !g_armada) return;
    const std::int32_t parent_team = read_at<std::int32_t>(
        parent, kObjectTeamOffset, 0);
    const std::int32_t child_team = read_at<std::int32_t>(
        child, kObjectTeamOffset, 0);
    void* parent_race = read_at<void*>(parent, kObjectRaceOffset, nullptr);
    void* child_race = read_at<void*>(child, kObjectRaceOffset, nullptr);
    if (parent_race && parent_race != child_race) {
        a2fo_turret_call_thiscall_2(
            at(g_armada, kGameObjectSwapRaceAndTeamRva), child,
            reinterpret_cast<std::uintptr_t>(parent_race),
            static_cast<std::uintptr_t>(parent_team));
    } else if (parent_team != child_team) {
        a2fo_turret_call_thiscall_1(
            at(g_armada, kGameObjectSwapTeamRva), child,
            static_cast<std::uintptr_t>(parent_team));
    }
}

std::int32_t get_craft_policy(
    void* craft, std::uintptr_t getter_rva) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(a2fo_turret_call_thiscall_0(
            at(g_armada, getter_rva), craft)));
}

void sync_craft_policy_value(
    void* child, void* parent, std::uintptr_t getter_rva,
    std::uintptr_t setter_rva) noexcept {
    const std::int32_t parent_value = get_craft_policy(parent, getter_rva);
    if (get_craft_policy(child, getter_rva) == parent_value) return;
    a2fo_turret_call_thiscall_1(
        at(g_armada, setter_rva), child,
        static_cast<std::uint32_t>(parent_value));
}

void sync_child_combat_policy(void* child, void* parent) noexcept {
    if (!child || !parent || !g_armada) return;
    // The semantic turret is a complete native Craft, so its CraftProcess
    // begins with independent policy fields. Copy the host values before the
    // child's native simulation selects or triggers any weapon target.
    sync_craft_policy_value(
        child, parent, kCraftGetAlertStatusRva,
        kCraftSetAlertStatusRva);
    sync_craft_policy_value(
        child, parent, kCraftGetSpecialWeaponAutonomyRva,
        kCraftSetSpecialWeaponAutonomyRva);
}

AttackOrder current_attack_order(void* parent) noexcept {
    AttackOrder order{};
    if (!parent || !g_armada) return order;
    const void* command = reinterpret_cast<const void*>(
        a2fo_turret_call_thiscall_0(
            at(g_armada, kGameObjectGetCurrentCommandRva), parent));
    const std::uint32_t command_id = read_at<std::uint32_t>(
        command, kCommandIdOffset, 0);
    const std::uint32_t target_form = read_at<std::uint32_t>(
        command, kCommandTargetFormOffset, 0);
    const std::uint32_t target_handle = read_at<std::uint32_t>(
        command, kCommandTargetHandleOffset, 0);
    void* target = find_entity(target_handle);
    const bool target_exists = target && !object_expired(target);
    order.target_handle = a2fo::turrets::attack_order_target_handle(
        command_id, target_form, target_handle, target_exists);
    if (order.target_handle == 0) return order;
    order.target = target;
    order.extra = read_at<std::uint32_t>(
        command, kCommandExtraOffset, 0);
    order.flags = read_at<std::uint8_t>(
        command, kCommandFlagsOffset, 0);
    return order;
}

bool child_has_attack_order(
    void* child, const AttackOrder& order) noexcept {
    const void* command = reinterpret_cast<const void*>(
        a2fo_turret_call_thiscall_0(
            at(g_armada, kGameObjectGetCurrentCommandRva), child));
    return read_at<std::uint32_t>(command, kCommandIdOffset, 0) ==
            a2fo::turrets::kAttackCommand &&
        read_at<std::uint32_t>(command, kCommandTargetFormOffset, 0) ==
            a2fo::turrets::kObjectCommandTargetForm &&
        read_at<std::uint32_t>(command, kCommandTargetHandleOffset, 0) ==
            order.target_handle &&
        read_at<std::uint32_t>(command, kCommandExtraOffset, 0) ==
            order.extra &&
        read_at<std::uint8_t>(command, kCommandFlagsOffset, 0) ==
            order.flags;
}

void mirror_attack_order(
    void* child, const AttackOrder& order,
    std::uint32_t previous_target_handle) noexcept {
    if (order.target_handle != 0 && order.target) {
        if (child_has_attack_order(child, order)) return;
        a2fo_turret_call_thiscall_4(
            at(g_armada, kGameObjectSetObjectCommandRva), child,
            a2fo::turrets::kAttackCommand,
            reinterpret_cast<std::uintptr_t>(order.target), order.extra,
            order.flags);
        return;
    }
    if (previous_target_handle == 0) return;
    // A targetless AiCommand 0 returns the child to ordinary native autonomy
    // after the host's explicit Attack order has ended.
    a2fo_turret_call_thiscall_4(
        at(g_armada, kGameObjectSetTargetlessCommandRva), child,
        0, 0, 0, 0);
}

void update_inherited_order_target(
    void* child, std::uint32_t child_handle,
    ChildRuntime* child_runtime, void* parent) noexcept {
    if (!child_runtime) return;
    const auto visual = g_targets.find(child_handle);
    const std::uint32_t visual_handle = visual == g_targets.end()
        ? 0 : visual->second;
    const AttackOrder order = current_attack_order(parent);
    mirror_attack_order(
        child, order, child_runtime->ordered_target_handle);
    const a2fo::turrets::OrderTargetState next =
        a2fo::turrets::update_order_target(
            child_runtime->ordered_target_handle,
            order.target_handle, visual_handle);
    child_runtime->ordered_target_handle = next.ordered_target_handle;
    if (next.visual_target_handle == visual_handle) return;
    try {
        if (next.visual_target_handle != 0) {
            g_targets[child_handle] = next.visual_target_handle;
        } else {
            g_targets.erase(child_handle);
        }
    } catch (...) {
        g_targets.erase(child_handle);
    }
}

void prepare_linked_turret_for_simulation(
    void* child, std::uint32_t child_handle) noexcept {
    const auto child_found = g_children.find(child_handle);
    if (child_found == g_children.end()) return;
    void* parent = find_entity(child_found->second.parent_handle);
    if (!parent || object_expired(parent)) return;
    sync_child_ownership(child, parent);
    sync_child_combat_policy(child, parent);
    update_inherited_order_target(
        child, child_handle, &child_found->second, parent);
}

void update_linked_turret(void* child, std::uint32_t child_handle,
                          float elapsed_seconds) noexcept {
    const auto child_found = g_children.find(child_handle);
    if (child_found == g_children.end()) return;
    ChildRuntime& child_runtime = child_found->second;
    void* parent = find_entity(child_runtime.parent_handle);
    if (!parent || object_expired(parent)) {
        expire_craft(child);
        return;
    }
    ParentRuntime* parent_runtime = ensure_parent_runtime(parent);
    MountRuntime* mount = find_mount(
        parent_runtime, child_runtime.mount_index);
    if (mount && mount->status == MountStatus::active &&
        mount->child_handle != 0 && mount->child_handle != child_handle) {
        log_line("Duplicate loaded turret detected; expiring the duplicate");
        expire_craft(child);
        return;
    }
    if (mount) {
        mount->child_handle = child_handle;
        mount->status = MountStatus::active;
    }
    Matrix34 mount_transform{};
    if (!mount || !mount_world_transform(
            parent, mount, &mount_transform)) {
        expire_craft(child);
        return;
    }
    sync_child_ownership(child, parent);

    const auto turret_config = g_turret_classes.find(
        read_live_at<void*>(child, kObjectClassOffset, nullptr));
    if (turret_config == g_turret_classes.end()) return;
    const TurretClassConfig& config = turret_config->second;
    AimAngles desired = child_runtime.current;
    const auto target_found = g_targets.find(child_handle);
    void* target = target_found == g_targets.end()
        ? nullptr : find_entity(target_found->second);
    if (target && !object_expired(target)) {
        const Matrix34* target_transform =
            reinterpret_cast<const Matrix34*>(
                a2fo_turret_call_thiscall_0(
                    at(g_armada, kEntityGetTransformRva), target));
        if (target_transform) {
            const float position[3]{
                target_transform->values[9],
                target_transform->values[10],
                target_transform->values[11],
            };
            desired = a2fo::turrets::calculate_aim_angles(
                mount_transform, position, config.limits,
                child_runtime.current);
        }
    } else if (config.return_to_rest) {
        desired = config.rest;
    }

    child_runtime.current = a2fo::turrets::advance_aim_angles(
        child_runtime.current, desired, config.limits, elapsed_seconds);
    const Matrix34 transform = a2fo::turrets::compose_turret_transform(
        mount_transform, child_runtime.current);
    a2fo_turret_call_thiscall_1(
        at(g_armada, kGameObjectSetTransformRva), child,
        reinterpret_cast<std::uintptr_t>(&transform));
}

void unlink_child(std::uint32_t child_handle) noexcept {
    const auto child = g_children.find(child_handle);
    if (child == g_children.end()) {
        g_targets.erase(child_handle);
        return;
    }
    const auto parent = g_parents.find(child->second.parent_handle);
    if (parent != g_parents.end()) {
        MountRuntime* mount = find_mount(
            &parent->second, child->second.mount_index);
        if (mount && mount->child_handle == child_handle) {
            mount->child_handle = 0;
            mount->status = MountStatus::destroyed;
        }
    }
    g_targets.erase(child_handle);
    g_children.erase(child);
}

void cleanup_parent(std::uint32_t parent_handle) noexcept {
    const auto parent = g_parents.find(parent_handle);
    if (parent == g_parents.end()) return;
    std::vector<std::uint32_t> children;
    try {
        for (const MountRuntime& mount : parent->second.mounts) {
            if (mount.child_handle != 0) children.push_back(mount.child_handle);
        }
    } catch (...) {
        // The state is still erased below; cleanup must never throw into the
        // engine even if collecting handles fails under memory pressure.
    }
    g_parents.erase(parent);
    for (const std::uint32_t child_handle : children) {
        g_children.erase(child_handle);
        g_targets.erase(child_handle);
        expire_craft(find_entity(child_handle));
    }
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !g_runtime_ready || !event->craft) {
        return;
    }
    void* craft = event->craft;
    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP) {
        const std::uint32_t handle = object_handle(craft);
        cleanup_parent(handle);
        unlink_child(handle);
        return;
    }
    if (event->kind == A2FO_CRAFT_EVENT_POST_LOAD) {
        if (g_parent_classes.empty() && g_turret_classes.empty()) return;
        attach_loaded_turret(craft);
        return;
    }

    if (g_children.empty() && g_parent_classes.empty()) return;

    const bool active =
        read_live_at<std::uint8_t>(craft, kObjectExpiredOffset, 1) == 0;
    if (!active) return;
    const std::uint32_t handle =
        read_live_at<std::uint32_t>(craft, kObjectHandleOffset, 0);
    if (event->kind == A2FO_CRAFT_EVENT_SIMULATE_PRE) {
        // Combat policy must be present before native Craft simulation: that
        // is where alert/autonomy rules select targets and weapons may fire.
        if (g_children.find(handle) != g_children.end()) {
            prepare_linked_turret_for_simulation(craft, handle);
        }
        return;
    }
    if (event->kind != A2FO_CRAFT_EVENT_SIMULATE_POST) return;

    // Apply the linked transform after native simulation so the child physics
    // pass cannot move it away from the mount during this frame.
    if (g_children.find(handle) != g_children.end()) {
        update_linked_turret(craft, handle, event->elapsed_seconds);
    }
    if (g_parent_classes.find(
            read_live_at<void*>(craft, kObjectClassOffset, nullptr)) !=
        g_parent_classes.end()) {
        spawn_pending_turrets(craft);
    }
}

void* weapon_owner(void* weapon) noexcept {
    if (!g_runtime_ready || !weapon || !g_armada) return nullptr;
    return reinterpret_cast<void*>(a2fo_turret_call_thiscall_0(
        at(g_armada, kWeaponGetOwnerRva), weapon));
}

void track_weapon_target_for_owner(
    void* owner, const void* target) noexcept {
    if (!owner || g_turret_classes.find(object_class(owner)) ==
            g_turret_classes.end()) {
        return;
    }
    const std::uint32_t owner_handle = object_handle(owner);
    if (owner_handle == 0) return;
    const std::uint32_t target_handle = object_handle(target);
    try {
        if (target_handle != 0) {
            g_targets[owner_handle] = target_handle;
        } else {
            g_targets.erase(owner_handle);
        }
    } catch (...) {
        g_targets.erase(owner_handle);
    }
}

const void* inherited_order_target_for_owner(void* owner) noexcept {
    const std::uint32_t owner_handle = object_handle(owner);
    const auto child = g_children.find(owner_handle);
    if (child == g_children.end() ||
        child->second.ordered_target_handle == 0) {
        return nullptr;
    }
    void* target = find_entity(child->second.ordered_target_handle);
    return target && !object_expired(target) ? target : nullptr;
}

bool A2FO_CALL weapon_trigger_handler(
    const A2FO_WeaponTriggerEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event)) return false;
    if (event->kind == A2FO_WEAPON_TRIGGER_COMMITTED) return true;
    if (event->kind != A2FO_WEAPON_TRIGGER_PRECHECK) return false;
    void* weapon = event->weapon;
    const void* target = event->target;
    void* owner = weapon_owner(weapon);
    const void* ordered_target = inherited_order_target_for_owner(owner);
    // Trigger is Armada's last weapon boundary. Reject a shot selected for a
    // different object while the host has an explicit Attack order;
    // substituting here would bypass native range/CanAttack validation.
    if (ordered_target &&
        object_handle(target) != object_handle(ordered_target)) {
        track_weapon_target_for_owner(owner, ordered_target);
        return false;
    }
    track_weapon_target_for_owner(owner, target);
    return true;
}

void __attribute__((fastcall)) weapon_set_target_hook(
    void* weapon, void*, const void* target) noexcept {
    void* owner = weapon_owner(weapon);
    const void* ordered_target = inherited_order_target_for_owner(owner);
    const void* effective_target = ordered_target
        ? ordered_target : target;
    a2fo_turret_call_thiscall_1(
        g_weapon_set_target_hook.gateway, weapon,
        reinterpret_cast<std::uintptr_t>(effective_target));
    track_weapon_target_for_owner(owner, effective_target);
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

bool preflight_signatures() noexcept {
    bool supported = true;
    supported = class_constructor_site_supported() && supported;
    supported = checked_signature(
        "Weapon::SetTarget", kWeaponSetTargetRva,
        kExpectedWeaponSetTarget) && supported;
    supported = checked_signature(
        "Craft::GetAlertStatus", kCraftGetAlertStatusRva,
        kExpectedCraftGetAlertStatus) && supported;
    supported = checked_signature(
        "Craft::SetAlertStatus", kCraftSetAlertStatusRva,
        kExpectedCraftSetAlertStatus) && supported;
    supported = checked_signature(
        "Craft::GetSpecialWeaponAutonomy",
        kCraftGetSpecialWeaponAutonomyRva,
        kExpectedCraftGetSpecialWeaponAutonomy) && supported;
    supported = checked_signature(
        "Craft::SetSpecialWeaponAutonomy",
        kCraftSetSpecialWeaponAutonomyRva,
        kExpectedCraftSetSpecialWeaponAutonomy) && supported;
    supported = checked_signature(
        "GameObject::GetCurrentCommand",
        kGameObjectGetCurrentCommandRva,
        kExpectedGameObjectGetCurrentCommand) && supported;
    supported = checked_signature(
        "GameObject::SetCommand(targetless)",
        kGameObjectSetTargetlessCommandRva,
        kExpectedGameObjectSetTargetlessCommand) && supported;
    supported = checked_signature(
        "GameObject::SetCommand(object)",
        kGameObjectSetObjectCommandRva,
        kExpectedGameObjectSetObjectCommand) && supported;
    return supported;
}

bool install_class_constructor_hook(const A2FO_ModuleApi* api) noexcept {
    void* site = at(g_armada, kGameObjectClassConstructorRva);
    if (signature_matches(kGameObjectClassConstructorRva,
                          kExpectedGameObjectClassConstructor)) {
        if (!api->install_inline_hook(
                site,
                reinterpret_cast<void*>(
                    &game_object_class_constructor_hook),
                sizeof(kExpectedGameObjectClassConstructor),
                kExpectedGameObjectClassConstructor,
                &g_class_constructor_hook)) {
            return false;
        }
        g_class_constructor_original = g_class_constructor_hook.gateway;
        g_chained_fo_class_constructor = false;
        return g_class_constructor_original != nullptr;
    }

    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (!supported_fleet_ops_detour(
            kGameObjectClassConstructorRva,
            kFoGameObjectClassConstructorHandlerRva,
            kExpectedFoGameObjectClassConstructorHandler,
            &destination, &patch_length) ||
        patch_length > 6) {
        return false;
    }
    std::uint8_t expected_detour[6]{};
    std::memcpy(expected_detour, site, patch_length);
    if (!api->patch_jump(
            site,
            reinterpret_cast<void*>(&game_object_class_constructor_hook),
            expected_detour, patch_length)) {
        return false;
    }
    g_class_constructor_original = destination;
    g_chained_fo_class_constructor = true;
    return true;
}

bool install_runtime_hooks(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->install_inline_hook || !g_armada) return false;
    if (!preflight_signatures()) {
        log_line("Supported ArmadaL signatures were not found; runtime disabled");
        return false;
    }
    bool installed = true;
    installed = install_class_constructor_hook(api) && installed;
    installed = api->install_inline_hook(
        at(g_armada, kWeaponSetTargetRva),
        reinterpret_cast<void*>(&weapon_set_target_hook),
        sizeof(kExpectedWeaponSetTarget), kExpectedWeaponSetTarget,
        &g_weapon_set_target_hook) && installed;
    if (!installed) {
        log_line("A turret runtime hook could not be installed; hooks fail closed");
    }
    return installed;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook || !api->patch_jump ||
        !api->register_classlabel_alias ||
        !A2FO_MODULE_API_HAS(api, get_original_classlabel) ||
        !api->get_original_classlabel ||
        !A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) ||
        !api->register_classlabel_odf_defaults ||
        (api->capabilities & A2FO_CAP_ORIGINAL_CLASSLABEL) == 0 ||
        (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) == 0 ||
        !A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler) ||
        !api->register_weapon_trigger_handler ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler) ||
        !api->register_craft_event_handler ||
        (api->capabilities & A2FO_CAP_WEAPON_TRIGGER_EVENTS) == 0 ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops) return false;
    resolve_shield_observers();

    const bool alias_registered = api->register_classlabel_alias(
        kModuleName, "turret", "sensor");
    const bool defaults_registered = api->register_classlabel_odf_defaults(
        kModuleName, "turret", kTurretDefaults.data(),
        static_cast<std::uint32_t>(kTurretDefaults.size()));
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
    const bool craft_events_registered = api->register_craft_event_handler(
        kModuleName, &craft_event_handler, nullptr);
    if (!alias_registered || !defaults_registered || !trigger_registered ||
        !craft_events_registered) {
        log_line("Turret semantic policy registration failed");
        return false;
    }

    g_runtime_ready = install_runtime_hooks(api);
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
    // A failed inline-hook transaction cannot safely unload this DLL after a
    // successful earlier patch. Keep the module resident and make every hook
    // a pass-through while g_runtime_ready is false.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Inline hooks are process-lifetime patches owned by the core. Runtime
    // module shutdown is therefore intentionally non-destructive.
}
