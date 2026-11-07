# Pure-Rust completion: source and target checklist

Snapshot: 29 October 2026. Companion to [the completion plan](pure-rust-completion-plan.md) and [machine inventory](pure-rust-completion-inventory.json).

**Every checkbox below is open for full-product closure.** Existing portable Rust implementations and bounded owners are credited separately; a native source can contain both retained implementation and forwarding code. Textual include discovery is conservative and does not evaluate preprocessor conditions. Headers with executable code, macro-generated paths, target-specific branches and archive members still require the P00/P16/P17 audits.

The historical stage-16 main DLL graph supplies 448 direct C/C++ compilation units (401 C++, 47 C), 44 Rust bridge objects and 2 resource objects. Current source pins in the JSON are a separate snapshot. Supplied linker arguments are not surviving-section counts. The discovered extra implementation paths are 53 included .cpp files and 21 .inl files; do not add those to the translation-unit count.

## Direct main-DLL C/C++ compilation units

### core: 10

- [ ] [core/dllmain.cpp](../core/dllmain.cpp)
- [ ] [core/extension_roots.cpp](../core/extension_roots.cpp)
- [ ] [core/fpq_paths.cpp](../core/fpq_paths.cpp)
- [ ] [core/game_monitor.cpp](../core/game_monitor.cpp)
- [ ] [core/game_monitor_policy.cpp](../core/game_monitor_policy.cpp)
- [ ] [core/module_loader.cpp](../core/module_loader.cpp)
- [ ] [core/module_menu.cpp](../core/module_menu.cpp)
- [ ] [core/module_policy.cpp](../core/module_policy.cpp)
- [ ] [core/nebula_emissive.cpp](../core/nebula_emissive.cpp)
- [ ] [core/odf_paths.cpp](../core/odf_paths.cpp)

### module: 115

- [ ] [core/builtin_a1_compat.cpp](../core/builtin_a1_compat.cpp)
- [ ] [core/builtin_a1_fallbacks.cpp](../core/builtin_a1_fallbacks.cpp)
- [ ] [core/builtin_always_shields.cpp](../core/builtin_always_shields.cpp)
- [ ] [core/builtin_animated_hardpoints.cpp](../core/builtin_animated_hardpoints.cpp)
- [ ] [core/builtin_animations.cpp](../core/builtin_animations.cpp)
- [ ] [core/builtin_breeders.cpp](../core/builtin_breeders.cpp)
- [ ] [core/builtin_build_tooltips.cpp](../core/builtin_build_tooltips.cpp)
- [ ] [core/builtin_camera_controls.cpp](../core/builtin_camera_controls.cpp)
- [ ] [core/builtin_cheats.cpp](../core/builtin_cheats.cpp)
- [ ] [core/builtin_collective_xp.cpp](../core/builtin_collective_xp.cpp)
- [ ] [core/builtin_craft_identity.cpp](../core/builtin_craft_identity.cpp)
- [ ] [core/builtin_directional_shields.cpp](../core/builtin_directional_shields.cpp)
- [ ] [core/builtin_edit_menu.cpp](../core/builtin_edit_menu.cpp)
- [ ] [core/builtin_energy_systems.cpp](../core/builtin_energy_systems.cpp)
- [ ] [core/builtin_feature_pack.cpp](../core/builtin_feature_pack.cpp)
- [ ] [core/builtin_feature_pack_buildyard.cpp](../core/builtin_feature_pack_buildyard.cpp)
- [ ] [core/builtin_feature_pack_hybrid_client.cpp](../core/builtin_feature_pack_hybrid_client.cpp)
- [ ] [core/builtin_feature_pack_queue.cpp](../core/builtin_feature_pack_queue.cpp)
- [ ] [core/builtin_feature_pack_upgrade_pods.cpp](../core/builtin_feature_pack_upgrade_pods.cpp)
- [ ] [core/builtin_fire_arcs.cpp](../core/builtin_fire_arcs.cpp)
- [ ] [core/builtin_hybrid_build.cpp](../core/builtin_hybrid_build.cpp)
- [ ] [core/builtin_hybrid_runtime.cpp](../core/builtin_hybrid_runtime.cpp)
- [ ] [core/builtin_infestation.cpp](../core/builtin_infestation.cpp)
- [ ] [core/builtin_info_ini.cpp](../core/builtin_info_ini.cpp)
- [ ] [core/builtin_instant_action_settings.cpp](../core/builtin_instant_action_settings.cpp)
- [ ] [core/builtin_log_mailer.cpp](../core/builtin_log_mailer.cpp)
- [ ] [core/builtin_m3d.cpp](../core/builtin_m3d.cpp)
- [ ] [core/builtin_mission_selector.cpp](../core/builtin_mission_selector.cpp)
- [ ] [core/builtin_modules.cpp](../core/builtin_modules.cpp)
- [ ] [core/builtin_multi_yard_build.cpp](../core/builtin_multi_yard_build.cpp)
- [ ] [core/builtin_muzzle_flashes.cpp](../core/builtin_muzzle_flashes.cpp)
- [ ] [core/builtin_nebula_renderer.cpp](../core/builtin_nebula_renderer.cpp)
- [ ] [core/builtin_normal_weapon_tech.cpp](../core/builtin_normal_weapon_tech.cpp)
- [ ] [core/builtin_odf_variants.cpp](../core/builtin_odf_variants.cpp)
- [ ] [core/builtin_point_defense_cycles.cpp](../core/builtin_point_defense_cycles.cpp)
- [ ] [core/builtin_radar_diagnostics.cpp](../core/builtin_radar_diagnostics.cpp)
- [ ] [core/builtin_recovery_tests.cpp](../core/builtin_recovery_tests.cpp)
- [ ] [core/builtin_refit_menus.cpp](../core/builtin_refit_menus.cpp)
- [ ] [core/builtin_refit_yards.cpp](../core/builtin_refit_yards.cpp)
- [ ] [core/builtin_resources.cpp](../core/builtin_resources.cpp)
- [ ] [core/builtin_rgb_textures.cpp](../core/builtin_rgb_textures.cpp)
- [ ] [core/builtin_shell_display.cpp](../core/builtin_shell_display.cpp)
- [ ] [core/builtin_sol_maps.cpp](../core/builtin_sol_maps.cpp)
- [ ] [core/builtin_squadrons.cpp](../core/builtin_squadrons.cpp)
- [ ] [core/builtin_startup_modules.cpp](../core/builtin_startup_modules.cpp)
- [ ] [core/builtin_startup_policy.cpp](../core/builtin_startup_policy.cpp)
- [ ] [core/builtin_state.cpp](../core/builtin_state.cpp)
- [ ] [core/builtin_station_rotation.cpp](../core/builtin_station_rotation.cpp)
- [ ] [core/builtin_swarm_system.cpp](../core/builtin_swarm_system.cpp)
- [ ] [core/builtin_team_change_weapons.cpp](../core/builtin_team_change_weapons.cpp)
- [ ] [core/builtin_texture_variants.cpp](../core/builtin_texture_variants.cpp)
- [ ] [core/builtin_turrets.cpp](../core/builtin_turrets.cpp)
- [ ] [core/builtin_video.cpp](../core/builtin_video.cpp)
- [ ] [core/builtin_view_distance.cpp](../core/builtin_view_distance.cpp)
- [ ] [core/builtin_weapon_damage_controls.cpp](../core/builtin_weapon_damage_controls.cpp)
- [ ] [core/builtin_wreckage.cpp](../core/builtin_wreckage.cpp)
- [ ] [modules/A1Compat/a1_bzn_policy.cpp](../modules/A1Compat/a1_bzn_policy.cpp)
- [ ] [modules/A1Compat/race_menu_policy.cpp](../modules/A1Compat/race_menu_policy.cpp)
- [ ] [modules/A1Compat/team_color_policy.cpp](../modules/A1Compat/team_color_policy.cpp)
- [ ] [modules/A1Fallbacks/fallback_policy.cpp](../modules/A1Fallbacks/fallback_policy.cpp)
- [ ] [modules/A2FOAlwaysShowShields/shield_visibility.cpp](../modules/A2FOAlwaysShowShields/shield_visibility.cpp)
- [ ] [modules/A2FOBuildTooltips/build_time_text.cpp](../modules/A2FOBuildTooltips/build_time_text.cpp)
- [ ] [modules/A2FOCollectiveXP/collective_xp.cpp](../modules/A2FOCollectiveXP/collective_xp.cpp)
- [ ] [modules/A2FOCraftIdentity/directional_shield_display_config.cpp](../modules/A2FOCraftIdentity/directional_shield_display_config.cpp)
- [ ] [modules/A2FOCraftIdentity/directional_shield_fill.cpp](../modules/A2FOCraftIdentity/directional_shield_fill.cpp)
- [ ] [modules/A2FOCraftIdentity/extended_weapon_icons.cpp](../modules/A2FOCraftIdentity/extended_weapon_icons.cpp)
- [ ] [modules/A2FOCraftIdentity/fleetops_object_editor.cpp](../modules/A2FOCraftIdentity/fleetops_object_editor.cpp)
- [ ] [modules/A2FOCraftIdentity/identity_selection.cpp](../modules/A2FOCraftIdentity/identity_selection.cpp)
- [ ] [modules/A2FOCraftIdentity/object_editor.cpp](../modules/A2FOCraftIdentity/object_editor.cpp)
- [ ] [modules/A2FOCraftIdentity/object_editor_state.cpp](../modules/A2FOCraftIdentity/object_editor_state.cpp)
- [ ] [modules/A2FOCraftIdentity/system_icon_state.cpp](../modules/A2FOCraftIdentity/system_icon_state.cpp)
- [ ] [modules/A2FODirectionalShields/directional_shields.cpp](../modules/A2FODirectionalShields/directional_shields.cpp)
- [ ] [modules/A2FOEditMenu/edit_menu_odf.cpp](../modules/A2FOEditMenu/edit_menu_odf.cpp)
- [ ] [modules/A2FOEnergySystems/energy_systems.cpp](../modules/A2FOEnergySystems/energy_systems.cpp)
- [ ] [modules/A2FOEnergySystems/resupply_policy.cpp](../modules/A2FOEnergySystems/resupply_policy.cpp)
- [ ] [modules/A2FOFeaturePack/refit_queue_bridge_client.cpp](../modules/A2FOFeaturePack/refit_queue_bridge_client.cpp)
- [ ] [modules/A2FOFeaturePack/upgrade_pod_config.cpp](../modules/A2FOFeaturePack/upgrade_pod_config.cpp)
- [ ] [modules/A2FOFireArcs/fire_arc.cpp](../modules/A2FOFireArcs/fire_arc.cpp)
- [ ] [modules/A2FOFireArcs/runtime_config.cpp](../modules/A2FOFireArcs/runtime_config.cpp)
- [ ] [modules/A2FOHybridBuild/build_submenu_config.cpp](../modules/A2FOHybridBuild/build_submenu_config.cpp)
- [ ] [modules/A2FOHybridBuild/hybrid_production.cpp](../modules/A2FOHybridBuild/hybrid_production.cpp)
- [ ] [modules/A2FOInstantActionSettings/load_button_bounds.cpp](../modules/A2FOInstantActionSettings/load_button_bounds.cpp)
- [ ] [modules/A2FOInstantActionSettings/setup_details_line.cpp](../modules/A2FOInstantActionSettings/setup_details_line.cpp)
- [ ] [modules/A2FOM3D/m3d_conversion.cpp](../modules/A2FOM3D/m3d_conversion.cpp)
- [ ] [modules/A2FONebulaRenderer/art_texture_suffix_config.cpp](../modules/A2FONebulaRenderer/art_texture_suffix_config.cpp)
- [ ] [modules/A2FOPointDefenseCycles/firing_cycle.cpp](../modules/A2FOPointDefenseCycles/firing_cycle.cpp)
- [ ] [modules/A2FORecoveryTests/recovery_tests.cpp](../modules/A2FORecoveryTests/recovery_tests.cpp)
- [ ] [modules/A2FORefitMenus/loading_content.cpp](../modules/A2FORefitMenus/loading_content.cpp)
- [ ] [modules/A2FORefitMenus/loading_gpu.cpp](../modules/A2FORefitMenus/loading_gpu.cpp)
- [ ] [modules/A2FORefitMenus/loading_native.cpp](../modules/A2FORefitMenus/loading_native.cpp)
- [ ] [modules/A2FORefitMenus/loading_presentation.cpp](../modules/A2FORefitMenus/loading_presentation.cpp)
- [ ] [modules/A2FORefitMenus/loading_theme.cpp](../modules/A2FORefitMenus/loading_theme.cpp)
- [ ] [modules/A2FORefitMenus/menu_scene.cpp](../modules/A2FORefitMenus/menu_scene.cpp)
- [ ] [modules/A2FORefitMenus/mission_backdrop.cpp](../modules/A2FORefitMenus/mission_backdrop.cpp)
- [ ] [modules/A2FORefitMenus/presentation.cpp](../modules/A2FORefitMenus/presentation.cpp)
- [ ] [modules/A2FORefitMenus/scene_catalogue.cpp](../modules/A2FORefitMenus/scene_catalogue.cpp)
- [ ] [modules/A2FORefitMenus/scene_composition.cpp](../modules/A2FORefitMenus/scene_composition.cpp)
- [ ] [modules/A2FORefitMenus/setup_native.cpp](../modules/A2FORefitMenus/setup_native.cpp)
- [ ] [modules/A2FORefitMenus/setup_presentation.cpp](../modules/A2FORefitMenus/setup_presentation.cpp)
- [ ] [modules/A2FORefitMenus/setup_skin.cpp](../modules/A2FORefitMenus/setup_skin.cpp)
- [ ] [modules/A2FORefitMenus/skin.cpp](../modules/A2FORefitMenus/skin.cpp)
- [ ] [modules/A2FORefitMenus/skin_native.cpp](../modules/A2FORefitMenus/skin_native.cpp)
- [ ] [modules/A2FORefitYards/refit_policy.cpp](../modules/A2FORefitYards/refit_policy.cpp)
- [ ] [modules/A2FOResources/additional_resources.cpp](../modules/A2FOResources/additional_resources.cpp)
- [ ] [modules/A2FOSOLMaps/sol_format.cpp](../modules/A2FOSOLMaps/sol_format.cpp)
- [ ] [modules/A2FOShellDisplay/bink_video.cpp](../modules/A2FOShellDisplay/bink_video.cpp)
- [ ] [modules/A2FOShellDisplay/canvas.cpp](../modules/A2FOShellDisplay/canvas.cpp)
- [ ] [modules/A2FOShellDisplay/native_intro.cpp](../modules/A2FOShellDisplay/native_intro.cpp)
- [ ] [modules/A2FOSquadrons/squadron_config.cpp](../modules/A2FOSquadrons/squadron_config.cpp)
- [ ] [modules/A2FOSquadrons/squadron_state.cpp](../modules/A2FOSquadrons/squadron_state.cpp)
- [ ] [modules/A2FOSwarmSystem/swarm_motion.cpp](../modules/A2FOSwarmSystem/swarm_motion.cpp)
- [ ] [modules/A2FOTextureVariants/texture_variants.cpp](../modules/A2FOTextureVariants/texture_variants.cpp)
- [ ] [modules/A2FOTurrets/turret_combat.cpp](../modules/A2FOTurrets/turret_combat.cpp)
- [ ] [modules/A2FOTurrets/turret_math.cpp](../modules/A2FOTurrets/turret_math.cpp)
- [ ] [modules/A2FOWreckage/wreckage_policy.cpp](../modules/A2FOWreckage/wreckage_policy.cpp)

