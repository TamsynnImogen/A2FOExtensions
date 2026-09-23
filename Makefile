CXX := i686-w64-mingw32-g++
CXX_HOST ?= g++
OBJDUMP := i686-w64-mingw32-objdump

.DEFAULT_GOAL := release

CPPFLAGS := -DWIN32_LEAN_AND_MEAN
CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra -Wpedantic -DNDEBUG
DLLFLAGS := -shared -static -static-libgcc -static-libstdc++ \
	-Wl,--enable-stdcall-fixup

BUILD_DIR := build
MODULE_DIR := $(BUILD_DIR)/modules
STA1_CLASSIC_DIR := $(BUILD_DIR)/sta1-classic
STA1_COMPAT_MODULE := $(MODULE_DIR)/A1Compat.dll
A1_FALLBACKS_MODULE := $(MODULE_DIR)/A1Fallbacks.dll
ALWAYS_SHOW_SHIELDS_MODULE := $(MODULE_DIR)/A2FOAlwaysShowShields.dll
ANIMATIONS_MODULE := $(MODULE_DIR)/A2FOAnimations.dll
ANIMATED_HARDPOINTS_MODULE := $(MODULE_DIR)/A2FOAnimatedHardpoints.dll
BUILD_TOOLTIPS_MODULE := $(MODULE_DIR)/A2FOBuildTooltips.dll
CHEATS_MODULE := $(MODULE_DIR)/A2FOCheats.dll
CRAFT_IDENTITY_MODULE := $(MODULE_DIR)/A2FOCraftIdentity.dll
EDIT_MENU_MODULE := $(MODULE_DIR)/A2FOEditMenu.dll
DIRECTIONAL_SHIELDS_MODULE := $(MODULE_DIR)/A2FODirectionalShields.dll
ENERGY_SYSTEMS_MODULE := $(MODULE_DIR)/A2FOEnergySystems.dll
MUZZLE_FLASHES_MODULE := $(MODULE_DIR)/A2FOMuzzleFlashes.dll
INSTANT_ACTION_SETTINGS_MODULE := $(MODULE_DIR)/A2FOInstantActionSettings.dll
RESOURCES_MODULE := $(MODULE_DIR)/A2FOResources.dll
MISSION_SELECTOR_MODULE := $(MODULE_DIR)/A2FOMissionSelector.dll
FIRE_ARCS_MODULE := $(MODULE_DIR)/A2FOFireArcs.dll
WEAPON_DAMAGE_CONTROLS_MODULE := $(MODULE_DIR)/A2FOWeaponDamageControls.dll
WRECKAGE_MODULE := $(MODULE_DIR)/A2FOWreckage.dll
NORMAL_WEAPON_TECH_MODULE := $(MODULE_DIR)/A2FONormalWeaponTech.dll
NEBULA_RENDERER_MODULE := $(MODULE_DIR)/A2FONebulaRenderer.dll
POINT_DEFENSE_CYCLES_MODULE := $(MODULE_DIR)/A2FOPointDefenseCycles.dll
SWARM_SYSTEM_MODULE := $(MODULE_DIR)/A2FOSwarmSystem.dll
TEXTURE_VARIANTS_MODULE := $(MODULE_DIR)/A2FOTextureVariants.dll
ODF_VARIANTS_MODULE := $(MODULE_DIR)/A2FOODFVariants.dll
TURRETS_MODULE := $(MODULE_DIR)/A2FOTurrets.dll
REFIT_YARDS_MODULE := $(MODULE_DIR)/A2FORefitYards.dll
SQUADRONS_MODULE := $(MODULE_DIR)/A2FOSquadrons.dll
STATION_ROTATION_MODULE := $(MODULE_DIR)/A2FOStationRotation.dll
TEAM_CHANGE_WEAPONS_MODULE := $(MODULE_DIR)/A2FOTeamChangeWeapons.dll
TEAM_CHANGE_WEAPONS_SMOKE := $(BUILD_DIR)/team_change_weapons_smoke.exe
NEBULA_SHADER_ASSETS := \
	$(BUILD_DIR)/Shaders/dot3_amd.nvv \
	$(BUILD_DIR)/Shaders/dot3_amd9.nvv \
	$(BUILD_DIR)/Shaders/dx8/pixel/ps.nvv \
	$(BUILD_DIR)/Shaders/dx8/pixel/ps_specular.nvv \
	$(BUILD_DIR)/Shaders/dx8/vertex/vs_flat_lighting.nvv
NEBULA_LICENSE := $(BUILD_DIR)/licenses/armada-nebula-patch.txt
STA1_CLASSIC_GUI_CFG := \
	mods/STA1Classic/misc/gui_fed.cfg \
	mods/STA1Classic/misc/gui_bor.cfg \
	mods/STA1Classic/misc/gui_kli.cfg \
	mods/STA1Classic/misc/gui_rom.cfg
STA1_CLASSIC_SPRITE_REGISTRY := mods/STA1Classic/Sprites/sprites.spr
STA1_CLASSIC_MISSION_SELECTOR := mods/STA1Classic/mission_selector.ini
SMOKE_TEST := $(BUILD_DIR)/dll_load_smoke.exe
GAME_MONITOR_SMOKE := $(BUILD_DIR)/game_monitor_win32_smoke.exe
FPQ_PATHS_TEST := $(BUILD_DIR)/fpq_paths_test
ODF_PATHS_TEST := $(BUILD_DIR)/odf_paths_test
ODF_MODULE_SMOKE := $(BUILD_DIR)/odf_module_init_smoke.exe
EXTENSION_ROOTS_TEST := $(BUILD_DIR)/extension_roots_test
EXTENSION_ROOT_SMOKE := $(BUILD_DIR)/extension_root_discovery_smoke.exe
MODULE_API_TEST := $(BUILD_DIR)/module_api_test
MODULE_POLICY_TEST := $(BUILD_DIR)/module_policy_test
HYBRID_PRODUCTION_TEST := $(BUILD_DIR)/hybrid_production_test
BUILD_SUBMENU_CONFIG_TEST := $(BUILD_DIR)/build_submenu_config_test
TURRET_MATH_TEST := $(BUILD_DIR)/turret_math_test
CRAFT_IDENTITY_TEST := $(BUILD_DIR)/craft_identity_test
EDIT_MENU_TEST := $(BUILD_DIR)/edit_menu_test
ENERGY_SYSTEMS_TEST := $(BUILD_DIR)/energy_systems_test
DIRECTIONAL_SHIELDS_TEST := $(BUILD_DIR)/directional_shields_test
INSTANT_ACTION_SETTINGS_TEST := $(BUILD_DIR)/instant_action_settings_test
BUILD_TIME_TEXT_TEST := $(BUILD_DIR)/build_time_text_test
ADDITIONAL_RESOURCES_TEST := $(BUILD_DIR)/additional_resources_test
A1_RACE_MENU_TEST := $(BUILD_DIR)/a1_race_menu_policy_test
A1_TEAM_COLOR_TEST := $(BUILD_DIR)/a1_team_color_policy_test
A1_BZN_POLICY_TEST := $(BUILD_DIR)/a1_bzn_policy_test
A1_UI_POLICY_TEST := $(BUILD_DIR)/a1_ui_policy_test
A1_FALLBACK_POLICY_TEST := $(BUILD_DIR)/a1_fallback_policy_test
FIRE_ARC_TEST := $(BUILD_DIR)/fire_arc_test
UPGRADE_POD_CONFIG_TEST := $(BUILD_DIR)/upgrade_pod_config_test
WRECKAGE_POLICY_TEST := $(BUILD_DIR)/wreckage_policy_test
WEAPON_DAMAGE_CONTROLS_TEST := $(BUILD_DIR)/weapon_damage_controls_test
SHIELD_VISIBILITY_TEST := $(BUILD_DIR)/shield_visibility_test
NEBULA_EMISSIVE_TEST := $(BUILD_DIR)/nebula_emissive_test
AMD_DOT3_COMPAT_TEST := $(BUILD_DIR)/amd_dot3_compat_test
COM_OWNER_TEST := $(BUILD_DIR)/com_owner_test
GAME_MONITOR_POLICY_TEST := $(BUILD_DIR)/game_monitor_policy_test
ART_TEXTURE_SUFFIX_CONFIG_TEST := $(BUILD_DIR)/art_texture_suffix_config_test
DECAL_MATH_TEST := $(BUILD_DIR)/decal_math_test
POINT_DEFENSE_CYCLE_TEST := $(BUILD_DIR)/point_defense_cycle_test
SWARM_MOTION_TEST := $(BUILD_DIR)/swarm_motion_test
TEXTURE_VARIANTS_TEST := $(BUILD_DIR)/texture_variants_test
REFIT_POLICY_TEST := $(BUILD_DIR)/refit_policy_test
SQUADRONS_TEST := $(BUILD_DIR)/squadrons_test
SQUADRONS_X86_TEST := $(BUILD_DIR)/squadrons_test.exe
SQUADRONS_SOURCES := modules/A2FOSquadrons/squadron_config.cpp \
	modules/A2FOSquadrons/squadron_state.cpp
