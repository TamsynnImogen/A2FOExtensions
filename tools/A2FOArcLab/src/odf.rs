use crate::fire_arc::{ArcConfig, ArcMode};
use anyhow::{anyhow, bail, Context, Result};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

const MAX_INCLUDE_DEPTH: usize = 32;
const MAX_PARENT_MOD_DEPTH: usize = 16;

#[derive(Debug, Clone)]
pub struct ResolvedValue {
    pub raw: String,
    pub tokens: Vec<String>,
    pub source: PathBuf,
    pub line: usize,
}

#[derive(Debug, Clone, Default)]
pub struct ResolvedOdf {
    pub path: PathBuf,
    pub values: HashMap<String, ResolvedValue>,
    pub files: Vec<PathBuf>,
    pub warnings: Vec<String>,
}

impl ResolvedOdf {
    pub fn value(&self, key: &str) -> Option<&ResolvedValue> {
        self.values.get(&key.to_ascii_lowercase())
    }

    pub fn string(&self, key: &str) -> Option<String> {
        self.value(key)
            .and_then(|value| value.tokens.first())
            .cloned()
    }

    pub fn list(&self, key: &str) -> Vec<String> {
        self.value(key)
            .map(|value| value.tokens.clone())
            .unwrap_or_default()
    }
}

#[derive(Debug, Clone)]
pub struct AssetRoot {
    pub path: PathBuf,
    pub label: String,
}

#[derive(Debug, Clone, Default)]
pub struct ResourceContext {
    pub roots: Vec<AssetRoot>,
    odf_index: HashMap<String, Vec<PathBuf>>,
    sod_index: HashMap<String, Vec<PathBuf>>,
}

#[derive(Debug, Clone)]
pub struct ArcParseResult {
    pub config: Option<ArcConfig>,
    pub errors: Vec<String>,
    pub sources: Vec<(String, PathBuf, usize)>,
}

#[derive(Debug, Clone)]
pub struct WeaponSlot {
    pub slot: u32,
    pub odf_name: String,
    pub odf_path: Option<PathBuf>,
    pub display_name: String,
    pub hardpoints: Vec<String>,
    pub arc: ArcParseResult,
    pub warnings: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct ArcProject {
    pub ship_path: PathBuf,
    pub ship_name: String,
    pub ship: ResolvedOdf,
    pub model_name: String,
    pub model_path: Option<PathBuf>,
    pub weapons: Vec<WeaponSlot>,
    pub resources: ResourceContext,
    pub warnings: Vec<String>,
}

impl ResourceContext {
    pub fn discover(ship_path: &Path) -> Result<Self> {
        let ship_path = ship_path
            .canonicalize()
            .unwrap_or_else(|_| ship_path.to_path_buf());
        let odf_root = ship_path
            .ancestors()
            .find(|path| file_name_eq(path, "odf"))
            .map(Path::to_path_buf)
            .or_else(|| ship_path.parent().map(Path::to_path_buf))
            .ok_or_else(|| anyhow!("Ship ODF has no parent directory"))?;
        let selected_asset_root = odf_root
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| odf_root.clone());
        let data_root = ship_path
            .ancestors()
            .find(|path| file_name_eq(path, "data"))
            .map(Path::to_path_buf);

        let mut roots = Vec::new();
        push_root(
            &mut roots,
            selected_asset_root.clone(),
            "selected mod".to_string(),
        );

        if let Some(data_root) = data_root.as_ref() {
            let mut current = selected_asset_root.clone();
            let mut seen = HashSet::new();
            for _ in 0..MAX_PARENT_MOD_DEPTH {
                let identity = path_identity(&current);
                if !seen.insert(identity) {
                    break;
                }
                let Some(parent_name) = read_parent_mod(&current.join("info.ini")) else {
                    break;
                };
                let Some(parent_path) =
                    find_case_insensitive_child(&data_root.join("Mods"), &parent_name)
                else {
                    break;
                };
                push_root(
                    &mut roots,
                    parent_path.clone(),
                    format!("parent mod: {parent_name}"),
                );
                current = parent_path;
            }
            push_root(&mut roots, data_root.clone(), "Data root".to_string());
        }

