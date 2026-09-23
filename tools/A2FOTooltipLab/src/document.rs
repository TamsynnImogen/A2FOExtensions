use anyhow::{bail, Context, Result};
use std::cmp::Reverse;
use std::collections::HashMap;
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};

use crate::glyphs::FontCatalog;

#[derive(Debug, Clone)]
pub struct TooltipEntry {
    pub key: String,
    pub editor_text: String,
    pub original_editor_text: String,
    pub line: usize,
    pub value_span: Option<(usize, usize)>,
    pub search_blob: String,
}

impl TooltipEntry {
    pub fn dirty(&self) -> bool {
        self.value_span.is_none() || self.editor_text != self.original_editor_text
    }

    pub fn refresh_search_blob(&mut self) {
        self.search_blob = format!("{}\n{}", self.key, self.editor_text).to_lowercase();
    }

    pub fn summary(&self) -> String {
        let mut summary = self
            .editor_text
            .lines()
            .find(|line| !line.trim().is_empty())
            .unwrap_or("")
            .trim()
            .to_string();
        if summary.chars().count() > 90 {
            summary = summary.chars().take(87).collect::<String>() + "...";
        }
        summary
    }
}

#[derive(Debug, Clone)]
pub struct TooltipDocument {
    pub path: PathBuf,
    pub source: Vec<u8>,
    pub entries: Vec<TooltipEntry>,
    pub newline: Vec<u8>,
    pub insertion_offset: usize,
    pub max_strings: Option<usize>,
    pub key_capacity: Option<usize>,
    pub translation_capacity: Option<usize>,
    pub warnings: Vec<String>,
}

#[derive(Debug)]
struct StringLiteral {
    content_start: usize,
    content_end: usize,
    line: usize,
    decoded: Vec<u8>,
}

impl TooltipDocument {
    pub fn load(path: &Path) -> Result<Self> {
        let source = fs::read(path).with_context(|| format!("Read {}", path.display()))?;
        Self::parse(path.to_path_buf(), source, None)
    }

    pub fn load_with_catalog(path: &Path, catalog: &FontCatalog) -> Result<Self> {
        let source = fs::read(path).with_context(|| format!("Read {}", path.display()))?;
        Self::parse(path.to_path_buf(), source, Some(catalog))
    }

    fn parse(path: PathBuf, source: Vec<u8>, catalog: Option<&FontCatalog>) -> Result<Self> {
        let table_start = find_table_start(&source).context(
            "Could not find the String_Table_Entry translations[MAX_STRINGS] initializer",
        )?;
        let literals = scan_string_literals(&source, table_start)?;
        if literals.len() % 2 != 0 {
            bail!(
                "Found an unmatched string literal near the translations table ({} literals)",
                literals.len()
            );
        }

        let mut entries = Vec::with_capacity(literals.len() / 2);
        let mut warnings = Vec::new();
        let mut seen = HashMap::<String, usize>::new();
        for pair in literals.chunks_exact(2) {
            let key = decode_key(&pair[0].decoded);
            if let Some(first_line) = seen.insert(key.to_ascii_lowercase(), pair[0].line) {
                warnings.push(format!(
                    "Duplicate key {key:?} on lines {first_line} and {}",
                    pair[0].line
                ));
            }
            let editor_text = bytes_to_editor(&pair[1].decoded, catalog);
            let mut entry = TooltipEntry {
                key,
                editor_text: editor_text.clone(),
                original_editor_text: editor_text,
                line: pair[0].line,
                value_span: Some((pair[1].content_start, pair[1].content_end)),
                search_blob: String::new(),
            };
            entry.refresh_search_blob();
            entries.push(entry);
        }

        let max_strings = parse_max_strings(&source);
        let key_capacity = parse_char_capacity(&source, "key_string");
        let translation_capacity = parse_char_capacity(&source, "translation_string");
        if let Some(limit) = max_strings {
            if entries.len() > limit {
                warnings.push(format!(
                    "The table has {} entries but MAX_STRINGS is only {limit}",
                    entries.len()
                ));
            }
        }
        for entry in &entries {
            if key_capacity.is_some_and(|capacity| {
                encode_plain_cp1252(&entry.key).is_ok_and(|key| key.len() >= capacity)
            }) {
                warnings.push(format!(
                    "Key {:?} on line {} exceeds key_string capacity {}",
                    entry.key,
                    entry.line,
                    key_capacity.expect("checked above")
                ));
            }
            if translation_capacity.is_some_and(|capacity| {
                editor_to_bytes(&entry.editor_text, b"\r\n")
                    .is_ok_and(|value| value.len() >= capacity)
            }) {
                warnings.push(format!(
                    "Value for {:?} on line {} exceeds translation_string capacity {}",
                    entry.key,
                    entry.line,
                    translation_capacity.expect("checked above")
                ));
            }
        }
        let insertion_offset = find_table_end(&source, table_start)
            .context("Could not find the closing brace of the translations table")?;
        let newline = b"\r\n".to_vec();
        Ok(Self {
            path,
            source,
            entries,
            newline,
            insertion_offset,
            max_strings,
            key_capacity,
            translation_capacity,
            warnings,
        })
    }

