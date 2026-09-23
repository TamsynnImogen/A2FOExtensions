use crate::odf::{decode_windows_text, find_case_insensitive_relative, ResourceContext};
use anyhow::{Context, Result};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};

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

#[derive(Debug, Clone, Default)]
pub struct SpriteIndex {
    entries: HashMap<String, SpriteEntry>,
    loaded_files: Vec<PathBuf>,
}

#[derive(Debug, Clone, Default)]
struct SpriteState {
    reference_width: Option<i32>,
    reference_height: Option<i32>,
}

impl SpriteIndex {
    pub fn load(resources: &ResourceContext, faction_sprite: Option<&str>) -> Result<Self> {
        let mut output = Self::default();
        let mut loaded = HashSet::new();

        // Load low-priority roots first. Later active-mod entries replace them.
        for root in resources.roots.iter().rev() {
            let Some(sprite_dir) = find_case_insensitive_relative(&root.path, Path::new("Sprites"))
            else {
                continue;
            };
            for name in [
                "sprites.spr",
                "gui_global.spr",
                "systembackgrounds.spr",
                "systemimages.spr",
            ] {
                if let Some(path) = find_case_insensitive_relative(&sprite_dir, Path::new(name)) {
                    output.load_with_includes(&path, &mut loaded)?;
                }
            }
            if let Some(name) = faction_sprite {
                if let Some(path) = find_case_insensitive_relative(&sprite_dir, Path::new(name)) {
                    output.load_with_includes(&path, &mut loaded)?;
                }
            }
        }
        Ok(output)
    }

    pub fn get(&self, name: &str) -> Option<&SpriteEntry> {
        self.entries.get(&name.to_ascii_lowercase())
    }

    pub fn loaded_files(&self) -> &[PathBuf] {
        &self.loaded_files
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
            .with_context(|| format!("Read sprite file {}", canonical.display()))?;
        let contents = decode_windows_text(&bytes);
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
            if let Some(path) = canonical.parent().and_then(|directory| {
                find_case_insensitive_relative(directory, Path::new(&include))
            }) {
                self.load_with_includes(&path, loaded)?;
            }
        }
        for entry in entries {
            self.entries.insert(entry.name.to_ascii_lowercase(), entry);
        }
        Ok(())
    }
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn directives_and_sprite_rects_parse() {
        let mut state = SpriteState::default();
        parse_directive("@referenceWidth=1024", &mut state);
        parse_directive("@referenceHeight=256", &mut state);
        assert_eq!(state.reference_width, Some(1024));
        assert_eq!(state.reference_height, Some(256));
    }
}
