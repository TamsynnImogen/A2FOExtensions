use crate::project::{load_project, IconPosition, IconProject, PanelPreview, SpriteAsset};
use crate::save::save_positions;
use anyhow::{Context, Result};
use bevy::prelude::*;
use bevy::render::render_asset::RenderAssetUsages;
use bevy::render::texture::{CompressedImageFormats, ImageSampler, ImageType};
use bevy_egui::{egui, EguiContexts, EguiPlugin};
use rfd::FileDialog;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

pub struct IconLabPlugin {
    initial_ship: Option<PathBuf>,
}

impl IconLabPlugin {
    pub fn new(initial_ship: Option<PathBuf>) -> Self {
        Self { initial_ship }
    }
}

impl Plugin for IconLabPlugin {
    fn build(&self, app: &mut App) {
        app.add_plugins(EguiPlugin)
            .insert_resource(ClearColor(Color::rgb(0.018, 0.024, 0.042)))
            .insert_resource(InitialShip(self.initial_ship.clone()))
            .init_resource::<IconLabState>()
            .add_event::<LoadProjectRequest>()
            .add_systems(Startup, (configure_visuals, open_initial_ship))
            .add_systems(Update, (handle_project_load, draw_ui).chain());
    }
}

#[derive(Resource)]
struct InitialShip(Option<PathBuf>);

#[derive(Event)]
struct LoadProjectRequest {
    path: PathBuf,
    override_root: Option<PathBuf>,
    success_message: Option<String>,
}

#[derive(Debug, Clone)]
struct LoadedTexture {
    _handle: Handle<Image>,
    texture_id: egui::TextureId,
    width: f32,
    height: f32,
}

#[derive(Resource)]
struct IconLabState {
    project: Option<IconProject>,
    last_ship_path: Option<PathBuf>,
    selected_weapon: usize,
    asset_root_override: Option<PathBuf>,
    show_faction_panel: bool,
    show_grid: bool,
    status: String,
    error: Option<String>,
    textures: HashMap<PathBuf, LoadedTexture>,
}

impl Default for IconLabState {
    fn default() -> Self {
        Self {
            project: None,
            last_ship_path: None,
            selected_weapon: 0,
            asset_root_override: None,
            show_faction_panel: true,
            show_grid: false,
            status: "Open a ship or station ODF to begin.".to_string(),
            error: None,
            textures: HashMap::new(),
        }
    }
}

fn configure_visuals(mut contexts: EguiContexts) {
    let ctx = contexts.ctx_mut();
    let mut visuals = egui::Visuals::dark();
    visuals.panel_fill = egui::Color32::from_rgb(10, 16, 29);
    visuals.window_fill = egui::Color32::from_rgb(12, 20, 35);
    visuals.selection.bg_fill = egui::Color32::from_rgb(20, 126, 168);
    ctx.set_visuals(visuals);
}

fn open_initial_ship(initial: Res<InitialShip>, mut requests: EventWriter<LoadProjectRequest>) {
    if let Some(path) = initial.0.clone() {
        requests.send(LoadProjectRequest {
            path,
            override_root: None,
            success_message: None,
        });
    }
}

fn handle_project_load(
    mut requests: EventReader<LoadProjectRequest>,
    mut state: ResMut<IconLabState>,
) {
    for request in requests.read() {
        state.last_ship_path = Some(request.path.clone());
        state.asset_root_override = request.override_root.clone();
        if !request.path.is_file() {
            state.error = Some(format!(
                "Ship ODF does not exist: {}",
                request.path.display()
            ));
            continue;
        }
        match load_project(&request.path, request.override_root.as_deref()) {
            Ok(project) => {
                let weapon_count = project.weapons.len();
                state.project = Some(project);
                state.selected_weapon = state.selected_weapon.min(weapon_count.saturating_sub(1));
                state.error = None;
                state.status = request.success_message.clone().unwrap_or_else(|| {
                    format!(
                        "Loaded {} weapon slot{}.",
                        weapon_count,
                        if weapon_count == 1 { "" } else { "s" }
                    )
                });
            }
            Err(error) => {
                state.error = Some(format!("{error:#}"));
                state.status = "The selected ODF was not loaded.".to_string();
            }
        }
    }
}