    pub fn dirty(&self) -> bool {
        self.entries.iter().any(TooltipEntry::dirty)
    }

    pub fn add_entry(&mut self, key: String, editor_text: String) -> Result<usize> {
        let key = key.trim().to_string();
        if key.is_empty() {
            bail!("The new entry needs a key");
        }
        if key.contains(['\r', '\n']) {
            bail!("A key cannot contain a line break");
        }
        let encoded_key =
            encode_plain_cp1252(&key).context("The key is not valid Fleet Ops text")?;
        if self
            .key_capacity
            .is_some_and(|capacity| encoded_key.len() >= capacity)
        {
            bail!("The key is too long for this file's key_string array");
        }
        if self
            .entries
            .iter()
            .any(|entry| entry.key.eq_ignore_ascii_case(&key))
        {
            bail!("The key {key:?} already exists");
        }
        if self
            .max_strings
            .is_some_and(|limit| self.entries.len() >= limit)
        {
            bail!("MAX_STRINGS must be increased before adding another entry");
        }
        let encoded_value = editor_to_bytes(&editor_text, &self.newline)
            .context("The new value is not valid Fleet Ops text")?;
        if self
            .translation_capacity
            .is_some_and(|capacity| encoded_value.len() >= capacity)
        {
            bail!("The value is too long for this file's translation_string array");
        }
        let mut entry = TooltipEntry {
            key,
            editor_text,
            original_editor_text: String::new(),
            line: 0,
            value_span: None,
            search_blob: String::new(),
        };
        entry.refresh_search_blob();
        self.entries.push(entry);
        Ok(self.entries.len() - 1)
    }

    pub fn validate(&self) -> Result<()> {
        for entry in &self.entries {
            if !entry.dirty() {
                continue;
            }
            let key =
                encode_plain_cp1252(&entry.key).with_context(|| format!("Key {:?}", entry.key))?;
            if self
                .key_capacity
                .is_some_and(|capacity| key.len() >= capacity)
            {
                bail!("Key {:?} exceeds the key_string capacity", entry.key);
            }
            let value = editor_to_bytes(&entry.editor_text, &self.newline)
                .with_context(|| format!("Entry {:?}", entry.key))?;
            if self
                .translation_capacity
                .is_some_and(|capacity| value.len() >= capacity)
            {
                bail!(
                    "Entry {:?} is {} bytes; translation_string allows at most {} plus its terminator",
                    entry.key,
                    value.len(),
                    self.translation_capacity
                        .expect("checked above")
                        .saturating_sub(1)
                );
            }
        }
        if self
            .max_strings
            .is_some_and(|limit| self.entries.len() > limit)
        {
            bail!("The entry count exceeds MAX_STRINGS");
        }
        Ok(())
    }

    pub fn save(&self) -> Result<Option<PathBuf>> {
        self.validate()?;
        let updated = self.render()?;
        if updated == self.source {
            return Ok(None);
        }
        let backup = backup_path(&self.path);
        let created_backup = if !backup.exists() {
            fs::copy(&self.path, &backup)
                .with_context(|| format!("Create backup {}", backup.display()))?;
            Some(backup)
        } else {
            None
        };
        fs::write(&self.path, updated).with_context(|| format!("Write {}", self.path.display()))?;
        Ok(created_backup)
    }

