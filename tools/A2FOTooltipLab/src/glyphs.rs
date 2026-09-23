use anyhow::{bail, Context, Result};
use std::collections::{BTreeMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};

use crate::document::normalize_crlf_bytes;

#[derive(Debug, Clone)]
pub struct FontGlyph {
    pub byte: u8,
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub advance: f32,
    pub height: u32,
    pub label: Option<String>,
}

#[derive(Debug, Clone)]
pub struct FontCatalog {
    pub spr_path: PathBuf,
    pub texture_path: PathBuf,
    pub reference_width: u32,
    pub reference_height: u32,
    pub line_height: u32,
    pub glyphs: BTreeMap<u8, FontGlyph>,
    pub roots: Vec<PathBuf>,
    pub warnings: Vec<String>,
}

impl FontCatalog {
    pub fn resolve(document_path: &Path, override_root: Option<&Path>) -> Result<Self> {
        let roots = asset_roots(document_path, override_root)?;
        let spr_path = find_asset(&roots, &["Sprites"], "FontSmall.spr")
            .context("Could not find Sprites/FontSmall.spr in this mod or its ParentMod chain")?;
        let parsed = parse_font_sprite(&spr_path)?;
        let texture_names = [
            format!("{}.tga", parsed.texture_name),
            format!("{}.dds", parsed.texture_name),
            format!("{}.png", parsed.texture_name),
        ];
        let texture_path = texture_names
            .iter()
            .find_map(|name| {
                find_asset(&roots, &["Textures"], name)
                    .or_else(|| find_asset(&roots, &["Textures", "RGB"], name))
            })
            .with_context(|| {
                format!(
                    "Could not find texture {} for {}",
                    parsed.texture_name,
                    spr_path.display()
                )
            })?;

        let mut glyphs = BTreeMap::new();
        let mut warnings = parsed.warnings;
        let glyph_count = parsed.positions.len().min(parsed.widths.len());
        let profile = detect_profile(
            &parsed.positions,
            &parsed.widths,
            parsed.reference_width,
            document_path,
        );
        if parsed.positions.len() != parsed.widths.len() {
            warnings.push(format!(
                "The font has {} UV positions but {} widths; using the first {glyph_count}",
                parsed.positions.len(),
                parsed.widths.len()
            ));
        }
        for index in 0..glyph_count.min(224) {
            let byte = (index + 32) as u8;
            let position = &parsed.positions[index];
            let width = parsed.widths[index].0.max(1.0);
            let label = profile_label(profile, byte)
                .map(str::to_string)
                .or_else(|| position.2.clone())
                .or_else(|| parsed.widths[index].1.clone());
            glyphs.insert(
                byte,
                FontGlyph {
                    byte,
                    x: position.0,
                    y: position.1,
                    width: width.ceil() as u32,
                    advance: width,
                    height: parsed.line_height,
                    label,
                },
            );
        }
        Ok(Self {
            spr_path,
            texture_path,
            reference_width: parsed.reference_width,
            reference_height: parsed.reference_height,
            line_height: parsed.line_height,
            glyphs,
            roots,
            warnings,
        })
    }

    pub fn glyph(&self, byte: u8) -> Option<&FontGlyph> {
        self.glyphs.get(&byte)
    }

    pub fn is_special(&self, byte: u8) -> bool {
        if byte < 0x7f {
            return false;
        }
        self.glyph(byte).is_some_and(|glyph| {
            glyph.label.is_some()
                || glyph.advance >= 24.0
                || (self.reference_width >= 512 && glyph.y >= 100)
        })
    }

    pub fn special_glyphs(&self) -> impl Iterator<Item = &FontGlyph> {
        self.glyphs
            .values()
            .filter(|glyph| self.is_special(glyph.byte))
    }
}

#[derive(Debug)]
struct ParsedFont {
    reference_width: u32,
    reference_height: u32,
    line_height: u32,
    texture_name: String,
    positions: Vec<(u32, u32, Option<String>)>,
    widths: Vec<(f32, Option<String>)>,
    warnings: Vec<String>,
}

