use crate::document::{editor_to_bytes, glyph_token, TooltipDocument};
use crate::glyph_builder::{build_phrase_glyph, GlyphBuildRequest};
use crate::glyphs::{FontCatalog, FontGlyph};
use crate::odf::{load_tooltip_references, looks_like_localization_key, OdfTooltipReference};
use anyhow::{Context, Result};
use bevy::prelude::*;
use bevy::render::render_asset::RenderAssetUsages;
use bevy::render::texture::{CompressedImageFormats, ImageSampler, ImageType};
use bevy_egui::{egui, EguiContexts, EguiPlugin};
use rfd::FileDialog;
use std::collections::{BTreeMap, HashMap};
use std::fs;
use std::path::{Path, PathBuf};

pub struct TooltipLabPlugin {
    initial_document: Option<PathBuf>,
}

impl TooltipLabPlugin {
    pub fn new(initial_document: Option<PathBuf>) -> Self {
        Self { initial_document }
    }
}

impl Plugin for TooltipLabPlugin {
    fn build(&self, app: &mut App) {
        app.add_plugins(EguiPlugin)
            .insert_resource(ClearColor(Color::rgb(0.018, 0.024, 0.042)))
            .insert_resource(InitialDocument(self.initial_document.clone()))
            .init_resource::<TooltipLabState>()
            .add_event::<LoadDocumentRequest>()
            .add_systems(Startup, (configure_visuals, open_initial_document))
            .add_systems(Update, (handle_document_load, draw_ui).chain());
    }
}

#[derive(Resource)]
struct InitialDocument(Option<PathBuf>);

#[derive(Event)]
struct LoadDocumentRequest {
    path: PathBuf,
    override_root: Option<PathBuf>,
    success_message: Option<String>,
    select_key: Option<String>,
}

#[derive(Debug, Clone)]
struct LoadedTexture {
    _handle: Handle<Image>,
    texture_id: egui::TextureId,
    width: f32,
    height: f32,
}

#[derive(Resource)]
struct TooltipLabState {
    document: Option<TooltipDocument>,
    catalog: Option<FontCatalog>,
    last_path: Option<PathBuf>,
    font_root_override: Option<PathBuf>,
    selected_entry: usize,
    search: String,
    filtered_entries: Vec<usize>,
    filter_dirty: bool,
    status: String,
    error: Option<String>,
    font_problem: Option<String>,
    textures: HashMap<PathBuf, LoadedTexture>,
    editor_cursor: usize,
    creating: bool,
    new_key: String,
    new_value: String,
    preview_width: f32,
    preview_scale: f32,
    odf_import: OdfImportState,
    glyph_builder: GlyphBuilderState,
}

#[derive(Debug, Clone, Default)]
struct OdfImportState {
    open: bool,
    loaded_files: usize,
    candidates: Vec<OdfImportCandidate>,
    warnings: Vec<String>,
}

#[derive(Debug, Clone)]
struct OdfImportCandidate {
    key: String,
    references: Vec<OdfTooltipReference>,
    exists: bool,
    selected: bool,
    value: String,
}

#[derive(Debug, Clone)]
struct GlyphBuilderState {
    open: bool,
    byte: u32,
    label: String,
    phrase: String,
    replacement_confirmed: bool,
}

impl Default for GlyphBuilderState {
    fn default() -> Self {
        Self {
            open: false,
            byte: 0x80,
            label: String::new(),
            phrase: String::new(),
            replacement_confirmed: false,
        }
    }
}

impl Default for TooltipLabState {
    fn default() -> Self {
        Self {
            document: None,
            catalog: None,
            last_path: None,
            font_root_override: None,
            selected_entry: 0,
            search: String::new(),
            filtered_entries: Vec::new(),
            filter_dirty: true,
            status: "Open a Dynamic_Localized_Strings.h file to begin.".to_string(),
            error: None,
            font_problem: None,
            textures: HashMap::new(),
            editor_cursor: 0,
            creating: false,
            new_key: String::new(),
            new_value: String::new(),
            preview_width: 560.0,
            preview_scale: 1.15,
            odf_import: OdfImportState::default(),
            glyph_builder: GlyphBuilderState::default(),
        }
    }
}

fn configure_visuals(mut contexts: EguiContexts) {
    let ctx = contexts.ctx_mut();
    let mut visuals = egui::Visuals::dark();
    visuals.panel_fill = egui::Color32::from_rgb(10, 16, 29);
    visuals.window_fill = egui::Color32::from_rgb(12, 20, 35);
    visuals.selection.bg_fill = egui::Color32::from_rgb(20, 126, 168);
    visuals.widgets.active.bg_fill = egui::Color32::from_rgb(18, 112, 151);
    ctx.set_visuals(visuals);
}

fn open_initial_document(
    initial: Res<InitialDocument>,
    mut requests: EventWriter<LoadDocumentRequest>,
) {
    if let Some(path) = initial.0.clone() {
        requests.send(LoadDocumentRequest {
            path,
            override_root: None,
            success_message: None,
            select_key: None,
        });
    }
}