    fn render(&self) -> Result<Vec<u8>> {
        let mut edits = Vec::<(usize, usize, Vec<u8>)>::new();
        let mut new_entries = Vec::new();
        for entry in &self.entries {
            if !entry.dirty() {
                continue;
            }
            let logical = editor_to_bytes(&entry.editor_text, &self.newline)
                .with_context(|| format!("Encode {:?}", entry.key))?;
            let escaped = escape_c_string(&logical);
            if let Some((start, end)) = entry.value_span {
                edits.push((start, end, escaped));
            } else {
                new_entries.push((entry, escaped));
            }
        }
        if !new_entries.is_empty() {
            let mut block = Vec::new();
            for (entry, escaped) in new_entries {
                block.extend_from_slice(b"\t\"");
                let key = encode_plain_cp1252(&entry.key)?;
                block.extend_from_slice(&escape_c_string(&key));
                block.extend_from_slice(b"\",\t\"");
                block.extend_from_slice(&escaped);
                block.extend_from_slice(b"\",");
                block.extend_from_slice(&self.newline);
            }
            edits.push((self.insertion_offset, self.insertion_offset, block));
        }
        edits.sort_by_key(|edit| Reverse(edit.0));
        let mut output = self.source.clone();
        for (start, end, replacement) in edits {
            output.splice(start..end, replacement);
        }
        Ok(normalize_crlf_bytes(&output))
    }
}

fn find_table_start(bytes: &[u8]) -> Option<usize> {
    let marker = b"translations";
    let mut search_start = 0;
    while search_start + marker.len() <= bytes.len() {
        let relative = bytes[search_start..]
            .windows(marker.len())
            .position(|part| {
                part.iter()
                    .zip(marker)
                    .all(|(left, right)| left.eq_ignore_ascii_case(right))
            })?;
        let marker_at = search_start + relative;
        let mut after_marker = marker_at + marker.len();
        while bytes.get(after_marker).is_some_and(u8::is_ascii_whitespace) {
            after_marker += 1;
        }
        if bytes.get(after_marker) == Some(&b'[') {
            return bytes[after_marker..]
                .iter()
                .position(|byte| *byte == b'{')
                .map(|offset| after_marker + offset + 1);
        }
        search_start = marker_at + marker.len();
    }
    None
}

fn find_table_end(bytes: &[u8], table_start: usize) -> Option<usize> {
    bytes[table_start..]
        .windows(2)
        .rposition(|pair| pair == b"};")
        .map(|offset| table_start + offset)
        .or_else(|| {
            bytes[table_start..]
                .iter()
                .rposition(|byte| *byte == b'}')
                .map(|offset| table_start + offset)
        })
}

fn scan_string_literals(bytes: &[u8], start: usize) -> Result<Vec<StringLiteral>> {
    let mut output = Vec::new();
    let mut index = start;
    let mut line = 1 + bytes[..start].iter().filter(|byte| **byte == b'\n').count();
    while index < bytes.len() {
        if bytes[index] == b'}' {
            break;
        }
        if bytes[index..].starts_with(b"//") {
            while index < bytes.len() && bytes[index] != b'\n' {
                index += 1;
            }
            continue;
        }
        if bytes[index..].starts_with(b"/*") {
            index += 2;
            let mut closed = false;
            while index < bytes.len() {
                if bytes[index..].starts_with(b"*/") {
                    index += 2;
                    closed = true;
                    break;
                }
                if bytes[index] == b'\n' {
                    line += 1;
                }
                index += 1;
            }
            if !closed {
                bail!("Unterminated block comment near line {line}");
            }
            continue;
        }
        if bytes[index] != b'"' {
            if bytes[index] == b'\n' {
                line += 1;
            }
            index += 1;
            continue;
        }

        let literal_line = line;
        let content_start = index + 1;
        index += 1;
        let mut decoded = Vec::new();
        let mut closed = false;
        while index < bytes.len() {
            match bytes[index] {
                b'"' => {
                    output.push(StringLiteral {
                        content_start,
                        content_end: index,
                        line: literal_line,
                        decoded,
                    });
                    index += 1;
                    closed = true;
                    break;
                }
                b'\\' if index + 1 < bytes.len() => {
                    let escaped = bytes[index + 1];
                    match escaped {
                        b'n' => decoded.push(b'\n'),
                        b'r' => decoded.push(b'\r'),
                        b't' => decoded.push(b'\t'),
                        b'"' => decoded.push(b'"'),
                        b'\\' => decoded.push(b'\\'),
                        _ => {
                            decoded.push(b'\\');
                            decoded.push(escaped);
                        }
                    }
                    index += 2;
                }
                byte => {
                    decoded.push(byte);
                    if byte == b'\n' {
                        line += 1;
                    }
                    index += 1;
                }
            }
        }
        if !closed {
            bail!("Unterminated string literal beginning on line {literal_line}");
        }
    }
    Ok(output)
}

