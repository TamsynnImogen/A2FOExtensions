/*
 * A2FORefitYards: synchronized ship-to-yard refits using the native Producer
 * FIFO, build progress, costs, completion object, and replacement handoff.
 */

#include "docking_transform.hpp"
#include "refit_policy.hpp"

#include "../A2FOFeaturePack/refit_queue_bridge_api.hpp"
#include "../A2FOHybridBuild/refit_ui_bridge_api.hpp"
#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>

extern "C" std::uintptr_t a2fo_refit_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_refit_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_refit_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" void a2fo_refit_call_thiscall_command_vector_int(
    void* function, void* self, std::int32_t command,
    const void* vector, std::int32_t value);

namespace {

constexpr char kModuleName[] = "A2FORefitYards";
constexpr std::size_t kMaximumObjects = 1000000;
constexpr std::size_t kMaximumRefitItems = 16;
constexpr float kArrivalDistance = 160.0f;
constexpr float kMaximumArrivalDistance = 1200.0f;
constexpr float kArrivalClearance = 64.0f;
constexpr float kApproachArrivalDistance = 96.0f;
constexpr float kMaximumApproachArrivalDistance = 320.0f;
constexpr float kApproachArrivalClearance = 32.0f;
constexpr unsigned kRetryIntervalTicks = 20;
constexpr unsigned kMoveRefreshTicks = 60;
constexpr float kDockingTransitionSeconds = 5.0f;
// The native selection dispatcher translates GO (4) to GO_SINGLE (5) before
// queuing an order for each individual Craft. Refit owns one source Craft, so
// it must issue the translated command directly.
constexpr std::int32_t kMoveSingleCommand = 5;

// ArmadaL.exe 1.1 and Fleet Operations Roots RVAs.
constexpr std::uintptr_t kGameObjectClassFindRva = 0x000cd370;
constexpr std::uintptr_t kGameObjectClassGetHierarchyRootRva = 0x000cd940;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetBoundingSphereRva = 0x000cfd70;
constexpr std::uintptr_t kEntityGetWorldTransformRva = 0x000cff90;
constexpr std::uintptr_t kEntityGetRva = 0x000cfff0;
constexpr std::uintptr_t kGameObjectQueueClassCommandRva = 0x000d4280;
// The synchronized command-0x10 receiver resolves the source object and calls
// this local executor.  A refit request is already synchronized by its custom
// class command, so travel must enter here rather than enqueue another network
// packet from inside that receiver.
constexpr std::uintptr_t kDequeueCommandVectorRva = 0x000d4850;
constexpr std::uintptr_t kGameObjectSetTransformRva = 0x000d4ce0;
constexpr std::uintptr_t kEvolverSwapObjectsRva = 0x000b0e10;
constexpr std::uintptr_t kCraftPreventCollisionsRva = 0x000c4940;
// Shipyard constructs an OutputQueueManager at +0x2b4. Its exact native
// vtable lets cancellation use the same virtual handoff as FinishBuild while
// remaining compatible with a supported runtime detour on QueueManager::add.
constexpr std::uintptr_t kOutputQueueManagerVtableRva = 0x002b3714;
constexpr std::uintptr_t kSt3dNodeFindRecursiveRva = 0x00238780;
constexpr std::uintptr_t kGameObjectListPointerRva = 0x00361084;
constexpr std::uintptr_t kDebriefingDataGlobalRva = 0x003a86b4;
constexpr std::uintptr_t kDebriefingDestroyShipRva = 0x001f17b0;
constexpr std::uintptr_t kFoProducerActDeleteRva = 0x00122c8c;

constexpr std::size_t kObjectExpiredOffset = 0x27;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectTeamOffset = 0xec;
// GameObject copies GameObjectClass::avoidMe into this per-instance byte.
// Craft::Prevent_Collisions checks it for both the moving Craft and every
// candidate, which makes it the narrow reversible docked-state switch.
constexpr std::size_t kObjectAvoidMeOffset = 0x10c;
constexpr std::size_t kCurrentBuildClassOffset = 0x254;
constexpr std::size_t kCurrentQueueIdOffset = 0x2a0;
constexpr std::size_t kShipyardBuildOutputQueueOffset = 0x2b4;
constexpr std::size_t kCraftQueueStageOffset = 0x194;
constexpr std::size_t kCraftQueueOwnerOffset = 0x198;

constexpr std::uint8_t kExpectedGameObjectClassFind[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedClassGetHierarchyRoot[] =
    {0x8b, 0x81, 0xd8, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedEntityGetTransform[] =
    {0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
constexpr std::uint8_t kExpectedEntityGetBoundingSphere[] =
    {0x8b, 0x41, 0x04, 0x83, 0xc0, 0x34, 0xc3};
constexpr std::uint8_t kExpectedEntityGetWorldTransform[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c};
constexpr std::uint8_t kExpectedEntityGet[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x08};
constexpr std::uint8_t kExpectedDequeueCommandVector[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x10};
constexpr std::uint8_t kExpectedGameObjectSetTransform[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x56};
constexpr std::uint8_t kExpectedEvolverSwapObjects[] =
    {0x55, 0x8b, 0xec, 0x53, 0x56, 0x57};
constexpr std::uint8_t kExpectedCraftPreventCollisions[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x01, 0x00, 0x00,
};
constexpr std::uint8_t kExpectedCraftAvoidMeGate[] = {
    0x8a, 0x87, 0x0c, 0x01, 0x00, 0x00,
};
constexpr std::uint8_t kExpectedNodeFindRecursive[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57};

constexpr std::array<const char*, 19> kRequiredFields{{
    "refitItem0", "refitItem1", "refitItem2", "refitItem3",
    "refitItem4", "refitItem5", "refitItem6", "refitItem7",
    "refitItem8", "refitItem9", "refitItem10", "refitItem11",
    "refitItem12", "refitItem13", "refitItem14", "refitItem15",
    "refitHardpoint", "buildHardpoint", "classLabel",
}};

using Matrix34 = a2fo::refit::DockingTransform;

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct BoundingSphere {
    Vector3 centre{};
    float radius = 0.0f;
};
static_assert(sizeof(BoundingSphere) == 16,
              "Armada bounding-sphere ABI must occupy sixteen bytes");

struct SourceConfig {
    std::array<std::string, kMaximumRefitItems> items{};
    std::size_t count = 0;
};

struct YardConfig {
    std::string hardpoint;
    void* hardpoint_node = nullptr;
    bool hardpoint_resolved = false;
};

enum class RefitPhase : std::uint8_t {
    travelling,
    waiting_for_queue,
    queued,
    active,
    ejecting,
};

struct RefitJob {
    std::uint32_t source_handle = 0;
    std::uint32_t yard_handle = 0;
    void* target_class = nullptr;
    std::uint32_t queue_id = 0;
    RefitPhase phase = RefitPhase::travelling;
    unsigned retry_ticks = 0;
    unsigned move_ticks = 0;
    float docking_elapsed_seconds = 0.0f;
    float last_distance_squared = -1.0f;
    Matrix34 docking_origin{};
    std::uint8_t saved_avoid_me = 0;
    bool avoidance_suppressed = false;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
A2FO_ProducerPushRefitFn g_push_refit = nullptr;
std::unordered_map<void*, SourceConfig> g_source_classes;
std::unordered_map<void*, YardConfig> g_yard_classes;
std::unordered_map<std::uint32_t, RefitJob> g_jobs;
bool g_runtime_ready = false;
bool g_logged_waiting = false;
bool g_logged_queued = false;
bool g_logged_active = false;
bool g_logged_completed = false;
bool g_logged_ejecting = false;
bool g_logged_travelling = false;
bool g_logged_travel_progress = false;
A2FO_InlineHook g_dequeue_command_vector_hook{};

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) noexcept {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

const std::uint8_t* bytes(const void* value) noexcept {
    return static_cast<const std::uint8_t*>(value);
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

bool memory_range(const void* pointer, std::size_t size,
                  bool writable) noexcept {
    if (!pointer || size == 0) return false;
    const auto* cursor = static_cast<const std::uint8_t*>(pointer);
    const auto* end = cursor + size;
    if (end < cursor) return false;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information{};
        if (!VirtualQuery(cursor, &information, sizeof(information)) ||
            information.State != MEM_COMMIT ||
            (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            return false;
        }
        if (writable) {
            const DWORD protection = information.Protect & 0xffu;
            if (protection != PAGE_READWRITE &&
                protection != PAGE_WRITECOPY &&
                protection != PAGE_EXECUTE_READWRITE &&
                protection != PAGE_EXECUTE_WRITECOPY) {
                return false;
            }
        }
        const auto* region_end = static_cast<const std::uint8_t*>(
            information.BaseAddress) + information.RegionSize;
        if (region_end <= cursor) return false;
        cursor = std::min(region_end, end);
    }
    return true;
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    return memory_range(pointer, size, false);
}

bool executable_address(const void* pointer) noexcept {
    if (!pointer) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (!VirtualQuery(pointer, &information, sizeof(information)) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
T read_at(const void* object, std::size_t offset,
          T fallback = T{}) noexcept {
    if (!object || !readable_range(bytes(object) + offset, sizeof(T))) {
        return fallback;
    }
    T value{};
    std::memcpy(&value, bytes(object) + offset, sizeof(value));
    return value;
}

template <typename T>
bool write_at(void* object, std::size_t offset,
              const T& value) noexcept {
    if (!object || !memory_range(bytes(object) + offset,
                                 sizeof(T), true)) {
        return false;
    }
    std::memcpy(const_cast<std::uint8_t*>(bytes(object)) + offset,
                &value, sizeof(value));
    return true;
}

std::uint32_t object_handle(const void* object) noexcept {
    return read_at<std::uint32_t>(object, kObjectHandleOffset, 0);
}

void* object_class(const void* object) noexcept {
    return read_at<void*>(object, kObjectClassOffset, nullptr);
}

std::int32_t object_team(const void* object) noexcept {
    return read_at<std::int32_t>(object, kObjectTeamOffset, -1);
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

void* find_class(const std::string& name) noexcept {
    if (!g_armada || name.empty()) return nullptr;
    using FindClassFn = void* (__cdecl*)(const char*);
    const auto find = reinterpret_cast<FindClassFn>(
        at(g_armada, kGameObjectClassFindRva));
    return find ? find(name.c_str()) : nullptr;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    return g_armada && readable_range(at(g_armada, rva), Size) &&
        std::memcmp(at(g_armada, rva), expected, Size) == 0;
}

bool preflight_signatures() noexcept {
    return signature_matches(kGameObjectClassFindRva,
                             kExpectedGameObjectClassFind) &&
        signature_matches(kGameObjectClassGetHierarchyRootRva,
                          kExpectedClassGetHierarchyRoot) &&
        signature_matches(kEntityGetTransformRva,
                          kExpectedEntityGetTransform) &&
        signature_matches(kEntityGetBoundingSphereRva,
                          kExpectedEntityGetBoundingSphere) &&
        signature_matches(kEntityGetWorldTransformRva,
                          kExpectedEntityGetWorldTransform) &&
        signature_matches(kEntityGetRva, kExpectedEntityGet) &&
        signature_matches(kDequeueCommandVectorRva,
                          kExpectedDequeueCommandVector) &&
        signature_matches(kGameObjectSetTransformRva,
                          kExpectedGameObjectSetTransform) &&
        signature_matches(kEvolverSwapObjectsRva,
                          kExpectedEvolverSwapObjects) &&
        signature_matches(kCraftPreventCollisionsRva,
                          kExpectedCraftPreventCollisions) &&
        signature_matches(kCraftPreventCollisionsRva + 0x2c,
                          kExpectedCraftAvoidMeGate) &&
        signature_matches(kSt3dNodeFindRecursiveRva,
                          kExpectedNodeFindRecursive);
}

a2fo::refit::OdfFields copy_fields(
    const A2FO_GameObjectClassLoadedEvent* event) {
    a2fo::refit::OdfFields fields;
    if (!event || !event->odf_fields) return fields;
    fields.reserve(event->odf_field_count);
    for (std::uint32_t index = 0;
         index < event->odf_field_count; ++index) {
        const A2FO_OdfFieldView& field = event->odf_fields[index];
        if (!field.name.data ||
            (!field.value.data && field.value.size != 0)) {
            continue;
        }
        fields.emplace_back(
            std::string(field.name.data, field.name.size),
            std::string(field.value.data ? field.value.data : "",
                        field.value.size));
    }
    return fields;
}

void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->object_class) {
        return;
    }
    try {
        const a2fo::refit::ClassPolicy policy =
            a2fo::refit::parse_class_policy(copy_fields(event));
        if (policy.is_refit_source()) {
            SourceConfig source;
            source.count = std::min(policy.refit_items.size(),
                                    source.items.size());
            for (std::size_t index = 0; index < source.count; ++index) {
                source.items[index] = policy.refit_items[index];
            }
            g_source_classes.insert_or_assign(
                event->object_class, std::move(source));
        }
        if (policy.is_supported_yard()) {
            YardConfig yard;
            yard.hardpoint = policy.refit_hardpoint;
            g_yard_classes.insert_or_assign(
                event->object_class, std::move(yard));
        } else if (!policy.refit_hardpoint.empty()) {
            log_line("Ignored refit yard: requires classLabel shipyard and "
                     "matching refitHardpoint/buildHardpoint");
        }
    } catch (...) {
        log_line("Could not allocate refit class policy");
    }
}

bool resolve_yard_hardpoint(void* yard, YardConfig* config) noexcept {
    if (!yard || !config || !g_armada) return false;
    if (config->hardpoint_resolved) {
        return config->hardpoint_node != nullptr;
    }
    config->hardpoint_resolved = true;
    void* root = reinterpret_cast<void*>(a2fo_refit_call_thiscall_0(
        at(g_armada, kGameObjectClassGetHierarchyRootRva),
        object_class(yard)));
    if (root) {
        config->hardpoint_node = reinterpret_cast<void*>(
            a2fo_refit_call_thiscall_1(
                at(g_armada, kSt3dNodeFindRecursiveRva), root,
                reinterpret_cast<std::uintptr_t>(
                    config->hardpoint.c_str())));
    }
    if (!config->hardpoint_node) {
        log_line("Configured refitHardpoint was not found in yard SOD");
    }
    return config->hardpoint_node != nullptr;
}

bool yard_transform(void* yard, Matrix34* output) noexcept {
    if (!yard || !output || object_expired(yard)) return false;
    const auto found = g_yard_classes.find(object_class(yard));
    if (found == g_yard_classes.end() ||
        !resolve_yard_hardpoint(yard, &found->second)) {
        return false;
    }
    const std::uintptr_t result = a2fo_refit_call_thiscall_2(
        at(g_armada, kEntityGetWorldTransformRva), yard,
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(found->second.hardpoint_node));
    return result == reinterpret_cast<std::uintptr_t>(output);
}

bool object_transform(void* object, Matrix34* output) noexcept {
    if (!object || !output) return false;
    const void* transform = reinterpret_cast<const void*>(
        a2fo_refit_call_thiscall_0(
            at(g_armada, kEntityGetTransformRva), object));
    if (!readable_range(transform, sizeof(*output))) return false;
    std::memcpy(output, transform, sizeof(*output));
    return true;
}

bool object_bounding_sphere(void* object,
                            BoundingSphere* output) noexcept {
    if (!object || !output) return false;
    const auto* sphere = reinterpret_cast<const BoundingSphere*>(
        a2fo_refit_call_thiscall_0(
            at(g_armada, kEntityGetBoundingSphereRva), object));
    if (!readable_range(sphere, sizeof(*sphere))) return false;
    std::memcpy(output, sphere, sizeof(*output));
    if (!std::isfinite(output->radius) || output->radius <= 0.0f) {
        *output = {};
        return false;
    }
    return true;
}

float refit_approach_clearance(void* source, void* yard) noexcept {
    BoundingSphere source_bounds{};
    BoundingSphere yard_bounds{};
    const float source_radius =
        object_bounding_sphere(source, &source_bounds)
            ? source_bounds.radius : 0.0f;
    const float yard_radius =
        object_bounding_sphere(yard, &yard_bounds)
            ? yard_bounds.radius : 0.0f;
    // Mirror OrientedQueueManager's repair/freighter entry rule: put the
    // staging point beyond the host and moving Craft collision spheres.
    return std::min(kMaximumArrivalDistance,
                    std::max(kArrivalDistance,
                             yard_radius + source_radius +
                                 kArrivalClearance));
}

float refit_approach_arrival_distance(void* source) noexcept {
    BoundingSphere source_bounds{};
    const float source_radius =
        object_bounding_sphere(source, &source_bounds)
            ? source_bounds.radius : 0.0f;
    return std::min(kMaximumApproachArrivalDistance,
                    std::max(kApproachArrivalDistance,
                             source_radius +
                                 kApproachArrivalClearance));
}

Matrix34 refit_approach_transform(
    void* source, void* yard,
    const Matrix34& hardpoint_transform) noexcept {
    return a2fo::refit::docking_approach_transform(
        hardpoint_transform, refit_approach_clearance(source, yard));
}

float distance_squared(const Matrix34& left,
                       const Matrix34& right) noexcept {
    const float x = left.values[9] - right.values[9];
    const float y = left.values[10] - right.values[10];
    const float z = left.values[11] - right.values[11];
    return x * x + y * y + z * z;
}

template <typename Visitor>
void for_each_live_object(Visitor&& visitor) noexcept {
    if (!g_armada) return;
    void* list = read_at<void*>(
        at(g_armada, kGameObjectListPointerRva), 0, nullptr);
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

void* nearest_refit_yard(void* source,
                         Matrix34* transform_output) noexcept {
    Matrix34 source_transform{};
    if (!source || !object_transform(source, &source_transform)) {
        return nullptr;
    }
    const std::int32_t team = object_team(source);
    void* best = nullptr;
    Matrix34 best_transform{};
    float best_distance = 0.0f;
    std::uint32_t best_handle = 0;
    for_each_live_object([&](void* candidate) noexcept {
        if (object_team(candidate) != team ||
            g_yard_classes.find(object_class(candidate)) ==
                g_yard_classes.end()) {
            return;
        }
        Matrix34 candidate_transform{};
        if (!yard_transform(candidate, &candidate_transform)) return;
        const float distance = distance_squared(
            source_transform, candidate_transform);
        const std::uint32_t handle = object_handle(candidate);
        if (!best || distance < best_distance ||
            (distance == best_distance && handle < best_handle)) {
            best = candidate;
            best_transform = candidate_transform;
            best_distance = distance;
            best_handle = handle;
        }
    });
    if (best && transform_output) *transform_output = best_transform;
    return best;
}

bool allowed_target(void* source, void* target_class) noexcept {
    const auto found = g_source_classes.find(object_class(source));
    if (found == g_source_classes.end() || !target_class) return false;
    for (std::size_t index = 0; index < found->second.count; ++index) {
        if (find_class(found->second.items[index]) == target_class) {
            return true;
        }
    }
    return false;
}

void execute_move_to_refit_approach(
    void* source, const Matrix34& transform) noexcept {
    const Vector3 destination{
        transform.values[9], transform.values[10], transform.values[11]};
    void* executor = g_dequeue_command_vector_hook.gateway
        ? g_dequeue_command_vector_hook.gateway
        : at(g_armada, kDequeueCommandVectorRva);
    a2fo_refit_call_thiscall_command_vector_int(
        executor, source,
        kMoveSingleCommand, &destination, 0);
}

void execute_docking_stop(void* source) noexcept {
    if (!source || !g_dequeue_command_vector_hook.gateway) return;
    // Enter through the original local executor so this internal transition
    // does not re-enter our synchronized Halt cancellation hook.
    const Vector3 unused_destination{};
    a2fo_refit_call_thiscall_command_vector_int(
        g_dequeue_command_vector_hook.gateway, source,
        3, &unused_destination, 0);
}

bool suppress_source_avoidance(void* source,
                               RefitJob* job) noexcept {
    if (!source || !job) return false;
    if (job->avoidance_suppressed) {
        const std::uint8_t disabled = 0;
        return write_at(source, kObjectAvoidMeOffset, disabled);
    }
    job->saved_avoid_me = read_at<std::uint8_t>(
        source, kObjectAvoidMeOffset, 0);
    const std::uint8_t disabled = 0;
    if (!write_at(source, kObjectAvoidMeOffset, disabled)) return false;
    job->avoidance_suppressed = true;
    return true;
}

bool restore_source_avoidance(void* source,
                              RefitJob* job) noexcept {
    if (!job || !job->avoidance_suppressed) return true;
    if (!source ||
        !write_at(source, kObjectAvoidMeOffset, job->saved_avoid_me) ||
        read_at<std::uint8_t>(source, kObjectAvoidMeOffset, 0xff) !=
            job->saved_avoid_me) {
        return false;
    }
    job->avoidance_suppressed = false;
    return true;
}

void restore_job_source(RefitJob* job) noexcept {
    if (!job || !job->avoidance_suppressed) return;
    restore_source_avoidance(find_entity(job->source_handle), job);
}

bool begin_source_ejection(RefitJob* job) noexcept {
    if (!job) return false;
    if (job->phase == RefitPhase::ejecting) return true;
    if (job->phase != RefitPhase::active ||
        !job->avoidance_suppressed) {
        return false;
    }
    void* source = find_entity(job->source_handle);
    void* yard = find_entity(job->yard_handle);
    if (!source || object_expired(source) || !yard ||
        object_expired(yard)) {
        return false;
    }
    void* output_queue = read_at<void*>(
        yard, kShipyardBuildOutputQueueOffset, nullptr);
    if (!output_queue || !readable_range(output_queue, sizeof(void*))) {
        return false;
    }
    void** output_vtable = read_at<void**>(output_queue, 0, nullptr);
    if (output_vtable !=
            at<void*>(g_armada, kOutputQueueManagerVtableRva) ||
        !readable_range(output_vtable + 4, sizeof(void*))) {
        return false;
    }
    void* add_method = output_vtable[4];
    if (!executable_address(add_method)) return false;

    // Shipyard::FinishBuild calls this same virtual slot against the manager
    // at Shipyard+0x2b4. Resolving the slot dynamically preserves any runtime
    // hook already installed on QueueManager::add while the exact vtable and
    // executable-page checks keep the indirect call constrained to the native
    // OutputQueueManager. It gives a cancelled source the yard's real output
    // path instead of synthesizing a reverse docking transform.
    execute_docking_stop(source);
    a2fo_refit_call_thiscall_1(
        add_method, output_queue,
        reinterpret_cast<std::uintptr_t>(source));
    if (read_at<std::uint32_t>(source, kCraftQueueStageOffset, 0) == 0 ||
        read_at<std::uint32_t>(source, kCraftQueueOwnerOffset, 0) !=
            job->yard_handle) {
        return false;
    }
    job->queue_id = 0;
    job->phase = RefitPhase::ejecting;
    // QueueEnter now protects the overlap exactly as it does for an ordinary
    // completed ship, so our temporary per-instance suppression can end.
    restore_source_avoidance(source, job);
    return true;
}

bool consume_synchronized_refit(
    void* source, void* target_class) noexcept {
    if (!g_runtime_ready || !source || object_expired(source) ||
        !allowed_target(source, target_class)) {
        return false;
    }
    const std::uint32_t source_handle = object_handle(source);
    if (source_handle == 0 || g_jobs.find(source_handle) != g_jobs.end()) {
        return false;
    }
    Matrix34 yard_position{};
    void* yard = nearest_refit_yard(source, &yard_position);
    if (!yard) return false;

    RefitJob job;
    job.source_handle = source_handle;
    job.yard_handle = object_handle(yard);
    job.target_class = target_class;
    try {
        g_jobs.emplace(source_handle, job);
    } catch (...) {
        return false;
    }
    const Matrix34 approach = refit_approach_transform(
        source, yard, yard_position);
    execute_move_to_refit_approach(source, approach);
    log_line("Synchronized refit accepted; native GO_SINGLE installed locally "
             "for travel to the nearest yard's oriented approach point");
    return true;
}

std::size_t enumerate_refit_items(
    void* source, A2FO_RefitUiItem* output,
    std::size_t capacity) noexcept {
    if (!g_runtime_ready || !source || object_expired(source)) return 0;
    const auto found = g_source_classes.find(object_class(source));
    if (found == g_source_classes.end()) return 0;
    const bool already_refitting =
        g_jobs.find(object_handle(source)) != g_jobs.end();
    const bool yard_available = nearest_refit_yard(source, nullptr) != nullptr;
    std::size_t count = 0;
    for (std::size_t index = 0; index < found->second.count; ++index) {
        void* target_class = find_class(found->second.items[index]);
        if (!target_class) continue;
        if (output && count < capacity) {
            output[count].target_class = target_class;
            output[count].enabled =
                !already_refitting && yard_available ? 1u : 0u;
        }
        ++count;
    }
    return count;
}

bool request_refit(void* source, void* target_class) noexcept {
    if (!g_runtime_ready || !source || !target_class ||
        object_expired(source) || !allowed_target(source, target_class) ||
        g_jobs.find(object_handle(source)) != g_jobs.end() ||
        !nearest_refit_yard(source, nullptr)) {
        return false;
    }
    a2fo_refit_call_thiscall_2(
        at(g_armada, kGameObjectQueueClassCommandRva), source,
        A2FO_REFIT_CLASS_COMMAND,
        reinterpret_cast<std::uintptr_t>(target_class));
    return true;
}

bool request_refit_cancel(void* source) noexcept {
    if (!g_runtime_ready || !source || object_expired(source) ||
        !object_class(source)) {
        return false;
    }
    // QueueClassCommand serializes a target-class project ID, so supply the
    // source's own valid class as an ignored payload for the cancel marker.
    a2fo_refit_call_thiscall_2(
        at(g_armada, kGameObjectQueueClassCommandRva), source,
        A2FO_REFIT_CANCEL_CLASS_COMMAND,
        reinterpret_cast<std::uintptr_t>(object_class(source)));
    return true;
}

bool has_waiting_refit(void* producer) noexcept {
    const std::uint32_t handle = object_handle(producer);
    if (handle == 0) return false;
    for (const auto& entry : g_jobs) {
        const RefitJob& job = entry.second;
        if (job.yard_handle == handle &&
            job.phase == RefitPhase::waiting_for_queue) {
            return true;
        }
    }
    return false;
}

void erase_jobs_for_yard(std::uint32_t yard_handle) noexcept {
    for (auto iterator = g_jobs.begin(); iterator != g_jobs.end();) {
        if (iterator->second.yard_handle == yard_handle) {
            restore_job_source(&iterator->second);
            iterator = g_jobs.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void job_removed(void* producer, std::uint32_t queue_id,
                 void*, std::uint32_t removal_kind) noexcept {
    const std::uint32_t yard_handle = object_handle(producer);
    if (yard_handle == 0) return;
    for (auto iterator = g_jobs.begin(); iterator != g_jobs.end();) {
        RefitJob& job = iterator->second;
        const bool producer_destroyed =
            removal_kind == A2FO_REFIT_QUEUE_PRODUCER_DESTROYED;
        const bool exact = job.yard_handle == yard_handle &&
            queue_id != 0 && job.queue_id == queue_id;
        if (producer_destroyed && job.yard_handle == yard_handle) {
            restore_job_source(&job);
            iterator = g_jobs.erase(iterator);
        } else if (exact && begin_source_ejection(&job)) {
            ++iterator;
        } else if (exact) {
            restore_job_source(&job);
            iterator = g_jobs.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void remove_original_after_swap(void* original) noexcept {
    void** debriefing_link = at<void*>(
        g_armada, kDebriefingDataGlobalRva);
    if (readable_range(debriefing_link, sizeof(void*)) &&
        *debriefing_link) {
        a2fo_refit_call_thiscall_2(
            at(g_armada, kDebriefingDestroyShipRva), *debriefing_link,
            reinterpret_cast<std::uintptr_t>(original), 1);
    }
    if (!readable_range(original, sizeof(void*))) return;
    void** vtable = *reinterpret_cast<void***>(original);
    if (readable_range(vtable + 1, sizeof(void*)) && vtable[1]) {
        a2fo_refit_call_thiscall_0(vtable[1], original);
    }
}

void job_finished(void* producer, std::uint32_t queue_id,
                  void* target_class, void* result) noexcept {
    const std::uint32_t yard_handle = object_handle(producer);
    auto found = g_jobs.end();
    for (auto iterator = g_jobs.begin(); iterator != g_jobs.end(); ++iterator) {
        const RefitJob& job = iterator->second;
        if (job.yard_handle == yard_handle &&
            job.queue_id == queue_id &&
            job.target_class == target_class) {
            found = iterator;
            break;
        }
    }
    if (found == g_jobs.end()) return;
    const std::uint32_t source_handle = found->second.source_handle;
    void* source = find_entity(source_handle);
    if (!source || !result || object_expired(source)) {
        if (source && !object_expired(source)) {
            restore_source_avoidance(source, &found->second);
        }
        g_jobs.erase(found);
        log_line("Refit build finished without a live replacement source");
        return;
    }

    // Keep the old hull non-avoiding through the identity handoff. Restoring
    // it here would make the two overlapping ships repel one another during
    // the completion tick. The source is removed immediately after the swap.
    g_jobs.erase(found);
    Matrix34 source_transform{};
    if (object_transform(source, &source_transform)) {
        a2fo_refit_call_thiscall_1(
            at(g_armada, kGameObjectSetTransformRva), result,
            reinterpret_cast<std::uintptr_t>(&source_transform));
    }
    a2fo_refit_call_thiscall_1(
        at(g_armada, kEvolverSwapObjectsRva), source,
        reinterpret_cast<std::uintptr_t>(result));
    remove_original_after_swap(source);
    if (!g_logged_completed) {
        g_logged_completed = true;
        log_line("Refit completed; replacement inherited the original "
                 "ship identity and the original was removed");
    }
}

void update_refit_job(void* source, RefitJob* job,
                      float elapsed_seconds) noexcept {
    if (!source || !job) return;
    void* yard = find_entity(job->yard_handle);
    if (!yard || object_expired(yard) ||
        object_team(yard) != object_team(source)) {
        restore_source_avoidance(source, job);
        g_jobs.erase(job->source_handle);
        return;
    }

    if (job->phase == RefitPhase::ejecting) {
        const bool avoidance_restored =
            restore_source_avoidance(source, job);
        const std::uint32_t queue_stage = read_at<std::uint32_t>(
            source, kCraftQueueStageOffset, 0);
        if (avoidance_restored && queue_stage == 0) {
            const std::uint32_t source_handle = job->source_handle;
            g_jobs.erase(source_handle);
            if (!g_logged_ejecting) {
                g_logged_ejecting = true;
                log_line("Cancelled refit handed to the yard's native "
                         "completed-ship output queue; source collision "
                         "avoidance restored");
            }
        }
        return;
    }

    Matrix34 source_transform{};
    Matrix34 refit_transform{};
    if (!object_transform(source, &source_transform) ||
        !yard_transform(yard, &refit_transform)) {
        return;
    }
    const Matrix34 approach_transform = refit_approach_transform(
        source, yard, refit_transform);

    if (job->phase == RefitPhase::travelling) {
        const float arrival_distance =
            refit_approach_arrival_distance(source);
        const float remaining_squared =
            distance_squared(source_transform, approach_transform);
        if (!g_logged_travelling) {
            g_logged_travelling = true;
            char message[192]{};
            std::snprintf(
                message, sizeof(message),
                "Refit travel started: %.1f units from yard approach "
                "(arrival radius %.1f)",
                std::sqrt(std::max(remaining_squared, 0.0f)),
                arrival_distance);
            log_line(message);
        }
        if (job->last_distance_squared < 0.0f) {
            job->last_distance_squared = remaining_squared;
        }
        if (remaining_squared <= arrival_distance * arrival_distance) {
            execute_docking_stop(source);
            job->phase = RefitPhase::waiting_for_queue;
            job->retry_ticks = kRetryIntervalTicks;
            if (!g_logged_waiting) {
                g_logged_waiting = true;
                log_line("Refit ship arrived and is waiting for a native "
                         "Producer queue slot");
            }
        } else if (++job->move_ticks >= kMoveRefreshTicks) {
            job->move_ticks = 0;
            if (!g_logged_travel_progress) {
                g_logged_travel_progress = true;
                char message[192]{};
                const float previous = std::sqrt(std::max(
                    job->last_distance_squared, 0.0f));
                const float current = std::sqrt(std::max(
                    remaining_squared, 0.0f));
                std::snprintf(
                    message, sizeof(message),
                    "Refit travel checkpoint: %.1f units remain "
                    "(%.1f units moved toward yard)",
                    current, previous - current);
                log_line(message);
            }
            job->last_distance_squared = remaining_squared;
            execute_move_to_refit_approach(source, approach_transform);
        }
        return;
    }

    if (job->phase == RefitPhase::waiting_for_queue) {
        if (++job->retry_ticks < kRetryIntervalTicks) return;
        job->retry_ticks = 0;
        const std::uint32_t queue_id = g_push_refit
            ? g_push_refit(yard, job->target_class) : 0;
        if (queue_id == 0) return;
        job->queue_id = queue_id;
        job->phase = RefitPhase::queued;
        if (!g_logged_queued) {
            g_logged_queued = true;
            log_line("Refit entered the yard's native production queue");
        }
        return;
    }

    const std::uint32_t current_id = read_at<std::uint32_t>(
        yard, kCurrentQueueIdOffset, 0);
    void* current_class = read_at<void*>(
        yard, kCurrentBuildClassOffset, nullptr);
    const bool active = current_id == job->queue_id &&
        current_class == job->target_class;
    if (active) {
        const bool entering_active = job->phase != RefitPhase::active;
        suppress_source_avoidance(source, job);
        if (entering_active) {
            execute_docking_stop(source);
            job->docking_origin = source_transform;
            job->docking_elapsed_seconds = 0.0f;
        }
        job->phase = RefitPhase::active;
        const float simulation_step =
            std::isfinite(elapsed_seconds) && elapsed_seconds > 0.0f
                ? elapsed_seconds : 0.0f;
        job->docking_elapsed_seconds = std::min(
            kDockingTransitionSeconds,
            job->docking_elapsed_seconds + simulation_step);
        Matrix34 docking_transform = refit_transform;
        if (job->docking_elapsed_seconds <
            kDockingTransitionSeconds) {
            const float progress =
                job->docking_elapsed_seconds /
                kDockingTransitionSeconds;
            docking_transform = a2fo::refit::interpolate_docking_transform(
                job->docking_origin, refit_transform, progress);
        }
        a2fo_refit_call_thiscall_1(
            at(g_armada, kGameObjectSetTransformRva), source,
            reinterpret_cast<std::uintptr_t>(&docking_transform));
        if (!g_logged_active) {
            g_logged_active = true;
            log_line("Refit construction active; source collision avoidance "
                     "suspended for a five-second smooth dock into the yard "
                     "build hardpoint, then held while progress fills");
        }
    } else if (++job->move_ticks >= kMoveRefreshTicks) {
        job->move_ticks = 0;
        execute_move_to_refit_approach(source, approach_transform);
    }
}

bool cancel_source_job(void* source,
                       bool allow_ejection = true) noexcept {
    const std::uint32_t source_handle = object_handle(source);
    auto found = g_jobs.find(source_handle);
    if (found == g_jobs.end()) return false;
    const std::uint32_t yard_handle = found->second.yard_handle;
    const std::uint32_t queue_id = found->second.queue_id;
    if (!allow_ejection) {
        restore_source_avoidance(source, &found->second);
        g_jobs.erase(found);
    }
    if (queue_id != 0) {
        if (void* yard = find_entity(yard_handle)) {
            a2fo_refit_call_thiscall_1(
                at(g_fleet_ops, kFoProducerActDeleteRva), yard, queue_id);
        }
    }
    if (!allow_ejection) return true;

    // Producer deletion normally calls job_removed and starts native output.
    // Keep this fallback for a missing/late optional bridge.
    found = g_jobs.find(source_handle);
    if (found == g_jobs.end()) return true;
    if (begin_source_ejection(&found->second)) return true;
    restore_source_avoidance(source, &found->second);
    g_jobs.erase(found);
    return true;
}

bool cancel_synchronized_refit(void* source) noexcept {
    if (!g_runtime_ready || !source || object_expired(source)) return false;
    if (cancel_source_job(source)) {
        log_line("Synchronized refit cancelled by Halt");
    }
    // Treat cancellation as idempotent: the source may receive repeated Halt
    // packets or its refit may have completed just before this packet arrived.
    return true;
}

void __attribute__((fastcall)) dequeue_command_vector_hook(
    void* source, void*, std::int32_t command,
    const Vector3* destination, std::int32_t value) noexcept {
    // Halt itself is a synchronized command-0x10 packet.  Cancel the local
    // refit job while every peer is processing that same packet, then preserve
    // Armada's ordinary STOP behaviour through the gateway.
    if (g_runtime_ready && command == 3 && cancel_source_job(source)) {
        log_line("Synchronized refit cancelled by native Halt command");
    }
    a2fo_refit_call_thiscall_command_vector_int(
        g_dequeue_command_vector_hook.gateway, source,
        command, destination, value);
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !g_runtime_ready || !event->craft) {
        return;
    }
    void* craft = event->craft;
    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP) {
        cancel_source_job(craft, false);
        if (g_yard_classes.find(object_class(craft)) !=
                g_yard_classes.end()) {
            erase_jobs_for_yard(object_handle(craft));
        }
        return;
    }
    if (event->kind != A2FO_CRAFT_EVENT_SIMULATE_POST ||
        object_expired(craft)) {
        return;
    }
    const auto found = g_jobs.find(object_handle(craft));
    if (found != g_jobs.end()) {
        update_refit_job(craft, &found->second,
                         event->elapsed_seconds);
    }
}

template <typename Function>
bool import_export(HMODULE module, const char* name,
                   Function* output) noexcept {
    if (!module || !name || !output) return false;
    FARPROC address = GetProcAddress(module, name);
    static_assert(sizeof(Function) == sizeof(address),
                  "function pointer size mismatch");
    std::memcpy(output, &address, sizeof(address));
    return *output != nullptr;
}

bool register_bridges() noexcept {
    HMODULE feature_pack = GetModuleHandleA("A2FOFeaturePack.dll");
    HMODULE hybrid_build = GetModuleHandleA("A2FOHybridBuild.dll");
    A2FO_RegisterRefitQueueBridgeFn register_queue = nullptr;
    A2FO_RegisterRefitUiBridgeFn register_ui = nullptr;
    if (!import_export(feature_pack, "A2FO_RegisterRefitQueueBridge",
                       &register_queue) ||
        !import_export(feature_pack, "A2FO_ProducerPushRefit",
                       &g_push_refit) ||
        !import_export(hybrid_build, "A2FO_RegisterRefitUiBridge",
                       &register_ui)) {
        log_line("FeaturePack/HybridBuild refit bridge exports unavailable");
        return false;
    }

    A2FO_RefitQueueBridge queue_bridge{};
    queue_bridge.struct_size = sizeof(queue_bridge);
    queue_bridge.version = A2FO_REFIT_QUEUE_BRIDGE_VERSION;
    queue_bridge.consume_synchronized_command =
        &consume_synchronized_refit;
    queue_bridge.cancel_synchronized_command =
        &cancel_synchronized_refit;
    queue_bridge.has_waiting_job = &has_waiting_refit;
    queue_bridge.job_finished = &job_finished;
    queue_bridge.job_removed = &job_removed;

    A2FO_RefitUiBridge ui_bridge{};
    ui_bridge.struct_size = sizeof(ui_bridge);
    ui_bridge.version = A2FO_REFIT_UI_BRIDGE_VERSION;
    ui_bridge.enumerate_items = &enumerate_refit_items;
    ui_bridge.request_refit = &request_refit;
    ui_bridge.cancel_refit = &request_refit_cancel;
    return register_queue(&queue_bridge) && register_ui(&ui_bridge);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->install_inline_hook ||
        !api->armada_module || !api->fleetops_module ||
        (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0 ||
        !A2FO_MODULE_API_HAS(api,
                            register_game_object_class_loaded_handler) ||
        !api->register_game_object_class_loaded_handler ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) ||
        !api->register_craft_event_handler_masked) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops || !preflight_signatures()) {
        log_line("Native refit dependency signature mismatch; disabled");
        return false;
    }
    if (!api->install_inline_hook(
            at(g_armada, kDequeueCommandVectorRva),
            reinterpret_cast<void*>(&dequeue_command_vector_hook),
            sizeof(kExpectedDequeueCommandVector),
            kExpectedDequeueCommandVector,
            &g_dequeue_command_vector_hook)) {
        log_line("Native synchronized movement receiver hook failed; disabled");
        return false;
    }
    try {
        g_source_classes.reserve(128);
        g_yard_classes.reserve(32);
        g_jobs.reserve(64);
    } catch (...) {
        log_line("Refit registry allocation failed");
        return false;
    }
    const bool classes_registered =
        api->register_game_object_class_loaded_handler(
            kModuleName, kRequiredFields.data(),
            static_cast<std::uint32_t>(kRequiredFields.size()),
            &class_loaded_handler, nullptr);
    const bool craft_registered =
        api->register_craft_event_handler_masked(
            kModuleName,
            A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                A2FO_CRAFT_EVENT_MASK_CLEANUP,
            &craft_event_handler, nullptr);
    if (!classes_registered || !craft_registered) {
        log_line("Shared class/Craft event registration failed");
        // The core may already retain one callback. Keep the DLL resident;
        // g_runtime_ready remains false so every partial registration is a
        // fail-closed no-op.
        return true;
    }
    if (!register_bridges()) {
        // One of the two hook owners may already retain this DLL's callback.
        // Keep it resident and disabled rather than leave a dangling pointer.
        log_line("Refit bridge registration failed; module kept resident "
                 "with gameplay disabled");
        return true;
    }
    g_runtime_ready = true;
    log_line("Refit yards initialized with synchronized requests, nearest-"
             "yard travel, native queue progress, and replacement handoff");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
    for (auto& entry : g_jobs) {
        restore_job_source(&entry.second);
    }
    g_jobs.clear();
}
