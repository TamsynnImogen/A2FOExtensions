#pragma once

#include "builtin_state.hpp"
#include "module_policy.hpp"
#include "../sdk/include/a2fo_module_api.h"

#include <string>
#include <vector>

namespace a2fo {

// Code and registrations live for the process lifetime. Menu changes affect
// only atomic behaviour gates for live modules; startup-only modules retain
// their initial state and report pending restart. No hooks are unloaded.
bool builtin_module_enabled(BuiltinFeature feature) noexcept;
std::uint64_t builtin_module_activation(BuiltinFeature feature) noexcept;
bool prepare_builtin_module(BuiltinFeature feature, const A2FO_ModuleApi& api,
                            bool requested);
void finish_builtin_module_preparation(bool destroyed_dispatch_ready,
                                       bool weapon_dispatch_ready,
                                       bool weapon_class_dispatch_ready,
                                       bool info_ini_dispatch_ready,
                                       bool craft_dispatch_ready,
                                       bool class_dispatch_ready = false,
                                       bool race_dispatch_ready = false,
                                       bool producer_dispatch_ready = false);
void set_builtin_module_roots(const std::vector<std::string>& roots);
bool builtin_modules_match_roots(const std::vector<std::string>& roots);
bool apply_builtin_module_policy(const std::vector<std::string>& roots,
                                const ModulePolicy& policy,
                                std::string& error);
std::string builtin_module_status(const std::string& name,
                                 const std::vector<std::string>& roots);
bool builtin_module_available(BuiltinFeature feature) noexcept;
// Startup snapshot, independent of pending menu choices. False means unknown.
bool builtin_module_initial_request(BuiltinFeature feature, bool& requested) noexcept;

}  // namespace a2fo
