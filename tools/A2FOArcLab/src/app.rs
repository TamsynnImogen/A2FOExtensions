use crate::fire_arc::{ArcConfig, ArcMode};
use crate::odf::{load_project, ArcProject, WeaponSlot};
use crate::sod::{spawn_sod, LoadedSod, SodNodeMarker};
use bevy::core_pipeline::tonemapping::Tonemapping;
use bevy::input::mouse::{MouseMotion, MouseWheel};
use bevy::prelude::*;
use bevy_egui::{egui, EguiContexts, EguiPlugin};
use rfd::FileDialog;
use std::collections::HashSet;
use std::f32::consts::{FRAC_PI_2, TAU};
use std::path::PathBuf;

pub struct ArcLabPlugin {
    initial_ship: Option<PathBuf>,
}

impl ArcLabPlugin {
    pub fn new(initial_ship: Option<PathBuf>) -> Self {
        Self { initial_ship }
    }
}

impl Plugin for ArcLabPlugin {
    fn build(&self, app: &mut App) {
        app.add_plugins(EguiPlugin)
            .insert_resource(ClearColor(Color::rgb(0.018, 0.024, 0.042)))
            .insert_resource(InitialShip(self.initial_ship.clone()))
            .init_resource::<ArcLabState>()
            .init_resource::<CameraRequest>()
            .add_event::<LoadShipRequest>()
            .add_event::<LoadModelRequest>()
            .add_systems(Startup, (setup_scene, open_initial_ship).chain())
            .add_systems(
                Update,
                (draw_ui, handle_ship_load, handle_model_load).chain(),
            )
            .add_systems(
                Update,
                (
                    orbit_camera,
                    keyboard_hardpoint_cycle,
                    draw_reference_gizmos,
                    draw_fire_arc_gizmos,
                ),
            );
    }
}

#[derive(Resource)]
struct InitialShip(Option<PathBuf>);

#[derive(Event)]
struct LoadShipRequest(PathBuf);

#[derive(Event)]
struct LoadModelRequest(PathBuf);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum CameraPreset {
    Perspective,
    Front,
    Side,
    Top,
}

#[derive(Resource, Default)]
struct CameraRequest {
    fit: Option<(Vec3, f32)>,
    preset: Option<CameraPreset>,
}

#[derive(Resource)]
struct ArcLabState {
    project: Option<ArcProject>,
    selected_weapon: usize,
    selected_hardpoint: usize,
    show_all_hardpoints: bool,
    config: ArcConfig,
    dirty: bool,
    status: String,
    error: Option<String>,
    loaded_model: Option<LoadedSod>,
    show_reference_grid: bool,
    show_probe: bool,
    probe_yaw: f32,
    probe_pitch: f32,
}

impl Default for ArcLabState {
    fn default() -> Self {
        Self {
            project: None,
            selected_weapon: 0,
            selected_hardpoint: 0,
            show_all_hardpoints: true,
            config: ArcConfig::default(),
            dirty: false,
            status: "Open a ship or station ODF to begin.".to_string(),
            error: None,
            loaded_model: None,
            show_reference_grid: true,
            show_probe: true,
            probe_yaw: 0.0,
            probe_pitch: 0.0,
        }
    }
}

impl ArcLabState {
    fn selected_weapon(&self) -> Option<&WeaponSlot> {
        self.project
            .as_ref()
            .and_then(|project| project.weapons.get(self.selected_weapon))
    }

    fn select_weapon(&mut self, index: usize) {
        let Some(project) = self.project.as_ref() else {
            return;
        };
        if index >= project.weapons.len() {
            return;
        }
        self.selected_weapon = index;
        self.selected_hardpoint = 0;
        self.config = project.weapons[index].arc.config.unwrap_or_default();
        self.dirty = false;
    }

    fn cycle_hardpoint(&mut self, direction: i32) {
        let count = self
            .selected_weapon()
            .map(|weapon| weapon.hardpoints.len())
            .unwrap_or(0);
        if count == 0 {
            self.selected_hardpoint = 0;
            return;
        }
        self.show_all_hardpoints = false;
        self.selected_hardpoint =
            (self.selected_hardpoint as i32 + direction).rem_euclid(count as i32) as usize;
    }
}

#[derive(Component)]
struct OrbitCamera {
    yaw: f32,
    pitch: f32,
    radius: f32,
    target: Vec3,
}

