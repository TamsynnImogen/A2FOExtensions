use crate::odf::{
    decode_windows_text, find_case_insensitive_relative, parse_assignment, strip_line_comment,
    tokenize_value, ResolvedOdf, ResourceContext,
};
use crate::sprite::{SpriteEntry, SpriteIndex, SpriteRect};
use anyhow::{Context, Result};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};

pub const MAX_WEAPON_ICON_SLOT: u32 = 128;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct IconPosition {
    pub x: i32,
    pub y: i32,
}

impl IconPosition {
    pub fn clamped(self) -> Self {
        Self {
            x: self.x.clamp(0, 100),
            y: self.y.clamp(0, 100),
        }
    }
}

#[derive(Debug, Clone)]
pub struct SpriteAsset {
    pub entry: SpriteEntry,
    pub texture_path: PathBuf,
}

#[derive(Debug, Clone)]
pub struct PanelPiece {
    pub sprite: SpriteAsset,
    pub target: SpriteRect,
}

#[derive(Debug, Clone)]
pub struct PanelPreview {
    pub width: i32,
    pub height: i32,
    pub pieces: Vec<PanelPiece>,
}

#[derive(Debug, Clone)]
pub struct WeaponIcon {
    pub slot: u32,
    pub odf_name: String,
    pub display_name: String,
    pub position: Option<IconPosition>,
    pub original_position: Option<IconPosition>,
    pub position_source: Option<(PathBuf, usize)>,
    pub sprite: Option<SpriteAsset>,
}

impl WeaponIcon {
    pub fn dirty(&self) -> bool {
        self.position != self.original_position
    }
}

#[derive(Debug)]
pub struct IconProject {
    pub ship_path: PathBuf,
    pub ship_name: String,
    pub race: String,
    pub model_name: String,
    pub system_sprite_key: String,
    pub system_background: SpriteAsset,
    pub interface_cfg: Option<String>,
    pub interface_sprites: Option<String>,
    pub system_icon_rect: Option<SpriteRect>,
    pub panel: Option<PanelPreview>,
    pub weapons: Vec<WeaponIcon>,
    pub resources: ResourceContext,
    pub warnings: Vec<String>,
}

impl IconProject {
    pub fn dirty(&self) -> bool {
        self.weapons.iter().any(WeaponIcon::dirty)
    }
}

