#pragma once
#include "native11_world_lights.hpp"
#include "../renderer/native11/world_model_association.hpp"
namespace a2fo::native11_capture {
struct WorldModelSnapshot {
    std::shared_ptr<const native11_world_lights::Snapshot> world;
    std::uint32_t source_ticket=0;
    std::uint64_t capture_lifecycle=0,model_generation=0;
    native11::WorldModelAssociations associations;
};
}