### mmm_module: 1

- [ ] [core/builtin_missions.cpp](../core/builtin_missions.cpp)

### mmm_mmm_armada: 217

- [ ] [third_party/mmm/source/mmm_armada/AntimatterMine.cpp](../third_party/mmm/source/mmm_armada/AntimatterMine.cpp)
- [ ] [third_party/mmm/source/mmm_armada/AntimatterMineBinding.cpp](../third_party/mmm/source/mmm_armada/AntimatterMineBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Application.cpp](../third_party/mmm/source/mmm_armada/Application.cpp)
- [ ] [third_party/mmm/source/mmm_armada/AreaEffectObjectBinding.cpp](../third_party/mmm/source/mmm_armada/AreaEffectObjectBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/AreaEffectObjectClassBinding.cpp](../third_party/mmm/source/mmm_armada/AreaEffectObjectClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/AreaEffectObjectClass_Internal.cpp](../third_party/mmm/source/mmm_armada/AreaEffectObjectClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/AreaEffectObject_Internal.cpp](../third_party/mmm/source/mmm_armada/AreaEffectObject_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Artillery.cpp](../third_party/mmm/source/mmm_armada/Artillery.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ArtilleryBinding.cpp](../third_party/mmm/source/mmm_armada/ArtilleryBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/AsteroidField.cpp](../third_party/mmm/source/mmm_armada/AsteroidField.cpp)
- [ ] [third_party/mmm/source/mmm_armada/AsteroidFieldBinding.cpp](../third_party/mmm/source/mmm_armada/AsteroidFieldBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundObjectBinding.cpp](../third_party/mmm/source/mmm_armada/BackgroundObjectBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundObjectClassBinding.cpp](../third_party/mmm/source/mmm_armada/BackgroundObjectClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundObjectClass_Internal.cpp](../third_party/mmm/source/mmm_armada/BackgroundObjectClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundObject_Internal.cpp](../third_party/mmm/source/mmm_armada/BackgroundObject_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundPlanetBinding.cpp](../third_party/mmm/source/mmm_armada/BackgroundPlanetBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundPlanetClassBinding.cpp](../third_party/mmm/source/mmm_armada/BackgroundPlanetClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundPlanetClass_Internal.cpp](../third_party/mmm/source/mmm_armada/BackgroundPlanetClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BackgroundPlanet_Internal.cpp](../third_party/mmm/source/mmm_armada/BackgroundPlanet_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BeamBinding.cpp](../third_party/mmm/source/mmm_armada/BeamBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BeamToggle.cpp](../third_party/mmm/source/mmm_armada/BeamToggle.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BeamToggleBinding.cpp](../third_party/mmm/source/mmm_armada/BeamToggleBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Beam_Internal.cpp](../third_party/mmm/source/mmm_armada/Beam_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BlackHoleBinding.cpp](../third_party/mmm/source/mmm_armada/BlackHoleBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BlackHoleClassBinding.cpp](../third_party/mmm/source/mmm_armada/BlackHoleClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BlackHoleClass_Internal.cpp](../third_party/mmm/source/mmm_armada/BlackHoleClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BlackHole_Internal.cpp](../third_party/mmm/source/mmm_armada/BlackHole_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BuildQueue.cpp](../third_party/mmm/source/mmm_armada/BuildQueue.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BuildQueueBinding.cpp](../third_party/mmm/source/mmm_armada/BuildQueueBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Bullet.cpp](../third_party/mmm/source/mmm_armada/Bullet.cpp)
- [ ] [third_party/mmm/source/mmm_armada/BulletBinding.cpp](../third_party/mmm/source/mmm_armada/BulletBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CameraBinding.cpp](../third_party/mmm/source/mmm_armada/CameraBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Camera_Internal.cpp](../third_party/mmm/source/mmm_armada/Camera_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CargoShipBinding.cpp](../third_party/mmm/source/mmm_armada/CargoShipBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CargoShip_Internal.cpp](../third_party/mmm/source/mmm_armada/CargoShip_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CineractiveBinding.cpp](../third_party/mmm/source/mmm_armada/CineractiveBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Cineractive_Internal.cpp](../third_party/mmm/source/mmm_armada/Cineractive_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ColonyBinding.cpp](../third_party/mmm/source/mmm_armada/ColonyBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ColonyClassBinding.cpp](../third_party/mmm/source/mmm_armada/ColonyClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ColonyClass_Internal.cpp](../third_party/mmm/source/mmm_armada/ColonyClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Colony_Internal.cpp](../third_party/mmm/source/mmm_armada/Colony_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ConstructionObjectBinding.cpp](../third_party/mmm/source/mmm_armada/ConstructionObjectBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ConstructionObject_Internal.cpp](../third_party/mmm/source/mmm_armada/ConstructionObject_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ConstructionRigBinding.cpp](../third_party/mmm/source/mmm_armada/ConstructionRigBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ConstructionRig_Internal.cpp](../third_party/mmm/source/mmm_armada/ConstructionRig_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Container.cpp](../third_party/mmm/source/mmm_armada/Container.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CraftBinding.cpp](../third_party/mmm/source/mmm_armada/CraftBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CraftClassBinding.cpp](../third_party/mmm/source/mmm_armada/CraftClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CraftClass_Internal.cpp](../third_party/mmm/source/mmm_armada/CraftClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CraftSystemBinding.cpp](../third_party/mmm/source/mmm_armada/CraftSystemBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/CraftSystem_Internal.cpp](../third_party/mmm/source/mmm_armada/CraftSystem_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Craft_Internal.cpp](../third_party/mmm/source/mmm_armada/Craft_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/DebugBinding.cpp](../third_party/mmm/source/mmm_armada/DebugBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Debug_Internal.cpp](../third_party/mmm/source/mmm_armada/Debug_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/DebuggerConnection.cpp](../third_party/mmm/source/mmm_armada/DebuggerConnection.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Entities_Internal.cpp](../third_party/mmm/source/mmm_armada/Entities_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityBinding.cpp](../third_party/mmm/source/mmm_armada/EntityBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityFinder.cpp](../third_party/mmm/source/mmm_armada/EntityFinder.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityFinderBinding.cpp](../third_party/mmm/source/mmm_armada/EntityFinderBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityFinderFilter.cpp](../third_party/mmm/source/mmm_armada/EntityFinderFilter.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityFinderFilterFunction.cpp](../third_party/mmm/source/mmm_armada/EntityFinderFilterFunction.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityFinderFilterObject.cpp](../third_party/mmm/source/mmm_armada/EntityFinderFilterObject.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityHook.cpp](../third_party/mmm/source/mmm_armada/EntityHook.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EntityReference.cpp](../third_party/mmm/source/mmm_armada/EntityReference.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Entity_Internal.cpp](../third_party/mmm/source/mmm_armada/Entity_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EvolverBinding.cpp](../third_party/mmm/source/mmm_armada/EvolverBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EvolverClassBinding.cpp](../third_party/mmm/source/mmm_armada/EvolverClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/EvolverClass_Internal.cpp](../third_party/mmm/source/mmm_armada/EvolverClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Evolver_Internal.cpp](../third_party/mmm/source/mmm_armada/Evolver_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ExplosionBinding.cpp](../third_party/mmm/source/mmm_armada/ExplosionBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Explosion_Internal.cpp](../third_party/mmm/source/mmm_armada/Explosion_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/FreighterBinding.cpp](../third_party/mmm/source/mmm_armada/FreighterBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Freighter_Internal.cpp](../third_party/mmm/source/mmm_armada/Freighter_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Functions_ItemCache.cpp](../third_party/mmm/source/mmm_armada/Functions_ItemCache.cpp)
- [ ] [third_party/mmm/source/mmm_armada/GameBinding.cpp](../third_party/mmm/source/mmm_armada/GameBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/GameObjectBinding.cpp](../third_party/mmm/source/mmm_armada/GameObjectBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/GameObjectClassBinding.cpp](../third_party/mmm/source/mmm_armada/GameObjectClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/GameObjectClass_Internal.cpp](../third_party/mmm/source/mmm_armada/GameObjectClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/GameObject_Internal.cpp](../third_party/mmm/source/mmm_armada/GameObject_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Game_Internal.cpp](../third_party/mmm/source/mmm_armada/Game_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Globals.cpp](../third_party/mmm/source/mmm_armada/Globals.cpp)
- [ ] [third_party/mmm/source/mmm_armada/GravityMine.cpp](../third_party/mmm/source/mmm_armada/GravityMine.cpp)
- [ ] [third_party/mmm/source/mmm_armada/GravityMineBinding.cpp](../third_party/mmm/source/mmm_armada/GravityMineBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Hook.cpp](../third_party/mmm/source/mmm_armada/Hook.cpp)
- [ ] [third_party/mmm/source/mmm_armada/IonStormBinding.cpp](../third_party/mmm/source/mmm_armada/IonStormBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/IonStorm_Internal.cpp](../third_party/mmm/source/mmm_armada/IonStorm_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/LatinumNebulaBinding.cpp](../third_party/mmm/source/mmm_armada/LatinumNebulaBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/LatinumNebulaClassBinding.cpp](../third_party/mmm/source/mmm_armada/LatinumNebulaClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/LatinumNebulaClass_Internal.cpp](../third_party/mmm/source/mmm_armada/LatinumNebulaClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/LatinumNebula_Internal.cpp](../third_party/mmm/source/mmm_armada/LatinumNebula_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/LoadBinding.cpp](../third_party/mmm/source/mmm_armada/LoadBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Load_Internal.cpp](../third_party/mmm/source/mmm_armada/Load_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/LuaBinding.cpp](../third_party/mmm/source/mmm_armada/LuaBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MapBinding.cpp](../third_party/mmm/source/mmm_armada/MapBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Map_Internal.cpp](../third_party/mmm/source/mmm_armada/Map_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Matrix.cpp](../third_party/mmm/source/mmm_armada/Matrix.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MatrixBinding.cpp](../third_party/mmm/source/mmm_armada/MatrixBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MediaBinding.cpp](../third_party/mmm/source/mmm_armada/MediaBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Media_Internal.cpp](../third_party/mmm/source/mmm_armada/Media_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MemRaider.cpp](../third_party/mmm/source/mmm_armada/MemRaider.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Mine.cpp](../third_party/mmm/source/mmm_armada/Mine.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MineArray.cpp](../third_party/mmm/source/mmm_armada/MineArray.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MineArrayBinding.cpp](../third_party/mmm/source/mmm_armada/MineArrayBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MineBinding.cpp](../third_party/mmm/source/mmm_armada/MineBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MiningStation.cpp](../third_party/mmm/source/mmm_armada/MiningStation.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MiningStationBinding.cpp](../third_party/mmm/source/mmm_armada/MiningStationBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MiningStationClassBinding.cpp](../third_party/mmm/source/mmm_armada/MiningStationClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MiningStationClass_Internal.cpp](../third_party/mmm/source/mmm_armada/MiningStationClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MissileBinding.cpp](../third_party/mmm/source/mmm_armada/MissileBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Missile_Internal.cpp](../third_party/mmm/source/mmm_armada/Missile_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MissionBinding.cpp](../third_party/mmm/source/mmm_armada/MissionBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Mission_Internal.cpp](../third_party/mmm/source/mmm_armada/Mission_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Monitor.cpp](../third_party/mmm/source/mmm_armada/Monitor.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MonitorHook.cpp](../third_party/mmm/source/mmm_armada/MonitorHook.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MonitorWrapper.cpp](../third_party/mmm/source/mmm_armada/MonitorWrapper.cpp)
- [ ] [third_party/mmm/source/mmm_armada/MonitorWrapperBinding.cpp](../third_party/mmm/source/mmm_armada/MonitorWrapperBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/NebulaBinding.cpp](../third_party/mmm/source/mmm_armada/NebulaBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Nebula_Internal.cpp](../third_party/mmm/source/mmm_armada/Nebula_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Objective.cpp](../third_party/mmm/source/mmm_armada/Objective.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ObjectivesBinding.cpp](../third_party/mmm/source/mmm_armada/ObjectivesBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Objectives_Internal.cpp](../third_party/mmm/source/mmm_armada/Objectives_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Ordnance.cpp](../third_party/mmm/source/mmm_armada/Ordnance.cpp)
- [ ] [third_party/mmm/source/mmm_armada/OrdnanceBinding.cpp](../third_party/mmm/source/mmm_armada/OrdnanceBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/OrdnanceDamage.cpp](../third_party/mmm/source/mmm_armada/OrdnanceDamage.cpp)
- [ ] [third_party/mmm/source/mmm_armada/OrdnanceDamageBinding.cpp](../third_party/mmm/source/mmm_armada/OrdnanceDamageBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/OrientedQueue.cpp](../third_party/mmm/source/mmm_armada/OrientedQueue.cpp)
- [ ] [third_party/mmm/source/mmm_armada/OrientedQueueBinding.cpp](../third_party/mmm/source/mmm_armada/OrientedQueueBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PathBinding.cpp](../third_party/mmm/source/mmm_armada/PathBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Path_Internal.cpp](../third_party/mmm/source/mmm_armada/Path_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PhaserBinding.cpp](../third_party/mmm/source/mmm_armada/PhaserBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Phaser_Internal.cpp](../third_party/mmm/source/mmm_armada/Phaser_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PhotonTorpedoBinding.cpp](../third_party/mmm/source/mmm_armada/PhotonTorpedoBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PhotonTorpedo_Internal.cpp](../third_party/mmm/source/mmm_armada/PhotonTorpedo_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PlanetBinding.cpp](../third_party/mmm/source/mmm_armada/PlanetBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PlanetMiningBase.cpp](../third_party/mmm/source/mmm_armada/PlanetMiningBase.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PlanetMiningBaseBinding.cpp](../third_party/mmm/source/mmm_armada/PlanetMiningBaseBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Planet_Internal.cpp](../third_party/mmm/source/mmm_armada/Planet_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PrintMessages.cpp](../third_party/mmm/source/mmm_armada/PrintMessages.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Producer.cpp](../third_party/mmm/source/mmm_armada/Producer.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ProducerBinding.cpp](../third_party/mmm/source/mmm_armada/ProducerBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ProducerClassBinding.cpp](../third_party/mmm/source/mmm_armada/ProducerClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ProducerClass_Internal.cpp](../third_party/mmm/source/mmm_armada/ProducerClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PulsePhaserBinding.cpp](../third_party/mmm/source/mmm_armada/PulsePhaserBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/PulsePhaser_Internal.cpp](../third_party/mmm/source/mmm_armada/PulsePhaser_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Queue.cpp](../third_party/mmm/source/mmm_armada/Queue.cpp)
- [ ] [third_party/mmm/source/mmm_armada/QueueBinding.cpp](../third_party/mmm/source/mmm_armada/QueueBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/RaceBinding.cpp](../third_party/mmm/source/mmm_armada/RaceBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Race_Internal.cpp](../third_party/mmm/source/mmm_armada/Race_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/RepairShipBinding.cpp](../third_party/mmm/source/mmm_armada/RepairShipBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/RepairShipClassBinding.cpp](../third_party/mmm/source/mmm_armada/RepairShipClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/RepairShipClass_Internal.cpp](../third_party/mmm/source/mmm_armada/RepairShipClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/RepairShip_Internal.cpp](../third_party/mmm/source/mmm_armada/RepairShip_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResearchPodBinding.cpp](../third_party/mmm/source/mmm_armada/ResearchPodBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResearchPod_Internal.cpp](../third_party/mmm/source/mmm_armada/ResearchPod_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResearchStation.cpp](../third_party/mmm/source/mmm_armada/ResearchStation.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResearchStationBinding.cpp](../third_party/mmm/source/mmm_armada/ResearchStationBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResourceInterfaceBinding.cpp](../third_party/mmm/source/mmm_armada/ResourceInterfaceBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResourceInterface_Internal.cpp](../third_party/mmm/source/mmm_armada/ResourceInterface_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResourceTransferInterface.cpp](../third_party/mmm/source/mmm_armada/ResourceTransferInterface.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ResourceTransferInterfaceBinding.cpp](../third_party/mmm/source/mmm_armada/ResourceTransferInterfaceBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ST3D_Colour.cpp](../third_party/mmm/source/mmm_armada/ST3D_Colour.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ST3D_ColourBinding.cpp](../third_party/mmm/source/mmm_armada/ST3D_ColourBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/SaveBinding.cpp](../third_party/mmm/source/mmm_armada/SaveBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Save_Internal.cpp](../third_party/mmm/source/mmm_armada/Save_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Scavenger.cpp](../third_party/mmm/source/mmm_armada/Scavenger.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ScavengerBinding.cpp](../third_party/mmm/source/mmm_armada/ScavengerBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ScrapBinding.cpp](../third_party/mmm/source/mmm_armada/ScrapBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ScrapClassBinding.cpp](../third_party/mmm/source/mmm_armada/ScrapClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ScrapClass_Internal.cpp](../third_party/mmm/source/mmm_armada/ScrapClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Scrap_Internal.cpp](../third_party/mmm/source/mmm_armada/Scrap_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ScriptErrors.cpp](../third_party/mmm/source/mmm_armada/ScriptErrors.cpp)
- [ ] [third_party/mmm/source/mmm_armada/SensorArray.cpp](../third_party/mmm/source/mmm_armada/SensorArray.cpp)
- [ ] [third_party/mmm/source/mmm_armada/SensorArrayBinding.cpp](../third_party/mmm/source/mmm_armada/SensorArrayBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/ShipyardBinding.cpp](../third_party/mmm/source/mmm_armada/ShipyardBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Shipyard_Internal.cpp](../third_party/mmm/source/mmm_armada/Shipyard_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/SplineQueue.cpp](../third_party/mmm/source/mmm_armada/SplineQueue.cpp)
- [ ] [third_party/mmm/source/mmm_armada/SplineQueueBinding.cpp](../third_party/mmm/source/mmm_armada/SplineQueueBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/StarbaseBinding.cpp](../third_party/mmm/source/mmm_armada/StarbaseBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Starbase_Internal.cpp](../third_party/mmm/source/mmm_armada/Starbase_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/StringAdapter.cpp](../third_party/mmm/source/mmm_armada/StringAdapter.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TeamBinding.cpp](../third_party/mmm/source/mmm_armada/TeamBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Team_Internal.cpp](../third_party/mmm/source/mmm_armada/Team_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Team_Resources.cpp](../third_party/mmm/source/mmm_armada/Team_Resources.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TerrainObjectBinding.cpp](../third_party/mmm/source/mmm_armada/TerrainObjectBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TerrainObjectClassBinding.cpp](../third_party/mmm/source/mmm_armada/TerrainObjectClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TerrainObjectClass_Internal.cpp](../third_party/mmm/source/mmm_armada/TerrainObjectClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TerrainObject_Internal.cpp](../third_party/mmm/source/mmm_armada/TerrainObject_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Test.cpp](../third_party/mmm/source/mmm_armada/Test.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TestBinding.cpp](../third_party/mmm/source/mmm_armada/TestBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TextInput.cpp](../third_party/mmm/source/mmm_armada/TextInput.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TextInputHook.cpp](../third_party/mmm/source/mmm_armada/TextInputHook.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Timer.cpp](../third_party/mmm/source/mmm_armada/Timer.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TimerHook.cpp](../third_party/mmm/source/mmm_armada/TimerHook.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TimerWrapper.cpp](../third_party/mmm/source/mmm_armada/TimerWrapper.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TimerWrapperBinding.cpp](../third_party/mmm/source/mmm_armada/TimerWrapperBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TradingStation.cpp](../third_party/mmm/source/mmm_armada/TradingStation.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TradingStationBinding.cpp](../third_party/mmm/source/mmm_armada/TradingStationBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TradingStationClassBinding.cpp](../third_party/mmm/source/mmm_armada/TradingStationClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/TradingStationClass_Internal.cpp](../third_party/mmm/source/mmm_armada/TradingStationClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Type_DebriefingData.cpp](../third_party/mmm/source/mmm_armada/Type_DebriefingData.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Type_Entity.cpp](../third_party/mmm/source/mmm_armada/Type_Entity.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Type_GameObjectClass.cpp](../third_party/mmm/source/mmm_armada/Type_GameObjectClass.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Type_ObjectivesText.cpp](../third_party/mmm/source/mmm_armada/Type_ObjectivesText.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Type_ST3D_Camera.cpp](../third_party/mmm/source/mmm_armada/Type_ST3D_Camera.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Type_ST3D_GraphicsEngine.cpp](../third_party/mmm/source/mmm_armada/Type_ST3D_GraphicsEngine.cpp)
- [ ] [third_party/mmm/source/mmm_armada/UIBinding.cpp](../third_party/mmm/source/mmm_armada/UIBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/UI_Internal.cpp](../third_party/mmm/source/mmm_armada/UI_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/UtritiumBallBinding.cpp](../third_party/mmm/source/mmm_armada/UtritiumBallBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/UtritiumBallClassBinding.cpp](../third_party/mmm/source/mmm_armada/UtritiumBallClassBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/UtritiumBallClass_Internal.cpp](../third_party/mmm/source/mmm_armada/UtritiumBallClass_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/UtritiumBall_Internal.cpp](../third_party/mmm/source/mmm_armada/UtritiumBall_Internal.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Vector3.cpp](../third_party/mmm/source/mmm_armada/Vector3.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Vector3Binding.cpp](../third_party/mmm/source/mmm_armada/Vector3Binding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/Wormhole.cpp](../third_party/mmm/source/mmm_armada/Wormhole.cpp)
- [ ] [third_party/mmm/source/mmm_armada/WormholeBinding.cpp](../third_party/mmm/source/mmm_armada/WormholeBinding.cpp)
- [ ] [third_party/mmm/source/mmm_armada/mmm_armada.cpp](../third_party/mmm/source/mmm_armada/mmm_armada.cpp)
- [ ] [third_party/mmm/source/mmm_armada/pch.cpp](../third_party/mmm/source/mmm_armada/pch.cpp)