fn parse_max_strings(bytes: &[u8]) -> Option<usize> {
    for line in bytes.split(|byte| *byte == b'\n') {
        let text = String::from_utf8_lossy(line);
        let mut fields = text.split_whitespace();
        if fields.next() == Some("#define") && fields.next() == Some("MAX_STRINGS") {
            return fields.next()?.parse().ok();
        }
    }
    None
}

fn parse_char_capacity(bytes: &[u8], field: &str) -> Option<usize> {
    for line in bytes.split(|byte| *byte == b'\n') {
        let text = String::from_utf8_lossy(line);
        let Some(field_at) = text.find(field) else {
            continue;
        };
        let after_field = &text[field_at + field.len()..];
        let Some(open) = after_field.find('[') else {
            continue;
        };
        let after_open = &after_field[open + 1..];
        let Some(close) = after_open.find(']') else {
            continue;
        };
        if let Ok(capacity) = after_open[..close].trim().parse() {
            return Some(capacity);
        }
    }
    None
}

fn decode_key(bytes: &[u8]) -> String {
    bytes.iter().map(|byte| decode_cp1252(*byte)).collect()
}

pub fn bytes_to_editor(bytes: &[u8], catalog: Option<&FontCatalog>) -> String {
    let mut output = String::new();
    let mut index = 0;
    while index < bytes.len() {
        if bytes[index..].starts_with(b"\r\n") {
            output.push('\n');
            index += 2;
            continue;
        }
        let byte = bytes[index];
        if byte == b'\r' || byte == b'\n' {
            output.push('\n');
        } else if catalog.is_some_and(|font| font.is_special(byte)) {
            let label = catalog
                .and_then(|font| font.glyph(byte))
                .and_then(|glyph| glyph.label.as_deref());
            output.push_str(&glyph_token(byte, label));
        } else if byte < 0x20 && byte != b'\t' {
            output.push_str(&format!("[[{:02X}:control byte]]", byte));
        } else if let Some(character) = decode_cp1252_option(byte) {
            output.push(character);
        } else {
            output.push_str(&format!("[[{:02X}:raw byte]]", byte));
        }
        index += 1;
    }
    output
}

pub fn editor_to_bytes(text: &str, newline: &[u8]) -> Result<Vec<u8>> {
    let mut output = Vec::new();
    let mut rest = text;
    while !rest.is_empty() {
        if rest.starts_with("[[") {
            let Some(end) = rest[2..].find("]]") else {
                bail!("An inserted glyph token is missing its closing ]]");
            };
            let token = &rest[2..2 + end];
            let code = token
                .split(':')
                .next()
                .unwrap_or("")
                .trim()
                .trim_start_matches("0x")
                .trim_start_matches("0X");
            if code.len() != 2 || !code.bytes().all(|byte| byte.is_ascii_hexdigit()) {
                bail!("Invalid glyph token [[{token}]]; expected a two-digit byte code");
            }
            let byte = u8::from_str_radix(code, 16)?;
            if byte == 0 {
                bail!("Byte 00 cannot appear inside a game string");
            }
            output.push(byte);
            rest = &rest[2 + end + 2..];
            continue;
        }
        let character = rest.chars().next().expect("non-empty text");
        rest = &rest[character.len_utf8()..];
        if character == '\n' {
            output.extend_from_slice(newline);
        } else if character == '\r' {
            // Egui normalizes line endings; ignore a stray CR pasted before LF.
        } else if let Some(byte) = encode_cp1252(character) {
            output.push(byte);
        } else {
            bail!("Character {character:?} is not supported by Fleet Ops' single-byte text format");
        }
    }
    Ok(output)
}

