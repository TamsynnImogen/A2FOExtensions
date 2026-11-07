#pragma once
#include "native11_world_model_snapshot.hpp"
#include "../renderer/native11/world_view_capture.hpp"
namespace a2fo::native11_capture {
struct WorldViewSnapshot {
    std::shared_ptr<const WorldModelSnapshot> models;
    native11::WorldCraftViews views;
};
}
