use crate::extensions;
use anyhow::{bail, Context, Result};
use std::collections::{HashMap, HashSet};
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};

pub const DEFAULT_A1_ROOT: &str = "/home/tamsynn/Games/Heroic/Star Trek Armada";
pub const DEFAULT_A2_ROOT: &str = "/home/tamsynn/Games/Heroic/Star Trek Armada II";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GameMode {
    Armada1,
    Armada2,
}

impl GameMode {
    pub fn label(self) -> &'static str {
        match self {
            Self::Armada1 => "Armada I",
            Self::Armada2 => "Armada II",
        }
    }

    pub fn short_label(self) -> &'static str {
        match self {
            Self::Armada1 => "A1",
            Self::Armada2 => "A2",
        }
    }

    pub fn default_root(self) -> PathBuf {
        match self {
            Self::Armada1 => PathBuf::from(DEFAULT_A1_ROOT),
            Self::Armada2 => PathBuf::from(DEFAULT_A2_ROOT),
        }
    }

    pub fn default_cfg(self, root: &Path) -> PathBuf {
        let misc = root.join("misc");
        let candidates: &[&str] = match self {
            Self::Armada1 => &["gui_fed.cfg", "gui_federation.cfg"],
            Self::Armada2 => &["gui_fed.cfg", "gui_federation.cfg"],
        };
        candidates
            .iter()
            .find_map(|name| find_case_insensitive_child(&misc, name))
            .unwrap_or_else(|| misc.join(candidates[0]))
    }

    pub fn default_canvas(self) -> (i32, i32) {
        match self {
            Self::Armada1 => (640, 480),
            Self::Armada2 => (1600, 1200),
        }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct UiRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

impl UiRect {
    pub fn translated(self, x: i32, y: i32) -> Self {
        Self {
            x: self.x + x,
            y: self.y + y,
            ..self
        }
    }

    pub fn right(self) -> i32 {
        self.x + self.width
    }

    pub fn bottom(self) -> i32 {
        self.y + self.height
    }
}

#[derive(Debug, Clone)]
pub struct UiRectangle {
    pub key: String,
    pub rect: UiRect,
    pub original: UiRect,
    pub source: PathBuf,
    pub line: usize,
    pub added: bool,
}

impl UiRectangle {
    pub fn dirty(&self) -> bool {
        self.added || self.rect != self.original
    }
}

#[derive(Debug, Clone)]
pub struct UiColor {
    pub key: String,
    pub value: [f32; 3],
    pub original: [f32; 3],
    pub source: PathBuf,
    pub line: usize,
    pub added: bool,
}

impl UiColor {
    pub fn dirty(&self) -> bool {
        self.added || self.value != self.original
    }
}

#[derive(Debug, Clone)]
pub struct ResolvedValue {
    pub key: String,
    pub value: String,
    pub source: PathBuf,
    pub line: usize,
}

#[derive(Debug, Clone)]
struct SourceFile {
    text: String,
    line_ending: &'static str,
}

#[derive(Debug, Clone)]
pub struct UiDocument {
    pub mode: GameMode,
    pub game_root: PathBuf,
    pub primary_path: PathBuf,
    pub screen_width: i32,
    pub screen_height: i32,
    pub rectangles: Vec<UiRectangle>,
    pub colors: Vec<UiColor>,
    pub warnings: Vec<String>,
    values: HashMap<String, ResolvedValue>,
    sources: HashMap<PathBuf, SourceFile>,
    root_indices: HashMap<String, usize>,
    root_members: HashSet<usize>,
    groups: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct SaveReport {
    pub files: Vec<PathBuf>,
    pub backups: Vec<PathBuf>,
    pub rectangle_count: usize,
    pub color_count: usize,
}

impl UiDocument {
    pub fn load(mode: GameMode, game_root: &Path, primary_path: &Path) -> Result<Self> {
        if !primary_path.is_file() {
            bail!("GUI CFG does not exist: {}", primary_path.display());
        }
        let primary_path = primary_path
            .canonicalize()
            .unwrap_or_else(|_| primary_path.to_path_buf());
        let game_root = game_root
            .canonicalize()
            .unwrap_or_else(|_| game_root.to_path_buf());

        let mut ordered = Vec::new();
        let mut sources = HashMap::new();
        let mut loaded = HashSet::new();
        let mut warnings = Vec::new();
        load_cfg_recursive(
            &primary_path,
            &mut ordered,
            &mut sources,
            &mut loaded,
            &mut warnings,
            0,
        )?;

        let mut values = HashMap::new();
        for value in ordered {
            values.insert(value.key.to_ascii_lowercase(), value);
        }

        let (default_width, default_height) = mode.default_canvas();
        let screen_width = values
            .get("screenwidth")
            .and_then(|value| first_integer(&value.value))
            .filter(|value| *value > 0)
            .unwrap_or(default_width);
        let screen_height = values
            .get("screenheight")
            .and_then(|value| first_integer(&value.value))
            .filter(|value| *value > 0)
            .unwrap_or(default_height);

        let mut rectangles = values
            .values()
            .filter_map(|value| {
                let rect = parse_rect(&value.value)?;
                looks_like_rectangle(&value.key, rect).then(|| UiRectangle {
                    key: value.key.clone(),
                    rect,
                    original: rect,
                    source: value.source.clone(),
                    line: value.line,
                    added: false,
                })
            })
            .collect::<Vec<_>>();
        rectangles.sort_by(|left, right| {
            left.key
                .to_ascii_lowercase()
                .cmp(&right.key.to_ascii_lowercase())
        });

        let mut colors = values
            .values()
            .filter_map(|value| {
                let color = parse_color(&value.value)?;
                looks_like_color(&value.key).then(|| UiColor {
                    key: value.key.clone(),
                    value: color,
                    original: color,
                    source: value.source.clone(),
                    line: value.line,
                    added: false,
                })
            })
            .collect::<Vec<_>>();
        colors.sort_by(|left, right| {
            left.key
                .to_ascii_lowercase()
                .cmp(&right.key.to_ascii_lowercase())
        });

        let (root_indices, root_members, groups) = build_roots(&rectangles);
        Ok(Self {
            mode,
            game_root,
            primary_path,
            screen_width,
            screen_height,
            rectangles,
            colors,
            warnings,
            values,
            sources,
            root_indices,
            root_members,
            groups,
        })
    }

    pub fn source_count(&self) -> usize {
        self.sources.len()
    }

    pub fn values(&self) -> &HashMap<String, ResolvedValue> {
        &self.values
    }

    pub fn value(&self, key: &str) -> Option<&str> {
        self.values
            .get(&key.to_ascii_lowercase())
            .map(|value| value.value.as_str())
    }

    pub fn groups(&self) -> &[String] {
        &self.groups
    }

    pub fn dirty_count(&self) -> usize {
        self.dirty_rectangle_count() + self.dirty_color_count()
    }

    pub fn dirty_rectangle_count(&self) -> usize {
        self.rectangles.iter().filter(|rect| rect.dirty()).count()
    }

    pub fn dirty_color_count(&self) -> usize {
        self.colors.iter().filter(|color| color.dirty()).count()
    }

    pub fn rectangle_index(&self, key: &str) -> Option<usize> {
        self.rectangles
            .iter()
            .position(|entry| entry.key.eq_ignore_ascii_case(key))
    }

    pub fn color_index(&self, key: &str) -> Option<usize> {
        self.colors
            .iter()
            .position(|entry| entry.key.eq_ignore_ascii_case(key))
    }

    pub fn color(&self, key: &str) -> Option<[f32; 3]> {
        let index = self.color_index(key)?;
        self.colors.get(index).map(|entry| entry.value)
    }

    pub fn add_rectangle(&mut self, key: &str, rect: UiRect) -> usize {
        if let Some(index) = self.rectangle_index(key) {
            return index;
        }
        self.rectangles.push(UiRectangle {
            key: key.to_string(),
            rect,
            original: rect,
            source: self.primary_path.clone(),
            line: 0,
            added: true,
        });
        self.rectangles.sort_by(|left, right| {
            left.key
                .to_ascii_lowercase()
                .cmp(&right.key.to_ascii_lowercase())
        });
        self.rebuild_groups();
        self.rectangle_index(key)
            .expect("new rectangle remains present after sorting")
    }

    pub fn add_color(&mut self, key: &str, value: [f32; 3]) -> usize {
        if let Some(index) = self.color_index(key) {
            return index;
        }
        self.colors.push(UiColor {
            key: key.to_string(),
            value,
            original: value,
            source: self.primary_path.clone(),
            line: 0,
            added: true,
        });
        self.colors.sort_by(|left, right| {
            left.key
                .to_ascii_lowercase()
                .cmp(&right.key.to_ascii_lowercase())
        });
        self.color_index(key)
            .expect("new color remains present after sorting")
    }

    pub fn set_color(&mut self, index: usize, value: [f32; 3]) -> bool {
        let Some(entry) = self.colors.get_mut(index) else {
            return false;
        };
        let value = value.map(|channel| channel.clamp(0.0, 1.0));
        if entry.value == value {
            return false;
        }
        entry.value = value;
        true
    }

    pub fn remove_added_rectangle(&mut self, index: usize) -> bool {
        if !self.rectangles.get(index).is_some_and(|entry| entry.added) {
            return false;
        }
        self.rectangles.remove(index);
        self.rebuild_groups();
        true
    }

    pub fn remove_added_color(&mut self, index: usize) -> bool {
        if !self.colors.get(index).is_some_and(|entry| entry.added) {
            return false;
        }
        self.colors.remove(index);
        true
    }

    pub fn group_for(&self, index: usize) -> Option<&str> {
        let key = self.rectangles.get(index)?.key.to_ascii_lowercase();
        self.groups
            .iter()
            .filter(|prefix| key.starts_with(prefix.as_str()))
            .max_by_key(|prefix| prefix.len())
            .map(String::as_str)
    }

    pub fn root_rect(&self, group: &str) -> Option<UiRect> {
        let index = *self.root_indices.get(&group.to_ascii_lowercase())?;
        self.rectangles.get(index).map(|entry| entry.rect)
    }

    pub fn absolute_rect(&self, index: usize) -> Option<UiRect> {
        let entry = self.rectangles.get(index)?;
        if self.root_members.contains(&index) {
            return Some(entry.rect);
        }
        let Some(group) = self.group_for(index) else {
            return Some(entry.rect);
        };
        let Some(root) = self.root_rect(group) else {
            return Some(entry.rect);
        };
        Some(entry.rect.translated(root.x, root.y))
    }

    pub fn set_rect(&mut self, index: usize, rect: UiRect) -> bool {
        let Some(entry) = self.rectangles.get_mut(index) else {
            return false;
        };
        if entry.rect == rect {
            return false;
        }
        entry.rect = rect;
        true
    }

    pub fn reset_all(&mut self) {
        self.rectangles.retain(|entry| !entry.added);
        for entry in &mut self.rectangles {
            entry.rect = entry.original;
        }
        self.colors.retain(|entry| !entry.added);
        for entry in &mut self.colors {
            entry.value = entry.original;
        }
        self.rebuild_groups();
    }

    fn rebuild_groups(&mut self) {
        let (root_indices, root_members, groups) = build_roots(&self.rectangles);
        self.root_indices = root_indices;
        self.root_members = root_members;
        self.groups = groups;
    }

    pub fn save(&mut self) -> Result<SaveReport> {
        let dirty_rectangles = self
            .rectangles
            .iter()
            .filter(|entry| entry.dirty())
            .map(|entry| {
                let value = format!(
                    "{}\t{}\t{}\t{}",
                    entry.rect.x, entry.rect.y, entry.rect.width, entry.rect.height
                );
                (
                    entry.source.clone(),
                    entry.line,
                    entry.key.clone(),
                    value,
                    entry.added,
                )
            })
            .collect::<Vec<_>>();
        let dirty_colors = self
            .colors
            .iter()
            .filter(|entry| entry.dirty())
            .map(|entry| {
                (
                    entry.source.clone(),
                    entry.line,
                    entry.key.clone(),
                    format_color(entry.value),
                    entry.added,
                )
            })
            .collect::<Vec<_>>();
        if dirty_rectangles.is_empty() && dirty_colors.is_empty() {
            return Ok(SaveReport {
                files: Vec::new(),
                backups: Vec::new(),
                rectangle_count: 0,
                color_count: 0,
            });
        }
        let rectangle_count = dirty_rectangles.len();
        let color_count = dirty_colors.len();

        let mut by_source: HashMap<PathBuf, Vec<(usize, String, String)>> = HashMap::new();
        let mut additions = Vec::new();
        for (source, line, key, value, added) in dirty_rectangles.iter().chain(dirty_colors.iter())
        {
            if *added {
                additions.push((key.clone(), value.clone()));
            } else {
                by_source.entry(source.clone()).or_default().push((
                    *line,
                    key.clone(),
                    value.clone(),
                ));
            }
        }

        let mut rewritten = HashMap::new();
        for (path, edits) in by_source {
            let source = self
                .sources
                .get(&path)
                .with_context(|| format!("No loaded source for {}", path.display()))?;
            let updated = apply_value_edits(&source.text, source.line_ending, &edits)
                .with_context(|| format!("Patch GUI CFG {}", path.display()))?;
            rewritten.insert(path, updated);
        }
        if !additions.is_empty() {
            let source = self
                .sources
                .get(&self.primary_path)
                .with_context(|| format!("No loaded source for {}", self.primary_path.display()))?;
            let current = rewritten
                .remove(&self.primary_path)
                .unwrap_or_else(|| source.text.clone());
            rewritten.insert(
                self.primary_path.clone(),
                append_extension_properties(&current, source.line_ending, &additions),
            );
        }

        let mut files = Vec::new();
        let mut backups = Vec::new();
        for (path, updated) in rewritten {
            let backup = appended_path(&path, ".armada-ui-editor.bak");
            if !backup.exists() {
                fs::copy(&path, &backup)
                    .with_context(|| format!("Create backup {}", backup.display()))?;
                backups.push(backup);
            }

            let temporary = appended_path(&path, ".armada-ui-editor.tmp");
            fs::write(&temporary, updated.as_bytes())
                .with_context(|| format!("Write temporary CFG {}", temporary.display()))?;
            if let Ok(metadata) = fs::metadata(&path) {
                let _ = fs::set_permissions(&temporary, metadata.permissions());
            }
            fs::rename(&temporary, &path)
                .with_context(|| format!("Install updated CFG {}", path.display()))?;
            files.push(path);
        }
        files.sort();
        backups.sort();

        let mode = self.mode;
        let game_root = self.game_root.clone();
        let primary_path = self.primary_path.clone();
        *self = Self::load(mode, &game_root, &primary_path)?;

        Ok(SaveReport {
            rectangle_count,
            color_count,
            files,
            backups,
        })
    }
}

pub fn discover_game_root(path: &Path, fallback: &Path) -> PathBuf {
    let start = if path.is_dir() {
        path
    } else {
        path.parent().unwrap_or(path)
    };
    for ancestor in start.ancestors() {
        if find_case_insensitive_child(ancestor, "misc").is_some()
            && find_case_insensitive_child(ancestor, "Sprites").is_some()
            && find_case_insensitive_child(ancestor, "Textures").is_some()
        {
            return ancestor.to_path_buf();
        }
    }
    fallback.to_path_buf()
}

pub fn first_token(value: &str) -> Option<String> {
    tokenize_value(value).into_iter().next()
}

pub fn split_numeric_suffix(value: &str) -> Option<(&str, usize)> {
    let (base, suffix) = value.rsplit_once('_')?;
    if suffix.is_empty() || !suffix.chars().all(|ch| ch.is_ascii_digit()) {
        return None;
    }
    Some((base, suffix.parse().ok()?))
}

fn load_cfg_recursive(
    path: &Path,
    ordered: &mut Vec<ResolvedValue>,
    sources: &mut HashMap<PathBuf, SourceFile>,
    loaded: &mut HashSet<String>,
    warnings: &mut Vec<String>,
    depth: usize,
) -> Result<()> {
    if depth > 64 {
        bail!("GUI CFG include depth exceeded 64 at {}", path.display());
    }
    let canonical = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
    let identity = canonical
        .to_string_lossy()
        .replace('\\', "/")
        .to_ascii_lowercase();
    if !loaded.insert(identity) {
        return Ok(());
    }

    let bytes =
        fs::read(&canonical).with_context(|| format!("Read GUI CFG {}", canonical.display()))?;
    let text = normalize_crlf_text(&String::from_utf8_lossy(&bytes));
    let line_ending = "\r\n";
    sources.insert(
        canonical.clone(),
        SourceFile {
            text: text.clone(),
            line_ending,
        },
    );

    for (line_index, raw_line) in text.lines().enumerate() {
        let clean = strip_line_comment(raw_line).trim();
        if clean.is_empty() {
            continue;
        }
        if clean.len() >= 8 && clean[..8].eq_ignore_ascii_case("#include") {
            if let Some(include) = first_token(&clean[8..]) {
                let include_path = canonical.parent().and_then(|directory| {
                    find_case_insensitive_relative(directory, Path::new(&include))
                });
                if let Some(include_path) = include_path {
                    load_cfg_recursive(
                        &include_path,
                        ordered,
                        sources,
                        loaded,
                        warnings,
                        depth + 1,
                    )?;
                } else {
                    warnings.push(format!(
                        "{}:{}: include '{}' was not found",
                        canonical.display(),
                        line_index + 1,
                        include
                    ));
                }
            }
            continue;
        }
        if let Some((key, value)) = parse_assignment(clean) {
            ordered.push(ResolvedValue {
                key,
                value,
                source: canonical.clone(),
                line: line_index + 1,
            });
        }
    }
    Ok(())
}

fn build_roots(
    rectangles: &[UiRectangle],
) -> (HashMap<String, usize>, HashSet<usize>, Vec<String>) {
    let mut candidates: HashMap<String, Vec<(usize, u8)>> = HashMap::new();
    let mut root_members = HashSet::new();
    for (index, entry) in rectangles.iter().enumerate() {
        let lower = entry.key.to_ascii_lowercase();
        let (base, suffix) = split_numeric_suffix(&lower)
            .map(|(base, suffix)| (base, Some(suffix)))
            .unwrap_or((lower.as_str(), None));
        if !base.ends_with("panelarea")
            || base.contains("backgroundpanelarea")
            || base.contains("minimizedpanelarea")
        {
            continue;
        }
        let prefix = base.trim_end_matches("panelarea").to_string();
        if prefix.is_empty() {
            continue;
        }
        let rank = match suffix {
            None => 0,
            Some(2) => 1,
            Some(1) => 2,
            Some(0) => 3,
            Some(_) => 4,
        };
        candidates.entry(prefix).or_default().push((index, rank));
        root_members.insert(index);
    }

    let mut root_indices = HashMap::new();
    for (group, mut entries) in candidates {
        entries.sort_by_key(|(_, rank)| *rank);
        if let Some((index, _)) = entries.first() {
            root_indices.insert(group, *index);
        }
    }
    let mut groups = root_indices.keys().cloned().collect::<Vec<_>>();
    groups.sort();
    (root_indices, root_members, groups)
}

fn looks_like_rectangle(key: &str, rect: UiRect) -> bool {
    let lower = key.to_ascii_lowercase();
    if lower.contains("color") || lower.contains("colour") {
        return false;
    }
    extensions::is_known_rectangle(key)
        || lower.ends_with("area")
        || lower.contains("area_")
        || lower.ends_with("rect")
        || lower.contains("rect_")
        || (split_numeric_suffix(&lower).is_some() && rect.width >= 0 && rect.height >= 0)
}

fn looks_like_color(key: &str) -> bool {
    let lower = key.to_ascii_lowercase();
    lower.contains("color") || lower.contains("colour")
}

fn parse_rect(value: &str) -> Option<UiRect> {
    let tokens = tokenize_value(value);
    if tokens.len() != 4 {
        return None;
    }
    let mut numbers = [0i32; 4];
    for (index, token) in tokens.iter().enumerate() {
        let number = token.trim_end_matches(['f', 'F']).parse::<f32>().ok()?;
        if !number.is_finite() {
            return None;
        }
        numbers[index] = number.round() as i32;
    }
    Some(UiRect {
        x: numbers[0],
        y: numbers[1],
        width: numbers[2],
        height: numbers[3],
    })
}

fn parse_color(value: &str) -> Option<[f32; 3]> {
    let tokens = tokenize_value(value);
    if tokens.len() != 3 {
        return None;
    }
    let mut color = [0.0f32; 3];
    for (index, token) in tokens.iter().enumerate() {
        let channel = token.trim_end_matches(['f', 'F']).parse::<f32>().ok()?;
        if !channel.is_finite() {
            return None;
        }
        color[index] = channel.clamp(0.0, 1.0);
    }
    Some(color)
}

fn format_color(color: [f32; 3]) -> String {
    color
        .into_iter()
        .map(|channel| {
            let value = format!("{:.3}", channel.clamp(0.0, 1.0));
            value
                .trim_end_matches('0')
                .trim_end_matches('.')
                .to_string()
        })
        .collect::<Vec<_>>()
        .join("\t")
}

fn first_integer(value: &str) -> Option<i32> {
    first_token(value)?.parse::<i32>().ok()
}

fn parse_assignment(line: &str) -> Option<(String, String)> {
    let (key, value) = line.split_once('=')?;
    let key = key.trim();
    if key.is_empty() {
        return None;
    }
    Some((key.to_string(), value.trim().to_string()))
}

fn tokenize_value(value: &str) -> Vec<String> {
    let mut output = Vec::new();
    let mut current = String::new();
    let mut quote = None;
    for character in value.chars() {
        if let Some(active) = quote {
            if character == active {
                quote = None;
            } else {
                current.push(character);
            }
            continue;
        }
        if character == '"' || character == '\'' {
            quote = Some(character);
        } else if character.is_whitespace() {
            if !current.is_empty() {
                output.push(std::mem::take(&mut current));
            }
        } else {
            current.push(character);
        }
    }
    if !current.is_empty() {
        output.push(current);
    }
    output
}

fn strip_line_comment(line: &str) -> &str {
    let bytes = line.as_bytes();
    let mut quote = None;
    let mut index = 0usize;
    while index < bytes.len() {
        let character = bytes[index] as char;
        if let Some(active) = quote {
            if character == active {
                quote = None;
            }
        } else if character == '"' || character == '\'' {
            quote = Some(character);
        } else if character == '/' && bytes.get(index + 1) == Some(&b'/') {
            return &line[..index];
        }
        index += 1;
    }
    line
}

fn comment_offset(line: &str) -> Option<usize> {
    let clean = strip_line_comment(line);
    (clean.len() != line.len()).then_some(clean.len())
}

fn apply_value_edits(
    text: &str,
    line_ending: &str,
    edits: &[(usize, String, String)],
) -> Result<String> {
    let terminal_newline = text.ends_with(line_ending);
    let mut lines = text
        .split_terminator(line_ending)
        .map(ToString::to_string)
        .collect::<Vec<_>>();
    for (line_number, key, value) in edits {
        let Some(line) = lines.get_mut(line_number.saturating_sub(1)) else {
            bail!("{} points beyond the end of the file", key);
        };
        *line = replace_assignment_value(line, key, value)?;
    }
    let mut output = lines.join(line_ending);
    if terminal_newline {
        output.push_str(line_ending);
    }
    Ok(output)
}

fn normalize_crlf_text(text: &str) -> String {
    let had_final_newline = text.ends_with('\r') || text.ends_with('\n');
    let mut output = text.lines().collect::<Vec<_>>().join("\r\n");
    if had_final_newline {
        output.push_str("\r\n");
    }
    output
}

fn append_extension_properties(
    text: &str,
    line_ending: &str,
    additions: &[(String, String)],
) -> String {
    let mut output = text.to_string();
    if !output.is_empty() && !output.ends_with(line_ending) {
        output.push_str(line_ending);
    }
    if !output.is_empty() && !output.ends_with(&format!("{line_ending}{line_ending}")) {
        output.push_str(line_ending);
    }
    output.push_str("// Armada UI Editor: A2FO / Fleet Operations HUD overrides");
    output.push_str(line_ending);
    for (key, value) in additions {
        output.push_str(key);
        output.push_str("\t=\t");
        output.push_str(value);
        output.push_str(line_ending);
    }
    output
}

fn replace_assignment_value(line: &str, expected_key: &str, value: &str) -> Result<String> {
    let comment = comment_offset(line).unwrap_or(line.len());
    let assignment = &line[..comment];
    let Some(equals) = assignment.find('=') else {
        bail!("line no longer contains an assignment for {expected_key}");
    };
    let actual_key = assignment[..equals].trim();
    if !actual_key.eq_ignore_ascii_case(expected_key) {
        bail!("expected key {expected_key}, but source line now contains {actual_key}");
    }
    let old_value = &assignment[equals + 1..];
    let leading_len = old_value.len() - old_value.trim_start().len();
    let trailing_len = old_value.len() - old_value.trim_end().len();
    let leading = &old_value[..leading_len];
    let trailing = &old_value[old_value.len().saturating_sub(trailing_len)..];
    let mut output = String::new();
    output.push_str(&line[..equals + 1]);
    output.push_str(if leading.is_empty() { "\t" } else { leading });
    output.push_str(value);
    if comment < line.len() {
        output.push_str(if trailing.is_empty() { "\t" } else { trailing });
        output.push_str(&line[comment..]);
    } else {
        output.push_str(trailing);
    }
    Ok(output)
}

fn appended_path(path: &Path, suffix: &str) -> PathBuf {
    let mut value: OsString = path.as_os_str().to_owned();
    value.push(suffix);
    PathBuf::from(value)
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

fn find_case_insensitive_relative(base: &Path, relative: &Path) -> Option<PathBuf> {
    let mut current = base.to_path_buf();
    for component in relative.components() {
        let wanted = component.as_os_str().to_string_lossy();
        current = find_case_insensitive_child(&current, &wanted)?;
    }
    Some(current)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn assignment_rewrite_preserves_spacing_and_comment() {
        let line = "minimapPanelArea\t\t=\t0\t334\t146\t146 // keep me";
        assert_eq!(
            replace_assignment_value(line, "minimapPanelArea", "1\t2\t3\t4").unwrap(),
            "minimapPanelArea\t\t=\t1\t2\t3\t4 // keep me"
        );
    }

    #[test]
    fn panel_roots_make_child_rectangles_absolute() {
        let rectangles = vec![
            UiRectangle {
                key: "minimapDisplayArea".to_string(),
                rect: UiRect {
                    x: 9,
                    y: 9,
                    width: 128,
                    height: 128,
                },
                original: UiRect::default(),
                source: PathBuf::from("test.cfg"),
                line: 2,
                added: false,
            },
            UiRectangle {
                key: "minimapPanelArea".to_string(),
                rect: UiRect {
                    x: 0,
                    y: 334,
                    width: 146,
                    height: 146,
                },
                original: UiRect::default(),
                source: PathBuf::from("test.cfg"),
                line: 1,
                added: false,
            },
        ];
        let (root_indices, root_members, groups) = build_roots(&rectangles);
        let document = UiDocument {
            mode: GameMode::Armada1,
            game_root: PathBuf::new(),
            primary_path: PathBuf::new(),
            screen_width: 640,
            screen_height: 480,
            rectangles,
            colors: Vec::new(),
            warnings: Vec::new(),
            values: HashMap::new(),
            sources: HashMap::new(),
            root_indices,
            root_members,
            groups,
        };
        assert_eq!(
            document.absolute_rect(0),
            Some(UiRect {
                x: 9,
                y: 343,
                width: 128,
                height: 128
            })
        );
    }

    #[test]
    fn quoted_tokens_remain_one_value() {
        assert_eq!(
            tokenize_value("\"gui panel\" 10 20"),
            vec!["gui panel", "10", "20"]
        );
    }

    #[test]
    fn fleet_ops_and_a2fo_properties_are_recognized() {
        assert!(looks_like_rectangle(
            "infoSingleSystemsDisplay",
            UiRect {
                x: 10,
                y: 18,
                width: 512,
                height: 128,
            }
        ));
        assert_eq!(parse_color("0.20 1.00 0.20"), Some([0.2, 1.0, 0.2]));
        assert_eq!(format_color([0.2, 1.0, 0.0]), "0.2\t1\t0");
    }

    #[test]
    fn new_extension_rectangles_and_colors_append_as_overrides() {
        let directory = std::env::temp_dir().join(format!(
            "armada-ui-editor-extension-test-{}",
            std::process::id()
        ));
        let _ = fs::remove_dir_all(&directory);
        fs::create_dir_all(directory.join("misc")).unwrap();
        fs::create_dir_all(directory.join("Sprites")).unwrap();
        fs::create_dir_all(directory.join("Textures")).unwrap();
        let primary = directory.join("misc/gui_federation.cfg");
        fs::write(
            &primary,
            b"screenWidth = 1600\nscreenHeight = 1200\ninfoPanelArea_1 = 352 1038 1249 163\n",
        )
        .unwrap();

        let mut document = UiDocument::load(GameMode::Armada2, &directory, &primary).unwrap();
        document.add_rectangle(
            "infoSinglePhotonTorpedoesTextArea",
            UiRect {
                x: 386,
                y: 186,
                width: 340,
                height: 20,
            },
        );
        document.add_color("photonTorpedoColor", [0.0, 1.0, 0.0]);
        assert_eq!(document.dirty_count(), 2);

        let report = document.save().unwrap();
        assert_eq!(report.rectangle_count, 1);
        assert_eq!(report.color_count, 1);
        assert_eq!(document.dirty_count(), 0);
        let saved = fs::read_to_string(&primary).unwrap();
        assert!(saved.contains("// Armada UI Editor: A2FO / Fleet Operations HUD overrides\r\n"));
        assert!(saved.contains("infoSinglePhotonTorpedoesTextArea\t=\t386\t186\t340\t20\r\n"));
        assert!(saved.contains("photonTorpedoColor\t=\t0\t1\t0\r\n"));
        assert!(!saved.replace("\r\n", "").contains('\n'));
        assert_eq!(document.color("photonTorpedoColor"), Some([0.0, 1.0, 0.0]));
        assert!(appended_path(&primary, ".armada-ui-editor.bak").is_file());
        let _ = fs::remove_dir_all(&directory);
    }

    #[test]
    fn save_preserves_crlf_comments_and_creates_one_backup() {
        let directory = std::env::temp_dir().join(format!(
            "armada-ui-editor-model-test-{}",
            std::process::id()
        ));
        let _ = fs::remove_dir_all(&directory);
        fs::create_dir_all(directory.join("misc")).unwrap();
        fs::create_dir_all(directory.join("Sprites")).unwrap();
        fs::create_dir_all(directory.join("Textures")).unwrap();
        let included = directory.join("misc/gui_base.cfg");
        let primary = directory.join("misc/gui_fed.cfg");
        fs::write(
            &included,
            b"minimapPanelArea = 0 334 146 146 // root\r\nminimapDisplayArea = 9 9 128 128\r\n",
        )
        .unwrap();
        fs::write(&primary, b"#include \"gui_base.cfg\"\r\n").unwrap();

        let mut document = UiDocument::load(GameMode::Armada1, &directory, &primary).unwrap();
        let index = document
            .rectangles
            .iter()
            .position(|entry| entry.key.eq_ignore_ascii_case("minimapDisplayArea"))
            .unwrap();
        document.set_rect(
            index,
            UiRect {
                x: 12,
                y: 14,
                width: 120,
                height: 122,
            },
        );
        let report = document.save().unwrap();
        assert_eq!(report.rectangle_count, 1);
        assert_eq!(report.files, vec![included.clone()]);
        assert_eq!(
            fs::read_to_string(&included).unwrap(),
            "minimapPanelArea = 0 334 146 146 // root\r\nminimapDisplayArea = 12\t14\t120\t122\r\n"
        );
        assert!(appended_path(&included, ".armada-ui-editor.bak").is_file());
        let second = document.save().unwrap();
        assert!(second.files.is_empty());
        let _ = fs::remove_dir_all(&directory);
    }
}