fn parse_font_sprite(path: &Path) -> Result<ParsedFont> {
    let bytes = fs::read(path).with_context(|| format!("Read {}", path.display()))?;
    let text = String::from_utf8_lossy(&bytes);
    let lines = text.lines().collect::<Vec<_>>();
    let reference_width = directive_u32(&lines, "@referenceWidth").unwrap_or(1024);
    let reference_height = directive_u32(&lines, "@referenceHeight").unwrap_or(256);

    let uv_animation = lines
        .iter()
        .find_map(|line| {
            let fields = code_part(line).split_whitespace().collect::<Vec<_>>();
            (fields.first() == Some(&"@animation")
                && fields.get(1).is_some_and(|name| name.ends_with("_uv")))
            .then(|| fields[1].to_string())
        })
        .context("FontSmall.spr has no UV animation")?;
    let width_animation = lines
        .iter()
        .find_map(|line| {
            let fields = code_part(line).split_whitespace().collect::<Vec<_>>();
            (fields.first() == Some(&"@animation")
                && fields.get(1).is_some_and(|name| name.ends_with("_w")))
            .then(|| fields[1].to_string())
        })
        .context("FontSmall.spr has no width animation")?;

    let positions = parse_uv_animation(&lines, &uv_animation)?;
    let widths = parse_width_animation(&lines, &width_animation)?;
    let (texture_name, line_height) = parse_page(&lines, &uv_animation, &width_animation)?;
    let mut warnings = Vec::new();
    if positions.len() < 224 {
        warnings.push(format!(
            "Only {} of the expected 224 byte glyph positions were declared",
            positions.len()
        ));
    }
    Ok(ParsedFont {
        reference_width,
        reference_height,
        line_height,
        texture_name,
        positions,
        widths,
        warnings,
    })
}

pub fn rewrite_glyph_keyframes(
    path: &Path,
    byte: u8,
    x: u32,
    y: u32,
    advance: f32,
    label: &str,
) -> Result<Vec<u8>> {
    if byte < 32 {
        bail!("FontSmall.spr only maps byte codes 0x20 through 0xFF");
    }
    let source = fs::read(path).with_context(|| format!("Read {}", path.display()))?;
    let text = std::str::from_utf8(&source).context("FontSmall.spr is not valid text")?;
    let lines = text.lines().collect::<Vec<_>>();
    let uv_animation =
        animation_name(&lines, "_uv").context("FontSmall.spr has no UV animation")?;
    let width_animation =
        animation_name(&lines, "_w").context("FontSmall.spr has no width animation")?;
    let index = byte as usize - 32;
    let uv_span = keyframe_source_span(text, &uv_animation, index, KeyframeKind::Uv)?;
    let width_span = keyframe_source_span(text, &width_animation, index, KeyframeKind::Width)?;
    let label = sanitize_label(label);
    let advance_text = if advance.fract().abs() < 0.001 {
        format!("{advance:.0}")
    } else {
        format!("{advance:.3}").trim_end_matches('0').to_string()
    };
    let uv_replacement = replacement_line(text, uv_span, &format!("{x}\t{y}\t# {label}"));
    let width_replacement =
        replacement_line(text, width_span, &format!("{advance_text}\t# {label}"));
    let mut edits = vec![
        (uv_span.0, uv_span.1, uv_replacement),
        (width_span.0, width_span.1, width_replacement),
    ];
    edits.sort_by_key(|edit| std::cmp::Reverse(edit.0));
    let mut output = source;
    for (start, end, replacement) in edits {
        output.splice(start..end, replacement);
    }
    Ok(normalize_crlf_bytes(&output))
}

fn animation_name(lines: &[&str], suffix: &str) -> Option<String> {
    lines.iter().find_map(|line| {
        let fields = code_part(line).split_whitespace().collect::<Vec<_>>();
        (fields.first() == Some(&"@animation")
            && fields.get(1).is_some_and(|name| name.ends_with(suffix)))
        .then(|| fields[1].to_string())
    })
}

#[derive(Clone, Copy)]
enum KeyframeKind {
    Uv,
    Width,
}

