/*
 * A2FORefitYards: synchronized ship-to-yard refits using the native Producer
 * FIFO, build progress, costs, completion object, and replacement handoff.
 */

#include "docking_transform.hpp"
#include "refit_policy.hpp"
#include "../../rust/stage10_presentation_completion_ffi.h"
#include "../../rust/stage8_refit_ffi.h"

#include "../A2FOFeaturePack/refit_queue_bridge_api.hpp"
#include "../A2FOHybridBuild/refit_ui_bridge_api.hpp"
#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_module_provider.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
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

struct RefitClassContext {const A2FO_GameObjectClassLoadedEvent* event;a2fo::refit::ClassPolicy policy{};SourceConfig source{};};
std::uint64_t refit_class_step(void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitClassContext*>(opaque);
    try {switch(op){
    case 0:return c.event&&c.event->struct_size>=sizeof(*c.event);
    case 1:return c.event->object_class!=nullptr;
    case 2:c.policy=a2fo::refit::parse_class_policy(copy_fields(c.event));return 0;
    case 3:return c.policy.is_refit_source();
    case 4:return c.policy.refit_items.size();
    case 5:c.source.count=a;return 0;
    case 6:c.source.items[a]=c.policy.refit_items[a];return 0;
    case 7:g_source_classes.insert_or_assign(c.event->object_class,std::move(c.source));return 0;
    case 8:return c.policy.is_supported_yard();
    case 9:{YardConfig yard;yard.hardpoint=c.policy.refit_hardpoint;g_yard_classes.insert_or_assign(c.event->object_class,std::move(yard));return 0;}
    case 10:return !c.policy.refit_hardpoint.empty();
    case 11:log_line("Ignored refit yard: requires classLabel shipyard and matching refitHardpoint/buildHardpoint");return 0;
    case 12:log_line("Could not allocate refit class policy");return 0;
    default:return 0;
    }}catch(...){return UINT64_MAX;}
}
void A2FO_CALL class_loaded_handler(const A2FO_GameObjectClassLoadedEvent* event,void*) {
    RefitClassContext context{event};a2fo_rs_stage8_refit_flow(9,&refit_class_step,&context);
}

struct RefitHardpointContext {void* yard;YardConfig* config;void* root=nullptr;};
std::uint64_t refit_hardpoint_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitHardpointContext*>(opaque);
    switch(op){
    case 0:return c.yard&&c.config&&g_armada;
    case 1:return c.config->hardpoint_resolved;
    case 2:return c.config->hardpoint_node!=nullptr;
    case 3:c.config->hardpoint_resolved=true;return 0;
    case 4:c.root=reinterpret_cast<void*>(a2fo_refit_call_thiscall_0(at(g_armada,kGameObjectClassGetHierarchyRootRva),object_class(c.yard)));return c.root!=nullptr;
    case 5:c.config->hardpoint_node=reinterpret_cast<void*>(a2fo_refit_call_thiscall_1(at(g_armada,kSt3dNodeFindRecursiveRva),c.root,reinterpret_cast<std::uintptr_t>(c.config->hardpoint.c_str())));return 0;
    case 6:log_line("Configured refitHardpoint was not found in yard SOD");return 0;
    default:return 0;
    }
}
bool resolve_yard_hardpoint(void* yard,YardConfig* config) noexcept {RefitHardpointContext c{yard,config};return a2fo_rs_stage8_refit_flow(16,&refit_hardpoint_step,&c)!=0;}

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
    struct Context {Visitor& visitor;void* list{};void* head{};void* node{};void* next{};void* object{};
        std::uint8_t failed{};} c{visitor};
    const auto step=+[](void* opaque,std::uint32_t id,std::uint64_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(id){
        case 0:return g_armada!=nullptr;
        case 1:c.list=read_at<void*>(at(g_armada,kGameObjectListPointerRva),0,nullptr);return readable_range(c.list,12);
        case 2:c.head=read_at<void*>(c.list,4,nullptr);return 0;
        case 3:return read_at<std::uint32_t>(c.list,8,0);
        case 4:return readable_range(c.head,12);
        case 6:c.node=read_at<void*>(c.head,0,nullptr);return 0;
        case 7:return c.node!=nullptr;
        case 8:return c.node!=c.head;
        case 10:c.next=read_at<void*>(c.node,0,nullptr);return 0;
        case 11:c.object=read_at<void*>(c.node,8,nullptr);return 0;
        case 12:return c.object!=nullptr;
        case 13:return object_expired(c.object);
        case 14:c.visitor(c.object);return 0;
        case 15:return c.next==c.node;
        case 16:c.node=c.next;return 0;
        default:return 0;
        }
    };
    a2fo_rs_stage10_presentation_flow(6,&c,step,&c.failed,kMaximumObjects);
}