fn handle_document_load(
    mut requests: EventReader<LoadDocumentRequest>,
    mut state: ResMut<TooltipLabState>,
) {
    for request in requests.read() {
        state.last_path = Some(request.path.clone());
        state.font_root_override = request.override_root.clone();
        if !request.path.is_file() {
            state.error = Some(format!(
                "Localized strings file does not exist: {}",
                request.path.display()
            ));
            continue;
        }

        let catalog = FontCatalog::resolve(&request.path, request.override_root.as_deref());
        let (document, catalog, font_problem) = match catalog {
            Ok(catalog) => match TooltipDocument::load_with_catalog(&request.path, &catalog) {
                Ok(document) => (Ok(document), Some(catalog), None),
                Err(error) => (Err(error), None, None),
            },
            Err(font_error) => (
                TooltipDocument::load(&request.path),
                None,
                Some(format!("{font_error:#}")),
            ),
        };
        match document {
            Ok(document) => {
                let entry_count = document.entries.len();
                state.selected_entry = request
                    .select_key
                    .as_deref()
                    .and_then(|key| {
                        document
                            .entries
                            .iter()
                            .position(|entry| entry.key.eq_ignore_ascii_case(key))
                    })
                    .unwrap_or_else(|| state.selected_entry.min(entry_count.saturating_sub(1)));
                state.document = Some(document);
                state.catalog = catalog;
                state.font_problem = font_problem;
                state.textures.clear();
                state.filter_dirty = true;
                state.creating = false;
                state.error = None;
                state.editor_cursor = 0;
                state.status = request.success_message.clone().unwrap_or_else(|| {
                    format!(
                        "Loaded {entry_count} localized string{}{}.",
                        if entry_count == 1 { "" } else { "s" },
                        if state.catalog.is_some() {
                            " with the active mod font"
                        } else {
                            ""
                        }
                    )
                });
            }
            Err(error) => {
                state.error = Some(format!("{error:#}"));
                state.status = "The selected file was not loaded.".to_string();
            }
        }
    }
}

fn draw_ui(
    mut contexts: EguiContexts,
    mut state: ResMut<TooltipLabState>,
    mut requests: EventWriter<LoadDocumentRequest>,
    mut images: ResMut<Assets<Image>>,
) {
    let ctx = contexts.ctx_mut().clone();
    let mut open_path = None;
    let mut reload = false;
    let mut choose_root = false;
    let mut save_now = false;
    let mut odf_paths = None;

    ctx.input(|input| {
        if input.modifiers.command && input.key_pressed(egui::Key::S) {
            save_now = true;
        }
    });

    let texture_path = state
        .catalog
        .as_ref()
        .map(|catalog| catalog.texture_path.clone());
    let texture = texture_path.as_deref().and_then(|path| {
        match ensure_texture(path, &mut state.textures, &mut images, &mut contexts) {
            Ok(texture) => Some(texture),
            Err(error) => {
                state.font_problem = Some(format!("{error:#}"));
                None
            }
        }
    });

    egui::TopBottomPanel::top("top_bar").show(&ctx, |ui| {
        ui.horizontal(|ui| {
            ui.heading("A2FO Tooltip Lab");
            ui.separator();
            if ui
                .button("Open DLS")
                .on_hover_text("Open Dynamic_Localized_Strings.h")
                .clicked()
            {
                let mut dialog = FileDialog::new().add_filter("Localized strings header", &["h"]);
                if let Some(directory) = state.last_path.as_deref().and_then(Path::parent) {
                    dialog = dialog.set_directory(directory);
                }
                open_path = dialog.pick_file();
            }
            if ui
                .add_enabled(state.last_path.is_some(), egui::Button::new("Reload"))
                .clicked()
            {
                reload = true;
            }
            if ui
                .add_enabled(
                    state.last_path.is_some(),
                    egui::Button::new("Choose Font / Mod Root"),
                )
                .on_hover_text("Override automatic ParentMod font and texture resolution")
                .clicked()
            {
                choose_root = true;
            }
            ui.separator();
            if ui
                .add_enabled(state.document.is_some(), egui::Button::new("New Entry"))
                .clicked()
            {
                state.creating = true;
                state.new_key.clear();
                state.new_value.clear();
                state.editor_cursor = 0;
            }
            if ui
                .add_enabled(state.document.is_some(), egui::Button::new("Import ODFs"))
                .on_hover_text("Discover tooltip keys in one or more ODF files")
                .clicked()
            {
                let mut dialog = FileDialog::new().add_filter("Fleet Operations ODF", &["odf"]);
                if let Some(directory) = state.last_path.as_deref().and_then(Path::parent) {
                    dialog = dialog.set_directory(directory);
                }
                odf_paths = dialog.pick_files();
            }
            if ui
                .add_enabled(
                    state.document.is_some() && state.catalog.is_some(),
                    egui::Button::new("Glyph Builder"),
                )
                .on_hover_text("Create a custom phrase glyph in this mod's font")
                .clicked()
            {
                if !state.glyph_builder.open {
                    state.glyph_builder.byte = suggested_glyph_byte(&state) as u32;
                    state.glyph_builder.replacement_confirmed = false;
                }
                state.glyph_builder.open = true;
            }
            let dirty = state.document.as_ref().is_some_and(TooltipDocument::dirty);
            if ui
                .add_enabled(dirty, egui::Button::new("Save All  Ctrl+S"))
                .clicked()
            {
                save_now = true;
            }
            if dirty {
                ui.colored_label(egui::Color32::from_rgb(255, 205, 80), "unsaved changes");
            }
        });
    });

    refresh_filter(&mut state);
    egui::SidePanel::left("entry_list")
        .resizable(true)
        .default_width(360.0)
        .min_width(280.0)
        .show(&ctx, |ui| draw_entry_list(ui, &mut state));

    egui::CentralPanel::default().show(&ctx, |ui| {
        draw_editor_and_preview(ui, &mut state, texture.as_ref());
    });

    egui::TopBottomPanel::bottom("status_bar").show(&ctx, |ui| {
        if let Some(error) = state.error.as_deref() {
            ui.colored_label(egui::Color32::from_rgb(255, 120, 105), error);
        } else {
            ui.label(&state.status);
        }
    });

    draw_odf_import_window(&ctx, &mut state);
    if let Some(build_request) = draw_glyph_builder_window(&ctx, &mut state, texture.as_ref()) {
        let resolved_catalog = writable_font_catalog(&state);
        let result =
            resolved_catalog.and_then(|catalog| build_phrase_glyph(&catalog, &build_request));
        match result {
            Ok(result) => {
                let document_path = state
                    .document
                    .as_ref()
                    .expect("builder requires a document")
                    .path
                    .clone();
                match FontCatalog::resolve(&document_path, state.font_root_override.as_deref()) {
                    Ok(updated_catalog) => {
                        state.catalog = Some(updated_catalog);
                        state.textures.clear();
                        state.font_problem = None;
                    }
                    Err(error) => state.font_problem = Some(format!("{error:#}")),
                }
                state.glyph_builder.open = false;
                state.glyph_builder.replacement_confirmed = false;
                state.error = None;
                state.status = format!(
                    "Built glyph 0x{:02X} at {},{} ({} px). Backups: {}; {}",
                    result.byte,
                    result.x,
                    result.y,
                    result.width,
                    result.spr_backup.display(),
                    result.texture_backup.display()
                );
            }
            Err(error) => state.error = Some(format!("Glyph build failed: {error:#}")),
        }
    }

    if let Some(path) = open_path {
        requests.send(LoadDocumentRequest {
            path,
            override_root: None,
            success_message: None,
            select_key: None,
        });
    }
    if let Some(paths) = odf_paths {
        prepare_odf_import(&mut state, paths);
    }
    if reload {
        if let Some(path) = state.last_path.clone() {
            requests.send(LoadDocumentRequest {
                path,
                override_root: state.font_root_override.clone(),
                success_message: None,
                select_key: selected_key(&state),
            });
        }
    }
    if choose_root {
        let mut dialog = FileDialog::new();
        if let Some(root) = state.font_root_override.as_deref().or_else(|| {
            state
                .catalog
                .as_ref()
                .and_then(|catalog| catalog.roots.first().map(PathBuf::as_path))
        }) {
            dialog = dialog.set_directory(root);
        }
        if let (Some(root), Some(path)) = (dialog.pick_folder(), state.last_path.clone()) {
            requests.send(LoadDocumentRequest {
                path,
                override_root: Some(root),
                success_message: Some("Reloaded with the selected font/mod root".to_string()),
                select_key: selected_key(&state),
            });
        }
    }
    if save_now {
        save_document(&mut state, &mut requests);
    }
}