### mmm_mmm_common: 5

- [ ] [third_party/mmm/source/mmm_common/FilePath.cpp](../third_party/mmm/source/mmm_common/FilePath.cpp)
- [ ] [third_party/mmm/source/mmm_common/Hash.cpp](../third_party/mmm/source/mmm_common/Hash.cpp)
- [ ] [third_party/mmm/source/mmm_common/ModFolder.cpp](../third_party/mmm/source/mmm_common/ModFolder.cpp)
- [ ] [third_party/mmm/source/mmm_common/mmm_common.cpp](../third_party/mmm/source/mmm_common/mmm_common.cpp)
- [ ] [third_party/mmm/source/mmm_common/pch.cpp](../third_party/mmm/source/mmm_common/pch.cpp)

### mmm_mmm_database: 2

- [ ] [third_party/mmm/source/mmm_database/Database.cpp](../third_party/mmm/source/mmm_database/Database.cpp)
- [ ] [third_party/mmm/source/mmm_database/DatabaseSchema.cpp](../third_party/mmm/source/mmm_database/DatabaseSchema.cpp)

### mmm_mmm_luapack: 5

- [ ] [third_party/mmm/source/mmm_luapack/File.cpp](../third_party/mmm/source/mmm_luapack/File.cpp)
- [ ] [third_party/mmm/source/mmm_luapack/Folder.cpp](../third_party/mmm/source/mmm_luapack/Folder.cpp)
- [ ] [third_party/mmm/source/mmm_luapack/Node.cpp](../third_party/mmm/source/mmm_luapack/Node.cpp)
- [ ] [third_party/mmm/source/mmm_luapack/Pack.cpp](../third_party/mmm/source/mmm_luapack/Pack.cpp)
- [ ] [third_party/mmm/source/mmm_luapack/PackInformation.cpp](../third_party/mmm/source/mmm_luapack/PackInformation.cpp)

### mmm_mmm_loader: 2

- [ ] [third_party/mmm/source/mmm_loader/RuleHelper.cpp](../third_party/mmm/source/mmm_loader/RuleHelper.cpp)
- [ ] [third_party/mmm/source/mmm_loader/Worker.cpp](../third_party/mmm/source/mmm_loader/Worker.cpp)

### mmm_luabind: 17

- [ ] [third_party/mmm/source/external/luabind/src/class.cpp](../third_party/mmm/source/external/luabind/src/class.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/class_info.cpp](../third_party/mmm/source/external/luabind/src/class_info.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/class_registry.cpp](../third_party/mmm/source/external/luabind/src/class_registry.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/class_rep.cpp](../third_party/mmm/source/external/luabind/src/class_rep.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/create_class.cpp](../third_party/mmm/source/external/luabind/src/create_class.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/error.cpp](../third_party/mmm/source/external/luabind/src/error.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/exception_handler.cpp](../third_party/mmm/source/external/luabind/src/exception_handler.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/function.cpp](../third_party/mmm/source/external/luabind/src/function.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/inheritance.cpp](../third_party/mmm/source/external/luabind/src/inheritance.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/link_compatibility.cpp](../third_party/mmm/source/external/luabind/src/link_compatibility.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/object_rep.cpp](../third_party/mmm/source/external/luabind/src/object_rep.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/open.cpp](../third_party/mmm/source/external/luabind/src/open.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/pcall.cpp](../third_party/mmm/source/external/luabind/src/pcall.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/scope.cpp](../third_party/mmm/source/external/luabind/src/scope.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/stack_content_by_name.cpp](../third_party/mmm/source/external/luabind/src/stack_content_by_name.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/weak_ref.cpp](../third_party/mmm/source/external/luabind/src/weak_ref.cpp)
- [ ] [third_party/mmm/source/external/luabind/src/wrapper_base.cpp](../third_party/mmm/source/external/luabind/src/wrapper_base.cpp)