        let mut context = Self {
            roots,
            ..Default::default()
        };
        context.rebuild_indexes();
        Ok(context)
    }

    fn rebuild_indexes(&mut self) {
        self.odf_index.clear();
        self.sod_index.clear();
        for root in &self.roots {
            if let Some(path) = find_case_insensitive_relative(&root.path, Path::new("odf")) {
                index_extension(&path, "odf", &mut self.odf_index);
            }
            if let Some(path) = find_case_insensitive_relative(&root.path, Path::new("SOD")) {
                index_extension(&path, "sod", &mut self.sod_index);
            }
        }
    }

    pub fn resolve_odf_name(&self, name: &str) -> Option<PathBuf> {
        let key = with_extension(name, "odf").to_ascii_lowercase();
        self.odf_index
            .get(&key)
            .and_then(|paths| paths.first())
            .cloned()
    }

    pub fn resolve_sod_name(&self, name: &str) -> Option<PathBuf> {
        let key = with_extension(name, "sod").to_ascii_lowercase();
        self.sod_index
            .get(&key)
            .and_then(|paths| paths.first())
            .cloned()
    }

    pub fn texture_roots(&self) -> Vec<PathBuf> {
        let mut output: Vec<PathBuf> = Vec::new();
        for root in &self.roots {
            for relative in [
                "Textures/RGB",
                "Textures/Index8",
                "Textures/Compressed",
                "Textures",
                "SOD",
            ] {
                if let Some(path) = find_case_insensitive_relative(&root.path, Path::new(relative))
                {
                    if path.is_dir() && !output.iter().any(|existing| path_eq(existing, &path)) {
                        output.push(path);
                    }
                }
            }
        }
        output
    }

    pub fn resolve(&self, path: &Path) -> Result<ResolvedOdf> {
        let mut output = ResolvedOdf {
            path: path.to_path_buf(),
            ..Default::default()
        };
        let mut stack = Vec::new();
        self.resolve_into(path, 0, &mut stack, &mut output)?;
        Ok(output)
    }

    fn resolve_into(
        &self,
        path: &Path,
        depth: usize,
        stack: &mut Vec<PathBuf>,
        output: &mut ResolvedOdf,
    ) -> Result<()> {
        if depth > MAX_INCLUDE_DEPTH {
            bail!("ODF include depth exceeded {MAX_INCLUDE_DEPTH}");
        }
        let canonical = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
        if stack.iter().any(|item| path_eq(item, &canonical)) {
            output
                .warnings
                .push(format!("Include cycle ignored at {}", canonical.display()));
            return Ok(());
        }
        let contents = fs::read_to_string(&canonical)
            .with_context(|| format!("Read ODF {}", canonical.display()))?;
        stack.push(canonical.clone());
        output.files.push(canonical.clone());

        // Armada permits list values to continue on following lines. Retain
        // an assignment until the next assignment/include so commands such as
        // multi-line weaponHardpointsX are represented exactly as the game
        // sees them.
        let mut pending: Option<(String, String, usize)> = None;
        for (line_index, original) in contents.lines().enumerate() {
            let line_number = line_index + 1;
            let statement = strip_line_comment(original).trim().to_string();
            if statement.is_empty() {
                continue;
            }
            if let Some(include_name) = parse_include(&statement) {
                if let Some(assignment) = pending.take() {
                    commit_assignment(output, &canonical, assignment);
                }
                let relative_hit = canonical.parent().and_then(|parent| {
                    find_case_insensitive_relative(parent, Path::new(&include_name))
                });
                let include_path = relative_hit.or_else(|| self.resolve_odf_name(&include_name));
                if let Some(include_path) = include_path {
                    self.resolve_into(&include_path, depth + 1, stack, output)?;
                } else {
                    output.warnings.push(format!(
                        "{}:{}: include '{}' was not found",
                        canonical.display(),
                        line_number,
                        include_name
                    ));
                }
                continue;
            }
            if let Some((key, raw_value)) = parse_assignment(&statement) {
                if let Some(assignment) = pending.take() {
                    commit_assignment(output, &canonical, assignment);
                }
                pending = Some((key, raw_value, line_number));
                continue;
            }
            if let Some((_, raw_value, _)) = pending.as_mut() {
                if !raw_value.is_empty() {
                    raw_value.push(' ');
                }
                raw_value.push_str(&statement);
            }
        }
        if let Some(assignment) = pending.take() {
            commit_assignment(output, &canonical, assignment);
        }
        stack.pop();
        Ok(())
    }
}

