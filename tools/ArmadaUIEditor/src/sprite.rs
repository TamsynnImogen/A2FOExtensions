use crate::model::{first_token, split_numeric_suffix, UiDocument, UiRect};
use anyhow::{Context, Result};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SpriteRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

#[derive(Debug, Clone)]
pub struct SpriteEntry {
    pub name: String,
    pub texture: String,
    pub rect: SpriteRect,
    pub reference_width: Option<i32>,
    pub reference_height: Option<i32>,
    pub source_path: PathBuf,
    pub line: usize,
}

#[derive(Debug, Clone)]
pub struct SpriteAsset {
    pub entry: SpriteEntry,
    pub texture_path: PathBuf,
    pub target: UiRect,
    pub group: Option<String>,
}

#[derive(Debug, Clone, Default)]
struct SpriteState {
    reference_width: Option<i32>,
    reference_height: Option<i32>,
}

#[derive(Debug, Clone, Default)]
pub struct SpriteIndex {
    entries: HashMap<String, SpriteEntry>,
    loaded_files: Vec<PathBuf>,
}

impl SpriteIndex {
    pub fn load(game_root: &Path, faction_sprite: Option<&str>) -> Result<Self> {
        let Some(sprite_dir) = find_case_insensitive_child(game_root, "Sprites") else {
            return Ok(Self::default());
        };
        let mut output = Self::default();
        let mut loaded = HashSet::new();
        for name in [
            "sprites.spr",
            "gui_global.spr",
            "gui_map.spr",
            "systembackgrounds.spr",
            "systemimages.spr",
        ] {
            if let Some(path) = find_case_insensitive_child(&sprite_dir, name) {
                output.load_with_includes(&path, &mut loaded)?;
            }
        }
        if let Some(name) = faction_sprite {
            if let Some(path) = find_case_insensitive_child(&sprite_dir, name) {
                output.load_with_includes(&path, &mut loaded)?;
            }
        }
        Ok(output)
    }

