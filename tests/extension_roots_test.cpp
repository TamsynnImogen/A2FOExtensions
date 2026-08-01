#include "extension_roots.hpp"

#include <cassert>

int main() {
    assert(a2fo::active_mod_from_command_line(
               "ArmadaL.exe /mod \"STA2 Classic Mod\"") ==
           "STA2 Classic Mod");
    assert(a2fo::active_mod_from_command_line(
               "ArmadaL.exe -MOD=Example") == "Example");
    assert(a2fo::parent_mod_from_info(
               "[other]\nParentMod=x\n[mod]\nParentMod = Base Mod\n") ==
           "Base Mod");

    const a2fo::FleetOpsInfoDefaults defaults =
        a2fo::fleet_ops_defaults_from_info(
            "[Mod]\nDefaultGameSpeed = 4\n"
            "SettingsDirectory = \"Shared Settings\"\n");
    assert(defaults.has_default_game_speed);
    assert(defaults.default_game_speed == 4);
    assert(defaults.settings_directory == "Shared Settings");
    assert(!a2fo::fleet_ops_defaults_from_info(
                "[mod]\nDefaultGameSpeed=0\n")
                .has_default_game_speed);
    assert(!a2fo::fleet_ops_defaults_from_info(
                "[mod]\nDefaultGameSpeed=7\n")
                .has_default_game_speed);
    assert(a2fo::resolve_fleet_ops_settings_directory(
               "C:\\Users\\Test\\Config\\mods\\Active Mod\\",
               "Shared Mod") ==
           "C:\\Users\\Test\\Config\\mods\\Shared Mod\\");
    assert(a2fo::resolve_fleet_ops_settings_directory(
               "C:\\Users\\Test\\Config\\mods\\Active Mod\\",
               "profiles\\Classic") ==
           "C:\\Users\\Test\\Config\\profiles\\Classic\\");
    assert(a2fo::resolve_fleet_ops_settings_directory(
               "C:\\Users\\Test\\Config\\mods\\Active Mod\\",
               "D:\\Portable\\A2") ==
           "D:\\Portable\\A2\\");
    assert(a2fo::resolve_fleet_ops_settings_directory(
               "", "Shared Mod", "C:\\Users\\Test\\Config") ==
           "C:\\Users\\Test\\Config\\mods\\Shared Mod\\");
    assert(a2fo::resolve_fleet_ops_settings_directory(
               "C:\\Users\\Test\\Config\\mods\\Active Mod\\",
               "Child Mod", "C:\\Users\\Test\\Config",
               "Z:\\Portable\\Fleet Ops Settings") ==
           "Z:\\Portable\\Fleet Ops Settings\\mods\\Child Mod\\");
    assert(a2fo::resolve_fleet_ops_settings_directory(
               "C:\\Users\\Test\\Config\\mods\\Active Mod\\",
               "profiles\\Explicit", "C:\\Users\\Test\\Config",
               "Z:\\Portable\\Fleet Ops Settings") ==
           "C:\\Users\\Test\\Config\\profiles\\Explicit\\");
    assert(a2fo::resolve_fleet_ops_settings_directory(
               "C:\\Users\\Test\\Config\\mods\\Active Mod\\",
               "D:\\Explicit\\Settings", "C:\\Users\\Test\\Config",
               "Z:\\Portable\\Fleet Ops Settings") ==
           "D:\\Explicit\\Settings\\");
    assert(a2fo::safe_mod_directory_name("STA2 Classic Mod"));
    assert(!a2fo::safe_mod_directory_name("../escape"));
    assert(!a2fo::safe_mod_directory_name("C:\\absolute"));
}