fn setup_scene(mut commands: Commands) {
    commands.insert_resource(AmbientLight {
        color: Color::rgb(0.72, 0.78, 1.0),
        brightness: 0.48,
    });
    commands.spawn(DirectionalLightBundle {
        directional_light: DirectionalLight {
            illuminance: 14_000.0,
            shadows_enabled: false,
            ..default()
        },
        transform: Transform::from_rotation(Quat::from_euler(EulerRot::XYZ, -0.8, 0.55, 0.0)),
        ..default()
    });
    commands.spawn(DirectionalLightBundle {
        directional_light: DirectionalLight {
            color: Color::rgb(0.3, 0.42, 0.7),
            illuminance: 3_500.0,
            shadows_enabled: false,
            ..default()
        },
        transform: Transform::from_rotation(Quat::from_euler(EulerRot::XYZ, 0.7, -2.2, 0.0)),
        ..default()
    });

    let orbit = OrbitCamera {
        yaw: 0.65,
        pitch: 0.42,
        radius: 20.0,
        target: Vec3::ZERO,
    };
    let mut transform = Transform::IDENTITY;
    apply_orbit_transform(&mut transform, &orbit);
    commands.spawn((
        Camera3dBundle {
            camera: Camera {
                hdr: false,
                ..default()
            },
            tonemapping: Tonemapping::None,
            transform,
            ..default()
        },
        orbit,
        Name::new("Arc Lab Camera"),
    ));
}

fn open_initial_ship(initial: Res<InitialShip>, mut requests: EventWriter<LoadShipRequest>) {
    if let Some(path) = initial.0.as_ref() {
        requests.send(LoadShipRequest(path.clone()));
    }
}