fn commit_assignment(
    output: &mut ResolvedOdf,
    source: &Path,
    (key, raw, line): (String, String, usize),
) {
    output.values.insert(
        key.to_ascii_lowercase(),
        ResolvedValue {
            tokens: tokenize_value(&raw),
            raw,
            source: source.to_path_buf(),
            line,
        },
    );
}

pub fn load_project(ship_path: &Path) -> Result<ArcProject> {
    let resources = ResourceContext::discover(ship_path)?;
    let ship = resources.resolve(ship_path)?;
    let ship_name = ship
        .string("unitName")
        .or_else(|| ship.string("tooltip"))
        .unwrap_or_else(|| file_stem(ship_path));
    let model_name = ship
        .string("baseName")
        .filter(|value| !value.trim().is_empty())
        .unwrap_or_else(|| file_stem(ship_path));
    let model_path = resources.resolve_sod_name(&model_name);
    let mut warnings = ship.warnings.clone();
    if model_path.is_none() {
        warnings.push(format!(
            "Model '{}.sod' was not found automatically; select it manually",
            model_name
        ));
    }

    let mut slots = Vec::new();
    for (key, value) in &ship.values {
        let Some(slot) = numeric_suffix(key, "weapon") else {
            continue;
        };
        let Some(odf_name) = value.tokens.first().cloned() else {
            continue;
        };
        if odf_name.trim().is_empty() {
            continue;
        }
        slots.push((slot, odf_name));
    }
    slots.sort_by_key(|(slot, _)| *slot);

    let mut weapons = Vec::new();
    for (slot, odf_name) in slots {
        let hardpoints = ship.list(&format!("weaponHardpoints{slot}"));
        let odf_path = resources.resolve_odf_name(&odf_name);
        let mut slot_warnings = Vec::new();
        let (display_name, arc) = if let Some(path) = odf_path.as_ref() {
            match resources.resolve(path) {
                Ok(weapon) => {
                    slot_warnings.extend(weapon.warnings.iter().cloned());
                    let display_name = weapon
                        .string("wpnName")
                        .or_else(|| weapon.string("tooltip"))
                        .unwrap_or_else(|| file_stem(path));
                    (display_name, parse_arc(&weapon))
                }
                Err(error) => {
                    slot_warnings.push(error.to_string());
                    (odf_name.clone(), empty_arc())
                }
            }
        } else {
            slot_warnings.push(format!("Weapon ODF '{}.odf' was not found", odf_name));
            (odf_name.clone(), empty_arc())
        };
        if hardpoints.is_empty() {
            slot_warnings.push(format!(
                "weapon{slot} has no weaponHardpoints{slot} assignment"
            ));
        }
        weapons.push(WeaponSlot {
            slot,
            odf_name,
            odf_path,
            display_name,
            hardpoints,
            arc,
            warnings: slot_warnings,
        });
    }

    if weapons.is_empty() {
        warnings.push("No weaponX assignments were found in the resolved ship ODF".to_string());
    }

    Ok(ArcProject {
        ship_path: ship_path.to_path_buf(),
        ship_name,
        ship,
        model_name,
        model_path,
        weapons,
        resources,
        warnings,
    })
}

