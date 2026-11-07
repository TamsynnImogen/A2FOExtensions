#pragma once
#include "native11_world_preparation_snapshot.hpp"
#include "../renderer/native11/world_prepared_visibility.hpp"
namespace a2fo::native11_capture {
struct WorldVisibilitySnapshot {
    std::shared_ptr<const WorldPreparationSnapshot> preparations;
    std::uint32_t owner_thread=0;
    native11::WorldPreparedVisibility visibility;
};
}
