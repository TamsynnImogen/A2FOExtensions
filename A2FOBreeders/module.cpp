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
 *   - a successful production admission feeds the breeder. If it remains idle
 *     because the next organism cannot be admitted, breederStarvationTime is a
 *     grace period before the breeder expires.
 *
 * Phase-one limitations are documented in README.md.
 */

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
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
constexpr float kDefaultStarvationSeconds = 180.0f;
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

constexpr std::array<const char*, 8> kRequiredFields{{
    "breederClass",
    "breederMother",
    "breederRadius",
    "breederSeed",
    "breederResult",
    "breederProduces",
    "breederConsumeRadius",
    "breederStarvationTime",
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
    float starvation_time = kDefaultStarvationSeconds;
    std::string seed_odf;
    std::string result_odf;
    std::string produces_odf;
};

struct ActiveBreederState {
    float retry = 0.0f;
    float starvation = 0.0f;
    bool starving = false;
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
std::unordered_map<std::uint32_t, ActiveBreederState> g_active_breeders;

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

std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string trim_unquote(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string odf_key(std::string value) {
    value = lower_ascii(trim_unquote(std::move(value)));
    std::replace(value.begin(), value.end(), '/', '\\');
    const std::size_t slash = value.find_last_of('\\');
    if (slash != std::string::npos) value.erase(0, slash + 1);
    if (value.size() >= 4 && value.compare(value.size() - 4, 4, ".odf") == 0) {
        value.erase(value.size() - 4);
    }
    return value;
}

bool parse_bool(std::string value, bool fallback = false) noexcept {
    value = lower_ascii(trim_unquote(std::move(value)));
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return fallback;
}

float parse_positive_float(std::string value, float fallback) noexcept {
    value = trim_unquote(std::move(value));
    if (value.empty()) return fallback;
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (!end || end == value.c_str() || *end != '\0' ||
        !std::isfinite(parsed) || parsed <= 0.0f) {
        return fallback;
    }
    return parsed;
}

float parse_nonnegative_float(std::string value, float fallback) noexcept {
    value = trim_unquote(std::move(value));
    if (value.empty()) return fallback;
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (!end || end == value.c_str() || *end != '\0' ||
        !std::isfinite(parsed) || parsed < 0.0f) {
        return fallback;
    }
    return parsed;
}

std::string field_value(const A2FO_GameObjectClassLoadedEvent* event,
                        const char* name) {
    if (!event || !name || !event->odf_fields) return {};
    for (std::uint32_t index = 0; index < event->odf_field_count; ++index) {
        const auto& field = event->odf_fields[index];
        if (!field.name.data || (!field.value.data && field.value.size != 0)) {
            continue;
        }
        if (field.name.size != std::strlen(name)) continue;
        bool equal = true;
        for (std::size_t i = 0; i < field.name.size; ++i) {
            if (std::tolower(static_cast<unsigned char>(field.name.data[i])) !=
                std::tolower(static_cast<unsigned char>(name[i]))) {
                equal = false;
                break;
            }
        }
        if (!equal) continue;
        return std::string(field.value.data ? field.value.data : "",
                           field.value.size);
    }
    return {};
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

template <typename Visitor>
void for_each_live_object(Visitor&& visitor) noexcept {
    if (!g_armada) return;
    void* list = read_at<void*>(at(g_armada, kGameObjectListPointerRva),
                                0, nullptr);
    if (!readable_range(list, 12)) return;
    void* head = read_at<void*>(list, 4, nullptr);
    const std::uint32_t count = read_at<std::uint32_t>(list, 8, 0);
    if (!readable_range(head, 12) || count > kMaximumObjects) return;
    void* node = read_at<void*>(head, 0, nullptr);
    for (std::uint32_t index = 0;
         node && node != head && index < count; ++index) {
        void* next = read_at<void*>(node, 0, nullptr);
        void* object = read_at<void*>(node, 8, nullptr);
        if (object && !object_expired(object)) visitor(object);
        if (next == node) break;
        node = next;
    }
}

void* nearest_mother(void* breeder) noexcept {
    if (!breeder || object_expired(breeder)) return nullptr;
    const std::int32_t team = object_team(breeder);
    const Vector3 breeder_position = object_position(breeder);
    void* best = nullptr;
    float best_distance = 0.0f;
    std::uint32_t best_handle = 0;
    for_each_live_object([&](void* candidate) noexcept {
        if (candidate == breeder || object_team(candidate) != team) return;
        const ClassPolicy* policy = policy_for_class(object_class(candidate));
        if (!policy || !policy->mother || policy->mother_radius <= 0.0f) return;
        const float distance = distance_squared(
            breeder_position, object_position(candidate));
        const float maximum = policy->mother_radius * policy->mother_radius;
        if (distance > maximum) return;
        const std::uint32_t handle = object_handle(candidate);
        if (!best || distance < best_distance ||
            (distance == best_distance && handle < best_handle)) {
            best = candidate;
            best_distance = distance;
            best_handle = handle;
        }
    });
    return best;
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
    if (!breeder || !target_class || target_policy.seed_odf.empty() ||
        target_policy.result_odf.empty()) {
        return false;
    }
    const std::uint32_t breeder_handle = object_handle(breeder);
    if (breeder_handle == 0 || g_waiting.find(breeder_handle) != g_waiting.end()) {
        return false;
    }
    void* mother = nearest_mother(breeder);
    if (!mother) {
        log_line("Breeder genetic choice rejected: no same-team mother in range");
        return false;
    }
    void* seed_class = find_class(target_policy.seed_odf);
    void* result_class = find_class(target_policy.result_odf);

    // breederProduces must match the selected target.  Compare against the
    // selected class by pointer where possible, avoiding dependence on native
    // ODF-name string layout.
    const ClassPolicy* result_policy = policy_for_class(result_class);
    void* produces_class = result_policy && !result_policy->produces_odf.empty()
        ? find_class(result_policy->produces_odf) : nullptr;
    if (!seed_class || !result_class || !result_policy ||
        !result_policy->breeder || !produces_class ||
        produces_class != target_class) {
        log_line("Breeder genetic choice rejected: seed/result/produces ODF chain is invalid");
        return false;
    }

    Matrix34 mother_transform{};
    if (!object_transform(mother, &mother_transform)) {
        log_line("Breeder genetic choice rejected: mother transform unavailable");
        return false;
    }
    char label[64]{};
    std::snprintf(label, sizeof(label), "A2FOBR:%08lX",
                  static_cast<unsigned long>(breeder_handle));
    void* seed = construct_object(
        seed_class, mother_transform, object_team(breeder), label);
    if (!seed) {
        log_line("Breeder genetic choice rejected: seed construction failed");
        return false;
    }
    const std::uint32_t seed_handle = object_handle(seed);
    if (seed_handle == 0) {
        expire_craft(seed);
        log_line("Breeder genetic choice rejected: seed has no live handle");
        return false;
    }

    WaitingSeed waiting;
    waiting.breeder_handle = breeder_handle;
    waiting.seed_handle = seed_handle;
    waiting.result_odf = odf_key(target_policy.result_odf);
    waiting.target_odf = odf_key(result_policy->produces_odf);
    g_waiting[breeder_handle] = std::move(waiting);
    issue_seed_move(seed, breeder);

    char message[256]{};
    std::snprintf(
        message, sizeof(message),
        "Breeder %lu requested seed %lu from mother %lu for '%s'",
        static_cast<unsigned long>(breeder_handle),
        static_cast<unsigned long>(seed_handle),
        static_cast<unsigned long>(object_handle(mother)),
        g_waiting[breeder_handle].target_odf.c_str());
    log_line(message);
    return true;
}

bool metamorphose_breeder(void* breeder, const WaitingSeed& waiting) noexcept {
    void* seed = find_entity(waiting.seed_handle);
    if (!breeder || !seed || object_expired(breeder) || object_expired(seed)) {
        return false;
    }
    void* result_class = find_class(waiting.result_odf);
    void* target_class = find_class(waiting.target_odf);
    const ClassPolicy* result_policy = policy_for_class(result_class);
    if (!result_class || !target_class || !result_policy ||
        !result_policy->breeder ||
        find_class(result_policy->produces_odf) != target_class) {
        log_line("Breeder metamorphosis rejected: committed breeder ODF is invalid");
        return false;
    }
    Matrix34 breeder_transform{};
    if (!object_transform(breeder, &breeder_transform)) return false;

    char label[64]{};
    std::snprintf(label, sizeof(label), "A2FOBRM:%08lX",
                  static_cast<unsigned long>(waiting.breeder_handle));
    void* replacement = construct_object(
        result_class, breeder_transform, object_team(breeder), label);
    if (!replacement) {
        log_line("Breeder metamorphosis failed: committed breeder construction failed");
        return false;
    }

    a2fo_breeders_call_thiscall_1(
        at(g_armada, kEvolverSwapObjectsRva), breeder,
        reinterpret_cast<std::uintptr_t>(replacement));
    // This runs from SIMULATE_POST.  Match A2FOODFVariants: expire the old
    // object and let Armada perform its normal deferred cleanup after the
    // callback instead of deleting the currently-callbacking Craft in-place.
    expire_craft(breeder);
    expire_craft(seed);

    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "Breeder committed to '%s'; seed %lu consumed",
                  waiting.target_odf.c_str(),
                  static_cast<unsigned long>(waiting.seed_handle));
    log_line(message);
    return true;
}

void update_waiting_breeder(void* breeder, float elapsed_seconds) noexcept {
    const std::uint32_t handle = object_handle(breeder);
    auto found = g_waiting.find(handle);
    if (found == g_waiting.end()) return;
    WaitingSeed& waiting = found->second;
    void* seed = find_entity(waiting.seed_handle);
    if (!seed || object_expired(seed)) {
        log_line("Breeder seed was lost; genetic choice unlocked again");
        g_waiting.erase(found);
        return;
    }
    const ClassPolicy* breeder_policy = policy_for_class(object_class(breeder));
    const float consume_radius = breeder_policy
        ? breeder_policy->consume_radius : kDefaultConsumeRadius;
    if (distance_squared(object_position(seed), object_position(breeder)) <=
        consume_radius * consume_radius) {
        // The native identity swap destroys the virgin Breeder and therefore
        // re-enters our CLEANUP callback.  Remove the transient entry first so
        // that nested cleanup cannot invalidate a reference held here.
        const WaitingSeed committed = waiting;
        g_waiting.erase(found);
        if (!metamorphose_breeder(breeder, committed) &&
            !object_expired(breeder)) {
            g_waiting.emplace(handle, committed);
        }
        return;
    }
    waiting.move_refresh += std::max(0.0f, elapsed_seconds);
    if (waiting.move_refresh >= kMoveRefreshSeconds) {
        waiting.move_refresh = 0.0f;
        issue_seed_move(seed, breeder);
    }
}

bool producer_idle(void* producer) noexcept {
    return producer &&
        read_at<void*>(producer, kProducerCurrentBuildClassOffset, nullptr) == nullptr &&
        read_at<std::uint32_t>(producer, kProducerQueueCountOffset, 0) == 0;
}

bool push_production_once(void* breeder, void* target_class) noexcept {
    if (!breeder || !target_class || !g_fleet_ops) return false;
    const std::uint32_t before = read_at<std::uint32_t>(
        breeder, kProducerQueueCountOffset, 0);
    a2fo_breeders_call_thiscall_1(
        at(g_fleet_ops, kFoProducerPushCheckedRva), breeder,
        reinterpret_cast<std::uintptr_t>(target_class));
    const std::uint32_t after = read_at<std::uint32_t>(
        breeder, kProducerQueueCountOffset, 0);
    const void* current = read_at<void*>(
        breeder, kProducerCurrentBuildClassOffset, nullptr);
    return after > before || current == target_class;
}

void update_active_breeder(void* breeder, const ClassPolicy& policy,
                           float elapsed_seconds) noexcept {
    if (policy.produces_odf.empty()) return;
    const std::uint32_t handle = object_handle(breeder);
    if (handle == 0) return;
    ActiveBreederState& state = g_active_breeders[handle];
    const float elapsed = std::max(0.0f, elapsed_seconds);

    // A breeder with a live job has already paid the organism's normal
    // resource cost, so that production cycle counts as being fed.
    if (!producer_idle(breeder)) {
        state.retry = 0.0f;
        state.starvation = 0.0f;
        state.starving = false;
        return;
    }

    // Once an idle breeder has failed to admit its next organism, it is no
    // longer receiving resources.  It may remain dormant for the configured
    // grace period; successful admission below immediately feeds it again.
    if (state.starving && policy.starvation_time > 0.0f) {
        state.starvation += elapsed;
        if (state.starvation >= policy.starvation_time) {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                          "Breeder %lu starved after %.1f seconds without a production feed",
                          static_cast<unsigned long>(handle),
                          static_cast<double>(state.starvation));
            log_line(message);
            g_active_breeders.erase(handle);
            expire_craft(breeder);
            return;
        }
    }

    state.retry -= elapsed;
    if (state.retry > 0.0f) return;
    state.retry = kProductionRetrySeconds;

    void* target_class = find_class(policy.produces_odf);
    if (!target_class) return;
    if (push_production_once(breeder, target_class)) {
        if (state.starving) {
            char message[160]{};
            std::snprintf(message, sizeof(message),
                          "Breeder %lu fed; automatic production resumed",
                          static_cast<unsigned long>(handle));
            log_line(message);
        }
        state.retry = 0.0f;
        state.starvation = 0.0f;
        state.starving = false;
        return;
    }

    if (!state.starving && policy.starvation_time > 0.0f) {
        state.starving = true;
        state.starvation = 0.0f;
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "Breeder %lu cannot start its next organism; starvation grace period %.1f seconds started",
                      static_cast<unsigned long>(handle),
                      static_cast<double>(policy.starvation_time));
        log_line(message);
    }
}

bool A2FO_CALL producer_event_handler(
    const A2FO_ProducerEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->producer || !g_runtime_ready) {
        return true;
    }
    if (event->kind != A2FO_PRODUCER_EVENT_ADMIT || !event->target_class) {
        return true;
    }
    const ClassPolicy* producer_policy = policy_for_class(
        object_class(event->producer));
    if (!producer_policy || !producer_policy->breeder) return true;

    // Committed breeders have no public build list.  If a mod/UI nevertheless
    // reaches native admission, reject it; automatic production enters through
    // Producer::PushChecked directly and never traverses this event.
    if (!producer_policy->produces_odf.empty()) return false;

    const std::uint32_t handle = object_handle(event->producer);
    if (handle == 0 || g_waiting.find(handle) != g_waiting.end()) return false;
    const ClassPolicy* target_policy = policy_for_class(event->target_class);
    if (!target_policy || target_policy->seed_odf.empty() ||
        target_policy->result_odf.empty()) {
        log_line("Breeder genetic choice rejected: target lacks breederSeed/breederResult");
        return false;
    }

    // Always claim the breeder build choice.  A successful request creates the
    // seed; a failed request leaves the breeder virgin and consumes no native
    // build order or resource cost.
    begin_seed_request(event->producer, event->target_class, *target_policy);
    return false;
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->craft || !g_runtime_ready) {
        return;
    }
    void* craft = event->craft;
    const std::uint32_t handle = object_handle(craft);
    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP) {
        auto waiting = g_waiting.find(handle);
        if (waiting != g_waiting.end()) {
            if (void* seed = find_entity(waiting->second.seed_handle)) {
                expire_craft(seed);
            }
            g_waiting.erase(waiting);
        }
        g_active_breeders.erase(handle);
        return;
    }
    if (event->kind != A2FO_CRAFT_EVENT_SIMULATE_POST ||
        object_expired(craft)) {
        return;
    }
    const ClassPolicy* policy = policy_for_class(object_class(craft));
    if (!policy || !policy->breeder) return;
    if (policy->produces_odf.empty()) {
        update_waiting_breeder(craft, event->elapsed_seconds);
    } else {
        update_active_breeder(craft, *policy, event->elapsed_seconds);
    }
}