fn writable_font_catalog(state: &TooltipLabState) -> Result<FontCatalog> {
    let catalog = state.catalog.as_ref().context("No active mod font")?;
    let document = state.document.as_ref().context("No active DLS document")?;
    let document_root = document
        .path
        .parent()
        .context("The DLS file has no parent directory")?;
    if state.font_root_override.is_some()
        || (catalog.spr_path.starts_with(document_root)
            && catalog.texture_path.starts_with(document_root))
    {
        return Ok(catalog.clone());
    }

    for source in [&catalog.spr_path, &catalog.texture_path] {
        if source.starts_with(document_root) {
            continue;
        }
        let relative = catalog
            .roots
            .iter()
            .find_map(|root| source.strip_prefix(root).ok())
            .with_context(|| {
                format!(
                    "Could not determine the mod-relative location of {}",
                    source.display()
                )
            })?;
        let destination = document_root.join(relative);
        if !destination.exists() {
            if let Some(parent) = destination.parent() {
                fs::create_dir_all(parent)
                    .with_context(|| format!("Create {}", parent.display()))?;
            }
            fs::copy(source, &destination).with_context(|| {
                format!(
                    "Create local font override {} from {}",
                    destination.display(),
                    source.display()
                )
            })?;
        }
    }
    FontCatalog::resolve(&document.path, None).context("Resolve the new local font override")
}

fn suggested_glyph_byte(state: &TooltipLabState) -> u8 {
    (0x7fu8..=0xff)
        .find(|byte| {
            !state
                .catalog
                .as_ref()
                .is_some_and(|catalog| catalog.is_special(*byte))
                && glyph_usage_count(state.document.as_ref(), *byte) == 0
        })
        .unwrap_or(0x80)
}

fn glyph_usage_count(document: Option<&TooltipDocument>, byte: u8) -> usize {
    document.map_or(0, |document| {
        document
            .entries
            .iter()
            .filter_map(|entry| editor_to_bytes(&entry.editor_text, &document.newline).ok())
            .map(|bytes| {
                bytes
                    .into_iter()
                    .filter(|candidate| *candidate == byte)
                    .count()
            })
            .sum()
    })
}