fn draw_ui(
    mut contexts: EguiContexts,
    mut state: ResMut<IconLabState>,
    mut requests: EventWriter<LoadProjectRequest>,
    mut images: ResMut<Assets<Image>>,
) {
    // EguiContexts already owns mutable access to EguiUserTextures. Clone the
    // cheap, reference-counted egui context so texture registration can use
    // that same SystemParam without requesting the resource a second time.
    let ctx = contexts.ctx_mut().clone();
    let mut open_path = None;
    let mut reload = false;
    let mut choose_root = false;
    let mut save_now = false;

    ctx.input(|input| {
        if input.modifiers.command && input.key_pressed(egui::Key::S) {
            save_now = true;
        }
    });

    egui::TopBottomPanel::top("top_bar").show(&ctx, |ui| {
        ui.horizontal(|ui| {
            ui.heading("A2FO Icon Lab");
            ui.separator();
            if ui.button("Open Ship ODF").clicked() {
                let mut dialog = FileDialog::new().add_filter("Armada ODF", &["odf"]);
                if let Some(path) = state.project.as_ref().map(|project| &project.ship_path) {
                    if let Some(directory) = path.parent() {
                        dialog = dialog.set_directory(directory);
                    }
                }
                open_path = dialog.pick_file();
            }
            let has_path = state.last_ship_path.is_some();
            if ui
                .add_enabled(has_path, egui::Button::new("Reload"))
                .clicked()
            {
                reload = true;
            }
            if ui
                .add_enabled(has_path, egui::Button::new("Choose Asset Root"))
                .on_hover_text("Use a mod or Data folder when the ODF is stored elsewhere")
                .clicked()
            {
                choose_root = true;
            }
            let dirty = state.project.as_ref().is_some_and(IconProject::dirty);
            if ui
                .add_enabled(dirty, egui::Button::new("Save All  Ctrl+S"))
                .clicked()
            {
                save_now = true;
            }
        });
    });

    egui::SidePanel::left("weapon_list")
        .resizable(true)
        .default_width(310.0)
        .min_width(250.0)
        .show(&ctx, |ui| draw_left_panel(ui, &mut state));

    egui::CentralPanel::default().show(&ctx, |ui| {
        draw_preview(ui, &mut state, &mut images, &mut contexts);
    });

    egui::TopBottomPanel::bottom("status_bar").show(&ctx, |ui| {
        if let Some(error) = state.error.as_deref() {
            ui.colored_label(egui::Color32::from_rgb(255, 120, 105), error);
        } else {
            ui.label(&state.status);
        }
    });

    if let Some(path) = open_path {
        requests.send(LoadProjectRequest {
            path,
            override_root: None,
            success_message: None,
        });
    }
    if reload {
        if let Some(path) = state.last_ship_path.clone() {
            requests.send(LoadProjectRequest {
                path,
                override_root: state.asset_root_override.clone(),
                success_message: None,
            });
        }
    }
    if choose_root {
        let mut dialog = FileDialog::new();
        if let Some(root) = state.asset_root_override.as_deref().or_else(|| {
            state
                .project
                .as_ref()?
                .resources
                .roots
                .first()
                .map(|root| root.path.as_path())
        }) {
            dialog = dialog.set_directory(root);
        }
        if let (Some(root), Some(path)) = (dialog.pick_folder(), state.last_ship_path.clone()) {
            requests.send(LoadProjectRequest {
                path,
                override_root: Some(root),
                success_message: None,
            });
        }
    }
    if save_now {
        save_current_project(&mut state, &mut requests);
    }
}

