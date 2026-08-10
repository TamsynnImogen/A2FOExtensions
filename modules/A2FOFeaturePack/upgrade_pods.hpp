/*
 * File: modules/A2FOFeaturePack/upgrade_pods.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Configurable upgrade-pod feature API declaration for tier-controlled research pod progression.
 */

#pragma once

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

namespace a2fo {

// Extends ResearchPod upgrade levels without allowing Armada's hardcoded
// tier-2/tier-3 Team arrays to be indexed out of bounds. Returns false when
// the supported executable signatures do not match; the rest of the feature
// pack remains available in that case.
bool initialize_upgrade_pods(const A2FO_ModuleApi* api,
                             HMODULE armada,
                             HMODULE fleet_ops);

}  // namespace a2fo