struct RefitNearestContext {
    void* source;Matrix34* output;Matrix34 source_transform{},best_transform{},candidate_transform{};
    std::int32_t team=-1;void* best=nullptr;void* candidate=nullptr;
    float best_distance=0,distance=0;std::uint32_t best_handle=0,handle=0;
};
std::uint64_t refit_candidate_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitNearestContext*>(opaque);
    switch(op){
    case 0:return static_cast<std::uint64_t>(static_cast<std::int64_t>(object_team(c.candidate)));
    case 1:return static_cast<std::uint64_t>(static_cast<std::int64_t>(c.team));
    case 2:return g_yard_classes.find(object_class(c.candidate))!=g_yard_classes.end();
    case 3:c.candidate_transform={};return yard_transform(c.candidate,&c.candidate_transform);
    case 4:c.distance=distance_squared(c.source_transform,c.candidate_transform);return 0;
    case 5:c.handle=object_handle(c.candidate);return 0;
    case 6:return c.best!=nullptr;
    case 7:return c.distance<c.best_distance;
    case 8:return c.distance==c.best_distance;
    case 9:return c.handle<c.best_handle;
    case 10:c.best=c.candidate;c.best_transform=c.candidate_transform;c.best_distance=c.distance;c.best_handle=c.handle;return 0;
    default:return 0;
    }
}
std::uint64_t refit_nearest_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitNearestContext*>(opaque);
    switch(op){
    case 0:return c.source!=nullptr;
    case 1:return object_transform(c.source,&c.source_transform);
    case 2:c.team=object_team(c.source);return 0;
    case 3:for_each_live_object([&](void* candidate)noexcept{c.candidate=candidate;a2fo_rs_stage8_refit_flow(20,&refit_candidate_step,&c);});return 0;
    case 4:return c.best!=nullptr;
    case 5:return c.output!=nullptr;
    case 6:*c.output=c.best_transform;return 0;
    case 7:return reinterpret_cast<std::uintptr_t>(c.best);
    default:return 0;
    }
}
void* nearest_refit_yard(void* source,Matrix34* transform_output) noexcept {RefitNearestContext c{source,transform_output};return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a2fo_rs_stage8_refit_flow(21,&refit_nearest_step,&c)));}

struct RefitTargetContext {void* source;void* target;SourceConfig* config=nullptr;};
std::uint64_t refit_target_step(void* opaque,std::uint32_t op,std::uintptr_t index,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitTargetContext*>(opaque);
    switch(op){
    case 0:{const auto found=g_source_classes.find(object_class(c.source));c.config=found==g_source_classes.end()?nullptr:&found->second;return c.config!=nullptr;}
    case 1:return c.target!=nullptr;
    case 2:return c.config->count;
    case 3:return find_class(c.config->items[index])==c.target;
    default:return 0;
    }
}
bool allowed_target(void* source,void* target_class) noexcept {
    RefitTargetContext c{source,target_class};return a2fo_rs_stage8_refit_flow(15,&refit_target_step,&c)!=0;
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

struct RefitAvoidanceContext {void* source;RefitJob* job;};
std::uint64_t refit_avoidance_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitAvoidanceContext*>(opaque);
    switch(op){
    case 0:return c.job!=nullptr;
    case 1:return c.source!=nullptr;
    case 2:return c.job->avoidance_suppressed;
    case 3:c.job->saved_avoid_me=read_at<std::uint8_t>(c.source,kObjectAvoidMeOffset,0);return 0;
    case 4:return write_at(c.source,kObjectAvoidMeOffset,std::uint8_t{0});
    case 5:c.job->avoidance_suppressed=true;return 0;
    case 6:return write_at(c.source,kObjectAvoidMeOffset,c.job->saved_avoid_me);
    case 7:return read_at<std::uint8_t>(c.source,kObjectAvoidMeOffset,0xff)==c.job->saved_avoid_me;
    case 8:c.job->avoidance_suppressed=false;return 0;
    default:return 0;
    }
}
bool suppress_source_avoidance(void* source,RefitJob* job) noexcept {RefitAvoidanceContext c{source,job};return a2fo_rs_stage8_refit_flow(12,&refit_avoidance_step,&c)!=0;}

bool restore_source_avoidance(void* source,RefitJob* job) noexcept {RefitAvoidanceContext c{source,job};return a2fo_rs_stage8_refit_flow(13,&refit_avoidance_step,&c)!=0;}

