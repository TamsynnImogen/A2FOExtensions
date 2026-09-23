#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]

mod app;
mod extensions;
mod model;
mod sprite;

use anyhow::{Context, Result};
use app::ArmadaUiEditorPlugin;
use bevy::prelude::*;
use model::{discover_game_root, GameMode, UiDocument};
use sprite::{collect_sprite_assets, faction_sprite_for_cfg, SpriteIndex, TextureIndex};
use std::collections::HashSet;
use std::path::{Path, PathBuf};

#[derive(Debug)]
struct Arguments {
    mode: GameMode,
    inspect: bool,
    root: Option<PathBuf>,
    cfg: Option<PathBuf>,
}

fn main() {
    let arguments = match parse_arguments() {
        Ok(Some(arguments)) => arguments,
        Ok(None) => return,
        Err(error) => {
            eprintln!("Armada UI Editor: {error:#}");
            std::process::exit(2);
        }
    };
    if arguments.inspect {
        if let Err(error) = inspect(&arguments) {
            eprintln!("Armada UI Editor inspection failed: {error:#}");
            std::process::exit(1);
        }
        return;
    }

    App::new()
        .add_plugins(DefaultPlugins.set(WindowPlugin {
            primary_window: Some(Window {
                title: "Armada UI Editor".to_string(),
                resolution: (1500.0, 900.0).into(),
                resizable: true,
                ..default()
            }),
            ..default()
        }))
        .add_plugins(ArmadaUiEditorPlugin::new(
            arguments.mode,
            arguments.root,
            arguments.cfg,
        ))
        .run();
}

fn parse_arguments() -> Result<Option<Arguments>> {
    let mut mode = GameMode::Armada2;
    let mut inspect = false;
    let mut root = None;
    let mut cfg = None;
    let mut args = std::env::args_os().skip(1);
    while let Some(argument) = args.next() {
        let text = argument.to_string_lossy();
        match text.as_ref() {
            "--help" | "-h" => {
                print_help();
                return Ok(None);
            }
            "--a1" => mode = GameMode::Armada1,
            "--a2" => mode = GameMode::Armada2,
            "--inspect" => inspect = true,
            "--root" => {
                let Some(value) = args.next() else {
                    anyhow::bail!("--root requires a game or mod directory");
                };
                root = Some(PathBuf::from(value));
            }
            value if value.starts_with('-') => anyhow::bail!("unknown option: {value}"),
            _ if cfg.is_none() => cfg = Some(PathBuf::from(argument)),
            _ => anyhow::bail!("only one GUI CFG may be opened at startup"),
        }
    }
    Ok(Some(Arguments {
        mode,
        inspect,
        root,
        cfg,
    }))
}

fn print_help() {
    println!(
        "Armada UI Editor\n\n\
         Usage:\n  armada_ui_editor [--a1|--a2] [--root GAME_DIR] [gui.cfg]\n  \
         armada_ui_editor --inspect [--a1|--a2] [--root GAME_DIR] [gui.cfg]\n\n\
         Visually edit Armada CFG interface rectangles with SPR/TGA previews.\n\
         A2 mode is the default. Without a CFG, the stock Federation HUD is loaded."
    );
}

fn inspect(arguments: &Arguments) -> Result<()> {
    let fallback_root = arguments
        .root
        .clone()
        .unwrap_or_else(|| arguments.mode.default_root());
    let cfg = arguments
        .cfg
        .clone()
        .unwrap_or_else(|| arguments.mode.default_cfg(&fallback_root));
    let root = arguments
        .root
        .clone()
        .unwrap_or_else(|| discover_game_root(&cfg, &fallback_root));
    let document = UiDocument::load(arguments.mode, &root, &cfg)
        .with_context(|| format!("Load {} layout {}", arguments.mode.label(), cfg.display()))?;
    let faction_sprite = faction_sprite_for_cfg(&document.primary_path);
    let sprites = SpriteIndex::load(&document.game_root, faction_sprite.as_deref())?;
    let textures = TextureIndex::build(&document.game_root);
    let assets = collect_sprite_assets(&document, &sprites, &textures);

    println!("Mode: {}", document.mode.label());
    println!("Game root: {}", document.game_root.display());
    println!("Primary CFG: {}", document.primary_path.display());
    println!(
        "Canvas: {} x {}",
        document.screen_width, document.screen_height
    );
    println!("CFG sources: {}", document.source_count());
    println!("Effective values: {}", document.values().len());
    println!("Editable rectangles: {}", document.rectangles.len());
    println!("Editable colours: {}", document.colors.len());
    println!("Groups: {}", document.groups().join(", "));
    println!(
        "Faction SPR: {}",
        faction_sprite.as_deref().unwrap_or("(global only)")
    );
    println!("Loaded SPR files: {}", sprites.loaded_files().len());
    println!("Sprite entries: {}", sprites.len());
    println!(
        "Fleet Operations system backgrounds: {}",
        sprites.system_background_names().len()
    );
    println!("Indexed textures: {}", textures.len());
    println!("Resolved preview pieces: {}", assets.len());
    if let Some(sample) = assets.first() {
        println!(
            "Preview sample: {} from {}:{} (reference {:?}x{:?})",
            sample.entry.name,
            sample.entry.source_path.display(),
            sample.entry.line,
            sample.entry.reference_width,
            sample.entry.reference_height
        );
    }
    let unique_textures = assets
        .iter()
        .map(|asset| asset.texture_path.clone())
        .collect::<HashSet<_>>();
    for path in &unique_textures {
        app::decode_texture(path)
            .with_context(|| format!("Decode preview texture {}", path.display()))?;
    }
    println!("Decoded preview textures: {}", unique_textures.len());
    for warning in &document.warnings {
        println!("WARNING: {warning}");
    }
    validate_expected_install(arguments.mode, &document.game_root);
    Ok(())
}

fn validate_expected_install(mode: GameMode, root: &Path) {
    let expected = mode.default_root();
    if root == expected {
        println!("Stock preset: matched");
    }
}
