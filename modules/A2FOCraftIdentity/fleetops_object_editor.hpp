#pragma once

#include "../../sdk/include/a2fo_module_api.h"
#include <windows.h>

namespace a2fo::object_editor::fleetops {
bool initialize(const A2FO_ModuleApi* api, HMODULE module) noexcept;
}
