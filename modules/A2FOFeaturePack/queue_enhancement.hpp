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

// Read-only admission hint for HybridBuild's native Producer::IsBusy gate.
// The real target-specific admission still happens in FeaturePack when the
// synchronized build command reaches the queue receiver.
bool producer_logical_queue_has_room(void* producer) noexcept;

}  // namespace a2fo