pub fn load_project(ship_path: &Path, override_root: Option<&Path>) -> Result<IconProject> {
    let resources = ResourceContext::discover(ship_path, override_root)?;
    let ship = resources
        .resolve(ship_path)
        .with_context(|| format!("Resolve ship ODF {}", ship_path.display()))?;
    let ship_name = ship
        .string("unitName")
        .or_else(|| ship.string("tooltip"))
        .unwrap_or_else(|| file_stem(ship_path));
    let race = ship.string("race").unwrap_or_else(|| "unknown".to_string());
    let model_name = ship
        .string("baseName")
        .filter(|value| !value.trim().is_empty())
        .map(|value| file_stem(Path::new(&value)))
        .unwrap_or_else(|| file_stem(ship_path));

    let race_odf = resolve_race_odf(&resources, &race);
    let interface_cfg = race_odf
        .as_ref()
        .and_then(|race| race.string("interfaceConfiguration"));
    let interface_sprites = race_odf
        .as_ref()
        .and_then(|race| race.string("interfaceSprites"));
    let sprite_index = SpriteIndex::load(&resources, interface_sprites.as_deref())?;

    let mut background_candidates = Vec::new();
    if let Some(explicit) = ship.string("systemBackground") {
        push_candidate(&mut background_candidates, explicit.clone());
        if !explicit.to_ascii_lowercase().ends_with("_si") {
            push_candidate(&mut background_candidates, format!("{explicit}_si"));
        }
    }
    push_candidate(&mut background_candidates, format!("{model_name}_si"));
    push_candidate(
        &mut background_candidates,
        format!("{}_si", file_stem(ship_path)),
    );

    let (system_sprite_key, system_entry) = background_candidates
        .iter()
        .find_map(|key| {
            sprite_index
                .get(key)
                .cloned()
                .map(|entry| (key.clone(), entry))
        })
        .with_context(|| {
            format!(
                "No system-background sprite matched {}",
                background_candidates.join(", ")
            )
        })?;
    let system_background = resolve_sprite_asset(&resources, system_entry)
        .with_context(|| format!("Resolve texture for system sprite '{system_sprite_key}'"))?;

    let slot_names = weapon_slot_names(&ship);

    let default_icon = sprite_index.get("systemicon_default").cloned();
    let mut weapons = Vec::new();
    for (slot, odf_name) in slot_names {
        let weapon_odf = resources
            .resolve_odf_name(&odf_name)
            .and_then(|path| resources.resolve(&path).ok());
        let display_name = weapon_odf
            .as_ref()
            .and_then(|weapon| {
                weapon
                    .string("wpnName")
                    .or_else(|| weapon.string("tooltip"))
            })
            .unwrap_or_else(|| odf_name.clone());
        let position_value = ship.value(&format!("weapon{slot}iconpos"));
        let position = position_value.and_then(parse_position);
        let icon_entry = sprite_index
            .get(&format!("i_{odf_name}"))
            .cloned()
            .or_else(|| default_icon.clone());
        let sprite = icon_entry.and_then(|entry| resolve_sprite_asset(&resources, entry).ok());
        weapons.push(WeaponIcon {
            slot,
            odf_name,
            display_name,
            position,
            original_position: position,
            position_source: position_value.map(|value| (value.source.clone(), value.line)),
            sprite,
        });
    }

    let interface_values = interface_cfg
        .as_deref()
        .and_then(|name| load_interface_cfg(&resources, name).ok())
        .flatten();
    let system_icon_rect = interface_values
        .as_ref()
        .and_then(configured_system_icon_rect);
    let panel = interface_values
        .as_ref()
        .and_then(|values| build_panel_preview(&resources, &sprite_index, values).ok())
        .flatten();
    let mut warnings = ship.warnings.clone();
    if weapons.is_empty() {
        warnings.push(format!(
            "No weapon1..weapon{MAX_WEAPON_ICON_SLOT} assignments were found"
        ));
    }
    if race_odf.is_none() {
        warnings.push(format!(
            "Faction definition for race '{race}' was not found"
        ));
    }
    if interface_cfg.is_some() && panel.is_none() {
        warnings.push(
            "Faction information panel could not be assembled; showing the system background alone"
                .to_string(),
        );
    }
    if interface_cfg.is_some() && system_icon_rect.is_none() {
        warnings.push(
            "infoSingleSystemsIcon was not found in the faction CFG; weapon icons use their SPR sizes as a fallback"
                .to_string(),
        );
    }
    if sprite_index.loaded_files().is_empty() {
        warnings.push("No sprite tables were found in the resolved asset roots".to_string());
    }

    Ok(IconProject {
        ship_path: ship_path.to_path_buf(),
        ship_name,
        race,
        model_name,
        system_sprite_key,
        system_background,
        interface_cfg,
        interface_sprites,
        system_icon_rect,
        panel,
        weapons,
        resources,
        warnings,
    })
}

fn weapon_slot_names(ship: &ResolvedOdf) -> Vec<(u32, String)> {
    let mut slot_names = ship
        .values
        .iter()
        .filter_map(|(key, value)| {
            numeric_suffix(key, "weapon")
                .filter(|slot| (1..=MAX_WEAPON_ICON_SLOT).contains(slot))
                .zip(value.tokens.first().cloned())
        })
        .filter(|(_, name)| !name.eq_ignore_ascii_case("null") && !name.trim().is_empty())
        .collect::<Vec<_>>();
    slot_names.sort_by_key(|(slot, _)| *slot);
    slot_names.dedup_by_key(|(slot, _)| *slot);
    slot_names
}

fn resolve_race_odf(resources: &ResourceContext, ship_race: &str) -> Option<ResolvedOdf> {
    let races_path = resources.resolve_odf_name("races")?;
    let races = resources.resolve(&races_path).ok()?;
    let mut entries = races
        .values
        .iter()
        .filter_map(|(key, value)| numeric_suffix(key, "race").zip(value.tokens.first().cloned()))
        .collect::<Vec<_>>();
    entries.sort_by_key(|(slot, _)| *slot);
    let mut candidates = Vec::new();
    for (_, name) in entries {
        let Some(path) = resources.resolve_odf_name(&name) else {
            continue;
        };
        let Ok(candidate) = resources.resolve(&path) else {
            continue;
        };
        let candidate_name = candidate.string("name").unwrap_or_else(|| file_stem(&path));
        candidates.push((candidate_name, candidate));
    }
    // Some NPC definitions deliberately reuse a playable faction's broad
    // alias (for example Observer uses "federation"). The engine race string
    // is exact, so prefer an exact name before applying legacy aliases.
    if let Some((_, candidate)) = candidates.iter().find(|(name, _)| {
        name.trim_matches(['"', '\''])
            .eq_ignore_ascii_case(ship_race.trim_matches(['"', '\'']))
    }) {
        return Some(candidate.clone());
    }
    if let Some((_, candidate)) = candidates
        .iter()
        .find(|(name, _)| normalize_race(name) == normalize_race(ship_race))
    {
        return Some(candidate.clone());
    }

    for fallback in race_file_candidates(ship_race) {
        if let Some(path) = resources.resolve_odf_name(fallback) {
            if let Ok(candidate) = resources.resolve(&path) {
                return Some(candidate);
            }
        }
    }
    None
}