pub fn parse_arc(odf: &ResolvedOdf) -> ArcParseResult {
    const COMMANDS: &[&str] = &[
        "fireArcCenter",
        "fireArcWidth",
        "fireArcYaw",
        "fireArcPitch",
        "fireArcYawAngle",
        "fireArcPitchAngle",
        "fireArcAngle",
        "fireArcMode",
    ];
    let mut sources = Vec::new();
    for command in COMMANDS {
        if let Some(value) = odf.value(command) {
            sources.push(((*command).to_string(), value.source.clone(), value.line));
        }
    }
    if sources.is_empty() {
        return empty_arc();
    }

    let mut errors = Vec::new();
    let centre = optional_number(odf, "fireArcCenter", &mut errors);
    let width = optional_number(odf, "fireArcWidth", &mut errors);
    let yaw = optional_number(odf, "fireArcYaw", &mut errors);
    let pitch = optional_number(odf, "fireArcPitch", &mut errors);
    let yaw_angle = optional_number(odf, "fireArcYawAngle", &mut errors);
    let pitch_angle = optional_number(odf, "fireArcPitchAngle", &mut errors);
    let cone_angle = optional_number(odf, "fireArcAngle", &mut errors);

    let mode_text = odf
        .string("fireArcMode")
        .map(|text| text.to_ascii_lowercase());
    let explicit_mode = mode_text.is_some();
    let mode = match mode_text.as_deref() {
        Some("box") => ArcMode::Box,
        Some("cone") => ArcMode::Cone,
        Some(other) => {
            errors.push(format!(
                "fireArcMode must be 'box' or 'cone', not '{other}'"
            ));
            ArcMode::Box
        }
        None if cone_angle.is_some() => ArcMode::Cone,
        None => ArcMode::Box,
    };

    let mut config = ArcConfig::default();
    config.set_mode(mode);
    if let Some(value) = centre {
        config.yaw_degrees = value;
    }
    if let Some(value) = yaw {
        config.yaw_degrees = value;
    }
    if let Some(value) = pitch {
        config.pitch_degrees = value;
    }

    match mode {
        ArcMode::Cone => {
            if let Some(value) = cone_angle {
                config.cone_angle_degrees = value;
            } else {
                errors.push("cone mode requires fireArcAngle".to_string());
            }
        }
        ArcMode::Box => {
            if width.is_none() && yaw_angle.is_none() && pitch_angle.is_none() {
                errors.push(if explicit_mode {
                    "box mode requires fireArcWidth, fireArcYawAngle, or fireArcPitchAngle"
                        .to_string()
                } else {
                    "an arc centre requires a width/angle command".to_string()
                });
            }
            if let Some(value) = width {
                config.yaw_angle_degrees = value;
            }
            if let Some(value) = yaw_angle {
                config.yaw_angle_degrees = value;
            }
            if let Some(value) = pitch_angle {
                config.pitch_angle_degrees = value;
            }
        }
    }

    ArcParseResult {
        config: if errors.is_empty() {
            Some(config.normalized())
        } else {
            None
        },
        errors,
        sources,
    }
}

fn empty_arc() -> ArcParseResult {
    ArcParseResult {
        config: None,
        errors: Vec::new(),
        sources: Vec::new(),
    }
}

fn optional_number(odf: &ResolvedOdf, key: &str, errors: &mut Vec<String>) -> Option<f32> {
    let value = odf.value(key)?;
    let token = value.tokens.first().map(String::as_str).unwrap_or("");
    let trimmed = token.trim().trim_end_matches(['f', 'F']);
    match trimmed.parse::<f32>() {
        Ok(number) if number.is_finite() => Some(number),
        _ => {
            errors.push(format!(
                "{}:{}: {key} has malformed value '{}'",
                value.source.display(),
                value.line,
                value.raw
            ));
            None
        }
    }
}

fn strip_line_comment(line: &str) -> &str {
    let bytes = line.as_bytes();
    let mut quoted = false;
    let mut escaped = false;
    let mut index = 0usize;
    while index + 1 < bytes.len() {
        let byte = bytes[index];
        if escaped {
            escaped = false;
            index += 1;
            continue;
        }
        if byte == b'\\' && quoted {
            escaped = true;
            index += 1;
            continue;
        }
        if byte == b'"' {
            quoted = !quoted;
            index += 1;
            continue;
        }
        if !quoted && byte == b'/' && bytes[index + 1] == b'/' {
            return &line[..index];
        }
        index += 1;
    }
    line
}