fn draw_ui(
    mut contexts: EguiContexts,
    mut state: ResMut<ArcLabState>,
    mut load_ship: EventWriter<LoadShipRequest>,
    mut load_model: EventWriter<LoadModelRequest>,
    mut camera_request: ResMut<CameraRequest>,
    nodes: Query<(&SodNodeMarker, &GlobalTransform)>,
    camera: Query<(&Camera, &GlobalTransform), With<OrbitCamera>>,
) {
    let context = contexts.ctx_mut();
    let mut requested_weapon = None;

    egui::TopBottomPanel::top("top_toolbar").show(context, |ui| {
        ui.horizontal_wrapped(|ui| {
            ui.heading("A2FO Arc Lab");
            ui.separator();
            if ui.button("Open Ship ODF…").clicked() {
                if let Some(path) = FileDialog::new()
                    .add_filter("Armada ODF", &["odf", "ODF"])
                    .pick_file()
                {
                    load_ship.send(LoadShipRequest(path));
                }
            }
            let can_reload = state.project.is_some();
            if ui
                .add_enabled(can_reload, egui::Button::new("Reload ODF"))
                .clicked()
            {
                if let Some(path) = state
                    .project
                    .as_ref()
                    .map(|project| project.ship_path.clone())
                {
                    load_ship.send(LoadShipRequest(path));
                }
            }
            if ui
                .add_enabled(state.project.is_some(), egui::Button::new("Choose SOD…"))
                .clicked()
            {
                if let Some(path) = FileDialog::new()
                    .add_filter("Armada SOD", &["sod", "SOD"])
                    .pick_file()
                {
                    load_model.send(LoadModelRequest(path));
                }
            }
            ui.separator();
            ui.checkbox(&mut state.show_reference_grid, "Reference grid");
            ui.checkbox(&mut state.show_probe, "Target probe");
        });
        if let Some(project) = state.project.as_ref() {
            ui.horizontal(|ui| {
                ui.strong(&project.ship_name);
                ui.weak(project.ship_path.display().to_string());
            });
            ui.collapsing("Resolution details", |ui| {
                ui.label(format!("Ship ODF: {}", project.ship.path.display()));
                ui.label(format!("Model name: {}.sod", project.model_name));
                if let Some(path) = project.model_path.as_ref() {
                    ui.label(format!("Resolved model: {}", path.display()));
                }
                ui.label(format!(
                    "ODF include chain: {} file{}",
                    project.ship.files.len(),
                    if project.ship.files.len() == 1 {
                        ""
                    } else {
                        "s"
                    }
                ));
                for root in &project.resources.roots {
                    ui.small(format!("{} — {}", root.label, root.path.display()));
                }
                if let Some(model) = state.loaded_model.as_ref() {
                    ui.separator();
                    ui.label(format!(
                        "Textures loaded: {}/{}",
                        model.loaded_textures.len(),
                        model.referenced_texture_count
                    ));
                    for path in &model.loaded_textures {
                        ui.small(path.display().to_string());
                    }
                    for issue in &model.texture_issues {
                        ui.colored_label(egui::Color32::YELLOW, issue);
                    }
                }
            });
        }
    });

    egui::TopBottomPanel::bottom("status_bar").show(context, |ui| {
        if let Some(error) = state.error.as_ref() {
            ui.colored_label(egui::Color32::LIGHT_RED, error);
        } else {
            ui.label(&state.status);
        }
    });

    egui::SidePanel::left("weapons_panel")
        .resizable(true)
        .default_width(285.0)
        .show(context, |ui| {
            ui.heading("Weapons");
            let Some(project) = state.project.as_ref() else {
                ui.separator();
                ui.label("Open a ship ODF to discover its weapons and hardpoints.");
                return;
            };
            ui.label(format!(
                "{} weapon slot{}",
                project.weapons.len(),
                if project.weapons.len() == 1 { "" } else { "s" }
            ));
            ui.separator();
            egui::ScrollArea::vertical().show(ui, |ui| {
                for (index, weapon) in project.weapons.iter().enumerate() {
                    let selected = index == state.selected_weapon;
                    let label = format!(
                        "weapon{} · {}\n{}.odf · {} HP{}",
                        weapon.slot,
                        weapon.display_name,
                        weapon.odf_name,
                        weapon.hardpoints.len(),
                        if weapon.hardpoints.len() == 1 {
                            ""
                        } else {
                            "s"
                        }
                    );
                    if ui.selectable_label(selected, label).clicked() {
                        requested_weapon = Some(index);
                    }
                    if !weapon.arc.errors.is_empty() {
                        ui.colored_label(
                            egui::Color32::LIGHT_RED,
                            format!("{} invalid arc value(s)", weapon.arc.errors.len()),
                        );
                    } else if weapon.arc.config.is_none() {
                        ui.weak("Native fire-arc behaviour");
                    }
                    ui.add_space(4.0);
                }
                if !project.warnings.is_empty() {
                    ui.separator();
                    ui.strong("Project warnings");
                    for warning in &project.warnings {
                        ui.colored_label(egui::Color32::YELLOW, warning);
                    }
                }
            });
        });

    egui::SidePanel::right("arc_editor_panel")
        .resizable(true)
        .default_width(355.0)
        .show(context, |ui| {
            ui.heading("Arc Editor");
            let weapon = state.selected_weapon().cloned();
            let Some(weapon) = weapon else {
                ui.separator();
                ui.label("Select a discovered weapon.");
                return;
            };

            ui.strong(format!("weapon{} · {}", weapon.slot, weapon.display_name));
            if let Some(path) = weapon.odf_path.as_ref() {
                ui.small(path.display().to_string());
            } else {
                ui.colored_label(egui::Color32::LIGHT_RED, "Weapon ODF was not resolved");
            }

            ui.separator();
            draw_hardpoint_controls(ui, &mut state, &weapon, &nodes);
            ui.separator();
            draw_arc_controls(ui, &mut state);
            ui.separator();
            draw_derived_coverage(ui, state.config);

            if !weapon.arc.errors.is_empty() {
                ui.separator();
                ui.colored_label(egui::Color32::LIGHT_RED, "Current ODF arc is invalid:");
                for error in &weapon.arc.errors {
                    ui.colored_label(egui::Color32::LIGHT_RED, error);
                }
            } else if weapon.arc.config.is_none() {
                ui.separator();
                ui.colored_label(
                    egui::Color32::YELLOW,
                    "No custom fire arc is currently configured. Copying the block below opts this weapon in.",
                );
            }

            if !weapon.warnings.is_empty() {
                ui.separator();
                for warning in &weapon.warnings {
                    ui.colored_label(egui::Color32::YELLOW, warning);
                }
            }

            if !weapon.arc.sources.is_empty() {
                ui.collapsing("Resolved ODF sources", |ui| {
                    for (command, path, line) in &weapon.arc.sources {
                        ui.label(format!("{command}: {}:{line}", path.display()));
                    }
                });
            }

            ui.separator();
            draw_probe_controls(ui, &mut state);
            ui.separator();
            draw_snippet_controls(ui, state.config);
        });

    egui::CentralPanel::default()
        .frame(egui::Frame::none())
        .show(context, |ui| {
            let rect = ui.max_rect();
            let painter = ui.painter();
            painter.text(
                rect.left_top() + egui::vec2(12.0, 10.0),
                egui::Align2::LEFT_TOP,
                "Right-drag: orbit   Middle-drag: pan   Wheel: zoom   [ / ]: hardpoints",
                egui::FontId::proportional(13.0),
                egui::Color32::from_gray(175),
            );
            painter.text(
                rect.left_bottom() + egui::vec2(12.0, -12.0),
                egui::Align2::LEFT_BOTTOM,
                "+X starboard/right   +Y dorsal/up   +Z forward",
                egui::FontId::monospace(13.0),
                egui::Color32::from_gray(175),
            );
            ui.horizontal(|ui| {
                for (label, preset) in [
                    ("Perspective", CameraPreset::Perspective),
                    ("Front", CameraPreset::Front),
                    ("Side", CameraPreset::Side),
                    ("Top", CameraPreset::Top),
                ] {
                    if ui.button(label).clicked() {
                        camera_request.preset = Some(preset);
                    }
                }
                if ui.button("Fit").clicked() {
                    if let Some(model) = state.loaded_model.as_ref() {
                        camera_request.fit = Some((model.center, model.radius));
                    }
                }
            });
        });

    if let Some(index) = requested_weapon {
        state.select_weapon(index);
    }
    draw_hardpoint_labels(context, &state, &nodes, &camera);
}