fn draw_glyph_builder_window(
    ctx: &egui::Context,
    state: &mut TooltipLabState,
    texture: Option<&LoadedTexture>,
) -> Option<GlyphBuildRequest> {
    if !state.glyph_builder.open {
        return None;
    }
    let catalog = state.catalog.clone()?;
    let mut open = state.glyph_builder.open;
    let mut build = false;
    let document_root = state
        .document
        .as_ref()
        .and_then(|document| document.path.parent())
        .map(Path::to_path_buf);
    let inherited = document_root.as_ref().is_some_and(|root| {
        !catalog.spr_path.starts_with(root) || !catalog.texture_path.starts_with(root)
    });
    egui::Window::new("Custom Code Glyph Builder")
        .open(&mut open)
        .default_width(700.0)
        .resizable(true)
        .show(ctx, |ui| {
            ui.label(
                "Compose a one-byte phrase glyph from characters already present in this mod's active font.",
            );
            ui.weak(
                "The phrase pixels are copied into transparent atlas space; the selected FontSmall.spr byte is then redirected to them.",
            );
            ui.separator();
            ui.horizontal(|ui| {
                ui.label("Byte code");
                let changed = ui
                    .add(
                        egui::DragValue::new(&mut state.glyph_builder.byte)
                            .clamp_range(0x7f..=0xff)
                            .hexadecimal(2, false, true),
                    )
                    .changed();
                if changed {
                    state.glyph_builder.replacement_confirmed = false;
                }
                ui.monospace(format!("decimal {}", state.glyph_builder.byte));
            });
            let byte = state.glyph_builder.byte.clamp(0x7f, 0xff) as u8;
            if let Some(existing) = catalog.glyph(byte) {
                ui.horizontal(|ui| {
                    ui.label("Current mapping:");
                    ui.monospace(format!(
                        "{},{}  width {:.1}",
                        existing.x, existing.y, existing.advance
                    ));
                    if let Some(label) = existing.label.as_deref() {
                        ui.label(label);
                    }
                });
            }
            let current_uses = glyph_usage_count(state.document.as_ref(), byte);
            if current_uses > 0 {
                ui.colored_label(
                    egui::Color32::from_rgb(255, 190, 80),
                    format!(
                        "Byte 0x{byte:02X} is currently used {current_uses} time{} in this DLS file. Those tooltips will display the new glyph.",
                        if current_uses == 1 { "" } else { "s" }
                    ),
                );
            }
            if inherited && state.font_root_override.is_none() {
                ui.colored_label(
                    egui::Color32::from_rgb(255, 190, 80),
                    format!(
                        "This font is inherited. The builder will first create local copies under this mod, leaving the parent assets untouched:\n{}\n{}",
                        catalog.spr_path.display(),
                        catalog.texture_path.display()
                    ),
                );
            } else if inherited {
                ui.colored_label(
                    egui::Color32::from_rgb(255, 190, 80),
                    format!(
                        "A Font / Mod Root override is active. The selected assets will be edited directly:\n{}\n{}",
                        catalog.spr_path.display(),
                        catalog.texture_path.display()
                    ),
                );
            }
            ui.add_space(6.0);
            ui.horizontal(|ui| {
                ui.label("Palette label");
                ui.add(
                    egui::TextEdit::singleline(&mut state.glyph_builder.label)
                        .desired_width(480.0)
                        .hint_text("Example: (Veteran)"),
                );
            });
            ui.horizontal(|ui| {
                ui.label("Displayed phrase");
                ui.add(
                    egui::TextEdit::singleline(&mut state.glyph_builder.phrase)
                        .desired_width(480.0)
                        .hint_text("Example: (Veteran)"),
                );
            });
            if let Ok(bytes) = editor_to_bytes(&state.glyph_builder.phrase, b"\n") {
                if !bytes.is_empty() {
                    if let Some(texture) = texture {
                        ui.label("Source-font preview");
                        draw_game_preview(ui, &bytes, &catalog, texture, 620.0, 1.2);
                    }
                }
            }
            ui.separator();
            ui.checkbox(
                &mut state.glyph_builder.replacement_confirmed,
                format!(
                    "I understand that this replaces the current 0x{byte:02X} font mapping"
                ),
            );
            ui.horizontal(|ui| {
                let ready = state.glyph_builder.replacement_confirmed
                    && !state.glyph_builder.label.trim().is_empty()
                    && !state.glyph_builder.phrase.trim().is_empty();
                if ui
                    .add_enabled(ready, egui::Button::new("Build Glyph and Save Font Assets"))
                    .clicked()
                {
                    build = true;
                }
                ui.weak("One-time .a2fo-tooltip-lab.bak files are created first.");
            });
        });
    state.glyph_builder.open = open;
    if build {
        Some(GlyphBuildRequest {
            byte: state.glyph_builder.byte.clamp(0x7f, 0xff) as u8,
            label: state.glyph_builder.label.clone(),
            phrase: state.glyph_builder.phrase.clone(),
        })
    } else {
        None
    }
}

fn prepare_odf_import(state: &mut TooltipLabState, paths: Vec<PathBuf>) {
    let Some(document) = state.document.as_ref() else {
        return;
    };
    let mut grouped = BTreeMap::<String, (String, Vec<OdfTooltipReference>)>::new();
    let mut warnings = Vec::new();
    for path in &paths {
        match load_tooltip_references(path) {
            Ok(references) => {
                for reference in references {
                    let normalized = reference.key.to_ascii_lowercase();
                    let group = grouped
                        .entry(normalized)
                        .or_insert_with(|| (reference.key.clone(), Vec::new()));
                    group.1.push(reference);
                }
            }
            Err(error) => warnings.push(format!("{}: {error:#}", path.display())),
        }
    }

    let candidates = grouped
        .into_values()
        .map(|(key, references)| {
            let exists = document
                .entries
                .iter()
                .any(|entry| entry.key.eq_ignore_ascii_case(&key));
            let short_tooltip = references
                .iter()
                .find(|reference| reference.field.eq_ignore_ascii_case("tooltip"))
                .and_then(|reference| reference.unit_name.clone())
                .unwrap_or_default();
            OdfImportCandidate {
                selected: !exists && looks_like_localization_key(&key),
                value: short_tooltip,
                key,
                references,
                exists,
            }
        })
        .collect::<Vec<_>>();

    state.odf_import = OdfImportState {
        open: true,
        loaded_files: paths.len(),
        candidates,
        warnings,
    };
    state.error = None;
}

