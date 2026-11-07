#pragma once
#include <windows.h>
#include "../renderer/native11/asset_snapshot.hpp"
#include "../renderer/native11/material_snapshot.hpp"
#include "native11_world_lights.hpp"
#include "native11_world_model_snapshot.hpp"
#include "native11_world_view_snapshot.hpp"
#include "native11_world_preparation_snapshot.hpp"
#include "native11_world_visibility_snapshot.hpp"
#include "native11_world_borg_snapshot.hpp"
namespace a2fo::native11_capture {
using Log = void (*)(const char*);
// Shared exact-image identity gate for the separately guarded native observers.
bool exact_engine_image(HMODULE image) noexcept;
bool exact_image_file(HMODULE image,const unsigned char expected_sha256[32]) noexcept;
// Startup-only opt-in telemetry. It neither submits graphics nor replaces a
// native draw. Returns whether main-view wrapping is required.
bool initialize(HMODULE image, bool enabled, Log log) noexcept;
bool begin(void* camera, unsigned main) noexcept;
void end(void* camera, bool owned) noexcept;
void disable() noexcept;
// Most recent completed version-bound CPU capture, if all required models were
// available. This owner alone neither approves static geometry nor renders.
std::shared_ptr<const native11::BoundCapture> latest_bound() noexcept;
std::shared_ptr<const native11::BoundMaterialCapture> latest_materials() noexcept;
std::shared_ptr<const native11_world_lights::Snapshot> latest_lights() noexcept;
// Latest completed association only, never geometry/visibility/caster authority.
// Tickets are revalidated; the packet still describes its own immutable frame.
std::shared_ptr<const WorldModelSnapshot> latest_world_models() noexcept;
// Fresh distance/fade policy for its owned roster/frame, not complete visibility.
std::shared_ptr<const WorldViewSnapshot> latest_world_views() noexcept;
// Fresh logical systems/Borg/hull policy for its frame, not effective geometry.
std::shared_ptr<const WorldPreparationSnapshot> latest_world_preparations() noexcept;
// Owner-thread base node flags only, not FOW/material/geometry/caster authority.
std::shared_ptr<const WorldVisibilitySnapshot> latest_world_visibility() noexcept;
// Owner-thread correlated base/Borg slot operations only; unspecified slots,
// class/caller/overlay authority and final effective materials remain unresolved.
std::shared_ptr<const WorldBorgSnapshot> latest_world_borg_materials() noexcept;
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
// Pure cache-provider double for source/lifecycle fixtures only; never built
// into the production DLL. Setter and capture are externally serialized.
void set_world_template_source_for_testing(native11::PinWorldTemplate,void*) noexcept;
void set_world_material_sources_for_testing(native11::PinWorldMaterialBaseline,void*,
    native11::ResolveTextureAsset,void*) noexcept;
#endif
bool material_target(void* instance,void* camera,native11::Object&) noexcept;
bool camera_view(void* camera,native11::EngineCameraView&) noexcept;
}
extern "C" void a2fo_native11_observe_entity(void* entity, void* camera) noexcept;
extern "C" void a2fo_native11_entity_render_tap();
