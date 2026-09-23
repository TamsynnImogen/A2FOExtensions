use anyhow::{Context, Result};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct OdfTooltipReference {
    pub path: PathBuf,
    pub field: String,
    pub key: String,
    pub line: usize,
    pub unit_name: Option<String>,
}

pub fn load_tooltip_references(path: &Path) -> Result<Vec<OdfTooltipReference>> {
    let bytes = fs::read(path).with_context(|| format!("Read ODF {}", path.display()))?;
    let text = String::from_utf8_lossy(&bytes);
    let mut assignments = Vec::<(String, String, usize)>::new();
    let mut in_block_comment = false;

    for (index, raw_line) in text.lines().enumerate() {
        let line = strip_comments(raw_line, &mut in_block_comment);
        let Some((field, value)) = parse_assignment(&line) else {
            continue;
        };
        assignments.push((field, value, index + 1));
    }

    let unit_name = assignments
        .iter()
        .rev()
        .find(|(field, _, _)| field.eq_ignore_ascii_case("unitName"))
        .map(|(_, value, _)| value.clone())
        .filter(|value| !value.is_empty());

    Ok(assignments
        .into_iter()
        .filter(|(field, value, _)| {
            field.to_ascii_lowercase().ends_with("tooltip") && !value.is_empty()
        })
        .map(|(field, key, line)| OdfTooltipReference {
            path: path.to_path_buf(),
            field,
            key,
            line,
            unit_name: unit_name.clone(),
        })
        .collect())
}

pub fn looks_like_localization_key(value: &str) -> bool {
    !value.is_empty()
        && value.len() <= 499
        && value
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || b"_-.:/".contains(&byte))
        && (value.bytes().any(|byte| byte == b'_' || byte == b'-')
            || value.bytes().all(|byte| !byte.is_ascii_lowercase()))
}

fn parse_assignment(line: &str) -> Option<(String, String)> {
    let (field, raw_value) = line.split_once('=')?;
    let field = field.trim();
    if field.is_empty()
        || !field
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || byte == b'_')
    {
        return None;
    }
    let raw_value = raw_value.trim();
    let value = if let Some(quoted) = raw_value.strip_prefix('"') {
        decode_quoted(quoted)
    } else {
        raw_value
            .split_whitespace()
            .next()
            .unwrap_or("")
            .to_string()
    };
    Some((field.to_string(), value))
}

fn decode_quoted(value: &str) -> String {
    let mut output = String::new();
    let mut escaped = false;
    for character in value.chars() {
        if escaped {
            output.push(match character {
                'n' => '\n',
                'r' => '\r',
                't' => '\t',
                other => other,
            });
            escaped = false;
        } else if character == '\\' {
            escaped = true;
        } else if character == '"' {
            break;
        } else {
            output.push(character);
        }
    }
    if escaped {
        output.push('\\');
    }
    output
}

fn strip_comments(line: &str, in_block_comment: &mut bool) -> String {
    let bytes = line.as_bytes();
    let mut output = String::with_capacity(line.len());
    let mut index = 0;
    let mut quoted = false;
    let mut escaped = false;
    while index < bytes.len() {
        if *in_block_comment {
            if bytes[index..].starts_with(b"*/") {
                *in_block_comment = false;
                index += 2;
            } else {
                index += 1;
            }
            continue;
        }
        if !quoted && bytes[index..].starts_with(b"/*") {
            *in_block_comment = true;
            index += 2;
            continue;
        }
        if !quoted && bytes[index..].starts_with(b"//") {
            break;
        }
        let byte = bytes[index];
        output.push(byte as char);
        if quoted {
            if escaped {
                escaped = false;
            } else if byte == b'\\' {
                escaped = true;
            } else if byte == b'"' {
                quoted = false;
            }
        } else if byte == b'"' {
            quoted = true;
        }
        index += 1;
    }
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn discovers_all_tooltip_fields_and_ignores_comments() {
        let fixture = r#"
            unitName = "Akira Class"
            tooltip = "FED_AKIRA" // short
            verboseTooltip = "FED_AKIRA_V"
            officerTooltip = "FED_AKIRA_OFFICER"
            // tooltip = "COMMENTED_OUT"
            /* verboseTooltip = "BLOCKED" */
            race = "federation"
        "#;
        let path = std::env::temp_dir().join(format!(
            "a2fo-tooltip-odf-{}-{}.odf",
            std::process::id(),
            std::thread::current().name().unwrap_or("test")
        ));
        fs::write(&path, fixture).unwrap();
        let references = load_tooltip_references(&path).unwrap();
        fs::remove_file(path).ok();
        assert_eq!(references.len(), 3);
        assert_eq!(references[0].key, "FED_AKIRA");
        assert_eq!(references[2].field, "officerTooltip");
        assert_eq!(references[1].unit_name.as_deref(), Some("Akira Class"));
    }

    #[test]
    fn distinguishes_keys_from_literal_tooltips() {
        assert!(looks_like_localization_key("FED_AKIRA_V"));
        assert!(looks_like_localization_key("AUTOTOOLTIP-fakira.odf"));
        assert!(!looks_like_localization_key("Akira Class"));
        assert!(!looks_like_localization_key("Launches a probe."));
    }
}