fn parse_include(statement: &str) -> Option<String> {
    let trimmed = statement.trim_start();
    let rest = if let Some(rest) = trimmed.strip_prefix("#include") {
        rest
    } else if trimmed.len() >= 7 && trimmed[..7].eq_ignore_ascii_case("include") {
        &trimmed[7..]
    } else {
        return None;
    };
    tokenize_value(rest).into_iter().next()
}

fn parse_assignment(statement: &str) -> Option<(String, String)> {
    let equals = statement.find('=')?;
    let key = statement[..equals].trim();
    if key.is_empty()
        || !key
            .chars()
            .all(|ch| ch.is_ascii_alphanumeric() || ch == '_')
    {
        return None;
    }
    let value = statement[equals + 1..]
        .trim()
        .trim_end_matches(';')
        .trim()
        .to_string();
    Some((key.to_string(), value))
}

fn tokenize_value(value: &str) -> Vec<String> {
    let mut tokens = Vec::new();
    let mut current = String::new();
    let mut quoted = false;
    let mut escaped = false;
    for ch in value.chars() {
        if escaped {
            current.push(ch);
            escaped = false;
            continue;
        }
        if quoted && ch == '\\' {
            escaped = true;
            continue;
        }
        if ch == '"' {
            if quoted {
                tokens.push(std::mem::take(&mut current));
            } else if !current.trim().is_empty() {
                tokens.push(current.trim().to_string());
                current.clear();
            }
            quoted = !quoted;
            continue;
        }
        if !quoted && ch.is_whitespace() {
            if !current.is_empty() {
                tokens.push(std::mem::take(&mut current));
            }
            continue;
        }
        current.push(ch);
    }
    if !current.trim().is_empty() {
        tokens.push(current.trim().to_string());
    }
    tokens
}

fn numeric_suffix(key: &str, prefix: &str) -> Option<u32> {
    let suffix = key.strip_prefix(prefix)?;
    if suffix.is_empty() || !suffix.chars().all(|ch| ch.is_ascii_digit()) {
        return None;
    }
    suffix.parse().ok()
}

fn read_parent_mod(path: &Path) -> Option<String> {
    let contents = fs::read_to_string(path).ok()?;
    for line in contents.lines() {
        let statement = strip_line_comment(line).trim();
        let Some((key, value)) = parse_assignment(statement) else {
            continue;
        };
        if key.eq_ignore_ascii_case("ParentMod") {
            return tokenize_value(&value).into_iter().next();
        }
    }
    None
}

fn index_extension(root: &Path, extension: &str, index: &mut HashMap<String, Vec<PathBuf>>) {
    if !root.is_dir() {
        return;
    }
    let mut files = WalkDir::new(root)
        .follow_links(false)
        .into_iter()
        .filter_map(|entry| entry.ok())
        .filter(|entry| entry.file_type().is_file())
        .map(|entry| entry.into_path())
        .filter(|path| {
            path.extension()
                .and_then(|value| value.to_str())
                .map(|value| value.eq_ignore_ascii_case(extension))
                .unwrap_or(false)
        })
        .collect::<Vec<_>>();
    files.sort_by_key(|path| path.to_string_lossy().to_ascii_lowercase());
    for path in files {
        if let Some(name) = path.file_name().and_then(|value| value.to_str()) {
            index
                .entry(name.to_ascii_lowercase())
                .or_default()
                .push(path);
        }
    }
}

fn push_root(roots: &mut Vec<AssetRoot>, path: PathBuf, label: String) {
    if roots.iter().any(|root| path_eq(&root.path, &path)) {
        return;
    }
    roots.push(AssetRoot { path, label });
}

