#pragma once
#include <windows.h>
#include <memory>
#include "../modules/A2FOAnimations/native_observation.hpp"
#include "../renderer/native11/scene_capture.hpp"
#include "../renderer/native11/instance_animation_time.hpp"
#include "../renderer/native11/runtime_pose_channels.hpp"
#include "../renderer/native11/model_lod_context.hpp"
#include "../renderer/native11/craft_view_capture.hpp"
namespace a2fo::native11 {struct ModelMaterialSource;struct MaterialObservation;struct ModelNodeTemplate;struct ModelMaterialBaseline;}
namespace a2fo::native11_animation_clock {
using Log=void(*)(const char*);
// Startup-only, default-off diagnostics at the existing checked node observer.
// No new hook, native function invocation, geometry producer or graphics takeover.
bool initialize(HMODULE,bool requested,Log,HMODULE fleet_ops=nullptr,
    animations::NativePlaybackSource=nullptr) noexcept;
void disable() noexcept;
bool enabled() noexcept;
void observe(const native11::Object&,std::uint32_t camera,
    const native11::ModelMaterialSource* source=nullptr,
    const native11::MaterialObservation* effective=nullptr,unsigned native_x87_control=0) noexcept;
// Owner-thread read-only producer, independent of the last rendered instance.
// Timing only: this does not approve offscreen geometry or caster coverage.
bool sample_time(const native11::Object&,native11::InstanceAnimationTime&,
                 std::string& error) noexcept;
bool sample_channels(const native11::Object&,native11::RuntimePoseChannels&,
                     std::string& error) noexcept;
bool sample_lod_context(const native11::Object&,std::uint32_t camera,
    std::uint32_t render_context,std::uint32_t native_x87_control,
    native11::ModelLodContext&,std::string& error) noexcept;
// Fresh logical position/camera/range/fade inputs, independent of native cached
// distance/alpha outputs. Caller still binds class, membership and frame scope.
bool sample_view(const native11::Object&,std::uint32_t camera,
    std::uint32_t native_x87_control,native11::CapturedCraftView&,
    std::string& error) noexcept;
// Only the existing completed ReadDatabase observer invokes this producer.
// It never refreshes a baseline from an instance render or a cache miss.
// Construction admission is independent of later playback-profile suspension;
// explicit disable revokes both metadata and runtime admission.
bool observe_model_loaded(const std::shared_ptr<const native11::ModelAsset>&) noexcept;
bool model_template(std::uint32_t database,std::uint64_t serial,
    std::shared_ptr<const native11::ModelNodeTemplate>&,std::string& error) noexcept;
// Optional raw load-time mesh slots. No late capture, current render-state
// reads, group assignment or class material selection. Retired textures revoke
// access even when the model and node template remain available.
bool model_material_baseline(std::uint32_t database,std::uint64_t serial,
    std::shared_ptr<const native11::ModelMaterialBaseline>&,std::string& error) noexcept;
}