fn draw_odf_import_window(ctx: &egui::Context, state: &mut TooltipLabState) {
    if !state.odf_import.open {
        return;
    }
    let mut open = state.odf_import.open;
    let mut add_selected = false;
    let mut open_existing = None;
    egui::Window::new("ODF Tooltip Import")
        .open(&mut open)
        .default_width(760.0)
        .default_height(620.0)
        .resizable(true)
        .show(ctx, |ui| {
            ui.label(format!(
                "Found {} unique tooltip reference{} in {} ODF file{}.",
                state.odf_import.candidates.len(),
                if state.odf_import.candidates.len() == 1 {
                    ""
                } else {
                    "s"
                },
                state.odf_import.loaded_files,
                if state.odf_import.loaded_files == 1 { "" } else { "s" }
            ));
            ui.weak(
                "Key-like missing references are selected automatically. Literal tooltip text is shown but left unselected.",
            );
            if !state.odf_import.warnings.is_empty() {
                ui.collapsing(
                    format!("Read warnings ({})", state.odf_import.warnings.len()),
                    |ui| {
                        for warning in &state.odf_import.warnings {
                            ui.colored_label(egui::Color32::YELLOW, warning);
                        }
                    },
                );
            }
            ui.separator();
            egui::ScrollArea::vertical()
                .id_source("odf_import_candidates")
                .max_height(480.0)
                .show(ui, |ui| {
                    for (index, candidate) in
                        state.odf_import.candidates.iter_mut().enumerate()
                    {
                        ui.push_id(index, |ui| {
                            ui.group(|ui| {
                                ui.horizontal(|ui| {
                                    ui.add_enabled(
                                        !candidate.exists,
                                        egui::Checkbox::new(&mut candidate.selected, ""),
                                    );
                                    ui.monospace(&candidate.key);
                                    if candidate.exists {
                                        ui.colored_label(
                                            egui::Color32::from_rgb(105, 210, 145),
                                            "already exists",
                                        );
                                        if ui.small_button("Open entry").clicked() {
                                            open_existing = Some(candidate.key.clone());
                                        }
                                    } else if !looks_like_localization_key(&candidate.key) {
                                        ui.weak("looks like literal ODF text");
                                    }
                                });
                                let sources = candidate
                                    .references
                                    .iter()
                                    .map(|reference| {
                                        format!(
                                            "{}:{} {}",
                                            reference
                                                .path
                                                .file_name()
                                                .and_then(|name| name.to_str())
                                                .unwrap_or("ODF"),
                                            reference.line,
                                            reference.field
                                        )
                                    })
                                    .collect::<Vec<_>>()
                                    .join("; ");
                                ui.small(sources);
                                if !candidate.exists {
                                    ui.add(
                                        egui::TextEdit::multiline(&mut candidate.value)
                                            .desired_rows(2)
                                            .desired_width(f32::INFINITY)
                                            .hint_text("Initial localized text (editable later)"),
                                    );
                                }
                            });
                        });
                    }
                });
            ui.separator();
            ui.horizontal(|ui| {
                let selected = state
                    .odf_import
                    .candidates
                    .iter()
                    .filter(|candidate| candidate.selected && !candidate.exists)
                    .count();
                if ui
                    .add_enabled(
                        selected > 0,
                        egui::Button::new(format!("Add {selected} Missing Entr{}", if selected == 1 { "y" } else { "ies" })),
                    )
                    .clicked()
                {
                    add_selected = true;
                }
                ui.weak("Entries remain unsaved until Save All.");
            });
        });
    state.odf_import.open = open;

    if let Some(key) = open_existing {
        if let Some(index) = state.document.as_ref().and_then(|document| {
            document
                .entries
                .iter()
                .position(|entry| entry.key.eq_ignore_ascii_case(&key))
        }) {
            state.selected_entry = index;
            state.creating = false;
            state.odf_import.open = false;
        }
    }
    if add_selected {
        let additions = state
            .odf_import
            .candidates
            .iter()
            .filter(|candidate| candidate.selected && !candidate.exists)
            .map(|candidate| (candidate.key.clone(), candidate.value.clone()))
            .collect::<Vec<_>>();
        let mut added = 0usize;
        let mut last_index = None;
        for (key, value) in additions {
            match state
                .document
                .as_mut()
                .expect("ODF import requires a document")
                .add_entry(key, value)
            {
                Ok(index) => {
                    added += 1;
                    last_index = Some(index);
                }
                Err(error) => {
                    state.error =
                        Some(format!("ODF import stopped after {added} entries: {error}"));
                    break;
                }
            }
        }
        if let Some(index) = last_index {
            state.selected_entry = index;
            state.creating = false;
            state.filter_dirty = true;
            state.status = format!(
                "Added {added} ODF localization entr{} (not saved).",
                if added == 1 { "y" } else { "ies" }
            );
            state.odf_import.open = false;
        }
    }
}

fn refresh_filter(state: &mut TooltipLabState) {
    if !state.filter_dirty {
        return;
    }
    state.filtered_entries.clear();
    let Some(document) = state.document.as_ref() else {
        state.filter_dirty = false;
        return;
    };
    let needle = state.search.trim().to_lowercase();
    state.filtered_entries.extend(
        document
            .entries
            .iter()
            .enumerate()
            .filter(|(_, entry)| needle.is_empty() || entry.search_blob.contains(&needle))
            .map(|(index, _)| index),
    );
    state.filter_dirty = false;
}

