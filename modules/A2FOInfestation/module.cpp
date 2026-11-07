/*
 * A2FOInfestation
 *
 * Noxter-style delayed infestation:
 *   - an opt-in weapon marks an enemy Craft after its native trigger commits;
 *   - the infection counts down on the target's synchronized Craft::Simulate;
 *   - an admitted repair-yard visit cures the infection;
 *   - hatching creates an authored explosion, applies direct internal damage,
 *     releases larvae for the original infector team, and rallies them toward
 *     the nearest same-team breederMother.
 *
 * The module owns no executable detours. It consumes the core's shared weapon,
 * class-load, and Craft event dispatchers and calls already-verified native
 * Armada entry points for damage, construction, effects, and movement.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/stage8_a_infestation_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

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
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

extern "C" std::uintptr_t a2fo_infestation_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
extern "C" void a2fo_infestation_call_thiscall_command_vector_int(
    void* function, void* self, std::int32_t command,
    const void* vector, std::int32_t value);

namespace {

constexpr char kModuleName[] = "A2FOInfestation";

constexpr float kDefaultDelaySeconds = 10.0f;
constexpr float kDefaultSpawnRadius = 20.0f;
constexpr float kLarvaMoveRefreshSeconds = 1.0f;
constexpr float kMotherArrivalRadius = 40.0f;
constexpr std::uint32_t kMaximumObjects = 1000000;
constexpr std::int32_t kMoveSingleCommand = 5;
constexpr std::uint32_t kIgnoreShieldsDamageFlag = 0x01u;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs used by the current modules.
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kCraftGetRva = 0x00013800;
constexpr std::uintptr_t kExplosionClassFindRva = 0x00064ce0;
constexpr std::uintptr_t kCraftDamageRva = 0x000c5bb0;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x000cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x000cd390;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetRva = 0x000cfff0;
constexpr std::uintptr_t kParameterDbGetProjectIdRva = 0x00135200;
constexpr std::uintptr_t kRepairQueueVtableRva = 0x002b3744;
constexpr std::uintptr_t kDequeueCommandVectorRva = 0x000d4850;
constexpr std::uintptr_t kGameObjectListPointerRva = 0x00361084;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;

constexpr std::size_t kObjectExpiredOffset = 0x27;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectCommandCarrierOffset = 0x44;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectPositionOffset = 0xac;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kCraftQueueStageOffset = 0x194;
constexpr std::size_t kCraftQueueOwnerOffset = 0x198;
constexpr std::size_t kYardRepairQueueOffset = 0x2ac;
constexpr std::size_t kWeaponClassOffset = 0x04;

constexpr std::size_t kDamageInfoOnSyntheticOrdnanceOffset = 0x3c;
constexpr std::size_t kDamageInfoSize = 0x20;
constexpr std::size_t kDamageInfoDamageOffset = 0x00;
constexpr std::size_t kDamageInfoSystemTypeOffset = 0x04;
constexpr std::size_t kDamageInfoFlagsOffset = 0x14;
constexpr std::size_t kDamageInfoAttackerHandleOffset = 0x1c;
constexpr std::size_t kSyntheticDamageBlockSize =
    kDamageInfoOnSyntheticOrdnanceOffset + kDamageInfoSize;

constexpr std::uint8_t kExpectedAiMissionGetCurrent[] = {
    0xa1, 0xdc, 0x47, 0x73, 0x00, 0xc3};
constexpr std::uint8_t kExpectedCraftGet[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedExplosionClassFind[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xf8, 0xaf, 0x69};
constexpr std::uint8_t kExpectedCraftDamage[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
constexpr std::uint8_t kExpectedGameObjectClassFind[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedGameObjectClassConstruct[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedEntityGetTransform[] = {
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
constexpr std::uint8_t kExpectedEntityGet[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x08};
constexpr std::uint8_t kExpectedParameterDbGetProjectId[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedDequeueCommandVector[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x10};
constexpr std::uint8_t kExpectedWeaponGetOwner[] = {
    0x8b, 0x49, 0x18, 0x51, 0xe8};

constexpr std::array<const char*, 8> kWeaponFields{{
    "noxterInfestation",
    "infestationDelay",
    "infestationDamage",
    "infestationLarva",
    "infestationLarvaCount",
    "infestationExplosion",
    "infestationBypassShields",
    "infestationSpawnRadius",
}};
constexpr std::array<const char*, 1> kObjectFields{{"breederMother"}};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Matrix34 {
    float values[12]{};
};

struct WeaponPolicy {
    bool enabled = false;
    float delay_seconds = kDefaultDelaySeconds;
    float damage = 0.0f;
    std::string larva_odf;
    std::uint32_t larva_count = 1;
    std::uint32_t explosion_project_id = 0;
    bool bypass_shields = true;
    float spawn_radius = kDefaultSpawnRadius;
};

struct Infection {
    std::uint32_t target_handle = 0;
    std::uint32_t infector_handle = 0;
    std::int32_t infector_team = -1;
    float remaining_seconds = 0.0f;
    WeaponPolicy policy;
};

struct LarvaRoute {
    std::uint32_t larva_handle = 0;
    std::uint32_t mother_handle = 0;
    std::int32_t team = -1;
    float refresh_seconds = 0.0f;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
bool g_runtime_ready = false;
bool g_explosions_ready = false;
std::unordered_map<void*, WeaponPolicy> g_weapon_policies;
std::unordered_map<void*, bool> g_mother_classes;
std::unordered_map<std::uint32_t, Infection> g_infections;
std::unordered_map<std::uint32_t, LarvaRoute> g_larva_routes;

bool runtime_enabled() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    static std::uint64_t activation = 0;
    const auto current = a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE);
    struct Context { std::uint64_t* previous; std::uint64_t current; } context{
        &activation, current};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        const auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: g_infections.clear(); break;
        case 1: g_larva_routes.clear(); break;
        case 2: *state.previous = state.current; break;
        default: break;
        }
        return 0;
    };
    return a2fo_rs_stage8_infestation_activation(
        current, activation, step, &context) != 0;
#else
    return g_runtime_ready;
#endif
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void log_text(const std::string& message) noexcept {
    log_line(message.c_str());
}

template <typename T>
T* at(std::uintptr_t rva) noexcept {
    return g_armada
        ? reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(g_armada) + rva)
        : nullptr;
}

void* at(std::uintptr_t rva) noexcept {
    return at<void>(rva);
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto end = begin + size;
    if (end < begin) return false;
    std::uintptr_t cursor = begin;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor),
                          &information, sizeof(information)) ||
            information.State != MEM_COMMIT ||
            (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            return false;
        }
        const std::uintptr_t region_end =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
            static_cast<std::uintptr_t>(information.RegionSize);
        if (region_end <= cursor) return false;
        cursor = std::min(region_end, end);
    }
    return true;
}

template <typename T>
T read_at(const void* base, std::size_t offset, T fallback = {}) noexcept {
    if (!base) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(base) + offset;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template <std::size_t N>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[N]) noexcept {
    const void* address = at(rva);
    return readable_range(address, N) &&
        std::memcmp(address, expected, N) == 0;
}

bool original_or_detoured(std::uintptr_t rva, const std::uint8_t* expected,
                          std::size_t expected_size) noexcept {
    const auto* address = static_cast<const std::uint8_t*>(at(rva));
    if (!readable_range(address, std::max<std::size_t>(expected_size, 1))) {
        return false;
    }
    return std::memcmp(address, expected, expected_size) == 0 ||
        address[0] == 0xe9;
}

void* object_class(const void* object) noexcept {
    return read_at<void*>(object, kObjectClassOffset, nullptr);
}

std::uint32_t object_handle(const void* object) noexcept {
    return read_at<std::uint32_t>(object, kObjectHandleOffset, 0);
}

std::int32_t object_team(const void* object) noexcept {
    return read_at<std::int32_t>(object, kObjectTeamOffset, -1);
}

bool object_expired(const void* object) noexcept {
    return read_at<std::uint8_t>(object, kObjectExpiredOffset, 1) != 0;
}

Vector3 object_position(const void* object) noexcept {
    const auto position = read_at<std::array<float, 3>>(
        object, kObjectPositionOffset, {});
    return {position[0], position[1], position[2]};
}

float distance_squared(const Vector3& left, const Vector3& right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return x * x + y * y + z * z;
}

void* find_entity(std::uint32_t handle) noexcept {
    if (!g_armada || handle == 0) return nullptr;
    using EntityGetFn = void* (__cdecl*)(std::uint32_t);
    const auto get = reinterpret_cast<EntityGetFn>(at(kEntityGetRva));
    return get ? get(handle) : nullptr;
}

void* find_craft(std::uint32_t handle) noexcept {
    if (!g_armada || handle == 0) return nullptr;
    using CraftGetFn = void* (__cdecl*)(std::uint32_t);
    const auto get = reinterpret_cast<CraftGetFn>(at(kCraftGetRva));
    return get ? get(handle) : nullptr;
}

bool is_live_craft(const void* object) noexcept {
    if (!object || object_expired(object)) return false;
    const auto handle = object_handle(object);
    return handle != 0 && find_craft(handle) == object;
}

void* find_class(const std::string& odf) noexcept {
    if (!g_armada || odf.empty()) return nullptr;
    using FindClassFn = void* (__cdecl*)(const char*);
    const auto find = reinterpret_cast<FindClassFn>(at(kGameObjectClassFindRva));
    return find ? find(odf.c_str()) : nullptr;
}

bool object_transform(void* object, Matrix34* output) noexcept {
    if (!object || !output || !g_armada) return false;
    const void* transform = reinterpret_cast<const void*>(
        a2fo_infestation_call_thiscall_0(
            at(kEntityGetTransformRva), object));
    if (!readable_range(transform, sizeof(*output))) return false;
    std::memcpy(output, transform, sizeof(*output));
    return true;
}

void add_to_current_mission(void* object) noexcept {
    struct Context { void* mission = nullptr; void* add = nullptr; } context;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_armada != nullptr;
        case 1: {
            using GetCurrentFn = void* (__cdecl*)();
            const auto get_current = reinterpret_cast<GetCurrentFn>(
                at(kAiMissionGetCurrentRva));
            state.mission = get_current ? get_current() : nullptr;
            return reinterpret_cast<std::uintptr_t>(state.mission);
        }
        case 2: {
            void* vtable = read_at<void*>(state.mission, 0, nullptr);
            state.add = read_at<void*>(vtable, 0x18, nullptr);
            return reinterpret_cast<std::uintptr_t>(state.add);
        }
        case 3:
            a2fo_infestation_call_thiscall_1(state.add, state.mission, argument);
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_add_to_mission(
        reinterpret_cast<std::uintptr_t>(object), step, &context);
}

void* construct_object(void* object_class_pointer, const Matrix34& transform,
                       std::int32_t team, const char* label) noexcept {
    struct Context {
        const Matrix34* transform;
        std::int32_t team;
        const char* label;
    } context{&transform, team, label};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_armada != nullptr;
        case 1: return a2fo_infestation_call_thiscall_4(
            at(kGameObjectClassConstructRva), reinterpret_cast<void*>(argument),
            reinterpret_cast<std::uintptr_t>(state.transform),
            static_cast<std::uintptr_t>(state.team), 0,
            reinterpret_cast<std::uintptr_t>(state.label ? state.label : "A2FOINF"));
        case 2: add_to_current_mission(reinterpret_cast<void*>(argument)); return 0;
        default: return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_stage8_infestation_construct(
        reinterpret_cast<std::uintptr_t>(object_class_pointer), team,
        step, &context));
}

bool is_mother_class(void* cls) noexcept {
    const auto found = g_mother_classes.find(cls);
    return found != g_mother_classes.end() && found->second;
}

void* nearest_mother(const Vector3& origin, std::int32_t team) noexcept {
    struct Context {
        Vector3 origin;
        std::int32_t team;
        void* head = nullptr;
        std::uint32_t count = 0;
        float distance = 0.0f;
        float best_distance = 0.0f;
    } context{origin, team};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: {
            if (!g_armada) return 0;
            void* list = read_at<void*>(at(kGameObjectListPointerRva), 0, nullptr);
            if (!readable_range(list, 12)) return 0;
            state.head = read_at<void*>(list, 4, nullptr);
            state.count = read_at<std::uint32_t>(list, 8, 0);
            if (!readable_range(state.head, 12) || state.count > kMaximumObjects) {
                return 0;
            }
            return reinterpret_cast<std::uintptr_t>(state.head);
        }
        case 1: return state.count;
        case 2: return reinterpret_cast<std::uintptr_t>(
            read_at<void*>(object, 0, nullptr));
        case 3: return reinterpret_cast<std::uintptr_t>(
            read_at<void*>(object, 0, nullptr));
        case 4: return reinterpret_cast<std::uintptr_t>(
            read_at<void*>(object, 8, nullptr));
        case 5: return object_expired(object);
        case 6: return object_team(object) == state.team;
        case 7: return is_mother_class(object_class(object));
        case 8:
            state.distance = distance_squared(state.origin, object_position(object));
            return 0;
        case 9: return object_handle(object);
        case 10:
            if (state.distance < state.best_distance) return 1;
            if (state.distance == state.best_distance) return 2;
            return 0;
        case 11: state.best_distance = state.distance; return 0;
        default: return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_stage8_infestation_nearest_mother(
        step, &context));
}

void issue_move(void* craft, const Vector3& destination) noexcept {
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: return object_expired(object);
        case 1:
            a2fo_infestation_call_thiscall_command_vector_int(
                at(kDequeueCommandVectorRva), object, kMoveSingleCommand,
                opaque, 0);
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_move(
        reinterpret_cast<std::uintptr_t>(craft), step,
        const_cast<Vector3*>(&destination));
}

std::uint32_t craft_command(void* craft) noexcept {
    return read_at<std::uint32_t>(
        read_at<void*>(craft, kObjectCommandCarrierOffset, nullptr),
        0x3c, ~0u);
}

bool admitted_to_repair_yard(void* craft) noexcept {
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        void* source = opaque;
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: return craft_command(source);
        case 1: return read_at<std::uint32_t>(source, kCraftQueueStageOffset, 0);
        case 2: return read_at<std::uint32_t>(source, kCraftQueueOwnerOffset, 0);
        case 3: return reinterpret_cast<std::uintptr_t>(
            find_entity(static_cast<std::uint32_t>(argument)));
        case 4: return object_expired(object);
        case 5: return static_cast<std::uint32_t>(object_team(object));
        case 6: return static_cast<std::uint32_t>(object_team(source));
        case 7: {
            void* repair_queue = read_at<void*>(object, kYardRepairQueueOffset, nullptr);
            return read_at<void*>(repair_queue, 0, nullptr) ==
                at(kRepairQueueVtableRva);
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_infestation_repair_admitted(
        reinterpret_cast<std::uintptr_t>(craft), step, craft) != 0;
}

bool parameter_db_project_id(void* parameter_db, const char* command,
                             std::uint32_t* project_id) noexcept {
    struct Context { const char* command; std::uint32_t* output; } context{
        command, project_id};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_explosions_ready;
        case 1: *state.output = 0; return 0;
        case 2: return a2fo_infestation_call_thiscall_3(
            at(kParameterDbGetProjectIdRva), reinterpret_cast<void*>(argument),
            reinterpret_cast<std::uintptr_t>(state.command),
            reinterpret_cast<std::uintptr_t>(state.output), 0);
        default: return 0;
        }
    };
    return a2fo_rs_stage8_infestation_project_id(
        reinterpret_cast<std::uintptr_t>(parameter_db),
        reinterpret_cast<std::uintptr_t>(command),
        reinterpret_cast<std::uintptr_t>(project_id), step, &context) != 0;
}

void spawn_explosion(const Matrix34& transform,
                     std::uint32_t project_id) noexcept {
    struct Context {
        const Matrix34* transform;
        std::uint32_t project_id;
        void* explosion_class = nullptr;
        void* build = nullptr;
    } context{&transform, project_id};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_explosions_ready;
        case 1: {
            using ExplosionClassFindFn = void* (__cdecl*)(const std::uint32_t*);
            const auto find = reinterpret_cast<ExplosionClassFindFn>(
                at(kExplosionClassFindRva));
            state.explosion_class = find ? find(&state.project_id) : nullptr;
            return reinterpret_cast<std::uintptr_t>(state.explosion_class);
        }
        case 2: {
            void* vtable = read_at<void*>(state.explosion_class, 0, nullptr);
            state.build = read_at<void*>(vtable, 0x08, nullptr);
            return state.build && readable_range(state.build, 1)
                ? reinterpret_cast<std::uintptr_t>(state.build) : 0;
        }
        case 3: {
            constexpr float scale = 1.0f;
            std::uint32_t scale_bits = 0;
            std::memcpy(&scale_bits, &scale, sizeof(scale_bits));
            a2fo_infestation_call_thiscall_2(
                state.build, state.explosion_class,
                reinterpret_cast<std::uintptr_t>(state.transform), scale_bits);
            return 0;
        }
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_explosion(project_id, step, &context);
}

void apply_internal_damage(void* craft, float amount, bool bypass_shields,
                           std::uint32_t attacker_handle) noexcept {
    struct Context {
        float amount;
        bool bypass_shields;
        std::uint32_t attacker_handle;
    } context{amount, bypass_shields, attacker_handle};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto& state = *static_cast<Context*>(opaque);
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: return is_live_craft(object);
        case 1: {
            // DamageInfo belongs to a zeroed synthetic ordnance. Its Weapon*
            // is null, so native damage bridges cannot infer weapon policy.
            std::array<std::uint8_t, kSyntheticDamageBlockSize> block{};
            auto* damage_info = block.data() + kDamageInfoOnSyntheticOrdnanceOffset;
            std::memcpy(damage_info + kDamageInfoDamageOffset,
                        &state.amount, sizeof(state.amount));
            const std::uint32_t system_type =
                std::numeric_limits<std::uint32_t>::max();
            std::memcpy(damage_info + kDamageInfoSystemTypeOffset,
                        &system_type, sizeof(system_type));
            const std::uint32_t flags = state.bypass_shields
                ? kIgnoreShieldsDamageFlag : 0u;
            std::memcpy(damage_info + kDamageInfoFlagsOffset, &flags, sizeof(flags));
            std::memcpy(damage_info + kDamageInfoAttackerHandleOffset,
                        &state.attacker_handle, sizeof(state.attacker_handle));
            // Final argument 1 avoids DamageInfo::Resolve on a synthetic block.
            a2fo_infestation_call_thiscall_4(
                at(kCraftDamageRva), object,
                reinterpret_cast<std::uintptr_t>(damage_info), 0, 0, 1);
            return 0;
        }
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_damage(
        reinterpret_cast<std::uintptr_t>(craft), amount, step, &context);
}

std::uint32_t safe_attacker_handle(const Infection& infection) noexcept {
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: return reinterpret_cast<std::uintptr_t>(
            find_craft(static_cast<std::uint32_t>(argument)));
        case 1: return object_expired(object);
        case 2: return static_cast<std::uint32_t>(object_team(object));
        default: return 0;
        }
    };
    return a2fo_rs_stage8_infestation_attacker_handle(
        infection.infector_handle, infection.infector_team, step, nullptr);
}

Matrix34 larva_transform(const Matrix34& source, std::uint32_t index,
                         std::uint32_t count, float radius) noexcept {
    Matrix34 result = source;
    if (count <= 1 || !std::isfinite(radius) || radius <= 0.0f) return result;
    constexpr double kTau = 6.28318530717958647692;
    const double angle = kTau * static_cast<double>(index) /
                         static_cast<double>(count);
    const float right = static_cast<float>(std::cos(angle) * radius);
    const float forward = static_cast<float>(std::sin(angle) * radius);
    result.values[9] += result.values[0] * right + result.values[6] * forward;
    result.values[10] += result.values[1] * right + result.values[7] * forward;
    result.values[11] += result.values[2] * right + result.values[8] * forward;
    return result;
}

void spawn_larvae(const Infection& infection,
                  const Matrix34& source_transform) noexcept {
    struct Context {
        const Infection* infection;
        const Matrix34* source_transform;
        void* larva_class = nullptr;
        void* mother = nullptr;
        std::uint32_t mother_handle = 0;
        Vector3 mother_position{};
    } context{&infection, &source_transform};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0:
            state.larva_class = find_class(state.infection->policy.larva_odf);
            return reinterpret_cast<std::uintptr_t>(state.larva_class);
        case 1:
            log_text("Infestation hatch could not find larva ODF '" +
                     state.infection->policy.larva_odf + "'");
            return 0;
        case 2: {
            const Vector3 source_position{
                state.source_transform->values[9],
                state.source_transform->values[10],
                state.source_transform->values[11]};
            state.mother = nearest_mother(source_position,
                                          state.infection->infector_team);
            return reinterpret_cast<std::uintptr_t>(state.mother);
        }
        case 3:
            state.mother_handle = object_handle(object);
            return state.mother_handle;
        case 4: state.mother_position = object_position(object); return 0;
        case 5: {
            const auto index = static_cast<std::uint32_t>(argument);
            const Matrix34 transform = larva_transform(
                *state.source_transform, index,
                state.infection->policy.larva_count,
                state.infection->policy.spawn_radius);
            char label[32]{};
            std::snprintf(label, sizeof(label), "A2FOINF%02lu",
                          static_cast<unsigned long>(index));
            return reinterpret_cast<std::uintptr_t>(construct_object(
                state.larva_class, transform,
                state.infection->infector_team, label));
        }
        case 6: return object_handle(object);
        case 7: {
            const auto handle = static_cast<std::uint32_t>(argument);
            LarvaRoute route;
            route.larva_handle = handle;
            route.mother_handle = state.mother_handle;
            route.team = state.infection->infector_team;
            route.refresh_seconds = 0.0f;
            g_larva_routes[handle] = route;
            return 0;
        }
        case 8: issue_move(object, state.mother_position); return 0;
        case 9: {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "Infestation hatched: released %lu/%lu larvae for team %ld%s",
                static_cast<unsigned long>(argument),
                static_cast<unsigned long>(state.infection->policy.larva_count),
                static_cast<long>(state.infection->infector_team),
                state.mother ? " and rallied them to the nearest mother"
                             : "; no mother currently available");
            log_line(message);
            return 0;
        }
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_spawn_larvae(
        infection.policy.larva_count, infection.infector_team,
        !infection.policy.larva_odf.empty(), step, &context);
}

void hatch(void* craft, Infection infection) noexcept {
    struct Context {
        void* craft;
        const Infection* infection;
        Matrix34 transform{};
    } context{craft, &infection};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return object_transform(state.craft, &state.transform);
        case 1:
            log_line("Infestation hatch aborted: target transform unavailable");
            return 0;
        case 2:
            spawn_explosion(state.transform,
                            state.infection->policy.explosion_project_id);
            return 0;
        case 3: return safe_attacker_handle(*state.infection);
        case 4:
            apply_internal_damage(state.craft, state.infection->policy.damage,
                state.infection->policy.bypass_shields,
                static_cast<std::uint32_t>(argument));
            return 0;
        case 5: spawn_larvae(*state.infection, state.transform); return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_hatch(
        reinterpret_cast<std::uintptr_t>(craft), step, &context);
}

void update_larva_route(void* larva, float elapsed_seconds) noexcept {
    struct Context {
        void* larva;
        float elapsed;
        decltype(g_larva_routes)::iterator found{};
        Vector3 destination{};
    } context{larva, elapsed_seconds};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: return object_handle(state.larva);
        case 1:
            state.found = g_larva_routes.find(static_cast<std::uint32_t>(argument));
            return state.found != g_larva_routes.end();
        case 2: return object_team(state.larva) == state.found->second.team;
        case 3: g_larva_routes.erase(state.found); return 0;
        case 4: state.found->second.refresh_seconds += state.elapsed; return 0;
        case 5: return state.found->second.refresh_seconds <
            kLarvaMoveRefreshSeconds;
        case 6: state.found->second.refresh_seconds = 0.0f; return 0;
        case 7: return state.found->second.mother_handle;
        case 8: return reinterpret_cast<std::uintptr_t>(
            find_entity(static_cast<std::uint32_t>(argument)));
        case 9: return object_expired(object);
        case 10: return object_team(object) == state.found->second.team;
        case 11: return is_mother_class(object_class(object));
        case 12: return reinterpret_cast<std::uintptr_t>(nearest_mother(
            object_position(state.larva), state.found->second.team));
        case 13:
            state.found->second.mother_handle = object ? object_handle(object) : 0;
            return 0;
        case 14:
            state.destination = object_position(object);
            return distance_squared(object_position(state.larva), state.destination) <=
                kMotherArrivalRadius * kMotherArrivalRadius;
        case 15: g_larva_routes.erase(state.found); return 0;
        case 16: issue_move(state.larva, state.destination); return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_update_route(
        reinterpret_cast<std::uintptr_t>(larva), elapsed_seconds,
        step, &context);
}

void register_weapon_policy(const A2FO_WeaponClassLoadedEvent* event) noexcept {
    struct Context {
        const A2FO_WeaponClassLoadedEvent* event;
        WeaponPolicy policy{};
        std::exception_ptr fatal_error;
    } context{};
    context.event = event;
    const auto lower = [](void*, std::uint8_t byte) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(std::tolower(byte));
    };
    const auto field = [](void* opaque, std::uint32_t index,
                          A2foStage8InfestationBytes* name,
                          A2foStage8InfestationBytes* value) noexcept -> std::uint8_t {
        const auto& state = *static_cast<Context*>(opaque);
        if (state.fatal_error || !state.event->odf_fields ||
            index >= state.event->odf_field_count) return 0;
        const auto& source = state.event->odf_fields[index];
        if ((!source.name.data && source.name.size != 0) ||
            (!source.value.data && source.value.size != 0)) return 0;
        *name = {reinterpret_cast<const std::uint8_t*>(source.name.data),
                 source.name.size};
        *value = {reinterpret_cast<const std::uint8_t*>(source.value.data),
                  source.value.size};
        return 1;
    };
    const auto parse_float_native = [](void* opaque,
            A2foStage8InfestationBytes value, float* output) noexcept -> std::uint8_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.fatal_error) return 2;
        try {
            const std::string text(reinterpret_cast<const char*>(value.data), value.size);
            char* end = nullptr;
            *output = std::strtof(text.c_str(), &end);
            return end && end != text.c_str() ? 1 : 0;
        } catch (...) {
            state.fatal_error = std::current_exception();
            return 2;
        }
    };
    const auto parse_count_native = [](void* opaque,
            A2foStage8InfestationBytes value,
            std::uint32_t* output) noexcept -> std::uint8_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.fatal_error) return 2;
        try {
            const std::string text(reinterpret_cast<const char*>(value.data), value.size);
            char* end = nullptr;
            *output = static_cast<std::uint32_t>(
                std::strtoul(text.c_str(), &end, 10));
            return end && end != text.c_str() ? 1 : 0;
        } catch (...) {
            state.fatal_error = std::current_exception();
            return 2;
        }
    };
    const auto project = [](void* opaque, std::uint32_t* output) noexcept -> std::uint8_t {
        const auto& state = *static_cast<Context*>(opaque);
        if (state.fatal_error) return 2;
        parameter_db_project_id(state.event->parameter_db,
                                "infestationExplosion", output);
        return 1;
    };
    const auto store = [](void* opaque,
            const A2foStage8InfestationPolicy* parsed) noexcept -> std::uint8_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.fatal_error) return 0;
        try {
            if (!parsed) {
                g_weapon_policies.erase(state.event->weapon_class);
                return 1;
            }
            state.policy.enabled = parsed->enabled != 0;
            state.policy.delay_seconds = parsed->delay_seconds;
            state.policy.damage = parsed->damage;
            state.policy.larva_odf.assign(
                parsed->larva_odf.size != 0
                    ? reinterpret_cast<const char*>(parsed->larva_odf.data) : "",
                parsed->larva_odf.size);
            state.policy.larva_count = parsed->larva_count;
            state.policy.explosion_project_id = parsed->explosion_project_id;
            state.policy.bypass_shields = parsed->bypass_shields != 0;
            state.policy.spawn_radius = parsed->spawn_radius;
            g_weapon_policies[state.event->weapon_class] = state.policy;
            return 1;
        } catch (...) {
            state.fatal_error = std::current_exception();
            return 0;
        }
    };
    const auto log = [](void* opaque) noexcept {
        const auto& policy = static_cast<Context*>(opaque)->policy;
        char message[320]{};
        std::snprintf(message, sizeof(message),
            "Registered infestation weapon: delay=%.2fs damage=%.2f larvae=%lu '%s' bypassShields=%s explosion=%lu",
            static_cast<double>(policy.delay_seconds),
            static_cast<double>(policy.damage),
            static_cast<unsigned long>(policy.larva_count),
            policy.larva_odf.empty() ? "<none>" : policy.larva_odf.c_str(),
            policy.bypass_shields ? "yes" : "no",
            static_cast<unsigned long>(policy.explosion_project_id));
        log_line(message);
    };
    a2fo_rs_stage8_infestation_weapon_policy(
        event ? reinterpret_cast<std::uintptr_t>(event->weapon_class) : 0,
        event && event->odf_fields ? event->odf_field_count : 0,
        lower, field, parse_float_native, parse_count_native,
        project, store, log, &context);
    if (context.fatal_error) std::rethrow_exception(context.fatal_error);
}

void A2FO_CALL weapon_class_loaded(
    const A2FO_WeaponClassLoadedEvent* event, void*) {
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        const auto* value = static_cast<const A2FO_WeaponClassLoadedEvent*>(opaque);
        switch (operation) {
        case 0: return g_runtime_ready;
        case 1: return value->struct_size;
        case 3: register_weapon_policy(value); return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_class_event(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event), 0,
        step, const_cast<A2FO_WeaponClassLoadedEvent*>(event));
}

void A2FO_CALL object_class_loaded(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    struct Context {
        const A2FO_GameObjectClassLoadedEvent* event;
        bool failed = false;
    } context{event, false};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return g_runtime_ready;
        case 1: return state.event->struct_size;
        case 2: return reinterpret_cast<std::uintptr_t>(state.event->object_class);
        case 3: {
            const auto lower = [](void*, std::uint8_t byte) noexcept -> std::uint8_t {
                return static_cast<std::uint8_t>(std::tolower(byte));
            };
            const auto field = [](void* opaque, std::uint32_t index,
                    A2foStage8InfestationBytes* name,
                    A2foStage8InfestationBytes* value) noexcept -> std::uint8_t {
                const auto& state = *static_cast<Context*>(opaque);
                if (!state.event->odf_fields ||
                    index >= state.event->odf_field_count) return 0;
                const auto& source = state.event->odf_fields[index];
                if ((!source.name.data && source.name.size != 0) ||
                    (!source.value.data && source.value.size != 0)) return 0;
                *name = {reinterpret_cast<const std::uint8_t*>(source.name.data),
                         source.name.size};
                *value = {reinterpret_cast<const std::uint8_t*>(source.value.data),
                          source.value.size};
                return 1;
            };
            const auto store = [](void* opaque,
                                  std::uint8_t mother) noexcept -> std::uint8_t {
                auto& state = *static_cast<Context*>(opaque);
                try {
                    if (mother) g_mother_classes[state.event->object_class] = true;
                    else g_mother_classes.erase(state.event->object_class);
                    return 1;
                } catch (...) {
                    state.failed = true;
                    return 0;
                }
            };
            a2fo_rs_stage8_infestation_mother_metadata(
                reinterpret_cast<std::uintptr_t>(state.event->object_class),
                state.event->odf_fields ? state.event->odf_field_count : 0,
                lower, field, store, &state);
            return 0;
        }
        default: return 0;
        }
    };
    a2fo_rs_stage8_infestation_class_event(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event), 1,
        step, &context);
    if (context.failed) {
        log_line("Could not register breederMother metadata for infestation rallying");
    }
}

const WeaponPolicy* policy_for_weapon(void* weapon) noexcept {
    if (!weapon) return nullptr;
    void* cls = read_at<void*>(weapon, kWeaponClassOffset, nullptr);
    const auto found = g_weapon_policies.find(cls);
    return found == g_weapon_policies.end() ? nullptr : &found->second;
}

void* weapon_owner(void* weapon) noexcept {
    if (!weapon) return nullptr;
    return reinterpret_cast<void*>(a2fo_infestation_call_thiscall_0(
        at(kWeaponGetOwnerRva), weapon));
}

bool valid_infestation_target(void* weapon, const void* target,
                              const WeaponPolicy** policy_out,
                              void** owner_out) noexcept {
    struct Context {
        const WeaponPolicy* policy = nullptr;
        void* owner = nullptr;
        const WeaponPolicy** policy_out;
        void** owner_out;
    } context{};
    context.policy_out = policy_out;
    context.owner_out = owner_out;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0:
            state.policy = policy_for_weapon(object);
            return reinterpret_cast<std::uintptr_t>(state.policy);
        case 1: return state.policy->enabled;
        case 2: return is_live_craft(object);
        case 3:
            state.owner = weapon_owner(object);
            return reinterpret_cast<std::uintptr_t>(state.owner);
        case 4: return is_live_craft(object);
        case 5: return static_cast<std::uint32_t>(object_team(object));
        case 6: return static_cast<std::uint32_t>(object_team(object));
        case 7: return object_handle(object);
        case 8: return g_infections.find(static_cast<std::uint32_t>(argument)) !=
            g_infections.end();
        case 9:
            if (state.policy_out) *state.policy_out = state.policy;
            if (state.owner_out) *state.owner_out = state.owner;
            return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_infestation_target_valid(
        reinterpret_cast<std::uintptr_t>(weapon),
        reinterpret_cast<std::uintptr_t>(target), step, &context) != 0;
}

bool A2FO_CALL weapon_trigger(const A2FO_WeaponTriggerEvent* event, void*) {
    struct Context {
        const A2FO_WeaponTriggerEvent* event;
        const WeaponPolicy* policy = nullptr;
        void* owner = nullptr;
        Infection infection{};
        std::exception_ptr error;
    } context{};
    context.event = event;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.error) return UINTPTR_MAX;
        const auto* event = state.event;
        try {
            switch (operation) {
            case 0: return runtime_enabled();
            case 1: return event->struct_size;
            case 2: {
                const bool valid = valid_infestation_target(
                    event->weapon, event->target,
                    &state.policy, &state.owner);
                return valid;
            }
            case 3: return event->kind;
            case 4: return policy_for_weapon(event->weapon) != nullptr;
            case 5: return state.policy != nullptr;
            case 6: return state.owner != nullptr;
            case 7:
                state.infection.target_handle = object_handle(event->target);
                return 0;
            case 8:
                state.infection.infector_handle = object_handle(state.owner);
                return 0;
            case 9:
                state.infection.infector_team = object_team(state.owner);
                return 0;
            case 10:
                state.infection.remaining_seconds = state.policy->delay_seconds;
                return 0;
            case 11: state.infection.policy = *state.policy; return 0;
            case 12:
                g_infections[state.infection.target_handle] = state.infection;
                return 0;
            case 13: {
                char message[256]{};
                std::snprintf(message, sizeof(message),
                    "Craft %lu infested by team %ld; hatch in %.2fs",
                    static_cast<unsigned long>(state.infection.target_handle),
                    static_cast<long>(state.infection.infector_team),
                    static_cast<double>(state.infection.remaining_seconds));
                log_line(message);
                return 0;
            }
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool result = a2fo_rs_stage8_infestation_trigger(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event),
        A2FO_WEAPON_TRIGGER_PRECHECK, A2FO_WEAPON_TRIGGER_COMMITTED,
        step, &context) != 0;
    if (context.error) {
        log_line("Infestation weapon trigger handling failed");
    }
    return context.error ? true : result;
}

void A2FO_CALL craft_event(const A2FO_CraftEvent* event, void*) {
    struct Context {
        const A2FO_CraftEvent* event;
        decltype(g_infections)::iterator found{};
        Infection infection{};
        std::exception_ptr error;
    } context{};
    context.event = event;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.error) return UINTPTR_MAX;
        const auto* event = state.event;
        try {
            switch (operation) {
            case 0: return runtime_enabled();
            case 1: return event->struct_size;
            case 2: return reinterpret_cast<std::uintptr_t>(event->craft);
            case 3: return object_handle(event->craft);
            case 4: return event->kind;
            case 5: g_infections.erase(static_cast<std::uint32_t>(argument)); return 0;
            case 6: g_larva_routes.erase(static_cast<std::uint32_t>(argument)); return 0;
            case 7: update_larva_route(event->craft, event->elapsed_seconds); return 0;
            case 8:
                state.found = g_infections.find(static_cast<std::uint32_t>(argument));
                return state.found != g_infections.end();
            case 9: return admitted_to_repair_yard(event->craft);
            case 10: g_infections.erase(state.found); return 0;
            case 11: {
                char message[160]{};
                std::snprintf(message, sizeof(message),
                    "Infestation removed from Craft %lu by yard repair",
                    static_cast<unsigned long>(argument));
                log_line(message);
                return 0;
            }
            case 12: {
                std::uint32_t bits = 0;
                std::memcpy(&bits, &event->elapsed_seconds, sizeof(bits));
                return bits;
            }
            case 13:
                state.found->second.remaining_seconds -= event->elapsed_seconds;
                return 0;
            case 14: return state.found->second.remaining_seconds > 0.0f;
            case 15: state.infection = std::move(state.found->second); return 0;
            case 16: g_infections.erase(state.found); return 0;
            case 17: hatch(event->craft, std::move(state.infection)); return 0;
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_infestation_craft_event(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event),
        A2FO_CRAFT_EVENT_CLEANUP, A2FO_CRAFT_EVENT_POST_LOAD,
        A2FO_CRAFT_EVENT_SIMULATE_POST, step, &context);
    if (context.error) {
        log_line("Infestation Craft simulation failed");
    }
}

bool runtime_signatures_supported() noexcept {
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        switch (operation) {
        case 0: return signature_matches(
            kAiMissionGetCurrentRva, kExpectedAiMissionGetCurrent);
        case 1: return signature_matches(kCraftGetRva, kExpectedCraftGet);
        case 2: return original_or_detoured(
            kCraftDamageRva, kExpectedCraftDamage, sizeof(kExpectedCraftDamage));
        case 3: return signature_matches(
            kGameObjectClassFindRva, kExpectedGameObjectClassFind);
        case 4: return signature_matches(
            kGameObjectClassConstructRva, kExpectedGameObjectClassConstruct);
        case 5: return signature_matches(
            kEntityGetTransformRva, kExpectedEntityGetTransform);
        case 6: return signature_matches(kEntityGetRva, kExpectedEntityGet);
        case 7: return signature_matches(
            kDequeueCommandVectorRva, kExpectedDequeueCommandVector);
        case 8: return signature_matches(kWeaponGetOwnerRva, kExpectedWeaponGetOwner);
        default: {
            constexpr const char* labels[]{
                "AIMission::GetCurrent", "Craft::Get", "Craft::Damage",
                "GameObjectClass::Find", "GameObjectClass::Construct",
                "Entity::GetTransform", "Entity::Get", "DequeueCommand(vector)",
                "Weapon::GetOwner"};
            if (operation < 9 || operation >= 18) return 0;
            std::string message =
                "Infestation unavailable: native signature mismatch at ";
            message += labels[operation - 9];
            log_text(message);
            return 0;
        }
        }
    };
    return a2fo_rs_stage8_infestation_signatures(step, nullptr) != 0;
}

} // namespace

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(
    const A2FO_ModuleApi* api) {
    struct Context {
        const A2FO_ModuleApi* api;
        std::exception_ptr error;
    } context{api, nullptr};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        const auto* api = state.api;
        if (state.error) return UINTPTR_MAX;
        try {
            switch (operation) {
            case 0:
                return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                    api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                    api->armada_module &&
                    A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) &&
                    A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) &&
                    A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler_masked) &&
                    A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) &&
                    api->register_game_object_class_loaded_handler &&
                    api->register_weapon_class_loaded_handler &&
                    api->register_weapon_trigger_handler_masked &&
                    api->register_craft_event_handler_masked &&
                    (api->capabilities & (A2FO_CAP_GAME_OBJECT_CLASS_LOADED |
                        A2FO_CAP_WEAPON_CLASS_LOADED |
                        A2FO_CAP_WEAPON_TRIGGER_EVENTS |
                        A2FO_CAP_CRAFT_EVENTS)) ==
                        (A2FO_CAP_GAME_OBJECT_CLASS_LOADED |
                         A2FO_CAP_WEAPON_CLASS_LOADED |
                         A2FO_CAP_WEAPON_TRIGGER_EVENTS |
                         A2FO_CAP_CRAFT_EVENTS);
            case 1:
                g_api = api;
                g_armada = static_cast<HMODULE>(api->armada_module());
                return g_armada != nullptr;
            case 2: return runtime_signatures_supported();
            case 3:
                g_explosions_ready =
                    signature_matches(kParameterDbGetProjectIdRva,
                                      kExpectedParameterDbGetProjectId) &&
                    signature_matches(kExplosionClassFindRva,
                                      kExpectedExplosionClassFind);
                return g_explosions_ready;
            case 4:
                log_line("Infestation explosion helpers unavailable; damage and larvae remain enabled");
                return 0;
            case 5: g_runtime_ready = true; return 0;
            case 6: return api->register_game_object_class_loaded_handler(
                kModuleName, kObjectFields.data(),
                static_cast<std::uint32_t>(kObjectFields.size()),
                &object_class_loaded, nullptr);
            case 7: return api->register_weapon_class_loaded_handler(
                kModuleName, kWeaponFields.data(),
                static_cast<std::uint32_t>(kWeaponFields.size()),
                &weapon_class_loaded, nullptr);
            case 8: return api->register_weapon_trigger_handler_masked(
                kModuleName, A2FO_WEAPON_TRIGGER_EVENT_MASK_ALL,
                &weapon_trigger, nullptr);
            case 9: return api->register_craft_event_handler_masked(
                kModuleName,
                A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                    A2FO_CRAFT_EVENT_MASK_CLEANUP |
                    A2FO_CRAFT_EVENT_MASK_POST_LOAD,
                &craft_event, nullptr);
            case 10: g_runtime_ready = false; return 0;
            case 11:
                log_line("Infestation disabled: shared dispatcher registration failed");
                return 0;
            case 12:
                log_line("Noxter infestation initialized: delayed hatch, internal damage, larvae rallying, and yard cure enabled");
                return 0;
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool result = a2fo_rs_stage8_infestation_init(step, &context) != 0;
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        switch (operation) {
        case 0: g_runtime_ready = false; break;
        case 1: g_infections.clear(); break;
        case 2: g_larva_routes.clear(); break;
        case 3: g_weapon_policies.clear(); break;
        case 4: g_mother_classes.clear(); break;
        default: break;
        }
        return 0;
    };
    a2fo_rs_stage8_infestation_shutdown(step, nullptr);
}
