#pragma once

#include <string>
#include <vector>

namespace a2fo {

struct ExtensionRootDiscovery {
    std::vector<std::string> roots;
    std::vector<std::string> diagnostics;
    std::string active_mod;
};

struct FleetOpsInfoDefaults {
    bool has_default_game_speed = false;
    int default_game_speed = 0;
    std::string settings_directory;
};

// Extracts the value following Armada/Fleet Ops' /mod or -mod switch. Both a
// normal quoted value and the escaped quotes used by launcher shortcuts are
// accepted.
std::string active_mod_from_command_line(const std::string& command_line);

// Extracts ParentMod from the [mod] section of a Fleet Ops info.ini.
std::string parent_mod_from_info(const std::string& contents);

// Extracts A2FO's Fleet Ops compatibility defaults from [mod]. Invalid or
// out-of-range DefaultGameSpeed values are ignored; the supported slider range
// is 1 through 6.
FleetOpsInfoDefaults fleet_ops_defaults_from_info(
    const std::string& contents);

// Resolves SettingsDirectory against Fleet Ops' normal per-mod user path.
// A bare name is shorthand for mods\<name>. If the Data-level info.ini defines
// a shared settings root, a mod's bare name is placed below that root instead.
// Relative paths containing separators remain rooted at the normal Fleet Ops
// configuration directory, while absolute paths remain direct overrides.
std::string resolve_fleet_ops_settings_directory(
    const std::string& normal_mod_directory,
    const std::string& configured_directory,
    const std::string& fallback_config_root = {},
    const std::string& shared_settings_root = {});

// Mod names become directory components below Data\Mods, so traversal and
// absolute-path syntax are rejected before any filesystem access.
bool safe_mod_directory_name(const std::string& name);

#if defined(_WIN32)
// Returns extension roots in increasing precedence: Data, oldest parent, ...,
// selected mod. A later root therefore overrides an earlier file with the same
// basename.
ExtensionRootDiscovery discover_extension_roots(
    const std::string& data_root,
    const std::string& command_line);
#endif

}  // namespace a2fo
