#pragma once
#include "../../sdk/include/a2fo_module_api.h"
#include "presentation.hpp"

namespace a2fo::refit_menu::skin_native {
// Caller has already admitted the complete FO file profile and pins the DLL.
bool preflight(HMODULE fo);
bool initialize(const A2FO_ModuleApi* api, HMODULE fo);
void shutdown();
Artwork main_artwork(void* form);
}
