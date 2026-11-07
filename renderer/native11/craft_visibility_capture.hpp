#pragma once
#include "craft_visibility.hpp"
#include "model_node_template.hpp"
#include "world_light_capture.hpp"
#include <memory>
#include <string>

namespace a2fo::native11 {
struct CapturedCraftPreparation;
struct CraftVisibilityReadProfile {
    std::uint32_t instance_vtable=0,database_vtable=0,engine_pointer=0;
    std::uint32_t render_context=0; // explicit host-selected context, not inferred
};
struct CapturedCraftVisibility {
    std::shared_ptr<const ModelNodeTemplate> baseline;
    std::shared_ptr<const LightClassRecord> object_class;
    std::uint32_t entity=0,instance=0,engine=0,render_context=0,quality=0;
    CraftVisibilityInput input;
    // Present only for the separately bracketed fresh logical-state route.
    std::shared_ptr<const CapturedCraftPreparation> preparation;
};
// Read-only, owner-serialized stored Craft flag inputs for one checked world
// member. Tree identity comes from an actual completed-load baseline; mutable
// native flags/matrices are never used as defaults. Pin must be pure and return
// the supplied class owner throughout both passes. The caller binds context,
// class behavior, model lifetime and world membership to its frame. This does
// not certify LOD/material/procedural policy or complete caster coverage.
// Failure leaves output unchanged; no native functions are invoked.
bool capture_craft_visibility(const Object&,const WorldMember&,
    std::shared_ptr<const LightClassRecord>,std::shared_ptr<const ModelNodeTemplate>,
    CraftVisibilityReadProfile,void* read_context,Read,
    void* class_context,PinLightClass,CapturedCraftVisibility&,
    std::string& error) noexcept;
} // namespace a2fo::native11