fn keyframe_source_span(
    text: &str,
    animation: &str,
    target: usize,
    kind: KeyframeKind,
) -> Result<(usize, usize)> {
    let mut animation_seen = false;
    let mut keyframes_seen = false;
    let mut count = 0usize;
    let mut offset = 0usize;
    for inclusive in text.split_inclusive('\n') {
        let line = inclusive.trim_end_matches(['\r', '\n']);
        let fields = code_part(line).split_whitespace().collect::<Vec<_>>();
        if fields.first() == Some(&"@animation") {
            if animation_seen {
                break;
            }
            animation_seen = fields.get(1) == Some(&animation);
            keyframes_seen = false;
        } else if animation_seen && code_part(line).trim() == "@keyframes" {
            keyframes_seen = true;
        } else if animation_seen && keyframes_seen {
            if code_part(line).trim_start().starts_with('@') {
                break;
            }
            let numeric = match kind {
                KeyframeKind::Uv => {
                    fields.len() >= 2
                        && fields[0].parse::<u32>().is_ok()
                        && fields[1].parse::<u32>().is_ok()
                }
                KeyframeKind::Width => fields.len() == 1 && fields[0].parse::<f32>().is_ok(),
            };
            if numeric {
                if count == target {
                    return Ok((offset, offset + inclusive.len()));
                }
                count += 1;
            }
        }
        offset += inclusive.len();
    }
    bail!(
        "Animation {animation} has only {count} usable keyframes; cannot replace byte index {target}"
    )
}

fn replacement_line(text: &str, span: (usize, usize), body: &str) -> Vec<u8> {
    let original = &text[span.0..span.1];
    let indent = original
        .chars()
        .take_while(|character| {
            character.is_ascii_whitespace() && *character != '\r' && *character != '\n'
        })
        .collect::<String>();
    let newline = if original.ends_with("\r\n") {
        "\r\n"
    } else if original.ends_with('\n') {
        "\n"
    } else {
        ""
    };
    format!("{indent}{body}{newline}").into_bytes()
}

fn sanitize_label(label: &str) -> String {
    let cleaned = label
        .chars()
        .filter(|character| *character != '#' && *character != '\r' && *character != '\n')
        .collect::<String>();
    let cleaned = cleaned.trim();
    if cleaned.is_empty() {
        "Custom glyph".to_string()
    } else {
        cleaned.to_string()
    }
}

fn directive_u32(lines: &[&str], directive: &str) -> Option<u32> {
    lines.iter().find_map(|line| {
        let code = code_part(line).trim();
        if let Some(value) = code.strip_prefix(directive)?.strip_prefix('=') {
            return value.trim().parse().ok();
        }
        let fields = code.split_whitespace().collect::<Vec<_>>();
        (fields.first() == Some(&directive)).then(|| fields.get(1)?.parse().ok())?
    })
}

fn parse_uv_animation(lines: &[&str], animation: &str) -> Result<Vec<(u32, u32, Option<String>)>> {
    let start = animation_keyframes_start(lines, animation)?;
    let mut output = Vec::new();
    for line in &lines[start..] {
        if code_part(line).trim_start().starts_with('@') {
            break;
        }
        let fields = code_part(line).split_whitespace().collect::<Vec<_>>();
        if fields.len() < 2 {
            continue;
        }
        let Ok(x) = fields[0].parse() else { continue };
        let Ok(y) = fields[1].parse() else { continue };
        output.push((x, y, comment_part(line)));
    }
    if output.is_empty() {
        bail!("UV animation {animation} has no keyframes");
    }
    Ok(output)
}

fn parse_width_animation(lines: &[&str], animation: &str) -> Result<Vec<(f32, Option<String>)>> {
    let start = animation_keyframes_start(lines, animation)?;
    let mut output = Vec::new();
    for line in &lines[start..] {
        if code_part(line).trim_start().starts_with('@') {
            break;
        }
        let fields = code_part(line).split_whitespace().collect::<Vec<_>>();
        if fields.len() != 1 {
            continue;
        }
        let Ok(width) = fields[0].parse() else {
            continue;
        };
        output.push((width, comment_part(line)));
    }
    if output.is_empty() {
        bail!("Width animation {animation} has no keyframes");
    }
    Ok(output)
}

