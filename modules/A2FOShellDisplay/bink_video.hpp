#pragma once

#include "../../sdk/include/a2fo_module_api.h"
#include <windows.h>

namespace a2fo::shell_display {

// Owns the Fleet Ops intro player and Armada movie scaling hooks so movies use
// the same presentation state as the native shell: selected resolution,
// framed/borderless mode and aspect-correct destination rectangles.
bool initialize_bink_video_scaling(const A2FO_ModuleApi* api,
                                   HMODULE armada) noexcept;
void shutdown_bink_video_scaling() noexcept;

} // namespace a2fo::shell_display
