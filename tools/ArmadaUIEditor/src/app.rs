use crate::extensions;
use crate::model::{discover_game_root, GameMode, UiDocument, UiRect};
use crate::sprite::{
    collect_sprite_assets, faction_sprite_for_cfg, SpriteAsset, SpriteIndex, TextureIndex,
};
use anyhow::{Context, Result};
use bevy::prelude::*;
use bevy::render::render_asset::RenderAssetUsages;
use bevy::render::texture::{CompressedImageFormats, ImageSampler, ImageType};
use bevy_egui::{egui, EguiContexts, EguiPlugin};
use rfd::FileDialog;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

const SCOPE_HUD: &str = "__hud__";
const SCOPE_ALL: &str = "__all__";
const SCOPE_FO_MULTI_NORMAL: &str = "__fo_multi_normal__";
const SCOPE_FO_MULTI_COMPACT: &str = "__fo_multi_compact__";

type RectangleSnapshot = (usize, String, UiRect, UiRect, bool, Option<String>);

pub struct ArmadaUiEditorPlugin {
    mode: GameMode,
    root: Option<PathBuf>,
    cfg: Option<PathBuf>,
}

impl ArmadaUiEditorPlugin {
    pub fn new(mode: GameMode, root: Option<PathBuf>, cfg: Option<PathBuf>) -> Self {
        Self { mode, root, cfg }
    }
}

impl Plugin for ArmadaUiEditorPlugin {
    fn build(&self, app: &mut App) {
        app.add_plugins(EguiPlugin)
            .insert_resource(ClearColor(Color::rgb(0.012, 0.018, 0.032)))
            .insert_resource(EditorState::new(
                self.mode,
                self.root.clone(),
                self.cfg.clone(),
            ))
            .add_systems(Startup, (configure_visuals, load_initial_document))
            .add_systems(Update, draw_ui);
    }
}

