#include "../core/builtin_state.hpp"

#include <atomic>
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>

int main() {
    assert(std::string(a2fo::canonical_builtin_name(a2fo::BuiltinFeature::fire_arcs)) == "A2FOFireArcs");
    assert(std::string(a2fo::canonical_builtin_name(a2fo::BuiltinFeature::animations)) == "A2FOAnimations");
    assert(std::string(a2fo::canonical_builtin_name(a2fo::BuiltinFeature::always_show_shields)) == "A2FOAlwaysShowShields");
    assert(std::string(a2fo::canonical_builtin_name(a2fo::BuiltinFeature::station_rotation)) == "A2FOStationRotation");
    assert(std::string(a2fo::canonical_builtin_name(a2fo::BuiltinFeature::radar_diagnostics)) == "A2FORadarDiagnostics");
    assert(std::string(a2fo::canonical_builtin_name(a2fo::BuiltinFeature::recovery_tests)) == "A2FORecoveryTests");

    using a2fo::BuiltinFeature;
    using a2fo::BuiltinState;

    BuiltinFeature feature = BuiltinFeature::count;
    assert(std::strcmp(a2fo::canonical_builtin_name(
               BuiltinFeature::build_tooltips), "A2FOBuildTooltips") == 0);
    assert(std::strcmp(a2fo::canonical_builtin_name(
               BuiltinFeature::normal_weapon_tech), "A2FONormalWeaponTech") == 0);
    assert(std::strcmp(a2fo::canonical_builtin_name(
               BuiltinFeature::wreckage), "A2FOWreckage") == 0);
    assert(a2fo::find_builtin_feature("a2fobuildtooltips.DLL", feature));
    assert(feature == BuiltinFeature::build_tooltips);
    assert(a2fo::find_builtin_feature("A2FONormalWeaponTech", feature));
    assert(feature == BuiltinFeature::normal_weapon_tech);
    assert(a2fo::find_builtin_feature("a2fowreckage.dll", feature));
    assert(feature == BuiltinFeature::wreckage);
    assert(a2fo::find_builtin_feature("a2foinfoini.DLL", feature));
    assert(feature == BuiltinFeature::info_ini);
    assert(a2fo::find_builtin_feature("A2FOMuzzleFlashes", feature));
    assert(feature == BuiltinFeature::muzzle_flashes);
    assert(a2fo::find_builtin_feature("a2foteamchangeweapons.DLL", feature));
    assert(feature == BuiltinFeature::team_change_weapons);
    assert(a2fo::find_builtin_feature("A2FOWeaponDamageControls", feature));
    assert(feature == BuiltinFeature::weapon_damage_controls);
    assert(a2fo::find_builtin_feature("a1fallbacks.DLL", feature));
    assert(feature == BuiltinFeature::a1_fallbacks);
    assert(a2fo::find_builtin_feature("A2FOAnimatedHardpoints", feature));
    assert(feature == BuiltinFeature::animated_hardpoints);
    assert(a2fo::find_builtin_feature("a2focameracontrols.DLL", feature));
    assert(feature == BuiltinFeature::camera_controls);
    assert(a2fo::find_builtin_feature("A2FOViewDistance", feature));
    assert(feature == BuiltinFeature::view_distance);
    assert(a2fo::find_builtin_feature("a2foinstantactionsettings.DLL", feature));
    assert(feature == BuiltinFeature::instant_action_settings);
    assert(a2fo::find_builtin_feature("a2foeditmenu.DLL", feature));
    assert(feature == BuiltinFeature::edit_menu);
    assert(a2fo::find_builtin_feature("a2fom3d.DLL", feature));
    assert(feature == BuiltinFeature::m3d);
    assert(a2fo::find_builtin_feature("a2fobreeders.DLL", feature));
    assert(feature == BuiltinFeature::breeders);
    assert(a2fo::find_builtin_feature("a2foinfestation.DLL", feature));
    assert(feature == BuiltinFeature::infestation);
    assert(a2fo::find_builtin_feature("a2foswarmsystem.DLL", feature));
    assert(feature == BuiltinFeature::swarm_system);
    assert(a2fo::find_builtin_feature("a2foodfvariants.DLL", feature));
    assert(feature == BuiltinFeature::odf_variants);
    for (int index = 0; index < static_cast<int>(BuiltinFeature::count); ++index) {
        const auto expected = static_cast<BuiltinFeature>(index);
        assert(a2fo::find_builtin_feature(a2fo::canonical_builtin_name(expected), feature));
        assert(feature == expected);
    }
    for (const char* invalid : {"", "Unknown.dll", "../A2FOWreckage.dll",
                                "dir/A2FOWreckage.dll", "dir\\A2FOWreckage.dll",
                                "C:A2FOWreckage.dll", "A2FOWreckage.dll.dll"}) {
        feature = BuiltinFeature::count;
        assert(!a2fo::find_builtin_feature(invalid, feature));
        assert(feature == BuiltinFeature::count);
    }

    const std::vector<std::string> active = {
        "C:\\Fleet Ops\\Data", "C:\\Fleet Ops\\Data\\Mods\\Parent",
        "C:\\Fleet Ops\\Data\\Mods\\Current"};
    assert(a2fo::same_module_roots(
        active, {"c:/fleet ops/data/", "c:/fleet ops/data/mods/parent\\",
                 "c:/fleet ops/data/mods/current///"}));
    assert(!a2fo::same_module_roots(active, {"C:/Fleet Ops/Data"}));
    assert(!a2fo::same_module_roots(
        active, {"C:/Fleet Ops/Data", "C:/Fleet Ops/Data/Mods/Parent",
                 "C:/Fleet Ops/Data/Mods/Other"}));
    assert(!a2fo::same_module_roots(
        active, {"C:/Fleet Ops/Data", "", "C:/Fleet Ops/Data/Mods/Current"}));
    assert(!a2fo::same_module_roots({}, {}));

    BuiltinState state;
    assert(state.activation() == 0);
    assert(!state.is_prepared() && !state.available() && !state.requested() &&
           !state.enabled());
    assert(!state.request(true));
    assert(state.request(false));
    state.prepared(true, false);
    assert(state.is_prepared() && state.available() && !state.enabled());
    assert(state.request(true) && state.enabled() && state.requested());
    const auto first_activation = state.activation();
    assert(first_activation != 0);
    assert(state.request(true) && state.activation() == first_activation);
    assert(state.request(false) && !state.enabled() && !state.requested());
    assert(state.activation() == 0);
    assert(state.request(true) && state.enabled());
    assert(state.activation() != 0 && state.activation() != first_activation);
    state.prepared(false, false);
    assert(state.available() && state.requested() && state.enabled());

    BuiltinState missing;
    missing.prepared(false, true);
    assert(missing.is_prepared() && !missing.available() &&
           missing.requested() && !missing.enabled());
    assert(!missing.request(true));
    assert(missing.activation() == 0);
    assert(missing.request(false) && !missing.requested());
    missing.prepared(true, true);
    assert(!missing.available() && !missing.enabled() && !missing.requested());

    std::atomic<bool> stop{false};
    std::atomic<unsigned> reads{0};
    std::thread reader([&] {
        while (!stop.load(std::memory_order_acquire)) {
            // Each accessor is a valid atomic snapshot while writes race.
            (void)state.enabled();
            (void)state.requested();
            ++reads;
        }
    });
    while (reads.load(std::memory_order_acquire) == 0) {
        std::this_thread::yield();
    }
    for (int i = 0; i < 100001; ++i) {
        assert(state.request((i & 1) != 0));
    }
    stop.store(true, std::memory_order_release);
    reader.join();
    assert(reads.load() > 0);
    assert(!state.enabled() && !state.requested());
}
