use anyhow::{anyhow, bail, Context, Result};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

const MAX_INCLUDE_DEPTH: usize = 32;
const MAX_PARENT_MOD_DEPTH: usize = 16;

#[derive(Debug, Clone)]
pub struct ResolvedValue {
    pub tokens: Vec<String>,
    pub source: PathBuf,
    pub line: usize,
}

#[derive(Debug, Clone, Default)]
pub struct ResolvedOdf {
    pub values: HashMap<String, ResolvedValue>,
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
}

impl ResourceContext {
    pub fn discover(ship_path: &Path, override_root: Option<&Path>) -> Result<Self> {
        let ship_path = ship_path
            .canonicalize()
            .unwrap_or_else(|_| ship_path.to_path_buf());
        let selected_asset_root = if let Some(root) = override_root {
            root.canonicalize().unwrap_or_else(|_| root.to_path_buf())
        } else {
            let odf_root = ship_path
                .ancestors()
                .find(|path| file_name_eq(path, "odf"))
                .map(Path::to_path_buf)
                .or_else(|| ship_path.parent().map(Path::to_path_buf))
                .ok_or_else(|| anyhow!("Ship ODF has no parent directory"))?;
            odf_root.parent().map(Path::to_path_buf).unwrap_or(odf_root)
        };

        let data_root = selected_asset_root
            .ancestors()
            .find(|path| file_name_eq(path, "data"))
            .map(Path::to_path_buf)
            .or_else(|| {
                ship_path
                    .ancestors()
                    .find(|path| file_name_eq(path, "data"))
                    .map(Path::to_path_buf)
            });

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
                if !seen.insert(path_identity(&current)) {
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
        context.rebuild_odf_index();
        Ok(context)
    }

    fn rebuild_odf_index(&mut self) {
        self.odf_index.clear();
        for root in &self.roots {
            let Some(odf_root) = find_case_insensitive_relative(&root.path, Path::new("odf"))
            else {
                continue;
            };
            let mut paths = WalkDir::new(odf_root)
                .follow_links(false)
                .into_iter()
                .filter_map(|entry| entry.ok())
                .filter(|entry| entry.file_type().is_file())
                .map(|entry| entry.into_path())
                .filter(|path| extension_eq(path, "odf"))
                .collect::<Vec<_>>();
            paths.sort_by_key(|path| path.to_string_lossy().to_ascii_lowercase());
            for path in paths {
                if let Some(name) = path.file_name().and_then(|value| value.to_str()) {
                    self.odf_index
                        .entry(name.to_ascii_lowercase())
                        .or_default()
                        .push(path);
                }
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

    pub fn resolve_named_file(&self, directory: &str, name: &str) -> Option<PathBuf> {
        let name = Path::new(name.trim().trim_matches(['"', '\'']));
        for root in &self.roots {
            let Some(directory) = find_case_insensitive_relative(&root.path, Path::new(directory))
            else {
                continue;
            };
            if let Some(path) = find_case_insensitive_relative(&directory, name) {
                if path.is_file() {
                    return Some(path);
                }
            }
        }
        None
    }

    pub fn resolve_texture(&self, name: &str) -> Option<PathBuf> {
        let normalized = name.trim().trim_matches(['"', '\'']).replace('\\', "/");
        let path = PathBuf::from(normalized);
        let mut candidates = vec![path.clone()];
        if path.extension().is_none() {
            for extension in ["tga", "dds", "png", "bmp", "jpg", "jpeg"] {
                candidates.push(path.with_extension(extension));
            }
        }

        for root in &self.roots {
            for relative in [
                "Textures/RGB",
                "Textures/Index8",
                "Textures/Compressed",
                "Textures",
            ] {
                let Some(texture_root) =
                    find_case_insensitive_relative(&root.path, Path::new(relative))
                else {
                    continue;
                };
                for candidate in &candidates {
                    if let Some(hit) = find_case_insensitive_relative(&texture_root, candidate) {
                        if hit.is_file() {
                            return Some(hit);
                        }
                    }
                }
            }
        }

        for root in &self.roots {
            let mut hits = WalkDir::new(&root.path)
                .follow_links(false)
                .into_iter()
                .filter_map(|entry| entry.ok())
                .filter(|entry| entry.file_type().is_file() && is_texture_path(entry.path()))
                .map(|entry| entry.into_path())
                .collect::<Vec<_>>();
            hits.sort_by_key(|path| path.to_string_lossy().to_ascii_lowercase());
            for candidate in &candidates {
                let candidate_name = candidate.file_name()?.to_string_lossy();
                if let Some(hit) = hits.iter().find(|path| {
                    path.file_name().is_some_and(|name| {
                        name.to_string_lossy().eq_ignore_ascii_case(&candidate_name)
                    })
                }) {
                    return Some(hit.clone());
                }
            }
        }
        None
    }

    pub fn resolve(&self, path: &Path) -> Result<ResolvedOdf> {
        let mut output = ResolvedOdf::default();
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
        let bytes =
            fs::read(&canonical).with_context(|| format!("Read ODF {}", canonical.display()))?;
        let contents = decode_windows_text(&bytes);
        stack.push(canonical.clone());

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
            } else if let Some((_, raw_value, _)) = pending.as_mut() {
                if !raw_value.is_empty() {
                    raw_value.push(' ');
                }
                raw_value.push_str(&statement);
            }
        }
        if let Some(assignment) = pending {
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
            source: source.to_path_buf(),
            line,
        },
    );
}

pub fn decode_windows_text(bytes: &[u8]) -> String {
    if let Ok(text) = std::str::from_utf8(bytes) {
        return text.to_string();
    }
    const CP1252: [char; 32] = [
        '\u{20ac}', '\u{0081}', '\u{201a}', '\u{0192}', '\u{201e}', '\u{2026}', '\u{2020}',
        '\u{2021}', '\u{02c6}', '\u{2030}', '\u{0160}', '\u{2039}', '\u{0152}', '\u{008d}',
        '\u{017d}', '\u{008f}', '\u{0090}', '\u{2018}', '\u{2019}', '\u{201c}', '\u{201d}',
        '\u{2022}', '\u{2013}', '\u{2014}', '\u{02dc}', '\u{2122}', '\u{0161}', '\u{203a}',
        '\u{0153}', '\u{009d}', '\u{017e}', '\u{0178}',
    ];
    bytes
        .iter()
        .map(|byte| match *byte {
            0x80..=0x9f => CP1252[(*byte - 0x80) as usize],
            value => value as char,
        })
        .collect()
}

pub fn strip_line_comment(line: &str) -> &str {
    let bytes = line.as_bytes();
    let mut quoted = false;
    let mut escaped = false;
    let mut index = 0usize;
    while index + 1 < bytes.len() {
        if escaped {
            escaped = false;
            index += 1;
            continue;
        }
        if bytes[index] == b'\\' && quoted {
            escaped = true;
        } else if bytes[index] == b'"' {
            quoted = !quoted;
        } else if !quoted && bytes[index] == b'/' && bytes[index + 1] == b'/' {
            return &line[..index];
        }
        index += 1;
    }
    line
}

pub fn parse_assignment(statement: &str) -> Option<(String, String)> {
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

pub fn tokenize_value(value: &str) -> Vec<String> {
    let mut tokens = Vec::new();
    let mut current = String::new();
    let mut quoted = false;
    let mut escaped = false;
    for ch in value.chars() {
        if escaped {
            current.push(ch);
            escaped = false;
        } else if quoted && ch == '\\' {
            escaped = true;
        } else if ch == '"' {
            if quoted {
                tokens.push(std::mem::take(&mut current));
            } else if !current.trim().is_empty() {
                tokens.push(current.trim().to_string());
                current.clear();
            }
            quoted = !quoted;
        } else if !quoted && ch.is_whitespace() {
            if !current.is_empty() {
                tokens.push(std::mem::take(&mut current));
            }
        } else {
            current.push(ch);
        }
    }
    if !current.trim().is_empty() {
        tokens.push(current.trim().to_string());
    }
    tokens
}

fn parse_include(statement: &str) -> Option<String> {
    let trimmed = statement.trim_start();
    let rest = if trimmed.len() >= 8 && trimmed[..8].eq_ignore_ascii_case("#include") {
        &trimmed[8..]
    } else if trimmed.len() >= 7 && trimmed[..7].eq_ignore_ascii_case("include") {
        &trimmed[7..]
    } else {
        return None;
    };
    tokenize_value(rest).into_iter().next()
}

fn read_parent_mod(path: &Path) -> Option<String> {
    let contents = decode_windows_text(&fs::read(path).ok()?);
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

fn with_extension(name: &str, extension: &str) -> String {
    let path = Path::new(name.trim().trim_matches(['"', '\'']));
    if path.extension().is_some_and(|value| {
        value
            .to_str()
            .is_some_and(|value| value.eq_ignore_ascii_case(extension))
    }) {
        path.file_name()
            .map(|value| value.to_string_lossy().to_string())
            .unwrap_or_else(|| name.to_string())
    } else {
        let stem = path
            .file_stem()
            .map(|value| value.to_string_lossy())
            .unwrap_or_else(|| "unknown".into());
        format!("{stem}.{extension}")
    }
}

fn extension_eq(path: &Path, expected: &str) -> bool {
    path.extension()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value.eq_ignore_ascii_case(expected))
}

fn is_texture_path(path: &Path) -> bool {
    path.extension()
        .and_then(|value| value.to_str())
        .is_some_and(|value| {
            ["tga", "dds", "png", "bmp", "jpg", "jpeg"]
                .iter()
                .any(|expected| value.eq_ignore_ascii_case(expected))
        })
}

fn file_name_eq(path: &Path, expected: &str) -> bool {
    path.file_name()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value.eq_ignore_ascii_case(expected))
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

fn push_root(roots: &mut Vec<AssetRoot>, path: PathBuf, label: String) {
    if !roots.iter().any(|root| path_eq(&root.path, &path)) {
        roots.push(AssetRoot { path, label });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cp1252_text_is_decoded_without_losing_ascii_commands() {
        let text = decode_windows_text(b"unitName = \"Admiral\x92s Ship\"\r\n");
        assert!(text.contains("Admiral\u{2019}s Ship"));
        assert!(text.contains("unitName"));
    }

    #[test]
    fn quoted_values_and_comments_parse() {
        let line = strip_line_comment("weapon1 = \"fedW_test\" // comment");
        let (_, raw) = parse_assignment(line).unwrap();
        assert_eq!(tokenize_value(&raw), vec!["fedW_test"]);
    }
}