fn draw_entry_list(ui: &mut egui::Ui, state: &mut TooltipLabState) {
    let Some(document) = state.document.as_ref() else {
        ui.heading("Localized strings");
        ui.add_space(8.0);
        ui.weak("Open a Dynamic_Localized_Strings.h file to browse and edit its entries.");
        return;
    };
    ui.heading(
        document
            .path
            .parent()
            .and_then(Path::file_name)
            .and_then(|name| name.to_str())
            .unwrap_or("Localized strings"),
    );
    ui.small(document.path.display().to_string());
    ui.add_space(8.0);
    if ui
        .add(
            egui::TextEdit::singleline(&mut state.search)
                .hint_text("Search keys and tooltip text..."),
        )
        .changed()
    {
        state.filter_dirty = true;
    }
    ui.horizontal(|ui| {
        ui.label(format!("{} shown", state.filtered_entries.len()));
        ui.weak(format!("{} total", document.entries.len()));
        if let Some(limit) = document.max_strings {
            ui.weak(format!("MAX_STRINGS {limit}"));
        }
    });
    ui.separator();

    let rows = state
        .filtered_entries
        .iter()
        .filter_map(|index| {
            document.entries.get(*index).map(|entry| {
                (
                    *index,
                    entry.key.clone(),
                    entry.summary(),
                    entry.dirty(),
                    entry.line,
                )
            })
        })
        .collect::<Vec<_>>();
    let available_height = (ui.available_height() - 165.0).max(120.0);
    egui::ScrollArea::vertical()
        .id_source("localized_entry_scroll")
        .max_height(available_height)
        .auto_shrink([false, false])
        .show_rows(ui, 42.0, rows.len(), |ui, range| {
            for row in &rows[range] {
                let (index, key, summary, dirty, line) = row;
                let marker = if *dirty { "* " } else { "" };
                let label = if summary.is_empty() {
                    format!("{marker}{key}\n    line {line}")
                } else {
                    format!("{marker}{key}\n    {summary}")
                };
                if ui
                    .selectable_label(!state.creating && state.selected_entry == *index, label)
                    .clicked()
                {
                    state.selected_entry = *index;
                    state.creating = false;
                    state.editor_cursor = 0;
                }
            }
        });

    let mut warnings = document.warnings.clone();
    if let Some(catalog) = state.catalog.as_ref() {
        warnings.extend(catalog.warnings.clone());
    }
    if let Some(problem) = state.font_problem.as_ref() {
        warnings.push(format!("Font preview unavailable: {problem}"));
    }
    if !warnings.is_empty() {
        ui.separator();
        ui.collapsing(format!("Warnings ({})", warnings.len()), |ui| {
            for warning in warnings {
                ui.colored_label(egui::Color32::YELLOW, warning);
            }
        });
    }
    if let Some(catalog) = state.catalog.as_ref() {
        ui.separator();
        ui.small(format!("Font: {}", catalog.spr_path.display()));
        ui.small(format!("Atlas: {}", catalog.texture_path.display()));
    }
}

fn draw_editor_and_preview(
    ui: &mut egui::Ui,
    state: &mut TooltipLabState,
    texture: Option<&LoadedTexture>,
) {
    if state.document.is_none() {
        ui.centered_and_justified(|ui| {
            ui.label(
                egui::RichText::new("Open Dynamic_Localized_Strings.h to build a tooltip")
                    .size(22.0)
                    .color(egui::Color32::from_gray(150)),
            );
        });
        return;
    }

    let catalog = state.catalog.clone();
    let editing_new = state.creating;
    let (key, line, dirty) = if editing_new {
        (state.new_key.clone(), 0, true)
    } else {
        let document = state.document.as_ref().expect("checked above");
        let selected = state
            .selected_entry
            .min(document.entries.len().saturating_sub(1));
        let Some(entry) = document.entries.get(selected) else {
            ui.weak("This file does not contain any entries.");
            return;
        };
        (entry.key.clone(), entry.line, entry.dirty())
    };

    ui.horizontal(|ui| {
        if editing_new {
            ui.heading("New localized string");
            ui.weak("The entry will be inserted before the translations table closes.");
        } else {
            ui.heading(&key);
            ui.weak(format!("source line {line}"));
            if dirty {
                ui.colored_label(egui::Color32::from_rgb(255, 205, 80), "modified");
            }
        }
    });
    if editing_new {
        ui.horizontal(|ui| {
            ui.label("Key");
            ui.add(
                egui::TextEdit::singleline(&mut state.new_key)
                    .desired_width(520.0)
                    .hint_text("Example: AUTOTOOLTIP-my_ship.odf"),
            );
            if ui.button("Cancel").clicked() {
                state.creating = false;
            }
        });
    }
    ui.add_space(5.0);

    let mut editor_text = if editing_new {
        state.new_value.clone()
    } else {
        let selected = state.selected_entry;
        state.document.as_ref().expect("checked above").entries[selected]
            .editor_text
            .clone()
    };
    let output = egui::TextEdit::multiline(&mut editor_text)
        .font(egui::TextStyle::Monospace)
        .desired_rows(13)
        .desired_width(f32::INFINITY)
        .hint_text("Type the tooltip here. Click a mod glyph below to insert it at the caret.")
        .show(ui);
    if let Some(cursor) = output.cursor_range {
        state.editor_cursor = cursor.primary.ccursor.index;
    }
    let editor_changed = output.response.changed();

    let mut inserted = None;
    ui.add_space(5.0);
    ui.collapsing("Special characters from this mod", |ui| {
        if let (Some(catalog), Some(texture)) = (catalog.as_ref(), texture) {
            ui.horizontal(|ui| {
                ui.label(format!(
                    "{} insertable glyphs",
                    catalog.special_glyphs().count()
                ));
                ui.weak("Each button uses the opened mod's own font mapping and atlas.");
            });
            egui::ScrollArea::vertical()
                .id_source("mod_glyph_palette")
                .max_height(190.0)
                .show(ui, |ui| {
                    ui.horizontal_wrapped(|ui| {
                        for glyph in catalog.special_glyphs() {
                            if glyph_button(ui, glyph, texture, catalog) {
                                inserted = Some(glyph.byte);
                            }
                        }
                    });
                });
        } else {
            ui.colored_label(
                egui::Color32::YELLOW,
                "No compatible mod font is loaded. Choose the mod root to enable the glyph palette.",
            );
        }
    });

    if let Some(byte) = inserted {
        let label = catalog
            .as_ref()
            .and_then(|font| font.glyph(byte))
            .and_then(|glyph| glyph.label.as_deref());
        let token = glyph_token(byte, label);
        insert_at_character(&mut editor_text, state.editor_cursor, &token);
        state.editor_cursor += token.chars().count();
    }
    if editor_changed || inserted.is_some() {
        if editing_new {
            state.new_value = editor_text.clone();
        } else {
            let selected = state.selected_entry;
            if let Some(entry) = state
                .document
                .as_mut()
                .and_then(|document| document.entries.get_mut(selected))
            {
                entry.editor_text = editor_text.clone();
                entry.refresh_search_blob();
                state.filter_dirty = true;
            }
        }
    }

    let newline = state
        .document
        .as_ref()
        .map(|document| document.newline.clone())
        .unwrap_or_else(|| b"\r\n".to_vec());
    let encoded = editor_to_bytes(&editor_text, &newline);
    ui.horizontal(|ui| {
        ui.weak(format!(
            "{} line{}",
            editor_text.lines().count().max(1),
            if editor_text.lines().count() == 1 {
                ""
            } else {
                "s"
            }
        ));
        if let Ok(bytes) = encoded.as_ref() {
            if let Some(capacity) = state
                .document
                .as_ref()
                .and_then(|document| document.translation_capacity)
            {
                let maximum = capacity.saturating_sub(1);
                let text = format!("{} / {maximum} bytes", bytes.len());
                if bytes.len() > maximum {
                    ui.colored_label(egui::Color32::from_rgb(255, 120, 105), text);
                } else {
                    ui.weak(text);
                }
            } else {
                ui.weak(format!("{} encoded bytes", bytes.len()));
            }
        }
        ui.weak("Tokens count as one byte each.");
    });

    if editing_new {
        ui.horizontal(|ui| {
            if ui.button("Add Entry").clicked() {
                let key = state.new_key.clone();
                let value = editor_text.clone();
                match state
                    .document
                    .as_mut()
                    .expect("checked above")
                    .add_entry(key, value)
                {
                    Ok(index) => {
                        state.selected_entry = index;
                        state.creating = false;
                        state.filter_dirty = true;
                        state.status = "Added a new entry (not saved).".to_string();
                        state.error = None;
                    }
                    Err(error) => state.error = Some(error.to_string()),
                }
            }
            ui.weak("The source file is not changed until Save All.");
        });
    }

    ui.separator();
    ui.horizontal(|ui| {
        ui.heading("In-game font preview");
        ui.add(
            egui::Slider::new(&mut state.preview_width, 280.0..=900.0)
                .text("wrap width")
                .suffix(" px"),
        );
        ui.add(
            egui::Slider::new(&mut state.preview_scale, 0.75..=2.0)
                .text("scale")
                .show_value(false),
        );
    });
    match encoded.as_ref() {
        Ok(bytes) => {
            if let (Some(catalog), Some(texture)) = (catalog.as_ref(), texture) {
                draw_game_preview(
                    ui,
                    bytes,
                    catalog,
                    texture,
                    state.preview_width,
                    state.preview_scale,
                );
            } else {
                ui.group(|ui| {
                    ui.set_min_height(120.0);
                    ui.label(editor_text.as_str());
                });
            }
        }
        Err(error) => {
            ui.colored_label(
                egui::Color32::from_rgb(255, 120, 105),
                format!("Cannot encode this tooltip: {error}"),
            );
        }
    }
}

