#pragma once

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

namespace a2fo {

// Covers Fleet Operations' separate Direct3D9 intro player plus Armada's GDI
// and decoded-texture movie renderers.  Each original path otherwise retains
// native movie dimensions after the outer window is resized.
bool initialize_bink_video_scaling(const A2FO_ModuleApi* api,
                                   HMODULE armada,
                                   HMODULE fleet_ops) noexcept;

}  // namespace a2fo