fn race_file_candidates(race: &str) -> &'static [&'static str] {
    match normalize_race(race).as_str() {
        "federation" => &["federation", "fed"],
        "klingon" => &["klingon", "kling"],
        "romulan" => &["romulan", "rom"],
        "borg" => &["borg"],
        "cardassian" => &["cardassian", "card"],
        "species8472" => &["species8472"],
        "dominion" => &["dominion"],
        _ => &[],
    }
}

fn normalize_race(value: &str) -> String {
    match value
        .trim()
        .trim_matches(['"', '\''])
        .to_ascii_lowercase()
        .as_str()
    {
        "unitedfederation" | "federation" | "fed" => "federation".to_string(),
        "klingonempire" | "klingon" | "kling" | "kli" => "klingon".to_string(),
        "romulanempire" | "romulan" | "rom" => "romulan".to_string(),
        "borg" | "bor" => "borg".to_string(),
        "cardassian" | "card" | "car" => "cardassian".to_string(),
        "species_8472" | "8472" | "species8472" => "species8472".to_string(),
        other => other.to_string(),
    }
}

fn resolve_sprite_asset(resources: &ResourceContext, entry: SpriteEntry) -> Result<SpriteAsset> {
    let texture_path = resources
        .resolve_texture(&entry.texture)
        .with_context(|| format!("Texture '{}' was not found", entry.texture))?;
    Ok(SpriteAsset {
        entry,
        texture_path,
    })
}

fn parse_position(value: &crate::odf::ResolvedValue) -> Option<IconPosition> {
    let x = value
        .tokens
        .first()?
        .trim_end_matches(['f', 'F'])
        .parse::<f32>()
        .ok()?;
    let y = value
        .tokens
        .get(1)?
        .trim_end_matches(['f', 'F'])
        .parse::<f32>()
        .ok()?;
    if !x.is_finite() || !y.is_finite() {
        return None;
    }
    Some(
        IconPosition {
            x: x.round() as i32,
            y: y.round() as i32,
        }
        .clamped(),
    )
}

fn build_panel_preview(
    resources: &ResourceContext,
    sprites: &SpriteIndex,
    values: &HashMap<String, String>,
) -> Result<Option<PanelPreview>> {
    let panel_name = first_token(values.get("infobackgroundpanel"))
        .unwrap_or_else(|| "infoBackgroundPanel".to_string());
    let panel_key = panel_name.to_ascii_lowercase();
    let mut targets = values
        .iter()
        .filter_map(|(key, value)| {
            numeric_suffix(key, &format!("{panel_key}_")).zip(parse_rect(value))
        })
        .collect::<Vec<_>>();
    targets.sort_by_key(|(index, _)| *index);
    let mut pieces = Vec::new();
    for (index, target) in targets {
        let Some(entry) = sprites.get(&format!("{panel_name}.{index}")).cloned() else {
            continue;
        };
        let Ok(sprite) = resolve_sprite_asset(resources, entry) else {
            continue;
        };
        pieces.push(PanelPiece { sprite, target });
    }
    if pieces.is_empty() {
        return Ok(None);
    }

    let bounds_width = pieces
        .iter()
        .map(|piece| piece.target.x + piece.target.width)
        .max()
        .unwrap_or(0);
    let bounds_height = pieces
        .iter()
        .map(|piece| piece.target.y + piece.target.height)
        .max()
        .unwrap_or(0);
    let configured_size = ["infopanelarea_2", "infopanelarea_1", "infopanelarea"]
        .iter()
        .find_map(|key| values.get(*key).and_then(|value| parse_rect(value)))
        .filter(|rect| rect.width > 0 && rect.height > 0);
    Ok(Some(PanelPreview {
        width: configured_size
            .map(|rect| rect.width)
            .unwrap_or(bounds_width)
            .max(bounds_width),
        height: configured_size
            .map(|rect| rect.height)
            .unwrap_or(bounds_height)
            .max(bounds_height),
        pieces,
    }))
}