pub(crate) fn normalize_crlf_bytes(bytes: &[u8]) -> Vec<u8> {
    let mut output = Vec::with_capacity(bytes.len());
    let mut index = 0;
    while index < bytes.len() {
        match bytes[index] {
            b'\r' => {
                output.extend_from_slice(b"\r\n");
                index += if bytes.get(index + 1) == Some(&b'\n') {
                    2
                } else {
                    1
                };
            }
            b'\n' => {
                output.extend_from_slice(b"\r\n");
                index += 1;
            }
            byte => {
                output.push(byte);
                index += 1;
            }
        }
    }
    output
}

pub fn glyph_token(byte: u8, label: Option<&str>) -> String {
    match label {
        Some(label) if !label.is_empty() => format!("[[{byte:02X}:{label}]]"),
        _ => format!("[[{byte:02X}:mod glyph]]"),
    }
}

fn escape_c_string(bytes: &[u8]) -> Vec<u8> {
    let mut output = Vec::with_capacity(bytes.len());
    for byte in bytes {
        match byte {
            b'"' => output.extend_from_slice(b"\\\""),
            b'\\' => output.extend_from_slice(b"\\\\"),
            _ => output.push(*byte),
        }
    }
    output
}

fn backup_path(path: &Path) -> PathBuf {
    let mut name = path
        .file_name()
        .map(OsString::from)
        .unwrap_or_else(|| OsString::from("Dynamic_Localized_Strings.h"));
    name.push(".a2fo-tooltip-lab.bak");
    path.with_file_name(name)
}

fn decode_cp1252(byte: u8) -> char {
    decode_cp1252_option(byte).unwrap_or('\u{FFFD}')
}

fn decode_cp1252_option(byte: u8) -> Option<char> {
    Some(match byte {
        0x80 => '€',
        0x81 | 0x8D | 0x8F | 0x90 | 0x9D => return None,
        0x82 => '‚',
        0x83 => 'ƒ',
        0x84 => '„',
        0x85 => '…',
        0x86 => '†',
        0x87 => '‡',
        0x88 => 'ˆ',
        0x89 => '‰',
        0x8A => 'Š',
        0x8B => '‹',
        0x8C => 'Œ',
        0x8E => 'Ž',
        0x91 => '‘',
        0x92 => '’',
        0x93 => '“',
        0x94 => '”',
        0x95 => '•',
        0x96 => '–',
        0x97 => '—',
        0x98 => '˜',
        0x99 => '™',
        0x9A => 'š',
        0x9B => '›',
        0x9C => 'œ',
        0x9E => 'ž',
        0x9F => 'Ÿ',
        value => char::from_u32(value as u32)?,
    })
}

fn encode_cp1252(character: char) -> Option<u8> {
    Some(match character {
        '\u{0000}'..='\u{007F}' => character as u8,
        '€' => 0x80,
        '‚' => 0x82,
        'ƒ' => 0x83,
        '„' => 0x84,
        '…' => 0x85,
        '†' => 0x86,
        '‡' => 0x87,
        'ˆ' => 0x88,
        '‰' => 0x89,
        'Š' => 0x8A,
        '‹' => 0x8B,
        'Œ' => 0x8C,
        'Ž' => 0x8E,
        '‘' => 0x91,
        '’' => 0x92,
        '“' => 0x93,
        '”' => 0x94,
        '•' => 0x95,
        '–' => 0x96,
        '—' => 0x97,
        '˜' => 0x98,
        '™' => 0x99,
        'š' => 0x9A,
        '›' => 0x9B,
        'œ' => 0x9C,
        'ž' => 0x9E,
        'Ÿ' => 0x9F,
        '\u{00A0}'..='\u{00FF}' => character as u8,
        _ => return None,
    })
}

