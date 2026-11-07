#pragma once
#include <cstddef>
#include <cstdint>
#include "engine_camera.hpp"

namespace a2fo::native11 {
// Called only at the engine's post-frustum, visible-object observation point.
// All addresses are telemetry identifiers; the snapshot owns every byte it uses.
using Read = bool(*)(void* context, std::uint32_t address,
                         void* output, std::size_t length);
// Optional internal asset-version lookup. A zero result leaves the object as
// telemetry only for version-bound submission. The callback must not throw.
using ReadModelSerial = std::uint64_t(*)(void*,std::uint32_t database,const char* copied_name);
constexpr std::size_t capacity = 2048;
enum class ModelIdentity { unresolved, plain_database_name, unsupported_database };
enum class PlacementIdentity { authored_placement, craft_runtime_scale, unsupported_instance, unresolved };

struct Object {
    std::uint32_t entity = 0;
    std::uint32_t instance = 0;
    std::uint32_t instance_vtable = 0;
    float world[16]{}; // row-major, column-vector local-to-world
    float runtime_scale[3]{1,1,1};
    PlacementIdentity placement_identity = PlacementIdentity::authored_placement;
    std::uint32_t database = 0;
    std::uint32_t database_vtable = 0;
    ModelIdentity model_identity = ModelIdentity::unresolved;
    char model_name[260]{}; // copied original database filename, if resolved
    std::uint64_t model_serial = 0; // immutable store version at observation
    bool shadow_only=false; // owned world submission role, never inferred from frustum failure
};
struct Frame {
    std::uint32_t camera = 0;
    std::uint64_t generation = 0;
    std::size_t object_count = 0;
    float engine_screen[16]{}; // original row-vector CPU camera matrix
    EngineCameraView view{}; // native11 column-vector matrix and viewport
    Object objects[capacity]{};
    bool requires_part_states=false; // never silently substitute authored pose
    bool complete_casters=false; // explicit owned world-provider assertion
};

// Single owning thread. No engine address is dereferenced directly.
class Collector {
public:
    Collector(void* context, Read read) noexcept : read_(read), context_(context) {}
    bool configure_plain_database_vtable(std::uint32_t vtable) noexcept;
    // Nonzero enables exact-profile CraftInstance scale interpretation. Other
    // instance types remain telemetry and cannot be submitted by the adapter.
    bool configure_craft_instance_vtable(std::uint32_t vtable) noexcept;
    bool configure_model_serial_reader(void* context,ReadModelSerial) noexcept;
    bool begin(std::uint32_t camera, bool main) noexcept;
    void observe_post_frustum(std::uint32_t entity, std::uint32_t camera) noexcept;
    // Single-owner render-time correlation. Duplicate instance observations are
    // ambiguous; an auxiliary camera cannot attach to the main view.
    bool material_target(std::uint32_t instance,std::uint32_t camera,Object& output) const noexcept;
    bool camera_view(std::uint32_t camera,EngineCameraView& output) const noexcept;
    bool end(std::uint32_t camera, Frame& output) noexcept;
    void abort() noexcept;

private:
    bool read_at(std::uint32_t base, std::uint32_t offset,
                 void* output, std::size_t length) const noexcept;
    void capture_model_identity(Object& item) noexcept;
    bool capture_runtime_scale(Object& item) noexcept;
    Read read_ = nullptr;
    void* context_ = nullptr;
    Frame pending_{};
    std::uint64_t generation_ = 0;
    std::uint32_t plain_database_vtable_ = 0;
    std::uint32_t craft_instance_vtable_ = 0;
    void* serial_context_ = nullptr;
    ReadModelSerial serial_reader_ = nullptr;
    bool active_ = false;
    bool invalid_ = false;
};
} // namespace a2fo::native11
