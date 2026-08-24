/*
 * File: modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: BuildYard module pseudo-technology initialization interface.
 */

#pragma once

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

namespace a2fo {

// Adds moduleXPseudoTechnology to Fleet Operations BuildYard configuration
// files. The pseudo project ID is checked in addition to the native
// moduleXRequiredTechnology ID while preserving native behavior when absent.
bool initialize_buildyard_pseudo_technology(
    const A2FO_ModuleApi* api, HMODULE armada, HMODULE fleet_ops) noexcept;

}  // namespace a2fo