### mmm_lua: 29

- [ ] [third_party/mmm/source/external/lua/src/lapi.c](../third_party/mmm/source/external/lua/src/lapi.c)
- [ ] [third_party/mmm/source/external/lua/src/lauxlib.c](../third_party/mmm/source/external/lua/src/lauxlib.c)
- [ ] [third_party/mmm/source/external/lua/src/lbaselib.c](../third_party/mmm/source/external/lua/src/lbaselib.c)
- [ ] [third_party/mmm/source/external/lua/src/lcode.c](../third_party/mmm/source/external/lua/src/lcode.c)
- [ ] [third_party/mmm/source/external/lua/src/ldblib.c](../third_party/mmm/source/external/lua/src/ldblib.c)
- [ ] [third_party/mmm/source/external/lua/src/ldebug.c](../third_party/mmm/source/external/lua/src/ldebug.c)
- [ ] [third_party/mmm/source/external/lua/src/ldo.c](../third_party/mmm/source/external/lua/src/ldo.c)
- [ ] [third_party/mmm/source/external/lua/src/ldump.c](../third_party/mmm/source/external/lua/src/ldump.c)
- [ ] [third_party/mmm/source/external/lua/src/lfunc.c](../third_party/mmm/source/external/lua/src/lfunc.c)
- [ ] [third_party/mmm/source/external/lua/src/lgc.c](../third_party/mmm/source/external/lua/src/lgc.c)
- [ ] [third_party/mmm/source/external/lua/src/linit.c](../third_party/mmm/source/external/lua/src/linit.c)
- [ ] [third_party/mmm/source/external/lua/src/liolib.c](../third_party/mmm/source/external/lua/src/liolib.c)
- [ ] [third_party/mmm/source/external/lua/src/llex.c](../third_party/mmm/source/external/lua/src/llex.c)
- [ ] [third_party/mmm/source/external/lua/src/lmathlib.c](../third_party/mmm/source/external/lua/src/lmathlib.c)
- [ ] [third_party/mmm/source/external/lua/src/lmem.c](../third_party/mmm/source/external/lua/src/lmem.c)
- [ ] [third_party/mmm/source/external/lua/src/loadlib.c](../third_party/mmm/source/external/lua/src/loadlib.c)
- [ ] [third_party/mmm/source/external/lua/src/lobject.c](../third_party/mmm/source/external/lua/src/lobject.c)
- [ ] [third_party/mmm/source/external/lua/src/lopcodes.c](../third_party/mmm/source/external/lua/src/lopcodes.c)
- [ ] [third_party/mmm/source/external/lua/src/loslib.c](../third_party/mmm/source/external/lua/src/loslib.c)
- [ ] [third_party/mmm/source/external/lua/src/lparser.c](../third_party/mmm/source/external/lua/src/lparser.c)
- [ ] [third_party/mmm/source/external/lua/src/lstate.c](../third_party/mmm/source/external/lua/src/lstate.c)
- [ ] [third_party/mmm/source/external/lua/src/lstring.c](../third_party/mmm/source/external/lua/src/lstring.c)
- [ ] [third_party/mmm/source/external/lua/src/lstrlib.c](../third_party/mmm/source/external/lua/src/lstrlib.c)
- [ ] [third_party/mmm/source/external/lua/src/ltable.c](../third_party/mmm/source/external/lua/src/ltable.c)
- [ ] [third_party/mmm/source/external/lua/src/ltablib.c](../third_party/mmm/source/external/lua/src/ltablib.c)
- [ ] [third_party/mmm/source/external/lua/src/ltm.c](../third_party/mmm/source/external/lua/src/ltm.c)
- [ ] [third_party/mmm/source/external/lua/src/lundump.c](../third_party/mmm/source/external/lua/src/lundump.c)
- [ ] [third_party/mmm/source/external/lua/src/lvm.c](../third_party/mmm/source/external/lua/src/lvm.c)
- [ ] [third_party/mmm/source/external/lua/src/lzio.c](../third_party/mmm/source/external/lua/src/lzio.c)

### mmm_zlib: 15

- [ ] [third_party/mmm/source/external/zlib/adler32.c](../third_party/mmm/source/external/zlib/adler32.c)
- [ ] [third_party/mmm/source/external/zlib/compress.c](../third_party/mmm/source/external/zlib/compress.c)
- [ ] [third_party/mmm/source/external/zlib/crc32.c](../third_party/mmm/source/external/zlib/crc32.c)
- [ ] [third_party/mmm/source/external/zlib/deflate.c](../third_party/mmm/source/external/zlib/deflate.c)
- [ ] [third_party/mmm/source/external/zlib/gzclose.c](../third_party/mmm/source/external/zlib/gzclose.c)
- [ ] [third_party/mmm/source/external/zlib/gzlib.c](../third_party/mmm/source/external/zlib/gzlib.c)
- [ ] [third_party/mmm/source/external/zlib/gzread.c](../third_party/mmm/source/external/zlib/gzread.c)
- [ ] [third_party/mmm/source/external/zlib/gzwrite.c](../third_party/mmm/source/external/zlib/gzwrite.c)
- [ ] [third_party/mmm/source/external/zlib/infback.c](../third_party/mmm/source/external/zlib/infback.c)
- [ ] [third_party/mmm/source/external/zlib/inffast.c](../third_party/mmm/source/external/zlib/inffast.c)
- [ ] [third_party/mmm/source/external/zlib/inflate.c](../third_party/mmm/source/external/zlib/inflate.c)
- [ ] [third_party/mmm/source/external/zlib/inftrees.c](../third_party/mmm/source/external/zlib/inftrees.c)
- [ ] [third_party/mmm/source/external/zlib/trees.c](../third_party/mmm/source/external/zlib/trees.c)
- [ ] [third_party/mmm/source/external/zlib/uncompr.c](../third_party/mmm/source/external/zlib/uncompr.c)
- [ ] [third_party/mmm/source/external/zlib/zutil.c](../third_party/mmm/source/external/zlib/zutil.c)

### mmm_minizip: 3

- [ ] [third_party/mmm/source/external/zlib/contrib/minizip/ioapi.c](../third_party/mmm/source/external/zlib/contrib/minizip/ioapi.c)
- [ ] [third_party/mmm/source/external/zlib/contrib/minizip/unzip.c](../third_party/mmm/source/external/zlib/contrib/minizip/unzip.c)
- [ ] [third_party/mmm/source/external/zlib/contrib/minizip/zip.c](../third_party/mmm/source/external/zlib/contrib/minizip/zip.c)

### renderer: 27

- [ ] [core/nebula_renderer.cpp](../core/nebula_renderer.cpp)
- [ ] [core/renderer_options.cpp](../core/renderer_options.cpp)
- [ ] [renderer/cloak/refraction.cpp](../renderer/cloak/refraction.cpp)
- [ ] [renderer/d3d11/com_guard.cpp](../renderer/d3d11/com_guard.cpp)
- [ ] [renderer/d3d11/compressed_texture.cpp](../renderer/d3d11/compressed_texture.cpp)
- [ ] [renderer/d3d11/game/backend.cpp](../renderer/d3d11/game/backend.cpp)
- [ ] [renderer/d3d11/game/draw_pipeline.cpp](../renderer/d3d11/game/draw_pipeline.cpp)
- [ ] [renderer/d3d11/probe_bridge.cpp](../renderer/d3d11/probe_bridge.cpp)
- [ ] [renderer/d3d11/probe_device.cpp](../renderer/d3d11/probe_device.cpp)
- [ ] [renderer/d3d11/texture_handle.cpp](../renderer/d3d11/texture_handle.cpp)
- [ ] [renderer/d3d11/ui_renderer.cpp](../renderer/d3d11/ui_renderer.cpp)
- [ ] [renderer/d3d8_compat/game/bc_catalog.cpp](../renderer/d3d8_compat/game/bc_catalog.cpp)
- [ ] [renderer/d3d8_compat/game/d3dx_bridge.cpp](../renderer/d3d8_compat/game/d3dx_bridge.cpp)
- [ ] [renderer/d3d8_compat/game/device.cpp](../renderer/d3d8_compat/game/device.cpp)
- [ ] [renderer/d3d8_compat/game/draw.cpp](../renderer/d3d8_compat/game/draw.cpp)
- [ ] [renderer/d3d8_compat/game/factory.cpp](../renderer/d3d8_compat/game/factory.cpp)
- [ ] [renderer/d3d8_compat/game/resources.cpp](../renderer/d3d8_compat/game/resources.cpp)
- [ ] [renderer/d3d8_compat/game/state_blocks.cpp](../renderer/d3d8_compat/game/state_blocks.cpp)
- [ ] [renderer/d3d8_compat/game/unsupported.cpp](../renderer/d3d8_compat/game/unsupported.cpp)
- [ ] [renderer/d3d8_compat/observer.cpp](../renderer/d3d8_compat/observer.cpp)
- [ ] [renderer/d3d8_compat/resources/compressed_texture8.cpp](../renderer/d3d8_compat/resources/compressed_texture8.cpp)
- [ ] [renderer/shaders/bytecode_cache.cpp](../renderer/shaders/bytecode_cache.cpp)
- [ ] [renderer/shaders/d3d8_tokens.cpp](../renderer/shaders/d3d8_tokens.cpp)
- [ ] [renderer/shaders/vs11.cpp](../renderer/shaders/vs11.cpp)
- [ ] [renderer/state/fvf.cpp](../renderer/state/fvf.cpp)
- [ ] [renderer/textures/dds.cpp](../renderer/textures/dds.cpp)
- [ ] [renderer/textures/upload.cpp](../renderer/textures/upload.cpp)

## Textually included implementation candidates

These bodies must migrate with their callers. Some whole modules are included into built-in wrappers. Their absence from a direct object filename is not retirement evidence.