fn encode_plain_cp1252(text: &str) -> Result<Vec<u8>> {
    text.chars()
        .map(|character| {
            encode_cp1252(character).with_context(|| {
                format!(
                    "Character {character:?} is not supported by Fleet Ops' single-byte text format"
                )
            })
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fixture() -> Vec<u8> {
        b"struct String_Table_Entry {\r\nchar key_string[20];\r\nchar translation_string[80];\r\n};\r\n#define MAX_STRINGS 4\r\nString_Table_Entry translations[MAX_STRINGS] =\r\n{\r\n\t\"ONE\", \"First\",\r\n\t// \"IGNORED\", \"comment\",\r\n\t\"TWO\", \"Line one\r\nLine two \\\"quoted\\\"\",\r\n};\r\n"
            .to_vec()
    }

    #[test]
    fn parses_multiline_values_and_ignores_comment_strings() {
        let document = TooltipDocument::parse(PathBuf::from("test.h"), fixture(), None).unwrap();
        assert_eq!(document.entries.len(), 2);
        assert_eq!(document.entries[1].key, "TWO");
        assert_eq!(
            document.entries[1].editor_text,
            "Line one\nLine two \"quoted\""
        );
        assert_eq!(document.max_strings, Some(4));
        assert_eq!(document.key_capacity, Some(20));
        assert_eq!(document.translation_capacity, Some(80));
    }

    #[test]
    fn edits_only_the_value_and_preserves_crlf() {
        let mut document =
            TooltipDocument::parse(PathBuf::from("test.h"), fixture(), None).unwrap();
        document.entries[0].editor_text = "Changed\nagain".to_string();
        let output = document.render().unwrap();
        assert!(output
            .windows(b"\"ONE\", \"Changed\r\nagain\",".len())
            .any(|part| part == b"\"ONE\", \"Changed\r\nagain\","));
        let comment = b"// \"IGNORED\"";
        assert!(output.windows(comment.len()).any(|part| part == comment));
    }

    #[test]
    fn editing_an_lf_source_normalizes_the_whole_file_to_crlf() {
        let source = fixture()
            .into_iter()
            .filter(|byte| *byte != b'\r')
            .collect();
        let mut document = TooltipDocument::parse(PathBuf::from("test.h"), source, None).unwrap();
        document.entries[0].editor_text = "Changed".to_string();
        let output = document.render().unwrap();
        assert!(output.windows(2).any(|part| part == b"\r\n"));
        assert!(!output
            .iter()
            .enumerate()
            .any(|(index, byte)| *byte == b'\n' && (index == 0 || output[index - 1] != b'\r')));
    }

    #[test]
    fn glyph_tokens_round_trip_exact_bytes() {
        let text = bytes_to_editor(&[b'A', 0x81, b'\n', 0x95], None);
        assert_eq!(text, "A[[81:raw byte]]\n•");
        assert_eq!(
            editor_to_bytes(&text, b"\r\n").unwrap(),
            [b'A', 0x81, b'\r', b'\n', 0x95]
        );
    }

    #[test]
    fn new_entries_are_inserted_before_the_table_end() {
        let mut document =
            TooltipDocument::parse(PathBuf::from("test.h"), fixture(), None).unwrap();
        document.add_entry("NEW".into(), "A value".into()).unwrap();
        let output = document.render().unwrap();
        let text = String::from_utf8(output).unwrap();
        assert!(text.contains("\t\"NEW\",\t\"A value\",\r\n};"));
    }

    #[test]
    fn pre_existing_duplicate_keys_do_not_block_an_unrelated_edit() {
        let source = fixture();
        let mut document = TooltipDocument::parse(PathBuf::from("test.h"), source, None).unwrap();
        document.entries[1].key = "ONE".to_string();
        document.entries[0].editor_text = "Changed".to_string();
        assert!(document.validate().is_ok());
    }
}