fn animation_keyframes_start(lines: &[&str], animation: &str) -> Result<usize> {
    let animation_index = lines
        .iter()
        .position(|line| {
            let fields = code_part(line).split_whitespace().collect::<Vec<_>>();
            fields.first() == Some(&"@animation") && fields.get(1) == Some(&animation)
        })
        .with_context(|| format!("Missing animation {animation}"))?;
    lines[animation_index + 1..]
        .iter()
        .position(|line| code_part(line).trim() == "@keyframes")
        .map(|offset| animation_index + 1 + offset + 1)
        .with_context(|| format!("Animation {animation} has no @keyframes section"))
}

fn parse_page(lines: &[&str], uv: &str, width: &str) -> Result<(String, u32)> {
    for line in lines {
        let code = code_part(line);
        if !code.contains(&format!("@anim={uv}")) || !code.contains(&format!("@anim={width}")) {
            continue;
        }
        let fields = code.split_whitespace().collect::<Vec<_>>();
        if fields.len() < 6 {
            continue;
        }
        let height = fields[5].parse().unwrap_or(23);
        return Ok((fields[1].to_string(), height));
    }
    bail!("Could not find the font page using {uv} and {width}")
}

fn code_part(line: &str) -> &str {
    line.split_once('#').map_or(line, |(code, _)| code)
}

fn comment_part(line: &str) -> Option<String> {
    let comment = line.split_once('#')?.1.trim();
    (comment.chars().count() >= 3 && !comment.contains('\u{FFFD}')).then(|| comment.to_string())
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum FontProfile {
    FleetOps4,
    Roots,
    Unknown,
}

fn detect_profile(
    positions: &[(u32, u32, Option<String>)],
    widths: &[(f32, Option<String>)],
    reference_width: u32,
    document_path: &Path,
) -> FontProfile {
    let document_root_name = document_path
        .parent()
        .and_then(Path::file_name)
        .and_then(|name| name.to_str())
        .unwrap_or("");
    let sample = |byte: u8| {
        let index = byte as usize - 32;
        Some((positions.get(index)?, widths.get(index)?.0))
    };
    if document_root_name.eq_ignore_ascii_case("Fleet Ops 4.0")
        && reference_width == 1024
        && sample(0xA1).is_some_and(|(position, width)| {
            position.0 == 2 && position.1 == 127 && (width - 112.0).abs() < 0.5
        })
        && sample(0xA8).is_some_and(|(position, _)| position.0 == 136 && position.1 == 127)
    {
        FontProfile::FleetOps4
    } else if document_root_name.eq_ignore_ascii_case("Data")
        && reference_width == 1024
        && sample(0xA1).is_some_and(|(position, width)| {
            position.0 == 2 && position.1 == 127 && (width - 120.0).abs() < 0.5
        })
        && sample(0xA8).is_some_and(|(position, _)| position.0 == 162 && position.1 == 127)
    {
        FontProfile::Roots
    } else {
        FontProfile::Unknown
    }
}

fn profile_label(profile: FontProfile, byte: u8) -> Option<&'static str> {
    if profile == FontProfile::Unknown {
        return None;
    }
    let common = match byte {
        0x7F => "Red 0",
        0x81 => "Red 1",
        0x85 => "Dilithium",
        0x86 => "Tritanium",
        0x87 => "Supply",
        0x89 => "Crew",
        0x8D => "Red 2",
        0x8F => "Red 3",
        0x90 => "Red 4",
        0x99 => "Collective Connections",
        0x9D => "Red 5",
        0xA0 => "Red 6",
        0xAD => "Red 7",
        0xBB => "Red +",
        0xBC => "Red -",
        0xBD => "Red separator",
        0xBE => "Red 8",
        0xD7 => "Red 9",
        0xF7 => "Special energy",
        _ => "",
    };
    if !common.is_empty() {
        return Some(common);
    }
    match profile {
        FontProfile::FleetOps4 => Some(match byte {
            0xA1 => "Support Vessel",
            0xA8 => "Fighter Carrier",
            0xA9 => "Troopship",
            0xAB => "(passive)",
            0xAC => "Units granted",
            0xAE => "Specials granted",
            0xB6 => "(Avatar)",
            0xB7 => "(Mixed-Tech)",
            _ => return None,
        }),
        FontProfile::Roots => Some(match byte {
            0x80 => "Time",
            0xA1 => "Support Ship",
            0xA2 => "Cruiser",
            0xA3 => "Battleship",
            0xA4 => "Scout",
            0xA5 => "Dreadnought",
            0xA8 => "Berserker",
            0xA9 => "Artillery",
            0xAA => "Construction Ship",
            0xAC => "(Mixed-Tech)",
            0xAE => "Mining Ship",
            0xB6 => "(passive)",
            0xB7 => "(Avatar)",
            0xD8 => "Destroyer",
            0xF8 => "Station",
            _ => return None,
        }),
        FontProfile::Unknown => unreachable!("handled above"),
    }
}