- [ ] [core/renderer_cloak_composite.inl](../core/renderer_cloak_composite.inl)
- [ ] [core/renderer_cloak_refraction.inl](../core/renderer_cloak_refraction.inl)
- [ ] [core/renderer_general_meshvb.inl](../core/renderer_general_meshvb.inl)
- [ ] [core/renderer_phong_meshvb.inl](../core/renderer_phong_meshvb.inl)
- [ ] [core/renderer_transparent_indices.inl](../core/renderer_transparent_indices.inl)
- [ ] [core/renderer_unmapped_meshvb.inl](../core/renderer_unmapped_meshvb.inl)
- [ ] [modules/A1Compat/module.cpp](../modules/A1Compat/module.cpp)
- [ ] [modules/A1Fallbacks/module.cpp](../modules/A1Fallbacks/module.cpp)
- [ ] [modules/A2FOAlwaysShowShields/module.cpp](../modules/A2FOAlwaysShowShields/module.cpp)
- [ ] [modules/A2FOAnimatedHardpoints/module.cpp](../modules/A2FOAnimatedHardpoints/module.cpp)
- [ ] [modules/A2FOAnimations/module.cpp](../modules/A2FOAnimations/module.cpp)
- [ ] [modules/A2FOBreeders/module.cpp](../modules/A2FOBreeders/module.cpp)
- [ ] [modules/A2FOBuildTooltips/module.cpp](../modules/A2FOBuildTooltips/module.cpp)
- [ ] [modules/A2FOCameraControls/module.cpp](../modules/A2FOCameraControls/module.cpp)
- [ ] [modules/A2FOCheats/module.cpp](../modules/A2FOCheats/module.cpp)
- [ ] [modules/A2FOCollectiveXP/module.cpp](../modules/A2FOCollectiveXP/module.cpp)
- [ ] [modules/A2FOCraftIdentity/module.cpp](../modules/A2FOCraftIdentity/module.cpp)
- [ ] [modules/A2FODirectionalShields/module.cpp](../modules/A2FODirectionalShields/module.cpp)
- [ ] [modules/A2FOEditMenu/module.cpp](../modules/A2FOEditMenu/module.cpp)
- [ ] [modules/A2FOEnergySystems/module.cpp](../modules/A2FOEnergySystems/module.cpp)
- [ ] [modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp](../modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp)
- [ ] [modules/A2FOFeaturePack/buildyard_tree_runtime.inl](../modules/A2FOFeaturePack/buildyard_tree_runtime.inl)
- [ ] [modules/A2FOFeaturePack/hybrid_bridge_client.cpp](../modules/A2FOFeaturePack/hybrid_bridge_client.cpp)
- [ ] [modules/A2FOFeaturePack/odf_recursive.cpp](../modules/A2FOFeaturePack/odf_recursive.cpp)
- [ ] [modules/A2FOFeaturePack/queue_enhancement.cpp](../modules/A2FOFeaturePack/queue_enhancement.cpp)
- [ ] [modules/A2FOFeaturePack/upgrade_pods.cpp](../modules/A2FOFeaturePack/upgrade_pods.cpp)
- [ ] [modules/A2FOFireArcs/module.cpp](../modules/A2FOFireArcs/module.cpp)
- [ ] [modules/A2FOHybridBuild/hybrid_production_runtime.cpp](../modules/A2FOHybridBuild/hybrid_production_runtime.cpp)
- [ ] [modules/A2FOHybridBuild/module.cpp](../modules/A2FOHybridBuild/module.cpp)
- [ ] [modules/A2FOInfestation/module.cpp](../modules/A2FOInfestation/module.cpp)
- [ ] [modules/A2FOInfoIni/module.cpp](../modules/A2FOInfoIni/module.cpp)
- [ ] [modules/A2FOInstantActionSettings/module.cpp](../modules/A2FOInstantActionSettings/module.cpp)
- [ ] [modules/A2FOLogMailer/module.cpp](../modules/A2FOLogMailer/module.cpp)
- [ ] [modules/A2FOM3D/module.cpp](../modules/A2FOM3D/module.cpp)
- [ ] [modules/A2FOMissionSelector/module.cpp](../modules/A2FOMissionSelector/module.cpp)
- [ ] [modules/A2FOMissionSelector/standard_buttons.inl](../modules/A2FOMissionSelector/standard_buttons.inl)
- [ ] [modules/A2FOMissions/module.cpp](../modules/A2FOMissions/module.cpp)
- [ ] [modules/A2FOMultiYardBuild/module.cpp](../modules/A2FOMultiYardBuild/module.cpp)
- [ ] [modules/A2FOMuzzleFlashes/module.cpp](../modules/A2FOMuzzleFlashes/module.cpp)
- [ ] [modules/A2FONebulaRenderer/module.cpp](../modules/A2FONebulaRenderer/module.cpp)
- [ ] [modules/A2FONormalWeaponTech/module.cpp](../modules/A2FONormalWeaponTech/module.cpp)
- [ ] [modules/A2FOODFVariants/module.cpp](../modules/A2FOODFVariants/module.cpp)
- [ ] [modules/A2FOPointDefenseCycles/module.cpp](../modules/A2FOPointDefenseCycles/module.cpp)
- [ ] [modules/A2FORGBTextures/module.cpp](../modules/A2FORGBTextures/module.cpp)
- [ ] [modules/A2FORadarDiagnostics/module.cpp](../modules/A2FORadarDiagnostics/module.cpp)
- [ ] [modules/A2FORecoveryTests/module.cpp](../modules/A2FORecoveryTests/module.cpp)
- [ ] [modules/A2FORefitMenus/module.cpp](../modules/A2FORefitMenus/module.cpp)
- [ ] [modules/A2FORefitYards/module.cpp](../modules/A2FORefitYards/module.cpp)
- [ ] [modules/A2FOResources/module.cpp](../modules/A2FOResources/module.cpp)
- [ ] [modules/A2FOSOLMaps/module.cpp](../modules/A2FOSOLMaps/module.cpp)
- [ ] [modules/A2FOShellDisplay/module.cpp](../modules/A2FOShellDisplay/module.cpp)
- [ ] [modules/A2FOSquadrons/module.cpp](../modules/A2FOSquadrons/module.cpp)
- [ ] [modules/A2FOSquadrons/native_economics.inl](../modules/A2FOSquadrons/native_economics.inl)
- [ ] [modules/A2FOSquadrons/native_pod_selection.inl](../modules/A2FOSquadrons/native_pod_selection.inl)
- [ ] [modules/A2FOSquadrons/native_repair.inl](../modules/A2FOSquadrons/native_repair.inl)
- [ ] [modules/A2FOStationRotation/footprint_runtime.inl](../modules/A2FOStationRotation/footprint_runtime.inl)
- [ ] [modules/A2FOStationRotation/module.cpp](../modules/A2FOStationRotation/module.cpp)
- [ ] [modules/A2FOSwarmSystem/module.cpp](../modules/A2FOSwarmSystem/module.cpp)
- [ ] [modules/A2FOTeamChangeWeapons/module.cpp](../modules/A2FOTeamChangeWeapons/module.cpp)
- [ ] [modules/A2FOTextureVariants/module.cpp](../modules/A2FOTextureVariants/module.cpp)
- [ ] [modules/A2FOTurrets/module.cpp](../modules/A2FOTurrets/module.cpp)
- [ ] [modules/A2FOVideo/module.cpp](../modules/A2FOVideo/module.cpp)
- [ ] [modules/A2FOViewDistance/module.cpp](../modules/A2FOViewDistance/module.cpp)
- [ ] [modules/A2FOWeaponDamageControls/module.cpp](../modules/A2FOWeaponDamageControls/module.cpp)
- [ ] [modules/A2FOWreckage/module.cpp](../modules/A2FOWreckage/module.cpp)
- [ ] [renderer/d3d8_compat/generated_thunks.inl](../renderer/d3d8_compat/generated_thunks.inl)
- [ ] [renderer/d3d8_compat/shader_diagnostics.inl](../renderer/d3d8_compat/shader_diagnostics.inl)
- [ ] [third_party/mmm/source/mmm_armada/Container.inl](../third_party/mmm/source/mmm_armada/Container.inl)
- [ ] [third_party/mmm/source/mmm_armada/Entities.inl](../third_party/mmm/source/mmm_armada/Entities.inl)
- [ ] [third_party/mmm/source/mmm_armada/Entity_Internal.inl](../third_party/mmm/source/mmm_armada/Entity_Internal.inl)
- [ ] [third_party/mmm/source/mmm_armada/ListAdapter.inl](../third_party/mmm/source/mmm_armada/ListAdapter.inl)
- [ ] [third_party/mmm/source/mmm_armada/Load_Internal.inl](../third_party/mmm/source/mmm_armada/Load_Internal.inl)
- [ ] [third_party/mmm/source/mmm_armada/MemRaider.inl](../third_party/mmm/source/mmm_armada/MemRaider.inl)
- [ ] [third_party/mmm/source/mmm_armada/VectorAdapter.inl](../third_party/mmm/source/mmm_armada/VectorAdapter.inl)

## Feature ownership and all module source files

The JSON includes a hash-pinned recursive source/header/bridge/resource/script file list for every family. Each row of the report has its own closure ID. It includes helper/relay sources where present; names alone do not classify production reachability.

### F01: modules/A2FOBuildTooltips

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOBuildTooltips/build_time_text.cpp](../modules/A2FOBuildTooltips/build_time_text.cpp)
- [modules/A2FOBuildTooltips/build_time_text.hpp](../modules/A2FOBuildTooltips/build_time_text.hpp)
- [modules/A2FOBuildTooltips/module.cpp](../modules/A2FOBuildTooltips/module.cpp)
- [modules/A2FOBuildTooltips/thiscall_bridge.S](../modules/A2FOBuildTooltips/thiscall_bridge.S)

### F02: modules/A2FONormalWeaponTech

Recorded state: `implemented_bounded_native_verified_pending_in_game_acceptance`. Full implementation closure: **open**.

- [modules/A2FONormalWeaponTech/delphi_bridge.S](../modules/A2FONormalWeaponTech/delphi_bridge.S)
- [modules/A2FONormalWeaponTech/module.cpp](../modules/A2FONormalWeaponTech/module.cpp)

### F03: modules/A2FOWreckage

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOWreckage/module.cpp](../modules/A2FOWreckage/module.cpp)
- [modules/A2FOWreckage/wreckage_policy.cpp](../modules/A2FOWreckage/wreckage_policy.cpp)
- [modules/A2FOWreckage/wreckage_policy.hpp](../modules/A2FOWreckage/wreckage_policy.hpp)

### F04: modules/A2FOInfoIni

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOInfoIni/module.cpp](../modules/A2FOInfoIni/module.cpp)

### F05: modules/A2FOMuzzleFlashes

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOMuzzleFlashes/module.cpp](../modules/A2FOMuzzleFlashes/module.cpp)
- [modules/A2FOMuzzleFlashes/thiscall_bridge.S](../modules/A2FOMuzzleFlashes/thiscall_bridge.S)

### F06: modules/A2FOTeamChangeWeapons

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOTeamChangeWeapons/module.cpp](../modules/A2FOTeamChangeWeapons/module.cpp)
- [modules/A2FOTeamChangeWeapons/thiscall_bridge.S](../modules/A2FOTeamChangeWeapons/thiscall_bridge.S)

### F07: modules/A2FOWeaponDamageControls

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOWeaponDamageControls/damage_controls.hpp](../modules/A2FOWeaponDamageControls/damage_controls.hpp)
- [modules/A2FOWeaponDamageControls/module.cpp](../modules/A2FOWeaponDamageControls/module.cpp)
- [modules/A2FOWeaponDamageControls/thiscall_bridge.S](../modules/A2FOWeaponDamageControls/thiscall_bridge.S)

### F08: modules/A1Fallbacks

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A1Fallbacks/fallback_policy.cpp](../modules/A1Fallbacks/fallback_policy.cpp)
- [modules/A1Fallbacks/fallback_policy.hpp](../modules/A1Fallbacks/fallback_policy.hpp)
- [modules/A1Fallbacks/module.cpp](../modules/A1Fallbacks/module.cpp)
- [modules/A1Fallbacks/thiscall_bridge.S](../modules/A1Fallbacks/thiscall_bridge.S)

### F09: modules/A2FOAnimatedHardpoints

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOAnimatedHardpoints/module.cpp](../modules/A2FOAnimatedHardpoints/module.cpp)
- [modules/A2FOAnimatedHardpoints/thiscall_bridge.S](../modules/A2FOAnimatedHardpoints/thiscall_bridge.S)

### F10: modules/A2FOCameraControls

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOCameraControls/module.cpp](../modules/A2FOCameraControls/module.cpp)
- [modules/A2FOCameraControls/native_bridge.S](../modules/A2FOCameraControls/native_bridge.S)

### F11: modules/A2FOViewDistance

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOViewDistance/module.cpp](../modules/A2FOViewDistance/module.cpp)
- [modules/A2FOViewDistance/native_bridge.S](../modules/A2FOViewDistance/native_bridge.S)

### F12: modules/A2FOInstantActionSettings

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOInstantActionSettings/load_button_bounds.cpp](../modules/A2FOInstantActionSettings/load_button_bounds.cpp)
- [modules/A2FOInstantActionSettings/load_button_bounds.hpp](../modules/A2FOInstantActionSettings/load_button_bounds.hpp)
- [modules/A2FOInstantActionSettings/module.cpp](../modules/A2FOInstantActionSettings/module.cpp)
- [modules/A2FOInstantActionSettings/setup_details_line.cpp](../modules/A2FOInstantActionSettings/setup_details_line.cpp)
- [modules/A2FOInstantActionSettings/setup_details_line.hpp](../modules/A2FOInstantActionSettings/setup_details_line.hpp)
- [modules/A2FOInstantActionSettings/thiscall_bridge.S](../modules/A2FOInstantActionSettings/thiscall_bridge.S)

### F13: modules/A2FOEditMenu

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOEditMenu/edit_menu_odf.cpp](../modules/A2FOEditMenu/edit_menu_odf.cpp)
- [modules/A2FOEditMenu/edit_menu_odf.hpp](../modules/A2FOEditMenu/edit_menu_odf.hpp)
- [modules/A2FOEditMenu/module.cpp](../modules/A2FOEditMenu/module.cpp)
- [modules/A2FOEditMenu/thiscall_bridge.S](../modules/A2FOEditMenu/thiscall_bridge.S)

### F14: modules/A2FOM3D

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOM3D/m3d_conversion.cpp](../modules/A2FOM3D/m3d_conversion.cpp)
- [modules/A2FOM3D/m3d_conversion.hpp](../modules/A2FOM3D/m3d_conversion.hpp)
- [modules/A2FOM3D/module.cpp](../modules/A2FOM3D/module.cpp)
- [modules/A2FOM3D/thiscall_bridge.S](../modules/A2FOM3D/thiscall_bridge.S)

### F15: modules/A2FOBreeders

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOBreeders/module.cpp](../modules/A2FOBreeders/module.cpp)
- [modules/A2FOBreeders/thiscall_bridge.S](../modules/A2FOBreeders/thiscall_bridge.S)

### F16: modules/A2FOInfestation

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOInfestation/module.cpp](../modules/A2FOInfestation/module.cpp)
- [modules/A2FOInfestation/thiscall_bridge.S](../modules/A2FOInfestation/thiscall_bridge.S)

### F17: modules/A2FOSwarmSystem

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOSwarmSystem/module.cpp](../modules/A2FOSwarmSystem/module.cpp)
- [modules/A2FOSwarmSystem/swarm_motion.cpp](../modules/A2FOSwarmSystem/swarm_motion.cpp)
- [modules/A2FOSwarmSystem/swarm_motion.hpp](../modules/A2FOSwarmSystem/swarm_motion.hpp)
- [modules/A2FOSwarmSystem/thiscall_bridge.S](../modules/A2FOSwarmSystem/thiscall_bridge.S)

### F18: modules/A2FOODFVariants

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOODFVariants/module.cpp](../modules/A2FOODFVariants/module.cpp)
- [modules/A2FOODFVariants/thiscall_bridge.S](../modules/A2FOODFVariants/thiscall_bridge.S)

### F19: modules/A2FOFireArcs

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOFireArcs/fire_arc.cpp](../modules/A2FOFireArcs/fire_arc.cpp)
- [modules/A2FOFireArcs/fire_arc.hpp](../modules/A2FOFireArcs/fire_arc.hpp)
- [modules/A2FOFireArcs/module.cpp](../modules/A2FOFireArcs/module.cpp)
- [modules/A2FOFireArcs/runtime_config.cpp](../modules/A2FOFireArcs/runtime_config.cpp)
- [modules/A2FOFireArcs/runtime_config.hpp](../modules/A2FOFireArcs/runtime_config.hpp)
- [modules/A2FOFireArcs/thiscall_bridge.S](../modules/A2FOFireArcs/thiscall_bridge.S)