fn draw_left_panel(ui: &mut egui::Ui, state: &mut IconLabState) {
    let Some(project) = state.project.as_ref() else {
        ui.heading("Weapon slots");
        ui.add_space(8.0);
        ui.weak("Open a ship ODF to resolve its faction UI, system background, and weapons.");
        return;
    };

    let ship_name = project.ship_name.clone();
    let race = project.race.clone();
    let model_name = project.model_name.clone();
    let system_sprite_key = project.system_sprite_key.clone();
    let interface_cfg = project.interface_cfg.clone();
    let interface_sprites = project.interface_sprites.clone();
    let ship_path = project.ship_path.clone();
    let warnings = project.warnings.clone();
    let rows = project
        .weapons
        .iter()
        .map(|weapon| {
            let position = weapon
                .position
                .map(|position| format!("{}, {}", position.x, position.y))
                .unwrap_or_else(|| "not placed".to_string());
            (
                weapon.slot,
                weapon.display_name.clone(),
                weapon.odf_name.clone(),
                position,
                weapon.dirty(),
            )
        })
        .collect::<Vec<_>>();

    ui.heading(ship_name);
    ui.label(format!("Race: {race}"));
    ui.label(format!("Model/UI key: {model_name}"));
    ui.small(format!("Sprite: {system_sprite_key}"));
    if let Some(cfg) = interface_cfg.as_deref() {
        ui.small(format!("Faction CFG: {cfg}"));
    }
    if let Some(sprites) = interface_sprites.as_deref() {
        ui.small(format!("Faction SPR: {sprites}"));
    }
    ui.add_space(8.0);

    ui.horizontal(|ui| {
        ui.checkbox(&mut state.show_faction_panel, "Faction panel");
        ui.checkbox(&mut state.show_grid, "10% grid");
    });
    ui.separator();
    ui.label(egui::RichText::new("Weapon slots").strong());

    let weapon_list_height = (ui.available_height() - 225.0).max(100.0);
    egui::ScrollArea::vertical()
        .id_source("weapon_slots_scroll")
        .max_height(weapon_list_height)
        .auto_shrink([false, false])
        .show(ui, |ui| {
            for (index, (slot, name, odf, position, dirty)) in rows.iter().enumerate() {
                let label = format!(
                    "{}weapon{} — {}\n    {}  •  {}",
                    if *dirty { "* " } else { "" },
                    slot,
                    name,
                    odf,
                    position
                );
                if ui
                    .selectable_label(state.selected_weapon == index, label)
                    .clicked()
                {
                    state.selected_weapon = index;
                }
            }
        });

    let selected = state.selected_weapon;
    if let Some(project) = state.project.as_mut() {
        if let Some(weapon) = project.weapons.get_mut(selected) {
            ui.separator();
            ui.label(egui::RichText::new(format!("weapon{} position", weapon.slot)).strong());
            let mut position = weapon.position.unwrap_or(IconPosition { x: 50, y: 50 });
            let changed = ui
                .horizontal(|ui| {
                    let x_changed = ui
                        .add(
                            egui::DragValue::new(&mut position.x)
                                .clamp_range(0..=100)
                                .prefix("X "),
                        )
                        .changed();
                    let y_changed = ui
                        .add(
                            egui::DragValue::new(&mut position.y)
                                .clamp_range(0..=100)
                                .prefix("Y "),
                        )
                        .changed();
                    x_changed || y_changed
                })
                .inner;
            if changed {
                weapon.position = Some(position.clamped());
            }
            ui.horizontal(|ui| {
                if ui.button("Centre").clicked() {
                    weapon.position = Some(IconPosition { x: 50, y: 50 });
                }
                if ui
                    .add_enabled(weapon.dirty(), egui::Button::new("Reset"))
                    .clicked()
                {
                    weapon.position = weapon.original_position;
                }
            });
            if let Some((source, line)) = weapon.position_source.as_ref() {
                ui.small(format!("Loaded from {}:{}", source.display(), line));
                if source != &ship_path {
                    ui.weak("Saving creates an override in the opened ship ODF.");
                }
            } else {
                ui.weak("No existing icon position; saving appends one.");
            }
        }
    }

    if !warnings.is_empty() {
        ui.separator();
        ui.collapsing(format!("Warnings ({})", warnings.len()), |ui| {
            for warning in &warnings {
                ui.colored_label(egui::Color32::YELLOW, warning);
            }
        });
    }
}