SQUADRONS_HEADERS := modules/A2FOSquadrons/squadron_config.hpp \
	modules/A2FOSquadrons/squadron_state.hpp
STATION_ROTATION_TEST := $(BUILD_DIR)/station_rotation_test
STATION_ROTATION_SMOKE := $(BUILD_DIR)/station_rotation_smoke.exe
A2FO_TELEMETRY := $(BUILD_DIR)/a2fo_telemetry
A2FO_RENDERER_HELPER := $(BUILD_DIR)/A2FORendererHelper.exe
ARCLAB_DIR := tools/A2FOArcLab

CORE_SOURCES := \
	core/dllmain.cpp \
	core/extension_roots.cpp \
	core/module_menu.cpp \
	core/module_loader.cpp \
	core/module_policy.cpp \
	core/renderer_options.cpp \
	core/game_monitor.cpp \
	core/game_monitor_policy.cpp \
	core/decal_math.cpp \
	core/nebula_emissive.cpp \
	core/nebula_renderer.cpp \
	core/hook.cpp \
	core/nebula_renderer_bridge.S \
	core/delphi_bridge.S

.PHONY: all release sta1-classic verify-sta1-classic sdk-examples clean \
	verify verify-sdk test smoke odf-module-smoke extension-root-smoke \
	arclab arclab-test telemetry

all: release

arclab:
	cargo build --release --locked --manifest-path $(ARCLAB_DIR)/Cargo.toml

arclab-test:
	cargo test --locked --manifest-path $(ARCLAB_DIR)/Cargo.toml

telemetry: $(A2FO_TELEMETRY)

$(A2FO_TELEMETRY): tools/a2fo_telemetry.cpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-o $@ tools/a2fo_telemetry.cpp -ldl

$(A2FO_RENDERER_HELPER): tools/a2fo_renderer_helper.cpp \
		core/build_identity.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-o $@ tools/a2fo_renderer_helper.cpp

release: \
	$(BUILD_DIR)/A2FOExtensions.dll \
	$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
	$(A2FO_RENDERER_HELPER) \
	$(ALWAYS_SHOW_SHIELDS_MODULE) \
	$(ANIMATED_HARDPOINTS_MODULE) \
	$(ANIMATIONS_MODULE) \
	$(BUILD_TOOLTIPS_MODULE) \
	$(MODULE_DIR)/A2FOFeaturePack.dll \
	$(MODULE_DIR)/A2FOHybridBuild.dll \
	$(MODULE_DIR)/A2FOInfoIni.dll \
	$(CHEATS_MODULE) \
	$(CRAFT_IDENTITY_MODULE) \
	$(EDIT_MENU_MODULE) \
	$(DIRECTIONAL_SHIELDS_MODULE) \
	$(ENERGY_SYSTEMS_MODULE) \
	$(MUZZLE_FLASHES_MODULE) \
	$(INSTANT_ACTION_SETTINGS_MODULE) \
	$(RESOURCES_MODULE) \
	$(MISSION_SELECTOR_MODULE) \
	$(FIRE_ARCS_MODULE) \
	$(WEAPON_DAMAGE_CONTROLS_MODULE) \
	$(WRECKAGE_MODULE) \
	$(NORMAL_WEAPON_TECH_MODULE) \
	$(NEBULA_RENDERER_MODULE) \
	$(POINT_DEFENSE_CYCLES_MODULE) \
	$(SWARM_SYSTEM_MODULE) \
	$(TEXTURE_VARIANTS_MODULE) \
	$(ODF_VARIANTS_MODULE) \
	$(STA1_COMPAT_MODULE) \
	$(A1_FALLBACKS_MODULE) \
	$(NEBULA_SHADER_ASSETS) \
	$(NEBULA_LICENSE) \
	$(TURRETS_MODULE) \
	$(REFIT_YARDS_MODULE) \
	$(SQUADRONS_MODULE) \
	$(STATION_ROTATION_MODULE) \
	$(TEAM_CHANGE_WEAPONS_MODULE) \
	$(MODULE_DIR)/A2FORGBTextures.dll

sta1-classic: release $(STA1_COMPAT_MODULE) $(STA1_CLASSIC_GUI_CFG) \
		$(STA1_CLASSIC_SPRITE_REGISTRY) $(STA1_CLASSIC_MISSION_SELECTOR)
	rm -rf $(STA1_CLASSIC_DIR)/modules
	mkdir -p $(STA1_CLASSIC_DIR)/AI $(STA1_CLASSIC_DIR)/bzn \
		$(STA1_CLASSIC_DIR)/misc $(STA1_CLASSIC_DIR)/odf \
		$(STA1_CLASSIC_DIR)/sod $(STA1_CLASSIC_DIR)/sounds \
		$(STA1_CLASSIC_DIR)/sprites $(STA1_CLASSIC_DIR)/techtree \
		$(STA1_CLASSIC_DIR)/textures
	cp mods/STA1Classic/info.ini $(STA1_CLASSIC_DIR)/info.ini
	cp mods/STA1Classic/a1compat.ini $(STA1_CLASSIC_DIR)/a1compat.ini
	cp $(STA1_CLASSIC_MISSION_SELECTOR) $(STA1_CLASSIC_DIR)/mission_selector.ini
	cp mods/STA1Classic/README.md $(STA1_CLASSIC_DIR)/README.md
	cp $(STA1_CLASSIC_GUI_CFG) $(STA1_CLASSIC_DIR)/misc/
	cp $(STA1_CLASSIC_SPRITE_REGISTRY) $(STA1_CLASSIC_DIR)/sprites/

sdk-examples: $(MODULE_DIR)/ExampleModule.dll

$(BUILD_DIR):
	mkdir -p $@

$(MODULE_DIR):
	mkdir -p $@

