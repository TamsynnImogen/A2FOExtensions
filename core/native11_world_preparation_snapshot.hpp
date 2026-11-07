#pragma once
#include "native11_world_model_snapshot.hpp"
#include "native11_craft_preparation_source.hpp"
#include "../renderer/native11/world_preparation_capture.hpp"
namespace a2fo::native11_capture {
// Owned logical flags/hull for this exact model roster and immutable frame.
// Fleet cache metadata is correlation only, never selected material authority.
struct WorldPreparationSnapshot {
    std::shared_ptr<const WorldModelSnapshot> models;
    native11_craft_preparation::source::Proof source;
    native11::WorldCraftPreparations preparations;
};
}