fn asset_roots(document_path: &Path, override_root: Option<&Path>) -> Result<Vec<PathBuf>> {
    let document_root = document_path
        .parent()
        .context("The localized strings path has no parent directory")?;
    let data_root = find_data_root(document_root);
    let mut roots = Vec::new();
    if let Some(root) = override_root {
        roots.push(root.to_path_buf());
    }
    roots.push(document_root.to_path_buf());

    if let Some(data_root) = data_root.as_deref() {
        let mods_root = child_case_insensitive(data_root, "Mods");
        let mut current = document_root.to_path_buf();
        let mut seen = HashSet::new();
        while seen.insert(normalized_key(&current)) {
            let Some(parent_name) = read_parent_mod(&current) else {
                break;
            };
            let Some(mods_root) = mods_root.as_deref() else {
                break;
            };
            let Some(parent) = child_case_insensitive(mods_root, &parent_name) else {
                break;
            };
            roots.push(parent.clone());
            current = parent;
        }
        roots.push(data_root.to_path_buf());
    }
    deduplicate_paths(&mut roots);
    Ok(roots)
}

fn find_data_root(start: &Path) -> Option<PathBuf> {
    for ancestor in start.ancestors() {
        if ancestor
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(|name| name.eq_ignore_ascii_case("Data"))
        {
            return Some(ancestor.to_path_buf());
        }
    }
    None
}

fn read_parent_mod(root: &Path) -> Option<String> {
    let path = child_case_insensitive(root, "info.ini")?;
    let bytes = fs::read(path).ok()?;
    for raw_line in String::from_utf8_lossy(&bytes).lines() {
        let line = raw_line
            .split_once(';')
            .map_or(raw_line, |(value, _)| value);
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        if key.trim().eq_ignore_ascii_case("ParentMod") {
            return Some(value.trim().trim_matches('"').to_string());
        }
    }
    None
}

fn find_asset(roots: &[PathBuf], directories: &[&str], name: &str) -> Option<PathBuf> {
    roots.iter().find_map(|root| {
        let mut directory = root.clone();
        for component in directories {
            directory = child_case_insensitive(&directory, component)?;
        }
        child_case_insensitive(&directory, name)
    })
}

fn child_case_insensitive(parent: &Path, name: &str) -> Option<PathBuf> {
    let exact = parent.join(name);
    if exact.exists() {
        return Some(exact);
    }
    fs::read_dir(parent).ok()?.flatten().find_map(|entry| {
        entry
            .file_name()
            .to_str()
            .is_some_and(|candidate| candidate.eq_ignore_ascii_case(name))
            .then(|| entry.path())
    })
}

fn deduplicate_paths(paths: &mut Vec<PathBuf>) {
    let mut seen = HashSet::new();
    paths.retain(|path| seen.insert(normalized_key(path)));
}