struct RefitRestoreContext {RefitJob* job;void* source=nullptr;};
std::uint64_t refit_restore_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitRestoreContext*>(opaque);
    switch(op){
    case 0:return c.job!=nullptr;
    case 1:return c.job->avoidance_suppressed;
    case 2:c.source=find_entity(c.job->source_handle);return 0;
    case 3:return restore_source_avoidance(c.source,c.job);
    default:return 0;
    }
}
void restore_job_source(RefitJob* job) noexcept {RefitRestoreContext c{job};a2fo_rs_stage8_refit_flow(18,&refit_restore_step,&c);}

struct RefitEjectionContext {RefitJob* job;void* source=nullptr;void* yard=nullptr;void* output_queue=nullptr;void* add_method=nullptr;};
std::uint64_t refit_ejection_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitEjectionContext*>(opaque);
    switch(op){
    case 0:return c.job!=nullptr;
    case 1:return static_cast<std::uint8_t>(c.job->phase);
    case 2:return c.job->avoidance_suppressed;
    case 3:c.source=find_entity(c.job->source_handle);return 0;
    case 4:c.yard=find_entity(c.job->yard_handle);return 0;
    case 5:return c.source!=nullptr;
    case 6:return object_expired(c.source);
    case 7:return c.yard!=nullptr;
    case 8:return object_expired(c.yard);
    case 9:{
        c.output_queue=read_at<void*>(c.yard,kShipyardBuildOutputQueueOffset,nullptr);
        if(!c.output_queue||!readable_range(c.output_queue,sizeof(void*)))return 0;
        void** table=read_at<void**>(c.output_queue,0,nullptr);
        if(table!=at<void*>(g_armada,kOutputQueueManagerVtableRva)||!readable_range(table+4,sizeof(void*)))return 0;
        c.add_method=table[4];return executable_address(c.add_method);
    }
    case 10:execute_docking_stop(c.source);return 0;
    case 11:a2fo_refit_call_thiscall_1(c.add_method,c.output_queue,reinterpret_cast<std::uintptr_t>(c.source));return 0;
    case 12:return read_at<std::uint32_t>(c.source,kCraftQueueStageOffset,0);
    case 13:return read_at<std::uint32_t>(c.source,kCraftQueueOwnerOffset,0);
    case 14:return c.job->yard_handle;
    case 15:c.job->queue_id=0;return 0;
    case 16:c.job->phase=RefitPhase::ejecting;return 0;
    case 17:return restore_source_avoidance(c.source,c.job);
    default:return 0;
    }
}
bool begin_source_ejection(RefitJob* job) noexcept {RefitEjectionContext c{job};return a2fo_rs_stage8_refit_flow(11,&refit_ejection_step,&c)!=0;}

struct RefitConsumeContext {void* source;void* target;std::uint32_t handle=0;void* yard=nullptr;Matrix34 position{},approach{};};
std::uint64_t refit_consume_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitConsumeContext*>(opaque);
    switch(op){
    case 0:return g_runtime_ready;
    case 1:return c.source!=nullptr;
    case 2:return object_expired(c.source);
    case 3:return allowed_target(c.source,c.target);
    case 4:c.handle=object_handle(c.source);return c.handle;
    case 5:return g_jobs.find(c.handle)!=g_jobs.end();
    case 6:c.yard=nearest_refit_yard(c.source,&c.position);return c.yard!=nullptr;
    case 7:{RefitJob job;job.source_handle=c.handle;job.yard_handle=object_handle(c.yard);job.target_class=c.target;
        try{g_jobs.emplace(c.handle,job);return 1;}catch(...){return 0;}}
    case 8:c.approach=refit_approach_transform(c.source,c.yard,c.position);return 0;
    case 9:execute_move_to_refit_approach(c.source,c.approach);return 0;
    case 10:log_line("Synchronized refit accepted; native GO_SINGLE installed locally for travel to the nearest yard's oriented approach point");return 0;
    default:return 0;
    }
}
bool consume_synchronized_refit(void* source,void* target_class) noexcept {
    RefitConsumeContext c{source,target_class};return a2fo_rs_stage8_refit_flow(0,&refit_consume_step,&c)!=0;
}

