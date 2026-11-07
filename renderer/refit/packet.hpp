#pragma once

#include "host.h"
#include "../native11/material_observation.hpp"
#include <memory>
#include <string>
#include <vector>

namespace a2fo::refit {
struct CameraView2 {
    float world_to_clip[16]{}; // row-major, column-vector
    float viewport[4]{};      // x, y, width, height
    float eye[3]{};
    float time=0;
};
struct Presentation2 {
    std::uint32_t flags=0; // A2FO_REFIT_TEAM | A2FO_REFIT_UNLIT
    float alpha=1;
    float team[3]{1,1,1};
    float ambient[3]{1,1,1};
    float self_illum=0;
};

// The callback supplies an already admitted upload identity. It performs no IO.
using ResolveTexture2=bool(*)(void*,const std::shared_ptr<const native11::TextureAsset>&,
    std::string& name,std::uint32_t& revision,std::string& error);

class Packet2 final {
public:
    Packet2(const Packet2&)=delete;
    Packet2& operator=(const Packet2&)=delete;
    Packet2(Packet2&&)=delete;
    Packet2& operator=(Packet2&&)=delete;
    const A2FORefitFrame2& frame() const noexcept {return frame_;}
private:
    Packet2()=default;
    friend bool build_packet2(const native11::Object&,const native11::MaterialObservation&,
        const CameraView2&,const Presentation2&,std::uint32_t,std::uint32_t,std::uint32_t,
        std::uint32_t,std::uint32_t,const std::vector<A2FORefitLight>&,void*,ResolveTexture2,
        std::shared_ptr<const Packet2>&,std::string&) noexcept;
    A2FORefitFrame2 frame_{};
    std::vector<A2FORefitPart2> parts_;
    std::vector<std::string> texture_names_;
    std::vector<A2FORefitLight> lights_;
};

// model_id/revision name an uploaded copy of observation.model; node_count
// comes from that upload's parser. Failure leaves output untouched.
bool build_packet2(const native11::Object& object,const native11::MaterialObservation& observation,
    const CameraView2& camera,const Presentation2& presentation,
    std::uint32_t model_id,std::uint32_t model_revision,
    std::uint32_t node_count,std::uint32_t source_epoch,std::uint32_t serial,
    const std::vector<A2FORefitLight>& lights,void* texture_context,ResolveTexture2 resolve_texture,
    std::shared_ptr<const Packet2>& output,std::string& error) noexcept;
} // namespace a2fo::refit
