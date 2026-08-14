/*
 * File: core/module_policy.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Per-mod native-module policy parsing, evaluation, and persistence.
 */

#pragma once

#include <string>
#include <vector>

namespace a2fo {

struct ModuleRules {
    bool has_section = false;
    std::vector<std::string> required;
    std::vector<std::string> rejected;
    std::vector<std::string> active;
    std::vector<std::string> diagnostics;
};

struct InstalledModule {
    std::string name;
    std::string filename;
    std::string path;
};

enum class ModulePolicyState {
    inactive,
    active,
    required,
    rejected,
    conflict,
    missing_active,
    missing_required,
};

struct ModulePolicyEntry {
    std::string key;
    std::string name;
    std::string filename;
    std::string path;
    ModulePolicyState state = ModulePolicyState::inactive;
    bool installed = false;
};

struct ModulePolicy {
    // False means no root declared a [modules] section. This legacy mode loads
    // every globally installed module so existing mods retain their behaviour.
    bool managed = false;
    bool valid = true;
    std::vector<ModulePolicyEntry> entries;
    std::vector<std::string> diagnostics;
};

// Module references are case-insensitive basenames. A trailing .dll is
// accepted in info.ini but is not part of the canonical key.
std::string normalize_module_name(const std::string& value);
std::string module_name_key(const std::string& value);

ModuleRules module_rules_from_info(const std::string& contents);

// Rules are ordered Data, oldest parent, ..., selected mod. Requirements and
// rejections accumulate across the chain; active choices come from the most
// specific root which has a [modules] section.
ModulePolicy evaluate_module_rules(
    const std::vector<ModuleRules>& ordered_rules,
    const std::vector<InstalledModule>& installed);

// Rewrites only activeX entries, preserving [mod], requiredX, rejectX,
// comments, and unrelated sections. A [modules] section is created if absent.
std::string rewrite_active_modules(
    const std::string& contents,
    const std::vector<std::string>& active_modules);

#if defined(_WIN32)
std::vector<InstalledModule> discover_installed_modules(
    const std::string& data_root,
    std::vector<std::string>* diagnostics = nullptr);

ModulePolicy evaluate_module_policy(
    const std::vector<std::string>& ordered_roots,
    const std::vector<InstalledModule>& installed);

bool save_active_module_selection(
    const std::string& info_path,
    const std::vector<std::string>& active_modules,
    std::string& error);
#endif

}  // namespace a2fo
