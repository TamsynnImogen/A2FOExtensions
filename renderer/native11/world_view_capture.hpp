#pragma once
#include "world_model_association.hpp"
#include "craft_view_capture.hpp"
#include <vector>
namespace a2fo::native11 {
enum class WorldViewStatus {association_unavailable,view_unavailable,captured};
struct WorldCraftView {
    WorldMember member;
    WorldViewStatus status=WorldViewStatus::association_unavailable;
    CapturedCraftView view;
    std::string reason;
};
struct WorldCraftViews {
    std::uint32_t camera=0,native_x87_control=0;
    std::vector<WorldCraftView> members;
};
// Stable-world serialized reader. Two full view sweeps must agree, including
// shared policy inputs, then a fresh checked model/list walk must still equal
// the supplied associations. Unavailable rows stay explicit. This supplies no
// FOW, frustum, selected-model, material, part-pose or complete visibility proof.
// Pin/read callbacks must be pure. Failure leaves output unchanged.
bool capture_world_craft_views(const WorldModelAssociations&,WorldModelReadProfile,
    CraftViewReadProfile,std::uint32_t camera,std::uint32_t native_x87_control,
    void* read_context,Read,void* model_context,PinWorldModel,
    WorldCraftViews&,std::string& error) noexcept;
}