fn draw_preview(
    ui: &mut egui::Ui,
    state: &mut IconLabState,
    images: &mut Assets<Image>,
    contexts: &mut EguiContexts,
) {
    let Some(project) = state.project.as_ref() else {
        ui.centered_and_justified(|ui| {
            ui.label(
                egui::RichText::new("Open a ship ODF to place weapon icons")
                    .size(22.0)
                    .color(egui::Color32::from_gray(150)),
            );
        });
        return;
    };

    let background = project.system_background.clone();
    let panel = project.panel.clone();
    let system_icon_rect = project.system_icon_rect;
    let weapons = project
        .weapons
        .iter()
        .map(|weapon| (weapon.slot, weapon.position, weapon.sprite.clone()))
        .collect::<Vec<_>>();
    let mut selected = state.selected_weapon.min(weapons.len().saturating_sub(1));

    ui.horizontal(|ui| {
        ui.heading("System background");
        ui.label(format!(
            "{} × {}",
            background.entry.rect.width, background.entry.rect.height
        ));
        ui.weak(
            "Click an icon to select it, drag an icon to move it, or click the backdrop to place the selection.",
        );
    });
    ui.add_space(6.0);

    let use_panel = state.show_faction_panel && panel.is_some();
    let background_width = background.entry.rect.width.max(1) as f32;
    let background_height = background.entry.rect.height.max(1) as f32;
    let (natural_width, natural_height, background_origin) =
        if let Some(panel) = panel.as_ref().filter(|_| use_panel) {
            let panel_width = panel.width.max(background.entry.rect.width) as f32;
            let panel_height = panel.height.max(background.entry.rect.height) as f32;
            let vertical_inset = ((panel_height - background_height) * 0.5).max(0.0);
            (
                panel_width,
                panel_height,
                egui::pos2(
                    (panel_width - background_width - vertical_inset).max(0.0),
                    vertical_inset,
                ),
            )
        } else {
            (background_width, background_height, egui::Pos2::ZERO)
        };

    let available = ui.available_size();
    let scale = (available.x / natural_width)
        .min((available.y - 54.0).max(80.0) / natural_height)
        .clamp(0.08, 2.5);
    let canvas_size = egui::vec2(natural_width * scale, natural_height * scale);
    let allocation_height = (canvas_size.y + 8.0).max(120.0).min(available.y.max(120.0));
    let (response, painter) = ui.allocate_painter(
        egui::vec2(available.x.max(100.0), allocation_height),
        egui::Sense::click_and_drag(),
    );
    let canvas = egui::Rect::from_min_size(
        egui::pos2(
            response.rect.center().x - canvas_size.x * 0.5,
            response.rect.top() + 4.0,
        ),
        canvas_size,
    );
    painter.rect_filled(canvas, 4.0, egui::Color32::BLACK);

    let mut texture_error = None;
    if let Some(panel) = panel.as_ref().filter(|_| use_panel) {
        draw_panel(
            &painter,
            canvas.min,
            scale,
            panel,
            &mut state.textures,
            images,
            contexts,
            &mut texture_error,
        );
    }

    let background_rect = egui::Rect::from_min_size(
        canvas.min + background_origin.to_vec2() * scale,
        egui::vec2(background_width * scale, background_height * scale),
    );
    if let Err(error) = draw_sprite(
        &painter,
        background_rect,
        &background,
        egui::Color32::WHITE,
        &mut state.textures,
        images,
        contexts,
    ) {
        texture_error = Some(format!("{error:#}"));
    }
    painter.rect_stroke(
        background_rect,
        0.0,
        egui::Stroke::new(1.0, egui::Color32::from_rgb(70, 165, 205)),
    );

    if state.show_grid {
        for step in 1..10 {
            let fraction = step as f32 / 10.0;
            let color = egui::Color32::from_white_alpha(35);
            painter.line_segment(
                [
                    egui::pos2(
                        egui::lerp(background_rect.x_range(), fraction),
                        background_rect.top(),
                    ),
                    egui::pos2(
                        egui::lerp(background_rect.x_range(), fraction),
                        background_rect.bottom(),
                    ),
                ],
                egui::Stroke::new(1.0, color),
            );
            painter.line_segment(
                [
                    egui::pos2(
                        background_rect.left(),
                        egui::lerp(background_rect.y_range(), fraction),
                    ),
                    egui::pos2(
                        background_rect.right(),
                        egui::lerp(background_rect.y_range(), fraction),
                    ),
                ],
                egui::Stroke::new(1.0, color),
            );
        }
    }

    let mut icon_hit_rects = Vec::with_capacity(weapons.len());
    for (index, (slot, position, sprite)) in weapons.iter().enumerate() {
        let Some(position) = position else {
            continue;
        };
        let centre = egui::pos2(
            background_rect.left() + background_rect.width() * position.x as f32 / 100.0,
            background_rect.top() + background_rect.height() * position.y as f32 / 100.0,
        );
        let icon_size = system_icon_rect
            .map(|rect| {
                egui::vec2(
                    rect.width.max(1) as f32 * scale,
                    rect.height.max(1) as f32 * scale,
                )
            })
            .or_else(|| {
                sprite.as_ref().map(|sprite| {
                    egui::vec2(
                        sprite.entry.rect.width.max(1) as f32 * scale,
                        sprite.entry.rect.height.max(1) as f32 * scale,
                    )
                })
            })
            .unwrap_or_else(|| egui::vec2(22.0, 22.0));
        let icon_rect = egui::Rect::from_center_size(centre, icon_size);
        icon_hit_rects.push((index, icon_rect.expand(2.0)));
        if let Some(sprite) = sprite {
            if let Err(error) = draw_sprite(
                &painter,
                icon_rect,
                sprite,
                if index == selected {
                    egui::Color32::WHITE
                } else {
                    egui::Color32::from_white_alpha(175)
                },
                &mut state.textures,
                images,
                contexts,
            ) {
                texture_error = Some(format!("{error:#}"));
            }
        } else {
            painter.circle_filled(
                centre,
                icon_size.x * 0.45,
                egui::Color32::from_rgb(25, 80, 110),
            );
        }
        let outline = if index == selected {
            egui::Stroke::new(2.5, egui::Color32::from_rgb(255, 211, 65))
        } else {
            egui::Stroke::new(1.0, egui::Color32::from_white_alpha(95))
        };
        painter.rect_stroke(icon_rect.expand(2.0), 3.0, outline);
        painter.text(
            icon_rect.left_top() + egui::vec2(-3.0, -3.0),
            egui::Align2::RIGHT_BOTTOM,
            slot.to_string(),
            egui::FontId::proportional(12.0),
            egui::Color32::WHITE,
        );
    }

    let pointer = response.interact_pointer_pos();
    let clicked_icon = response
        .clicked()
        .then(|| pointer.and_then(|position| icon_at_position(&icon_hit_rects, position)))
        .flatten();
    let dragged_icon = response
        .drag_started()
        .then(|| {
            ui.input(|input| input.pointer.press_origin())
                .and_then(|position| icon_at_position(&icon_hit_rects, position))
        })
        .flatten();
    if let Some(index) = dragged_icon.or(clicked_icon) {
        selected = index;
        state.selected_weapon = index;
        if response.clicked() {
            if let Some((slot, _, _)) = weapons.get(index) {
                state.status = format!("Selected weapon{slot}.");
            }
        }
    }

    let placing = (response.dragged() || (response.clicked() && clicked_icon.is_none()))
        && response
            .interact_pointer_pos()
            .is_some_and(|position| background_rect.contains(position));
    if placing {
        if let Some(pointer) = response.interact_pointer_pos() {
            let position = IconPosition {
                x: (((pointer.x - background_rect.left()) / background_rect.width()) * 100.0)
                    .round() as i32,
                y: (((pointer.y - background_rect.top()) / background_rect.height()) * 100.0)
                    .round() as i32,
            }
            .clamped();
            if let Some(project) = state.project.as_mut() {
                if let Some(weapon) = project.weapons.get_mut(selected) {
                    weapon.position = Some(position);
                    state.status = format!(
                        "weapon{}iconpos = {} {} (not saved)",
                        weapon.slot, position.x, position.y
                    );
                }
            }
        }
    }

    if let Some(error) = texture_error {
        state.error = Some(error);
    }
    if let Some((slot, Some(position), _)) = weapons.get(selected) {
        ui.horizontal(|ui| {
            ui.label(
                egui::RichText::new(format!(
                    "Selected: weapon{}iconpos = {} {}",
                    slot, position.x, position.y
                ))
                .strong(),
            );
            ui.weak("Coordinates are rounded to whole percentages before saving.");
        });
    }
}