#[derive(Debug, Clone, Copy)]
enum HistoryEntry {
    Rectangle {
        index: usize,
        before: UiRect,
        after: UiRect,
    },
    Color {
        index: usize,
        before: [f32; 3],
        after: [f32; 3],
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum BrowserTab {
    Layout,
    Extensions,
    Colors,
}

#[derive(Debug, Clone, Copy)]
enum ExtensionPreview {
    Text {
        rect: UiRect,
        text: &'static str,
        color: egui::Color32,
    },
    Experience {
        rect: UiRect,
        background: egui::Color32,
        fill: egui::Color32,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum DragKind {
    Move,
    Resize,
}

#[derive(Debug, Clone)]
struct ActiveDrag {
    index: usize,
    before: UiRect,
    kind: DragKind,
}

#[derive(Debug, Clone)]
struct LoadedTexture {
    _handle: Handle<Image>,
    texture_id: egui::TextureId,
    width: f32,
    height: f32,
}

#[derive(Resource)]
struct EditorState {
    mode: GameMode,
    a1_root: PathBuf,
    a2_root: PathBuf,
    initial_cfg: Option<PathBuf>,
    document: Option<UiDocument>,
    sprites: SpriteIndex,
    texture_index: TextureIndex,
    loaded_textures: HashMap<PathBuf, LoadedTexture>,
    selected: Option<usize>,
    selected_color: Option<usize>,
    browser_tab: BrowserTab,
    scope: String,
    filter: String,
    selected_system_background: Option<String>,
    system_background_filter: String,
    show_sprites: bool,
    show_rectangles: bool,
    show_grid: bool,
    snap: i32,
    zoom: f32,
    status: String,
    error: Option<String>,
    undo: Vec<HistoryEntry>,
    redo: Vec<HistoryEntry>,
    active_drag: Option<ActiveDrag>,
}

impl EditorState {
    fn new(mode: GameMode, root: Option<PathBuf>, cfg: Option<PathBuf>) -> Self {
        let mut a1_root = GameMode::Armada1.default_root();
        let mut a2_root = GameMode::Armada2.default_root();
        if let Some(root) = root {
            match mode {
                GameMode::Armada1 => a1_root = root,
                GameMode::Armada2 => a2_root = root,
            }
        }
        Self {
            mode,
            a1_root,
            a2_root,
            initial_cfg: cfg,
            document: None,
            sprites: SpriteIndex::default(),
            texture_index: TextureIndex::default(),
            loaded_textures: HashMap::new(),
            selected: None,
            selected_color: None,
            browser_tab: BrowserTab::Layout,
            scope: SCOPE_HUD.to_string(),
            filter: String::new(),
            selected_system_background: None,
            system_background_filter: String::new(),
            show_sprites: true,
            show_rectangles: true,
            show_grid: false,
            snap: 1,
            zoom: 1.0,
            status: "Loading the stock interface…".to_string(),
            error: None,
            undo: Vec::new(),
            redo: Vec::new(),
            active_drag: None,
        }
    }

    fn root_for(&self, mode: GameMode) -> &Path {
        match mode {
            GameMode::Armada1 => &self.a1_root,
            GameMode::Armada2 => &self.a2_root,
        }
    }

    fn has_unsaved_changes(&self) -> bool {
        self.document
            .as_ref()
            .is_some_and(|document| document.dirty_count() > 0)
    }

    fn refuse_discard(&mut self, action: &str) -> bool {
        if !self.has_unsaved_changes() {
            return false;
        }
        self.error = Some(format!(
            "Save All or Reset All before {action}; unsaved layout changes were kept."
        ));
        true
    }

    fn set_root_for(&mut self, mode: GameMode, root: PathBuf) {
        match mode {
            GameMode::Armada1 => self.a1_root = root,
            GameMode::Armada2 => self.a2_root = root,
        }
    }

    fn load(&mut self, mode: GameMode, cfg: &Path, root_override: Option<&Path>) -> Result<()> {
        let fallback = root_override
            .map(Path::to_path_buf)
            .unwrap_or_else(|| self.root_for(mode).to_path_buf());
        let root = root_override
            .map(Path::to_path_buf)
            .unwrap_or_else(|| discover_game_root(cfg, &fallback));
        let document = UiDocument::load(mode, &root, cfg)?;
        let faction_sprite = faction_sprite_for_cfg(&document.primary_path);
        let sprites = SpriteIndex::load(&document.game_root, faction_sprite.as_deref())?;
        let system_backgrounds = sprites.system_background_names();
        let texture_index = TextureIndex::build(&document.game_root);
        let rectangle_count = document.rectangles.len();
        let source_count = document.source_count();
        let canvas = (document.screen_width, document.screen_height);
        let root = document.game_root.clone();
        let cfg_name = document
            .primary_path
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("GUI CFG")
            .to_string();

        self.mode = mode;
        self.set_root_for(mode, root);
        self.document = Some(document);
        self.sprites = sprites;
        self.texture_index = texture_index;
        self.loaded_textures.clear();
        self.selected = None;
        self.selected_color = None;
        self.selected_system_background = system_backgrounds.first().cloned();
        self.scope = SCOPE_HUD.to_string();
        self.undo.clear();
        self.redo.clear();
        self.active_drag = None;
        self.error = None;
        self.status = format!(
            "Loaded {cfg_name}: {rectangle_count} rectangles from {source_count} CFG file(s), canvas {}×{}.",
            canvas.0, canvas.1
        );
        Ok(())
    }

    fn load_default(&mut self, mode: GameMode) -> Result<()> {
        let root = self.root_for(mode).to_path_buf();
        let cfg = mode.default_cfg(&root);
        self.load(mode, &cfg, Some(&root))
    }

    fn apply_edit(&mut self, index: usize, after: UiRect, record: bool) {
        let Some(document) = self.document.as_mut() else {
            return;
        };
        let Some(before) = document.rectangles.get(index).map(|entry| entry.rect) else {
            return;
        };
        if !document.set_rect(index, after) {
            return;
        }
        if record {
            self.undo.push(HistoryEntry::Rectangle {
                index,
                before,
                after,
            });
            self.redo.clear();
        }
        self.status = format!(
            "{} = {} {} {} {} (not saved)",
            document.rectangles[index].key, after.x, after.y, after.width, after.height
        );
    }

    fn apply_color_edit(&mut self, index: usize, after: [f32; 3], record: bool) {
        let Some(document) = self.document.as_mut() else {
            return;
        };
        let Some(before) = document.colors.get(index).map(|entry| entry.value) else {
            return;
        };
        if !document.set_color(index, after) {
            return;
        }
        let after = document.colors[index].value;
        if record {
            self.undo.push(HistoryEntry::Color {
                index,
                before,
                after,
            });
            self.redo.clear();
        }
        self.status = format!(
            "{} = {:.3} {:.3} {:.3} (not saved)",
            document.colors[index].key, after[0], after[1], after[2]
        );
    }

    fn add_extension_rectangle(&mut self, key: &str, rect: UiRect) {
        let Some(document) = self.document.as_mut() else {
            return;
        };
        let existed = document.rectangle_index(key).is_some();
        let index = document.add_rectangle(key, rect);
        self.selected = Some(index);
        self.selected_color = None;
        self.scope = document
            .group_for(index)
            .map(str::to_string)
            .unwrap_or_else(|| SCOPE_HUD.to_string());
        if !existed {
            self.undo.clear();
            self.redo.clear();
        }
        self.status = if existed {
            format!("Selected existing {key}.")
        } else {
            format!("Added {key} as an unsaved override.")
        };
    }

    fn add_extension_color(&mut self, key: &str, value: [f32; 3]) {
        let Some(document) = self.document.as_mut() else {
            return;
        };
        let existed = document.color_index(key).is_some();
        let index = document.add_color(key, value);
        self.selected = None;
        self.selected_color = Some(index);
        if !existed {
            self.undo.clear();
            self.redo.clear();
        }
        self.status = if existed {
            format!("Selected existing {key}.")
        } else {
            format!("Added {key} as an unsaved override.")
        };
    }

    fn commit_drag(&mut self) {
        let Some(drag) = self.active_drag.take() else {
            return;
        };
        let Some(after) = self
            .document
            .as_ref()
            .and_then(|document| document.rectangles.get(drag.index))
            .map(|entry| entry.rect)
        else {
            return;
        };
        if drag.before != after {
            self.undo.push(HistoryEntry::Rectangle {
                index: drag.index,
                before: drag.before,
                after,
            });
            self.redo.clear();
        }
    }

    fn undo(&mut self) {
        let Some(edit) = self.undo.pop() else {
            return;
        };
        if let Some(document) = self.document.as_mut() {
            match edit {
                HistoryEntry::Rectangle { index, before, .. } => {
                    document.set_rect(index, before);
                    self.selected = Some(index);
                    self.selected_color = None;
                    self.status = format!("Undid change to {}.", document.rectangles[index].key);
                }
                HistoryEntry::Color { index, before, .. } => {
                    document.set_color(index, before);
                    self.selected = None;
                    self.selected_color = Some(index);
                    self.status = format!("Undid colour change to {}.", document.colors[index].key);
                }
            }
            self.redo.push(edit);
        }
    }

    fn redo(&mut self) {
        let Some(edit) = self.redo.pop() else {
            return;
        };
        if let Some(document) = self.document.as_mut() {
            match edit {
                HistoryEntry::Rectangle { index, after, .. } => {
                    document.set_rect(index, after);
                    self.selected = Some(index);
                    self.selected_color = None;
                    self.status = format!("Redid change to {}.", document.rectangles[index].key);
                }
                HistoryEntry::Color { index, after, .. } => {
                    document.set_color(index, after);
                    self.selected = None;
                    self.selected_color = Some(index);
                    self.status = format!("Redid colour change to {}.", document.colors[index].key);
                }
            }
            self.undo.push(edit);
        }
    }

    fn reset_all(&mut self) {
        let Some(document) = self.document.as_mut() else {
            return;
        };
        let count = document.dirty_count();
        if count == 0 {
            return;
        }
        document.reset_all();
        self.undo.clear();
        self.redo.clear();
        self.selected = None;
        self.selected_color = None;
        self.error = None;
        self.status = format!("Reset {count} unsaved layout/colour change(s).");
    }

    fn save(&mut self) {
        let dirty = self
            .document
            .as_ref()
            .map(UiDocument::dirty_count)
            .unwrap_or(0);
        if dirty == 0 {
            self.status = "There are no changed layout or colour values to save.".to_string();
            return;
        }
        let result = self
            .document
            .as_mut()
            .expect("dirty document exists")
            .save();
        match result {
            Ok(report) => {
                self.error = None;
                self.undo.clear();
                self.redo.clear();
                self.selected = None;
                self.selected_color = None;
                self.status = format!(
                    "Saved {} rectangle{} and {} colour{} across {} CFG file{}; {} new backup{} created.",
                    report.rectangle_count,
                    if report.rectangle_count == 1 { "" } else { "s" },
                    report.color_count,
                    if report.color_count == 1 { "" } else { "s" },
                    report.files.len(),
                    if report.files.len() == 1 { "" } else { "s" },
                    report.backups.len(),
                    if report.backups.len() == 1 { "" } else { "s" },
                );
            }
            Err(error) => self.error = Some(format!("Save failed: {error:#}")),
        }
    }
}

fn configure_visuals(mut contexts: EguiContexts) {
    let ctx = contexts.ctx_mut();
    let mut visuals = egui::Visuals::dark();
    visuals.panel_fill = egui::Color32::from_rgb(9, 15, 28);
    visuals.window_fill = egui::Color32::from_rgb(12, 20, 35);
    visuals.selection.bg_fill = egui::Color32::from_rgb(15, 118, 158);
    visuals.widgets.active.bg_fill = egui::Color32::from_rgb(18, 132, 170);
    ctx.set_visuals(visuals);
}

fn load_initial_document(mut state: ResMut<EditorState>) {
    let mode = state.mode;
    let cfg = state
        .initial_cfg
        .take()
        .unwrap_or_else(|| mode.default_cfg(state.root_for(mode)));
    if let Err(error) = state.load(mode, &cfg, None) {
        state.error = Some(format!("Initial layout could not be loaded: {error:#}"));
        state.status = "Choose an Armada installation or GUI CFG to begin.".to_string();
    }
}

#[allow(clippy::too_many_arguments)]
fn draw_ui(
    mut contexts: EguiContexts,
    mut state: ResMut<EditorState>,
    mut images: ResMut<Assets<Image>>,
) {
    let ctx = contexts.ctx_mut().clone();
    let mut switch_mode = None;
    let mut open_cfg = false;
    let mut choose_root = false;
    let mut reload = false;
    let mut save = false;
    let mut reset_all = false;

    ctx.input(|input| {
        if input.modifiers.command && input.key_pressed(egui::Key::S) {
            save = true;
        }
        if input.modifiers.command && input.key_pressed(egui::Key::Z) {
            if input.modifiers.shift {
                state.redo();
            } else {
                state.undo();
            }
        } else if input.modifiers.command && input.key_pressed(egui::Key::Y) {
            state.redo();
        }
    });
    nudge_selected(&ctx, &mut state);

    egui::TopBottomPanel::top("toolbar").show(&ctx, |ui| {
        ui.horizontal_wrapped(|ui| {
            ui.heading("Armada UI Editor");
            ui.separator();
            if ui
                .selectable_label(state.mode == GameMode::Armada1, "Armada I")
                .clicked()
                && state.mode != GameMode::Armada1
            {
                switch_mode = Some(GameMode::Armada1);
            }
            if ui
                .selectable_label(state.mode == GameMode::Armada2, "Armada II")
                .clicked()
                && state.mode != GameMode::Armada2
            {
                switch_mode = Some(GameMode::Armada2);
            }
            ui.separator();
            if ui.button("Open CFG…").clicked() {
                open_cfg = true;
            }
            if ui.button("Choose Game Root…").clicked() {
                choose_root = true;
            }
            if ui
                .add_enabled(state.document.is_some(), egui::Button::new("Reload"))
                .clicked()
            {
                reload = true;
            }
            let dirty = state
                .document
                .as_ref()
                .map(UiDocument::dirty_count)
                .unwrap_or(0);
            if ui
                .add_enabled(
                    dirty > 0,
                    egui::Button::new(format!("Save All ({dirty})  Ctrl+S")),
                )
                .clicked()
            {
                save = true;
            }
            ui.separator();
            if ui
                .add_enabled(!state.undo.is_empty(), egui::Button::new("Undo"))
                .clicked()
            {
                state.undo();
            }
            if ui
                .add_enabled(!state.redo.is_empty(), egui::Button::new("Redo"))
                .clicked()
            {
                state.redo();
            }
            if ui
                .add_enabled(dirty > 0, egui::Button::new("Reset All"))
                .clicked()
            {
                reset_all = true;
            }
        });
    });

    egui::SidePanel::left("layout_entries")
        .resizable(true)
        .default_width(330.0)
        .min_width(260.0)
        .show(&ctx, |ui| draw_left_panel(ui, &mut state));

    egui::SidePanel::right("rectangle_inspector")
        .resizable(true)
        .default_width(285.0)
        .min_width(235.0)
        .show(&ctx, |ui| draw_inspector(ui, &mut state));

    egui::CentralPanel::default().show(&ctx, |ui| {
        draw_canvas(ui, &ctx, &mut state, &mut images, &mut contexts)
    });

    egui::TopBottomPanel::bottom("status_bar").show(&ctx, |ui| {
        if let Some(error) = state.error.as_deref() {
            ui.colored_label(egui::Color32::from_rgb(255, 122, 110), error);
        } else {
            ui.label(&state.status);
        }
    });

    if let Some(mode) = switch_mode {
        if state.refuse_discard("switching game mode") {
            // Keep the current document and mode.
        } else if let Err(error) = state.load_default(mode) {
            state.error = Some(format!(
                "{} mode could not be loaded: {error:#}",
                mode.label()
            ));
        }
    }
    if open_cfg && !state.refuse_discard("opening another CFG") {
        let directory = state.root_for(state.mode).join("misc");
        let path = FileDialog::new()
            .set_directory(directory)
            .add_filter("Armada GUI CFG", &["cfg"])
            .pick_file();
        if let Some(path) = path {
            let mode = state.mode;
            if let Err(error) = state.load(mode, &path, None) {
                state.error = Some(format!("CFG could not be loaded: {error:#}"));
            }
        }
    }
    if choose_root && !state.refuse_discard("choosing another game root") {
        let root = FileDialog::new()
            .set_directory(state.root_for(state.mode))
            .pick_folder();
        if let Some(root) = root {
            let mode = state.mode;
            state.set_root_for(mode, root);
            if let Err(error) = state.load_default(mode) {
                state.error = Some(format!("Game root could not be loaded: {error:#}"));
            }
        }
    }
    if reload && !state.refuse_discard("reloading from disk") {
        if let Some((mode, path, root)) = state.document.as_ref().map(|document| {
            (
                document.mode,
                document.primary_path.clone(),
                document.game_root.clone(),
            )
        }) {
            if let Err(error) = state.load(mode, &path, Some(&root)) {
                state.error = Some(format!("Reload failed: {error:#}"));
            }
        }
    }
    if save {
        state.save();
    }
    if reset_all {
        state.reset_all();
    }
}

fn draw_left_panel(ui: &mut egui::Ui, state: &mut EditorState) {
    ui.heading("Browser");
    let Some(summary) = state.document.as_ref().map(|document| {
        (
            document.mode.short_label(),
            document.screen_width,
            document.screen_height,
            document.source_count(),
            document.primary_path.display().to_string(),
        )
    }) else {
        ui.add_space(8.0);
        ui.weak("Open a gui_*.cfg file to inspect its editable rectangles.");
        return;
    };
    ui.label(format!(
        "{} • {}×{} • {} source file{}",
        summary.0,
        summary.1,
        summary.2,
        summary.3,
        if summary.3 == 1 { "" } else { "s" }
    ));
    ui.small(summary.4);
    ui.add_space(6.0);
    ui.horizontal_wrapped(|ui| {
        ui.selectable_value(&mut state.browser_tab, BrowserTab::Layout, "Layout");
        ui.selectable_value(&mut state.browser_tab, BrowserTab::Extensions, "A2FO / FO");
        ui.selectable_value(&mut state.browser_tab, BrowserTab::Colors, "Colours");
    });
    ui.separator();
    match state.browser_tab {
        BrowserTab::Layout => draw_layout_browser(ui, state),
        BrowserTab::Extensions => draw_extension_browser(ui, state),
        BrowserTab::Colors => draw_color_browser(ui, state),
    }
}

fn draw_layout_browser(ui: &mut egui::Ui, state: &mut EditorState) {
    ui.heading("Layout rectangles");
    ui.horizontal(|ui| {
        ui.label("Find");
        ui.text_edit_singleline(&mut state.filter);
    });
    ui.add_space(4.0);

    let filter = state.filter.to_ascii_lowercase();
    let Some(document) = state.document.as_ref() else {
        return;
    };
    let rows = document
        .rectangles
        .iter()
        .enumerate()
        .filter(|(_, entry)| filter.is_empty() || entry.key.to_ascii_lowercase().contains(&filter))
        .map(|(index, entry)| {
            (
                index,
                entry.key.clone(),
                entry.rect,
                entry.dirty(),
                document.group_for(index).unwrap_or("global").to_string(),
                entry
                    .source
                    .file_name()
                    .and_then(|value| value.to_str())
                    .unwrap_or("cfg")
                    .to_string(),
            )
        })
        .collect::<Vec<_>>();

    egui::ScrollArea::vertical()
        .id_source("layout_rectangle_rows")
        .auto_shrink([false, false])
        .show(ui, |ui| {
            for (index, key, rect, dirty, group, source) in rows {
                let label = format!(
                    "{}{}\n  {} {} {} {}  •  {}  •  {}",
                    if dirty { "* " } else { "" },
                    key,
                    rect.x,
                    rect.y,
                    rect.width,
                    rect.height,
                    group,
                    source
                );
                if ui
                    .selectable_label(state.selected == Some(index), label)
                    .clicked()
                {
                    state.selected = Some(index);
                    state.selected_color = None;
                    if group != "global" {
                        state.scope = group;
                    }
                }
            }
        });
}

fn draw_extension_browser(ui: &mut egui::Ui, state: &mut EditorState) {
    ui.heading("A2FO HUD elements");
    ui.small("Mission selector excluded. New entries are unsaved overrides in the open GUI CFG.");
    let missing = state.document.as_ref().map_or(0, |document| {
        extensions::rectangle_templates()
            .filter(|template| document.rectangle_index(template.key).is_none())
            .count()
    });
    let add_all = ui
        .add_enabled(
            missing > 0,
            egui::Button::new(format!("Add all missing ({missing})")),
        )
        .clicked();

    let rows = state.document.as_ref().map_or_else(Vec::new, |document| {
        extensions::rectangle_templates()
            .map(|template| {
                let index = document.rectangle_index(template.key);
                let dirty = index.is_some_and(|index| document.rectangles[index].dirty());
                (*template, index, dirty)
            })
            .collect::<Vec<_>>()
    });
    let mut action = None;
    egui::ScrollArea::vertical()
        .id_source("a2fo_element_rows")
        .auto_shrink([false, false])
        .show(ui, |ui| {
            let mut section = "";
            for (template, index, dirty) in rows {
                if section != template.section {
                    section = template.section;
                    ui.add_space(7.0);
                    ui.label(egui::RichText::new(section).strong());
                }
                ui.horizontal(|ui| {
                    let status = if dirty {
                        "*"
                    } else if index.is_some() {
                        "✓"
                    } else {
                        "+"
                    };
                    let response = ui
                        .selectable_label(
                            index.is_some_and(|index| state.selected == Some(index)),
                            format!("{status} {}", template.label),
                        )
                        .on_hover_text(format!("{}\n{}", template.key, template.description));
                    if response.clicked() {
                        action = Some((template.key, template.default));
                    }
                });
            }

            ui.add_space(10.0);
            ui.separator();
            ui.heading("Fleet Operations previews");
            let names = state.sprites.system_background_names();
            if names.is_empty() {
                ui.weak("No systembackgrounds.spr entries were found in this root.");
            } else {
                ui.label(format!("System backgrounds ({})", names.len()));
                ui.text_edit_singleline(&mut state.system_background_filter);
                let filter = state.system_background_filter.to_ascii_lowercase();
                egui::ComboBox::from_id_source("system_background_preview")
                    .selected_text(
                        state
                            .selected_system_background
                            .as_deref()
                            .unwrap_or("Choose a system background"),
                    )
                    .show_ui(ui, |ui| {
                        for name in names.iter().filter(|name| {
                            filter.is_empty() || name.to_ascii_lowercase().contains(&filter)
                        }) {
                            ui.selectable_value(
                                &mut state.selected_system_background,
                                Some(name.clone()),
                                name,
                            );
                        }
                    });
            }

            let (normal, compact) = multi_select_counts(state.document.as_ref());
            ui.add_space(7.0);
            ui.label(egui::RichText::new("Multi-selection stages").strong());
            ui.horizontal_wrapped(|ui| {
                if ui
                    .add_enabled(normal > 0, egui::Button::new(format!("Normal ({normal})")))
                    .clicked()
                {
                    state.scope = SCOPE_FO_MULTI_NORMAL.to_string();
                }
                if ui
                    .add_enabled(
                        compact > 0,
                        egui::Button::new(format!("Compact ({compact})")),
                    )
                    .clicked()
                {
                    state.scope = SCOPE_FO_MULTI_COMPACT.to_string();
                }
            });
            ui.small(
                "Normal covers the 16 large slots; compact covers Fleet Operations' 30 Sm slots.",
            );
        });

    if add_all {
        for template in extensions::rectangle_templates() {
            if state
                .document
                .as_ref()
                .is_some_and(|document| document.rectangle_index(template.key).is_none())
            {
                state.add_extension_rectangle(template.key, template.default);
            }
        }
        state.selected = None;
        state.status = format!("Added {missing} missing A2FO/FO rectangle override(s).");
    } else if let Some((key, rect)) = action {
        state.add_extension_rectangle(key, rect);
    }
}

fn draw_color_browser(ui: &mut egui::Ui, state: &mut EditorState) {
    ui.heading("Element colours");
    ui.small(
        "Edits RGB float triplets from the loaded CFG chain, including A2FO and native FO colours.",
    );
    let missing = state.document.as_ref().map_or(0, |document| {
        extensions::color_templates()
            .filter(|template| document.color_index(template.key).is_none())
            .count()
    });
    let add_all = ui
        .add_enabled(
            missing > 0,
            egui::Button::new(format!("Add missing A2FO colours ({missing})")),
        )
        .clicked();
    ui.horizontal(|ui| {
        ui.label("Find");
        ui.text_edit_singleline(&mut state.filter);
    });

    let filter = state.filter.to_ascii_lowercase();
    let catalog = state.document.as_ref().map_or_else(Vec::new, |document| {
        extensions::color_templates()
            .filter(|template| {
                filter.is_empty()
                    || template.key.to_ascii_lowercase().contains(&filter)
                    || template.label.to_ascii_lowercase().contains(&filter)
            })
            .map(|template| {
                let index = document.color_index(template.key);
                let value = index
                    .and_then(|index| document.colors.get(index))
                    .map(|entry| entry.value)
                    .unwrap_or(template.default);
                let dirty = index.is_some_and(|index| document.colors[index].dirty());
                (*template, index, value, dirty)
            })
            .collect::<Vec<_>>()
    });
    let other = state.document.as_ref().map_or_else(Vec::new, |document| {
        document
            .colors
            .iter()
            .enumerate()
            .filter(|(_, color)| {
                !extensions::color_templates()
                    .any(|template| template.key.eq_ignore_ascii_case(&color.key))
                    && (filter.is_empty() || color.key.to_ascii_lowercase().contains(&filter))
            })
            .map(|(index, color)| (index, color.key.clone(), color.value, color.dirty()))
            .collect::<Vec<_>>()
    });
    let mut action = None;
    egui::ScrollArea::vertical()
        .id_source("color_property_rows")
        .auto_shrink([false, false])
        .show(ui, |ui| {
            let mut section = "";
            for (template, index, value, dirty) in catalog {
                if section != template.section {
                    section = template.section;
                    ui.add_space(7.0);
                    ui.label(egui::RichText::new(section).strong());
                }
                let swatch = rgb_to_egui(value);
                ui.horizontal(|ui| {
                    let (rect, _) =
                        ui.allocate_exact_size(egui::vec2(16.0, 16.0), egui::Sense::hover());
                    ui.painter().rect_filled(rect, 2.0, swatch);
                    let status = if dirty {
                        "*"
                    } else if index.is_some() {
                        "✓"
                    } else {
                        "+"
                    };
                    if ui
                        .selectable_label(
                            index.is_some_and(|index| state.selected_color == Some(index)),
                            format!("{status} {}", template.label),
                        )
                        .on_hover_text(template.key)
                        .clicked()
                    {
                        action = Some((template.key, template.default));
                    }
                });
            }
            if !other.is_empty() {
                ui.add_space(8.0);
                ui.label(egui::RichText::new("Other loaded colours").strong());
                for (index, key, value, dirty) in other {
                    ui.horizontal(|ui| {
                        let (rect, _) =
                            ui.allocate_exact_size(egui::vec2(16.0, 16.0), egui::Sense::hover());
                        ui.painter().rect_filled(rect, 2.0, rgb_to_egui(value));
                        if ui
                            .selectable_label(
                                state.selected_color == Some(index),
                                format!("{}{}", if dirty { "* " } else { "" }, key),
                            )
                            .clicked()
                        {
                            state.selected = None;
                            state.selected_color = Some(index);
                        }
                    });
                }
            }
        });

    if add_all {
        for template in extensions::color_templates() {
            if state
                .document
                .as_ref()
                .is_some_and(|document| document.color_index(template.key).is_none())
            {
                state.add_extension_color(template.key, template.default);
            }
        }
        state.selected_color = None;
        state.status = format!("Added {missing} missing A2FO colour override(s).");
    } else if let Some((key, value)) = action {
        state.add_extension_color(key, value);
    }
}

fn multi_select_counts(document: Option<&UiDocument>) -> (usize, usize) {
    let Some(document) = document else {
        return (0, 0);
    };
    let mut normal = 0;
    let mut compact = 0;
    for rectangle in &document.rectangles {
        let key = rectangle.key.to_ascii_lowercase();
        if key.starts_with("infomultishipiconsm_") {
            compact += 1;
        } else if key.starts_with("infomultishipicon_") {
            normal += 1;
        }
    }
    (normal, compact)
}

fn draw_inspector(ui: &mut egui::Ui, state: &mut EditorState) {
    ui.heading("Inspector");
    if let Some(index) = state.selected_color {
        draw_color_inspector(ui, state, index);
        draw_document_warnings(ui, state);
        return;
    }
    let Some(index) = state.selected else {
        ui.add_space(8.0);
        ui.weak("Select a rectangle in the list or canvas.");
        draw_document_warnings(ui, state);
        return;
    };
    let Some(snapshot) = state.document.as_ref().and_then(|document| {
        let entry = document.rectangles.get(index)?;
        Some((
            entry.key.clone(),
            entry.rect,
            entry.original,
            entry.source.clone(),
            entry.line,
            entry.added,
            document.absolute_rect(index),
            document.group_for(index).map(str::to_string),
        ))
    }) else {
        state.selected = None;
        return;
    };
    let (key, mut rect, original, source, line, added, absolute, group) = snapshot;

    ui.label(egui::RichText::new(&key).strong());
    if let Some(group) = group.as_deref() {
        ui.small(format!("Component: {group}"));
    }
    if added {
        ui.small(format!("New override → {}", source.display()));
    } else {
        ui.small(format!("{}:{line}", source.display()));
    }
    ui.add_space(10.0);

    let mut changed = false;
    egui::Grid::new("rect_inspector_grid")
        .num_columns(2)
        .spacing([12.0, 7.0])
        .show(ui, |ui| {
            ui.label("X");
            changed |= ui.add(egui::DragValue::new(&mut rect.x).speed(1)).changed();
            ui.end_row();
            ui.label("Y");
            changed |= ui.add(egui::DragValue::new(&mut rect.y).speed(1)).changed();
            ui.end_row();
            ui.label("Width");
            changed |= ui
                .add(
                    egui::DragValue::new(&mut rect.width)
                        .speed(1)
                        .clamp_range(0..=10000),
                )
                .changed();
            ui.end_row();
            ui.label("Height");
            changed |= ui
                .add(
                    egui::DragValue::new(&mut rect.height)
                        .speed(1)
                        .clamp_range(0..=10000),
                )
                .changed();
            ui.end_row();
        });
    if changed {
        state.apply_edit(index, rect, true);
    }
    if let Some(absolute) = absolute {
        ui.add_space(5.0);
        ui.weak(format!(
            "Screen position: {}, {} → {}, {}",
            absolute.x,
            absolute.y,
            absolute.right(),
            absolute.bottom()
        ));
    }
    ui.add_space(8.0);
    ui.horizontal(|ui| {
        if added {
            if ui.button("Remove unsaved addition").clicked() {
                if let Some(document) = state.document.as_mut() {
                    document.remove_added_rectangle(index);
                }
                state.selected = None;
                state.undo.clear();
                state.redo.clear();
                state.status = format!("Removed unsaved {key} override.");
            }
        } else if ui
            .add_enabled(rect != original, egui::Button::new("Reset selected"))
            .clicked()
        {
            state.apply_edit(index, original, true);
        }
        if ui.button("Focus component").clicked() {
            if let Some(group) = group {
                state.scope = group;
            }
        }
    });
    ui.separator();
    ui.label("Mouse: drag the box to move it; drag its lower-right square to resize.");
    ui.label("Keyboard: arrow keys nudge; hold Shift for 10 units.");
    draw_document_warnings(ui, state);
}

fn draw_color_inspector(ui: &mut egui::Ui, state: &mut EditorState, index: usize) {
    let Some((key, mut value, original, source, line, added)) = state
        .document
        .as_ref()
        .and_then(|document| document.colors.get(index))
        .map(|entry| {
            (
                entry.key.clone(),
                entry.value,
                entry.original,
                entry.source.clone(),
                entry.line,
                entry.added,
            )
        })
    else {
        state.selected_color = None;
        return;
    };

    ui.label(egui::RichText::new(&key).strong());
    if added {
        ui.small(format!("New override → {}", source.display()));
    } else {
        ui.small(format!("{}:{line}", source.display()));
    }
    ui.add_space(10.0);
    let before = value;
    let response = ui.color_edit_button_rgb(&mut value);
    let mut numeric_changed = false;
    egui::Grid::new("color_inspector_grid")
        .num_columns(2)
        .spacing([12.0, 7.0])
        .show(ui, |ui| {
            for (label, channel) in ["Red", "Green", "Blue"].into_iter().zip(value.iter_mut()) {
                ui.label(label);
                numeric_changed |= ui
                    .add(
                        egui::DragValue::new(channel)
                            .speed(0.01)
                            .clamp_range(0.0..=1.0),
                    )
                    .changed();
                ui.end_row();
            }
        });
    if response.changed() || numeric_changed {
        state.apply_color_edit(index, value, true);
    }
    ui.small(format!(
        "RGB {:.3}  {:.3}  {:.3}",
        value[0], value[1], value[2]
    ));
    ui.add_space(8.0);
    if added {
        if ui.button("Remove unsaved addition").clicked() {
            if let Some(document) = state.document.as_mut() {
                document.remove_added_color(index);
            }
            state.selected_color = None;
            state.undo.clear();
            state.redo.clear();
            state.status = format!("Removed unsaved {key} override.");
        }
    } else if ui
        .add_enabled(before != original, egui::Button::new("Reset selected"))
        .clicked()
    {
        state.apply_color_edit(index, original, true);
    }
    ui.separator();
    ui.label("Colour values are saved as Armada RGB floats from 0 to 1.");
}

fn draw_document_warnings(ui: &mut egui::Ui, state: &EditorState) {
    let Some(document) = state.document.as_ref() else {
        return;
    };
    if !document.warnings.is_empty() {
        ui.add_space(12.0);
        ui.collapsing(format!("Warnings ({})", document.warnings.len()), |ui| {
            for warning in &document.warnings {
                ui.colored_label(egui::Color32::from_rgb(235, 184, 92), warning);
            }
        });
    }
}

#[allow(clippy::too_many_arguments)]
fn draw_canvas(
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    state: &mut EditorState,
    images: &mut Assets<Image>,
    contexts: &mut EguiContexts,
) {
    let Some(document) = state.document.as_ref() else {
        ui.centered_and_justified(|ui| {
            ui.heading("Choose a valid Armada game root or open a GUI CFG.");
        });
        return;
    };

    let groups = document.groups().to_vec();
    let (normal_multi, compact_multi) = multi_select_counts(Some(document));
    ui.horizontal_wrapped(|ui| {
        ui.label("View");
        egui::ComboBox::from_id_source("canvas_scope")
            .selected_text(scope_label(&state.scope))
            .show_ui(ui, |ui| {
                ui.selectable_value(&mut state.scope, SCOPE_HUD.to_string(), "Gameplay HUD");
                ui.selectable_value(&mut state.scope, SCOPE_ALL.to_string(), "All components");
                if normal_multi > 0 || compact_multi > 0 {
                    ui.separator();
                    if normal_multi > 0 {
                        ui.selectable_value(
                            &mut state.scope,
                            SCOPE_FO_MULTI_NORMAL.to_string(),
                            "FO multi-select: normal",
                        );
                    }
                    if compact_multi > 0 {
                        ui.selectable_value(
                            &mut state.scope,
                            SCOPE_FO_MULTI_COMPACT.to_string(),
                            "FO multi-select: compact",
                        );
                    }
                }
                ui.separator();
                for group in &groups {
                    ui.selectable_value(&mut state.scope, group.clone(), group);
                }
            });
        ui.separator();
        ui.checkbox(&mut state.show_sprites, "Sprites");
        ui.checkbox(&mut state.show_rectangles, "Rectangles");
        ui.checkbox(&mut state.show_grid, "Grid");
        ui.label("Snap");
        egui::ComboBox::from_id_source("snap_size")
            .selected_text(state.snap.to_string())
            .show_ui(ui, |ui| {
                for snap in [1, 2, 4, 8, 16] {
                    ui.selectable_value(&mut state.snap, snap, snap.to_string());
                }
            });
        ui.label("Zoom");
        ui.add(egui::Slider::new(&mut state.zoom, 0.35..=2.5).show_value(false));
    });
    ui.separator();

    let viewport = if state.scope == SCOPE_HUD || state.scope == SCOPE_ALL {
        UiRect {
            x: 0,
            y: 0,
            width: document.screen_width,
            height: document.screen_height,
        }
    } else if state.scope == SCOPE_FO_MULTI_NORMAL || state.scope == SCOPE_FO_MULTI_COMPACT {
        document.root_rect("info").unwrap_or(UiRect {
            x: 0,
            y: 0,
            width: document.screen_width,
            height: document.screen_height,
        })
    } else {
        document.root_rect(&state.scope).unwrap_or(UiRect {
            x: 0,
            y: 0,
            width: document.screen_width,
            height: document.screen_height,
        })
    };
    let rect_snapshots = document
        .rectangles
        .iter()
        .enumerate()
        .filter_map(|(index, entry)| {
            let absolute = document.absolute_rect(index).unwrap_or(entry.rect);
            let group = document.group_for(index).map(str::to_string);
            scope_includes(&state.scope, &entry.key, group.as_deref()).then(|| {
                (
                    index,
                    entry.key.clone(),
                    entry.rect,
                    absolute,
                    entry.dirty(),
                    group,
                )
            })
        })
        .collect::<Vec<_>>();
    let mut sprite_assets = if state.show_sprites {
        collect_sprite_assets(document, &state.sprites, &state.texture_index)
    } else {
        Vec::new()
    };
    if state.show_sprites {
        if let (Some(name), Some(index)) = (
            state.selected_system_background.as_deref(),
            document.rectangle_index("infoSingleSystemsDisplay"),
        ) {
            if let Some(target) = document.absolute_rect(index) {
                if let Some(asset) = state.sprites.resolve_asset(
                    name,
                    &state.texture_index,
                    target,
                    document.group_for(index).map(str::to_string),
                ) {
                    sprite_assets.push(asset);
                }
            }
        }
        if let Some(index) = document.rectangle_index("infoSingleDirectionalShieldsGraphicArea") {
            if let Some(target) = document.absolute_rect(index) {
                for name in ["dsf", "dsb", "dsl", "dsr"] {
                    if let Some(asset) = state.sprites.resolve_asset(
                        name,
                        &state.texture_index,
                        target,
                        document.group_for(index).map(str::to_string),
                    ) {
                        sprite_assets.push(asset);
                    }
                }
            }
        }
    }
    let extension_previews = collect_extension_previews(document, &rect_snapshots);

    let available = ui.available_size();
    let natural_width = viewport.width.max(1) as f32;
    let natural_height = viewport.height.max(1) as f32;
    let fit = (available.x / natural_width)
        .min(available.y / natural_height)
        .max(0.01);
    let scale = fit * state.zoom;
    let canvas_size = egui::vec2(natural_width * scale, natural_height * scale);
    let allocation = egui::vec2(
        available.x.max(canvas_size.x).max(120.0),
        available.y.max(canvas_size.y).max(120.0),
    );
    egui::ScrollArea::both()
        .id_source("ui_layout_canvas_scroll")
        .auto_shrink([false, false])
        .show(ui, |ui| {
            let (response, painter) = ui.allocate_painter(allocation, egui::Sense::click());
            let canvas = egui::Rect::from_min_size(response.rect.min, canvas_size);
            painter.rect_filled(canvas, 0.0, egui::Color32::from_rgb(2, 5, 10));

            if state.show_grid {
                draw_grid(&painter, canvas, viewport, scale, state.snap.max(8));
            }

            let mut texture_error = None;
            for asset in sprite_assets.iter().filter(|asset| {
                scope_includes(&state.scope, &asset.entry.name, asset.group.as_deref())
                    && rects_intersect(asset.target, viewport)
            }) {
                let target = map_rect(asset.target, viewport, canvas.min, scale);
                if let Err(error) = draw_sprite(
                    &painter,
                    target,
                    asset,
                    &mut state.loaded_textures,
                    images,
                    contexts,
                ) {
                    texture_error = Some(format!("Texture preview failed: {error:#}"));
                }
            }

            draw_extension_previews(&painter, &extension_previews, viewport, canvas.min, scale);

            let display_rects = rect_snapshots
                .iter()
                .map(|(index, key, local, absolute, dirty, group)| {
                    (
                        *index,
                        key.clone(),
                        *local,
                        *absolute,
                        *dirty,
                        group.clone(),
                        map_rect(*absolute, viewport, canvas.min, scale),
                    )
                })
                .collect::<Vec<_>>();

            if state.show_rectangles {
                for (index, _, _, _, dirty, _, display) in &display_rects {
                    let selected = state.selected == Some(*index);
                    let color = if selected {
                        egui::Color32::from_rgb(255, 214, 66)
                    } else if *dirty {
                        egui::Color32::from_rgb(255, 126, 65)
                    } else {
                        egui::Color32::from_rgba_unmultiplied(64, 191, 225, 145)
                    };
                    painter.rect_stroke(
                        *display,
                        0.0,
                        egui::Stroke::new(if selected { 2.5 } else { 1.0 }, color),
                    );
                }
            }

            if response.clicked() {
                if let Some(pointer) = response.interact_pointer_pos() {
                    let hit = display_rects
                        .iter()
                        .filter(|(_, _, _, _, _, _, rect)| rect.expand(2.0).contains(pointer))
                        .min_by_key(|(_, _, _, _, _, _, rect)| {
                            (rect.width().abs() * rect.height().abs()) as i64
                        })
                        .map(|(index, _, _, _, _, _, _)| *index);
                    state.selected = hit;
                }
            }

            if let Some(selected) = state.selected {
                if let Some((_, key, local, _, _, _, display)) =
                    display_rects.iter().find(|(index, ..)| *index == selected)
                {
                    let body_id = ui.id().with(("move_rect", selected));
                    let body = ui.interact(*display, body_id, egui::Sense::drag());
                    let handle_rect = egui::Rect::from_center_size(
                        display.right_bottom(),
                        egui::vec2(12.0, 12.0),
                    );
                    let handle_id = ui.id().with(("resize_rect", selected));
                    let handle = ui.interact(handle_rect, handle_id, egui::Sense::drag());
                    painter.rect_filled(handle_rect, 1.0, egui::Color32::from_rgb(255, 214, 66));
                    painter.text(
                        display.left_top() + egui::vec2(3.0, -3.0),
                        egui::Align2::LEFT_BOTTOM,
                        key,
                        egui::FontId::proportional(12.0),
                        egui::Color32::WHITE,
                    );

                    if handle.drag_started() {
                        state.active_drag = Some(ActiveDrag {
                            index: selected,
                            before: *local,
                            kind: DragKind::Resize,
                        });
                    } else if body.drag_started() {
                        state.active_drag = Some(ActiveDrag {
                            index: selected,
                            before: *local,
                            kind: DragKind::Move,
                        });
                    }
                    let active = state
                        .active_drag
                        .as_ref()
                        .filter(|drag| drag.index == selected);
                    if let Some(active) = active {
                        let delta = match active.kind {
                            DragKind::Resize => handle.drag_delta(),
                            DragKind::Move => body.drag_delta(),
                        };
                        let dx = snap_delta(delta.x / scale, state.snap);
                        let dy = snap_delta(delta.y / scale, state.snap);
                        let after = match active.kind {
                            DragKind::Move => UiRect {
                                x: active.before.x + dx,
                                y: active.before.y + dy,
                                ..active.before
                            },
                            DragKind::Resize => UiRect {
                                width: (active.before.width + dx).max(0),
                                height: (active.before.height + dy).max(0),
                                ..active.before
                            },
                        };
                        state.apply_edit(selected, after, false);
                    }
                }
            }

            if let Some(error) = texture_error {
                state.error = Some(error);
            }
        });

    let pointer_down = ctx.input(|input| input.pointer.primary_down());
    if !pointer_down && state.active_drag.is_some() {
        state.commit_drag();
    }
}

fn nudge_selected(ctx: &egui::Context, state: &mut EditorState) {
    if ctx.wants_keyboard_input() {
        return;
    }
    let Some(index) = state.selected else {
        return;
    };
    let Some(before) = state
        .document
        .as_ref()
        .and_then(|document| document.rectangles.get(index))
        .map(|entry| entry.rect)
    else {
        return;
    };
    let (dx, dy) = ctx.input(|input| {
        let step = if input.modifiers.shift { 10 } else { 1 };
        let mut dx = 0;
        let mut dy = 0;
        if input.key_pressed(egui::Key::ArrowLeft) {
            dx -= step;
        }
        if input.key_pressed(egui::Key::ArrowRight) {
            dx += step;
        }
        if input.key_pressed(egui::Key::ArrowUp) {
            dy -= step;
        }
        if input.key_pressed(egui::Key::ArrowDown) {
            dy += step;
        }
        (dx, dy)
    });
    if dx != 0 || dy != 0 {
        state.apply_edit(
            index,
            UiRect {
                x: before.x + dx,
                y: before.y + dy,
                ..before
            },
            true,
        );
    }
}

fn scope_label(scope: &str) -> &str {
    match scope {
        SCOPE_HUD => "Gameplay HUD",
        SCOPE_ALL => "All components",
        SCOPE_FO_MULTI_NORMAL => "FO multi-select: normal",
        SCOPE_FO_MULTI_COMPACT => "FO multi-select: compact",
        value => value,
    }
}

fn scope_includes(scope: &str, key: &str, group: Option<&str>) -> bool {
    match scope {
        SCOPE_ALL => true,
        SCOPE_HUD => group.is_some_and(is_hud_group),
        SCOPE_FO_MULTI_NORMAL => {
            let key = key.to_ascii_lowercase();
            key.starts_with("infomultiship") && !key.contains("sm_")
        }
        SCOPE_FO_MULTI_COMPACT => {
            let key = key.to_ascii_lowercase();
            key.starts_with("infomultiship") && key.contains("sm_")
        }
        value => group.is_some_and(|group| group.eq_ignore_ascii_case(value)),
    }
}

fn draw_extension_previews(
    painter: &egui::Painter,
    previews: &[ExtensionPreview],
    viewport: UiRect,
    origin: egui::Pos2,
    scale: f32,
) {
    for preview in previews {
        match *preview {
            ExtensionPreview::Text { rect, text, color } => {
                let display = map_rect(rect, viewport, origin, scale);
                painter.rect_filled(
                    display,
                    1.0,
                    egui::Color32::from_rgba_unmultiplied(2, 7, 13, 150),
                );
                painter.text(
                    display.left_center() + egui::vec2(3.0, 0.0),
                    egui::Align2::LEFT_CENTER,
                    text,
                    egui::FontId::proportional((13.0 * scale.sqrt()).clamp(8.0, 18.0)),
                    color,
                );
            }
            ExtensionPreview::Experience {
                rect,
                background,
                fill,
            } => {
                let display = map_rect(rect, viewport, origin, scale);
                painter.rect_filled(display, 1.0, background);
                let filled = egui::Rect::from_min_max(
                    display.min,
                    egui::pos2(display.left() + display.width() * 0.68, display.bottom()),
                );
                painter.rect_filled(filled, 1.0, fill);
            }
        }
    }
}

fn collect_extension_previews(
    document: &UiDocument,
    rectangles: &[RectangleSnapshot],
) -> Vec<ExtensionPreview> {
    let mut output = Vec::new();
    for (_, key, _, absolute, _, _) in rectangles {
        if let Some((text, color_key)) = extensions::preview_text(key) {
            let prefix = key.strip_suffix("Area").unwrap_or(key);
            let color = document
                .color(&format!("{prefix}Color"))
                .or_else(|| color_key.and_then(|key| document.color(key)))
                .map(rgb_to_egui)
                .unwrap_or(egui::Color32::WHITE);
            output.push(ExtensionPreview::Text {
                rect: *absolute,
                text,
                color,
            });
        } else if key.to_ascii_lowercase().ends_with("bararea")
            && (key.starts_with("infoSingle") || key.starts_with("infoBuild"))
        {
            let prefix = key.strip_suffix("Area").unwrap_or(key);
            output.push(ExtensionPreview::Experience {
                rect: *absolute,
                background: document
                    .color(&format!("{prefix}BackgroundColor"))
                    .or_else(|| document.color("experienceBarBackgroundColor"))
                    .map(rgb_to_egui)
                    .unwrap_or_else(|| egui::Color32::from_gray(64)),
                fill: document
                    .color(&format!("{prefix}Color"))
                    .or_else(|| document.color("experienceBarColor"))
                    .map(rgb_to_egui)
                    .unwrap_or_else(|| egui::Color32::from_rgb(51, 166, 255)),
            });
        }
    }
    output
}

fn rgb_to_egui(value: [f32; 3]) -> egui::Color32 {
    egui::Color32::from_rgb(
        (value[0].clamp(0.0, 1.0) * 255.0).round() as u8,
        (value[1].clamp(0.0, 1.0) * 255.0).round() as u8,
        (value[2].clamp(0.0, 1.0) * 255.0).round() as u8,
    )
}

fn is_hud_group(group: &str) -> bool {
    matches!(
        group.to_ascii_lowercase().as_str(),
        "minimap"
            | "resource"
            | "info"
            | "button"
            | "control"
            | "cinematic"
            | "speed"
            | "officer"
            | "crew"
            | "dilithium"
            | "latinum"
            | "metal"
            | "biomatter"
    )
}

fn map_rect(rect: UiRect, viewport: UiRect, origin: egui::Pos2, scale: f32) -> egui::Rect {
    egui::Rect::from_min_size(
        origin
            + egui::vec2(
                (rect.x - viewport.x) as f32 * scale,
                (rect.y - viewport.y) as f32 * scale,
            ),
        egui::vec2(
            rect.width.max(1) as f32 * scale,
            rect.height.max(1) as f32 * scale,
        ),
    )
}

fn rects_intersect(left: UiRect, right: UiRect) -> bool {
    left.x < right.right()
        && left.right() > right.x
        && left.y < right.bottom()
        && left.bottom() > right.y
}

fn snap_delta(delta: f32, snap: i32) -> i32 {
    let snap = snap.max(1) as f32;
    (delta / snap).round() as i32 * snap as i32
}

fn draw_grid(
    painter: &egui::Painter,
    canvas: egui::Rect,
    viewport: UiRect,
    scale: f32,
    spacing: i32,
) {
    let spacing = spacing.max(1);
    let color = egui::Color32::from_white_alpha(24);
    let stroke = egui::Stroke::new(1.0, color);
    let first_x = viewport.x.div_euclid(spacing) * spacing;
    let mut x = first_x;
    while x <= viewport.right() {
        let screen_x = canvas.left() + (x - viewport.x) as f32 * scale;
        painter.line_segment(
            [
                egui::pos2(screen_x, canvas.top()),
                egui::pos2(screen_x, canvas.bottom()),
            ],
            stroke,
        );
        x += spacing;
    }
    let first_y = viewport.y.div_euclid(spacing) * spacing;
    let mut y = first_y;
    while y <= viewport.bottom() {
        let screen_y = canvas.top() + (y - viewport.y) as f32 * scale;
        painter.line_segment(
            [
                egui::pos2(canvas.left(), screen_y),
                egui::pos2(canvas.right(), screen_y),
            ],
            stroke,
        );
        y += spacing;
    }
}

fn draw_sprite(
    painter: &egui::Painter,
    target: egui::Rect,
    sprite: &SpriteAsset,
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
    painter.image(texture.texture_id, target, uv, egui::Color32::WHITE);
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
