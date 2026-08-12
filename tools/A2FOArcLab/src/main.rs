mod app;
mod fire_arc;
mod odf;
mod sod;
mod window_icon;

use anyhow::{bail, Context, Result};
use app::ArcLabPlugin;
use bevy::prelude::*;
use std::path::{Path, PathBuf};

fn main() {
    let arguments = std::env::args_os().skip(1).collect::<Vec<_>>();
    if arguments
        .first()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value == "--help" || value == "-h")
    {
        print_usage();
        return;
    }
    if arguments
        .first()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value == "--inspect")
    {
        let Some(path) = arguments.get(1).map(PathBuf::from) else {
            eprintln!("--inspect requires a ship ODF path\n");
            print_usage();
            std::process::exit(2);
        };
        if let Err(error) = inspect_project(&path) {
            eprintln!("A2FO Arc Lab inspection failed: {error:#}");
            std::process::exit(1);
        }
        return;
    }
    let initial_ship = arguments.first().map(PathBuf::from);
    App::new()
        .add_plugins(DefaultPlugins.set(WindowPlugin {
            primary_window: Some(Window {
                title: "A2FO Arc Lab".to_string(),
                resolution: (1440.0, 900.0).into(),
                resizable: true,
                ..default()
            }),
            ..default()
        }))
        .add_plugins(ArcLabPlugin::new(initial_ship))
        .add_systems(Startup, window_icon::install_window_icon)
        .run();
}

fn print_usage() {
    println!(
        "A2FO Arc Lab\n\n\
         Usage:\n  \
           a2fo_arclab [ship.odf]\n  \
           a2fo_arclab --inspect ship.odf\n\n\
         With no path, use Open Ship ODF in the application. --inspect checks\n\
         ODF/include/model resolution without opening a window."
    );
}

fn inspect_project(path: &Path) -> Result<()> {
    if !path.is_file() {
        bail!("Ship ODF does not exist: {}", path.display());
    }
    let project = odf::load_project(path).context("Resolve ship project")?;
    println!("Ship: {}", project.ship_name);
    println!("ODF: {}", project.ship_path.display());
    println!("Model command: {}.sod", project.model_name);
    for root in &project.resources.roots {
        println!("Root: {} = {}", root.label, root.path.display());
    }
    if let Some(model_path) = project.model_path.as_ref() {
        let summary = sod::inspect_sod(model_path).context("Parse resolved SOD")?;
        println!("SOD: {}", model_path.display());
        println!(
            "SOD content: {} nodes, {} meshes, {} numeric hardpoints, {} base texture(s)",
            summary.node_names.len(),
            summary.mesh_count,
            summary.hardpoint_count,
            summary.texture_names.len()
        );
        let texture_roots = project.resources.texture_roots();
        for (name, resolved) in sod::resolve_texture_names(&summary.texture_names, &texture_roots) {
            if let Some(path) = resolved {
                match sod::validate_texture(&path) {
                    Ok(()) => println!("Texture: {name} -> {} (decoded)", path.display()),
                    Err(error) => println!(
                        "WARNING: texture '{name}' resolved to {} but did not decode: {error:#}",
                        path.display()
                    ),
                }
            } else {
                println!("WARNING: texture '{name}' did not resolve");
            }
        }
        for weapon in &project.weapons {
            for hardpoint in &weapon.hardpoints {
                if !summary
                    .node_names
                    .iter()
                    .any(|name| name.eq_ignore_ascii_case(hardpoint))
                {
                    println!(
                        "WARNING: weapon{} hardpoint '{}' is absent from the SOD",
                        weapon.slot, hardpoint
                    );
                }
            }
        }
    } else {
        println!("WARNING: the model did not resolve automatically");
    }
    println!("Weapons: {}", project.weapons.len());
    for weapon in &project.weapons {
        println!(
            "  weapon{}: {} ({}.odf), hardpoints: {}",
            weapon.slot,
            weapon.display_name,
            weapon.odf_name,
            if weapon.hardpoints.is_empty() {
                "(none)".to_string()
            } else {
                weapon.hardpoints.join(", ")
            }
        );
        if let Some(config) = weapon.arc.config {
            for line in config.snippet().lines() {
                println!("    {line}");
            }
        } else if weapon.arc.errors.is_empty() {
            println!("    native fire-arc behaviour");
        } else {
            for error in &weapon.arc.errors {
                println!("    ERROR: {error}");
            }
        }
        for warning in &weapon.warnings {
            println!("    WARNING: {warning}");
        }
    }
    for warning in &project.warnings {
        println!("WARNING: {warning}");
    }
    Ok(())
}