$(BUILD_DIR)/A2FOExtensions.dll: $(CORE_SOURCES) \
		core/build_identity.hpp \
		core/game_monitor.hpp \
		core/game_monitor_policy.hpp \
		core/renderer_draw_policy.hpp \
		sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ $(CORE_SOURCES) -lcomctl32 -lgdi32

$(BUILD_DIR)/Win2kDisableTaskSwitch.dll: \
		core/startup_proxy.cpp core/startup_proxy.def | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ core/startup_proxy.cpp core/startup_proxy.def

$(ALWAYS_SHOW_SHIELDS_MODULE): \
		modules/A2FOAlwaysShowShields/module.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.hpp \
		modules/A2FOAlwaysShowShields/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOAlwaysShowShields/module.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp \
		modules/A2FOAlwaysShowShields/thiscall_bridge.S

$(ANIMATIONS_MODULE): modules/A2FOAnimations/module.cpp \
		modules/A2FOAnimations/playback.hpp modules/A2FOAnimations/api.hpp \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ modules/A2FOAnimations/module.cpp

$(BUILD_DIR)/animations_test: tests/animations_test.cpp modules/A2FOAnimations/playback.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ tests/animations_test.cpp

$(BUILD_DIR)/animations_native_test.exe: tests/animations_native_test.cpp \
		modules/A2FOAnimations/module.cpp modules/A2FOAnimations/playback.hpp \
		modules/A2FOAnimations/api.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -static -o $@ tests/animations_native_test.cpp

$(BUILD_DIR)/animations_runtime_init_smoke.exe: tests/animations_runtime_init_smoke.cpp \
		modules/A2FOAnimations/module.cpp modules/A2FOAnimations/playback.hpp \
		modules/A2FOAnimations/api.hpp sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -static -o $@ tests/animations_runtime_init_smoke.cpp

$(BUILD_DIR)/muzzle_animation_bridge_test.exe: tests/muzzle_animation_bridge_test.cpp \
		modules/A2FOMuzzleFlashes/module.cpp modules/A2FOMuzzleFlashes/thiscall_bridge.S \
		modules/A2FOAnimations/api.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -static -o $@ \
		tests/muzzle_animation_bridge_test.cpp modules/A2FOMuzzleFlashes/thiscall_bridge.S

.PHONY: animations-test
animations-test: $(BUILD_DIR)/animations_test
	./$(BUILD_DIR)/animations_test

$(ANIMATED_HARDPOINTS_MODULE): modules/A2FOAnimations/api.hpp \
		modules/A2FOAnimatedHardpoints/module.cpp \
		modules/A2FOAnimatedHardpoints/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOAnimatedHardpoints/module.cpp \
		modules/A2FOAnimatedHardpoints/thiscall_bridge.S

$(BUILD_TOOLTIPS_MODULE): \
		modules/A2FOBuildTooltips/module.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp \
		modules/A2FOBuildTooltips/build_time_text.hpp \
		modules/A2FOResources/api.hpp \
		modules/A2FOBuildTooltips/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOBuildTooltips/module.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp \
		modules/A2FOBuildTooltips/thiscall_bridge.S

$(MODULE_DIR)/ExampleModule.dll: \
		sdk/examples/ExampleModule/example_module.cpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-Isdk/include \
		-o $@ sdk/examples/ExampleModule/example_module.cpp

$(MODULE_DIR)/A2FOFeaturePack.dll: \
		modules/A2FOFeaturePack/odf_recursive.cpp \
		modules/A2FOFeaturePack/bink_video.cpp \
		modules/A2FOFeaturePack/bink_video.hpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp \
		modules/A2FOFeaturePack/hybrid_bridge_api.hpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.hpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/queue_enhancement.hpp \
		modules/A2FOFeaturePack/upgrade_pods.cpp \
		modules/A2FOFeaturePack/upgrade_pods.hpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.hpp \
		modules/A2FOFeaturePack/delphi_bridge.S \
		core/fpq_paths.cpp core/fpq_paths.hpp \
		core/odf_paths.cpp core/odf_paths.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOFeaturePack/odf_recursive.cpp \
		modules/A2FOFeaturePack/bink_video.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/upgrade_pods.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp \
		modules/A2FOFeaturePack/delphi_bridge.S \
		core/fpq_paths.cpp core/odf_paths.cpp \
		-lgdi32

$(MODULE_DIR)/A2FOHybridBuild.dll: \
		modules/A2FOHybridBuild/module.cpp \
		modules/A2FOHybridBuild/delphi_bridge.S \
		modules/A2FOFeaturePack/hybrid_bridge_api.hpp \
		modules/A2FOHybridBuild/refit_ui_bridge_api.hpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp \
		modules/A2FOHybridBuild/build_submenu_config.hpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production.hpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.cpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOHybridBuild/module.cpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.cpp \
		modules/A2FOHybridBuild/delphi_bridge.S

$(REFIT_YARDS_MODULE): \
		modules/A2FORefitYards/module.cpp \
		modules/A2FORefitYards/docking_transform.hpp \
		modules/A2FORefitYards/refit_policy.cpp \
		modules/A2FORefitYards/refit_policy.hpp \
		modules/A2FORefitYards/thiscall_bridge.S \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOHybridBuild/refit_ui_bridge_api.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FORefitYards/module.cpp \
		modules/A2FORefitYards/refit_policy.cpp \
		modules/A2FORefitYards/thiscall_bridge.S

$(SQUADRONS_MODULE): modules/A2FOSquadrons/native_repair.inl \
		modules/A2FOSquadrons/native_economics.inl \
		modules/A2FOSquadrons/api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOSquadrons/module.cpp \
		modules/A2FOSquadrons/squadron_config.cpp \
		modules/A2FOSquadrons/squadron_config.hpp \
		modules/A2FOSquadrons/squadron_state.cpp \
		modules/A2FOSquadrons/squadron_state.hpp \
		modules/A2FOSquadrons/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOSquadrons/module.cpp \
		modules/A2FOSquadrons/squadron_config.cpp \
		modules/A2FOSquadrons/squadron_state.cpp \
		modules/A2FOSquadrons/thiscall_bridge.S

$(TEAM_CHANGE_WEAPONS_MODULE): modules/A2FOTeamChangeWeapons/module.cpp \
		modules/A2FOTeamChangeWeapons/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ \
		modules/A2FOTeamChangeWeapons/module.cpp \
		modules/A2FOTeamChangeWeapons/thiscall_bridge.S

$(TEAM_CHANGE_WEAPONS_SMOKE): tests/team_change_weapons_smoke.cpp \
		modules/A2FOTeamChangeWeapons/module.cpp \
		modules/A2FOTeamChangeWeapons/thiscall_bridge.S \
		core/hook.cpp core/hook.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< \
		core/hook.cpp modules/A2FOTeamChangeWeapons/thiscall_bridge.S

$(STATION_ROTATION_MODULE): modules/A2FOStationRotation/module.cpp \
		modules/A2FOStationRotation/rotation.hpp \
		modules/A2FOStationRotation/footprint.hpp \
		modules/A2FOStationRotation/footprint_runtime.inl \
		modules/A2FOStationRotation/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ \
		modules/A2FOStationRotation/module.cpp \
		modules/A2FOStationRotation/thiscall_bridge.S

$(STATION_ROTATION_TEST): tests/station_rotation_test.cpp \
		modules/A2FOStationRotation/rotation.hpp \
		modules/A2FOStationRotation/footprint.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ $<

