/*
 * File: modules/A2FOFeaturePack/bink_video.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Bink video scaling module interface for initializing viewport-correct rendering hooks.
 */

#pragma once

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

namespace a2fo {

// Covers Fleet Operations' separate Direct3D9 intro player plus Armada's GDI
// and decoded-texture movie renderers.  Each original path otherwise retains
// native movie dimensions after the outer window is resized.
bool initialize_bink_video_scaling(const A2FO_ModuleApi* api,
                                   HMODULE armada) noexcept;

}  // namespace a2fo