struct RefitEnumerateContext {void* source;A2FO_RefitUiItem* output;std::size_t capacity;SourceConfig* config=nullptr;void* target=nullptr;};
std::uint64_t refit_enumerate_step(void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t b) noexcept {
    auto& c=*static_cast<RefitEnumerateContext*>(opaque);
    switch(op){
    case 0:return g_runtime_ready;
    case 1:return c.source!=nullptr;
    case 2:return object_expired(c.source);
    case 3:{const auto found=g_source_classes.find(object_class(c.source));c.config=found==g_source_classes.end()?nullptr:&found->second;return c.config!=nullptr;}
    case 4:return g_jobs.find(object_handle(c.source))!=g_jobs.end();
    case 5:return nearest_refit_yard(c.source,nullptr)!=nullptr;
    case 6:return c.config->count;
    case 7:c.target=find_class(c.config->items[a]);return c.target!=nullptr;
    case 8:return c.output && a<c.capacity;
    case 9:c.output[a].target_class=c.target;c.output[a].enabled=static_cast<std::uint32_t>(b);return 0;
    default:return 0;
    }
}
std::size_t enumerate_refit_items(void* source,A2FO_RefitUiItem* output,std::size_t capacity) noexcept {
    RefitEnumerateContext c{source,output,capacity};return static_cast<std::size_t>(a2fo_rs_stage8_refit_flow(1,&refit_enumerate_step,&c));
}

struct RefitRequestContext {void* source;void* target;};
std::uint64_t refit_request_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitRequestContext*>(opaque);
    switch(op){
    case 0:return g_runtime_ready;
    case 1:return c.source!=nullptr;
    case 2:return c.target!=nullptr;
    case 3:return object_expired(c.source);
    case 4:return allowed_target(c.source,c.target);
    case 5:return g_jobs.find(object_handle(c.source))!=g_jobs.end();
    case 6:return nearest_refit_yard(c.source,nullptr)!=nullptr;
    case 7:a2fo_refit_call_thiscall_2(at(g_armada,kGameObjectQueueClassCommandRva),c.source,A2FO_REFIT_CLASS_COMMAND,reinterpret_cast<std::uintptr_t>(c.target));return 0;
    default:return 0;
    }
}
bool request_refit(void* source,void* target_class) noexcept {
    RefitRequestContext c{source,target_class};return a2fo_rs_stage8_refit_flow(2,&refit_request_step,&c)!=0;
}

std::uint64_t refit_cancel_request_step(void* source,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    switch(op){
    case 0:return g_runtime_ready;
    case 1:return source!=nullptr;
    case 2:return object_expired(source);
    case 3:return object_class(source)!=nullptr;
    case 4:a2fo_refit_call_thiscall_2(at(g_armada,kGameObjectQueueClassCommandRva),source,A2FO_REFIT_CANCEL_CLASS_COMMAND,reinterpret_cast<std::uintptr_t>(object_class(source)));return 0;
    default:return 0;
    }
}
bool request_refit_cancel(void* source) noexcept {return a2fo_rs_stage8_refit_flow(3,&refit_cancel_request_step,source)!=0;}

struct RefitWaitingContext {void* producer;std::uint32_t handle=0;std::unordered_map<std::uint32_t,RefitJob>::const_iterator iterator{};};
std::uint64_t refit_waiting_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitWaitingContext*>(opaque);
    switch(op){
    case 0:c.handle=object_handle(c.producer);return c.handle;
    case 1:c.iterator=g_jobs.begin();return 0;
    case 2:return c.iterator!=g_jobs.end();
    case 3:return c.iterator->second.yard_handle==c.handle;
    case 4:return c.iterator->second.phase==RefitPhase::waiting_for_queue;
    case 5:++c.iterator;return 0;
    default:return 0;
    }
}
bool has_waiting_refit(void* producer) noexcept {RefitWaitingContext c{producer};return a2fo_rs_stage8_refit_flow(4,&refit_waiting_step,&c)!=0;}

struct RefitEraseContext {std::uint32_t handle;std::unordered_map<std::uint32_t,RefitJob>::iterator iterator{};};
std::uint64_t refit_erase_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitEraseContext*>(opaque);
    switch(op){
    case 0:c.iterator=g_jobs.begin();return 0;
    case 1:return c.iterator!=g_jobs.end();
    case 2:return c.iterator->second.yard_handle==c.handle;
    case 3:restore_job_source(&c.iterator->second);return 0;
    case 4:c.iterator=g_jobs.erase(c.iterator);return 0;
    case 5:++c.iterator;return 0;
    default:return 0;
    }
}
void erase_jobs_for_yard(std::uint32_t yard_handle) noexcept {RefitEraseContext c{yard_handle};a2fo_rs_stage8_refit_flow(19,&refit_erase_step,&c);}