### F20: modules/A2FOAnimations

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOAnimations/api.hpp](../modules/A2FOAnimations/api.hpp)
- [modules/A2FOAnimations/module.cpp](../modules/A2FOAnimations/module.cpp)
- [modules/A2FOAnimations/playback.hpp](../modules/A2FOAnimations/playback.hpp)

### F21: modules/A2FOAlwaysShowShields

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOAlwaysShowShields/module.cpp](../modules/A2FOAlwaysShowShields/module.cpp)
- [modules/A2FOAlwaysShowShields/shield_visibility.cpp](../modules/A2FOAlwaysShowShields/shield_visibility.cpp)
- [modules/A2FOAlwaysShowShields/shield_visibility.hpp](../modules/A2FOAlwaysShowShields/shield_visibility.hpp)
- [modules/A2FOAlwaysShowShields/thiscall_bridge.S](../modules/A2FOAlwaysShowShields/thiscall_bridge.S)

### F22: modules/A2FOStationRotation

Recorded state: `implemented_bounded_native_verified_pending_in_game_acceptance`. Full implementation closure: **open**.

- [modules/A2FOStationRotation/footprint.hpp](../modules/A2FOStationRotation/footprint.hpp)
- [modules/A2FOStationRotation/footprint_runtime.inl](../modules/A2FOStationRotation/footprint_runtime.inl)
- [modules/A2FOStationRotation/module.cpp](../modules/A2FOStationRotation/module.cpp)
- [modules/A2FOStationRotation/rotation.hpp](../modules/A2FOStationRotation/rotation.hpp)
- [modules/A2FOStationRotation/thiscall_bridge.S](../modules/A2FOStationRotation/thiscall_bridge.S)

### F23: modules/A2FORadarDiagnostics

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FORadarDiagnostics/module.cpp](../modules/A2FORadarDiagnostics/module.cpp)

### F24: modules/A2FORecoveryTests

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FORecoveryTests/armadal/engine_context.hpp](../modules/A2FORecoveryTests/armadal/engine_context.hpp)
- [modules/A2FORecoveryTests/armadal/entity_registry.hpp](../modules/A2FORecoveryTests/armadal/entity_registry.hpp)
- [modules/A2FORecoveryTests/armadal/node_transform.hpp](../modules/A2FORecoveryTests/armadal/node_transform.hpp)
- [modules/A2FORecoveryTests/module.cpp](../modules/A2FORecoveryTests/module.cpp)
- [modules/A2FORecoveryTests/native_bridge.S](../modules/A2FORecoveryTests/native_bridge.S)
- [modules/A2FORecoveryTests/recovery_tests.cpp](../modules/A2FORecoveryTests/recovery_tests.cpp)
- [modules/A2FORecoveryTests/recovery_tests.hpp](../modules/A2FORecoveryTests/recovery_tests.hpp)
- [modules/A2FORecoveryTests/signatures.hpp](../modules/A2FORecoveryTests/signatures.hpp)

### F25: modules/A2FOCheats

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOCheats/module.cpp](../modules/A2FOCheats/module.cpp)
- [modules/A2FOCheats/thiscall_bridge.S](../modules/A2FOCheats/thiscall_bridge.S)

### F26: modules/A2FOCollectiveXP

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOCollectiveXP/collective_xp.cpp](../modules/A2FOCollectiveXP/collective_xp.cpp)
- [modules/A2FOCollectiveXP/collective_xp.hpp](../modules/A2FOCollectiveXP/collective_xp.hpp)
- [modules/A2FOCollectiveXP/module.cpp](../modules/A2FOCollectiveXP/module.cpp)
- [modules/A2FOCollectiveXP/native_bridge.S](../modules/A2FOCollectiveXP/native_bridge.S)
- [modules/A2FOCollectiveXP/native_layout.hpp](../modules/A2FOCollectiveXP/native_layout.hpp)
- [modules/A2FOCollectiveXP/queue_resolver.hpp](../modules/A2FOCollectiveXP/queue_resolver.hpp)

### F27: modules/A2FODirectionalShields

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FODirectionalShields/api.hpp](../modules/A2FODirectionalShields/api.hpp)
- [modules/A2FODirectionalShields/directional_shields.cpp](../modules/A2FODirectionalShields/directional_shields.cpp)
- [modules/A2FODirectionalShields/directional_shields.hpp](../modules/A2FODirectionalShields/directional_shields.hpp)
- [modules/A2FODirectionalShields/module.cpp](../modules/A2FODirectionalShields/module.cpp)
- [modules/A2FODirectionalShields/thiscall_bridge.S](../modules/A2FODirectionalShields/thiscall_bridge.S)

### F28: modules/A2FOEnergySystems

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOEnergySystems/energy_systems.cpp](../modules/A2FOEnergySystems/energy_systems.cpp)
- [modules/A2FOEnergySystems/energy_systems.hpp](../modules/A2FOEnergySystems/energy_systems.hpp)
- [modules/A2FOEnergySystems/module.cpp](../modules/A2FOEnergySystems/module.cpp)
- [modules/A2FOEnergySystems/resupply_policy.cpp](../modules/A2FOEnergySystems/resupply_policy.cpp)
- [modules/A2FOEnergySystems/resupply_policy.hpp](../modules/A2FOEnergySystems/resupply_policy.hpp)
- [modules/A2FOEnergySystems/thiscall_bridge.S](../modules/A2FOEnergySystems/thiscall_bridge.S)

### F29: modules/A2FOHybridBuild

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOHybridBuild/build_submenu_config.cpp](../modules/A2FOHybridBuild/build_submenu_config.cpp)
- [modules/A2FOHybridBuild/build_submenu_config.hpp](../modules/A2FOHybridBuild/build_submenu_config.hpp)
- [modules/A2FOHybridBuild/delphi_bridge.S](../modules/A2FOHybridBuild/delphi_bridge.S)
- [modules/A2FOHybridBuild/hybrid_production.cpp](../modules/A2FOHybridBuild/hybrid_production.cpp)
- [modules/A2FOHybridBuild/hybrid_production.hpp](../modules/A2FOHybridBuild/hybrid_production.hpp)
- [modules/A2FOHybridBuild/hybrid_production_runtime.cpp](../modules/A2FOHybridBuild/hybrid_production_runtime.cpp)
- [modules/A2FOHybridBuild/hybrid_production_runtime.hpp](../modules/A2FOHybridBuild/hybrid_production_runtime.hpp)
- [modules/A2FOHybridBuild/module.cpp](../modules/A2FOHybridBuild/module.cpp)
- [modules/A2FOHybridBuild/refit_ui_bridge_api.hpp](../modules/A2FOHybridBuild/refit_ui_bridge_api.hpp)

### F30: modules/A2FOMultiYardBuild

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOMultiYardBuild/bridges.S](../modules/A2FOMultiYardBuild/bridges.S)
- [modules/A2FOMultiYardBuild/module.cpp](../modules/A2FOMultiYardBuild/module.cpp)
- [modules/A2FOMultiYardBuild/policy.hpp](../modules/A2FOMultiYardBuild/policy.hpp)

### F31: modules/A2FONebulaRenderer

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FONebulaRenderer/art_texture_suffix_config.cpp](../modules/A2FONebulaRenderer/art_texture_suffix_config.cpp)
- [modules/A2FONebulaRenderer/art_texture_suffix_config.hpp](../modules/A2FONebulaRenderer/art_texture_suffix_config.hpp)
- [modules/A2FONebulaRenderer/module.cpp](../modules/A2FONebulaRenderer/module.cpp)
- [modules/A2FONebulaRenderer/thiscall_bridge.S](../modules/A2FONebulaRenderer/thiscall_bridge.S)

### F32: modules/A2FOPointDefenseCycles

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOPointDefenseCycles/firing_cycle.cpp](../modules/A2FOPointDefenseCycles/firing_cycle.cpp)
- [modules/A2FOPointDefenseCycles/firing_cycle.hpp](../modules/A2FOPointDefenseCycles/firing_cycle.hpp)
- [modules/A2FOPointDefenseCycles/module.cpp](../modules/A2FOPointDefenseCycles/module.cpp)
- [modules/A2FOPointDefenseCycles/thiscall_bridge.S](../modules/A2FOPointDefenseCycles/thiscall_bridge.S)

### F33: modules/A2FORefitYards

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FORefitYards/docking_transform.hpp](../modules/A2FORefitYards/docking_transform.hpp)
- [modules/A2FORefitYards/module.cpp](../modules/A2FORefitYards/module.cpp)
- [modules/A2FORefitYards/refit_policy.cpp](../modules/A2FORefitYards/refit_policy.cpp)
- [modules/A2FORefitYards/refit_policy.hpp](../modules/A2FORefitYards/refit_policy.hpp)
- [modules/A2FORefitYards/thiscall_bridge.S](../modules/A2FORefitYards/thiscall_bridge.S)

### F34: modules/A2FOTurrets

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOTurrets/module.cpp](../modules/A2FOTurrets/module.cpp)
- [modules/A2FOTurrets/thiscall_bridge.S](../modules/A2FOTurrets/thiscall_bridge.S)
- [modules/A2FOTurrets/turret_combat.cpp](../modules/A2FOTurrets/turret_combat.cpp)
- [modules/A2FOTurrets/turret_combat.hpp](../modules/A2FOTurrets/turret_combat.hpp)
- [modules/A2FOTurrets/turret_math.cpp](../modules/A2FOTurrets/turret_math.cpp)
- [modules/A2FOTurrets/turret_math.hpp](../modules/A2FOTurrets/turret_math.hpp)

### F35: modules/A2FORGBTextures

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FORGBTextures/module.cpp](../modules/A2FORGBTextures/module.cpp)
- [modules/A2FORGBTextures/thiscall_bridge.S](../modules/A2FORGBTextures/thiscall_bridge.S)

### F36: modules/A2FOTextureVariants

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOTextureVariants/module.cpp](../modules/A2FOTextureVariants/module.cpp)
- [modules/A2FOTextureVariants/texture_variants.cpp](../modules/A2FOTextureVariants/texture_variants.cpp)
- [modules/A2FOTextureVariants/texture_variants.hpp](../modules/A2FOTextureVariants/texture_variants.hpp)
- [modules/A2FOTextureVariants/thiscall_bridge.S](../modules/A2FOTextureVariants/thiscall_bridge.S)

### F37: modules/A1Compat

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A1Compat/a1_bzn_policy.cpp](../modules/A1Compat/a1_bzn_policy.cpp)
- [modules/A1Compat/a1_bzn_policy.hpp](../modules/A1Compat/a1_bzn_policy.hpp)
- [modules/A1Compat/a1_ui_policy.hpp](../modules/A1Compat/a1_ui_policy.hpp)
- [modules/A1Compat/module.cpp](../modules/A1Compat/module.cpp)
- [modules/A1Compat/race_menu_policy.cpp](../modules/A1Compat/race_menu_policy.cpp)
- [modules/A1Compat/race_menu_policy.hpp](../modules/A1Compat/race_menu_policy.hpp)
- [modules/A1Compat/team_color_policy.cpp](../modules/A1Compat/team_color_policy.cpp)
- [modules/A1Compat/team_color_policy.hpp](../modules/A1Compat/team_color_policy.hpp)
- [modules/A1Compat/thiscall_bridge.S](../modules/A1Compat/thiscall_bridge.S)

### F38: modules/A2FOCraftIdentity

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOCraftIdentity/directional_shield_display_config.cpp](../modules/A2FOCraftIdentity/directional_shield_display_config.cpp)
- [modules/A2FOCraftIdentity/directional_shield_display_config.hpp](../modules/A2FOCraftIdentity/directional_shield_display_config.hpp)
- [modules/A2FOCraftIdentity/directional_shield_fill.cpp](../modules/A2FOCraftIdentity/directional_shield_fill.cpp)
- [modules/A2FOCraftIdentity/directional_shield_fill.hpp](../modules/A2FOCraftIdentity/directional_shield_fill.hpp)
- [modules/A2FOCraftIdentity/extended_weapon_icons.cpp](../modules/A2FOCraftIdentity/extended_weapon_icons.cpp)
- [modules/A2FOCraftIdentity/extended_weapon_icons.hpp](../modules/A2FOCraftIdentity/extended_weapon_icons.hpp)
- [modules/A2FOCraftIdentity/fleetops_object_editor.cpp](../modules/A2FOCraftIdentity/fleetops_object_editor.cpp)
- [modules/A2FOCraftIdentity/fleetops_object_editor.hpp](../modules/A2FOCraftIdentity/fleetops_object_editor.hpp)
- [modules/A2FOCraftIdentity/identity_selection.cpp](../modules/A2FOCraftIdentity/identity_selection.cpp)
- [modules/A2FOCraftIdentity/identity_selection.hpp](../modules/A2FOCraftIdentity/identity_selection.hpp)
- [modules/A2FOCraftIdentity/module.cpp](../modules/A2FOCraftIdentity/module.cpp)
- [modules/A2FOCraftIdentity/object_editor.cpp](../modules/A2FOCraftIdentity/object_editor.cpp)
- [modules/A2FOCraftIdentity/object_editor.hpp](../modules/A2FOCraftIdentity/object_editor.hpp)
- [modules/A2FOCraftIdentity/object_editor_state.cpp](../modules/A2FOCraftIdentity/object_editor_state.cpp)
- [modules/A2FOCraftIdentity/object_editor_state.hpp](../modules/A2FOCraftIdentity/object_editor_state.hpp)
- [modules/A2FOCraftIdentity/selected_panel_anchor.hpp](../modules/A2FOCraftIdentity/selected_panel_anchor.hpp)
- [modules/A2FOCraftIdentity/system_icon_state.cpp](../modules/A2FOCraftIdentity/system_icon_state.cpp)
- [modules/A2FOCraftIdentity/system_icon_state.hpp](../modules/A2FOCraftIdentity/system_icon_state.hpp)
- [modules/A2FOCraftIdentity/thiscall_bridge.S](../modules/A2FOCraftIdentity/thiscall_bridge.S)

