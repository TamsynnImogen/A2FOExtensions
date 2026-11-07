#pragma once
#include "native11_world_lights.hpp"
#include "../renderer/native11/world_session.hpp"
#include "../renderer/native11/observed_world.hpp"
#include <string>
#include <vector>
namespace a2fo::native11_world_pipeline {
using Log=void(*)(const char*);
struct Geometry {
    std::shared_ptr<const native11::BoundMaterialCapture> all;
    bool complete_casters=false,complete_visibility=false;
};
using GeometryProvider=bool(*)(void*,const native11::BoundMaterialCapture&,
    const native11_world_lights::Snapshot&,Geometry&,std::string&);
struct CandidateGeometry {
    std::vector<std::shared_ptr<const native11::BoundMaterialCapture>> shadow_candidates;
    native11::DomainEvidence absent_domains;
};
using CandidateProvider=bool(*)(void*,const native11::BoundMaterialCapture&,
    const native11_world_lights::Snapshot&,CandidateGeometry&,std::string&);
// Preferred adapter: the core audits same-frame membership and assigns roles.
// Absence proofs remain explicit host evidence; no native sampler is invented.
bool set_candidate_provider(CandidateProvider,void*) noexcept;
enum class State {disabled,waiting_sources,waiting_geometry,unavailable,rendered};
struct Status {State state=State::disabled;std::uint64_t frame=0,published_frame=0,rendered_frames=0;std::string reason;};
enum class MaterialState {disabled,waiting_sources,unavailable,prepared,previewed};
struct MaterialStatus {
    MaterialState state=MaterialState::disabled;
    std::uint64_t frame=0,source_epoch=0,submitted_frames=0;
    std::uint32_t source_ticket=0;
    std::string reason;
};
using MaterialValidator=bool(*)(const std::shared_ptr<const native11::BoundMaterialCapture>&) noexcept;
// Startup/source-owner registration; never promotes visibility/caster coverage.
bool set_material_validator(MaterialValidator) noexcept;
MaterialStatus material_status() noexcept;
bool readback_material_preview(std::vector<std::uint8_t>&,std::uint32_t&,std::uint32_t&,std::string&) noexcept;
// Startup only; no GPU work or helper loading occurs here. Default off.
bool configure_from_roots(const std::vector<std::string>&,Log) noexcept;
bool requested() noexcept;
void ambient(float out[3]) noexcept;
// Built-in checked geometry providers only. Default is absent, not the visible
// object list: it cannot establish complete offscreen casters. Registration and
// all operations are externally serialized; reentry is refused.
bool set_geometry_provider(GeometryProvider,void*) noexcept;
std::uint32_t source_epoch() noexcept;
// Immediately revokes publication. Candidate owners purge at next serialized
// admission/setter/shutdown; one previous accepted session packet may stay pinned.
void invalidate_sources() noexcept;
bool on_frame(std::shared_ptr<const native11::BoundMaterialCapture>,
    std::shared_ptr<const native11_world_lights::Snapshot>,std::uint32_t source_ticket=0) noexcept;
Status status() noexcept;
// Current owned no-provider diagnostic; empty after admission or source invalidation.
std::string diagnostic_json() noexcept;
bool readback(std::vector<std::uint8_t>&,std::uint32_t&,std::uint32_t&,std::string&) noexcept;
// Must succeed on the rendering owner outside loader lock before DLL unload.
bool shutdown() noexcept;
}
