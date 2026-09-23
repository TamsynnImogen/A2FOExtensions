#pragma once

#include "object_editor_state.hpp"
#include "../../sdk/include/a2fo_module_api.h"
#include <windows.h>

namespace a2fo::object_editor {

using ReadDefaults = void (*)(void* craft, char* captain, char* registry,
                              std::size_t capacity) noexcept;

bool initialize(const A2FO_ModuleApi* api, HMODULE armada,
                ReadDefaults read_defaults) noexcept;
const SavedState* overrides(void* craft) noexcept;
std::uint64_t revision(void* craft) noexcept;
bool read_values(void* craft, SavedState* state, bool* directional) noexcept;
bool apply_values(void* craft, const SavedState& state,
                  bool apply_directional) noexcept;

}  // namespace a2fo::object_editor
