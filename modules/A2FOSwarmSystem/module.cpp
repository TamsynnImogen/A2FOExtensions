/*
 * Dynamic, render-only ambient swarms for Fleet Operations.
 *
 * Every visual member is a native ST3D_Instance with sidecar movement state.
 * Swarms never enter the GameObject list and therefore have no AI, physics,
 * collision, selection, weapons, commands, team registration, or save record.
 */

#include "../../sdk/include/a2fo_module_api.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "swarm_motion.hpp"
#include "../../rust/stage8_a_swarm_ffi.h"
#include "../../rust/stage10_visibility_swarm_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_swarm_call_thiscall_0(void* function, void* self);
std::uintptr_t __cdecl a2fo_swarm_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_swarm_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_swarm_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_swarm_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
}

namespace {

using a2fo::swarm::Matrix34;
using a2fo::swarm::Random;
using a2fo::swarm::Vec3;

constexpr char kModuleName[] = "A2FOSwarmSystem";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs, derived from armada2.map.
constexpr std::uintptr_t kBeeColonySimulateRva = 0x00067db0;
constexpr std::uintptr_t kBeeColonyRenderRva = 0x000688a0;
constexpr std::uintptr_t kBeeColonyShouldDrawBeeRva = 0x00068d50;
constexpr std::uintptr_t kWorkerBeeColonyRva = 0x00336e48;
constexpr std::uintptr_t kGameObjectListPointerRva = 0x00361084;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetBoundingSphereRva = 0x000cfd70;
constexpr std::uintptr_t kEntityGetWorldTransformRva = 0x000cff90;
constexpr std::uintptr_t kGameObjectClassGetHierarchyRootRva = 0x000cd940;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uintptr_t kSt3dNodeFindRecursiveRva = 0x00238780;
constexpr std::uintptr_t kParameterDbConstructorRva = 0x00134160;
constexpr std::uintptr_t kParameterDbDestructorRva = 0x001341d0;
constexpr std::uintptr_t kParameterDbGetIntRva = 0x00134bf0;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00134df0;
constexpr std::uintptr_t kParameterDbGetBoolRva = 0x00134f50;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetStringVectorRva = 0x00135e80;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;
constexpr std::uintptr_t kSt3dInstanceConstructorRva = 0x0022e120;
constexpr std::uintptr_t kSt3dInstanceDestructorRva = 0x0022e1f0;
constexpr std::uintptr_t kSt3dInstanceSetAnimationFlagsRva = 0x0022e290;
constexpr std::uintptr_t kSt3dInstanceScaleGeometryRva = 0x0022e2c0;
constexpr std::uintptr_t kSt3dInstanceSetTransformRva = 0x0022e330;
constexpr std::uintptr_t kSt3dInstanceRenderRva = 0x0022e750;
constexpr std::uintptr_t kSt3dInstanceTriggerAnimationRva = 0x0022ea90;
constexpr std::uintptr_t kSt3dInstanceSetDatabaseRva = 0x0022eed0;

constexpr std::size_t kObjectExpiredOffset = 0x27;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kParameterDbSize = 0x38;
constexpr std::size_t kSt3dInstanceSize = 0x84;
constexpr std::size_t kSt3dBoundingSphereOffset = 0x34;
constexpr std::size_t kSt3dLogicalDatabaseOffset = 0x7c;
constexpr std::size_t kSt3dVisibleDatabaseOffset = 0x80;
constexpr std::size_t kMaximumDefinitions = 64;
constexpr std::size_t kMaximumMembersPerDefinition = 256;
constexpr std::size_t kMaximumMembersPerHost = 1024;
constexpr std::size_t kMaximumHardpoints = 128;
constexpr std::size_t kMaximumObjects = 1000000;
constexpr std::uint32_t kHostDiscoveryIntervalTicks = 256;
constexpr std::uint32_t kHostDiscoveryMinimumTicks = 16;

constexpr std::uint8_t kExpectedBeeColonySimulate[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x08, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedBeeColonyRender[] = {
    0x55, 0x8b, 0xec, 0x51, 0x8b, 0x01, 0x53, 0x56};
constexpr std::uint8_t kExpectedBeeColonyShouldDrawBee[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedParameterDbConstructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedParameterDbDestructor[] = {
    0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x77, 0x24};
constexpr std::uint8_t kExpectedParameterDbGetScalar[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetString[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetStringVector[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedSt3dInstanceConstructor[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
constexpr std::uint8_t kExpectedSt3dInstanceDestructor[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57};
constexpr std::uint8_t kExpectedSt3dInstanceSetAnimationFlags[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedSt3dInstanceScaleGeometry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c};
constexpr std::uint8_t kExpectedSt3dInstanceSetTransform[] = {
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08};
constexpr std::uint8_t kExpectedSt3dInstanceSetDatabase[] = {
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::uint8_t kExpectedSt3dInstanceRender[] = {
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0x57};
constexpr std::uint8_t kExpectedSt3dInstanceTriggerAnimation[] = {
    0x56, 0x8b, 0xf1, 0x8b, 0x0d};
constexpr std::uint8_t kExpectedEntityGetTransform[] = {
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
constexpr std::uint8_t kExpectedEntityGetBoundingSphere[] = {
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x34, 0xc3};
constexpr std::uint8_t kExpectedEntityGetWorldTransform[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c};
constexpr std::uint8_t kExpectedClassGetHierarchyRoot[] = {
    0x8b, 0x81, 0xd8, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00, 0xe9};
constexpr std::uint8_t kExpectedNodeFindRecursive[] = {
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::uint8_t kExpectedEngineOperatorDelete[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};

struct NativeStringVector {
    std::uint32_t allocator = 0;
    char** begin = nullptr;
    char** end = nullptr;
    char** capacity = nullptr;
};
static_assert(sizeof(NativeStringVector) == 16,
              "Armada string-vector ABI must occupy sixteen bytes");

struct Hardpoint {
    std::string name;
    void* node = nullptr;
};

struct BoundingSphere {
    Vec3 centre{};
    float radius = 0.0f;
};
static_assert(sizeof(BoundingSphere) == 16,
              "Armada bounding-sphere ABI must occupy sixteen bytes");

struct SwarmDefinition {
    std::uint32_t index = 0;
    std::string model;
    std::uint32_t count = 1;
    float scale = 1.0f;
    float minimum_radius = 0.0f;
    float maximum_radius = 50.0f;
    float minimum_speed = 6.0f;
    float maximum_speed = 10.0f;
    float turn_rate = 2.0f;
    float interaction_chance = 0.35f;
    float interaction_time = 3.0f;
    float interaction_radius = 2.0f;
    float host_clearance = 0.5f;
    float member_separation = 1.0f;
    std::uint32_t hardpoint_capacity = 1;
    std::uint32_t interaction_capacity = 1;
    bool return_to_hardpoint = true;
    bool avoid_host = true;
    std::vector<Hardpoint> launch_hardpoints;
    std::vector<Hardpoint> interaction_hardpoints;
};

struct ClassPolicy {
    std::string odf_name;
    std::vector<SwarmDefinition> definitions;
};

enum class AgentPhase {
    roaming,
    approaching_interaction,
    dwelling_interaction,
    returning,
    dwelling_return,
};

class NativeInstance {
public:
    NativeInstance() = default;
    NativeInstance(const NativeInstance&) = delete;
    NativeInstance& operator=(const NativeInstance&) = delete;

    ~NativeInstance() { release(); }

    bool initialize(const std::string& model, float scale) noexcept;
    float bounding_radius() const noexcept;
    void set_transform(const Matrix34& transform) noexcept;
    bool render(void* camera) noexcept;
    void release() noexcept;

private:
    alignas(4) std::array<std::uint8_t, kSt3dInstanceSize> bytes_{};
    bool constructed_ = false;
};

struct Agent {
    explicit Agent(std::uint32_t seed) : random(seed), identity(seed) {}

    // Keep the native render instance in the Agent allocation. The old
    // pointer form required a second heap allocation for every swarm member.
    NativeInstance visual;
    Random random;
    std::uint32_t identity = 0;
    Vec3 position{};
    Vec3 direction{0.0f, 0.0f, 1.0f};
    Vec3 target{};
    Vec3 target_offset{};
    float speed = 8.0f;
    float dwell_remaining = 0.0f;
    float visual_radius = 0.0f;
    std::size_t target_hardpoint = 0;
    AgentPhase phase = AgentPhase::roaming;
};

struct RuntimeGroup {
    std::size_t definition = 0;
    std::vector<std::unique_ptr<Agent>> agents;
};

struct HostRuntime {
    void* object = nullptr;
    void* object_class = nullptr;
    std::uint32_t handle = 0;
    std::uint32_t last_seen_epoch = 0;
    BoundingSphere bounds{};
    std::vector<RuntimeGroup> groups;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
bool g_runtime_ready = false;
bool g_inside_simulation = false;
std::uint32_t g_epoch = 0;
std::uint32_t g_host_discovery_countdown = 0;
std::uint32_t g_last_object_count =
    std::numeric_limits<std::uint32_t>::max();
void* g_cached_object_list = nullptr;
void* g_cached_object_list_head = nullptr;
A2FO_InlineHook g_bee_simulate_hook{};
A2FO_InlineHook g_bee_render_hook{};
std::unordered_map<void*, ClassPolicy> g_class_policies;
#ifdef A2FO_BUILTIN_FEATURE
// Remaining native instances must not call into an engine already tearing
// down from the core's CRT destructors. Release them at normal simulation
// boundaries; let the OS reclaim any retained process-lifetime store at exit.
std::unordered_map<std::uint32_t, HostRuntime>& g_hosts =
    *new std::unordered_map<std::uint32_t, HostRuntime>;
#else
std::unordered_map<std::uint32_t, HostRuntime> g_hosts;
#endif
LONG g_instance_failure_reports = 0;
bool g_logged_simulation_boundary = false;
bool g_logged_render_boundary = false;
bool g_logged_visibility_rejection = false;
bool g_logged_first_instance_render = false;
bool g_logged_frustum_rejection = false;
bool g_logged_avoidance_volume = false;
bool g_logged_missing_host_bounds = false;

#ifdef A2FO_BUILTIN_FEATURE
std::atomic<std::uint64_t> g_swarm_activation{0};
std::uintptr_t swarm_activation_step(void*, std::uint32_t op,
                                     std::uintptr_t value) noexcept {
    try {
        switch (op) {
        case 0:
            *reinterpret_cast<std::uint64_t*>(value) =
                a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE);
            return 1;
        case 1:
            *reinterpret_cast<std::uint64_t*>(value) =
                g_swarm_activation.load(std::memory_order_acquire);
            return 1;
        case 2: g_hosts.clear(); return 1;
        case 3: g_class_policies.clear(); return 1;
        case 4: g_epoch = 0; return 1;
        case 5: g_host_discovery_countdown = 0; return 1;
        case 6:
            g_last_object_count = std::numeric_limits<std::uint32_t>::max();
            return 1;
        case 7: g_cached_object_list = nullptr; return 1;
        case 8: g_cached_object_list_head = nullptr; return 1;
        case 9:
            g_swarm_activation.store(
                *reinterpret_cast<const std::uint64_t*>(value),
                std::memory_order_release);
            return 1;
        default: return 0;
        }
    } catch (...) {
        // The original boundary is noexcept; preserve the active exception
        // at termination and never unwind across Rust.
        std::terminate();
    }
}
bool sync_runtime_activation() noexcept {
    return a2fo_rs_stage8_swarm_activation(
        &swarm_activation_step, nullptr, 1) != 0;
}
bool runtime_render_enabled() noexcept {
    return a2fo_rs_stage8_swarm_activation(
        &swarm_activation_step, nullptr, 0) != 0;
}
#else
bool sync_runtime_activation() noexcept { return g_runtime_ready; }
bool runtime_render_enabled() noexcept { return g_runtime_ready; }
#endif

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
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
    const auto base = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    return start >= base &&
        size <= information.RegionSize - (start - base);
}

template <typename T>
T read_at(const void* object, std::size_t offset, T fallback = T{}) noexcept {
    const auto* address = object
        ? static_cast<const std::uint8_t*>(object) + offset : nullptr;
    if (!readable_range(address, sizeof(T))) return fallback;
    T result{};
    std::memcpy(&result, address, sizeof(result));
    return result;
}

template <typename T>
T read_live_at(const void* object, std::size_t offset,
               T fallback = T{}) noexcept {
    if (!object) return fallback;
    T result{};
    std::memcpy(&result,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(result));
    return result;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(g_armada, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

template <std::size_t Size>
bool checked_binding(const char* name, std::uintptr_t rva,
                     const std::uint8_t (&expected)[Size],
                     bool allow_core_jump = false) noexcept {
    if (signature_matches(rva, expected)) return true;
    const auto* live = static_cast<const std::uint8_t*>(at(g_armada, rva));
    if (allow_core_jump && readable_range(live, 5) && live[0] == 0xe9) {
        return true;
    }
    char message[192]{};
    std::snprintf(message, sizeof(message),
                  "Native binding mismatch: %s at RVA 0x%08lx",
                  name ? name : "<unnamed>",
                  static_cast<unsigned long>(rva));
    log_line(message);
    return false;
}

bool checked_readable_binding(const char* name, std::uintptr_t rva,
                              std::size_t size) noexcept {
    if (readable_range(at(g_armada, rva), size)) return true;
    char message[192]{};
    std::snprintf(message, sizeof(message),
                  "Native binding unreadable: %s at RVA 0x%08lx",
                  name ? name : "<unnamed>",
                  static_cast<unsigned long>(rva));
    log_line(message);
    return false;
}

std::uint32_t float_bits(float value) noexcept {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float ABI mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
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

std::string class_odf_name(void* object_class_pointer) {
    if (!object_class_pointer) return {};
    const char* value = reinterpret_cast<const char*>(
        a2fo_swarm_call_thiscall_0(
            at(g_armada, kGameObjectClassGetOdfNameRva),
            object_class_pointer));
    if (!value || IsBadStringPtrA(value, 1024)) return {};
    return value;
}

void trim(std::string* value) {
    if (!value) return;
    std::size_t first = 0;
    std::size_t last = value->size();
    const auto classify = +[](void*, std::uint32_t,
                              std::uintptr_t byte) noexcept -> std::uintptr_t {
        return std::isspace(static_cast<unsigned char>(byte)) != 0;
    };
    a2fo_rs_stage8_swarm_trim(
        reinterpret_cast<const std::uint8_t*>(value->data()),
        value->size(), classify, nullptr, &first, &last);
    *value = value->substr(first, last - first);
}

class ParameterDbView {
public:
    explicit ParameterDbView(const std::string& odf_name) noexcept {
        if (odf_name.empty()) return;
        a2fo_swarm_call_thiscall_1(
            at(g_armada, kParameterDbConstructorRva), bytes_.data(),
            reinterpret_cast<std::uintptr_t>(odf_name.c_str()));
        constructed_ = true;
    }

    ~ParameterDbView() {
        if (constructed_) {
            a2fo_swarm_call_thiscall_0(
                at(g_armada, kParameterDbDestructorRva), bytes_.data());
        }
    }

    void* get() noexcept { return constructed_ ? bytes_.data() : nullptr; }

private:
    alignas(4) std::array<std::uint8_t, kParameterDbSize> bytes_{};
    bool constructed_ = false;
};

bool read_parameter_string(void* database, const char* key,
                           std::string* output) noexcept {
    if (!database || !key || !*key || !output) return false;
    std::array<char, 512> value{};
    const std::uintptr_t found = a2fo_swarm_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), database,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(value.data()), value.size(),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if ((found & 0xffu) == 0 || value[0] == '\0') return false;
    try {
        *output = value.data();
        trim(output);
        return !output->empty();
    } catch (...) {
        return false;
    }
}

bool valid_vector_bounds(const NativeStringVector& values,
                         std::size_t* count) noexcept {
    if (count) *count = 0;
    if (!values.begin && !values.end) return true;
    if (!values.begin || !values.end) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(values.begin);
    const auto end = reinterpret_cast<std::uintptr_t>(values.end);
    if (end < begin || (end - begin) % sizeof(char*) != 0) return false;
    const std::size_t entries = (end - begin) / sizeof(char*);
    if (entries > kMaximumHardpoints ||
        (entries != 0 && !readable_range(
            values.begin, entries * sizeof(char*)))) return false;
    if (count) *count = entries;
    return true;
}

void release_native_vector(NativeStringVector* values) noexcept {
    if (!values) return;
    std::size_t count = 0;
    if (!valid_vector_bounds(*values, &count)) return;
    using DeleteFn = void (__cdecl*)(void*);
    const auto engine_delete = reinterpret_cast<DeleteFn>(
        at(g_armada, kEngineOperatorDeleteRva));
    if (!engine_delete) return;
    for (std::size_t index = 0; index < count; ++index) {
        if (values->begin[index]) engine_delete(values->begin[index]);
    }
    if (values->begin) engine_delete(values->begin);
    *values = NativeStringVector{};
}

bool read_parameter_list(void* database, const char* key,
                         std::vector<std::string>* output) noexcept {
    if (!database || !key || !output) return false;
    NativeStringVector native{};
    const std::uintptr_t found = a2fo_swarm_call_thiscall_3(
        at(g_armada, kParameterDbGetStringVectorRva), database,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(&native), 0);
    bool copied = false;
    try {
        std::size_t count = 0;
        if (valid_vector_bounds(native, &count)) {
            output->clear();
            output->reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                const char* value = native.begin[index];
                if (!value || IsBadStringPtrA(value, 1024)) continue;
                std::string item(value);
                trim(&item);
                if (!item.empty()) output->push_back(std::move(item));
            }
            copied = !output->empty();
        }
    } catch (...) {
        output->clear();
    }
    release_native_vector(&native);
    return (found & 0xffu) != 0 && copied;
}

bool read_float(void* database, const std::string& key,
                float* output) noexcept {
    if (!database || key.empty() || !output) return false;
    float value = *output;
    const std::uintptr_t found = a2fo_swarm_call_thiscall_3(
        at(g_armada, kParameterDbGetFloatRva), database,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(&value), float_bits(value));
    if ((found & 0xffu) == 0 || !std::isfinite(value)) return false;
    *output = value;
    return true;
}

bool read_count(void* database, const std::string& key,
                std::uint32_t* output) noexcept {
    if (!database || key.empty() || !output) return false;
    int value = a2fo_rs_stage8_swarm_count_default(*output);
    const std::uintptr_t found = a2fo_swarm_call_thiscall_3(
        at(g_armada, kParameterDbGetIntRva), database,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(&value),
        static_cast<std::uintptr_t>(value));
    if ((found & 0xffu) == 0 || value < 0) return false;
    *output = a2fo_rs_stage8_swarm_count_value(value);
    return true;
}

bool read_bool(void* database, const std::string& key,
               bool* output) noexcept {
    if (!database || key.empty() || !output) return false;
    std::uint8_t value = *output ? 1 : 0;
    const std::uintptr_t found = a2fo_swarm_call_thiscall_3(
        at(g_armada, kParameterDbGetBoolRva), database,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(&value), value ? 1u : 0u);
    if ((found & 0xffu) == 0) return false;
    *output = value != 0;
    return true;
}

std::string indexed_key(std::size_t index, const char* suffix = "") {
    char key[64]{};
    std::snprintf(key, sizeof(key), "swarm%lu%s",
                  static_cast<unsigned long>(index), suffix ? suffix : "");
    return key;
}

void normalize_definition(SwarmDefinition* definition) noexcept {
    if (!definition) return;
    A2FO_Stage8SwarmScalars scalars{
        definition->count, definition->scale,
        definition->minimum_radius, definition->maximum_radius,
        definition->minimum_speed, definition->maximum_speed,
        definition->turn_rate, definition->interaction_chance,
        definition->interaction_time, definition->interaction_radius,
        definition->host_clearance, definition->member_separation,
        definition->hardpoint_capacity, definition->interaction_capacity};
    a2fo_rs_stage8_swarm_normalize(&scalars);
    definition->count = scalars.count;
    definition->scale = scalars.scale;
    definition->minimum_radius = scalars.minimum_radius;
    definition->maximum_radius = scalars.maximum_radius;
    definition->minimum_speed = scalars.minimum_speed;
    definition->maximum_speed = scalars.maximum_speed;
    definition->turn_rate = scalars.turn_rate;
    definition->interaction_chance = scalars.interaction_chance;
    definition->interaction_time = scalars.interaction_time;
    definition->interaction_radius = scalars.interaction_radius;
    definition->host_clearance = scalars.host_clearance;
    definition->member_separation = scalars.member_separation;
}

struct SwarmParseContext {
    explicit SwarmParseContext(void* value) : object_class(value) {}
    void* object_class = nullptr;
    ClassPolicy policy{};
    std::optional<ParameterDbView> database;
    SwarmDefinition definition{};
    std::vector<std::string> launch_names;
    std::vector<std::string> interaction_names;
    void* root = nullptr;
    void* node = nullptr;
    std::exception_ptr exception{};
};

std::vector<std::string>& names_for(SwarmParseContext& context,
                                     std::size_t kind) noexcept {
    return kind == 0 ? context.launch_names : context.interaction_names;
}

std::vector<Hardpoint>& hardpoints_for(SwarmParseContext& context,
                                        std::size_t kind) noexcept {
    return kind == 0 ? context.definition.launch_hardpoints
                     : context.definition.interaction_hardpoints;
}

std::uintptr_t swarm_parse_step(void* opaque, std::uint32_t op,
                                std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmParseContext*>(opaque);
    if (context.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
        const auto key = [&context](const char* suffix) {
            return indexed_key(context.definition.index, suffix);
        };
        void* database = context.database ? context.database->get() : nullptr;
        switch (op) {
        case 0:
            context.policy.odf_name = class_odf_name(context.object_class);
            return !context.policy.odf_name.empty();
        case 1:
            context.database.emplace(context.policy.odf_name);
            return context.database->get() != nullptr;
        case 2:
            context.definition = SwarmDefinition{};
            context.definition.index = static_cast<std::uint32_t>(arg);
            return read_parameter_string(database, key("").c_str(),
                                         &context.definition.model);
        case 3: return read_count(database, key("Count"), &context.definition.count);
        case 4: return read_float(database, key("Scale"), &context.definition.scale);
        case 5: return read_float(database, key("Radius"), &context.definition.maximum_radius);
        case 6: return read_float(database, key("MinRadius"), &context.definition.minimum_radius);
        case 7: return read_float(database, key("MaxRadius"), &context.definition.maximum_radius);
        case 8: return read_float(database, key("MinSpeed"), &context.definition.minimum_speed);
        case 9: return read_float(database, key("MaxSpeed"), &context.definition.maximum_speed);
        case 10: return read_float(database, key("TurnRate"), &context.definition.turn_rate);
        case 11: return read_float(database, key("InteractionChance"), &context.definition.interaction_chance);
        case 12: return read_float(database, key("InteractionTime"), &context.definition.interaction_time);
        case 13: return read_float(database, key("InteractionRadius"), &context.definition.interaction_radius);
        case 14: return read_float(database, key("HostClearance"), &context.definition.host_clearance);
        case 15: return read_float(database, key("Separation"), &context.definition.member_separation);
        case 16: return read_count(database, key("HardpointCapacity"), &context.definition.hardpoint_capacity);
        case 17: return read_count(database, key("InteractionCapacity"), &context.definition.interaction_capacity);
        case 18: return read_bool(database, key("ReturnToHardpoint"), &context.definition.return_to_hardpoint);
        case 19: return read_bool(database, key("AvoidHost"), &context.definition.avoid_host);
        case 20: normalize_definition(&context.definition); return 1;
        case 21: return context.definition.count;
        case 22: context.definition.count = static_cast<std::uint32_t>(arg); return 1;
        case 23:
            context.launch_names.clear();
            read_parameter_list(database, key("Hardpoint").c_str(), &context.launch_names);
            return 1;
        case 24:
            context.interaction_names.clear();
            read_parameter_list(database, key("Interaction").c_str(), &context.interaction_names);
            return 1;
        case 25: return names_for(context, arg).size();
        case 26: {
            void* hierarchy = read_at<void*>(context.object_class, 0x1d8, nullptr);
            return hierarchy && readable_range(
                static_cast<std::uint8_t*>(hierarchy) + 0x3c, sizeof(void*));
        }
        case 27: {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "Ignoring swarm%lu %s hardpoints on '%s': "
                "the host has no loaded SOD hierarchy",
                static_cast<unsigned long>(context.definition.index),
                arg == 0 ? "launch" : "interaction",
                context.policy.odf_name.c_str());
            log_line(message);
            return 1;
        }
        case 28:
            context.root = reinterpret_cast<void*>(a2fo_swarm_call_thiscall_0(
                at(g_armada, kGameObjectClassGetHierarchyRootRva),
                context.object_class));
            return 1;
        case 29: {
            const std::size_t kind = arg >> 16;
            const std::size_t index = arg & 0xffffu;
            const std::string& name = names_for(context, kind)[index];
            context.node = context.root ? reinterpret_cast<void*>(
                a2fo_swarm_call_thiscall_1(
                    at(g_armada, kSt3dNodeFindRecursiveRva), context.root,
                    reinterpret_cast<std::uintptr_t>(name.c_str()))) : nullptr;
            return context.node != nullptr;
        }
        case 30: {
            const std::size_t kind = arg >> 16;
            const std::size_t index = arg & 0xffffu;
            char message[384]{};
            std::snprintf(message, sizeof(message),
                "Ignoring missing swarm%lu %s hardpoint '%s' on '%s'",
                static_cast<unsigned long>(context.definition.index),
                kind == 0 ? "launch" : "interaction",
                names_for(context, kind)[index].c_str(),
                context.policy.odf_name.c_str());
            log_line(message);
            return 1;
        }
        case 31: {
            const std::size_t kind = arg >> 16;
            const std::size_t index = arg & 0xffffu;
            hardpoints_for(context, kind).push_back(Hardpoint{
                names_for(context, kind)[index], context.node});
            return 1;
        }
        case 32: {
            const auto& d = context.definition;
            char message[512]{};
            std::snprintf(message, sizeof(message),
                "Resolved swarm%lu on '%s': model='%s', count=%lu, "
                "scale=%.2f, radius=%.2f..%.2f, speed=%.2f..%.2f, "
                "launch=%lu, interaction=%lu, hostAvoidance=%s+%.2f, "
                "separation=visualBounds+%.2f, capacity=%lu/%lu",
                static_cast<unsigned long>(d.index),
                context.policy.odf_name.c_str(), d.model.c_str(),
                static_cast<unsigned long>(d.count), d.scale,
                d.minimum_radius, d.maximum_radius,
                d.minimum_speed, d.maximum_speed,
                static_cast<unsigned long>(d.launch_hardpoints.size()),
                static_cast<unsigned long>(d.interaction_hardpoints.size()),
                d.avoid_host ? "modelRadius" : "off", d.host_clearance,
                d.member_separation,
                static_cast<unsigned long>(d.hardpoint_capacity),
                static_cast<unsigned long>(d.interaction_capacity));
            log_line(message);
            return 1;
        }
        case 33:
            context.policy.definitions.push_back(std::move(context.definition));
            return 1;
        case 34: return context.policy.definitions.size();
        case 35: {
            char message[320]{};
            std::snprintf(message, sizeof(message),
                "Registered %lu swarm definition%s (%lu visual "
                "instance%s) on '%s'",
                static_cast<unsigned long>(context.policy.definitions.size()),
                context.policy.definitions.size() == 1 ? "" : "s",
                static_cast<unsigned long>(arg), arg == 1 ? "" : "s",
                context.policy.odf_name.c_str());
            log_line(message);
            return 1;
        }
        case 36: context.definition.maximum_radius = 50.0f; return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

ClassPolicy parse_class_policy(void* object_class_pointer) {
    SwarmParseContext context(object_class_pointer);
    a2fo_rs_stage8_swarm_parse(&swarm_parse_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    return std::move(context.policy);
}

const ClassPolicy* ensure_class_policy(void* object_class_pointer) noexcept {
    if (!object_class_pointer) return nullptr;
    const auto found = g_class_policies.find(object_class_pointer);
    if (found != g_class_policies.end()) return &found->second;
    try {
        auto inserted = g_class_policies.emplace(
            object_class_pointer, parse_class_policy(object_class_pointer));
        return &inserted.first->second;
    } catch (...) {
        log_line("Could not retain a swarm class policy");
        return nullptr;
    }
}

bool NativeInstance::initialize(const std::string& model,
                                float scale) noexcept {
    struct Context {
        NativeInstance* instance;
        const std::string* model;
        float scale;
    } context{this, &model, scale};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: return c.instance->constructed_;
            case 1: return c.model->empty();
            case 2:
                a2fo_swarm_call_thiscall_0(
                    at(g_armada, kSt3dInstanceConstructorRva),
                    c.instance->bytes_.data());
                return 1;
            case 3: c.instance->constructed_ = true; return 1;
            case 4:
                a2fo_swarm_call_thiscall_1(
                    at(g_armada, kSt3dInstanceSetDatabaseRva),
                    c.instance->bytes_.data(),
                    reinterpret_cast<std::uintptr_t>(c.model->c_str()));
                return 1;
            case 5:
                return read_at<void*>(c.instance->bytes_.data(),
                    kSt3dLogicalDatabaseOffset, nullptr) != nullptr;
            case 6:
                return read_at<void*>(c.instance->bytes_.data(),
                    kSt3dVisibleDatabaseOffset, nullptr) != nullptr;
            case 7: c.instance->release(); return 1;
            case 8: return std::fabs(c.scale - 1.0f) > 0.0001f;
            case 9:
                a2fo_swarm_call_thiscall_1(
                    at(g_armada, kSt3dInstanceScaleGeometryRva),
                    c.instance->bytes_.data(), float_bits(c.scale));
                return 1;
            case 10:
                a2fo_swarm_call_thiscall_1(
                    at(g_armada, kSt3dInstanceSetAnimationFlagsRva),
                    c.instance->bytes_.data(), 1);
                return 1;
            case 11:
                a2fo_swarm_call_thiscall_0(
                    at(g_armada, kSt3dInstanceTriggerAnimationRva),
                    c.instance->bytes_.data());
                return 1;
            default: return 0;
            }
        } catch (...) {
            std::terminate();
        }
    };
    return a2fo_rs_stage8_swarm_instance_init(step, &context) != 0;
}

float NativeInstance::bounding_radius() const noexcept {
    if (!constructed_) return 0.0f;
    const float radius = read_at<float>(
        bytes_.data(), kSt3dBoundingSphereOffset + sizeof(Vec3), 0.0f);
    return std::isfinite(radius) ? std::max(0.0f, radius) : 0.0f;
}

void NativeInstance::set_transform(const Matrix34& transform) noexcept {
    if (!constructed_) return;
    a2fo_swarm_call_thiscall_1(
        at(g_armada, kSt3dInstanceSetTransformRva), bytes_.data(),
        reinterpret_cast<std::uintptr_t>(&transform));
}

bool NativeInstance::render(void* camera) noexcept {
    if (!constructed_ || !camera) return false;
    return (a2fo_swarm_call_thiscall_1(
        at(g_armada, kSt3dInstanceRenderRva), bytes_.data(),
        reinterpret_cast<std::uintptr_t>(camera)) & 0xffu) != 0;
}

void NativeInstance::release() noexcept {
    if (!constructed_ || !g_armada) return;
    a2fo_swarm_call_thiscall_0(
        at(g_armada, kSt3dInstanceDestructorRva), bytes_.data());
    constructed_ = false;
    bytes_.fill(0);
}

bool host_transform(void* host, Matrix34* output) noexcept {
    if (!host || !output) return false;
    const auto* transform = reinterpret_cast<const Matrix34*>(
        a2fo_swarm_call_thiscall_0(
            at(g_armada, kEntityGetTransformRva), host));
    if (!readable_range(transform, sizeof(*transform))) return false;
    std::memcpy(output, transform, sizeof(*output));
    return true;
}

bool host_bounding_sphere(void* host, BoundingSphere* output) noexcept {
    if (!host || !output) return false;
    const auto* sphere = reinterpret_cast<const BoundingSphere*>(
        a2fo_swarm_call_thiscall_0(
            at(g_armada, kEntityGetBoundingSphereRva), host));
    if (!readable_range(sphere, sizeof(*sphere))) return false;
    std::memcpy(output, sphere, sizeof(*output));
    if (!std::isfinite(output->centre.x) ||
        !std::isfinite(output->centre.y) ||
        !std::isfinite(output->centre.z) ||
        !std::isfinite(output->radius) || output->radius <= 0.0f) {
        *output = {};
        return false;
    }
    return true;
}

bool hardpoint_local_position(void* host, const Hardpoint* hardpoint,
                              Vec3* output) noexcept {
    if (!host || !output) return false;
    if (!hardpoint || !hardpoint->node) {
        *output = {};
        return true;
    }
    Matrix34 host_world{};
    Matrix34 hardpoint_world{};
    if (!host_transform(host, &host_world)) return false;
    const std::uintptr_t result = a2fo_swarm_call_thiscall_2(
        at(g_armada, kEntityGetWorldTransformRva), host,
        reinterpret_cast<std::uintptr_t>(&hardpoint_world),
        reinterpret_cast<std::uintptr_t>(hardpoint->node));
    if (result != reinterpret_cast<std::uintptr_t>(&hardpoint_world)) {
        return false;
    }
    *output = a2fo::swarm::inverse_transform_point(
        host_world, {hardpoint_world.values[9], hardpoint_world.values[10],
                     hardpoint_world.values[11]});
    return true;
}

const Hardpoint* hardpoint_at(const std::vector<Hardpoint>& hardpoints,
                             std::size_t index) noexcept {
    std::size_t selected = 0;
    return a2fo_rs_stage10_swarm_hardpoint_index(
        hardpoints.size(), index, &selected) ? &hardpoints[selected] : nullptr;
}

float host_exclusion_radius(const BoundingSphere& bounds,
                            const Agent& agent,
                            const SwarmDefinition& definition) noexcept {
    if (!definition.avoid_host || bounds.radius <= 0.0f) return 0.0f;
    return bounds.radius + agent.visual_radius + definition.host_clearance;
}

struct SwarmRoamingContext {
    Agent* agent = nullptr;
    const SwarmDefinition* definition = nullptr;
    const BoundingSphere* bounds = nullptr;
};

std::uintptr_t swarm_roaming_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& context = *static_cast<SwarmRoamingContext*>(opaque);
    Agent& agent = *context.agent;
    const SwarmDefinition& definition = *context.definition;
    const BoundingSphere& bounds = *context.bounds;
    switch (op) {
    case 0: agent.phase = AgentPhase::roaming; return 1;
    case 1: {
        const float exclusion = host_exclusion_radius(
            bounds, agent, definition);
        agent.target = a2fo::swarm::add(
            exclusion > 0.0f ? bounds.centre : Vec3{},
            a2fo::swarm::random_shell_point(
                agent.random,
                exclusion + definition.minimum_radius,
                exclusion + definition.maximum_radius));
        return 1;
    }
    case 2:
        agent.speed = agent.random.range(
            definition.minimum_speed, definition.maximum_speed);
        return 1;
    case 3: agent.target_offset = {}; return 1;
    default: return 0;
    }
}

void choose_roaming_target(Agent* agent,
                           const SwarmDefinition& definition,
                           const BoundingSphere& bounds) noexcept {
    if (!agent) return;
    SwarmRoamingContext context{agent, &definition, &bounds};
    a2fo_rs_stage8_swarm_choose_roaming(&swarm_roaming_step, &context);
}

struct SwarmHardpointChoiceContext {
    Agent* agent = nullptr;
    const RuntimeGroup* group = nullptr;
    std::size_t* selected = nullptr;
};

std::uintptr_t swarm_hardpoint_choice_step(void* opaque, std::uint32_t op,
                                           std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmHardpointChoiceContext*>(opaque);
    switch (op) {
    case 0:
        *reinterpret_cast<std::uint32_t*>(arg) = context.agent->random.next_u32();
        return 1;
    case 1: return context.group->agents.size();
    case 2: {
        const Agent* other = context.group->agents[arg].get();
        return other && other != context.agent;
    }
    case 3:
        return static_cast<std::uintptr_t>(
            context.group->agents[arg]->phase);
    case 4: return context.group->agents[arg]->target_hardpoint;
    case 5: *context.selected = arg; return 1;
    default: return 0;
    }
}

bool choose_available_hardpoint(
        Agent* agent, const RuntimeGroup* group,
        std::size_t hardpoint_count, std::uint32_t capacity,
        bool interaction, std::size_t* selected) noexcept {
    if (!agent || !selected) return false;
    SwarmHardpointChoiceContext context{agent, group, selected};
    return a2fo_rs_stage8_swarm_choose_hardpoint(
        &swarm_hardpoint_choice_step, &context, hardpoint_count, capacity,
        interaction ? 1 : 0, group ? 1 : 0) != 0;
}

struct SwarmTargetContext {
    Agent* agent = nullptr;
    const SwarmDefinition* definition = nullptr;
    const BoundingSphere* bounds = nullptr;
    const RuntimeGroup* group = nullptr;
    std::size_t selected = 0;
};

std::uintptr_t swarm_target_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& context = *static_cast<SwarmTargetContext*>(opaque);
    Agent& agent = *context.agent;
    const SwarmDefinition& definition = *context.definition;
    switch (op) {
    case 0: return !definition.interaction_hardpoints.empty();
    case 1: return agent.random.unit() < definition.interaction_chance;
    case 2:
        context.selected = 0;
        return choose_available_hardpoint(
            &agent, context.group, definition.interaction_hardpoints.size(),
            definition.interaction_capacity, true, &context.selected);
    case 3:
        agent.phase = AgentPhase::approaching_interaction;
        agent.target_hardpoint = context.selected;
        return 1;
    case 4:
        agent.target_offset = a2fo::swarm::random_shell_point(
            agent.random, 0.0f, definition.interaction_radius);
        return 1;
    case 5:
        agent.speed = agent.random.range(
            definition.minimum_speed, definition.maximum_speed);
        return 1;
    case 6: return definition.return_to_hardpoint;
    case 7: return agent.random.unit() < 0.12f;
    case 8: agent.phase = AgentPhase::returning; return 1;
    case 9:
        context.selected = 0;
        return choose_available_hardpoint(
            &agent, context.group,
            std::max<std::size_t>(1, definition.launch_hardpoints.size()),
            definition.hardpoint_capacity, false, &context.selected);
    case 10:
        agent.target_hardpoint = context.selected;
        agent.target_offset = {};
        return 1;
    case 11:
        agent.speed = agent.random.range(
            definition.minimum_speed, definition.maximum_speed);
        return 1;
    case 12: agent.phase = AgentPhase::roaming; return 1;
    case 13:
        choose_roaming_target(&agent, definition, *context.bounds);
        return 1;
    default: return 0;
    }
}

void choose_next_target(Agent* agent,
                        const SwarmDefinition& definition,
                        const BoundingSphere& bounds,
                        const RuntimeGroup* group) noexcept {
    if (!agent) return;
    SwarmTargetContext context{agent, &definition, &bounds, group};
    a2fo_rs_stage8_swarm_choose_target(&swarm_target_step, &context);
}

struct SwarmRefreshContext {
    const HostRuntime* host = nullptr;
    Agent* agent = nullptr;
    const SwarmDefinition* definition = nullptr;
    const std::vector<Hardpoint>* hardpoints = nullptr;
    Vec3 anchor{};
    float exclusion = 0.0f;
};

std::uintptr_t swarm_refresh_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& context = *static_cast<SwarmRefreshContext*>(opaque);
    if (op == 0) return context.host->object && context.agent;
    Agent& agent = *context.agent;
    const HostRuntime& host = *context.host;
    const SwarmDefinition& definition = *context.definition;
    switch (op) {
    case 1: return static_cast<std::uintptr_t>(agent.phase);
    case 2: context.hardpoints = &definition.interaction_hardpoints; return 1;
    case 3: context.hardpoints = &definition.launch_hardpoints; return 1;
    case 4:
        return hardpoint_local_position(
            host.object,
            hardpoint_at(*context.hardpoints, agent.target_hardpoint),
            &context.anchor);
    case 5:
        agent.target = a2fo::swarm::add(
            context.anchor, agent.target_offset);
        return 1;
    case 6:
        context.exclusion = host_exclusion_radius(
            host.bounds, agent, definition);
        return context.exclusion > 0.0f;
    case 7:
        agent.target = a2fo::swarm::project_outside_sphere(
            agent.target, host.bounds.centre, context.exclusion,
            a2fo::swarm::subtract(context.anchor, host.bounds.centre));
        return 1;
    default: return 0;
    }
}

bool refresh_hardpoint_target(const HostRuntime& host, Agent* agent,
                              const SwarmDefinition& definition) noexcept {
    SwarmRefreshContext context{&host, agent, &definition};
    return a2fo_rs_stage8_swarm_refresh_target(
        &swarm_refresh_step, &context) != 0;
}

struct SwarmCreationContext {
    void* object = nullptr;
    const ClassPolicy* policy = nullptr;
    HostRuntime runtime{};
    RuntimeGroup group{};
    const SwarmDefinition* definition = nullptr;
    std::unique_ptr<Agent> agent;
    float exclusion = 0.0f;
    A2FO_Stage8SwarmCreation state{};
    std::exception_ptr exception{};
};

std::uintptr_t swarm_creation_step(void* opaque, std::uint32_t op,
                                   std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmCreationContext*>(opaque);
    if (context.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
        switch (op) {
        case 0: context.runtime.object = context.object; return 1;
        case 1:
            context.runtime.object_class = object_class(context.object);
            return 1;
        case 2:
            context.runtime.handle = object_handle(context.object);
            context.state.handle = context.runtime.handle;
            return 1;
        case 3: context.runtime.last_seen_epoch = g_epoch; return 1;
        case 4:
            host_bounding_sphere(context.object, &context.runtime.bounds);
            return 1;
        case 5:
            context.runtime.groups.reserve(context.policy->definitions.size());
            return 1;
        case 6:
            context.state.definition_count = static_cast<std::uint32_t>(
                context.policy->definitions.size());
            return 1;
        case 7:
            context.definition = &context.policy->definitions[arg];
            context.state.definition_index = context.definition->index;
            context.group = RuntimeGroup{};
            context.group.definition = arg;
            context.group.agents.reserve(context.definition->count);
            return 1;
        case 8:
            context.state.member_count = context.definition->count;
            return 1;
        case 9:
            context.agent = std::make_unique<Agent>(
                static_cast<std::uint32_t>(arg));
            return 1;
        case 10:
            return context.agent->visual.initialize(
                context.definition->model, context.definition->scale);
        case 11:
            context.state.failure_report = InterlockedIncrement(
                &g_instance_failure_reports);
            return 1;
        case 12: {
            char message[320]{};
            std::snprintf(message, sizeof(message),
                "Could not load swarm%lu model '%s' for '%s'",
                static_cast<unsigned long>(context.definition->index),
                context.definition->model.c_str(),
                context.policy->odf_name.c_str());
            log_line(message);
            return 1;
        }
        case 13:
            log_line("Further swarm model-load failures suppressed");
            return 1;
        case 14:
            // Native ST3D sphere and scale multiplication keep GNU/x87 bits.
            context.agent->visual_radius =
                context.agent->visual.bounding_radius() *
                context.definition->scale;
            return 1;
        case 15: return context.definition->avoid_host;
        case 16: return g_logged_avoidance_volume;
        case 17: return context.runtime.bounds.radius > 0.0f;
        case 18: {
            char message[320]{};
            std::snprintf(message, sizeof(message),
                "Host avoidance active on '%s': hostRadius=%.2f, "
                "visualRadius=%.2f, clearance=%.2f",
                context.policy->odf_name.c_str(),
                context.runtime.bounds.radius,
                context.agent->visual_radius,
                context.definition->host_clearance);
            log_line(message);
            g_logged_avoidance_volume = true;
            return 1;
        }
        case 19: return g_logged_missing_host_bounds;
        case 20: {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "Host avoidance unavailable on '%s': the native model "
                "has no valid bounding sphere",
                context.policy->odf_name.c_str());
            log_line(message);
            g_logged_missing_host_bounds = true;
            return 1;
        }
        case 21:
            context.state.launch_count = std::max<std::size_t>(
                1, context.definition->launch_hardpoints.size());
            return 1;
        case 22:
            context.state.random = context.agent->random.next_u32();
            return 1;
        case 23:
            hardpoint_local_position(
                context.object,
                hardpoint_at(context.definition->launch_hardpoints, arg),
                &context.agent->position);
            return 1;
        case 24:
            choose_roaming_target(context.agent.get(), *context.definition,
                                  context.runtime.bounds);
            return 1;
        case 25:
            context.exclusion = host_exclusion_radius(
                context.runtime.bounds, *context.agent,
                *context.definition);
            return context.exclusion > 0.0f;
        case 26:
            context.agent->position = a2fo::swarm::project_outside_sphere(
                context.agent->position, context.runtime.bounds.centre,
                context.exclusion,
                a2fo::swarm::subtract(
                    context.agent->target, context.runtime.bounds.centre));
            return 1;
        case 27:
            context.agent->direction = a2fo::swarm::normalized(
                a2fo::swarm::subtract(context.agent->target,
                                      context.agent->position));
            return 1;
        case 28:
            context.group.agents.push_back(std::move(context.agent));
            return 1;
        case 29: return !context.group.agents.empty();
        case 30:
            context.runtime.groups.push_back(std::move(context.group));
            return 1;
        case 31: return context.runtime.bounds.radius <= 0.0f;
        case 32: context.agent.reset(); return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

HostRuntime create_host_runtime(void* object, const ClassPolicy& policy) {
    SwarmCreationContext context{};
    context.object = object;
    context.policy = &policy;
    a2fo_rs_stage8_swarm_create_host(
        &swarm_creation_step, &context, &context.state);
    if (context.exception) std::rethrow_exception(context.exception);
    return std::move(context.runtime);
}

struct SwarmObserveContext {
    explicit SwarmObserveContext(void* value) : object(value) {}
    void* object = nullptr;
    A2FO_Stage8SwarmObserved observed{};
    const ClassPolicy* policy = nullptr;
    std::unordered_map<std::uint32_t, HostRuntime>::iterator present{};
    HostRuntime runtime{};
    std::exception_ptr exception{};
};

std::uintptr_t swarm_observe_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& context = *static_cast<SwarmObserveContext*>(opaque);
    if (context.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
        switch (op) {
        case 0:
            return context.object && read_live_at<std::uint8_t>(
                context.object, kObjectExpiredOffset, 1) == 0;
        case 1:
            context.observed.handle = read_live_at<std::uint32_t>(
                context.object, kObjectHandleOffset, 0);
            return 1;
        case 2:
            context.observed.object_class = reinterpret_cast<std::uintptr_t>(
                read_live_at<void*>(context.object, kObjectClassOffset, nullptr));
            return 1;
        case 3:
            context.policy = ensure_class_policy(
                reinterpret_cast<void*>(context.observed.object_class));
            return context.policy && !context.policy->definitions.empty();
        case 4:
            context.present = g_hosts.find(context.observed.handle);
            return context.present != g_hosts.end();
        case 5:
            return context.present->second.object == context.object &&
                context.present->second.object_class ==
                    reinterpret_cast<void*>(context.observed.object_class);
        case 6:
            context.present->second.last_seen_epoch = g_epoch;
            return 1;
        case 7:
            context.runtime = create_host_runtime(context.object,
                                                  *context.policy);
            return 1;
        case 8: return context.runtime.groups.empty();
        case 9:
            g_hosts.insert_or_assign(context.observed.handle,
                                     std::move(context.runtime));
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

void observe_host(void* object) noexcept {
    SwarmObserveContext context(object);
    a2fo_rs_stage8_swarm_observe(&swarm_observe_step, &context,
                                 &context.observed);
    if (context.exception) log_line("Could not allocate per-host swarm state");
}

bool live_object_list(void** list_output, void** head_output,
                      std::uint32_t* count_output) noexcept {
    void* list = read_live_at<void*>(
        at(g_armada, kGameObjectListPointerRva), 0, nullptr);
    if (list != g_cached_object_list) {
        if (!readable_range(list, 12)) return false;
        g_cached_object_list = list;
    }
    void* current_head = read_live_at<void*>(list, 4, nullptr);
    if (current_head != g_cached_object_list_head) {
        if (!readable_range(current_head, 12)) return false;
        g_cached_object_list_head = current_head;
    }
    void* head = g_cached_object_list_head;
    std::uint32_t count = read_live_at<std::uint32_t>(list, 8, 0);
    if (!head || count > kMaximumObjects) return false;
    if (list_output) *list_output = list;
    if (head_output) *head_output = head;
    if (count_output) *count_output = count;
    return true;
}

struct SwarmEnumerationContext {
    A2FO_Stage8SwarmEnumeration state{};
};

std::uintptr_t swarm_enumeration_step(void* opaque, std::uint32_t op,
                                      std::uintptr_t) noexcept {
    auto& context = *static_cast<SwarmEnumerationContext*>(opaque);
    switch (op) {
    case 0: {
        void* list = nullptr;
        void* head = nullptr;
        if (!live_object_list(&list, &head, &context.state.count)) return 0;
        (void)list;
        context.state.head = reinterpret_cast<std::uintptr_t>(head);
        return 1;
    }
    case 1:
        context.state.node = reinterpret_cast<std::uintptr_t>(
            read_live_at<void*>(reinterpret_cast<void*>(context.state.head),
                                0, nullptr));
        return 1;
    case 2:
        context.state.next = reinterpret_cast<std::uintptr_t>(
            read_live_at<void*>(reinterpret_cast<void*>(context.state.node),
                                0, nullptr));
        return 1;
    case 3:
        observe_host(read_live_at<void*>(
            reinterpret_cast<void*>(context.state.node), 8, nullptr));
        return 1;
    default: return 0;
    }
}

bool enumerate_hosts(std::uint32_t* count_output) noexcept {
    SwarmEnumerationContext context{};
    const bool enumerated = a2fo_rs_stage8_swarm_enumerate(
        &swarm_enumeration_step, &context, &context.state) != 0;
    if (enumerated && count_output) *count_output = context.state.count;
    return enumerated;
}

struct SwarmUpdateContext {
    const HostRuntime* host = nullptr;
    RuntimeGroup* group = nullptr;
    Agent* agent = nullptr;
    const SwarmDefinition* definition = nullptr;
    float elapsed = 0.0f;
    Vec3 previous_target{};
    Vec3 previous_position{};
    Vec3 radius_relative{};
    float arrival = 0.0f;
    float exclusion = 0.0f;
    bool arrived = false;
};

std::uintptr_t swarm_update_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmUpdateContext*>(opaque);
    if (op == 0) return context.host->object && context.agent;
    Agent& agent = *context.agent;
    const HostRuntime& host = *context.host;
    const SwarmDefinition& definition = *context.definition;
    switch (op) {
    case 1: context.previous_target = agent.target; return 1;
    case 2: return refresh_hardpoint_target(host, &agent, definition);
    case 3:
        choose_roaming_target(&agent, definition, host.bounds);
        return 1;
    case 4: return static_cast<std::uintptr_t>(agent.phase);
    case 5:
        agent.position = a2fo::swarm::add(
            agent.position,
            a2fo::swarm::subtract(agent.target, context.previous_target));
        agent.dwell_remaining -= context.elapsed;
        return agent.dwell_remaining <= 0.0f;
    case 6:
        context.arrival = arg != 0
            ? std::max(0.25f, definition.interaction_radius * 0.25f)
            : 0.75f;
        return 1;
    case 7:
        context.previous_position = agent.position;
        context.arrived = a2fo::swarm::advance_agent(
            &agent.position, &agent.direction, agent.target,
            agent.speed, definition.turn_rate, context.elapsed,
            context.arrival);
        return 1;
    case 8:
        context.exclusion = host_exclusion_radius(
            host.bounds, agent, definition);
        return context.exclusion > 0.0f;
    case 9:
        return a2fo::swarm::constrain_motion_outside_sphere(
            context.previous_position, &agent.position, &agent.direction,
            host.bounds.centre, context.exclusion);
    case 10: context.arrived = false; return 1;
    case 11:
        context.radius_relative = a2fo::swarm::subtract(
            agent.position, arg != 0 ? host.bounds.centre : Vec3{});
        return 1;
    case 12:
        return a2fo::swarm::constrain_to_radius(
            &context.radius_relative,
            context.exclusion + definition.maximum_radius);
    case 13:
        agent.position = a2fo::swarm::add(
            arg != 0 ? host.bounds.centre : Vec3{},
            context.radius_relative);
        agent.direction = a2fo::swarm::normalized(
            a2fo::swarm::subtract(agent.target, agent.position),
            agent.direction);
        return 1;
    case 14: return context.arrived;
    case 15: return !(definition.interaction_time <= 0.0f);
    case 16:
        agent.phase = AgentPhase::dwelling_interaction;
        agent.dwell_remaining = definition.interaction_time;
        return 1;
    case 17:
        agent.phase = AgentPhase::dwelling_return;
        agent.dwell_remaining = agent.random.range(0.25f, 1.25f);
        return 1;
    case 18:
        choose_next_target(&agent, definition, host.bounds, context.group);
        return 1;
    default: return 0;
    }
}

void update_agent(const HostRuntime& host, RuntimeGroup* group, Agent* agent,
                  const SwarmDefinition& definition,
                  float elapsed_seconds) noexcept {
    SwarmUpdateContext context{&host, group, agent, &definition,
                               elapsed_seconds};
    a2fo_rs_stage8_swarm_update(&swarm_update_step, &context);
}

Vec3 pair_separation_axis(std::uint32_t left,
                          std::uint32_t right) noexcept {
    std::uint32_t seed = left ^ (right << 16) ^ (right >> 16) ^ 0x27d4eb2du;
    seed ^= seed >> 15;
    seed *= 0x85ebca6bu;
    seed ^= seed >> 13;
    Random random(seed == 0 ? 1u : seed);
    return a2fo::swarm::random_shell_point(random, 1.0f, 1.0f);
}

struct SwarmSeparationContext {
    HostRuntime* host = nullptr;
    RuntimeGroup* group = nullptr;
    const SwarmDefinition* definition = nullptr;
    Agent* left = nullptr;
    Agent* right = nullptr;
    Agent* agent = nullptr;
    float minimum_distance = 0.0f;
    float exclusion = 0.0f;
    Vec3 relative{};
};

std::uintptr_t swarm_separation_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmSeparationContext*>(opaque);
    if (op == 0) return context.group ? context.group->agents.size() : 0;
    RuntimeGroup& group = *context.group;
    const SwarmDefinition& definition = *context.definition;
    switch (op) {
    case 1:
        context.left = group.agents[arg].get();
        return context.left != nullptr;
    case 2:
        context.right = group.agents[arg].get();
        return context.right != nullptr;
    case 3: {
        context.minimum_distance =
            context.left->visual_radius + context.right->visual_radius +
            definition.member_separation;
        const Vec3 separation = a2fo::swarm::subtract(
            context.left->position, context.right->position);
        const float distance_squared =
            a2fo::swarm::length_squared(separation);
        return std::isfinite(distance_squared) &&
            distance_squared >=
                context.minimum_distance * context.minimum_distance;
    }
    case 4:
        a2fo::swarm::separate_pair(
            &context.left->position, &context.left->direction,
            &context.right->position, &context.right->direction,
            context.minimum_distance,
            pair_separation_axis(context.left->identity,
                                 context.right->identity));
        return 1;
    case 5:
        context.agent = group.agents[arg].get();
        return context.agent != nullptr;
    case 6:
        context.exclusion = host_exclusion_radius(
            context.host->bounds, *context.agent, definition);
        return context.exclusion > 0.0f;
    case 7:
        context.agent->position = a2fo::swarm::project_outside_sphere(
            context.agent->position, context.host->bounds.centre,
            context.exclusion, context.agent->direction);
        return 1;
    case 8: return static_cast<std::uintptr_t>(context.agent->phase);
    case 9:
        context.relative = a2fo::swarm::subtract(
            context.agent->position,
            arg != 0 ? context.host->bounds.centre : Vec3{});
        return a2fo::swarm::constrain_to_radius(
            &context.relative,
            context.exclusion + definition.maximum_radius);
    case 10:
        context.agent->position = a2fo::swarm::add(
            arg != 0 ? context.host->bounds.centre : Vec3{},
            context.relative);
        return 1;
    default: return 0;
    }
}

void separate_group_members(HostRuntime& host, RuntimeGroup* group,
                            const SwarmDefinition& definition) noexcept {
    SwarmSeparationContext context{&host, group, &definition};
    a2fo_rs_stage8_swarm_separate(&swarm_separation_step, &context);
}

struct SwarmSimulationContext {
    A2FO_Stage8SwarmSimulation state{};
    float elapsed = 0.0f;
    std::unordered_map<std::uint32_t, HostRuntime>::iterator iterator{};
    std::unordered_map<std::uint32_t, HostRuntime>::iterator cleanup{};
    std::unordered_map<void*, ClassPolicy>::iterator policy{};
    HostRuntime* host = nullptr;
    RuntimeGroup* group = nullptr;
    const SwarmDefinition* definition = nullptr;
    std::exception_ptr exception{};
};

std::uintptr_t swarm_simulation_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmSimulationContext*>(opaque);
    if (context.exception && op != 31 && op != 32) {
        return std::numeric_limits<std::uintptr_t>::max();
    }
    try {
        switch (op) {
        case 0: return g_inside_simulation;
        case 1: g_inside_simulation = true; return 1;
        case 2: context.state.epoch = g_epoch; return 1;
        case 3: g_epoch = static_cast<std::uint32_t>(arg); return 1;
        case 4:
            for (auto& entry : g_hosts) entry.second.last_seen_epoch = 0;
            return 1;
        case 5:
            return live_object_list(nullptr, nullptr,
                                    &context.state.object_count);
        case 6:
            context.state.last_object_count = g_last_object_count;
            context.state.countdown = g_host_discovery_countdown;
            return 1;
        case 7:
            return enumerate_hosts(&context.state.enumerated_count);
        case 8:
            g_last_object_count = context.state.enumerated_count;
            return 1;
        case 9:
            g_host_discovery_countdown = static_cast<std::uint32_t>(arg);
            return 1;
        case 10: --g_host_discovery_countdown; return 1;
        case 11: context.iterator = g_hosts.begin(); return 1;
        case 12: return context.iterator != g_hosts.end();
        case 13:
            context.host = &context.iterator->second;
            context.state.last_seen_epoch = context.host->last_seen_epoch;
            return 1;
        case 14: return context.host->object != nullptr;
        case 15: return object_expired(context.host->object);
        case 16:
            context.policy = g_class_policies.find(context.host->object_class);
            return context.policy != g_class_policies.end();
        case 17:
            context.state.group_count = static_cast<std::uint32_t>(
                context.host->groups.size());
            return 1;
        case 18:
            context.group = &context.host->groups[arg];
            context.state.group_definition = static_cast<std::uint32_t>(
                context.group->definition);
            return 1;
        case 19:
            context.state.definition_count = static_cast<std::uint32_t>(
                context.policy->second.definitions.size());
            if (context.group->definition <
                    context.policy->second.definitions.size()) {
                context.definition = &context.policy->second.definitions[
                    context.group->definition];
            }
            return 1;
        case 20:
            context.state.agent_count = static_cast<std::uint32_t>(
                context.group->agents.size());
            return 1;
        case 21:
            update_agent(*context.host, context.group,
                         context.group->agents[arg].get(),
                         *context.definition, context.elapsed);
            return 1;
        case 22:
            separate_group_members(*context.host, context.group,
                                   *context.definition);
            return 1;
        case 23: ++context.iterator; return 1;
        case 24: context.cleanup = g_hosts.begin(); return 1;
        case 25: return context.cleanup != g_hosts.end();
        case 26:
            context.state.last_seen_epoch =
                context.cleanup->second.last_seen_epoch;
            return 1;
        case 27: return context.cleanup->second.object != nullptr;
        case 28: return object_expired(context.cleanup->second.object);
        case 29: context.cleanup = g_hosts.erase(context.cleanup); return 1;
        case 30: ++context.cleanup; return 1;
        case 31: g_inside_simulation = false; return 1;
        case 32:
            log_line("Swarm simulation recovered from an allocation failure");
            return 1;
        case 33: {
            const std::uint32_t bits = static_cast<std::uint32_t>(arg);
            std::memcpy(&context.elapsed, &bits, sizeof(bits));
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

void simulate_swarms(float elapsed_seconds) noexcept {
    SwarmSimulationContext context{};
    a2fo_rs_stage8_swarm_simulate(
        &swarm_simulation_step, &context, &context.state, elapsed_seconds);
}

bool should_draw_host(std::uint32_t handle) noexcept {
    if (handle == 0) return false;
    alignas(4) std::array<std::uint8_t, 0xc0> fake_bee{};
    std::memcpy(fake_bee.data() + 0xbc, &handle, sizeof(handle));
    return (a2fo_swarm_call_thiscall_1(
        at(g_armada, kBeeColonyShouldDrawBeeRva),
        at(g_armada, kWorkerBeeColonyRva),
        reinterpret_cast<std::uintptr_t>(fake_bee.data())) & 0xffu) != 0;
}

struct SwarmRenderContext {
    void* camera = nullptr;
    std::unordered_map<std::uint32_t, HostRuntime>::iterator iterator{};
    HostRuntime* host = nullptr;
    RuntimeGroup* group = nullptr;
    Agent* agent = nullptr;
    Matrix34 transform{};
    Vec3 preferred_up{};
    Vec3 world_position{};
};

std::uintptr_t swarm_render_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmRenderContext*>(opaque);
    switch (op) {
    case 0: return context.camera != nullptr;
    case 1: context.iterator = g_hosts.begin(); return 1;
    case 2: return context.iterator != g_hosts.end();
    case 3:
        context.host = &context.iterator->second;
        return context.host->object != nullptr;
    case 4: return object_expired(context.host->object);
    case 5: return should_draw_host(context.host->handle);
    case 6: return g_logged_visibility_rejection;
    case 7:
        log_line("A configured swarm host was rejected by native "
                 "worker-bee visibility policy");
        g_logged_visibility_rejection = true;
        return 1;
    case 8: ++context.iterator; return 1;
    case 9:
        if (!host_transform(context.host->object, &context.transform)) {
            return 0;
        }
        context.preferred_up = {
            context.transform.values[3], context.transform.values[4],
            context.transform.values[5]};
        return 1;
    case 11: return context.host->groups.size();
    case 12: context.group = &context.host->groups[arg]; return 1;
    case 13: return context.group->agents.size();
    case 14:
        context.agent = context.group->agents[arg].get();
        return context.agent != nullptr;
    case 15: {
        context.world_position = a2fo::swarm::transform_point(
            context.transform, context.agent->position);
        const Vec3 world_direction = a2fo::swarm::transform_direction(
            context.transform, context.agent->direction);
        const Matrix34 visual_transform =
            a2fo::swarm::compose_facing_transform(
                context.world_position, world_direction, context.preferred_up);
        context.agent->visual.set_transform(visual_transform);
        return context.agent->visual.render(context.camera);
    }
    case 16: return g_logged_first_instance_render;
    case 17:
        log_line("First dynamic swarm visual instance submitted "
                 "after passing Storm3D frustum testing");
        g_logged_first_instance_render = true;
        return 1;
    case 18: return g_logged_frustum_rejection;
    case 19: {
        char message[320]{};
        std::snprintf(message, sizeof(message),
            "A swarm instance was rejected by Storm3D frustum "
            "testing at world position %.1f, %.1f, %.1f",
            context.world_position.x, context.world_position.y,
            context.world_position.z);
        log_line(message);
        g_logged_frustum_rejection = true;
        return 1;
    }
    default: return 0;
    }
}

void render_swarms(void* camera) noexcept {
    SwarmRenderContext context{};
    context.camera = camera;
    a2fo_rs_stage8_swarm_render(&swarm_render_step, &context);
}

struct SwarmHookContext {
    void* self = nullptr;
    float elapsed = 0.0f;
    void* camera = nullptr;
    std::uint32_t flags = 0;
    bool rendering = false;
};

std::uintptr_t swarm_hook_step(void* opaque, std::uint32_t op,
                               std::uintptr_t) noexcept {
    auto& context = *static_cast<SwarmHookContext*>(opaque);
    try {
        switch (op) {
        case 0:
            if (context.rendering) {
                a2fo_swarm_call_thiscall_2(
                    g_bee_render_hook.gateway, context.self,
                    reinterpret_cast<std::uintptr_t>(context.camera),
                    context.flags);
            } else {
                a2fo_swarm_call_thiscall_1(
                    g_bee_simulate_hook.gateway, context.self,
                    float_bits(context.elapsed));
            }
            return 1;
        case 1:
            return context.rendering ? runtime_render_enabled()
                                     : sync_runtime_activation();
        case 2:
            return context.self == at(g_armada, kWorkerBeeColonyRva);
        case 3:
            return context.rendering ? g_logged_render_boundary
                                     : g_logged_simulation_boundary;
        case 4:
            if (context.rendering) {
                log_line("Worker-bee render boundary reached");
                g_logged_render_boundary = true;
            } else {
                log_line("Worker-bee simulation boundary reached");
                g_logged_simulation_boundary = true;
            }
            return 1;
        case 5:
            if (context.rendering) render_swarms(context.camera);
            else simulate_swarms(context.elapsed);
            return 1;
        default: return 0;
        }
    } catch (...) {
        // The original hooks are noexcept, including their gateway calls.
        std::terminate();
    }
}

void __attribute__((fastcall)) bee_colony_simulate_hook(
    void* self, void*, float elapsed_seconds) noexcept {
    SwarmHookContext context{};
    context.self = self;
    context.elapsed = elapsed_seconds;
    a2fo_rs_stage8_swarm_hook(&swarm_hook_step, &context);
}

void __attribute__((fastcall)) bee_colony_render_hook(
    void* self, void*, void* camera, std::uint32_t flags) noexcept {
    SwarmHookContext context{};
    context.self = self;
    context.camera = camera;
    context.flags = flags;
    context.rendering = true;
    a2fo_rs_stage8_swarm_hook(&swarm_hook_step, &context);
}

bool checked_binding_index(std::size_t index) noexcept {
    switch (index) {
    case 0: return checked_binding("BeeColony::Simulate", kBeeColonySimulateRva, kExpectedBeeColonySimulate);
    case 1: return checked_binding("BeeColony::Render", kBeeColonyRenderRva, kExpectedBeeColonyRender);
    case 2: return checked_binding("BeeColony::ShouldDrawBee", kBeeColonyShouldDrawBeeRva, kExpectedBeeColonyShouldDrawBee);
    case 3: return checked_binding("ParameterDB::ParameterDB", kParameterDbConstructorRva, kExpectedParameterDbConstructor);
    case 4: return checked_binding("ParameterDB::~ParameterDB", kParameterDbDestructorRva, kExpectedParameterDbDestructor);
    // The core may have installed its checked typed-default near jumps.
    case 5: return checked_binding("ParameterDB::Get(int)", kParameterDbGetIntRva, kExpectedParameterDbGetScalar, true);
    case 6: return checked_binding("ParameterDB::Get(float)", kParameterDbGetFloatRva, kExpectedParameterDbGetScalar, true);
    case 7: return checked_binding("ParameterDB::Get(bool)", kParameterDbGetBoolRva, kExpectedParameterDbGetScalar, true);
    case 8: return checked_binding("ParameterDB::Get(string)", kParameterDbGetStringRva, kExpectedParameterDbGetString, true);
    case 9: return checked_binding("ParameterDB::Get(string vector)", kParameterDbGetStringVectorRva, kExpectedParameterDbGetStringVector, true);
    case 10: return checked_binding("ST3D_Instance::ST3D_Instance", kSt3dInstanceConstructorRva, kExpectedSt3dInstanceConstructor);
    case 11: return checked_binding("ST3D_Instance::~ST3D_Instance", kSt3dInstanceDestructorRva, kExpectedSt3dInstanceDestructor);
    case 12: return checked_binding("ST3D_Instance::SetAnimationFlags", kSt3dInstanceSetAnimationFlagsRva, kExpectedSt3dInstanceSetAnimationFlags);
    case 13: return checked_binding("ST3D_Instance::ScaleGeometry", kSt3dInstanceScaleGeometryRva, kExpectedSt3dInstanceScaleGeometry);
    case 14: return checked_binding("ST3D_Instance::SetTransform", kSt3dInstanceSetTransformRva, kExpectedSt3dInstanceSetTransform);
    case 15: return checked_binding("ST3D_Instance::SetDatabase", kSt3dInstanceSetDatabaseRva, kExpectedSt3dInstanceSetDatabase);
    case 16: return checked_binding("ST3D_Instance::Render", kSt3dInstanceRenderRva, kExpectedSt3dInstanceRender);
    case 17: return checked_binding("ST3D_Instance::TriggerAnimation", kSt3dInstanceTriggerAnimationRva, kExpectedSt3dInstanceTriggerAnimation);
    case 18: return checked_binding("Entity::GetTransform", kEntityGetTransformRva, kExpectedEntityGetTransform);
    case 19: return checked_binding("Entity::GetBoundingSphere", kEntityGetBoundingSphereRva, kExpectedEntityGetBoundingSphere);
    case 20: return checked_binding("Entity::GetWorldTransform", kEntityGetWorldTransformRva, kExpectedEntityGetWorldTransform);
    case 21: return checked_binding("GameObjectClass::GetHierarchyRoot", kGameObjectClassGetHierarchyRootRva, kExpectedClassGetHierarchyRoot);
    case 22: return checked_binding("GameObjectClass::GetOdfName", kGameObjectClassGetOdfNameRva, kExpectedClassGetOdfName);
    case 23: return checked_binding("ST3D_Node::FindRecursive", kSt3dNodeFindRecursiveRva, kExpectedNodeFindRecursive);
    // Fleet Operations may detour the shared allocator. Retain this live
    // readability check instead of comparing the stock bytes.
    case 24: return checked_readable_binding("operator delete", kEngineOperatorDeleteRva, sizeof(kExpectedEngineOperatorDelete));
    case 25: return checked_readable_binding("GameObject::objectList", kGameObjectListPointerRva, 4);
    case 26: return checked_readable_binding("workerBeeColony", kWorkerBeeColonyRva, 4);
    default: return false;
    }
}

struct SwarmInitContext {
    const A2FO_ModuleApi* api = nullptr;
    std::exception_ptr exception{};
    bool installing = false;
};

std::uintptr_t swarm_step(void* opaque, std::uint32_t op,
                          std::uintptr_t arg) noexcept {
    auto& context = *static_cast<SwarmInitContext*>(opaque);
    if (context.exception) return std::numeric_limits<std::uintptr_t>::max();
    try {
        switch (op) {
        case 0:
            return context.api &&
                context.api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                context.api->api_version == A2FO_MODULE_API_VERSION &&
                context.api->log && context.api->armada_module &&
                context.api->install_inline_hook;
        case 1:
            g_api = context.api;
            return 1;
        case 2:
            g_armada = static_cast<HMODULE>(context.api->armada_module());
            return g_armada != nullptr;
        case 3:
            return checked_binding_index(arg);
        case 4:
            log_line("Supported ArmadaL swarm signatures were not found; runtime disabled");
            return 1;
        case 5:
            context.installing = true;
            return context.api->install_inline_hook(
                at(g_armada, kBeeColonySimulateRva),
                reinterpret_cast<void*>(&bee_colony_simulate_hook),
                sizeof(kExpectedBeeColonySimulate),
                kExpectedBeeColonySimulate, &g_bee_simulate_hook);
        case 6:
            log_line("Could not install the checked BeeColony simulation hook");
            return 1;
        case 7:
            context.installing = true;
            return context.api->install_inline_hook(
                at(g_armada, kBeeColonyRenderRva),
                reinterpret_cast<void*>(&bee_colony_render_hook),
                sizeof(kExpectedBeeColonyRender), kExpectedBeeColonyRender,
                &g_bee_render_hook);
        case 8:
            log_line("Could not install the checked BeeColony render hook");
            return 1;
        case 9:
            return g_bee_simulate_hook.gateway && g_bee_render_hook.gateway;
        case 10:
            g_runtime_ready = arg != 0;
            log_line(g_runtime_ready
                ? "Dynamic render-only swarm runtime initialized"
                : "Swarm module loaded with runtime disabled");
            return 1;
        case 11: g_runtime_ready = false; return 1;
        case 12: g_hosts.clear(); return 1;
        case 13: g_class_policies.clear(); return 1;
        case 14: g_host_discovery_countdown = 0; return 1;
        case 15:
            g_last_object_count = std::numeric_limits<std::uint32_t>::max();
            return 1;
        case 16: g_cached_object_list = nullptr; return 1;
        case 17: g_cached_object_list_head = nullptr; return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return std::numeric_limits<std::uintptr_t>::max();
    }
}

// The original install_hooks is noexcept. Keep its active-exception terminate
// behavior if an external installer throws, after Rust has stopped callbacks.
void rethrow_install_exception(std::exception_ptr error) noexcept {
    std::rethrow_exception(error);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    SwarmInitContext context{api};
    const bool result = a2fo_rs_stage8_swarm_init(&swarm_step, &context) != 0;
    if (context.exception) {
        if (context.installing) rethrow_install_exception(context.exception);
        std::rethrow_exception(context.exception);
    }
    // Hooks are process-lifetime patches. Remain resident and fail open even
    // if the second hook or a future executable signature is unsupported.
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    SwarmInitContext context{};
    a2fo_rs_stage8_swarm_shutdown(&swarm_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}
