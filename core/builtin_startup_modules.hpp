#pragma once
#include "builtin_state.hpp"
#include "../sdk/include/a2fo_module_api.h"
namespace a2fo {
// Early renderer admission uses the fixed startup snapshot when known; before
// deferred preparation it reads the same mod-root policy as the module loader.
bool startup_builtin_selected(BuiltinFeature feature, const std::string& data_root,
                              const std::string& command_line);
bool initialize_startup_builtin(BuiltinFeature feature, const A2FO_ModuleApi& api);
A2FO_ModuleShutdownFn startup_builtin_shutdown(BuiltinFeature feature) noexcept;
}
