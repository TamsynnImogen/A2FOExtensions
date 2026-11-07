#pragma once
#include <windows.h>
#include "../../sdk/include/a2fo_module_api.h"
#include "presentation.hpp"
#include "../../sdk/include/a2fo_menu_scene.h"

namespace a2fo::refit_menu::loading_native {
// Called only after the parent module admits both complete image profiles.
bool initialize(const A2FO_ModuleApi* api, HMODULE armada, HMODULE fleetops, bool animate_between_steps = false);
// Copy of the native setup's own minimap, keyed by the current native map.
// No HWND/device or original pixel pointers survive the call.
void remember_setup_preview(const Artwork& image);
bool set_mission(const A2FO_MenuMissionV1* mission);
void clear_mission();
void shutdown();
}