$(STATION_ROTATION_SMOKE): tests/station_rotation_smoke.cpp \
		tests/station_rotation_smoke_bridge.S \
		modules/A2FOStationRotation/module.cpp \
		modules/A2FOStationRotation/rotation.hpp \
		modules/A2FOStationRotation/footprint.hpp \
		modules/A2FOStationRotation/footprint_runtime.inl \
		modules/A2FOStationRotation/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< \
		tests/station_rotation_smoke_bridge.S \
		modules/A2FOStationRotation/thiscall_bridge.S

$(MODULE_DIR)/A2FOInfoIni.dll: \
		modules/A2FOInfoIni/module.cpp \
		core/extension_roots.cpp core/extension_roots.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOInfoIni/module.cpp core/extension_roots.cpp

$(CHEATS_MODULE): \
		modules/A2FOCheats/module.cpp \
		modules/A2FOCheats/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOCheats/module.cpp \
		modules/A2FOCheats/thiscall_bridge.S

$(CRAFT_IDENTITY_MODULE): \
		modules/A2FOCraftIdentity/module.cpp \
		modules/A2FOCraftIdentity/selected_panel_anchor.hpp \
		modules/A2FOCraftIdentity/object_editor.cpp \
		modules/A2FOCraftIdentity/object_editor.hpp \
		modules/A2FOCraftIdentity/fleetops_object_editor.cpp \
		modules/A2FOCraftIdentity/fleetops_object_editor.hpp \
		modules/A2FOCraftIdentity/object_editor_state.cpp \
		modules/A2FOCraftIdentity/object_editor_state.hpp \
		sdk/include/a2fo_shield_values.hpp \
		sdk/include/a2fo_supported_armada.hpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.hpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.hpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.cpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.hpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/identity_selection.hpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp \
		modules/A2FOCraftIdentity/system_icon_state.hpp \
		modules/A2FODirectionalShields/api.hpp \
		modules/A2FOCraftIdentity/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOCraftIdentity/module.cpp \
		modules/A2FOCraftIdentity/object_editor.cpp \
		modules/A2FOCraftIdentity/object_editor_state.cpp \
		modules/A2FOCraftIdentity/fleetops_object_editor.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp \
		modules/A2FOCraftIdentity/thiscall_bridge.S