fn draw_hardpoint_controls(
    ui: &mut egui::Ui,
    state: &mut ArcLabState,
    weapon: &WeaponSlot,
    nodes: &Query<(&SodNodeMarker, &GlobalTransform)>,
) {
    ui.label("Hardpoint display");
    ui.horizontal(|ui| {
        if ui.button("◀").clicked() {
            state.cycle_hardpoint(-1);
        }
        let current = weapon
            .hardpoints
            .get(state.selected_hardpoint)
            .map(String::as_str)
            .unwrap_or("(none)");
        egui::ComboBox::from_id_source("hardpoint_selector")
            .selected_text(if state.show_all_hardpoints {
                "All hardpoints"
            } else {
                current
            })
            .show_ui(ui, |ui| {
                ui.selectable_value(&mut state.show_all_hardpoints, true, "All hardpoints");
                for (index, hardpoint) in weapon.hardpoints.iter().enumerate() {
                    if ui
                        .selectable_label(
                            !state.show_all_hardpoints && state.selected_hardpoint == index,
                            hardpoint,
                        )
                        .clicked()
                    {
                        state.show_all_hardpoints = false;
                        state.selected_hardpoint = index;
                    }
                }
            });
        if ui.button("▶").clicked() {
            state.cycle_hardpoint(1);
        }
    });

    let available = nodes
        .iter()
        .map(|(node, _)| node.name.to_ascii_lowercase())
        .collect::<HashSet<_>>();
    let missing = weapon
        .hardpoints
        .iter()
        .filter(|name| !available.contains(&name.to_ascii_lowercase()))
        .collect::<Vec<_>>();
    if !missing.is_empty() {
        ui.colored_label(
            egui::Color32::LIGHT_RED,
            format!(
                "Missing from loaded SOD: {}",
                missing
                    .iter()
                    .map(|value| value.as_str())
                    .collect::<Vec<_>>()
                    .join(", ")
            ),
        );
    }
}

fn draw_arc_controls(ui: &mut egui::Ui, state: &mut ArcLabState) {
    let original = state.config;
    let mut mode = state.config.mode();
    ui.horizontal(|ui| {
        ui.label("Mode");
        egui::ComboBox::from_id_source("arc_mode")
            .selected_text(match mode {
                ArcMode::Box => "box",
                ArcMode::Cone => "cone",
            })
            .show_ui(ui, |ui| {
                ui.selectable_value(&mut mode, ArcMode::Box, "box");
                ui.selectable_value(&mut mode, ArcMode::Cone, "cone");
            });
    });
    state.config.set_mode(mode);
    angle_row(
        ui,
        "Yaw centre",
        &mut state.config.yaw_degrees,
        -180.0,
        180.0,
    );
    angle_row(
        ui,
        "Pitch centre",
        &mut state.config.pitch_degrees,
        -90.0,
        90.0,
    );
    match mode {
        ArcMode::Box => {
            angle_row(
                ui,
                "Yaw total width",
                &mut state.config.yaw_angle_degrees,
                0.0,
                360.0,
            );
            angle_row(
                ui,
                "Pitch total width",
                &mut state.config.pitch_angle_degrees,
                0.0,
                180.0,
            );
        }
        ArcMode::Cone => {
            angle_row(
                ui,
                "Cone total angle",
                &mut state.config.cone_angle_degrees,
                0.0,
                360.0,
            );
        }
    }

    ui.label("Presets");
    ui.horizontal_wrapped(|ui| {
        if ui.button("Dorsal").clicked() {
            state.config = box_preset(0.0, 90.0, 360.0, 180.0);
        }
        if ui.button("Ventral").clicked() {
            state.config = box_preset(0.0, -90.0, 360.0, 180.0);
        }
        if ui.button("Forward dorsal").clicked() {
            state.config = box_preset(0.0, 90.0, 270.0, 180.0);
        }
        if ui.button("Forward ventral").clicked() {
            state.config = box_preset(0.0, -90.0, 270.0, 180.0);
        }
        if ui.button("Forward").clicked() {
            state.config = box_preset(0.0, 0.0, 90.0, 180.0);
        }
        if ui.button("Rear").clicked() {
            state.config = box_preset(180.0, 0.0, 90.0, 180.0);
        }
        if ui.button("Port").clicked() {
            state.config = box_preset(-90.0, 0.0, 90.0, 180.0);
        }
        if ui.button("Starboard").clicked() {
            state.config = box_preset(90.0, 0.0, 90.0, 180.0);
        }
        if ui.button("Unrestricted").clicked() {
            state.config = box_preset(0.0, 0.0, 360.0, 180.0);
        }
    });

    if !same_config(original, state.config) {
        state.dirty = true;
    }
}

fn angle_row(ui: &mut egui::Ui, label: &str, value: &mut f32, minimum: f32, maximum: f32) {
    ui.horizontal(|ui| {
        ui.label(label);
        ui.add(
            egui::DragValue::new(value)
                .speed(1.0)
                .clamp_range(minimum..=maximum)
                .suffix("°"),
        );
    });
}