fn find_case_insensitive_child(root: &Path, name: &str) -> Option<PathBuf> {
    fs::read_dir(root)
        .ok()?
        .filter_map(|entry| entry.ok())
        .find(|entry| {
            entry
                .file_name()
                .to_string_lossy()
                .eq_ignore_ascii_case(name)
        })
        .map(|entry| entry.path())
}

pub fn find_case_insensitive_relative(root: &Path, relative: &Path) -> Option<PathBuf> {
    if relative.is_absolute() {
        return relative.exists().then(|| relative.to_path_buf());
    }
    let mut current = root.to_path_buf();
    for component in relative.components() {
        let name = component.as_os_str().to_string_lossy();
        current = find_case_insensitive_child(&current, &name)?;
    }
    Some(current)
}

fn with_extension(name: &str, extension: &str) -> String {
    let path = Path::new(name.trim());
    if path
        .extension()
        .and_then(|value| value.to_str())
        .map(|value| value.eq_ignore_ascii_case(extension))
        .unwrap_or(false)
    {
        path.file_name()
            .and_then(|value| value.to_str())
            .unwrap_or(name)
            .to_string()
    } else {
        format!("{}.{}", file_stem(path), extension)
    }
}

fn file_stem(path: &Path) -> String {
    path.file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("unknown")
        .to_string()
}

fn file_name_eq(path: &Path, expected: &str) -> bool {
    path.file_name()
        .and_then(|value| value.to_str())
        .map(|value| value.eq_ignore_ascii_case(expected))
        .unwrap_or(false)
}

fn path_identity(path: &Path) -> String {
    path.canonicalize()
        .unwrap_or_else(|_| path.to_path_buf())
        .to_string_lossy()
        .replace('\\', "/")
        .to_ascii_lowercase()
}

fn path_eq(left: &Path, right: &Path) -> bool {
    path_identity(left) == path_identity(right)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn temp_dir() -> PathBuf {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let path = std::env::temp_dir().join(format!("a2fo_arclab_odf_{unique}"));
        fs::create_dir_all(&path).unwrap();
        path
    }

    #[test]
    fn comments_and_quoted_lists_parse() {
        let value = strip_line_comment("weaponHardpoints1 = \"hp01\" \"hp02\" // \"hp99\"");
        let (_, value) = parse_assignment(value).unwrap();
        assert_eq!(tokenize_value(&value), vec!["hp01", "hp02"]);
    }

    #[test]
    fn include_values_are_overridden_in_order() {
        let root = temp_dir();
        let odf = root.join("odf");
        fs::create_dir_all(&odf).unwrap();
        fs::write(
            odf.join("parent.odf"),
            "fireArcPitch = 0\nfireArcPitchAngle = 180\n",
        )
        .unwrap();
        fs::write(
            odf.join("weapon.odf"),
            "#include \"parent.odf\"\nfireArcPitch = 90\n",
        )
        .unwrap();
        let context = ResourceContext {
            roots: vec![AssetRoot {
                path: root.clone(),
                label: "test".to_string(),
            }],
            ..Default::default()
        };
        let resolved = context.resolve(&odf.join("weapon.odf")).unwrap();
        assert_eq!(resolved.string("fireArcPitch").as_deref(), Some("90"));
        assert_eq!(resolved.string("fireArcPitchAngle").as_deref(), Some("180"));
        fs::remove_dir_all(root).ok();
    }

    #[test]
    fn multiline_hardpoint_lists_are_preserved() {
        let root = temp_dir();
        let odf = root.join("ship.odf");
        fs::write(
            &odf,
            "weapon1 = \"test_phaser\"\nweaponHardpoints1 = \"hp01\"\n    \"hp02\" \"hp03\"\nunitName = \"Test Ship\"\n",
        )
        .unwrap();
        let context = ResourceContext::default();
        let resolved = context.resolve(&odf).unwrap();
        assert_eq!(
            resolved.list("weaponHardpoints1"),
            vec!["hp01", "hp02", "hp03"]
        );
        assert_eq!(resolved.string("unitName").as_deref(), Some("Test Ship"));
        fs::remove_dir_all(root).ok();
    }
}
