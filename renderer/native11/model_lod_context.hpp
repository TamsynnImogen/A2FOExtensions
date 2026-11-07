#pragma once
#include "native_matrix34.hpp"
#include "scene_capture.hpp"
#include <cstdint>
#include <string>
namespace a2fo::native11 {
struct ModelLodContextProfile {
    std::uint32_t engine_pointer=0,instance_vtable=0,database_vtable=0;
    std::uint32_t render_context=0;
};
struct ModelLodContext {
    std::uint32_t engine=0,camera=0,render_context=0,instance=0,database=0;
    std::uint32_t native_x87_control=0;
    NativeMatrix34 placement{},view{};
    std::array<float,4> thresholds{};
    std::int32_t requested=-1,quality_cap=4;
    bool force_highest=false;
};
// Two complete checked reads, including explicit engine/camera/context identity.
// Placement must still match the version-bound Object; no last-rendered
// instance, node matrix, current LOD, or node-to-camera global is consumed.
// Caller owns thread/frame/model/class lifetime and supplies the original CW.
// This does not certify class behavior, materials or complete caster coverage.
bool capture_model_lod_context(const Object&,std::uint32_t camera,
    std::uint32_t native_x87_control,ModelLodContextProfile,
    void* context,Read,ModelLodContext& output,std::string& error) noexcept;
} // namespace a2fo::native11
