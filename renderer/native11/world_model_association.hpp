#pragma once
#include "world_light_capture.hpp"
#include "model_store.hpp"
#include <array>

namespace a2fo::native11 {
struct WorldModelReadProfile {
    std::uint32_t object_list_pointer=0,craft_instance_vtable=0,plain_database_vtable=0;
};
enum class WorldModelStatus {
    bound_current_database,no_instance,unsupported_instance,no_database,
    unsupported_database,unreadable,invalid_placement,model_unavailable
};
struct WorldModelAssociation {
    WorldMember member;
    WorldModelStatus status=WorldModelStatus::unreadable;
    std::uint32_t instance_vtable=0,database=0,database_vtable=0;
    char model_name[260]{};
    std::array<float,12> native_placement{}; // original Matrix34; no scale applied
    std::array<float,3> runtime_scale{{1,1,1}};
    std::uint8_t scale_active=0;
    std::shared_ptr<const ModelAsset> model;
};
struct WorldModelAssociations {
    // Exactly one entry per supplied checked-list member, including unresolved
    // and null-instance entries. No geometry, role or visibility assertion.
    std::vector<WorldModelAssociation> members;
};
// Exact owned association equality for transactional batch rewalks.
bool same_world_model_associations(const WorldModelAssociations&,const WorldModelAssociations&) noexcept;
using PinWorldModel=std::shared_ptr<const ModelAsset>(*)(void*,std::uint32_t,const char*);
// Externally serialized stable-world boundary, like capture_world_lights.
// Both traversals must exactly match the supplied roster and each other.
// The model is the instance's current DB field, not proof of render selection.
// Callbacks may not mutate the world. Failure preserves the previous output.
bool capture_world_model_associations(WorldModelReadProfile,
    const std::vector<WorldMember>&,void*,Read,void*,PinWorldModel,
    WorldModelAssociations&,std::string& error) noexcept;
}