    pub fn get(&self, name: &str) -> Option<&SpriteEntry> {
        self.entries.get(&name.to_ascii_lowercase())
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn loaded_files(&self) -> &[PathBuf] {
        &self.loaded_files
    }

    pub fn system_background_names(&self) -> Vec<String> {
        let mut names = self
            .entries
            .values()
            .filter(|entry| {
                entry.name.to_ascii_lowercase().ends_with("_si")
                    && entry
                        .source_path
                        .file_name()
                        .and_then(|name| name.to_str())
                        .is_some_and(|name| name.eq_ignore_ascii_case("systembackgrounds.spr"))
            })
            .map(|entry| entry.name.clone())
            .collect::<Vec<_>>();
        names.sort_by_key(|name| name.to_ascii_lowercase());
        names
    }

    pub fn resolve_asset(
        &self,
        name: &str,
        textures: &TextureIndex,
        target: UiRect,
        group: Option<String>,
    ) -> Option<SpriteAsset> {
        let entry = self.get(name)?.clone();
        let texture_path = textures.resolve(&entry.texture)?.to_path_buf();
        Some(SpriteAsset {
            entry,
            texture_path,
            target,
            group,
        })
    }

    fn load_with_includes(&mut self, path: &Path, loaded: &mut HashSet<String>) -> Result<()> {
        let canonical = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
        let identity = canonical
            .to_string_lossy()
            .replace('\\', "/")
            .to_ascii_lowercase();
        if !loaded.insert(identity) {
            return Ok(());
        }
        let bytes = fs::read(&canonical)
            .with_context(|| format!("Read sprite table {}", canonical.display()))?;
        let contents = String::from_utf8_lossy(&bytes);
        self.loaded_files.push(canonical.clone());

        let mut state = SpriteState::default();
        let mut entries = Vec::new();
        let mut includes = Vec::new();
        for (line_index, raw_line) in contents.lines().enumerate() {
            let line = strip_comment(raw_line).trim();
            if line.is_empty() || line.eq_ignore_ascii_case("sprite_table") {
                continue;
            }
            if line.len() >= 8 && line[..8].eq_ignore_ascii_case("@include") {
                let rest = line[8..].trim();
                let name = rest
                    .strip_prefix('=')
                    .unwrap_or(rest)
                    .trim()
                    .trim_matches(['"', '\'']);
                if !name.is_empty() {
                    includes.push(name.to_string());
                }
                continue;
            }
            if line.starts_with('@') {
                parse_directive(line, &mut state);
                continue;
            }
            let parts = line.split_whitespace().collect::<Vec<_>>();
            if parts.len() < 6 {
                continue;
            }
            let (Ok(x), Ok(y), Ok(width), Ok(height)) = (
                parts[2].parse::<i32>(),
                parts[3].parse::<i32>(),
                parts[4].parse::<i32>(),
                parts[5].parse::<i32>(),
            ) else {
                continue;
            };
            entries.push(SpriteEntry {
                name: parts[0].to_string(),
                texture: parts[1].to_string(),
                rect: SpriteRect {
                    x,
                    y,
                    width,
                    height,
                },
                reference_width: state.reference_width,
                reference_height: state.reference_height,
                source_path: canonical.clone(),
                line: line_index + 1,
            });
        }

        for include in includes {
            if let Some(path) = canonical
                .parent()
                .and_then(|directory| find_case_insensitive_child(directory, &include))
            {
                self.load_with_includes(&path, loaded)?;
            }
        }
        for entry in entries {
            self.entries.insert(entry.name.to_ascii_lowercase(), entry);
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Default)]
pub struct TextureIndex {
    entries: HashMap<String, PathBuf>,
}

impl TextureIndex {
    pub fn build(game_root: &Path) -> Self {
        let Some(textures) = find_case_insensitive_child(game_root, "Textures") else {
            return Self::default();
        };
        let mut roots = Vec::new();
        for name in ["RGB", "Index8", "Compressed"] {
            if let Some(path) = find_case_insensitive_child(&textures, name) {
                roots.push(path);
            }
        }
        roots.push(textures);

        let mut candidates = Vec::new();
        for (priority, root) in roots.iter().enumerate() {
            for entry in WalkDir::new(root)
                .max_depth(3)
                .follow_links(false)
                .into_iter()
                .flatten()
                .filter(|entry| entry.file_type().is_file())
            {
                let path = entry.path().to_path_buf();
                let extension_rank = match path
                    .extension()
                    .and_then(|value| value.to_str())
                    .unwrap_or("")
                    .to_ascii_lowercase()
                    .as_str()
                {
                    "tga" => 0,
                    "png" => 1,
                    "dds" => 2,
                    "bmp" => 3,
                    "jpg" | "jpeg" => 4,
                    _ => 9,
                };
                candidates.push((priority, extension_rank, path));
            }
        }
        candidates.sort_by_key(|(priority, extension, path)| {
            (
                *priority,
                *extension,
                path.to_string_lossy().to_ascii_lowercase(),
            )
        });

        let mut output = Self::default();
        for (_, _, path) in candidates {
            if let Some(name) = path.file_name().and_then(|value| value.to_str()) {
                output
                    .entries
                    .entry(name.to_ascii_lowercase())
                    .or_insert_with(|| path.clone());
            }
            if let Some(stem) = path.file_stem().and_then(|value| value.to_str()) {
                output
                    .entries
                    .entry(stem.to_ascii_lowercase())
                    .or_insert(path);
            }
        }
        output
    }

    pub fn resolve(&self, texture: &str) -> Option<&Path> {
        let normalized = Path::new(texture)
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or(texture)
            .to_ascii_lowercase();
        self.entries
            .get(&normalized)
            .or_else(|| {
                Path::new(&normalized)
                    .file_stem()
                    .and_then(|value| value.to_str())
                    .and_then(|stem| self.entries.get(stem))
            })
            .map(PathBuf::as_path)
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }
}

pub fn faction_sprite_for_cfg(path: &Path) -> Option<String> {
    let stem = path.file_stem()?.to_string_lossy().to_ascii_lowercase();
    let name = match stem.as_str() {
        value if value.contains("fed") => "gui_federation.spr",
        value if value.contains("kli") => "gui_klingon.spr",
        value if value.contains("rom") => "gui_romulan.spr",
        value if value.contains("bor") => "gui_borg.spr",
        value if value.contains("card") => "gui_cardassian.spr",
        value if value.contains("8472") => "gui_species8472.spr",
        value if value.contains("dom") => "gui_dominion.spr",
        _ => return None,
    };
    Some(name.to_string())
}

pub fn collect_sprite_assets(
    document: &UiDocument,
    sprites: &SpriteIndex,
    textures: &TextureIndex,
) -> Vec<SpriteAsset> {
    let mut output = Vec::new();
    for (index, rectangle) in document.rectangles.iter().enumerate() {
        let Some(target) = document.absolute_rect(index) else {
            continue;
        };
        let lower = rectangle.key.to_ascii_lowercase();
        let sprite = if let Some((base, suffix)) = split_numeric_suffix(&lower) {
            document
                .value(base)
                .and_then(first_token)
                .and_then(|name| sprites.get(&format!("{name}.{suffix}")))
        } else if let Some(base) = lower.strip_suffix("area") {
            [
                format!("{base}image"),
                format!("{base}background"),
                format!("{base}sprite"),
                base.to_string(),
            ]
            .into_iter()
            .find_map(|key| {
                document
                    .value(&key)
                    .and_then(first_token)
                    .and_then(|name| sprites.get(&name))
            })
        } else {
            None
        };
        let Some(entry) = sprite.cloned() else {
            continue;
        };
        let Some(texture_path) = textures.resolve(&entry.texture) else {
            continue;
        };
        output.push(SpriteAsset {
            entry,
            texture_path: texture_path.to_path_buf(),
            target,
            group: document.group_for(index).map(str::to_string),
        });
    }
    output.sort_by_key(|asset| (asset.target.width * asset.target.height).abs());
    output.reverse();
    output
}

fn parse_directive(line: &str, state: &mut SpriteState) {
    let Some((key, value)) = line.split_once('=') else {
        return;
    };
    let key = key.trim().trim_start_matches('@').to_ascii_lowercase();
    let value = value.trim();
    match key.as_str() {
        "reference" => {
            if let Ok(size) = value.parse::<i32>() {
                state.reference_width = Some(size);
                state.reference_height = Some(size);
            }
        }
        "referencewidth" => state.reference_width = value.parse().ok(),
        "referenceheight" => state.reference_height = value.parse().ok(),
        _ => {}
    }
}

fn strip_comment(line: &str) -> &str {
    let hash = line.find('#');
    let slash = line.find("//");
    match (hash, slash) {
        (Some(left), Some(right)) => &line[..left.min(right)],
        (Some(index), None) | (None, Some(index)) => &line[..index],
        (None, None) => line,
    }
}

fn find_case_insensitive_child(directory: &Path, wanted: &str) -> Option<PathBuf> {
    fs::read_dir(directory).ok()?.flatten().find_map(|entry| {
        entry
            .file_name()
            .to_string_lossy()
            .eq_ignore_ascii_case(wanted)
            .then(|| entry.path())
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn faction_sprite_mapping_covers_stock_games() {
        assert_eq!(
            faction_sprite_for_cfg(Path::new("misc/gui_fed.cfg")).as_deref(),
            Some("gui_federation.spr")
        );
        assert_eq!(
            faction_sprite_for_cfg(Path::new("misc/gui_species8472.cfg")).as_deref(),
            Some("gui_species8472.spr")
        );
    }
}
