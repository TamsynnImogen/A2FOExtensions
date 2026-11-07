#pragma once
#include "../../sdk/include/a2fo_module_api.h"
#include "presentation.hpp"

namespace a2fo::refit_menu::mission_backdrop {
// Parent admits/pins the supported engine before installing this observer.
bool initialize(const A2FO_ModuleApi*, HMODULE engine, HMODULE fleetops = nullptr);
bool active();
// UI thread only. Reads the game's front buffer once per interrupted mission
// frame, retaining CPU pixels across nested menu handovers. Never starts a scene.
const Artwork* frame();
void reset();
void shutdown();
}