void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->object_class) {
        return;
    }
    try {
        ClassPolicy policy;
        policy.breeder = parse_bool(field_value(event, "breederClass"));
        policy.mother = parse_bool(field_value(event, "breederMother"));
        policy.mother_radius = parse_positive_float(
            field_value(event, "breederRadius"), kDefaultMotherRadius);
        policy.consume_radius = parse_positive_float(
            field_value(event, "breederConsumeRadius"),
            kDefaultConsumeRadius);
        policy.starvation_time = parse_nonnegative_float(
            field_value(event, "breederStarvationTime"),
            kDefaultStarvationSeconds);
        policy.seed_odf = odf_key(field_value(event, "breederSeed"));
        policy.result_odf = odf_key(field_value(event, "breederResult"));
        policy.produces_odf = odf_key(field_value(event, "breederProduces"));
        if (!policy.breeder && !policy.mother && policy.seed_odf.empty() &&
            policy.result_odf.empty() && policy.produces_odf.empty()) {
            g_class_policies.erase(event->object_class);
            return;
        }
        g_class_policies[event->object_class] = std::move(policy);
    } catch (...) {
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
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->register_classlabel_alias ||
        !A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) ||
        !api->register_classlabel_odf_defaults ||
        !A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) ||
        !api->register_game_object_class_loaded_handler ||
        !A2FO_MODULE_API_HAS(api, register_producer_event_handler) ||
        !api->register_producer_event_handler ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler) ||
        !api->register_craft_event_handler ||
        (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) == 0 ||
        (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) == 0 ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0) {
        return false;
    }

    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops || !preflight_signatures()) {
        log_line("Supported breeder runtime signatures are unavailable");
        return false;
    }

    const bool alias_registered = api->register_classlabel_alias(
        kModuleName, "breeder", "shipyard");
    const bool defaults_registered = api->register_classlabel_odf_defaults(
        kModuleName, "breeder", kBreederDefaults.data(),
        static_cast<std::uint32_t>(kBreederDefaults.size()));
    const bool classes_registered = api->register_game_object_class_loaded_handler(
        kModuleName, kRequiredFields.data(),
        static_cast<std::uint32_t>(kRequiredFields.size()),
        &class_loaded_handler, nullptr);
    const bool producer_registered = api->register_producer_event_handler(
        kModuleName, &producer_event_handler, nullptr);

    bool craft_registered = false;
    if (A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) &&
        api->register_craft_event_handler_masked) {
        craft_registered = api->register_craft_event_handler_masked(
            kModuleName,
            A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                A2FO_CRAFT_EVENT_MASK_CLEANUP,
            &craft_event_handler, nullptr);
    } else {
        craft_registered = api->register_craft_event_handler(
            kModuleName, &craft_event_handler, nullptr);
    }

    if (!alias_registered || !defaults_registered || !classes_registered ||
        !producer_registered || !craft_registered) {
        log_line("Breeder semantic/runtime registration failed");
        return false;
    }

    g_runtime_ready = true;
    log_line("Breeder prototype initialized: genetic choices, mother seeds, metamorphosis, automatic production and starvation enabled");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
    g_waiting.clear();
    g_active_breeders.clear();
    g_class_policies.clear();
    g_armada = nullptr;
    g_fleet_ops = nullptr;
    g_api = nullptr;
}
