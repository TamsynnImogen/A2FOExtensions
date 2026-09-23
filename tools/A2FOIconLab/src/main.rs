#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]

mod app;
mod odf;
mod project;
mod save;
mod sprite;

use anyhow::{bail, Context, Result};
use app::IconLabPlugin;
use bevy::prelude::*;
use std::collections::HashSet;
use std::path::{Path, PathBuf};

fn main() {
    let arguments = std::env::args_os().skip(1).collect::<Vec<_>>();
    if arguments
        .first()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value == "--help" || value == "-h")
    {
        println!(
            "A2FO Icon Lab\n\nUsage:\n  a2fo_iconlab [ship.odf]\n  \
             a2fo_iconlab --inspect ship.odf\n\n\
             Open a ship ODF, select a weapon from the list or preview, place\n\
             its icon on the system background, then save weaponXiconpos."
        );
        return;
    }
    if arguments
        .first()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value == "--inspect")
    {
        let Some(path) = arguments.get(1).map(PathBuf::from) else {
            eprintln!("--inspect requires a ship ODF path");
            std::process::exit(2);
        };
        if let Err(error) = inspect_project(&path) {
            eprintln!("A2FO Icon Lab inspection failed: {error:#}");
            std::process::exit(1);
        }
        return;
    }

    let initial_ship = arguments.first().map(PathBuf::from);
    App::new()
        .add_plugins(DefaultPlugins.set(WindowPlugin {
            primary_window: Some(Window {
                title: "A2FO Icon Lab".to_string(),
                resolution: (1360.0, 760.0).into(),
                resizable: true,
                ..default()
            }),
            ..default()
        }))
        .add_plugins(IconLabPlugin::new(initial_ship))
        .run();
}

fn inspect_project(path: &Path) -> Result<()> {
    if !path.is_file() {
        bail!("Ship ODF does not exist: {}", path.display());
    }
    let project = project::load_project(path, None).context("Resolve icon project")?;
    println!("Ship: {}", project.ship_name);
    println!("ODF: {}", project.ship_path.display());
    println!("Race: {}", project.race);
    println!("Model/UI key: {}", project.model_name);
    println!("System sprite: {}", project.system_sprite_key);
    println!(
        "  SPR: {}:{}",
        project.system_background.entry.source_path.display(),
        project.system_background.entry.line
    );
    println!(
        "  texture: {} [{} {} {} {}] (reference {:?} x {:?})",
        project.system_background.texture_path.display(),
        project.system_background.entry.rect.x,
        project.system_background.entry.rect.y,
        project.system_background.entry.rect.width,
        project.system_background.entry.rect.height,
        project.system_background.entry.reference_width,
        project.system_background.entry.reference_height,
    );
    println!(
        "Faction UI: CFG {}, SPR {}, panel {}",
        project.interface_cfg.as_deref().unwrap_or("(not found)"),
        project
            .interface_sprites
            .as_deref()
            .unwrap_or("(not found)"),
        project
            .panel
            .as_ref()
            .map(|panel| format!(
                "{}x{}, {} piece(s)",
                panel.width,
                panel.height,
                panel.pieces.len()
            ))
            .unwrap_or_else(|| "(not resolved)".to_string())
    );
    println!(
        "Weapon icon size: {}",
        project
            .system_icon_rect
            .map(|rect| format!("{}x{} (infoSingleSystemsIcon)", rect.width, rect.height))
            .unwrap_or_else(|| "SPR fallback".to_string())
    );
    for root in &project.resources.roots {
        println!("Root: {} = {}", root.label, root.path.display());
    }
    let mut texture_paths = HashSet::new();
    texture_paths.insert(project.system_background.texture_path.clone());
    if let Some(panel) = &project.panel {
        texture_paths.extend(
            panel
                .pieces
                .iter()
                .map(|piece| piece.sprite.texture_path.clone()),
        );
    }
    texture_paths.extend(
        project
            .weapons
            .iter()
            .filter_map(|weapon| weapon.sprite.as_ref())
            .map(|sprite| sprite.texture_path.clone()),
    );
    let mut texture_paths = texture_paths.into_iter().collect::<Vec<_>>();
    texture_paths.sort();
    for texture_path in texture_paths {
        let image = app::decode_texture(&texture_path)
            .with_context(|| format!("Decode texture {}", texture_path.display()))?;
        println!(
            "Decoded texture: {} ({}x{})",
            texture_path.display(),
            image.texture_descriptor.size.width,
            image.texture_descriptor.size.height
        );
    }
    println!("Weapons: {}", project.weapons.len());
    for weapon in &project.weapons {
        let position = weapon
            .position
            .map(|position| format!("{} {}", position.x, position.y))
            .unwrap_or_else(|| "(not placed)".to_string());
        println!(
            "  weapon{}: {} ({}) @ {}, icon {}",
            weapon.slot,
            weapon.display_name,
            weapon.odf_name,
            position,
            weapon
                .sprite
                .as_ref()
                .map(|sprite| sprite.entry.name.as_str())
                .unwrap_or("(missing)")
        );
    }
    for warning in &project.warnings {
        println!("WARNING: {warning}");
    }
    Ok(())
}
