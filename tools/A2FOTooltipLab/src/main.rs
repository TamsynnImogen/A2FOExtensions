#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]

mod app;
mod document;
mod glyph_builder;
mod glyphs;
mod odf;

use anyhow::{bail, Context, Result};
use app::TooltipLabPlugin;
use bevy::prelude::*;
use std::path::{Path, PathBuf};

fn main() {
    let arguments = std::env::args_os().skip(1).collect::<Vec<_>>();
    if arguments
        .first()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value == "--help" || value == "-h")
    {
        println!(
            "A2FO Tooltip Lab\n\nUsage:\n  a2fo_tooltiplab [Dynamic_Localized_Strings.h]\n  \
             a2fo_tooltiplab --inspect Dynamic_Localized_Strings.h\n\n\
             Edit tooltip relays, import missing keys from ODFs, insert or build\n\
             the opened mod's real font glyphs, preview them, validate the file,\n\
             and save with backups."
        );
        return;
    }
    if arguments
        .first()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value == "--inspect")
    {
        let Some(path) = arguments.get(1).map(PathBuf::from) else {
            eprintln!("--inspect requires a Dynamic_Localized_Strings.h path");
            std::process::exit(2);
        };
        if let Err(error) = inspect(&path) {
            eprintln!("A2FO Tooltip Lab inspection failed: {error:#}");
            std::process::exit(1);
        }
        return;
    }

    let initial_document = arguments.first().map(PathBuf::from);
    App::new()
        .add_plugins(DefaultPlugins.set(WindowPlugin {
            primary_window: Some(Window {
                title: "A2FO Tooltip Lab".to_string(),
                resolution: (1500.0, 900.0).into(),
                resizable: true,
                ..default()
            }),
            ..default()
        }))
        .add_plugins(TooltipLabPlugin::new(initial_document))
        .run();
}

fn inspect(path: &Path) -> Result<()> {
    if !path.is_file() {
        bail!("Localized strings file does not exist: {}", path.display());
    }
    let document = document::TooltipDocument::load(path).context("Parse localized strings")?;
    let catalog = glyphs::FontCatalog::resolve(path, None).context("Resolve mod font")?;
    println!("Document: {}", document.path.display());
    println!("Entries: {}", document.entries.len());
    println!(
        "MAX_STRINGS: {}",
        document
            .max_strings
            .map(|value| value.to_string())
            .unwrap_or_else(|| "not declared".to_string())
    );
    println!(
        "String capacities: key {}, translation {}",
        document
            .key_capacity
            .map(|value| value.to_string())
            .unwrap_or_else(|| "not declared".to_string()),
        document
            .translation_capacity
            .map(|value| value.to_string())
            .unwrap_or_else(|| "not declared".to_string())
    );
    println!("Font SPR: {}", catalog.spr_path.display());
    println!("Font texture: {}", catalog.texture_path.display());
    println!(
        "Font reference: {}x{}, line height {}",
        catalog.reference_width, catalog.reference_height, catalog.line_height
    );
    println!(
        "Insertable mod glyphs: {}",
        catalog.special_glyphs().count()
    );
    for glyph in catalog.special_glyphs() {
        println!(
            "  0x{:02X}: {} [{} {} {}x{}]",
            glyph.byte,
            glyph.label.as_deref().unwrap_or("mod glyph"),
            glyph.x,
            glyph.y,
            glyph.width,
            glyph.height
        );
    }
    for warning in document.warnings.iter().chain(catalog.warnings.iter()) {
        println!("WARNING: {warning}");
    }
    Ok(())
}
