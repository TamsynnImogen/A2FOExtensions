/*
 * File: modules/A2FOFeaturePack/queue_enhancement.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Queue enhancements API (ctrl-click fill and continuous production) initialization interface.
 */

#pragma once

#include <windows.h>

#include "../../sdk/include/a2fo_module_api.h"

namespace a2fo {

// Installs optional, signature-checked queue conveniences. Failure leaves the
// recursive ODF feature pack loaded and is reported through the core logger.
bool initialize_queue_enhancements(const A2FO_ModuleApi* api,
                                   HMODULE armada,
                                   HMODULE fleet_ops) noexcept;

}  // namespace a2fo