fn glyph_button(
    ui: &mut egui::Ui,
    glyph: &FontGlyph,
    texture: &LoadedTexture,
    catalog: &FontCatalog,
) -> bool {
    let scale = (150.0 / glyph.width.max(1) as f32).min(1.0);
    let image_size = egui::vec2(
        glyph.width.max(1) as f32 * scale,
        glyph.height.max(1) as f32 * scale,
    );
    let size = egui::vec2((image_size.x + 12.0).max(42.0), image_size.y + 22.0);
    let (rect, response) = ui.allocate_exact_size(size, egui::Sense::click());
    let fill = if response.hovered() {
        egui::Color32::from_rgb(26, 65, 82)
    } else {
        egui::Color32::from_rgb(13, 29, 43)
    };
    ui.painter().rect_filled(rect, 3.0, fill);
    ui.painter().rect_stroke(
        rect,
        3.0,
        egui::Stroke::new(1.0, egui::Color32::from_rgb(42, 83, 102)),
    );
    draw_glyph(
        ui.painter(),
        egui::Rect::from_min_size(
            egui::pos2(rect.center().x - image_size.x * 0.5, rect.top() + 4.0),
            image_size,
        ),
        glyph,
        texture,
        catalog,
    );
    ui.painter().text(
        egui::pos2(rect.center().x, rect.bottom() - 3.0),
        egui::Align2::CENTER_BOTTOM,
        format!("{:02X}", glyph.byte),
        egui::FontId::monospace(10.0),
        egui::Color32::from_gray(165),
    );
    let clicked = response.clicked();
    response.on_hover_text(format!(
        "{}\nByte 0x{:02X} ({})\nClick to insert at the caret",
        glyph.label.as_deref().unwrap_or("Mod-defined glyph"),
        glyph.byte,
        glyph.byte
    ));
    clicked
}

fn draw_game_preview(
    ui: &mut egui::Ui,
    bytes: &[u8],
    catalog: &FontCatalog,
    texture: &LoadedTexture,
    requested_width: f32,
    scale: f32,
) {
    let width = requested_width.min(ui.available_width().max(120.0));
    let line_height = catalog.line_height as f32 * scale;
    let content_width = width - 24.0;
    let (layout, lines) = layout_preview(bytes, catalog, content_width, scale);
    let height = ((lines as f32 * line_height) + 24.0).clamp(110.0, 430.0);
    let (response, painter) = ui.allocate_painter(egui::vec2(width, height), egui::Sense::hover());
    painter.rect_filled(response.rect, 4.0, egui::Color32::from_rgb(2, 7, 12));
    painter.rect_stroke(
        response.rect,
        4.0,
        egui::Stroke::new(1.0, egui::Color32::from_rgb(42, 84, 105)),
    );
    let content = response.rect.shrink(12.0);
    for (byte, x, y) in layout {
        let position = content.min + egui::vec2(x, y * line_height);
        let Some(glyph) = catalog.glyph(byte) else {
            painter.text(
                position,
                egui::Align2::LEFT_TOP,
                format!("{:02X}", byte),
                egui::FontId::monospace(10.0 * scale),
                egui::Color32::RED,
            );
            continue;
        };
        if byte != b' ' && byte != b'\t' {
            draw_glyph(
                &painter,
                egui::Rect::from_min_size(
                    position,
                    egui::vec2(glyph.width as f32 * scale, glyph.height as f32 * scale),
                ),
                glyph,
                texture,
                catalog,
            );
        }
    }
}

