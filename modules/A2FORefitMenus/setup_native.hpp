#pragma once
#include "../../sdk/include/a2fo_module_api.h"
#include "presentation.hpp"
#include <string_view>

namespace a2fo::refit_menu::setup_native {
void configure(const A2FO_ModuleApi*, bool enabled);
void start(); // Native UI thread, after the admitted main form exists.
bool asset(std::string_view path);
void observe(HWND window, const Artwork& fallback);
// Invoked only for a verified, accepted native Instant Action loading session.
bool begin_loading_handover();
void shutdown();
}