fn load_interface_cfg(
    resources: &ResourceContext,
    cfg_name: &str,
) -> Result<Option<HashMap<String, String>>> {
    let Some(cfg_path) = resources.resolve_named_file("misc", cfg_name) else {
        return Ok(None);
    };
    let mut values = HashMap::new();
    load_gui_cfg(&cfg_path, &mut values, &mut HashSet::new())?;
    Ok(Some(values))
}

fn configured_system_icon_rect(values: &HashMap<String, String>) -> Option<SpriteRect> {
    values
        .get("infosinglesystemsicon")
        .and_then(|value| parse_rect(value))
        .filter(|rect| rect.width > 0 && rect.height > 0)
}

fn load_gui_cfg(
    path: &Path,
    values: &mut HashMap<String, String>,
    loaded: &mut HashSet<String>,
) -> Result<()> {
    let canonical = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
    let identity = canonical
        .to_string_lossy()
        .replace('\\', "/")
        .to_ascii_lowercase();
    if !loaded.insert(identity) {
        return Ok(());
    }
    let contents = decode_windows_text(&fs::read(&canonical)?);
    let mut assignments = Vec::new();
    for raw in contents.lines() {
        let line = strip_line_comment(raw).trim();
        if line.is_empty() {
            continue;
        }
        if line.len() >= 8 && line[..8].eq_ignore_ascii_case("#include") {
            if let Some(name) = tokenize_value(&line[8..]).into_iter().next() {
                if let Some(include) = canonical.parent().and_then(|directory| {
                    find_case_insensitive_relative(directory, Path::new(&name))
                }) {
                    load_gui_cfg(&include, values, loaded)?;
                }
            }
        } else if let Some((key, value)) = parse_assignment(line) {
            assignments.push((key.to_ascii_lowercase(), value));
        }
    }
    for (key, value) in assignments {
        values.insert(key, value);
    }
    Ok(())
}

fn parse_rect(value: &str) -> Option<SpriteRect> {
    let values = tokenize_value(value)
        .into_iter()
        .take(4)
        .map(|value| value.parse::<i32>().ok())
        .collect::<Option<Vec<_>>>()?;
    (values.len() == 4).then(|| SpriteRect {
        x: values[0],
        y: values[1],
        width: values[2],
        height: values[3],
    })
}

fn first_token(value: Option<&String>) -> Option<String> {
    tokenize_value(value?).into_iter().next()
}

fn push_candidate(output: &mut Vec<String>, value: String) {
    let value = value.trim().trim_matches(['"', '\'']).to_string();
    if !value.is_empty() && !output.iter().any(|item| item.eq_ignore_ascii_case(&value)) {
        output.push(value);
    }
}

fn numeric_suffix(key: &str, prefix: &str) -> Option<u32> {
    let suffix = key.strip_prefix(&prefix.to_ascii_lowercase())?;
    if suffix.is_empty() || !suffix.chars().all(|ch| ch.is_ascii_digit()) {
        return None;
    }
    suffix.parse().ok()
}

fn file_stem(path: &Path) -> String {
    path.file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("unknown")
        .to_string()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::odf::ResolvedValue;

    fn insert_weapon(ship: &mut ResolvedOdf, slot: u32, value: &str) {
        ship.values.insert(
            format!("weapon{slot}"),
            ResolvedValue {
                tokens: vec![value.to_string()],
                source: PathBuf::from("ship.odf"),
                line: slot as usize,
            },
        );
    }

    #[test]
    fn extended_weapon_slot_boundary_is_one_through_128() {
        let mut ship = ResolvedOdf::default();
        for (slot, name) in [
            (0, "zero"),
            (1, "one"),
            (32, "stock_boundary"),
            (33, "extended_first"),
            (128, "extended_last"),
            (129, "too_high"),
        ] {
            insert_weapon(&mut ship, slot, name);
        }
        insert_weapon(&mut ship, 64, "null");

        assert_eq!(
            weapon_slot_names(&ship),
            vec![
                (1, "one".to_string()),
                (32, "stock_boundary".to_string()),
                (33, "extended_first".to_string()),
                (128, "extended_last".to_string()),
            ]
        );
    }

    #[test]
    fn system_icon_size_comes_from_the_faction_cfg_rectangle() {
        let values =
            HashMap::from([("infosinglesystemsicon".to_string(), "7 9 24 18".to_string())]);

        assert_eq!(
            configured_system_icon_rect(&values),
            Some(SpriteRect {
                x: 7,
                y: 9,
                width: 24,
                height: 18,
            })
        );
    }
}