### F39: modules/A2FOFeaturePack

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOFeaturePack/bink_video.cpp](../modules/A2FOFeaturePack/bink_video.cpp)
- [modules/A2FOFeaturePack/bink_video.hpp](../modules/A2FOFeaturePack/bink_video.hpp)
- [modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp](../modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp)
- [modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp](../modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp)
- [modules/A2FOFeaturePack/buildyard_tooltip_native.hpp](../modules/A2FOFeaturePack/buildyard_tooltip_native.hpp)
- [modules/A2FOFeaturePack/buildyard_tree_runtime.inl](../modules/A2FOFeaturePack/buildyard_tree_runtime.inl)
- [modules/A2FOFeaturePack/buildyard_tree_signatures.hpp](../modules/A2FOFeaturePack/buildyard_tree_signatures.hpp)
- [modules/A2FOFeaturePack/delphi_bridge.S](../modules/A2FOFeaturePack/delphi_bridge.S)
- [modules/A2FOFeaturePack/hybrid_bridge_api.hpp](../modules/A2FOFeaturePack/hybrid_bridge_api.hpp)
- [modules/A2FOFeaturePack/hybrid_bridge_client.cpp](../modules/A2FOFeaturePack/hybrid_bridge_client.cpp)
- [modules/A2FOFeaturePack/hybrid_bridge_client.hpp](../modules/A2FOFeaturePack/hybrid_bridge_client.hpp)
- [modules/A2FOFeaturePack/module_technology_policy.hpp](../modules/A2FOFeaturePack/module_technology_policy.hpp)
- [modules/A2FOFeaturePack/odf_recursive.cpp](../modules/A2FOFeaturePack/odf_recursive.cpp)
- [modules/A2FOFeaturePack/queue_enhancement.cpp](../modules/A2FOFeaturePack/queue_enhancement.cpp)
- [modules/A2FOFeaturePack/queue_enhancement.hpp](../modules/A2FOFeaturePack/queue_enhancement.hpp)
- [modules/A2FOFeaturePack/refit_queue_bridge_api.hpp](../modules/A2FOFeaturePack/refit_queue_bridge_api.hpp)
- [modules/A2FOFeaturePack/refit_queue_bridge_client.cpp](../modules/A2FOFeaturePack/refit_queue_bridge_client.cpp)
- [modules/A2FOFeaturePack/refit_queue_bridge_client.hpp](../modules/A2FOFeaturePack/refit_queue_bridge_client.hpp)
- [modules/A2FOFeaturePack/upgrade_pod_config.cpp](../modules/A2FOFeaturePack/upgrade_pod_config.cpp)
- [modules/A2FOFeaturePack/upgrade_pod_config.hpp](../modules/A2FOFeaturePack/upgrade_pod_config.hpp)
- [modules/A2FOFeaturePack/upgrade_pods.cpp](../modules/A2FOFeaturePack/upgrade_pods.cpp)
- [modules/A2FOFeaturePack/upgrade_pods.hpp](../modules/A2FOFeaturePack/upgrade_pods.hpp)

### F40: modules/A2FOLogMailer

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOLogMailer/cloudflare/send-test.py](../modules/A2FOLogMailer/cloudflare/send-test.py)
- [modules/A2FOLogMailer/config.hpp](../modules/A2FOLogMailer/config.hpp)
- [modules/A2FOLogMailer/distribution.hpp](../modules/A2FOLogMailer/distribution.hpp)
- [modules/A2FOLogMailer/helper.cpp](../modules/A2FOLogMailer/helper.cpp)
- [modules/A2FOLogMailer/module.cpp](../modules/A2FOLogMailer/module.cpp)
- [modules/A2FOLogMailer/relay.py](../modules/A2FOLogMailer/relay.py)
- [modules/A2FOLogMailer/report.hpp](../modules/A2FOLogMailer/report.hpp)

### F41: modules/A2FOMissionSelector

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOMissionSelector/module.cpp](../modules/A2FOMissionSelector/module.cpp)
- [modules/A2FOMissionSelector/standard_buttons.inl](../modules/A2FOMissionSelector/standard_buttons.inl)
- [modules/A2FOMissionSelector/thiscall_bridge.S](../modules/A2FOMissionSelector/thiscall_bridge.S)

### F42: modules/A2FOMissions

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOMissions/fault_diagnostics.hpp](../modules/A2FOMissions/fault_diagnostics.hpp)
- [modules/A2FOMissions/module.cpp](../modules/A2FOMissions/module.cpp)

### F43: modules/A2FORefitMenus

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FORefitMenus/bindings.hpp](../modules/A2FORefitMenus/bindings.hpp)
- [modules/A2FORefitMenus/fonts.rc](../modules/A2FORefitMenus/fonts.rc)
- [modules/A2FORefitMenus/interaction.hpp](../modules/A2FORefitMenus/interaction.hpp)
- [modules/A2FORefitMenus/loading_bindings.hpp](../modules/A2FORefitMenus/loading_bindings.hpp)
- [modules/A2FORefitMenus/loading_content.cpp](../modules/A2FORefitMenus/loading_content.cpp)
- [modules/A2FORefitMenus/loading_content.hpp](../modules/A2FORefitMenus/loading_content.hpp)
- [modules/A2FORefitMenus/loading_gpu.cpp](../modules/A2FORefitMenus/loading_gpu.cpp)
- [modules/A2FORefitMenus/loading_gpu.hpp](../modules/A2FORefitMenus/loading_gpu.hpp)
- [modules/A2FORefitMenus/loading_native.cpp](../modules/A2FORefitMenus/loading_native.cpp)
- [modules/A2FORefitMenus/loading_native.hpp](../modules/A2FORefitMenus/loading_native.hpp)
- [modules/A2FORefitMenus/loading_presentation.cpp](../modules/A2FORefitMenus/loading_presentation.cpp)
- [modules/A2FORefitMenus/loading_presentation.hpp](../modules/A2FORefitMenus/loading_presentation.hpp)
- [modules/A2FORefitMenus/loading_theme.cpp](../modules/A2FORefitMenus/loading_theme.cpp)
- [modules/A2FORefitMenus/loading_theme.hpp](../modules/A2FORefitMenus/loading_theme.hpp)
- [modules/A2FORefitMenus/menu_scene.cpp](../modules/A2FORefitMenus/menu_scene.cpp)
- [modules/A2FORefitMenus/menu_scene.hpp](../modules/A2FORefitMenus/menu_scene.hpp)
- [modules/A2FORefitMenus/mission_backdrop.cpp](../modules/A2FORefitMenus/mission_backdrop.cpp)
- [modules/A2FORefitMenus/mission_backdrop.hpp](../modules/A2FORefitMenus/mission_backdrop.hpp)
- [modules/A2FORefitMenus/module.cpp](../modules/A2FORefitMenus/module.cpp)
- [modules/A2FORefitMenus/native_bridge.S](../modules/A2FORefitMenus/native_bridge.S)
- [modules/A2FORefitMenus/native_model.hpp](../modules/A2FORefitMenus/native_model.hpp)
- [modules/A2FORefitMenus/presentation.cpp](../modules/A2FORefitMenus/presentation.cpp)
- [modules/A2FORefitMenus/presentation.hpp](../modules/A2FORefitMenus/presentation.hpp)
- [modules/A2FORefitMenus/profile.hpp](../modules/A2FORefitMenus/profile.hpp)
- [modules/A2FORefitMenus/scene_budget.hpp](../modules/A2FORefitMenus/scene_budget.hpp)
- [modules/A2FORefitMenus/scene_catalogue.cpp](../modules/A2FORefitMenus/scene_catalogue.cpp)
- [modules/A2FORefitMenus/scene_catalogue.hpp](../modules/A2FORefitMenus/scene_catalogue.hpp)
- [modules/A2FORefitMenus/scene_composition.cpp](../modules/A2FORefitMenus/scene_composition.cpp)
- [modules/A2FORefitMenus/scene_composition.hpp](../modules/A2FORefitMenus/scene_composition.hpp)
- [modules/A2FORefitMenus/scene_handover.hpp](../modules/A2FORefitMenus/scene_handover.hpp)
- [modules/A2FORefitMenus/scene_mailbox.hpp](../modules/A2FORefitMenus/scene_mailbox.hpp)
- [modules/A2FORefitMenus/scene_settings.hpp](../modules/A2FORefitMenus/scene_settings.hpp)
- [modules/A2FORefitMenus/scene_transition.hpp](../modules/A2FORefitMenus/scene_transition.hpp)
- [modules/A2FORefitMenus/setup_native.cpp](../modules/A2FORefitMenus/setup_native.cpp)
- [modules/A2FORefitMenus/setup_native.hpp](../modules/A2FORefitMenus/setup_native.hpp)
- [modules/A2FORefitMenus/setup_presentation.cpp](../modules/A2FORefitMenus/setup_presentation.cpp)
- [modules/A2FORefitMenus/setup_presentation.hpp](../modules/A2FORefitMenus/setup_presentation.hpp)
- [modules/A2FORefitMenus/setup_skin.cpp](../modules/A2FORefitMenus/setup_skin.cpp)
- [modules/A2FORefitMenus/setup_skin.hpp](../modules/A2FORefitMenus/setup_skin.hpp)
- [modules/A2FORefitMenus/skin.cpp](../modules/A2FORefitMenus/skin.cpp)
- [modules/A2FORefitMenus/skin.hpp](../modules/A2FORefitMenus/skin.hpp)
- [modules/A2FORefitMenus/skin_bindings.hpp](../modules/A2FORefitMenus/skin_bindings.hpp)
- [modules/A2FORefitMenus/skin_native.cpp](../modules/A2FORefitMenus/skin_native.cpp)
- [modules/A2FORefitMenus/skin_native.hpp](../modules/A2FORefitMenus/skin_native.hpp)
- [modules/A2FORefitMenus/status.hpp](../modules/A2FORefitMenus/status.hpp)

### F44: modules/A2FOResources

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOResources/additional_resources.cpp](../modules/A2FOResources/additional_resources.cpp)
- [modules/A2FOResources/additional_resources.hpp](../modules/A2FOResources/additional_resources.hpp)
- [modules/A2FOResources/api.hpp](../modules/A2FOResources/api.hpp)
- [modules/A2FOResources/module.cpp](../modules/A2FOResources/module.cpp)
- [modules/A2FOResources/thiscall_bridge.S](../modules/A2FOResources/thiscall_bridge.S)

### F45: modules/A2FOSOLMaps

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOSOLMaps/module.cpp](../modules/A2FOSOLMaps/module.cpp)
- [modules/A2FOSOLMaps/sol_format.cpp](../modules/A2FOSOLMaps/sol_format.cpp)
- [modules/A2FOSOLMaps/sol_format.hpp](../modules/A2FOSOLMaps/sol_format.hpp)
- [modules/A2FOSOLMaps/thiscall_bridge.S](../modules/A2FOSOLMaps/thiscall_bridge.S)

### F46: modules/A2FOShellDisplay

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOShellDisplay/bink_video.cpp](../modules/A2FOShellDisplay/bink_video.cpp)
- [modules/A2FOShellDisplay/bink_video.hpp](../modules/A2FOShellDisplay/bink_video.hpp)
- [modules/A2FOShellDisplay/canvas.cpp](../modules/A2FOShellDisplay/canvas.cpp)
- [modules/A2FOShellDisplay/canvas.hpp](../modules/A2FOShellDisplay/canvas.hpp)
- [modules/A2FOShellDisplay/delphi_bridge.S](../modules/A2FOShellDisplay/delphi_bridge.S)
- [modules/A2FOShellDisplay/module.cpp](../modules/A2FOShellDisplay/module.cpp)
- [modules/A2FOShellDisplay/native_intro.cpp](../modules/A2FOShellDisplay/native_intro.cpp)
- [modules/A2FOShellDisplay/native_intro.hpp](../modules/A2FOShellDisplay/native_intro.hpp)
- [modules/A2FOShellDisplay/policy.hpp](../modules/A2FOShellDisplay/policy.hpp)
- [modules/A2FOShellDisplay/presentation.hpp](../modules/A2FOShellDisplay/presentation.hpp)

### F47: modules/A2FOSquadrons

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOSquadrons/api.hpp](../modules/A2FOSquadrons/api.hpp)
- [modules/A2FOSquadrons/module.cpp](../modules/A2FOSquadrons/module.cpp)
- [modules/A2FOSquadrons/native_economics.inl](../modules/A2FOSquadrons/native_economics.inl)
- [modules/A2FOSquadrons/native_pod_selection.inl](../modules/A2FOSquadrons/native_pod_selection.inl)
- [modules/A2FOSquadrons/native_repair.inl](../modules/A2FOSquadrons/native_repair.inl)
- [modules/A2FOSquadrons/squadron_config.cpp](../modules/A2FOSquadrons/squadron_config.cpp)
- [modules/A2FOSquadrons/squadron_config.hpp](../modules/A2FOSquadrons/squadron_config.hpp)
- [modules/A2FOSquadrons/squadron_state.cpp](../modules/A2FOSquadrons/squadron_state.cpp)
- [modules/A2FOSquadrons/squadron_state.hpp](../modules/A2FOSquadrons/squadron_state.hpp)
- [modules/A2FOSquadrons/thiscall_bridge.S](../modules/A2FOSquadrons/thiscall_bridge.S)