fn draw_derived_coverage(ui: &mut egui::Ui, config: ArcConfig) {
    let config = config.normalized();
    ui.strong("Derived coverage");
    match config.mode() {
        ArcMode::Box => {
            let (yaw_min, yaw_max) = config.yaw_limits();
            let (pitch_min, pitch_max) = config.pitch_limits();
            ui.monospace(format!("Yaw:   {yaw_min:.1}° through {yaw_max:.1}°"));
            ui.monospace(format!("Pitch: {pitch_min:.1}° through {pitch_max:.1}°"));
            if config.yaw_angle_degrees < 360.0 {
                ui.label(format!(
                    "Horizontal blind coverage: {:.1}°",
                    360.0 - config.yaw_angle_degrees
                ));
            }
            if config.pitch_angle_degrees <= 0.0001 {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    "A zero pitch width only accepts the exact centre line.",
                );
            }
            if config.pitch_degrees > 0.0 && pitch_min < 0.0 {
                ui.colored_label(
                    egui::Color32::YELLOW,
                    format!(
                        "Dorsal-centred arc includes {:.1}° below the ship.",
                        -pitch_min
                    ),
                );
            }
            if config.pitch_degrees < 0.0 && pitch_max > 0.0 {
                ui.colored_label(
                    egui::Color32::YELLOW,
                    format!(
                        "Ventral-centred arc includes {:.1}° above the ship.",
                        pitch_max
                    ),
                );
            }
        }
        ArcMode::Cone => {
            ui.monospace(format!(
                "Circular cap: {:.1}° each side of centre",
                config.cone_angle_degrees * 0.5
            ));
        }
    }
    ui.weak("All width/angle commands are total coverage, not per-side values.");
}

fn draw_probe_controls(ui: &mut egui::Ui, state: &mut ArcLabState) {
    ui.strong("Target probe");
    angle_row(ui, "Probe yaw", &mut state.probe_yaw, -180.0, 180.0);
    angle_row(ui, "Probe pitch", &mut state.probe_pitch, -90.0, 90.0);
    let target = direction_for(state.probe_yaw, state.probe_pitch).to_array();
    let allowed = state.config.allows_identity(target);
    ui.colored_label(
        if allowed {
            egui::Color32::LIGHT_GREEN
        } else {
            egui::Color32::LIGHT_RED
        },
        if allowed {
            "ALLOWED by the DLL geometry"
        } else {
            "BLOCKED by the DLL geometry"
        },
    );
}

fn draw_snippet_controls(ui: &mut egui::Ui, config: ArcConfig) {
    ui.strong("ODF output");
    let snippet = config.snippet();
    let mut snippet_display = snippet.clone();
    egui::ScrollArea::vertical()
        .max_height(125.0)
        .show(ui, |ui| {
            ui.add(
                egui::TextEdit::multiline(&mut snippet_display)
                    .font(egui::TextStyle::Monospace)
                    .desired_width(f32::INFINITY)
                    .interactive(false),
            );
        });
    ui.horizontal(|ui| {
        if ui.button("Copy ODF block").clicked() {
            ui.output_mut(|output| output.copied_text = snippet.clone());
        }
        if ui.button("Save snippet…").clicked() {
            if let Some(path) = FileDialog::new()
                .set_file_name("firearc-snippet.odf")
                .save_file()
            {
                if let Err(error) = std::fs::write(&path, &snippet) {
                    eprintln!("Could not save {}: {error}", path.display());
                }
            }
        }
    });
}

fn handle_ship_load(
    mut requests: EventReader<LoadShipRequest>,
    mut model_requests: EventWriter<LoadModelRequest>,
    mut state: ResMut<ArcLabState>,
    mut commands: Commands,
) {
    for request in requests.read() {
        if let Some(model) = state.loaded_model.take() {
            commands.entity(model.root).despawn_recursive();
        }
        state.status = format!("Loading {}…", request.0.display());
        state.error = None;
        match load_project(&request.0) {
            Ok(project) => {
                let model_path = project.model_path.clone();
                let weapon_count = project.weapons.len();
                state.project = Some(project);
                state.selected_weapon = 0;
                state.selected_hardpoint = 0;
                state.config = state
                    .project
                    .as_ref()
                    .and_then(|project| project.weapons.first())
                    .and_then(|weapon| weapon.arc.config)
                    .unwrap_or_default();
                state.dirty = false;
                state.status = format!("Resolved {weapon_count} weapon slot(s)");
                if let Some(path) = model_path {
                    model_requests.send(LoadModelRequest(path));
                }
            }
            Err(error) => {
                state.project = None;
                state.error = Some(format!("Could not load ship ODF: {error:#}"));
            }
        }
    }
}