fn layout_preview(
    bytes: &[u8],
    catalog: &FontCatalog,
    max_width: f32,
    scale: f32,
) -> (Vec<(u8, f32, f32)>, usize) {
    let mut output = Vec::with_capacity(bytes.len());
    let mut x = 0.0f32;
    let mut line = 0usize;
    let mut index = 0usize;
    while index < bytes.len() {
        let byte = bytes[index];
        if byte == b'\r' {
            index += 1;
            continue;
        }
        if byte == b'\n' {
            x = 0.0;
            line += 1;
            index += 1;
            continue;
        }

        let is_word_start = byte != b' '
            && byte != b'\t'
            && (index == 0 || {
                let previous = bytes[index - 1];
                previous == b' ' || previous == b'\t' || previous == b'\n'
            });
        if is_word_start {
            let word_width = bytes[index..]
                .iter()
                .take_while(|candidate| {
                    **candidate != b' '
                        && **candidate != b'\t'
                        && **candidate != b'\r'
                        && **candidate != b'\n'
                })
                .map(|candidate| preview_advance(*candidate, catalog, scale))
                .sum::<f32>();
            if x > 0.0 && x + word_width > max_width {
                x = 0.0;
                line += 1;
            }
        }

        let advance = preview_advance(byte, catalog, scale);
        if x > 0.0 && x + advance > max_width {
            x = 0.0;
            line += 1;
        }
        output.push((byte, x, line as f32));
        x += advance;
        index += 1;
    }
    (output, line + 1)
}

fn preview_advance(byte: u8, catalog: &FontCatalog, scale: f32) -> f32 {
    if byte == b'\t' {
        catalog
            .glyph(b' ')
            .map_or(20.0, |space| space.advance * 4.0)
            * scale
    } else {
        catalog.glyph(byte).map_or(12.0, |glyph| glyph.advance) * scale
    }
}

fn draw_glyph(
    painter: &egui::Painter,
    target: egui::Rect,
    glyph: &FontGlyph,
    texture: &LoadedTexture,
    catalog: &FontCatalog,
) {
    let atlas_width = catalog.reference_width.max(1) as f32;
    let atlas_height = catalog.reference_height.max(1) as f32;
    let actual_width = texture.width.max(1.0);
    let actual_height = texture.height.max(1.0);
    let x_scale = actual_width / atlas_width;
    let y_scale = actual_height / atlas_height;
    let uv = egui::Rect::from_min_max(
        egui::pos2(
            glyph.x as f32 * x_scale / actual_width,
            glyph.y as f32 * y_scale / actual_height,
        ),
        egui::pos2(
            (glyph.x + glyph.width) as f32 * x_scale / actual_width,
            (glyph.y + glyph.height) as f32 * y_scale / actual_height,
        ),
    );
    painter.image(texture.texture_id, target, uv, egui::Color32::WHITE);
}

fn insert_at_character(text: &mut String, character_index: usize, insertion: &str) {
    let byte_index = text
        .char_indices()
        .nth(character_index)
        .map(|(index, _)| index)
        .unwrap_or(text.len());
    text.insert_str(byte_index, insertion);
}

fn selected_key(state: &TooltipLabState) -> Option<String> {
    state
        .document
        .as_ref()?
        .entries
        .get(state.selected_entry)
        .map(|entry| entry.key.clone())
}

fn save_document(state: &mut TooltipLabState, requests: &mut EventWriter<LoadDocumentRequest>) {
    let Some(document) = state.document.as_ref() else {
        return;
    };
    if !document.dirty() {
        state.status = "There are no changed entries to save.".to_string();
        return;
    }
    let path = document.path.clone();
    let key = selected_key(state);
    match document.save() {
        Ok(backup) => {
            let message = if let Some(backup) = backup {
                format!(
                    "Saved localized strings; original backup: {}",
                    backup.display()
                )
            } else {
                "Saved localized strings; the existing original backup was retained.".to_string()
            };
            requests.send(LoadDocumentRequest {
                path,
                override_root: state.font_root_override.clone(),
                success_message: Some(message),
                select_key: key,
            });
        }
        Err(error) => state.error = Some(format!("Save failed: {error:#}")),
    }
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

fn decode_texture(path: &Path) -> Result<Image> {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn inserts_at_a_unicode_character_boundary() {
        let mut text = "A•B".to_string();
        insert_at_character(&mut text, 2, "[[81:Red 1]]");
        assert_eq!(text, "A•[[81:Red 1]]B");
    }

    #[test]
    fn preview_layout_wraps_before_a_word() {
        let catalog = FontCatalog {
            spr_path: PathBuf::new(),
            texture_path: PathBuf::new(),
            reference_width: 1,
            reference_height: 1,
            line_height: 10,
            glyphs: (b' '..=b'~')
                .map(|byte| {
                    (
                        byte,
                        FontGlyph {
                            byte,
                            x: 0,
                            y: 0,
                            width: 5,
                            advance: 5.0,
                            height: 10,
                            label: None,
                        },
                    )
                })
                .collect(),
            roots: Vec::new(),
            warnings: Vec::new(),
        };
        let (layout, lines) = layout_preview(b"one two", &catalog, 24.0, 1.0);
        assert_eq!(lines, 2);
        assert_eq!(layout[4].1, 0.0);
        assert_eq!(layout[4].2, 1.0);
    }
}
