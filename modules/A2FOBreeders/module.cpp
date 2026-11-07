/*
 * A2FOBreeders
 *
 * Noxter-style breeder prototype:
 *   - classLabel = "breeder" aliases to Fleet Ops' native shipyard Producer.
 *   - a virgin breeder exposes ordinary buildItem entries as genetic choices.
 *   - choosing an eligible target is intercepted before native queue admission.
 *   - the nearest same-team breederMother within breederRadius emits the target's
 *     breederSeed object, which receives a native move order toward the breeder.
 *   - when the seed reaches breederConsumeRadius the breeder is swapped to the
 *     target's breederResult ODF.
 *   - a committed breeder has no visible buildItem list; breederProduces names
 *     the one real unit class repeatedly admitted through Producer::PushChecked.
 *
 * Phase-one limitations are documented in README.md.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/stage8_a_breeders_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

extern "C" std::uintptr_t a2fo_breeders_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_breeders_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_breeders_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_breeders_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
extern "C" void a2fo_breeders_call_thiscall_command_vector_int(
    void* function, void* self, std::int32_t command,
    const void* vector, std::int32_t value);

namespace {

constexpr char kModuleName[] = "A2FOBreeders";
constexpr float kDefaultMotherRadius = 600.0f;
constexpr float kDefaultConsumeRadius = 40.0f;
constexpr float kMoveRefreshSeconds = 1.0f;
constexpr float kProductionRetrySeconds = 0.50f;
constexpr std::uint32_t kMaximumObjects = 1000000;
constexpr std::int32_t kMoveSingleCommand = 5;

// ArmadaL.exe 1.1 / supported Fleet Operations Roots RVAs already used by
// A2FOSquadrons, A2FORefitYards and A2FOFeaturePack.
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x000cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x000cd390;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetRva = 0x000cfff0;
constexpr std::uintptr_t kCraftDoExpireRva = 0x000caae0;
constexpr std::uintptr_t kDequeueCommandVectorRva = 0x000d4850;
constexpr std::uintptr_t kEvolverSwapObjectsRva = 0x000b0e10;
constexpr std::uintptr_t kGameObjectListPointerRva = 0x00361084;

// FleetOpsHook.dll Producer local checked-admission path.  Calling this from
// the already-synchronised breeder request keeps normal cost/build-time rules
// without emitting another multiplayer command.
constexpr std::uintptr_t kFoProducerPushCheckedRva = 0x001229b8;

constexpr std::size_t kObjectExpiredOffset = 0x27;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectPositionOffset = 0xac;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kProducerCurrentBuildClassOffset = 0x254;
constexpr std::size_t kProducerQueueCountOffset = 0x274;

constexpr std::uint8_t kExpectedAiMissionGetCurrent[] = {
    0xa1, 0xdc, 0x47, 0x73, 0x00, 0xc3};
constexpr std::uint8_t kExpectedGameObjectClassFind[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedGameObjectClassConstruct[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedEntityGetTransform[] = {
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
constexpr std::uint8_t kExpectedEntityGet[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x08};
constexpr std::uint8_t kExpectedDequeueCommandVector[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x10};
constexpr std::uint8_t kExpectedEvolverSwapObjects[] = {
    0x55, 0x8b, 0xec, 0x53, 0x56, 0x57};
constexpr std::uint8_t kExpectedFoPushChecked[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53};

constexpr std::array<A2FO_ClasslabelOdfDefault, 5> kBreederDefaults{{
    {"breederClass", "1"},
    {"builder_ship", "1"},
    {"transporter", "1"},
    {"SHOW_MOVEMENT_AUTONOMY", "1"},
    {"SHOW_SW_AUTONOMY", "1"},
}};

constexpr std::array<const char*, 7> kRequiredFields{{
    "breederClass",
    "breederMother",
    "breederRadius",
    "breederSeed",
    "breederResult",
    "breederProduces",
    "breederConsumeRadius",
}};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Matrix34 {
    float values[12]{};
};

struct ClassPolicy {
    bool breeder = false;
    bool mother = false;
    float mother_radius = kDefaultMotherRadius;
    float consume_radius = kDefaultConsumeRadius;
    std::string seed_odf;
    std::string result_odf;
    std::string produces_odf;
};

struct WaitingSeed {
    std::uint32_t breeder_handle = 0;
    std::uint32_t seed_handle = 0;
    std::string result_odf;
    std::string target_odf;
    float move_refresh = 0.0f;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;
std::unordered_map<void*, ClassPolicy> g_class_policies;
std::unordered_map<std::uint32_t, WaitingSeed> g_waiting;
std::unordered_map<std::uint32_t, float> g_production_retry;

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
        case 0: g_waiting.clear(); break;
        case 1: g_production_retry.clear(); break;
        case 2: *state.previous = state.current; break;
        default: break;
        }
        return 0;
    };
    return a2fo_rs_stage8_breeder_activation(
        current, activation, step, &context) != 0;
#else
    return g_runtime_ready;
#endif
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

template <typename T>
T* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return at<void>(module, rva);
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
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[N]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, N) &&
        std::memcmp(address, expected, N) == 0;
}

std::string odf_key(std::string value) {
    struct Context {
        std::string result;
        std::exception_ptr error;
    } context;
    const auto lower = [](void*, std::uint8_t byte) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(
            std::tolower(static_cast<unsigned char>(byte)));
    };
    const auto emit = [](void* opaque, const std::uint8_t* bytes,
                         std::size_t size) noexcept {
        auto& state = *static_cast<Context*>(opaque);
        try {
            state.result.assign(reinterpret_cast<const char*>(bytes), size);
        } catch (...) {
            state.error = std::current_exception();
        }
    };
    a2fo_rs_stage8_breeder_odf_key(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size(),
        lower, emit, &context);
    if (context.error) std::rethrow_exception(context.error);
    return context.result;
}

const ClassPolicy* policy_for_class(const void* object_class) noexcept {
    const auto found = g_class_policies.find(const_cast<void*>(object_class));
    return found == g_class_policies.end() ? nullptr : &found->second;
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
    const auto get = reinterpret_cast<EntityGetFn>(at(g_armada, kEntityGetRva));
    return get ? get(handle) : nullptr;
}

void* find_class(const std::string& odf) noexcept {
    if (!g_armada || odf.empty()) return nullptr;
    using FindClassFn = void* (__cdecl*)(const char*);
    const auto find = reinterpret_cast<FindClassFn>(
        at(g_armada, kGameObjectClassFindRva));
    return find ? find(odf.c_str()) : nullptr;
}

bool object_transform(void* object, Matrix34* output) noexcept {
    if (!object || !output || !g_armada) return false;
    const void* transform = reinterpret_cast<const void*>(
        a2fo_breeders_call_thiscall_0(
            at(g_armada, kEntityGetTransformRva), object));
    if (!readable_range(transform, sizeof(*output))) return false;
    std::memcpy(output, transform, sizeof(*output));
    return true;
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
        a2fo_breeders_call_thiscall_1(
            add_object, mission, reinterpret_cast<std::uintptr_t>(object));
    }
}

void* construct_object(void* object_class_pointer, const Matrix34& transform,
                       std::int32_t team, const char* label) noexcept {
    if (!object_class_pointer || !g_armada || team < 0) return nullptr;
    void* object = reinterpret_cast<void*>(
        a2fo_breeders_call_thiscall_4(
            at(g_armada, kGameObjectClassConstructRva), object_class_pointer,
            reinterpret_cast<std::uintptr_t>(&transform),
            static_cast<std::uintptr_t>(team), 0,
            reinterpret_cast<std::uintptr_t>(label ? label : "A2FOBR")));
    if (object) add_to_current_mission(object);
    return object;
}

void expire_craft(void* craft) noexcept {
    if (!craft || object_expired(craft) || !g_armada) return;
    a2fo_breeders_call_thiscall_0(at(g_armada, kCraftDoExpireRva), craft);
}

void* nearest_mother(void* breeder) noexcept {
    struct Context {
        Vector3 breeder_position{};
        void* head = nullptr;
        std::uint32_t count = 0;
        float radius = 0.0f;
        float distance = 0.0f;
        float best_distance = 0.0f;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: return object_expired(object);
        case 1: return static_cast<std::uint32_t>(object_team(object));
        case 2: state.breeder_position = object_position(object); return 0;
        case 3: {
            if (!g_armada) return 0;
            void* list = read_at<void*>(at(g_armada, kGameObjectListPointerRva),
                                        0, nullptr);
            if (!readable_range(list, 12)) return 0;
            state.head = read_at<void*>(list, 4, nullptr);
            state.count = read_at<std::uint32_t>(list, 8, 0);
            if (!readable_range(state.head, 12) || state.count > kMaximumObjects) {
                return 0;
            }
            return reinterpret_cast<std::uintptr_t>(state.head);
        }
        case 4: return state.count;
        case 5: return reinterpret_cast<std::uintptr_t>(read_at<void*>(object, 0, nullptr));
        case 6: return reinterpret_cast<std::uintptr_t>(read_at<void*>(object, 0, nullptr));
        case 7: return reinterpret_cast<std::uintptr_t>(read_at<void*>(object, 8, nullptr));
        case 8: return object_expired(object);
        case 9: return static_cast<std::uint32_t>(object_team(object));
        case 10: {
            const ClassPolicy* policy = policy_for_class(object_class(object));
            if (!policy || !policy->mother || policy->mother_radius <= 0.0f) {
                return 0;
            }
            state.radius = policy->mother_radius;
            return 1;
        }
        case 11: {
            state.distance = distance_squared(
                state.breeder_position, object_position(object));
            const float maximum = state.radius * state.radius;
            return state.distance > maximum ? 0 : 1;
        }
        case 12: return object_handle(object);
        case 13:
            if (state.distance < state.best_distance) return 1;
            if (state.distance == state.best_distance) return 2;
            return 0;
        case 14: state.best_distance = state.distance; return 0;
        default: return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_stage8_breeder_nearest_mother(
        reinterpret_cast<std::uintptr_t>(breeder), step, &context));
}

void issue_seed_move(void* seed, void* breeder) noexcept {
    if (!seed || !breeder || object_expired(seed) || object_expired(breeder)) {
        return;
    }
    const Vector3 destination = object_position(breeder);
    a2fo_breeders_call_thiscall_command_vector_int(
        at(g_armada, kDequeueCommandVectorRva), seed,
        kMoveSingleCommand, &destination, 0);
}

bool begin_seed_request(void* breeder, void* target_class,
                        const ClassPolicy& target_policy) noexcept {
    // breederProduces must match the selected target.  Compare against the
    // selected class by pointer where possible, avoiding dependence on native
    // ODF-name string layout.
    struct Context {
        void* breeder;
        const ClassPolicy* target_policy;
        const ClassPolicy* result_policy = nullptr;
        void* mother = nullptr;
        void* seed_class = nullptr;
        void* result_class = nullptr;
        void* seed = nullptr;
        std::uint32_t breeder_handle = 0;
        std::uint32_t seed_handle = 0;
        Matrix34 mother_transform{};
        bool fatal = false;
    } context{breeder, &target_policy};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.fatal) return 0;
        try {
            switch (operation) {
            case 0: return !state.target_policy->seed_odf.empty() &&
                !state.target_policy->result_odf.empty();
            case 1:
                state.breeder_handle = object_handle(state.breeder);
                return state.breeder_handle;
            case 2: return g_waiting.find(static_cast<std::uint32_t>(argument)) !=
                g_waiting.end();
            case 3:
                state.mother = nearest_mother(state.breeder);
                return reinterpret_cast<std::uintptr_t>(state.mother);
            case 4:
                log_line("Breeder genetic choice rejected: no same-team mother in range");
                return 0;
            case 5:
                state.seed_class = find_class(state.target_policy->seed_odf);
                return reinterpret_cast<std::uintptr_t>(state.seed_class);
            case 6:
                state.result_class = find_class(state.target_policy->result_odf);
                return reinterpret_cast<std::uintptr_t>(state.result_class);
            case 7:
                state.result_policy = policy_for_class(
                    reinterpret_cast<void*>(argument));
                return state.result_policy != nullptr;
            case 8: return reinterpret_cast<std::uintptr_t>(
                state.result_policy && !state.result_policy->produces_odf.empty()
                    ? find_class(state.result_policy->produces_odf) : nullptr);
            case 9: return state.result_policy->breeder;
            case 10:
                log_line("Breeder genetic choice rejected: seed/result/produces ODF chain is invalid");
                return 0;
            case 11: return object_transform(state.mother, &state.mother_transform);
            case 12:
                log_line("Breeder genetic choice rejected: mother transform unavailable");
                return 0;
            case 13: {
                char label[64]{};
                std::snprintf(label, sizeof(label), "A2FOBR:%08lX",
                    static_cast<unsigned long>(state.breeder_handle));
                state.seed = construct_object(state.seed_class,
                    state.mother_transform, object_team(state.breeder), label);
                return reinterpret_cast<std::uintptr_t>(state.seed);
            }
            case 14:
                log_line("Breeder genetic choice rejected: seed construction failed");
                return 0;
            case 15:
                state.seed_handle = object_handle(state.seed);
                return state.seed_handle;
            case 16: expire_craft(reinterpret_cast<void*>(argument)); return 0;
            case 17:
                log_line("Breeder genetic choice rejected: seed has no live handle");
                return 0;
            case 18: {
                WaitingSeed waiting;
                waiting.breeder_handle = state.breeder_handle;
                waiting.seed_handle = state.seed_handle;
                waiting.result_odf = odf_key(state.target_policy->result_odf);
                waiting.target_odf = odf_key(state.result_policy->produces_odf);
                g_waiting[state.breeder_handle] = std::move(waiting);
                return 0;
            }
            case 19: issue_seed_move(state.seed, state.breeder); return 0;
            case 20: {
                char message[256]{};
                std::snprintf(message, sizeof(message),
                    "Breeder %lu requested seed %lu from mother %lu for '%s'",
                    static_cast<unsigned long>(state.breeder_handle),
                    static_cast<unsigned long>(state.seed_handle),
                    static_cast<unsigned long>(object_handle(state.mother)),
                    g_waiting[state.breeder_handle].target_odf.c_str());
                log_line(message);
                return 0;
            }
            default: return 0;
            }
        } catch (...) {
            state.fatal = true;
            return 0;
        }
    };
    const bool result = a2fo_rs_stage8_breeder_begin_request(
        reinterpret_cast<std::uintptr_t>(breeder),
        reinterpret_cast<std::uintptr_t>(target_class), step, &context) != 0;
    if (context.fatal) std::terminate();
    return result;
}

bool metamorphose_breeder(void* breeder, const WaitingSeed& waiting) noexcept {
    // This runs from SIMULATE_POST.  Match A2FOODFVariants: expire the old
    // object and let Armada perform its normal deferred cleanup after the
    // callback instead of deleting the currently-callbacking Craft in-place.
    struct Context {
        void* breeder;
        const WaitingSeed* waiting;
        void* seed = nullptr;
        void* result_class = nullptr;
        const ClassPolicy* result_policy = nullptr;
        Matrix34 transform{};
    } context{breeder, &waiting};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        void* object = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0:
            state.seed = find_entity(state.waiting->seed_handle);
            return reinterpret_cast<std::uintptr_t>(state.seed);
        case 1: return object_expired(object);
        case 2: return object_expired(object);
        case 3:
            state.result_class = find_class(state.waiting->result_odf);
            return reinterpret_cast<std::uintptr_t>(state.result_class);
        case 4: return reinterpret_cast<std::uintptr_t>(
            find_class(state.waiting->target_odf));
        case 5:
            state.result_policy = policy_for_class(object);
            return state.result_policy != nullptr;
        case 6: return state.result_policy->breeder;
        case 7: return reinterpret_cast<std::uintptr_t>(
            find_class(state.result_policy->produces_odf));
        case 8:
            log_line("Breeder metamorphosis rejected: committed breeder ODF is invalid");
            return 0;
        case 9: return object_transform(object, &state.transform);
        case 10: {
            char label[64]{};
            std::snprintf(label, sizeof(label), "A2FOBRM:%08lX",
                static_cast<unsigned long>(state.waiting->breeder_handle));
            return reinterpret_cast<std::uintptr_t>(construct_object(
                state.result_class, state.transform,
                object_team(state.breeder), label));
        }
        case 11:
            log_line("Breeder metamorphosis failed: committed breeder construction failed");
            return 0;
        case 12:
            a2fo_breeders_call_thiscall_1(
                at(g_armada, kEvolverSwapObjectsRva), state.breeder, argument);
            return 0;
        case 13: expire_craft(object); return 0;
        case 14: expire_craft(object); return 0;
        case 15: {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "Breeder committed to '%s'; seed %lu consumed",
                state.waiting->target_odf.c_str(),
                static_cast<unsigned long>(state.waiting->seed_handle));
            log_line(message);
            return 0;
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_breeder_metamorphose(
        reinterpret_cast<std::uintptr_t>(breeder), step, &context) != 0;
}

void update_waiting_breeder(void* breeder, float elapsed_seconds) noexcept {
    struct Context {
        void* breeder;
        float elapsed;
        decltype(g_waiting)::iterator waiting;
        void* seed = nullptr;
        float consume_radius = kDefaultConsumeRadius;
        std::optional<WaitingSeed> committed;
        bool fatal = false;
    } context{};
    context.breeder = breeder;
    context.elapsed = elapsed_seconds;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.fatal) return UINTPTR_MAX;
        try {
            switch (operation) {
            case 0: return object_handle(state.breeder);
            case 1:
                state.waiting = g_waiting.find(static_cast<std::uint32_t>(argument));
                return state.waiting != g_waiting.end();
            case 2:
                state.seed = find_entity(state.waiting->second.seed_handle);
                return reinterpret_cast<std::uintptr_t>(state.seed);
            case 3: return object_expired(reinterpret_cast<void*>(argument));
            case 4:
                log_line("Breeder seed was lost; genetic choice unlocked again");
                return 0;
            case 5: g_waiting.erase(state.waiting); return 0;
            case 6: {
                const ClassPolicy* policy = policy_for_class(
                    object_class(state.breeder));
                state.consume_radius = policy
                    ? policy->consume_radius : kDefaultConsumeRadius;
                return 0;
            }
            case 7: {
                return distance_squared(
                    object_position(state.seed), object_position(state.breeder)) <=
                    state.consume_radius * state.consume_radius;
            }
            case 8:
                // The swap reenters CLEANUP. Copy and erase before it runs.
                state.committed.emplace(state.waiting->second);
                return 0;
            case 9: return metamorphose_breeder(state.breeder, *state.committed);
            case 10: return object_expired(state.breeder);
            case 11:
                g_waiting.emplace(static_cast<std::uint32_t>(argument),
                                  *state.committed);
                return 0;
            case 12:
                state.waiting->second.move_refresh +=
                    std::max(0.0f, state.elapsed);
                return state.waiting->second.move_refresh >= kMoveRefreshSeconds;
            case 13: state.waiting->second.move_refresh = 0.0f; return 0;
            case 14: issue_seed_move(state.seed, state.breeder); return 0;
            default: return 0;
            }
        } catch (...) {
            state.fatal = true;
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_breeder_update_waiting(
        reinterpret_cast<std::uintptr_t>(breeder), step, &context);
    if (context.fatal) std::terminate();
}

void update_active_breeder(void* breeder, const ClassPolicy& policy,
                           float elapsed_seconds) noexcept {
    struct Context {
        void* breeder;
        const ClassPolicy* policy;
        float elapsed;
        float* retry = nullptr;
        void* target = nullptr;
        bool fatal = false;
    } context{breeder, &policy, elapsed_seconds};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.fatal) return UINTPTR_MAX;
        try {
            switch (operation) {
            case 0: return !state.policy->produces_odf.empty();
            case 1: return state.breeder != nullptr;
            case 2: return read_at<void*>(state.breeder,
                kProducerCurrentBuildClassOffset, nullptr) == nullptr;
            case 3: return read_at<std::uint32_t>(state.breeder,
                kProducerQueueCountOffset, 0) == 0;
            case 4: return object_handle(state.breeder);
            case 5:
                state.retry = &g_production_retry[
                    static_cast<std::uint32_t>(argument)];
                *state.retry -= std::max(0.0f, state.elapsed);
                return *state.retry > 0.0f;
            case 6: *state.retry = kProductionRetrySeconds; return 0;
            case 7:
                state.target = find_class(state.policy->produces_odf);
                return reinterpret_cast<std::uintptr_t>(state.target);
            case 8: return g_fleet_ops != nullptr;
            case 9: return read_at<std::uint32_t>(state.breeder,
                kProducerQueueCountOffset, 0);
            case 10:
                a2fo_breeders_call_thiscall_1(
                    at(g_fleet_ops, kFoProducerPushCheckedRva), state.breeder,
                    reinterpret_cast<std::uintptr_t>(state.target));
                return 0;
            case 11: return read_at<std::uint32_t>(state.breeder,
                kProducerQueueCountOffset, 0);
            case 12: return reinterpret_cast<std::uintptr_t>(read_at<void*>(
                state.breeder, kProducerCurrentBuildClassOffset, nullptr));
            case 13: *state.retry = 0.0f; return 0;
            default: return 0;
            }
        } catch (...) {
            state.fatal = true;
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_breeder_update_active(
        reinterpret_cast<std::uintptr_t>(breeder), step, &context);
    if (context.fatal) std::terminate();
}

bool A2FO_CALL producer_event_handler(
    const A2FO_ProducerEvent* event, void*) {
    // Committed breeders have no public build list.  If a mod/UI nevertheless
    // reaches native admission, reject it; automatic production enters through
    // Producer::PushChecked directly and never traverses this event.
    // Always claim the breeder build choice.  A successful request creates the
    // seed; a failed request leaves the breeder virgin and consumes no native
    // build order or resource cost.
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto* event = reinterpret_cast<const A2FO_ProducerEvent*>(argument);
        switch (operation) {
        case 0: return runtime_enabled();
        case 1: return event->struct_size;
        case 2: return reinterpret_cast<std::uintptr_t>(event->producer);
        case 3: return g_runtime_ready;
        case 4: return event->kind;
        case 5: return reinterpret_cast<std::uintptr_t>(event->target_class);
        case 6: {
            const ClassPolicy* policy = policy_for_class(
                object_class(event->producer));
            if (!policy || !policy->breeder) return 0;
            return policy->produces_odf.empty() ? 1 : 2;
        }
        case 7: return object_handle(event->producer);
        case 8: return g_waiting.find(static_cast<std::uint32_t>(argument)) !=
            g_waiting.end();
        case 9: {
            const ClassPolicy* policy = policy_for_class(event->target_class);
            return policy && !policy->seed_odf.empty() &&
                !policy->result_odf.empty();
        }
        case 10:
            log_line("Breeder genetic choice rejected: target lacks breederSeed/breederResult");
            return 0;
        case 11: {
            const ClassPolicy* policy = policy_for_class(event->target_class);
            begin_seed_request(event->producer, event->target_class, *policy);
            return 0;
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_breeder_producer_event(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event),
        A2FO_PRODUCER_EVENT_ADMIT, step, nullptr) != 0;
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    struct Context {
        decltype(g_waiting)::iterator waiting;
        const ClassPolicy* policy = nullptr;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        const auto* event = reinterpret_cast<const A2FO_CraftEvent*>(argument);
        switch (operation) {
        case 0: return runtime_enabled();
        case 1: return event->struct_size;
        case 2: return reinterpret_cast<std::uintptr_t>(event->craft);
        case 3: return g_runtime_ready;
        case 4: return object_handle(event->craft);
        case 5: return event->kind;
        case 6: g_waiting.erase(static_cast<std::uint32_t>(argument)); return 0;
        case 7: g_production_retry.erase(static_cast<std::uint32_t>(argument)); return 0;
        case 8:
            state.waiting = g_waiting.find(static_cast<std::uint32_t>(argument));
            return state.waiting != g_waiting.end();
        case 9: return reinterpret_cast<std::uintptr_t>(
            find_entity(state.waiting->second.seed_handle));
        case 10: expire_craft(reinterpret_cast<void*>(argument)); return 0;
        case 11: g_waiting.erase(state.waiting); return 0;
        case 12: return object_expired(event->craft);
        case 13: {
            state.policy = policy_for_class(object_class(event->craft));
            if (!state.policy || !state.policy->breeder) return 0;
            return state.policy->produces_odf.empty() ? 1 : 2;
        }
        case 14: update_waiting_breeder(event->craft, event->elapsed_seconds); return 0;
        case 15: {
            update_active_breeder(event->craft, *state.policy, event->elapsed_seconds);
            return 0;
        }
        default: return 0;
        }
    };
#ifdef A2FO_BUILTIN_FEATURE
    constexpr std::uint8_t builtin = 1;
#else
    constexpr std::uint8_t builtin = 0;
#endif
    a2fo_rs_stage8_breeder_craft_event(
        reinterpret_cast<std::uintptr_t>(event), sizeof(*event),
        A2FO_CRAFT_EVENT_CLEANUP, A2FO_CRAFT_EVENT_POST_LOAD,
        A2FO_CRAFT_EVENT_SIMULATE_POST, builtin, step, &context);
}

void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->object_class) {
        return;
    }
    struct Context {
        const A2FO_GameObjectClassLoadedEvent* event;
        bool failed = false;
    } context{event};
    const auto lower = [](void*, std::uint8_t byte) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(
            std::tolower(static_cast<unsigned char>(byte)));
    };
    const auto field = [](void* opaque, std::uint32_t index,
                          A2foStage8BreederBytes* name,
                          A2foStage8BreederBytes* value) noexcept -> std::uint8_t {
        const auto& state = *static_cast<Context*>(opaque);
        if (state.failed || !state.event->odf_fields ||
            index >= state.event->odf_field_count) return 0;
        const auto& source = state.event->odf_fields[index];
        if (!source.name.data || (!source.value.data && source.value.size != 0)) {
            return 0;
        }
        *name = {reinterpret_cast<const std::uint8_t*>(source.name.data),
                 source.name.size};
        *value = {reinterpret_cast<const std::uint8_t*>(source.value.data),
                  source.value.size};
        return 1;
    };
    const auto parse_float = [](void* opaque, A2foStage8BreederBytes bytes,
                                float* output) noexcept -> std::uint8_t {
        try {
            const std::string value(
                reinterpret_cast<const char*>(bytes.data), bytes.size);
            char* end = nullptr;
            *output = std::strtof(value.c_str(), &end);
            return end && end != value.c_str() && *end == '\0';
        } catch (...) {
            static_cast<Context*>(opaque)->failed = true;
            return 2;
        }
    };
    const auto store = [](void* opaque, std::uintptr_t object_class,
                          const A2foStage8BreederPolicy* parsed) noexcept -> std::uint8_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            auto* object = reinterpret_cast<void*>(object_class);
            if (!parsed) {
                g_class_policies.erase(object);
                return 1;
            }
            const auto copy = [](A2foStage8BreederBytes bytes) {
                return std::string(reinterpret_cast<const char*>(bytes.data), bytes.size);
            };
            ClassPolicy policy;
            policy.breeder = parsed->breeder != 0;
            policy.mother = parsed->mother != 0;
            policy.mother_radius = parsed->mother_radius;
            policy.consume_radius = parsed->consume_radius;
            policy.seed_odf = copy(parsed->seed);
            policy.result_odf = copy(parsed->result);
            policy.produces_odf = copy(parsed->produces);
            g_class_policies[object] = std::move(policy);
            return 1;
        } catch (...) {
            state.failed = true;
            return 0;
        }
    };
    a2fo_rs_stage8_breeder_class_loaded(
        reinterpret_cast<std::uintptr_t>(event->object_class),
        event->odf_fields ? event->odf_field_count : 0,
        lower, field, parse_float, store, &context);
    if (context.failed) {
        log_line("Could not retain breeder ODF metadata");
    }
}

bool preflight_signatures() noexcept {
    return g_armada && g_fleet_ops &&
        signature_matches(g_armada, kAiMissionGetCurrentRva,
                          kExpectedAiMissionGetCurrent) &&
        signature_matches(g_armada, kGameObjectClassFindRva,
                          kExpectedGameObjectClassFind) &&
        signature_matches(g_armada, kGameObjectClassConstructRva,
                          kExpectedGameObjectClassConstruct) &&
        signature_matches(g_armada, kEntityGetTransformRva,
                          kExpectedEntityGetTransform) &&
        signature_matches(g_armada, kEntityGetRva, kExpectedEntityGet) &&
        signature_matches(g_armada, kDequeueCommandVectorRva,
                          kExpectedDequeueCommandVector) &&
        signature_matches(g_armada, kEvolverSwapObjectsRva,
                          kExpectedEvolverSwapObjects) &&
        signature_matches(g_fleet_ops, kFoProducerPushCheckedRva,
                          kExpectedFoPushChecked);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
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
                    api->armada_module && api->fleetops_module &&
                    api->register_classlabel_alias &&
                    A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) &&
                    api->register_classlabel_odf_defaults &&
                    A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) &&
                    api->register_game_object_class_loaded_handler &&
                    A2FO_MODULE_API_HAS(api, register_producer_event_handler) &&
                    api->register_producer_event_handler &&
                    A2FO_MODULE_API_HAS(api, register_craft_event_handler) &&
                    api->register_craft_event_handler &&
                    (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) != 0 &&
                    (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) != 0 &&
                    (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) != 0 &&
                    (api->capabilities & A2FO_CAP_CRAFT_EVENTS) != 0;
            case 1:
                g_api = api;
                g_armada = static_cast<HMODULE>(api->armada_module());
                g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
                return g_armada && g_fleet_ops && preflight_signatures();
            case 2:
                log_line("Supported breeder runtime signatures are unavailable");
                return 0;
            case 3: return api->register_classlabel_alias(
                kModuleName, "breeder", "shipyard");
            case 4: return api->register_classlabel_odf_defaults(
                kModuleName, "breeder", kBreederDefaults.data(),
                static_cast<std::uint32_t>(kBreederDefaults.size()));
            case 5: return api->register_game_object_class_loaded_handler(
                kModuleName, kRequiredFields.data(),
                static_cast<std::uint32_t>(kRequiredFields.size()),
                &class_loaded_handler, nullptr);
            case 6: return api->register_producer_event_handler(
                kModuleName, &producer_event_handler, nullptr);
            case 7: return A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) &&
                api->register_craft_event_handler_masked;
            case 8: return api->register_craft_event_handler_masked(
                kModuleName,
                A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
#ifdef A2FO_BUILTIN_FEATURE
                    A2FO_CRAFT_EVENT_MASK_POST_LOAD |
#endif
                    A2FO_CRAFT_EVENT_MASK_CLEANUP,
                &craft_event_handler, nullptr);
            case 9: return api->register_craft_event_handler(
                kModuleName, &craft_event_handler, nullptr);
            case 10:
                log_line("Breeder semantic/runtime registration failed");
                return 0;
            case 11:
                g_runtime_ready = true;
                log_line("Breeder prototype initialized: genetic choices, mother seeds, metamorphosis and automatic production enabled");
                return 0;
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool result = a2fo_rs_stage8_breeder_init(step, &context) != 0;
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        switch (operation) {
        case 0: g_runtime_ready = false; break;
        case 1: g_waiting.clear(); break;
        case 2: g_production_retry.clear(); break;
        case 3: g_class_policies.clear(); break;
        case 4: g_armada = nullptr; break;
        case 5: g_fleet_ops = nullptr; break;
        case 6: g_api = nullptr; break;
        default: break;
        }
        return 0;
    };
    a2fo_rs_stage8_breeder_shutdown(step, nullptr);
}