fn handle_model_load(
    mut requests: EventReader<LoadModelRequest>,
    mut state: ResMut<ArcLabState>,
    mut camera_request: ResMut<CameraRequest>,
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<StandardMaterial>>,
    mut images: ResMut<Assets<Image>>,
) {
    for request in requests.read() {
        if let Some(model) = state.loaded_model.take() {
            commands.entity(model.root).despawn_recursive();
        }
        let texture_roots = state
            .project
            .as_ref()
            .map(|project| project.resources.texture_roots())
            .unwrap_or_default();
        match spawn_sod(
            &request.0,
            &texture_roots,
            &mut commands,
            &mut meshes,
            &mut materials,
            &mut images,
        ) {
            Ok(model) => {
                if let Some(project) = state.project.as_mut() {
                    project.model_path = Some(request.0.clone());
                }
                let camera_fit = (model.center, model.radius);
                state.status = format!(
                    "Loaded {} nodes, {} hardpoints, and {}/{} textures from {}{}",
                    model.node_count,
                    model.hardpoint_count,
                    model.loaded_textures.len(),
                    model.referenced_texture_count,
                    request.0.display(),
                    if model.texture_issues.is_empty() {
                        String::new()
                    } else {
                        format!(" ({} texture warning(s))", model.texture_issues.len())
                    }
                );
                state.loaded_model = Some(model);
                state.error = None;
                camera_request.fit = Some(camera_fit);
            }
            Err(error) => {
                state.error = Some(format!("Could not load SOD: {error:#}"));
            }
        }
    }
}

fn orbit_camera(
    mut cameras: Query<(&mut Transform, &mut OrbitCamera)>,
    mut motions: EventReader<MouseMotion>,
    mut wheels: EventReader<MouseWheel>,
    buttons: Res<ButtonInput<MouseButton>>,
    mut contexts: EguiContexts,
    mut request: ResMut<CameraRequest>,
) {
    let Ok((mut transform, mut orbit)) = cameras.get_single_mut() else {
        return;
    };
    let pointer_owned = contexts.ctx_mut().wants_pointer_input();
    let motion = motions
        .read()
        .fold(Vec2::ZERO, |sum, event| sum + event.delta);
    if !pointer_owned && buttons.pressed(MouseButton::Right) {
        orbit.yaw -= motion.x * 0.008;
        orbit.pitch = (orbit.pitch - motion.y * 0.008).clamp(-1.54, 1.54);
    }
    if !pointer_owned && buttons.pressed(MouseButton::Middle) {
        let right = transform.rotation * Vec3::X;
        let up = transform.rotation * Vec3::Y;
        let scale = orbit.radius * 0.0015;
        orbit.target += right * -motion.x * scale + up * motion.y * scale;
    }
    if !pointer_owned {
        for wheel in wheels.read() {
            orbit.radius = (orbit.radius * (1.0 - wheel.y * 0.08)).clamp(0.1, 100_000.0);
        }
    } else {
        wheels.clear();
    }

    if let Some((center, radius)) = request.fit.take() {
        orbit.target = center;
        orbit.radius = (radius * 2.6).max(1.0);
    }
    if let Some(preset) = request.preset.take() {
        match preset {
            CameraPreset::Perspective => {
                orbit.yaw = 0.65;
                orbit.pitch = 0.42;
            }
            CameraPreset::Front => {
                orbit.yaw = 0.0;
                orbit.pitch = 0.0;
            }
            CameraPreset::Side => {
                orbit.yaw = FRAC_PI_2;
                orbit.pitch = 0.0;
            }
            CameraPreset::Top => {
                orbit.yaw = 0.0;
                orbit.pitch = 1.535;
            }
        }
    }
    apply_orbit_transform(&mut transform, &orbit);
}

fn apply_orbit_transform(transform: &mut Transform, orbit: &OrbitCamera) {
    let (sin_yaw, cos_yaw) = orbit.yaw.sin_cos();
    let (sin_pitch, cos_pitch) = orbit.pitch.sin_cos();
    let direction = Vec3::new(cos_pitch * sin_yaw, sin_pitch, cos_pitch * cos_yaw);
    transform.translation = orbit.target + direction * orbit.radius;
    transform.look_at(orbit.target, Vec3::Y);
}

fn keyboard_hardpoint_cycle(keys: Res<ButtonInput<KeyCode>>, mut state: ResMut<ArcLabState>) {
    if keys.just_pressed(KeyCode::BracketLeft) {
        state.cycle_hardpoint(-1);
    }
    if keys.just_pressed(KeyCode::BracketRight) {
        state.cycle_hardpoint(1);
    }
}

