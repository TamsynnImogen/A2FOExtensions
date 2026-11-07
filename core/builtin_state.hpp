/* Portable state and identity policy for process-local builtin modules. */
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo {

enum class BuiltinFeature {
    build_tooltips,
    normal_weapon_tech,
    wreckage,
    info_ini,
    muzzle_flashes,
    team_change_weapons,
    weapon_damage_controls,
    a1_fallbacks,
    animated_hardpoints,
    camera_controls,
    view_distance,
    instant_action_settings,
    edit_menu,
    m3d,
    breeders,
    infestation,
    swarm_system,
    odf_variants,
    fire_arcs,
    animations,
    always_show_shields,
    station_rotation,
    radar_diagnostics,
    recovery_tests,
    cheats,
    collective_xp,
    directional_shields,
    energy_systems,
    hybrid_build,
    multi_yard_build,
    nebula_renderer,
    point_defense_cycles,
    refit_yards,
    turrets,
    rgb_textures,
    texture_variants,
    a1_compat,
    craft_identity,
    feature_pack,
    log_mailer,
    mission_selector,
    missions,
    refit_menus,
    resources,
    sol_maps,
    shell_display,
    squadrons,
    video,
    count,
};

// These modules retain startup selection; native hooks are never toggled live.
bool builtin_requires_restart(BuiltinFeature feature) noexcept;

const char* canonical_builtin_name(BuiltinFeature feature) noexcept;
bool find_builtin_feature(const std::string& name,
                          BuiltinFeature& feature) noexcept;
bool same_module_roots(const std::vector<std::string>& active,
                       const std::vector<std::string>& selected) noexcept;

class BuiltinState {
public:
    BuiltinState() noexcept = default;
    BuiltinState(const BuiltinState&) = delete;
    BuiltinState& operator=(const BuiltinState&) = delete;
    // First preparation fixes availability for this instance's lifetime.
    void prepared(bool available, bool requested) noexcept;
    // Disabling is always accepted; enabling requires available preparation.
    bool request(bool enabled) noexcept;
    bool enabled() const noexcept;
    bool available() const noexcept;
    bool is_prepared() const noexcept;
    bool requested() const noexcept;
    // Nonzero only while enabled; changes across every off/on cycle so queued
    // actions cannot survive a policy change, even without a simulation tick.
    std::uint64_t activation() const noexcept;

private:
    // Rust exclusively performs atomic access after publication. Explicit
    // alignment satisfies AtomicU64 on both i686 and the host test target.
    alignas(8) mutable std::uint64_t flags_{0};
};

}  // namespace a2fo
