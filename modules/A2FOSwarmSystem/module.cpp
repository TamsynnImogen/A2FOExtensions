/*
 * Dynamic, render-only ambient swarms for Fleet Operations.
 *
 * Every visual member is a native ST3D_Instance with sidecar movement state.
 * Swarms never enter the GameObject list and therefore have no AI, physics,
 * collision, selection, weapons, commands, team registration, or save record.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "swarm_motion.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
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
std::unordered_map<std::uint32_t, HostRuntime> g_hosts;
LONG g_instance_failure_reports = 0;
bool g_logged_simulation_boundary = false;
bool g_logged_render_boundary = false;
bool g_logged_visibility_rejection = false;
bool g_logged_first_instance_render = false;
bool g_logged_frustum_rejection = false;
bool g_logged_avoidance_volume = false;
bool g_logged_missing_host_bounds = false;

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
    while (first < value->size() && std::isspace(
               static_cast<unsigned char>((*value)[first]))) ++first;
    std::size_t last = value->size();
    while (last > first && std::isspace(
               static_cast<unsigned char>((*value)[last - 1]))) --last;
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
    int value = static_cast<int>(std::min<std::uint32_t>(
        *output, static_cast<std::uint32_t>(std::numeric_limits<int>::max())));
    const std::uintptr_t found = a2fo_swarm_call_thiscall_3(
        at(g_armada, kParameterDbGetIntRva), database,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(&value),
        static_cast<std::uintptr_t>(value));
    if ((found & 0xffu) == 0 || value < 0) return false;
    *output = static_cast<std::uint32_t>(
        std::min<int>(value,
                      static_cast<int>(kMaximumMembersPerDefinition)));
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

std::vector<Hardpoint> resolve_hardpoints(
    void* object_class_pointer, const std::vector<std::string>& names,
    const char* kind, const std::string& odf_name,
    std::uint32_t definition_index) {
    std::vector<Hardpoint> result;
    if (names.empty()) return result;
    void* hierarchy = read_at<void*>(object_class_pointer, 0x1d8, nullptr);
    if (!hierarchy || !readable_range(
            static_cast<std::uint8_t*>(hierarchy) + 0x3c, sizeof(void*))) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "Ignoring swarm%lu %s hardpoints on '%s': "
                      "the host has no loaded SOD hierarchy",
                      static_cast<unsigned long>(definition_index), kind,
                      odf_name.c_str());
        log_line(message);
        return result;
    }
    void* root = reinterpret_cast<void*>(a2fo_swarm_call_thiscall_0(
        at(g_armada, kGameObjectClassGetHierarchyRootRva),
        object_class_pointer));
    for (const std::string& name : names) {
        void* node = root ? reinterpret_cast<void*>(
            a2fo_swarm_call_thiscall_1(
                at(g_armada, kSt3dNodeFindRecursiveRva), root,
                reinterpret_cast<std::uintptr_t>(name.c_str()))) : nullptr;
        if (!node) {
            char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "Ignoring missing swarm%lu %s hardpoint '%s' on '%s'",
                static_cast<unsigned long>(definition_index), kind,
                name.c_str(), odf_name.c_str());
            log_line(message);
            continue;
        }
        result.push_back(Hardpoint{name, node});
    }
    return result;
}

void normalize_definition(SwarmDefinition* definition) noexcept {
    if (!definition) return;
    definition->scale = std::max(0.001f, std::min(definition->scale, 100.0f));
    definition->minimum_radius = std::max(0.0f,
        std::min(definition->minimum_radius, 100000.0f));
    definition->maximum_radius = std::max(definition->minimum_radius,
        std::min(definition->maximum_radius, 100000.0f));
    definition->minimum_speed = std::max(0.0f,
        std::min(definition->minimum_speed, 100000.0f));
    definition->maximum_speed = std::max(definition->minimum_speed,
        std::min(definition->maximum_speed, 100000.0f));
    definition->turn_rate = std::max(0.01f,
        std::min(definition->turn_rate, 1000.0f));
    definition->interaction_chance = std::max(0.0f,
        std::min(definition->interaction_chance, 1.0f));
    definition->interaction_time = std::max(0.0f,
        std::min(definition->interaction_time, 3600.0f));
    definition->interaction_radius = std::max(0.0f,
        std::min(definition->interaction_radius, 100000.0f));
    definition->host_clearance = std::max(0.0f,
        std::min(definition->host_clearance, 100000.0f));
    definition->member_separation = std::max(0.0f,
        std::min(definition->member_separation, 100000.0f));
    definition->count = static_cast<std::uint32_t>(
        std::min<std::size_t>(definition->count,
                              kMaximumMembersPerDefinition));
}

ClassPolicy parse_class_policy(void* object_class_pointer) {
    ClassPolicy policy{};
    policy.odf_name = class_odf_name(object_class_pointer);
    if (policy.odf_name.empty()) return policy;
    ParameterDbView database(policy.odf_name);
    if (!database.get()) return policy;

    std::size_t total_members = 0;
    for (std::size_t index = 0; index < kMaximumDefinitions; ++index) {
        SwarmDefinition definition{};
        definition.index = static_cast<std::uint32_t>(index);
        if (!read_parameter_string(
                database.get(), indexed_key(index).c_str(),
                &definition.model)) continue;

        read_count(database.get(), indexed_key(index, "Count"),
                   &definition.count);
        read_float(database.get(), indexed_key(index, "Scale"),
                   &definition.scale);
        const bool has_radius = read_float(
            database.get(), indexed_key(index, "Radius"),
            &definition.maximum_radius);
        read_float(database.get(), indexed_key(index, "MinRadius"),
                   &definition.minimum_radius);
        if (!read_float(database.get(), indexed_key(index, "MaxRadius"),
                        &definition.maximum_radius) && !has_radius) {
            definition.maximum_radius = 50.0f;
        }
        read_float(database.get(), indexed_key(index, "MinSpeed"),
                   &definition.minimum_speed);
        read_float(database.get(), indexed_key(index, "MaxSpeed"),
                   &definition.maximum_speed);
        read_float(database.get(), indexed_key(index, "TurnRate"),
                   &definition.turn_rate);
        read_float(database.get(), indexed_key(index, "InteractionChance"),
                   &definition.interaction_chance);
        read_float(database.get(), indexed_key(index, "InteractionTime"),
                   &definition.interaction_time);
        read_float(database.get(), indexed_key(index, "InteractionRadius"),
                   &definition.interaction_radius);
        read_float(database.get(), indexed_key(index, "HostClearance"),
                   &definition.host_clearance);
        read_float(database.get(), indexed_key(index, "Separation"),
                   &definition.member_separation);
        read_count(database.get(), indexed_key(index, "HardpointCapacity"),
                   &definition.hardpoint_capacity);
        read_count(database.get(), indexed_key(index, "InteractionCapacity"),
                   &definition.interaction_capacity);
        read_bool(database.get(), indexed_key(index, "ReturnToHardpoint"),
                  &definition.return_to_hardpoint);
        read_bool(database.get(), indexed_key(index, "AvoidHost"),
                  &definition.avoid_host);
        normalize_definition(&definition);

        if (definition.count == 0) continue;
        if (total_members >= kMaximumMembersPerHost) break;
        definition.count = static_cast<std::uint32_t>(
            std::min<std::size_t>(definition.count,
                kMaximumMembersPerHost - total_members));

        std::vector<std::string> launch_names;
        std::vector<std::string> interaction_names;
        read_parameter_list(database.get(),
                            indexed_key(index, "Hardpoint").c_str(),
                            &launch_names);
        read_parameter_list(database.get(),
                            indexed_key(index, "Interaction").c_str(),
                            &interaction_names);
        definition.launch_hardpoints = resolve_hardpoints(
            object_class_pointer, launch_names, "launch", policy.odf_name,
            definition.index);
        definition.interaction_hardpoints = resolve_hardpoints(
            object_class_pointer, interaction_names, "interaction",
            policy.odf_name, definition.index);

        char definition_message[512]{};
        std::snprintf(
            definition_message, sizeof(definition_message),
            "Resolved swarm%lu on '%s': model='%s', count=%lu, "
            "scale=%.2f, radius=%.2f..%.2f, speed=%.2f..%.2f, "
            "launch=%lu, interaction=%lu, hostAvoidance=%s+%.2f, "
            "separation=visualBounds+%.2f, capacity=%lu/%lu",
            static_cast<unsigned long>(definition.index),
            policy.odf_name.c_str(), definition.model.c_str(),
            static_cast<unsigned long>(definition.count), definition.scale,
            definition.minimum_radius, definition.maximum_radius,
            definition.minimum_speed, definition.maximum_speed,
            static_cast<unsigned long>(definition.launch_hardpoints.size()),
            static_cast<unsigned long>(
                definition.interaction_hardpoints.size()),
            definition.avoid_host ? "modelRadius" : "off",
            definition.host_clearance, definition.member_separation,
            static_cast<unsigned long>(definition.hardpoint_capacity),
            static_cast<unsigned long>(definition.interaction_capacity));
        log_line(definition_message);

        total_members += definition.count;
        policy.definitions.push_back(std::move(definition));
    }

    if (!policy.definitions.empty()) {
        char message[320]{};
        std::snprintf(message, sizeof(message),
                      "Registered %lu swarm definition%s (%lu visual "
                      "instance%s) on '%s'",
                      static_cast<unsigned long>(policy.definitions.size()),
                      policy.definitions.size() == 1 ? "" : "s",
                      static_cast<unsigned long>(total_members),
                      total_members == 1 ? "" : "s", policy.odf_name.c_str());
        log_line(message);
    }
    return policy;
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
    if (constructed_ || model.empty()) return false;
    a2fo_swarm_call_thiscall_0(
        at(g_armada, kSt3dInstanceConstructorRva), bytes_.data());
    constructed_ = true;
    a2fo_swarm_call_thiscall_1(
        at(g_armada, kSt3dInstanceSetDatabaseRva), bytes_.data(),
        reinterpret_cast<std::uintptr_t>(model.c_str()));
    void* logical = read_at<void*>(
        bytes_.data(), kSt3dLogicalDatabaseOffset, nullptr);
    void* visible = read_at<void*>(
        bytes_.data(), kSt3dVisibleDatabaseOffset, nullptr);
    if (!logical && !visible) {
        release();
        return false;
    }
    if (std::fabs(scale - 1.0f) > 0.0001f) {
        a2fo_swarm_call_thiscall_1(
            at(g_armada, kSt3dInstanceScaleGeometryRva), bytes_.data(),
            float_bits(scale));
    }
    // Native WorkerBee::CreateBee enables and starts animation for animated
    // bee SODs. Doing this unconditionally is harmless for static databases
    // and prevents animated-only hierarchy channels from remaining at an
    // invisible pre-trigger state.
    a2fo_swarm_call_thiscall_1(
        at(g_armada, kSt3dInstanceSetAnimationFlagsRva), bytes_.data(), 1);
    a2fo_swarm_call_thiscall_0(
        at(g_armada, kSt3dInstanceTriggerAnimationRva), bytes_.data());
    return true;
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
    return hardpoints.empty() ? nullptr :
        &hardpoints[index % hardpoints.size()];
}

float host_exclusion_radius(const BoundingSphere& bounds,
                            const Agent& agent,
                            const SwarmDefinition& definition) noexcept {
    if (!definition.avoid_host || bounds.radius <= 0.0f) return 0.0f;
    return bounds.radius + agent.visual_radius + definition.host_clearance;
}

void choose_roaming_target(Agent* agent,
                           const SwarmDefinition& definition,
                           const BoundingSphere& bounds) noexcept {
    if (!agent) return;
    agent->phase = AgentPhase::roaming;
    const float exclusion = host_exclusion_radius(
        bounds, *agent, definition);
    agent->target = a2fo::swarm::add(
        exclusion > 0.0f ? bounds.centre : Vec3{},
        a2fo::swarm::random_shell_point(
            agent->random, exclusion + definition.minimum_radius,
            exclusion + definition.maximum_radius));
    agent->speed = agent->random.range(
        definition.minimum_speed, definition.maximum_speed);
    agent->target_offset = {};
}

bool phase_claims_hardpoint(AgentPhase phase,
                            bool interaction) noexcept {
    return interaction
        ? phase == AgentPhase::approaching_interaction ||
              phase == AgentPhase::dwelling_interaction
        : phase == AgentPhase::returning ||
              phase == AgentPhase::dwelling_return;
}

bool choose_available_hardpoint(
        Agent* agent, const RuntimeGroup* group,
        std::size_t hardpoint_count, std::uint32_t capacity,
        bool interaction, std::size_t* selected) noexcept {
    if (!agent || !selected || hardpoint_count == 0) return false;
    const std::size_t first = static_cast<std::size_t>(
        agent->random.next_u32()) % hardpoint_count;
    for (std::size_t offset = 0; offset < hardpoint_count; ++offset) {
        const std::size_t candidate = (first + offset) % hardpoint_count;
        if (capacity == 0 || !group) {
            *selected = candidate;
            return true;
        }
        std::uint32_t claims = 0;
        for (const auto& member : group->agents) {
            const Agent* other = member.get();
            if (!other || other == agent ||
                !phase_claims_hardpoint(other->phase, interaction) ||
                other->target_hardpoint != candidate) continue;
            if (++claims >= capacity) break;
        }
        if (claims < capacity) {
            *selected = candidate;
            return true;
        }
    }
    return false;
}

void choose_next_target(Agent* agent,
                        const SwarmDefinition& definition,
                        const BoundingSphere& bounds,
                        const RuntimeGroup* group) noexcept {
    if (!agent) return;
    if (!definition.interaction_hardpoints.empty() &&
        agent->random.unit() < definition.interaction_chance) {
        std::size_t selected = 0;
        if (choose_available_hardpoint(
                agent, group, definition.interaction_hardpoints.size(),
                definition.interaction_capacity, true, &selected)) {
            agent->phase = AgentPhase::approaching_interaction;
            agent->target_hardpoint = selected;
            agent->target_offset = a2fo::swarm::random_shell_point(
                agent->random, 0.0f, definition.interaction_radius);
            agent->speed = agent->random.range(
                definition.minimum_speed, definition.maximum_speed);
            return;
        }
    }
    if (definition.return_to_hardpoint &&
        agent->random.unit() < 0.12f) {
        agent->phase = AgentPhase::returning;
        const std::size_t count = std::max<std::size_t>(
            1, definition.launch_hardpoints.size());
        std::size_t selected = 0;
        if (choose_available_hardpoint(
                agent, group, count, definition.hardpoint_capacity,
                false, &selected)) {
            agent->target_hardpoint = selected;
            agent->target_offset = {};
            agent->speed = agent->random.range(
                definition.minimum_speed, definition.maximum_speed);
            return;
        }
        agent->phase = AgentPhase::roaming;
    }
    choose_roaming_target(agent, definition, bounds);
}

bool refresh_hardpoint_target(const HostRuntime& host, Agent* agent,
                              const SwarmDefinition& definition) noexcept {
    if (!host.object || !agent) return false;
    const std::vector<Hardpoint>* hardpoints = nullptr;
    if (agent->phase == AgentPhase::approaching_interaction ||
        agent->phase == AgentPhase::dwelling_interaction) {
        hardpoints = &definition.interaction_hardpoints;
    } else if (agent->phase == AgentPhase::returning ||
               agent->phase == AgentPhase::dwelling_return) {
        hardpoints = &definition.launch_hardpoints;
    } else {
        return true;
    }
    Vec3 anchor{};
    if (!hardpoint_local_position(
            host.object, hardpoint_at(*hardpoints, agent->target_hardpoint),
            &anchor)) return false;
    agent->target = a2fo::swarm::add(anchor, agent->target_offset);
    const float exclusion = host_exclusion_radius(
        host.bounds, *agent, definition);
    if (exclusion > 0.0f) {
        agent->target = a2fo::swarm::project_outside_sphere(
            agent->target, host.bounds.centre, exclusion,
            a2fo::swarm::subtract(anchor, host.bounds.centre));
    }
    return true;
}

std::uint32_t mix_seed(std::uint32_t handle, std::uint32_t definition,
                       std::uint32_t member) noexcept {
    std::uint32_t value = handle ^ 0x9e3779b9u;
    value ^= definition + 0x85ebca6bu + (value << 6) + (value >> 2);
    value ^= member + 0xc2b2ae35u + (value << 6) + (value >> 2);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return value == 0 ? 1u : value;
}

HostRuntime create_host_runtime(void* object, const ClassPolicy& policy) {
    HostRuntime runtime{};
    runtime.object = object;
    runtime.object_class = object_class(object);
    runtime.handle = object_handle(object);
    runtime.last_seen_epoch = g_epoch;
    host_bounding_sphere(object, &runtime.bounds);
    runtime.groups.reserve(policy.definitions.size());

    for (std::size_t definition_index = 0;
         definition_index < policy.definitions.size(); ++definition_index) {
        const SwarmDefinition& definition =
            policy.definitions[definition_index];
        RuntimeGroup group{};
        group.definition = definition_index;
        group.agents.reserve(definition.count);
        for (std::uint32_t member = 0; member < definition.count; ++member) {
            auto agent = std::make_unique<Agent>(mix_seed(
                runtime.handle, definition.index, member));
            if (!agent->visual.initialize(
                    definition.model, definition.scale)) {
                const LONG report = InterlockedIncrement(
                    &g_instance_failure_reports);
                if (report <= 8) {
                    char message[320]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Could not load swarm%lu model '%s' for '%s'",
                        static_cast<unsigned long>(definition.index),
                        definition.model.c_str(), policy.odf_name.c_str());
                    log_line(message);
                } else if (report == 9) {
                    log_line("Further swarm model-load failures suppressed");
                }
                // Database lookup is definition-wide. Avoid repeating the
                // same failed native load for every requested member.
                break;
            }
            // ST3D_Instance keeps its database sphere unscaled even when
            // ScaleGeometry stores a separate per-axis render scale.
            agent->visual_radius =
                agent->visual.bounding_radius() * definition.scale;
            if (definition.avoid_host && !g_logged_avoidance_volume &&
                runtime.bounds.radius > 0.0f) {
                char message[320]{};
                std::snprintf(
                    message, sizeof(message),
                    "Host avoidance active on '%s': hostRadius=%.2f, "
                    "visualRadius=%.2f, clearance=%.2f",
                    policy.odf_name.c_str(), runtime.bounds.radius,
                    agent->visual_radius, definition.host_clearance);
                log_line(message);
                g_logged_avoidance_volume = true;
            } else if (definition.avoid_host &&
                       !g_logged_missing_host_bounds &&
                       runtime.bounds.radius <= 0.0f) {
                char message[256]{};
                std::snprintf(
                    message, sizeof(message),
                    "Host avoidance unavailable on '%s': the native model "
                    "has no valid bounding sphere",
                    policy.odf_name.c_str());
                log_line(message);
                g_logged_missing_host_bounds = true;
            }

            const std::size_t launch_count = std::max<std::size_t>(
                1, definition.launch_hardpoints.size());
            const std::size_t launch_index =
                (static_cast<std::size_t>(member) +
                 static_cast<std::size_t>(agent->random.next_u32())) %
                launch_count;
            hardpoint_local_position(
                object, hardpoint_at(definition.launch_hardpoints,
                                     launch_index),
                &agent->position);
            choose_roaming_target(agent.get(), definition, runtime.bounds);
            const float exclusion = host_exclusion_radius(
                runtime.bounds, *agent, definition);
            if (exclusion > 0.0f) {
                agent->position = a2fo::swarm::project_outside_sphere(
                    agent->position, runtime.bounds.centre, exclusion,
                    a2fo::swarm::subtract(
                        agent->target, runtime.bounds.centre));
            }
            agent->direction = a2fo::swarm::normalized(
                a2fo::swarm::subtract(agent->target, agent->position));
            group.agents.push_back(std::move(agent));
        }
        if (!group.agents.empty()) runtime.groups.push_back(std::move(group));
    }
    return runtime;
}

void observe_host(void* object) noexcept {
    if (!object || read_live_at<std::uint8_t>(
            object, kObjectExpiredOffset, 1) != 0) return;
    const std::uint32_t handle = read_live_at<std::uint32_t>(
        object, kObjectHandleOffset, 0);
    void* class_pointer = read_live_at<void*>(
        object, kObjectClassOffset, nullptr);
    if (handle == 0 || !class_pointer) return;
    const ClassPolicy* policy = ensure_class_policy(class_pointer);
    if (!policy || policy->definitions.empty()) return;

    auto present = g_hosts.find(handle);
    if (present != g_hosts.end() &&
        present->second.object == object &&
        present->second.object_class == class_pointer) {
        present->second.last_seen_epoch = g_epoch;
        return;
    }
    try {
        HostRuntime runtime = create_host_runtime(object, *policy);
        if (runtime.groups.empty()) return;
        g_hosts.insert_or_assign(handle, std::move(runtime));
    } catch (...) {
        log_line("Could not allocate per-host swarm state");
    }
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

bool enumerate_hosts(std::uint32_t* count_output) noexcept {
    void* list = nullptr;
    void* head = nullptr;
    std::uint32_t count = 0;
    if (!live_object_list(&list, &head, &count)) return false;
    (void)list;
    void* node = read_live_at<void*>(head, 0, nullptr);
    for (std::uint32_t index = 0;
         node && node != head && index < count; ++index) {
        void* next = read_live_at<void*>(node, 0, nullptr);
        observe_host(read_live_at<void*>(node, 8, nullptr));
        if (next == node) break;
        node = next;
    }
    if (count_output) *count_output = count;
    return true;
}

void update_agent(const HostRuntime& host, RuntimeGroup* group, Agent* agent,
                  const SwarmDefinition& definition,
                  float elapsed_seconds) noexcept {
    if (!host.object || !agent) return;
    const Vec3 previous_target = agent->target;
    if (!refresh_hardpoint_target(host, agent, definition)) {
        choose_roaming_target(agent, definition, host.bounds);
    }
    if (agent->phase == AgentPhase::dwelling_interaction ||
        agent->phase == AgentPhase::dwelling_return) {
        // Preserve separation offsets established after the prior update.
        // Moving/animated hardpoints still carry the whole local formation by
        // the target delta instead of collapsing every member onto one point.
        agent->position = a2fo::swarm::add(
            agent->position,
            a2fo::swarm::subtract(agent->target, previous_target));
        agent->dwell_remaining -= elapsed_seconds;
        if (agent->dwell_remaining <= 0.0f) {
            // Always release a claimed hardpoint into a roaming leg. This
            // prevents repeated visits from turning interaction locations
            // into permanent gathering points.
            choose_roaming_target(agent, definition, host.bounds);
        }
        return;
    }

    const float arrival =
        agent->phase == AgentPhase::approaching_interaction
            ? std::max(0.25f, definition.interaction_radius * 0.25f)
            : 0.75f;
    const Vec3 previous_position = agent->position;
    bool arrived = a2fo::swarm::advance_agent(
            &agent->position, &agent->direction, agent->target,
            agent->speed, definition.turn_rate, elapsed_seconds,
            arrival);
    const float exclusion = host_exclusion_radius(
        host.bounds, *agent, definition);
    if (exclusion > 0.0f) {
        if (a2fo::swarm::constrain_motion_outside_sphere(
            previous_position, &agent->position, &agent->direction,
            host.bounds.centre, exclusion)) {
            arrived = false;
        }
    }
    Vec3 radius_relative = a2fo::swarm::subtract(
        agent->position,
        exclusion > 0.0f ? host.bounds.centre : Vec3{});
    if (agent->phase == AgentPhase::roaming &&
        a2fo::swarm::constrain_to_radius(
            &radius_relative, exclusion + definition.maximum_radius)) {
        agent->position = a2fo::swarm::add(
            exclusion > 0.0f ? host.bounds.centre : Vec3{},
            radius_relative);
        agent->direction = a2fo::swarm::normalized(
            a2fo::swarm::subtract(agent->target, agent->position),
            agent->direction);
    }
    if (!arrived) return;

    if (agent->phase == AgentPhase::approaching_interaction) {
        if (definition.interaction_time <= 0.0f) {
            choose_roaming_target(agent, definition, host.bounds);
        } else {
            agent->phase = AgentPhase::dwelling_interaction;
            agent->dwell_remaining = definition.interaction_time;
        }
    } else if (agent->phase == AgentPhase::returning) {
        agent->phase = AgentPhase::dwelling_return;
        agent->dwell_remaining = agent->random.range(0.25f, 1.25f);
    } else {
        choose_next_target(agent, definition, host.bounds, group);
    }
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

void separate_group_members(HostRuntime& host, RuntimeGroup* group,
                            const SwarmDefinition& definition) noexcept {
    if (!group || group->agents.size() < 2) return;

    // Small authored groups benefit from extra relaxation. Large ambient
    // groups use one pass so the fallback remains bounded instead of doing
    // three full O(n^2) sweeps at the 256-member limit.
    const unsigned relaxation_passes = group->agents.size() <= 16
        ? 3u : (group->agents.size() <= 64 ? 2u : 1u);
    for (unsigned pass = 0; pass < relaxation_passes; ++pass) {
        for (std::size_t left_index = 0;
             left_index + 1 < group->agents.size(); ++left_index) {
            Agent* left = group->agents[left_index].get();
            if (!left) continue;
            for (std::size_t right_index = left_index + 1;
                 right_index < group->agents.size(); ++right_index) {
                Agent* right = group->agents[right_index].get();
                if (!right) continue;
                const float minimum_distance =
                    left->visual_radius + right->visual_radius +
                    definition.member_separation;
                const Vec3 separation = a2fo::swarm::subtract(
                    left->position, right->position);
                const float distance_squared =
                    a2fo::swarm::length_squared(separation);
                if (std::isfinite(distance_squared) &&
                    distance_squared >=
                        minimum_distance * minimum_distance) {
                    continue;
                }
                a2fo::swarm::separate_pair(
                    &left->position, &left->direction,
                    &right->position, &right->direction,
                    minimum_distance,
                    pair_separation_axis(left->identity, right->identity));
            }
        }
    }

    // Pair correction can push one member toward the host or beyond the
    // roaming shell. Reapply the same cheap sidecar constraints afterward.
    for (const auto& member : group->agents) {
        Agent* agent = member.get();
        if (!agent) continue;
        const float exclusion = host_exclusion_radius(
            host.bounds, *agent, definition);
        if (exclusion > 0.0f) {
            agent->position = a2fo::swarm::project_outside_sphere(
                agent->position, host.bounds.centre, exclusion,
                agent->direction);
        }
        if (agent->phase != AgentPhase::roaming) continue;
        Vec3 relative = a2fo::swarm::subtract(
            agent->position,
            exclusion > 0.0f ? host.bounds.centre : Vec3{});
        if (a2fo::swarm::constrain_to_radius(
                &relative, exclusion + definition.maximum_radius)) {
            agent->position = a2fo::swarm::add(
                exclusion > 0.0f ? host.bounds.centre : Vec3{}, relative);
        }
    }
}

void simulate_swarms(float elapsed_seconds) noexcept {
    if (g_inside_simulation) return;
    g_inside_simulation = true;
    try {
        ++g_epoch;
        if (g_epoch == 0) {
            g_epoch = 1;
            for (auto& entry : g_hosts) entry.second.last_seen_epoch = 0;
        }
        std::uint32_t current_object_count = 0;
        const bool object_count_available = live_object_list(
            nullptr, nullptr, &current_object_count);
        const bool object_count_changed = object_count_available &&
            current_object_count != g_last_object_count;
        const bool changed_scan_due = object_count_changed &&
            g_host_discovery_countdown <=
                kHostDiscoveryIntervalTicks -
                    kHostDiscoveryMinimumTicks - 1;
        bool enumerated_hosts = false;
        if (g_host_discovery_countdown == 0 || changed_scan_due) {
            std::uint32_t enumerated_count = 0;
            enumerated_hosts = enumerate_hosts(&enumerated_count);
            if (enumerated_hosts) g_last_object_count = enumerated_count;
            g_host_discovery_countdown = kHostDiscoveryIntervalTicks - 1;
        } else {
            --g_host_discovery_countdown;
        }

        elapsed_seconds = std::max(0.0f, std::min(elapsed_seconds, 0.25f));
        for (auto& host_entry : g_hosts) {
            HostRuntime& host = host_entry.second;
            if ((enumerated_hosts && host.last_seen_epoch != g_epoch) ||
                !host.object ||
                object_expired(host.object)) continue;
            const auto policy = g_class_policies.find(host.object_class);
            if (policy == g_class_policies.end()) continue;
            for (RuntimeGroup& group : host.groups) {
                if (group.definition >= policy->second.definitions.size()) {
                    continue;
                }
                const SwarmDefinition& definition =
                    policy->second.definitions[group.definition];
                for (const auto& agent : group.agents) {
                    update_agent(host, &group, agent.get(), definition,
                                 elapsed_seconds);
                }
                separate_group_members(host, &group, definition);
            }
        }

        for (auto iterator = g_hosts.begin(); iterator != g_hosts.end();) {
            if ((enumerated_hosts &&
                 iterator->second.last_seen_epoch != g_epoch) ||
                !iterator->second.object ||
                object_expired(iterator->second.object)) {
                iterator = g_hosts.erase(iterator);
            } else {
                ++iterator;
            }
        }
    } catch (...) {
        log_line("Swarm simulation recovered from an allocation failure");
    }
    g_inside_simulation = false;
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

void render_swarms(void* camera) noexcept {
    if (!camera) return;
    for (auto& host_entry : g_hosts) {
        HostRuntime& host = host_entry.second;
        if (!host.object || object_expired(host.object)) continue;
        if (!should_draw_host(host.handle)) {
            if (!g_logged_visibility_rejection) {
                log_line("A configured swarm host was rejected by native "
                         "worker-bee visibility policy");
                g_logged_visibility_rejection = true;
            }
            continue;
        }
        Matrix34 transform{};
        if (!host_transform(host.object, &transform)) continue;
        const Vec3 preferred_up{transform.values[3], transform.values[4],
                                transform.values[5]};
        for (RuntimeGroup& group : host.groups) {
            for (const auto& agent : group.agents) {
                if (!agent) continue;
                const Vec3 world_position = a2fo::swarm::transform_point(
                    transform, agent->position);
                const Vec3 world_direction = a2fo::swarm::transform_direction(
                    transform, agent->direction);
                const Matrix34 visual_transform =
                    a2fo::swarm::compose_facing_transform(
                        world_position, world_direction, preferred_up);
                agent->visual.set_transform(visual_transform);
                const bool rendered = agent->visual.render(camera);
                if (rendered && !g_logged_first_instance_render) {
                    log_line("First dynamic swarm visual instance submitted "
                             "after passing Storm3D frustum testing");
                    g_logged_first_instance_render = true;
                } else if (!rendered && !g_logged_frustum_rejection) {
                    char message[320]{};
                    std::snprintf(
                        message, sizeof(message),
                        "A swarm instance was rejected by Storm3D frustum "
                        "testing at world position %.1f, %.1f, %.1f",
                        world_position.x, world_position.y,
                        world_position.z);
                    log_line(message);
                    g_logged_frustum_rejection = true;
                }
            }
        }
    }
}

void __attribute__((fastcall)) bee_colony_simulate_hook(
    void* self, void*, float elapsed_seconds) noexcept {
    a2fo_swarm_call_thiscall_1(
        g_bee_simulate_hook.gateway, self, float_bits(elapsed_seconds));
    if (g_runtime_ready && self == at(g_armada, kWorkerBeeColonyRva)) {
        if (!g_logged_simulation_boundary) {
            log_line("Worker-bee simulation boundary reached");
            g_logged_simulation_boundary = true;
        }
        simulate_swarms(elapsed_seconds);
    }
}

void __attribute__((fastcall)) bee_colony_render_hook(
    void* self, void*, void* camera, std::uint32_t flags) noexcept {
    a2fo_swarm_call_thiscall_2(
        g_bee_render_hook.gateway, self,
        reinterpret_cast<std::uintptr_t>(camera), flags);
    if (g_runtime_ready && self == at(g_armada, kWorkerBeeColonyRva)) {
        if (!g_logged_render_boundary) {
            log_line("Worker-bee render boundary reached");
            g_logged_render_boundary = true;
        }
        render_swarms(camera);
    }
}

bool preflight_signatures() noexcept {
    bool supported = true;
    supported &= checked_binding(
        "BeeColony::Simulate", kBeeColonySimulateRva,
        kExpectedBeeColonySimulate);
    supported &= checked_binding(
        "BeeColony::Render", kBeeColonyRenderRva,
        kExpectedBeeColonyRender);
    supported &= checked_binding(
        "BeeColony::ShouldDrawBee", kBeeColonyShouldDrawBeeRva,
        kExpectedBeeColonyShouldDrawBee);
    supported &= checked_binding(
        "ParameterDB::ParameterDB", kParameterDbConstructorRva,
        kExpectedParameterDbConstructor);
    supported &= checked_binding(
        "ParameterDB::~ParameterDB", kParameterDbDestructorRva,
        kExpectedParameterDbDestructor);
    // The core installs these checked typed-default hooks before deferred
    // modules load. Calling their live entry remains ABI-correct and preserves
    // registered classlabel defaults, so accept either stock bytes or A2FO's
    // five-byte near jump.
    supported &= checked_binding(
        "ParameterDB::Get(int)", kParameterDbGetIntRva,
        kExpectedParameterDbGetScalar, true);
    supported &= checked_binding(
        "ParameterDB::Get(float)", kParameterDbGetFloatRva,
        kExpectedParameterDbGetScalar, true);
    supported &= checked_binding(
        "ParameterDB::Get(bool)", kParameterDbGetBoolRva,
        kExpectedParameterDbGetScalar, true);
    supported &= checked_binding(
        "ParameterDB::Get(string)", kParameterDbGetStringRva,
        kExpectedParameterDbGetString, true);
    supported &= checked_binding(
        "ParameterDB::Get(string vector)",
        kParameterDbGetStringVectorRva,
        kExpectedParameterDbGetStringVector, true);
    supported &= checked_binding(
        "ST3D_Instance::ST3D_Instance", kSt3dInstanceConstructorRva,
        kExpectedSt3dInstanceConstructor);
    supported &= checked_binding(
        "ST3D_Instance::~ST3D_Instance", kSt3dInstanceDestructorRva,
        kExpectedSt3dInstanceDestructor);
    supported &= checked_binding(
        "ST3D_Instance::SetAnimationFlags",
        kSt3dInstanceSetAnimationFlagsRva,
        kExpectedSt3dInstanceSetAnimationFlags);
    supported &= checked_binding(
        "ST3D_Instance::ScaleGeometry", kSt3dInstanceScaleGeometryRva,
        kExpectedSt3dInstanceScaleGeometry);
    supported &= checked_binding(
        "ST3D_Instance::SetTransform", kSt3dInstanceSetTransformRva,
        kExpectedSt3dInstanceSetTransform);
    supported &= checked_binding(
        "ST3D_Instance::SetDatabase", kSt3dInstanceSetDatabaseRva,
        kExpectedSt3dInstanceSetDatabase);
    supported &= checked_binding(
        "ST3D_Instance::Render", kSt3dInstanceRenderRva,
        kExpectedSt3dInstanceRender);
    supported &= checked_binding(
        "ST3D_Instance::TriggerAnimation",
        kSt3dInstanceTriggerAnimationRva,
        kExpectedSt3dInstanceTriggerAnimation);
    supported &= checked_binding(
        "Entity::GetTransform", kEntityGetTransformRva,
        kExpectedEntityGetTransform);
    supported &= checked_binding(
        "Entity::GetBoundingSphere", kEntityGetBoundingSphereRva,
        kExpectedEntityGetBoundingSphere);
    supported &= checked_binding(
        "Entity::GetWorldTransform", kEntityGetWorldTransformRva,
        kExpectedEntityGetWorldTransform);
    supported &= checked_binding(
        "GameObjectClass::GetHierarchyRoot",
        kGameObjectClassGetHierarchyRootRva,
        kExpectedClassGetHierarchyRoot);
    supported &= checked_binding(
        "GameObjectClass::GetOdfName", kGameObjectClassGetOdfNameRva,
        kExpectedClassGetOdfName);
    supported &= checked_binding(
        "ST3D_Node::FindRecursive", kSt3dNodeFindRecursiveRva,
        kExpectedNodeFindRecursive);
    // Fleet Operations may detour the shared Armada allocator entry. Native
    // ParameterDB vector storage must be released through that live entry,
    // rather than bypassing its active memory-manager policy.
    supported &= checked_readable_binding(
        "operator delete", kEngineOperatorDeleteRva,
        sizeof(kExpectedEngineOperatorDelete));
    supported &= checked_readable_binding(
        "GameObject::objectList", kGameObjectListPointerRva, 4);
    supported &= checked_readable_binding(
        "workerBeeColony", kWorkerBeeColonyRva, 4);
    if (!supported) {
        log_line("Supported ArmadaL swarm signatures were not found; "
                 "runtime disabled");
    }
    return supported;
}

bool install_hooks(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->install_inline_hook || !preflight_signatures()) {
        return false;
    }
    if (!api->install_inline_hook(
            at(g_armada, kBeeColonySimulateRva),
            reinterpret_cast<void*>(&bee_colony_simulate_hook),
            sizeof(kExpectedBeeColonySimulate),
            kExpectedBeeColonySimulate, &g_bee_simulate_hook)) {
        log_line("Could not install the checked BeeColony simulation hook");
        return false;
    }
    if (!api->install_inline_hook(
            at(g_armada, kBeeColonyRenderRva),
            reinterpret_cast<void*>(&bee_colony_render_hook),
            sizeof(kExpectedBeeColonyRender), kExpectedBeeColonyRender,
            &g_bee_render_hook)) {
        log_line("Could not install the checked BeeColony render hook");
        return false;
    }
    return g_bee_simulate_hook.gateway && g_bee_render_hook.gateway;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada) return false;
    g_runtime_ready = install_hooks(api);
    if (g_runtime_ready) {
        log_line("Dynamic render-only swarm runtime initialized");
    } else {
        log_line("Swarm module loaded with runtime disabled");
    }
    // Hooks are process-lifetime patches. Remain resident and fail open even
    // if the second hook or a future executable signature is unsupported.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
    g_hosts.clear();
    g_class_policies.clear();
    g_host_discovery_countdown = 0;
    g_last_object_count = std::numeric_limits<std::uint32_t>::max();
    g_cached_object_list = nullptr;
    g_cached_object_list_head = nullptr;
}