fn draw_reference_gizmos(state: Res<ArcLabState>, mut gizmos: Gizmos) {
    if !state.show_reference_grid {
        return;
    }
    let Some(model) = state.loaded_model.as_ref() else {
        return;
    };
    let center = model.center;
    let extent = model.radius.max(1.0) * 1.35;
    let divisions = 10;
    let color = Color::rgba(0.18, 0.28, 0.42, 0.36);
    for index in -divisions..=divisions {
        let offset = extent * index as f32 / divisions as f32;
        gizmos.line(
            center + Vec3::new(-extent, 0.0, offset),
            center + Vec3::new(extent, 0.0, offset),
            color,
        );
        gizmos.line(
            center + Vec3::new(offset, 0.0, -extent),
            center + Vec3::new(offset, 0.0, extent),
            color,
        );
    }
    let axis = extent * 0.45;
    gizmos.arrow(center, center + Vec3::X * axis, Color::RED);
    gizmos.arrow(center, center + Vec3::Y * axis, Color::GREEN);
    gizmos.arrow(center, center + Vec3::Z * axis, Color::BLUE);
}

fn draw_fire_arc_gizmos(
    state: Res<ArcLabState>,
    nodes: Query<(&SodNodeMarker, &GlobalTransform)>,
    mut gizmos: Gizmos,
) {
    let Some(model) = state.loaded_model.as_ref() else {
        return;
    };
    let Some(weapon) = state.selected_weapon() else {
        return;
    };
    let hardpoints = visible_hardpoints(&state, weapon);
    let radius = (model.radius * 1.28).max(1.0);
    for (visible_index, hardpoint) in hardpoints.iter().enumerate() {
        let Some((_, transform)) = nodes
            .iter()
            .find(|(node, _)| node.name.eq_ignore_ascii_case(hardpoint))
        else {
            continue;
        };
        let origin = transform.translation();
        let color = hardpoint_color(visible_index, state.show_all_hardpoints);
        gizmos.sphere(
            origin,
            Quat::IDENTITY,
            (model.radius * 0.018).max(0.03),
            color,
        );
        draw_arc_gizmo(&mut gizmos, origin, radius, state.config, color);
    }

    if state.show_probe {
        let direction = direction_for(state.probe_yaw, state.probe_pitch);
        let target = model.center + direction * radius;
        let allowed = state.config.allows_identity(direction.to_array());
        let color = if allowed {
            Color::LIME_GREEN
        } else {
            Color::RED
        };
        gizmos.line(model.center, target, color);
        gizmos.sphere(
            target,
            Quat::IDENTITY,
            (model.radius * 0.025).max(0.04),
            color,
        );
    }
}

fn draw_arc_gizmo(gizmos: &mut Gizmos, origin: Vec3, radius: f32, config: ArcConfig, color: Color) {
    let config = config.normalized();
    let centre = direction_for(config.yaw_degrees, config.pitch_degrees);
    gizmos.arrow(origin, origin + centre * radius * 0.92, color);
    match config.mode() {
        ArcMode::Box => draw_box_gizmo(gizmos, origin, radius, config, color),
        ArcMode::Cone => draw_cone_gizmo(gizmos, origin, radius, config, color),
    }
}

fn draw_box_gizmo(gizmos: &mut Gizmos, origin: Vec3, radius: f32, config: ArcConfig, color: Color) {
    let (yaw_min, yaw_max) = config.yaw_limits();
    let (pitch_min, pitch_max) = config.pitch_limits();
    let yaw_steps = ((config.yaw_angle_degrees / 6.0).ceil() as usize).clamp(2, 64);
    let pitch_steps = ((config.pitch_angle_degrees / 10.0).ceil() as usize).clamp(2, 18);

    for pitch_index in 0..=pitch_steps.min(8) {
        let fraction = pitch_index as f32 / pitch_steps.min(8) as f32;
        let pitch = pitch_min + (pitch_max - pitch_min) * fraction;
        let mut previous = None;
        for yaw_index in 0..=yaw_steps {
            let yaw_fraction = yaw_index as f32 / yaw_steps as f32;
            let yaw = yaw_min + (yaw_max - yaw_min) * yaw_fraction;
            let point = origin + direction_for(yaw, pitch) * radius;
            if let Some(previous) = previous {
                gizmos.line(previous, point, color);
            }
            previous = Some(point);
        }
    }

    let longitude_count = 8usize;
    for yaw_index in 0..=longitude_count {
        let fraction = yaw_index as f32 / longitude_count as f32;
        let yaw = yaw_min + (yaw_max - yaw_min) * fraction;
        let mut previous = None;
        for pitch_index in 0..=pitch_steps {
            let pitch_fraction = pitch_index as f32 / pitch_steps as f32;
            let pitch = pitch_min + (pitch_max - pitch_min) * pitch_fraction;
            let point = origin + direction_for(yaw, pitch) * radius;
            if let Some(previous) = previous {
                gizmos.line(previous, point, color);
            }
            previous = Some(point);
        }
    }
    for (yaw, pitch) in [
        (yaw_min, pitch_min),
        (yaw_min, pitch_max),
        (yaw_max, pitch_min),
        (yaw_max, pitch_max),
    ] {
        gizmos.line(origin, origin + direction_for(yaw, pitch) * radius, color);
    }
}