fn icon_at_position(icon_hit_rects: &[(usize, egui::Rect)], position: egui::Pos2) -> Option<usize> {
    icon_hit_rects
        .iter()
        .rev()
        .find_map(|(index, rect)| rect.contains(position).then_some(*index))
}

#[allow(clippy::too_many_arguments)]
fn draw_panel(
    painter: &egui::Painter,
    origin: egui::Pos2,
    scale: f32,
    panel: &PanelPreview,
    cache: &mut HashMap<PathBuf, LoadedTexture>,
    images: &mut Assets<Image>,
    contexts: &mut EguiContexts,
    error: &mut Option<String>,
) {
    for piece in &panel.pieces {
        let rect = egui::Rect::from_min_size(
            origin + egui::vec2(piece.target.x as f32, piece.target.y as f32) * scale,
            egui::vec2(piece.target.width as f32, piece.target.height as f32) * scale,
        );
        if let Err(problem) = draw_sprite(
            painter,
            rect,
            &piece.sprite,
            egui::Color32::WHITE,
            cache,
            images,
            contexts,
        ) {
            *error = Some(format!("{problem:#}"));
        }
    }
}

fn draw_sprite(
    painter: &egui::Painter,
    target: egui::Rect,
    sprite: &SpriteAsset,
    tint: egui::Color32,
    cache: &mut HashMap<PathBuf, LoadedTexture>,
    images: &mut Assets<Image>,
    contexts: &mut EguiContexts,
) -> Result<()> {
    let texture = ensure_texture(&sprite.texture_path, cache, images, contexts)?;
    let rect = sprite.entry.rect;
    let uv = egui::Rect::from_min_max(
        egui::pos2(
            rect.x as f32 / texture.width,
            rect.y as f32 / texture.height,
        ),
        egui::pos2(
            (rect.x + rect.width) as f32 / texture.width,
            (rect.y + rect.height) as f32 / texture.height,
        ),
    );
    painter.image(texture.texture_id, target, uv, tint);
    Ok(())
}