### F48: modules/A2FOVideo

Recorded state: `pending_audit`. Full implementation closure: **open**.

- [modules/A2FOVideo/delphi_bridge.S](../modules/A2FOVideo/delphi_bridge.S)
- [modules/A2FOVideo/module.cpp](../modules/A2FOVideo/module.cpp)

## Shared refit support

- [ ] [modules/refit_common/bindings.hpp](../modules/refit_common/bindings.hpp)
- [ ] [modules/refit_common/fleetops_profile.hpp](../modules/refit_common/fleetops_profile.hpp)
- [ ] [modules/refit_common/runtime.hpp](../modules/refit_common/runtime.hpp)
- [ ] [modules/refit_common/settings.hpp](../modules/refit_common/settings.hpp)
- [ ] [modules/refit_common/status.hpp](../modules/refit_common/status.hpp)

## Current Makefile standalone/helper/utility target contracts

54 expanded named executable rules: 49 module DLLs (including the SDK example), proxy, two helpers, mission loader and telemetry. These are prerequisites, **not actual per-artifact linker receipts**. Main/media/scene-provider and separate authoring-tool builds must also pass the full product gate. Missing/dormant sources and supported variants need explicit P00 reconciliation.

- [ ] `build/A2FORendererHelper.exe`
- [ ] `build/Win2kDisableTaskSwitch.dll`
- [ ] `build/a2fo_telemetry`
- [ ] `build/missions/mmm_loader.dll`
- [ ] `build/modules/A1Compat.dll`
- [ ] `build/modules/A1Fallbacks.dll`
- [ ] `build/modules/A2FOAlwaysShowShields.dll`
- [ ] `build/modules/A2FOAnimatedHardpoints.dll`
- [ ] `build/modules/A2FOAnimations.dll`
- [ ] `build/modules/A2FOBreeders.dll`
- [ ] `build/modules/A2FOBuildTooltips.dll`
- [ ] `build/modules/A2FOCameraControls.dll`
- [ ] `build/modules/A2FOCheats.dll`
- [ ] `build/modules/A2FOCollectiveXP.dll`
- [ ] `build/modules/A2FOCraftIdentity.dll`
- [ ] `build/modules/A2FODirectionalShields.dll`
- [ ] `build/modules/A2FOEditMenu.dll`
- [ ] `build/modules/A2FOEnergySystems.dll`
- [ ] `build/modules/A2FOFeaturePack.dll`
- [ ] `build/modules/A2FOFireArcs.dll`
- [ ] `build/modules/A2FOHybridBuild.dll`
- [ ] `build/modules/A2FOInfestation.dll`
- [ ] `build/modules/A2FOInfoIni.dll`
- [ ] `build/modules/A2FOInstantActionSettings.dll`
- [ ] `build/modules/A2FOLogMailer.dll`
- [ ] `build/modules/A2FOLogMailerHelper.exe`
- [ ] `build/modules/A2FOM3D.dll`
- [ ] `build/modules/A2FOMissionSelector.dll`
- [ ] `build/modules/A2FOMissions.dll`
- [ ] `build/modules/A2FOMultiYardBuild.dll`
- [ ] `build/modules/A2FOMuzzleFlashes.dll`
- [ ] `build/modules/A2FONebulaRenderer.dll`
- [ ] `build/modules/A2FONormalWeaponTech.dll`
- [ ] `build/modules/A2FOODFVariants.dll`
- [ ] `build/modules/A2FOPointDefenseCycles.dll`
- [ ] `build/modules/A2FORGBTextures.dll`
- [ ] `build/modules/A2FORadarDiagnostics.dll`
- [ ] `build/modules/A2FORecoveryTests.dll`
- [ ] `build/modules/A2FORefitMenus.dll`
- [ ] `build/modules/A2FORefitYards.dll`
- [ ] `build/modules/A2FOResources.dll`
- [ ] `build/modules/A2FOSOLMaps.dll`
- [ ] `build/modules/A2FOShellDisplay.dll`
- [ ] `build/modules/A2FOSquadrons.dll`
- [ ] `build/modules/A2FOStationRotation.dll`
- [ ] `build/modules/A2FOSwarmSystem.dll`
- [ ] `build/modules/A2FOTeamChangeWeapons.dll`
- [ ] `build/modules/A2FOTextureVariants.dll`
- [ ] `build/modules/A2FOTurrets.dll`
- [ ] `build/modules/A2FOVideo.dll`
- [ ] `build/modules/A2FOViewDistance.dll`
- [ ] `build/modules/A2FOWeaponDamageControls.dll`
- [ ] `build/modules/A2FOWreckage.dll`
- [ ] `build/modules/ExampleModule.dll`

## Already-Rust bridge source mappings

These are reuse/verification items under P07, not 48 pending assembly ports. All 48 recorded families are listed; only 44 objects occur in the audited main-DLL manifest. Legacy .S sources may remain as excluded references.

- [rust/a2fo_bridges/core/builtin_camera_controls_bridge.rs](../rust/a2fo_bridges/core/builtin_camera_controls_bridge.rs) — reference `core/builtin_camera_controls_bridge.S`
- [rust/a2fo_bridges/core/builtin_feature_pack_bridge.rs](../rust/a2fo_bridges/core/builtin_feature_pack_bridge.rs) — reference `core/builtin_feature_pack_bridge.S`
- [rust/a2fo_bridges/core/builtin_hybrid_bridge.rs](../rust/a2fo_bridges/core/builtin_hybrid_bridge.rs) — reference `core/builtin_hybrid_bridge.S`
- [rust/a2fo_bridges/core/builtin_view_distance_bridge.rs](../rust/a2fo_bridges/core/builtin_view_distance_bridge.rs) — reference `core/builtin_view_distance_bridge.S`
- [rust/a2fo_bridges/core/delphi_bridge.rs](../rust/a2fo_bridges/core/delphi_bridge.rs) — reference `core/delphi_bridge.S`
- [rust/a2fo_bridges/core/nebula_renderer_bridge.rs](../rust/a2fo_bridges/core/nebula_renderer_bridge.rs) — reference `core/nebula_renderer_bridge.S`
- [rust/a2fo_bridges/modules/A1Compat/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A1Compat/thiscall_bridge.rs) — reference `modules/A1Compat/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A1Fallbacks/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A1Fallbacks/thiscall_bridge.rs) — reference `modules/A1Fallbacks/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOAlwaysShowShields/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOAlwaysShowShields/thiscall_bridge.rs) — reference `modules/A2FOAlwaysShowShields/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOAnimatedHardpoints/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOAnimatedHardpoints/thiscall_bridge.rs) — reference `modules/A2FOAnimatedHardpoints/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOBreeders/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOBreeders/thiscall_bridge.rs) — reference `modules/A2FOBreeders/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOBuildTooltips/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOBuildTooltips/thiscall_bridge.rs) — reference `modules/A2FOBuildTooltips/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOCameraControls/native_bridge.rs](../rust/a2fo_bridges/modules/A2FOCameraControls/native_bridge.rs) — reference `modules/A2FOCameraControls/native_bridge.S`
- [rust/a2fo_bridges/modules/A2FOCheats/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOCheats/thiscall_bridge.rs) — reference `modules/A2FOCheats/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOCollectiveXP/native_bridge.rs](../rust/a2fo_bridges/modules/A2FOCollectiveXP/native_bridge.rs) — reference `modules/A2FOCollectiveXP/native_bridge.S`
- [rust/a2fo_bridges/modules/A2FOCraftIdentity/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOCraftIdentity/thiscall_bridge.rs) — reference `modules/A2FOCraftIdentity/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FODirectionalShields/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FODirectionalShields/thiscall_bridge.rs) — reference `modules/A2FODirectionalShields/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOEditMenu/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOEditMenu/thiscall_bridge.rs) — reference `modules/A2FOEditMenu/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOEnergySystems/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOEnergySystems/thiscall_bridge.rs) — reference `modules/A2FOEnergySystems/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOFeaturePack/delphi_bridge.rs](../rust/a2fo_bridges/modules/A2FOFeaturePack/delphi_bridge.rs) — reference `modules/A2FOFeaturePack/delphi_bridge.S`
- [rust/a2fo_bridges/modules/A2FOFireArcs/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOFireArcs/thiscall_bridge.rs) — reference `modules/A2FOFireArcs/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOHybridBuild/delphi_bridge.rs](../rust/a2fo_bridges/modules/A2FOHybridBuild/delphi_bridge.rs) — reference `modules/A2FOHybridBuild/delphi_bridge.S`
- [rust/a2fo_bridges/modules/A2FOInfestation/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOInfestation/thiscall_bridge.rs) — reference `modules/A2FOInfestation/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOInstantActionSettings/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOInstantActionSettings/thiscall_bridge.rs) — reference `modules/A2FOInstantActionSettings/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOM3D/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOM3D/thiscall_bridge.rs) — reference `modules/A2FOM3D/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOMissionSelector/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOMissionSelector/thiscall_bridge.rs) — reference `modules/A2FOMissionSelector/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOMultiYardBuild/bridges.rs](../rust/a2fo_bridges/modules/A2FOMultiYardBuild/bridges.rs) — reference `modules/A2FOMultiYardBuild/bridges.S`
- [rust/a2fo_bridges/modules/A2FOMuzzleFlashes/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOMuzzleFlashes/thiscall_bridge.rs) — reference `modules/A2FOMuzzleFlashes/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FONebulaRenderer/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FONebulaRenderer/thiscall_bridge.rs) — reference `modules/A2FONebulaRenderer/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FONormalWeaponTech/delphi_bridge.rs](../rust/a2fo_bridges/modules/A2FONormalWeaponTech/delphi_bridge.rs) — reference `modules/A2FONormalWeaponTech/delphi_bridge.S`
- [rust/a2fo_bridges/modules/A2FOODFVariants/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOODFVariants/thiscall_bridge.rs) — reference `modules/A2FOODFVariants/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOPointDefenseCycles/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOPointDefenseCycles/thiscall_bridge.rs) — reference `modules/A2FOPointDefenseCycles/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FORGBTextures/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FORGBTextures/thiscall_bridge.rs) — reference `modules/A2FORGBTextures/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FORecoveryTests/native_bridge.rs](../rust/a2fo_bridges/modules/A2FORecoveryTests/native_bridge.rs) — reference `modules/A2FORecoveryTests/native_bridge.S`
- [rust/a2fo_bridges/modules/A2FORefitMenus/native_bridge.rs](../rust/a2fo_bridges/modules/A2FORefitMenus/native_bridge.rs) — reference `modules/A2FORefitMenus/native_bridge.S`
- [rust/a2fo_bridges/modules/A2FORefitYards/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FORefitYards/thiscall_bridge.rs) — reference `modules/A2FORefitYards/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOResources/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOResources/thiscall_bridge.rs) — reference `modules/A2FOResources/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOSOLMaps/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOSOLMaps/thiscall_bridge.rs) — reference `modules/A2FOSOLMaps/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOShellDisplay/delphi_bridge.rs](../rust/a2fo_bridges/modules/A2FOShellDisplay/delphi_bridge.rs) — reference `modules/A2FOShellDisplay/delphi_bridge.S`
- [rust/a2fo_bridges/modules/A2FOSquadrons/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOSquadrons/thiscall_bridge.rs) — reference `modules/A2FOSquadrons/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOStationRotation/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOStationRotation/thiscall_bridge.rs) — reference `modules/A2FOStationRotation/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOSwarmSystem/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOSwarmSystem/thiscall_bridge.rs) — reference `modules/A2FOSwarmSystem/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOTeamChangeWeapons/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOTeamChangeWeapons/thiscall_bridge.rs) — reference `modules/A2FOTeamChangeWeapons/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOTextureVariants/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOTextureVariants/thiscall_bridge.rs) — reference `modules/A2FOTextureVariants/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOTurrets/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOTurrets/thiscall_bridge.rs) — reference `modules/A2FOTurrets/thiscall_bridge.S`
- [rust/a2fo_bridges/modules/A2FOVideo/delphi_bridge.rs](../rust/a2fo_bridges/modules/A2FOVideo/delphi_bridge.rs) — reference `modules/A2FOVideo/delphi_bridge.S`
- [rust/a2fo_bridges/modules/A2FOViewDistance/native_bridge.rs](../rust/a2fo_bridges/modules/A2FOViewDistance/native_bridge.rs) — reference `modules/A2FOViewDistance/native_bridge.S`
- [rust/a2fo_bridges/modules/A2FOWeaponDamageControls/thiscall_bridge.rs](../rust/a2fo_bridges/modules/A2FOWeaponDamageControls/thiscall_bridge.rs) — reference `modules/A2FOWeaponDamageControls/thiscall_bridge.S`

## Required unresolved provenance work

- [ ] Reconcile current source/Makefile with the historical candidate; source existence is not byte identity.
- [ ] Produce actual link maps/member origins for all standalone DLLs, sidecars, proxy, helpers and supported utilities.
- [ ] Resolve native CRT/bootstrap/exception/thread/compiler-support provenance for every target.
- [ ] Expand executable headers, generated code, dynamic dependencies and optional features beyond this quoted-include scan.
- [ ] Reconcile all installation base/parent/mod directories; do not rely on the five-binary tester update as the complete product manifest.
- [ ] Audit all production Cargo/rustc dependencies, build scripts and vendor subprojects, including already-Rust artifacts.
- [ ] Close P00–P19 and F01–F48 using the report evidence rules; retaining an old native binary or dropping a supported feature fails closure.