fn draw_cone_gizmo(
    gizmos: &mut Gizmos,
    origin: Vec3,
    radius: f32,
    config: ArcConfig,
    color: Color,
) {
    if config.cone_angle_degrees >= 359.999 {
        gizmos.sphere(origin, Quat::IDENTITY, radius, color);
        return;
    }
    let centre = direction_for(config.yaw_degrees, config.pitch_degrees).normalize_or_zero();
    let helper = if centre.dot(Vec3::Y).abs() > 0.98 {
        Vec3::X
    } else {
        Vec3::Y
    };
    let right = centre.cross(helper).normalize_or_zero();
    let up = right.cross(centre).normalize_or_zero();
    let half_angle = (config.cone_angle_degrees * 0.5).to_radians();
    let (sin_half, cos_half) = half_angle.sin_cos();
    let segments = 48usize;
    let mut first = None;
    let mut previous = None;
    for index in 0..=segments {
        let theta = index as f32 / segments as f32 * TAU;
        let radial = right * theta.cos() + up * theta.sin();
        let direction = centre * cos_half + radial * sin_half;
        let point = origin + direction * radius;
        if first.is_none() {
            first = Some(point);
        }
        if let Some(previous) = previous {
            gizmos.line(previous, point, color);
        }
        if index % 6 == 0 {
            gizmos.line(origin, point, color);
        }
        previous = Some(point);
    }
    if let (Some(first), Some(last)) = (first, previous) {
        gizmos.line(last, first, color);
    }
}

fn draw_hardpoint_labels(
    context: &egui::Context,
    state: &ArcLabState,
    nodes: &Query<(&SodNodeMarker, &GlobalTransform)>,
    camera_query: &Query<(&Camera, &GlobalTransform), With<OrbitCamera>>,
) {
    let Some(weapon) = state.selected_weapon() else {
        return;
    };
    let Ok((camera, camera_transform)) = camera_query.get_single() else {
        return;
    };
    let painter = context.layer_painter(egui::LayerId::new(
        egui::Order::Foreground,
        egui::Id::new("hardpoint_labels"),
    ));
    for hardpoint in visible_hardpoints(state, weapon) {
        let Some((_, transform)) = nodes
            .iter()
            .find(|(node, _)| node.name.eq_ignore_ascii_case(hardpoint))
        else {
            continue;
        };
        if let Some(position) = camera.world_to_viewport(camera_transform, transform.translation())
        {
            painter.text(
                egui::pos2(position.x + 7.0, position.y - 7.0),
                egui::Align2::LEFT_BOTTOM,
                hardpoint,
                egui::FontId::monospace(12.0),
                egui::Color32::WHITE,
            );
        }
    }
}

fn visible_hardpoints<'a>(state: &ArcLabState, weapon: &'a WeaponSlot) -> Vec<&'a str> {
    if state.show_all_hardpoints {
        weapon.hardpoints.iter().map(String::as_str).collect()
    } else {
        weapon
            .hardpoints
            .get(state.selected_hardpoint)
            .map(|name| vec![name.as_str()])
            .unwrap_or_default()
    }
}

fn hardpoint_color(index: usize, all: bool) -> Color {
    if !all {
        return Color::YELLOW;
    }
    match index % 8 {
        0 => Color::CYAN,
        1 => Color::ORANGE,
        2 => Color::LIME_GREEN,
        3 => Color::FUCHSIA,
        4 => Color::YELLOW,
        5 => Color::rgb(0.35, 0.65, 1.0),
        6 => Color::rgb(1.0, 0.45, 0.58),
        _ => Color::rgb(0.75, 0.55, 1.0),
    }
}

fn direction_for(yaw_degrees: f32, pitch_degrees: f32) -> Vec3 {
    let yaw = yaw_degrees.to_radians();
    let pitch = pitch_degrees.to_radians();
    let cos_pitch = pitch.cos();
    Vec3::new(yaw.sin() * cos_pitch, pitch.sin(), yaw.cos() * cos_pitch)
}

fn box_preset(yaw: f32, pitch: f32, yaw_width: f32, pitch_width: f32) -> ArcConfig {
    let mut config = ArcConfig::default();
    config.yaw_degrees = yaw;
    config.pitch_degrees = pitch;
    config.yaw_angle_degrees = yaw_width;
    config.pitch_angle_degrees = pitch_width;
    config
}

fn same_config(left: ArcConfig, right: ArcConfig) -> bool {
    left.mode() == right.mode()
        && left.yaw_degrees.to_bits() == right.yaw_degrees.to_bits()
        && left.pitch_degrees.to_bits() == right.pitch_degrees.to_bits()
        && left.yaw_angle_degrees.to_bits() == right.yaw_angle_degrees.to_bits()
        && left.pitch_angle_degrees.to_bits() == right.pitch_angle_degrees.to_bits()
        && left.cone_angle_degrees.to_bits() == right.cone_angle_degrees.to_bits()
}
