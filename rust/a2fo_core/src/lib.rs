//! Migrated A2FO policies, runtime owners and supported native backends.
//! The C ABI carries bytes, POD and opaque handles, never Rust/STL objects.
//! Release builds abort on panic: unwinding across the ABI is forbidden.

#![cfg_attr(not(test), no_std)]
#[cfg(feature = "std-host")]
extern crate std;
extern crate alloc;

mod allocator;
mod native11_model;
mod native11_group_material;
mod builtin_state;
mod config;
mod energy;
mod gameplay;
mod hybrid;
mod lead;
mod legacy_float;
mod module_policy;
mod resupply;
mod textures;
mod ui;
mod wreckage;

mod a1_maps;
mod combat_config;
mod sol;
mod text_geometry;

mod m3d;

mod motion;

mod refit_present;

mod native_hooks;

mod roots;

mod domain_state;

mod d3d_resources;

#[cfg(all(target_os = "windows", target_arch = "x86", target_env = "gnu"))]
mod d3d_native;

mod builtin_runtime;

mod wide_math;

mod loader;

mod emissive;

mod refit_native;

mod render_cache_state;

mod render_draw;

mod refraction;

mod monitor;

mod backdrop;

mod d3dx;

mod shields;

mod module_runtime_a;

mod module_runtime_b;

mod queue_bridges;

mod game_resources;

mod intro_runtime;

mod module_runtime_b2;

mod module_runtime_a2;

mod craft_inspector;

mod module_runtime_a3;

mod game_device;

mod module_runtime_b3;

#[cfg(all(target_arch = "x86", target_os = "windows"))]
mod shield_wide;

mod extended_icons;

mod module_runtime_a4;

mod refit_ui_runtime;

mod module_runtime_a5;

mod module_runtime_b4;

mod game_backend;

mod module_runtime_b5;

mod module_runtime_b6;

mod module_runtime_a6;

mod ui_renderer;

mod game_draw;

mod stage8_a;

mod stage8_b_loading;

mod stage8_b_refit;

mod stage8_b_scene;

mod stage8_b_setup;

mod stage8_b_unsupported;

mod stage8_c_resources;

mod stage8_c_solmaps;

mod stage8_refit;

mod stage8_a_breeders;

mod stage8_core;

mod stage8_a_infestation;

mod stage8_b_renderer_options;

mod stage8_a_directional;

mod stage8_c_mission;

mod stage8_a_fire_arcs;

mod stage8_a1;

mod stage8_b_core;

mod stage8_core_events;

mod stage8_a_energy;

mod stage8_c_odf;

mod stage8_b_object_editor;

mod stage8_b_bink_video;

mod stage8_a_m3d;

mod stage8_b_canvas;

mod stage8_c_buildyard;

mod stage8_core_odf;

mod stage8_a1_lifecycle;

mod stage8_a_squadrons;

mod stage8_core_lifecycle;

mod stage8_a1_shutdown;

mod stage8_c_queue;

mod stage8_core_settings;

mod stage8_a1_relationships;

mod stage8_c_upgrade;

mod stage8_core_cocoon;

mod stage8_a1_maps_runtime;

mod stage8_a_swarm;

mod stage8_core_dispatch;

mod stage8_hybrid_runtime;

mod stage8_core_classdefaults;

mod stage8_core_lookup;

mod stage8_a1_physics;

mod stage8_hybrid_lists;

mod stage8_core_classload;

mod stage8_hybrid_state;

mod stage8_a1_officer;

mod stage8_a_turrets;

mod stage8_c_nebula;

mod stage8_texture_runtime;

mod stage9_core_parameters;

mod stage9_texture_visuals;

mod stage9_rgb_runtime;

mod stage9_video_runtime;

mod stage9_core_shell;

mod stage9_core_config;

mod stage9_a1_config;
mod stage9_core_nebula;
mod stage9_craft_runtime;

mod stage9_craft_sprite;
mod stage9_hybrid_runtime;

mod stage9_module_menu;
mod stage9_renderer_options;

mod stage9_directional_parse;
mod stage9_energy_numeric;

mod stage9_a1_mission_scan;
mod stage9_sol_numeric;
mod stage9_sol_runtime;

mod stage9_a1_ui;
mod stage9_sol_completion;
mod stage9_sol_discovery;

mod stage9_a1_assets;
mod stage9_a1_bzn_runtime;
mod stage9_a1_class_defaults;
mod stage9_a1_geometry;
mod stage9_a1_gui_config;
mod stage9_a1_layout;
mod stage9_a1_numeric;
mod stage9_a1_officer_policy;
mod stage9_a1_physics_defaults;
mod stage9_a1_quarters;
mod stage9_a1_race;
mod stage9_a1_reporting;
mod stage9_a1_sod;
mod stage9_a1_speed;

mod stage9_a1_aip_name;
mod stage9_a1_crew;
mod stage9_a1_display_numeric;
mod stage9_a1_display_text;
mod stage9_a1_fofs;
mod stage9_a1_gui_scale;
mod stage9_a1_native_ui;
mod stage9_a1_node_policy;
mod stage9_a1_officer_ui;
mod stage9_a1_officer_workflow;
mod stage9_a1_race_workflow;
mod stage9_a1_render_policy;
mod stage9_a1_resource_limits;
mod stage9_a1_state_policy;
mod stage9_a1_ui_numeric;

mod stage10_resource_presentation;

mod stage10_fire_arcs_runtime;
mod stage10_fire_arcs_text;

mod stage10_nebula_runtime;
mod stage10_nebula_text;

mod stage10_energy_runtime;

mod stage10_resources_runtime;

mod stage10_squadrons_numeric;
mod stage10_squadrons_runtime;

mod stage10_feature_completion;
mod stage10_rgb_completion;
mod stage10_ui_completion;

mod stage10_identity_completion;
mod stage10_texture_completion;

mod stage10_core_menu;
mod stage10_core_pixels;
mod stage10_core_render;

mod stage10_mission_completion;
mod stage10_video_completion;

mod stage10_presentation_completion;

mod stage10_visibility_swarm;

mod stage10_hybrid_completion;

mod stage10_m3d_completion;

mod runtime_modules;

/// Retain the implementation crate in the Rust-owned DLL link.
#[inline(never)]
pub fn runtime_link_anchor() -> u32 {
    11
}

mod runtime_globals;

mod runtime_handlers;

mod runtime_normal_state;
mod runtime_station_state;

mod native_fp;

mod mmm_runtime;