$(BUILD_DIR)/craft_identity_panel_smoke.exe: tests/craft_identity_panel_smoke.cpp \
		$(CRAFT_IDENTITY_MODULE) core/hook.cpp core/hook.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< \
		core/hook.cpp $(filter-out modules/A2FOCraftIdentity/module.cpp,\
		$(wildcard modules/A2FOCraftIdentity/*.cpp)) \
		modules/A2FOCraftIdentity/thiscall_bridge.S

$(EDIT_MENU_MODULE): \
		modules/A2FOEditMenu/module.cpp \
		modules/A2FOEditMenu/edit_menu_odf.cpp \
		modules/A2FOEditMenu/edit_menu_odf.hpp \
		modules/A2FOEditMenu/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOEditMenu/module.cpp \
		modules/A2FOEditMenu/edit_menu_odf.cpp \
		modules/A2FOEditMenu/thiscall_bridge.S

$(INSTANT_ACTION_SETTINGS_MODULE): \
		modules/A2FOInstantActionSettings/module.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.hpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.hpp \
		modules/A2FOInstantActionSettings/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOInstantActionSettings/module.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp \
		modules/A2FOInstantActionSettings/thiscall_bridge.S

$(MISSION_SELECTOR_MODULE): \
		modules/A2FOMissionSelector/module.cpp \
		modules/A2FOMissionSelector/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOMissionSelector/module.cpp \
		modules/A2FOMissionSelector/thiscall_bridge.S \
		-lcomctl32 -lgdi32 -lgdiplus

$(FIRE_ARCS_MODULE): \
		modules/A2FOFireArcs/module.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/fire_arc.hpp \
		modules/A2FOFireArcs/runtime_config.cpp \
		modules/A2FOFireArcs/runtime_config.hpp \
		modules/A2FOFireArcs/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOFireArcs/module.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/runtime_config.cpp \
		modules/A2FOFireArcs/thiscall_bridge.S

$(ENERGY_SYSTEMS_MODULE): \
		modules/A2FOEnergySystems/module.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/energy_systems.hpp \
		modules/A2FOEnergySystems/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOEnergySystems/module.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/thiscall_bridge.S

$(MUZZLE_FLASHES_MODULE): modules/A2FOAnimations/api.hpp \
		modules/A2FOMuzzleFlashes/module.cpp \
		modules/A2FOMuzzleFlashes/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOMuzzleFlashes/module.cpp \
		modules/A2FOMuzzleFlashes/thiscall_bridge.S

$(DIRECTIONAL_SHIELDS_MODULE): \
		modules/A2FODirectionalShields/module.cpp \
		sdk/include/a2fo_shield_values.hpp \
		modules/A2FODirectionalShields/directional_shields.cpp \
		modules/A2FODirectionalShields/directional_shields.hpp \
		modules/A2FODirectionalShields/api.hpp \
		modules/A2FODirectionalShields/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FODirectionalShields/module.cpp \
		modules/A2FODirectionalShields/directional_shields.cpp \
		modules/A2FODirectionalShields/thiscall_bridge.S

$(WEAPON_DAMAGE_CONTROLS_MODULE): \
		modules/A2FOWeaponDamageControls/module.cpp \
		modules/A2FOWeaponDamageControls/damage_controls.hpp \
		modules/A2FODirectionalShields/api.hpp \
		modules/A2FOWeaponDamageControls/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOWeaponDamageControls/module.cpp \
		modules/A2FOWeaponDamageControls/thiscall_bridge.S

$(NORMAL_WEAPON_TECH_MODULE): \
		modules/A2FONormalWeaponTech/module.cpp \
		modules/A2FONormalWeaponTech/delphi_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FONormalWeaponTech/module.cpp \
		modules/A2FONormalWeaponTech/delphi_bridge.S

$(NEBULA_RENDERER_MODULE): \
		modules/A2FONebulaRenderer/module.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.hpp \
		modules/A2FONebulaRenderer/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FONebulaRenderer/module.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp \
		modules/A2FONebulaRenderer/thiscall_bridge.S

$(POINT_DEFENSE_CYCLES_MODULE): \
		modules/A2FOPointDefenseCycles/module.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.hpp \
		modules/A2FOPointDefenseCycles/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOPointDefenseCycles/module.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp \
		modules/A2FOPointDefenseCycles/thiscall_bridge.S

$(SWARM_SYSTEM_MODULE): \
		modules/A2FOSwarmSystem/module.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp \
		modules/A2FOSwarmSystem/swarm_motion.hpp \
		modules/A2FOSwarmSystem/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOSwarmSystem/module.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp \
		modules/A2FOSwarmSystem/thiscall_bridge.S

$(BUILD_DIR)/Shaders/dx8/pixel/%.nvv: \
		modules/A2FONebulaRenderer/Shaders/dx8/pixel/%.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/Shaders/dx8/vertex/%.nvv: \
		modules/A2FONebulaRenderer/Shaders/dx8/vertex/%.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/Shaders/dot3_amd.nvv: \
		modules/A2FONebulaRenderer/Shaders/dot3_amd.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/Shaders/dot3_amd9.nvv: \
		modules/A2FONebulaRenderer/Shaders/dot3_amd9.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(NEBULA_LICENSE): third_party/armada-nebula-patch/LICENSE.txt | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(TURRETS_MODULE): \
		modules/A2FOTurrets/module.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_combat.hpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/turret_math.hpp \
		modules/A2FOTurrets/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOTurrets/module.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/thiscall_bridge.S

$(MODULE_DIR)/A2FORGBTextures.dll: \
		modules/A2FORGBTextures/module.cpp \
		modules/A2FORGBTextures/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FORGBTextures/module.cpp \
		modules/A2FORGBTextures/thiscall_bridge.S

$(TEXTURE_VARIANTS_MODULE): \
		modules/A2FOTextureVariants/module.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp \
		modules/A2FOTextureVariants/texture_variants.hpp \
		modules/A2FOTextureVariants/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOTextureVariants/module.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp \
		modules/A2FOTextureVariants/thiscall_bridge.S

$(ODF_VARIANTS_MODULE): \
		modules/A2FOODFVariants/module.cpp \
		modules/A2FOODFVariants/thiscall_bridge.S \
		sdk/include/a2fo_faction_suffix.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOODFVariants/module.cpp \
		modules/A2FOODFVariants/thiscall_bridge.S

$(RESOURCES_MODULE): \
		modules/A2FOSquadrons/api.hpp \
		modules/A2FOResources/module.cpp \
		modules/A2FOResources/additional_resources.cpp \
		modules/A2FOResources/additional_resources.hpp \
		modules/A2FOResources/api.hpp \
		modules/A2FOResources/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOResources/module.cpp \
		modules/A2FOResources/additional_resources.cpp \
		modules/A2FOResources/thiscall_bridge.S

$(WRECKAGE_MODULE): \
		modules/A2FOWreckage/module.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp \
		modules/A2FOWreckage/wreckage_policy.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOWreckage/module.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp

$(STA1_COMPAT_MODULE): \
		modules/A1Compat/module.cpp \
		modules/A1Compat/a1_bzn_policy.cpp \
		modules/A1Compat/a1_bzn_policy.hpp \
		modules/A1Compat/a1_ui_policy.hpp \
		modules/A1Compat/race_menu_policy.cpp \
		modules/A1Compat/race_menu_policy.hpp \
		modules/A1Compat/team_color_policy.cpp \
		modules/A1Compat/team_color_policy.hpp \
		modules/A1Compat/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A1Compat/module.cpp \
		modules/A1Compat/a1_bzn_policy.cpp \
		modules/A1Compat/race_menu_policy.cpp \
		modules/A1Compat/team_color_policy.cpp \
		modules/A1Compat/thiscall_bridge.S

$(A1_FALLBACKS_MODULE): \
		modules/A1Fallbacks/module.cpp \
		modules/A1Fallbacks/fallback_policy.cpp \
		modules/A1Fallbacks/fallback_policy.hpp \
		modules/A1Fallbacks/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A1Fallbacks/module.cpp \
		modules/A1Fallbacks/fallback_policy.cpp \
		modules/A1Fallbacks/thiscall_bridge.S

$(SMOKE_TEST): tests/dll_load_smoke.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-o $@ $<

$(BUILD_DIR)/supported_armada_smoke.exe: tests/supported_armada_smoke.cpp \
		sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-o $@ $<

$(GAME_MONITOR_SMOKE): tests/game_monitor_win32_smoke.cpp \
		core/game_monitor.cpp core/game_monitor.hpp \
		core/game_monitor_policy.cpp core/game_monitor_policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-Icore -o $@ tests/game_monitor_win32_smoke.cpp \
		core/game_monitor.cpp core/game_monitor_policy.cpp

$(FPQ_PATHS_TEST): tests/fpq_paths_test.cpp \
		core/fpq_paths.cpp core/fpq_paths.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/fpq_paths_test.cpp core/fpq_paths.cpp

$(ADDITIONAL_RESOURCES_TEST): \
		tests/additional_resources_test.cpp \
		modules/A2FOResources/additional_resources.cpp \
		modules/A2FOResources/additional_resources.hpp \
		modules/A2FOResources/api.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOResources -o $@ \
		tests/additional_resources_test.cpp \
		modules/A2FOResources/additional_resources.cpp

$(A1_RACE_MENU_TEST): \
		tests/a1_race_menu_policy_test.cpp \
		modules/A1Compat/race_menu_policy.cpp \
		modules/A1Compat/race_menu_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ \
		tests/a1_race_menu_policy_test.cpp \
		modules/A1Compat/race_menu_policy.cpp

$(A1_TEAM_COLOR_TEST): \
		tests/a1_team_color_policy_test.cpp \
		modules/A1Compat/team_color_policy.cpp \
		modules/A1Compat/team_color_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ \
		tests/a1_team_color_policy_test.cpp \
		modules/A1Compat/team_color_policy.cpp

$(A1_BZN_POLICY_TEST): \
		tests/a1_bzn_policy_test.cpp \
		modules/A1Compat/a1_bzn_policy.cpp \
		modules/A1Compat/a1_bzn_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ \
		tests/a1_bzn_policy_test.cpp \
		modules/A1Compat/a1_bzn_policy.cpp

$(A1_UI_POLICY_TEST): \
		tests/a1_ui_policy_test.cpp \
		modules/A1Compat/a1_ui_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ tests/a1_ui_policy_test.cpp

$(A1_FALLBACK_POLICY_TEST): \
		tests/a1_fallback_policy_test.cpp \
		modules/A1Fallbacks/fallback_policy.cpp \
		modules/A1Fallbacks/fallback_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Fallbacks -o $@ \
		tests/a1_fallback_policy_test.cpp \
		modules/A1Fallbacks/fallback_policy.cpp

$(ODF_PATHS_TEST): tests/odf_paths_test.cpp \
		core/odf_paths.cpp core/odf_paths.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/odf_paths_test.cpp core/odf_paths.cpp

$(EXTENSION_ROOTS_TEST): tests/extension_roots_test.cpp \
		core/extension_roots.cpp core/extension_roots.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/extension_roots_test.cpp core/extension_roots.cpp

$(EXTENSION_ROOT_SMOKE): tests/extension_root_discovery_smoke.cpp \
		core/extension_roots.cpp core/extension_roots.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ -Icore \
		-o $@ tests/extension_root_discovery_smoke.cpp \
		core/extension_roots.cpp

$(ODF_MODULE_SMOKE): tests/odf_module_init_smoke.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-Isdk/include -o $@ $<

$(MODULE_API_TEST): tests/module_api_test.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Isdk/include -o $@ $<

$(MODULE_POLICY_TEST): tests/module_policy_test.cpp \
		core/module_policy.cpp core/module_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/module_policy_test.cpp core/module_policy.cpp

$(HYBRID_PRODUCTION_TEST): tests/hybrid_production_test.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOHybridBuild -o $@ \
		tests/hybrid_production_test.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp

$(BUILD_SUBMENU_CONFIG_TEST): tests/build_submenu_config_test.cpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp \
		modules/A2FOHybridBuild/build_submenu_config.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOHybridBuild -o $@ \
		tests/build_submenu_config_test.cpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp

$(TURRET_MATH_TEST): tests/turret_math_test.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_combat.hpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/turret_math.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOTurrets -o $@ \
		tests/turret_math_test.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_math.cpp

$(CRAFT_IDENTITY_TEST): tests/craft_identity_test.cpp \
		modules/A2FOCraftIdentity/selected_panel_anchor.hpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.hpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.hpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.hpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/identity_selection.hpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp \
		modules/A2FOCraftIdentity/system_icon_state.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOCraftIdentity -o $@ \
		tests/craft_identity_test.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp

$(EDIT_MENU_TEST): tests/edit_menu_test.cpp \
		modules/A2FOEditMenu/edit_menu_odf.cpp \
		modules/A2FOEditMenu/edit_menu_odf.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOEditMenu -o $@ \
		tests/edit_menu_test.cpp modules/A2FOEditMenu/edit_menu_odf.cpp

$(ENERGY_SYSTEMS_TEST): tests/energy_systems_test.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/energy_systems.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOEnergySystems -o $@ \
		tests/energy_systems_test.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp

$(DIRECTIONAL_SHIELDS_TEST): tests/directional_shields_test.cpp \
		modules/A2FODirectionalShields/directional_shields.cpp \
		modules/A2FODirectionalShields/directional_shields.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FODirectionalShields -o $@ \
		tests/directional_shields_test.cpp \
		modules/A2FODirectionalShields/directional_shields.cpp

$(INSTANT_ACTION_SETTINGS_TEST): \
		tests/instant_action_settings_test.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.hpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOInstantActionSettings -o $@ \
		tests/instant_action_settings_test.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp

$(BUILD_TIME_TEXT_TEST): tests/build_time_text_test.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp \
		modules/A2FOBuildTooltips/build_time_text.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOBuildTooltips -o $@ \
		tests/build_time_text_test.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp

$(FIRE_ARC_TEST): tests/fire_arc_test.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/fire_arc.hpp \
		modules/A2FOFireArcs/runtime_config.cpp \
		modules/A2FOFireArcs/runtime_config.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOFireArcs -o $@ \
		tests/fire_arc_test.cpp modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/runtime_config.cpp

$(UPGRADE_POD_CONFIG_TEST): tests/upgrade_pod_config_test.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOFeaturePack -o $@ \
		tests/upgrade_pod_config_test.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp

$(WRECKAGE_POLICY_TEST): tests/wreckage_policy_test.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp \
		modules/A2FOWreckage/wreckage_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOWreckage -o $@ \
		tests/wreckage_policy_test.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp

$(REFIT_POLICY_TEST): tests/refit_policy_test.cpp \
		modules/A2FORefitYards/docking_transform.hpp \
		modules/A2FORefitYards/refit_policy.cpp \
		modules/A2FORefitYards/refit_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FORefitYards -o $@ \
		tests/refit_policy_test.cpp \
		modules/A2FORefitYards/refit_policy.cpp

# Policy tests are host-only. The separate x86 harness exercises the native
# adapter against mocked gateways; in-game acceptance remains a manual step.
.PHONY: squadrons-test
squadrons-test: $(SQUADRONS_TEST)
	$(SQUADRONS_TEST)

$(SQUADRONS_TEST): tests/squadrons_test.cpp $(SQUADRONS_SOURCES) \
		$(SQUADRONS_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOSquadrons -o $@ tests/squadrons_test.cpp $(SQUADRONS_SOURCES)

$(SQUADRONS_X86_TEST): tests/squadrons_test.cpp $(SQUADRONS_SOURCES) \
		$(SQUADRONS_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Imodules/A2FOSquadrons \
		-o $@ tests/squadrons_test.cpp $(SQUADRONS_SOURCES)

$(BUILD_DIR)/squadrons_native_test.exe: tests/squadrons_native_test.cpp \
		tests/squadrons_native_economics_test.inl \
		modules/A2FOSquadrons/native_economics.inl \
		modules/A2FOSquadrons/api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		tests/squadrons_native_repair_test.inl \
		modules/A2FOSquadrons/module.cpp modules/A2FOSquadrons/thiscall_bridge.S \
		modules/A2FOSquadrons/native_repair.inl \
		$(SQUADRONS_SOURCES) $(SQUADRONS_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ \
		tests/squadrons_native_test.cpp $(SQUADRONS_SOURCES) \
		modules/A2FOSquadrons/thiscall_bridge.S

$(BUILD_DIR)/squadrons_resources_test.exe: tests/squadrons_resources_test.cpp \
		modules/A2FOResources/module.cpp modules/A2FOResources/api.hpp \
		modules/A2FOResources/additional_resources.cpp modules/A2FOResources/additional_resources.hpp \
		modules/A2FOSquadrons/api.hpp modules/A2FOResources/thiscall_bridge.S | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ tests/squadrons_resources_test.cpp \
		modules/A2FOResources/additional_resources.cpp modules/A2FOResources/thiscall_bridge.S

$(BUILD_DIR)/squadrons_queue_bridge_test.exe: tests/squadrons_queue_bridge_test.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp modules/A2FOFeaturePack/queue_enhancement.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp modules/A2FOFeaturePack/delphi_bridge.S | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ tests/squadrons_queue_bridge_test.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp modules/A2FOFeaturePack/delphi_bridge.S

$(WEAPON_DAMAGE_CONTROLS_TEST): tests/weapon_damage_controls_test.cpp \
		modules/A2FOWeaponDamageControls/damage_controls.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOWeaponDamageControls -o $@ \
		tests/weapon_damage_controls_test.cpp

$(SHIELD_VISIBILITY_TEST): tests/shield_visibility_test.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOAlwaysShowShields -o $@ \
		tests/shield_visibility_test.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp

$(NEBULA_EMISSIVE_TEST): tests/nebula_emissive_test.cpp \
		core/nebula_emissive.cpp core/nebula_emissive.hpp \
		core/renderer_draw_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/nebula_emissive_test.cpp \
		core/nebula_emissive.cpp

$(AMD_DOT3_COMPAT_TEST): tests/amd_dot3_compat_test.cpp \
		core/amd_dot3_compat.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/amd_dot3_compat_test.cpp

$(COM_OWNER_TEST): tests/com_owner_test.cpp core/com_owner.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/com_owner_test.cpp

$(GAME_MONITOR_POLICY_TEST): tests/game_monitor_policy_test.cpp \
		core/game_monitor_policy.cpp core/game_monitor_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/game_monitor_policy_test.cpp \
		core/game_monitor_policy.cpp

$(ART_TEXTURE_SUFFIX_CONFIG_TEST): tests/art_texture_suffix_config_test.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FONebulaRenderer -o $@ \
		tests/art_texture_suffix_config_test.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp

$(DECAL_MATH_TEST): tests/decal_math_test.cpp \
		core/decal_math.cpp core/decal_math.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/decal_math_test.cpp core/decal_math.cpp

$(POINT_DEFENSE_CYCLE_TEST): tests/point_defense_cycle_test.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOPointDefenseCycles -o $@ \
		tests/point_defense_cycle_test.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp

$(SWARM_MOTION_TEST): tests/swarm_motion_test.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp \
		modules/A2FOSwarmSystem/swarm_motion.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOSwarmSystem -o $@ \
		tests/swarm_motion_test.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp

$(TEXTURE_VARIANTS_TEST): tests/texture_variants_test.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp \
		modules/A2FOTextureVariants/texture_variants.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOTextureVariants -o $@ \
		tests/texture_variants_test.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp

verify: release
	@echo "A2FOExtensions exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FO_Initialize|A2FO_NebulaRendererStatus|A2FO_NebulaSet(EmissiveBumpMultiplier|BumpLightBias|EmissiveDiffuseRestore|FastNonBumpEnabled)|A2FO_NebulaRegister(Emissive(Class|Materials)|SpecularMaterials)|A2FO_NebulaBeginCraftRender|A2FO_NebulaEndCraftRender|DLL Name" || true
	@echo
	@echo "Proxy exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/Win2kDisableTaskSwitch.dll | \
		grep -E "LowLevelKeyboardProc|SetHookID|DLL Name" || true
	@echo
	@echo "Renderer helper dependencies:"
	@$(OBJDUMP) -p $(A2FO_RENDERER_HELPER) | \
		grep -E "DLL Name" || true
	@echo
	@echo "A2FOAlwaysShowShields module exports:"
	@$(OBJDUMP) -p $(ALWAYS_SHOW_SHIELDS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOAlwaysShowShields_RegisterClass|A2FOAlwaysShowShields_UpdateCraft|A2FOAlwaysShowShields_CleanupCraft|DLL Name" || true
	@echo
	@echo "A2FOAnimatedHardpoints module exports:"
	@$(OBJDUMP) -p $(ANIMATED_HARDPOINTS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOBuildTooltips module exports:"
	@$(OBJDUMP) -p $(BUILD_TOOLTIPS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOFeaturePack module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOFeaturePack.dll | \
		grep -E "A2FO_ModuleInit|A2FO_(RegisterRefitQueueBridge|ProducerPushRefit)|DLL Name" || true
	@echo
	@echo "A2FOHybridBuild module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOHybridBuild.dll | \
		grep -E "A2FO_ModuleInit|A2FO_RegisterRefitUiBridge|DLL Name" || true
	@echo
	@echo "A2FOInfoIni module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOInfoIni.dll | \
		grep -E "A2FO_ModuleInit|DLL Name" || true
	@echo
	@echo "A2FOCheats module exports:"
	@$(OBJDUMP) -p $(CHEATS_MODULE) | \
		grep -E "A2FO_ModuleInit|DLL Name" || true
	@echo
	@echo "A2FOCraftIdentity module exports:"
	@$(OBJDUMP) -p $(CRAFT_IDENTITY_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOEditMenu module exports:"
	@$(OBJDUMP) -p $(EDIT_MENU_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FODirectionalShields module exports:"
	@$(OBJDUMP) -p $(DIRECTIONAL_SHIELDS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FODirectionalShields_(ConnectDamageBridge|BeginDamage|EndDamage|IsEnabled|GetCurrent|GetMaximum)|DLL Name" || true
	@echo
	@echo "A2FOEnergySystems module exports:"
	@$(OBJDUMP) -p $(ENERGY_SYSTEMS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOEnergySystems_(Get|Set|Add)|DLL Name" || true
	@echo
	@echo "A2FOInstantActionSettings module exports:"
	@$(OBJDUMP) -p $(INSTANT_ACTION_SETTINGS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOResources module exports:"
	@$(OBJDUMP) -p $(RESOURCES_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOResources_(Get|Set|Add|GetCost|GetPresentationText)|DLL Name" || true
	@echo
	@echo "A2FOMissionSelector module exports:"
	@$(OBJDUMP) -p $(MISSION_SELECTOR_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOFireArcs module exports:"
	@$(OBJDUMP) -p $(FIRE_ARCS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOFireArcs_AllowWeaponTrigger|DLL Name" || true
	@echo
	@echo "A2FOWeaponDamageControls module exports:"
	@$(OBJDUMP) -p $(WEAPON_DAMAGE_CONTROLS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOWeaponDamageControls_RefreshDirectionalShieldsBridge|DLL Name" || true
	@echo
	@echo "A2FONormalWeaponTech module exports:"
	@$(OBJDUMP) -p $(NORMAL_WEAPON_TECH_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FONormalWeaponTech_AllowWeaponTrigger|DLL Name" || true
	@echo
	@echo "A2FONebulaRenderer module exports:"
	@$(OBJDUMP) -p $(NEBULA_RENDERER_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FONebulaRenderer_RegisterClass|DLL Name" || true
	@echo
	@echo "A2FOPointDefenseCycles module exports:"
	@$(OBJDUMP) -p $(POINT_DEFENSE_CYCLES_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOSwarmSystem module exports:"
	@$(OBJDUMP) -p $(SWARM_SYSTEM_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOTextureVariants module exports:"
	@$(OBJDUMP) -p $(TEXTURE_VARIANTS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOTextureVariants_RegisterClass|DLL Name" || true
	@echo
	@echo "A2FOODFVariants module exports:"
	@$(OBJDUMP) -p $(ODF_VARIANTS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOTurrets module exports:"
	@$(OBJDUMP) -p $(TURRETS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FORefitYards module exports:"
	@$(OBJDUMP) -p $(REFIT_YARDS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOTeamChangeWeapons module exports:"
	@$(OBJDUMP) -p $(TEAM_CHANGE_WEAPONS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOStationRotation module exports:"
	@$(OBJDUMP) -p $(STATION_ROTATION_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FORGBTextures module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FORGBTextures.dll | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A1Fallbacks module exports:"
	@$(OBJDUMP) -p $(A1_FALLBACKS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOWreckage module exports:"
	@$(OBJDUMP) -p $(WRECKAGE_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "Checking for non-system MinGW runtime dependencies:"
	@for dll in \
		$(BUILD_DIR)/A2FOExtensions.dll \
		$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
		$(A2FO_RENDERER_HELPER) \
		$(ALWAYS_SHOW_SHIELDS_MODULE) \
		$(ANIMATED_HARDPOINTS_MODULE) \
		$(ANIMATIONS_MODULE) \
		$(BUILD_TOOLTIPS_MODULE) \
		$(MODULE_DIR)/A2FOFeaturePack.dll \
		$(MODULE_DIR)/A2FOHybridBuild.dll \
		$(MODULE_DIR)/A2FOInfoIni.dll \
		$(CHEATS_MODULE) \
		$(CRAFT_IDENTITY_MODULE) \
		$(EDIT_MENU_MODULE) \
		$(DIRECTIONAL_SHIELDS_MODULE) \
		$(ENERGY_SYSTEMS_MODULE) \
		$(INSTANT_ACTION_SETTINGS_MODULE) \
		$(RESOURCES_MODULE) \
		$(MISSION_SELECTOR_MODULE) \
		$(FIRE_ARCS_MODULE) \
		$(WEAPON_DAMAGE_CONTROLS_MODULE) \
		$(WRECKAGE_MODULE) \
		$(NORMAL_WEAPON_TECH_MODULE) \
		$(NEBULA_RENDERER_MODULE) \
		$(POINT_DEFENSE_CYCLES_MODULE) \
		$(SWARM_SYSTEM_MODULE) \
		$(TEXTURE_VARIANTS_MODULE) \
		$(ODF_VARIANTS_MODULE) \
		$(STA1_COMPAT_MODULE) \
		$(A1_FALLBACKS_MODULE) \
		$(TURRETS_MODULE) \
		$(REFIT_YARDS_MODULE) \
		$(STATION_ROTATION_MODULE) \
		$(TEAM_CHANGE_WEAPONS_MODULE) \
		$(MODULE_DIR)/A2FORGBTextures.dll; do \
		if $(OBJDUMP) -p "$$dll" | \
			grep -Eiq 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)'; then \
			echo "Unexpected MinGW runtime dependency in $$dll" >&2; \
			$(OBJDUMP) -p "$$dll" | grep -Ei 'DLL Name:' >&2; \
			exit 1; \
		fi; \
	done
	@echo "No external MinGW runtime DLLs required."

verify-sta1-classic: sta1-classic
	@test ! -d "$(STA1_CLASSIC_DIR)/modules" || \
		(echo "Mod package must not contain a native modules directory" >&2; exit 1)
	@for cfg in gui_fed.cfg gui_bor.cfg gui_kli.cfg gui_rom.cfg; do \
		test -f "$(STA1_CLASSIC_DIR)/misc/$$cfg" || exit 1; \
	done
	@echo "A1Compat module exports:"
	@$(OBJDUMP) -p $(STA1_COMPAT_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@if $(OBJDUMP) -p $(STA1_COMPAT_MODULE) | \
		grep -Eiq 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)'; then \
		echo "Unexpected MinGW runtime dependency in $(STA1_COMPAT_MODULE)" >&2; \
		$(OBJDUMP) -p $(STA1_COMPAT_MODULE) | grep -Ei 'DLL Name:' >&2; \
		exit 1; \
	fi
	@echo "A1Fallbacks module exports:"
	@$(OBJDUMP) -p $(A1_FALLBACKS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@if $(OBJDUMP) -p $(A1_FALLBACKS_MODULE) | \
		grep -Eiq 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)'; then \
		echo "Unexpected MinGW runtime dependency in $(A1_FALLBACKS_MODULE)" >&2; \
		$(OBJDUMP) -p $(A1_FALLBACKS_MODULE) | grep -Ei 'DLL Name:' >&2; \
		exit 1; \
	fi
	@echo "STA1 Classic selects its centrally installed modules through info.ini."

verify-sdk: sdk-examples
	@$(OBJDUMP) -p $(MODULE_DIR)/ExampleModule.dll | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true

test: $(FPQ_PATHS_TEST) $(ODF_PATHS_TEST) $(EXTENSION_ROOTS_TEST) \
	$(MODULE_API_TEST) $(MODULE_POLICY_TEST) $(HYBRID_PRODUCTION_TEST) \
	$(BUILD_SUBMENU_CONFIG_TEST) $(TURRET_MATH_TEST) \
	$(CRAFT_IDENTITY_TEST) $(EDIT_MENU_TEST) $(INSTANT_ACTION_SETTINGS_TEST) \
	$(ENERGY_SYSTEMS_TEST) $(DIRECTIONAL_SHIELDS_TEST) \
	$(BUILD_TIME_TEXT_TEST) $(ADDITIONAL_RESOURCES_TEST) $(FIRE_ARC_TEST) \
	$(A1_RACE_MENU_TEST) $(A1_TEAM_COLOR_TEST) $(A1_BZN_POLICY_TEST) \
	$(A1_UI_POLICY_TEST) $(A1_FALLBACK_POLICY_TEST) \
	$(UPGRADE_POD_CONFIG_TEST) \
	$(WRECKAGE_POLICY_TEST) \
	$(REFIT_POLICY_TEST) \
	$(SQUADRONS_TEST) \
	$(STATION_ROTATION_TEST) \
	$(WEAPON_DAMAGE_CONTROLS_TEST) \
	$(SHIELD_VISIBILITY_TEST) $(NEBULA_EMISSIVE_TEST) \
	$(AMD_DOT3_COMPAT_TEST) $(COM_OWNER_TEST) \
	$(GAME_MONITOR_POLICY_TEST) \
	$(ART_TEXTURE_SUFFIX_CONFIG_TEST) $(DECAL_MATH_TEST) \
	$(POINT_DEFENSE_CYCLE_TEST) $(SWARM_MOTION_TEST) \
	$(TEXTURE_VARIANTS_TEST)
	$(FPQ_PATHS_TEST)
	$(ODF_PATHS_TEST)
	$(EXTENSION_ROOTS_TEST)
	$(MODULE_API_TEST)
	$(MODULE_POLICY_TEST)
	$(HYBRID_PRODUCTION_TEST)
	$(BUILD_SUBMENU_CONFIG_TEST)
	$(TURRET_MATH_TEST)
	$(CRAFT_IDENTITY_TEST)
	$(EDIT_MENU_TEST)
	$(ENERGY_SYSTEMS_TEST)
	$(DIRECTIONAL_SHIELDS_TEST)
	$(INSTANT_ACTION_SETTINGS_TEST)
	$(BUILD_TIME_TEXT_TEST)
	$(ADDITIONAL_RESOURCES_TEST)
	$(A1_RACE_MENU_TEST)
	$(A1_TEAM_COLOR_TEST)
	$(A1_BZN_POLICY_TEST)
	$(A1_UI_POLICY_TEST)
	$(A1_FALLBACK_POLICY_TEST)
	$(FIRE_ARC_TEST)
	$(UPGRADE_POD_CONFIG_TEST)
	$(WRECKAGE_POLICY_TEST)
	$(REFIT_POLICY_TEST)
	$(SQUADRONS_TEST)
	$(STATION_ROTATION_TEST)
	$(WEAPON_DAMAGE_CONTROLS_TEST)
	$(SHIELD_VISIBILITY_TEST)
	$(NEBULA_EMISSIVE_TEST)
	$(AMD_DOT3_COMPAT_TEST)
	$(COM_OWNER_TEST)
	$(GAME_MONITOR_POLICY_TEST)
	$(ART_TEXTURE_SUFFIX_CONFIG_TEST)
	$(DECAL_MATH_TEST)
	$(POINT_DEFENSE_CYCLE_TEST)
	$(SWARM_MOTION_TEST)
	$(TEXTURE_VARIANTS_TEST)
	python3 -m unittest tests/test_odf_formatter.py \
		tests/test_modder_documentation.py

smoke: release $(SMOKE_TEST) $(GAME_MONITOR_SMOKE)
	cd $(BUILD_DIR) && wine dll_load_smoke.exe
	cd $(BUILD_DIR) && wine game_monitor_win32_smoke.exe

odf-module-smoke: release $(STA1_COMPAT_MODULE) $(ODF_MODULE_SMOKE)
	@test -f $(BUILD_DIR)/FleetOpsHook.fixture.dll || \
		(echo "Copy the supported FleetOpsHook.dll to build/FleetOpsHook.fixture.dll" >&2; exit 1)
	cd $(BUILD_DIR) && wine odf_module_init_smoke.exe

extension-root-smoke: $(EXTENSION_ROOT_SMOKE)
	@test -n "$(A2FO_TEST_DATA_ROOT)" || \
		(echo "Set A2FO_TEST_DATA_ROOT to the Fleet Ops Data path" >&2; exit 1)
	cd $(BUILD_DIR) && wine extension_root_discovery_smoke.exe \
		"$(A2FO_TEST_DATA_ROOT)" "$(A2FO_TEST_ACTIVE_MOD)" \
		"$(A2FO_TEST_PARENT_MOD)"

clean:
	rm -rf $(BUILD_DIR)

# Included implementation for automatic non-bump MeshVB preparation.
build/A2FOExtensions.dll: core/renderer_unmapped_meshvb.inl

# Hardware-lit Phong support shares native MeshVB buffer ownership.
build/A2FOExtensions.dll: core/renderer_phong_meshvb.inl

# Cloak lighting is composited once, without opaque framebuffer accumulation.
build/A2FOExtensions.dll: core/renderer_cloak_composite.inl

# Standard shell button artwork and interaction in the mission selector.
$(MISSION_SELECTOR_MODULE): modules/A2FOMissionSelector/standard_buttons.inl

# Shared visible/cloaked mesh compositor integration.
build/A2FOExtensions.dll: core/renderer_general_meshvb.inl

# Alpha MeshVB dynamic index-stream sorting.
build/A2FOExtensions.dll: core/renderer_transparent_indices.inl