struct RefitRemovedContext {RefitJob* job;std::uint32_t yard,queue;};
std::uint64_t refit_removed_step(void* opaque,std::uint32_t op,
                                std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitRemovedContext*>(opaque);
    switch(op) {
    case 0:return c.job->yard_handle==c.yard;
    case 1:restore_job_source(c.job);return 0;
    case 2:return c.queue!=0;
    case 3:return c.job->queue_id==c.queue;
    case 4:return begin_source_ejection(c.job);
    default:return 0;
    }
}
void job_removed(void* producer,std::uint32_t queue_id,void*,std::uint32_t removal_kind) noexcept {
    const auto yard=object_handle(producer);if(yard==0)return;
    for(auto i=g_jobs.begin();i!=g_jobs.end();) {
        RefitRemovedContext context{&i->second,yard,queue_id};
        if(a2fo_rs_stage8_refit_removed(removal_kind==A2FO_REFIT_QUEUE_PRODUCER_DESTROYED,
            &refit_removed_step,&context))i=g_jobs.erase(i);else ++i;
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

struct RefitFinishedContext {
    void* producer;std::uint32_t queue;void* target;void* result;
    std::unordered_map<std::uint32_t,RefitJob>::iterator found{};
    void* source=nullptr;Matrix34 transform{};
    std::uint32_t yard=0;std::unordered_map<std::uint32_t,RefitJob>::iterator iterator{};
};
std::uint64_t refit_finish_find_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitFinishedContext*>(opaque);
    switch(op){
    case 0:c.yard=object_handle(c.producer);c.found=g_jobs.end();c.iterator=g_jobs.begin();return 0;
    case 1:return c.iterator!=g_jobs.end();
    case 2:return c.iterator->second.yard_handle;
    case 3:return c.yard;
    case 4:return c.iterator->second.queue_id;
    case 5:return c.queue;
    case 6:return reinterpret_cast<std::uintptr_t>(c.iterator->second.target_class);
    case 7:return reinterpret_cast<std::uintptr_t>(c.target);
    case 8:c.found=c.iterator;return 0;
    case 9:++c.iterator;return 0;
    default:return 0;
    }
}
std::uint64_t refit_finished_step(void* opaque,std::uint32_t op,
                                 std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitFinishedContext*>(opaque);
    switch(op) {
    case 0:return a2fo_rs_stage8_refit_flow(22,&refit_finish_find_step,&c);
    case 1:c.source=find_entity(c.found->second.source_handle);return 0;
    case 2:return c.source!=nullptr;
    case 3:return c.result!=nullptr;
    case 4:return object_expired(c.source);
    case 5:return restore_source_avoidance(c.source,&c.found->second);
    case 6:g_jobs.erase(c.found);return 0;
    case 7:log_line("Refit build finished without a live replacement source");return 0;
    case 8:return object_transform(c.source,&c.transform);
    case 9:a2fo_refit_call_thiscall_1(at(g_armada,kGameObjectSetTransformRva),c.result,
        reinterpret_cast<std::uintptr_t>(&c.transform));return 0;
    case 10:a2fo_refit_call_thiscall_1(at(g_armada,kEvolverSwapObjectsRva),c.source,
        reinterpret_cast<std::uintptr_t>(c.result));return 0;
    case 11:remove_original_after_swap(c.source);return 0;
    case 12:return g_logged_completed;
    case 13:g_logged_completed=true;log_line("Refit completed; replacement inherited the original ship identity and the original was removed");return 0;
    default:return 0;
    }
}
void job_finished(void* producer,std::uint32_t queue_id,void* target_class,void* result) noexcept {
    RefitFinishedContext context{producer,queue_id,target_class,result};
    a2fo_rs_stage8_refit_finished(&refit_finished_step,&context);
}

struct RefitUpdateContext {
    void* source; RefitJob* job; float elapsed; void* yard=nullptr;
    Matrix34 source_transform{},refit_transform{},approach_transform{},docking_transform{};
    float arrival_distance=0,remaining_squared=0;
};
std::uint64_t refit_update_step(void* opaque,std::uint32_t op,
                               std::uintptr_t a,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitUpdateContext*>(opaque);
    switch(op) {
    case 0:return c.source && c.job;
    case 1:c.yard=find_entity(c.job->yard_handle);return c.yard!=nullptr;
    case 2:return object_expired(c.yard);
    case 3:return static_cast<std::uint64_t>(static_cast<std::int64_t>(object_team(c.yard)));
    case 4:return static_cast<std::uint64_t>(static_cast<std::int64_t>(object_team(c.source)));
    case 5:return restore_source_avoidance(c.source,c.job);
    case 6:g_jobs.erase(c.job->source_handle);return 0;
    case 7:return static_cast<std::uint8_t>(c.job->phase);
    case 8:return read_at<std::uint32_t>(c.source,kCraftQueueStageOffset,0);
    case 9:return object_transform(c.source,&c.source_transform);
    case 10:return yard_transform(c.yard,&c.refit_transform);
    case 11:c.approach_transform=refit_approach_transform(c.source,c.yard,c.refit_transform);return 0;
    case 12:
        c.arrival_distance=refit_approach_arrival_distance(c.source);
        c.remaining_squared=distance_squared(c.source_transform,c.approach_transform);return 0;
    case 13:return g_logged_travelling;
    case 14:{
        g_logged_travelling=true;char message[192]{};
        std::snprintf(message,sizeof(message),
            "Refit travel started: %.1f units from yard approach (arrival radius %.1f)",
            std::sqrt(std::max(c.remaining_squared,0.0f)),c.arrival_distance);
        log_line(message);return 0;
    }
    case 15:return c.job->last_distance_squared<0.0f;
    case 16:c.job->last_distance_squared=c.remaining_squared;return 0;
    case 17:return c.remaining_squared<=c.arrival_distance*c.arrival_distance;
    case 18:execute_docking_stop(c.source);return 0;
    case 19:c.job->phase=RefitPhase::waiting_for_queue;return 0;
    case 20:c.job->retry_ticks=kRetryIntervalTicks;return 0;
    case 21:return g_logged_waiting;
    case 22:g_logged_waiting=true;log_line("Refit ship arrived and is waiting for a native Producer queue slot");return 0;
    case 23:return ++c.job->move_ticks>=kMoveRefreshTicks;
    case 24:c.job->move_ticks=0;return 0;
    case 25:return g_logged_travel_progress;
    case 26:{
        g_logged_travel_progress=true;char message[192]{};
        const float previous=std::sqrt(std::max(c.job->last_distance_squared,0.0f));
        const float current=std::sqrt(std::max(c.remaining_squared,0.0f));
        std::snprintf(message,sizeof(message),
            "Refit travel checkpoint: %.1f units remain (%.1f units moved toward yard)",
            current,previous-current);log_line(message);return 0;
    }
    case 27:execute_move_to_refit_approach(c.source,c.approach_transform);return 0;
    case 28:return ++c.job->retry_ticks<kRetryIntervalTicks;
    case 29:c.job->retry_ticks=0;return 0;
    case 30:return g_push_refit?g_push_refit(c.yard,c.job->target_class):0;
    case 31:c.job->queue_id=static_cast<std::uint32_t>(a);return 0;
    case 32:c.job->phase=RefitPhase::queued;return 0;
    case 33:return g_logged_queued;
    case 34:g_logged_queued=true;log_line("Refit entered the yard's native production queue");return 0;
    case 35:return read_at<std::uint32_t>(c.yard,kCurrentQueueIdOffset,0);
    case 36:return reinterpret_cast<std::uintptr_t>(read_at<void*>(c.yard,kCurrentBuildClassOffset,nullptr));
    case 37:return c.job->queue_id;
    case 38:return reinterpret_cast<std::uintptr_t>(c.job->target_class);
    case 39:c.job->phase=RefitPhase::active;return 0;
    case 40:return suppress_source_avoidance(c.source,c.job);
    case 41:c.job->docking_origin=c.source_transform;return 0;
    case 42:c.job->docking_elapsed_seconds=0.0f;return 0;
    case 43:{
        // This is the original GNU x87/store kernel, not a reassociated Rust sum.
        const float step=std::isfinite(c.elapsed)&&c.elapsed>0.0f?c.elapsed:0.0f;
        c.job->docking_elapsed_seconds=std::min(kDockingTransitionSeconds,
            c.job->docking_elapsed_seconds+step);return 0;
    }
    case 44:
        c.docking_transform=c.refit_transform;
        if(c.job->docking_elapsed_seconds<kDockingTransitionSeconds) {
            const float progress=c.job->docking_elapsed_seconds/kDockingTransitionSeconds;
            c.docking_transform=a2fo::refit::interpolate_docking_transform(
                c.job->docking_origin,c.refit_transform,progress);
        }
        return 0;
    case 45:a2fo_refit_call_thiscall_1(at(g_armada,kGameObjectSetTransformRva),c.source,
        reinterpret_cast<std::uintptr_t>(&c.docking_transform));return 0;
    case 46:return g_logged_active;
    case 47:g_logged_active=true;
        log_line("Refit construction active; source collision avoidance suspended for a five-second smooth dock into the yard build hardpoint, then held while progress fills");return 0;
    case 48:return g_logged_ejecting;
    case 49:g_logged_ejecting=true;
        log_line("Cancelled refit handed to the yard's native completed-ship output queue; source collision avoidance restored");return 0;
    default:return 0;
    }
}
void update_refit_job(void* source,RefitJob* job,float elapsed_seconds) noexcept {
    RefitUpdateContext context{source,job,elapsed_seconds};
    a2fo_rs_stage8_refit_update(&refit_update_step,&context);
}

struct RefitCancelContext {
    void* source;std::uint32_t source_handle=0,yard_handle=0,queue_id=0;
    std::unordered_map<std::uint32_t,RefitJob>::iterator found{};
    void* yard=nullptr;
};
std::uint64_t refit_cancel_step(void* opaque,std::uint32_t op,
                               std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitCancelContext*>(opaque);
    switch(op) {
    case 0:c.found=g_jobs.find(c.source_handle);return c.found!=g_jobs.end();
    case 1:c.yard_handle=c.found->second.yard_handle;c.queue_id=c.found->second.queue_id;return 0;
    case 2:restore_source_avoidance(c.source,&c.found->second);return 0;
    case 3:g_jobs.erase(c.found);return 0;
    case 4:return c.queue_id;
    case 5:c.yard=find_entity(c.yard_handle);return c.yard!=nullptr;
    case 6:a2fo_refit_call_thiscall_1(at(g_fleet_ops,kFoProducerActDeleteRva),c.yard,c.queue_id);return 0;
    case 7:return begin_source_ejection(&c.found->second);
    default:return 0;
    }
}
bool cancel_source_job(void* source,bool allow_ejection=true) noexcept {
    RefitCancelContext context{source};context.source_handle=object_handle(source);
    return a2fo_rs_stage8_refit_cancel(allow_ejection,&refit_cancel_step,&context)!=0;
}

std::uint64_t refit_cancel_synchronized_step(void* source,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    switch(op){
    case 0:return g_runtime_ready;
    case 1:return source!=nullptr;
    case 2:return object_expired(source);
    case 3:return cancel_source_job(source);
    case 4:log_line("Synchronized refit cancelled by Halt");return 0;
    default:return 0;
    }
}
bool cancel_synchronized_refit(void* source) noexcept {return a2fo_rs_stage8_refit_flow(7,&refit_cancel_synchronized_step,source)!=0;}

struct RefitDequeueContext {void* source;std::int32_t command;const Vector3* destination;std::int32_t value;};
std::uint64_t refit_dequeue_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitDequeueContext*>(opaque);
    switch(op){
    case 0:return g_runtime_ready;
    case 1:return static_cast<std::uint64_t>(static_cast<std::int64_t>(c.command));
    case 2:return cancel_source_job(c.source);
    case 3:log_line("Synchronized refit cancelled by native Halt command");return 0;
    case 4:a2fo_refit_call_thiscall_command_vector_int(g_dequeue_command_vector_hook.gateway,c.source,c.command,c.destination,c.value);return 0;
    default:return 0;
    }
}
void __attribute__((fastcall)) dequeue_command_vector_hook(void* source,void*,std::int32_t command,const Vector3* destination,std::int32_t value) noexcept {
    RefitDequeueContext c{source,command,destination,value};a2fo_rs_stage8_refit_flow(8,&refit_dequeue_step,&c);
}