fn ensure_texture(
    path: &Path,
    cache: &mut HashMap<PathBuf, LoadedTexture>,
    images: &mut Assets<Image>,
    contexts: &mut EguiContexts,
) -> Result<LoadedTexture> {
    let key = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
    if let Some(texture) = cache.get(&key) {
        return Ok(texture.clone());
    }
    let image = decode_texture(&key)?;
    let width = image.texture_descriptor.size.width as f32;
    let height = image.texture_descriptor.size.height as f32;
    let handle = images.add(image);
    let texture_id = contexts.add_image(handle.clone());
    let texture = LoadedTexture {
        _handle: handle,
        texture_id,
        width,
        height,
    };
    cache.insert(key, texture.clone());
    Ok(texture)
}

pub(crate) fn decode_texture(path: &Path) -> Result<Image> {
    let bytes = fs::read(path).with_context(|| format!("Read texture {}", path.display()))?;
    let extension = path
        .extension()
        .and_then(|extension| extension.to_str())
        .unwrap_or("tga");
    #[cfg(debug_assertions)]
    let image = Image::from_buffer(
        path.display().to_string(),
        &bytes,
        ImageType::Extension(extension),
        CompressedImageFormats::BC,
        true,
        ImageSampler::default(),
        RenderAssetUsages::all(),
    )?;
    #[cfg(not(debug_assertions))]
    let image = Image::from_buffer(
        &bytes,
        ImageType::Extension(extension),
        CompressedImageFormats::BC,
        true,
        ImageSampler::default(),
        RenderAssetUsages::all(),
    )?;
    Ok(image)
}

fn save_current_project(state: &mut IconLabState, requests: &mut EventWriter<LoadProjectRequest>) {
    let Some(project) = state.project.as_ref() else {
        return;
    };
    let updates = project
        .weapons
        .iter()
        .filter(|weapon| weapon.dirty())
        .filter_map(|weapon| weapon.position.map(|position| (weapon.slot, position)))
        .collect::<Vec<_>>();
    if updates.is_empty() {
        state.status = "There are no changed positions to save.".to_string();
        return;
    }
    let path = project.ship_path.clone();
    match save_positions(&path, &updates) {
        Ok(backup) => {
            let message = if let Some(backup) = backup {
                format!(
                    "Saved {} icon position{}; backup: {}",
                    updates.len(),
                    if updates.len() == 1 { "" } else { "s" },
                    backup.display()
                )
            } else {
                format!(
                    "Saved {} icon position{}.",
                    updates.len(),
                    if updates.len() == 1 { "" } else { "s" }
                )
            };
            requests.send(LoadProjectRequest {
                path,
                override_root: state.asset_root_override.clone(),
                success_message: Some(message),
            });
        }
        Err(error) => {
            state.error = Some(format!("Save failed: {error:#}"));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::icon_at_position;
    use bevy_egui::egui;

    #[test]
    fn icon_hit_test_selects_the_topmost_overlapping_icon() {
        let rects = vec![
            (
                2,
                egui::Rect::from_min_max(egui::pos2(10.0, 10.0), egui::pos2(30.0, 30.0)),
            ),
            (
                7,
                egui::Rect::from_min_max(egui::pos2(20.0, 20.0), egui::pos2(40.0, 40.0)),
            ),
        ];

        assert_eq!(icon_at_position(&rects, egui::pos2(25.0, 25.0)), Some(7));
        assert_eq!(icon_at_position(&rects, egui::pos2(15.0, 15.0)), Some(2));
        assert_eq!(icon_at_position(&rects, egui::pos2(50.0, 50.0)), None);
    }
}