fn normalized_key(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/").to_lowercase()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_byte_order_and_mod_comments() {
        let fixture = r#"sprite_table
@referenceWidth=256
@referenceHeight=128
@animation test_uv
offset 3 0.0 step
@keyframes
0 0
10 20 # Custom Thing
30 20
@animation test_w
width 3 0.0 step
@keyframes
4
51 # Width label
8
pagea atlas 0 0 0 23 @anim=test_uv @anim=test_w
"#;
        let path = std::env::temp_dir().join(format!(
            "a2fo-tooltip-font-{}-{}.spr",
            std::process::id(),
            std::thread::current().name().unwrap_or("test")
        ));
        fs::write(&path, fixture).unwrap();
        let parsed = parse_font_sprite(&path).unwrap();
        fs::remove_file(path).ok();
        assert_eq!(parsed.reference_width, 256);
        assert_eq!(parsed.positions[1].2.as_deref(), Some("Custom Thing"));
        assert_eq!(parsed.widths[1].1.as_deref(), Some("Width label"));
        assert_eq!(parsed.texture_name, "atlas");
        assert_eq!(parsed.line_height, 23);
    }

    #[test]
    fn recognizes_fo4_and_roots_as_different_font_profiles() {
        let mut positions = vec![(0, 0, None); 224];
        let mut widths = vec![(1.0, None); 224];
        positions[0xA1 - 32] = (2, 127, None);
        positions[0xA8 - 32] = (136, 127, None);
        widths[0xA1 - 32].0 = 112.0;
        let profile = detect_profile(
            &positions,
            &widths,
            1024,
            Path::new("/Data/Mods/Fleet Ops 4.0/Dynamic_Localized_Strings.h"),
        );
        assert_eq!(profile, FontProfile::FleetOps4);
        assert_eq!(profile_label(profile, 0xB6), Some("(Avatar)"));

        positions[0xA8 - 32] = (162, 127, None);
        widths[0xA1 - 32].0 = 120.0;
        let profile = detect_profile(
            &positions,
            &widths,
            1024,
            Path::new("/Data/Dynamic_Localized_Strings.h"),
        );
        assert_eq!(profile, FontProfile::Roots);
        assert_eq!(profile_label(profile, 0xB7), Some("(Avatar)"));

        let profile = detect_profile(
            &positions,
            &widths,
            1024,
            Path::new("/Data/Mods/Custom/Dynamic_Localized_Strings.h"),
        );
        assert_eq!(profile, FontProfile::Unknown);
        assert_eq!(profile_label(profile, 0xB7), None);
    }

    #[test]
    fn rewrites_only_the_selected_uv_and_width_keyframes() {
        let fixture = "@referenceWidth=256\r\n@referenceHeight=128\r\n@animation font_uv\r\n@keyframes\r\n0 0 # space\r\n10 0 # old\r\n@animation font_w\r\n@keyframes\r\n4 # space\r\n8 # old\r\npage atlas 0 0 0 23 @anim=font_uv @anim=font_w\r\n";
        let path = std::env::temp_dir().join(format!(
            "a2fo-tooltip-font-rewrite-{}-{}.spr",
            std::process::id(),
            std::thread::current().name().unwrap_or("test")
        ));
        fs::write(&path, fixture).unwrap();
        let updated = rewrite_glyph_keyframes(&path, 0x21, 91, 102, 37.0, "New # Glyph").unwrap();
        fs::remove_file(path).ok();
        let text = String::from_utf8(updated).unwrap();
        assert!(text.contains("0 0 # space\r\n91\t102\t# New  Glyph\r\n"));
        assert!(text.contains("4 # space\r\n37\t# New  Glyph\r\n"));
    }

    #[test]
    fn rewriting_an_lf_sprite_normalizes_the_table_to_crlf() {
        let fixture = "@referenceWidth=256\n@referenceHeight=128\n@animation font_uv\n@keyframes\n0 0 # space\n10 0 # old\n@animation font_w\n@keyframes\n4 # space\n8 # old\npage atlas 0 0 0 23 @anim=font_uv @anim=font_w\n";
        let path = std::env::temp_dir().join(format!(
            "a2fo-tooltip-font-lf-rewrite-{}-{}.spr",
            std::process::id(),
            std::thread::current().name().unwrap_or("test")
        ));
        fs::write(&path, fixture).unwrap();
        let updated = rewrite_glyph_keyframes(&path, 0x21, 91, 102, 37.0, "New Glyph").unwrap();
        fs::remove_file(path).ok();
        let text = String::from_utf8(updated).unwrap();
        assert!(text.contains("\r\n"));
        assert!(!text.replace("\r\n", "").contains('\n'));
    }
}