struct RefitEventContext {const A2FO_CraftEvent* event;void* craft=nullptr;RefitJob* job=nullptr;};
std::uint64_t refit_event_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitEventContext*>(opaque);
    switch(op){
    case 0:return c.event&&c.event->struct_size>=sizeof(*c.event);
    case 1:return g_runtime_ready;
    case 2:return c.event->craft!=nullptr;
    case 3:c.craft=c.event->craft;return 0;
    case 4:return c.event->kind;
    case 5:return cancel_source_job(c.craft,false);
    case 6:return g_yard_classes.find(object_class(c.craft))!=g_yard_classes.end();
    case 7:erase_jobs_for_yard(object_handle(c.craft));return 0;
    case 8:return object_expired(c.craft);
    case 9:{const auto found=g_jobs.find(object_handle(c.craft));c.job=found==g_jobs.end()?nullptr:&found->second;return c.job!=nullptr;}
    case 10:update_refit_job(c.craft,c.job,c.event->elapsed_seconds);return 0;
    default:return 0;
    }
}
void A2FO_CALL craft_event_handler(const A2FO_CraftEvent* event,void*) {RefitEventContext c{event};a2fo_rs_stage8_refit_flow(6,&refit_event_step,&c);}

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

struct RefitBridgesContext {
    HMODULE feature=nullptr,hybrid=nullptr;A2FO_RegisterRefitQueueBridgeFn queue=nullptr;
    A2FO_RegisterRefitUiBridgeFn ui=nullptr;bool(A2FO_CALL* ready)()noexcept=nullptr;
    A2FO_RefitQueueBridge queue_bridge{};A2FO_RefitUiBridge ui_bridge{};
};
std::uint64_t refit_bridges_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<RefitBridgesContext*>(opaque);
    switch(op){
    case 0:c.feature=a2fo::startup_module_provider("A2FOFeaturePack.dll",{"A2FO_RegisterRefitQueueBridge","A2FO_ProducerPushRefit","A2FO_ProducerEventsReady"});return 0;
    case 1:c.hybrid=a2fo::startup_module_provider("A2FOHybridBuild.dll",{"A2FO_RegisterRefitUiBridge"});return 0;
    case 2:return import_export(c.feature,"A2FO_RegisterRefitQueueBridge",&c.queue);
    case 3:return import_export(c.feature,"A2FO_ProducerPushRefit",&g_push_refit);
    case 4:return import_export(c.hybrid,"A2FO_RegisterRefitUiBridge",&c.ui);
    case 5:return import_export(c.feature,"A2FO_ProducerEventsReady",&c.ready);
    case 6:return c.ready();
    case 7:
        c.queue_bridge.struct_size=sizeof(c.queue_bridge);c.queue_bridge.version=A2FO_REFIT_QUEUE_BRIDGE_VERSION;
        c.queue_bridge.consume_synchronized_command=&consume_synchronized_refit;
        c.queue_bridge.cancel_synchronized_command=&cancel_synchronized_refit;
        c.queue_bridge.has_waiting_job=&has_waiting_refit;c.queue_bridge.job_finished=&job_finished;c.queue_bridge.job_removed=&job_removed;
        c.ui_bridge.struct_size=sizeof(c.ui_bridge);c.ui_bridge.version=A2FO_REFIT_UI_BRIDGE_VERSION;
        c.ui_bridge.enumerate_items=&enumerate_refit_items;c.ui_bridge.request_refit=&request_refit;c.ui_bridge.cancel_refit=&request_refit_cancel;
        return c.queue(&c.queue_bridge);
    case 8:return c.ui(&c.ui_bridge);
    case 9:log_line("FeaturePack/HybridBuild refit bridge exports unavailable");return 0;
    case 10:log_line("FeaturePack Producer event dispatch unavailable");return 0;
    default:return 0;
    }
}
bool register_bridges() noexcept {RefitBridgesContext c;return a2fo_rs_stage8_refit_flow(14,&refit_bridges_step,&c)!=0;}

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
    g_api=api;
    struct Context {const A2FO_ModuleApi* api;std::exception_ptr error{};} c{api};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& state=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:g_armada=static_cast<HMODULE>(state.api->armada_module());return 0;
        case 1:g_fleet_ops=static_cast<HMODULE>(state.api->fleetops_module());return 0;
        case 2:return g_armada&&g_fleet_ops;
        case 3:return preflight_signatures();
        case 4:return state.api->install_inline_hook(at(g_armada,kDequeueCommandVectorRva),reinterpret_cast<void*>(&dequeue_command_vector_hook),sizeof(kExpectedDequeueCommandVector),kExpectedDequeueCommandVector,&g_dequeue_command_vector_hook);
        case 5:try{g_source_classes.reserve(128);g_yard_classes.reserve(32);g_jobs.reserve(64);return 1;}catch(...){return 0;}
        case 6:return state.api->register_game_object_class_loaded_handler(kModuleName,kRequiredFields.data(),static_cast<std::uint32_t>(kRequiredFields.size()),&class_loaded_handler,nullptr);
        case 7:return state.api->register_craft_event_handler_masked(kModuleName,A2FO_CRAFT_EVENT_MASK_SIMULATE_POST|A2FO_CRAFT_EVENT_MASK_CLEANUP,&craft_event_handler,nullptr);
        case 8:return register_bridges();
        case 9:g_runtime_ready=true;return 0;
        case 10:log_line("Refit yards initialized with synchronized requests, nearest-yard travel, native queue progress, and replacement handoff");return 0;
        case 11:log_line("Native refit dependency signature mismatch; disabled");return 0;
        case 12:log_line("Native synchronized movement receiver hook failed; disabled");return 0;
        case 13:log_line("Refit registry allocation failed");return 0;
        case 14:log_line("Shared class/Craft event registration failed");return 0;
        case 15:log_line("Refit bridge registration failed; module kept resident with gameplay disabled");return 0;
        default:return 0;
        }}catch(...){state.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_refit_flow(10,step,&c)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    struct Context {std::unordered_map<std::uint32_t,RefitJob>::iterator iterator{};} c;
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& state=*static_cast<Context*>(opaque);
        switch(op){
        case 0:g_runtime_ready=false;return 0;
        case 1:state.iterator=g_jobs.begin();return 0;
        case 2:return state.iterator!=g_jobs.end();
        case 3:restore_job_source(&state.iterator->second);return 0;
        case 4:++state.iterator;return 0;
        case 5:g_jobs.clear();return 0;
        default:return 0;
        }
    };
    a2fo_rs_stage8_refit_flow(5,step,&c);
}
