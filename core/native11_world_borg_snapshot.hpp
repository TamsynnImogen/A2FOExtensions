#pragma once
#include "native11_world_preparation_snapshot.hpp"
#include "../renderer/native11/world_borg_materials.hpp"
namespace a2fo::native11_capture {
// Owned candidate slot operations, not an effective material or render packet.
struct WorldBorgSnapshot {
    std::shared_ptr<const WorldPreparationSnapshot> preparations;
    std::uint32_t owner_thread=0;
    native11::WorldBorgMaterials materials;
};
}
