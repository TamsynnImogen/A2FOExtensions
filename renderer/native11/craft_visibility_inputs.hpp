#pragma once
#include "craft_visibility_capture.hpp"
#include "craft_preparation.hpp"
namespace a2fo::native11::detail {
// Identity only. No renderable placement, model selection or visibility claim.
struct CraftVisibilitySubject {
    std::uint32_t entity=0,instance=0,database=0,instance_vtable=0,database_vtable=0;
    std::uint64_t model_serial=0;
    ModelIdentity model_identity=ModelIdentity::unresolved;
    char model_name[260]{};
};
// Shared checked role/topology reader. Null preparation selects the historical
// stored-flag route; a supplied state bypasses all cached 9c..a4 outputs.
bool capture_craft_visibility_inputs(const CraftVisibilitySubject&,const WorldMember&,
    std::shared_ptr<const LightClassRecord>,std::shared_ptr<const ModelNodeTemplate>,
    CraftVisibilityReadProfile,const PreparedCraftState*,void*,Read,void*,PinLightClass,
    CapturedCraftVisibility&,std::string&) noexcept;
}
