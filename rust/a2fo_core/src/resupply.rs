use alloc::vec::Vec;
// Byte-oriented resupply policy, matching the ODF scalar parser's ASCII rules.

use core::ffi::c_void;

const MAX_INPUT_BYTES: usize = 4094;
const MAX_RULES: usize = 256;
const TYPE_RULE: u8 = 0;
const ODF_RULE: u8 = 1;

type RuleVisitor = unsafe extern "C" fn(*mut c_void, u8, *const u8, usize, u8) -> u8;

#[derive(PartialEq, Eq)]
struct Rule {
    kind: u8,
    key: Vec<u8>,
    allow: bool,
}

struct Policy {
    default_allow: bool,
    rules: Vec<Rule>,
}

fn ascii_space(byte: u8) -> bool {
    matches!(byte, b' ' | b'\t' | b'\r' | b'\n' | 0x0b | 0x0c)
}

fn trim(mut bytes: &[u8]) -> &[u8] {
    while bytes.first().is_some_and(|&byte| ascii_space(byte)) {
        bytes = &bytes[1..];
    }
    while bytes.last().is_some_and(|&byte| ascii_space(byte)) {
        bytes = &bytes[..bytes.len() - 1];
    }
    bytes
}

fn dequote(raw: &[u8]) -> Option<&[u8]> {
    let mut value = trim(raw);
    if value.first() == Some(&b'"') || value.last() == Some(&b'"') {
        if value.len() < 2 || value.first() != Some(&b'"') || value.last() != Some(&b'"') {
            return None;
        }
        value = trim(&value[1..value.len() - 1]);
    }
    (!value.contains(&b'"')).then_some(value)
}

fn identifier(raw: &[u8]) -> Option<Vec<u8>> {
    if raw.is_empty() {
        return None;
    }
    let mut normalized = Vec::with_capacity(raw.len());
    for &byte in raw {
        let lower = byte.to_ascii_lowercase();
        if !matches!(lower, b'a'..=b'z' | b'0'..=b'9' | b'_' | b'-' | b'.') {
            return None;
        }
        normalized.push(lower);
    }
    (normalized.as_slice() != b"." && normalized.as_slice() != b"..").then_some(normalized)
}

fn normalize_type(raw: &[u8]) -> Option<Vec<u8>> {
    identifier(dequote(raw)?)
}

fn normalize_odf(raw: &[u8]) -> Option<Vec<u8>> {
    let value = dequote(raw)?;
    if value.is_empty() {
        return None;
    }
    let basename = value
        .iter()
        .rposition(|&byte| byte == b'/' || byte == b'\\')
        .map_or(value, |index| &value[index + 1..]);
    let mut normalized = identifier(basename)?;
    if normalized.len() > 4 && normalized.ends_with(b".odf") {
        normalized.truncate(normalized.len() - 4);
    }
    (normalized.as_slice() != b"." && normalized.as_slice() != b"..").then_some(normalized)
}

fn parse(raw: &[u8]) -> Option<Policy> {
    if raw.len() > MAX_INPUT_BYTES {
        return None;
    }
    let mut value = trim(raw);
    if value.first() == Some(&b'"') || value.last() == Some(&b'"') {
        if value.len() < 2 || value.first() != Some(&b'"') || value.last() != Some(&b'"') {
            return None;
        }
        value = &value[1..value.len() - 1];
    }
    if value.contains(&b'"') {
        return None;
    }

    let mut policy = Policy {
        default_allow: false,
        rules: Vec::new(),
    };
    let mut count = 0;
    loop {
        value = trim(value);
        if value.is_empty() {
            break;
        }
        count += 1;
        if count > MAX_RULES {
            return None;
        }
        let end = value
            .iter()
            .position(|&byte| ascii_space(byte))
            .unwrap_or(value.len());
        let mut token = &value[..end];
        value = &value[end..];
        if token == b"*" {
            policy.default_allow = true;
            continue;
        }
        let allow = if token.first() == Some(&b'!') {
            token = &token[1..];
            false
        } else {
            true
        };
        let colon = token.iter().position(|&byte| byte == b':')?;
        if colon == 0 || colon + 1 == token.len() {
            return None;
        }
        let prefix = identifier(&token[..colon])?;
        let name = &token[colon + 1..];
        let (kind, key) = if prefix == b"type" {
            (TYPE_RULE, normalize_type(name)?)
        } else if prefix == b"odf" {
            if name.contains(&b'/') || name.contains(&b'\\') {
                return None;
            }
            (ODF_RULE, normalize_odf(name)?)
        } else {
            return None;
        };
        if let Some(existing) = policy
            .rules
            .iter()
            .find(|rule| rule.kind == kind && rule.key == key)
        {
            if existing.allow != allow {
                return None;
            }
        } else {
            policy.rules.push(Rule { kind, key, allow });
        }
    }
    Some(policy)
}

fn allowed(
    valid: bool,
    default_allow: bool,
    type_rule: i8,
    odf_rule: i8,
    same_object: bool,
    can_resupply_self: bool,
) -> bool {
    if same_object {
        return can_resupply_self;
    }
    if !valid {
        return false;
    }
    if odf_rule >= 0 {
        return odf_rule != 0;
    }
    if type_rule >= 0 {
        return type_rule != 0;
    }
    default_allow
}

unsafe fn input<'a>(raw: *const u8, len: usize) -> Option<&'a [u8]> {
    if len == 0 {
        Some(&[])
    } else if raw.is_null() {
        None
    } else {
        // SAFETY: The caller guarantees a readable byte slice for the call.
        Some(unsafe { core::slice::from_raw_parts(raw, len) })
    }
}

unsafe fn write_normalized(
    raw: *const u8,
    raw_len: usize,
    output: *mut u8,
    capacity: usize,
    normalize: fn(&[u8]) -> Option<Vec<u8>>,
) -> usize {
    // SAFETY: Forwarding the caller's byte view; validity is checked by input.
    let Some(bytes) = (unsafe { input(raw, raw_len) }) else {
        return 0;
    };
    let Some(normalized) = normalize(bytes) else {
        return 0;
    };
    if normalized.len() > capacity || output.is_null() {
        return 0;
    }
    // SAFETY: The caller supplies a writable buffer of at least capacity bytes.
    unsafe { core::ptr::copy_nonoverlapping(normalized.as_ptr(), output, normalized.len()) };
    normalized.len()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_resupply_normalize_type(
    raw: *const u8,
    raw_len: usize,
    output: *mut u8,
    capacity: usize,
) -> usize {
    // SAFETY: Uses the caller's byte views only for this invocation.
    unsafe { write_normalized(raw, raw_len, output, capacity, normalize_type) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_resupply_normalize_odf(
    raw: *const u8,
    raw_len: usize,
    output: *mut u8,
    capacity: usize,
) -> usize {
    // SAFETY: Uses the caller's byte views only for this invocation.
    unsafe { write_normalized(raw, raw_len, output, capacity, normalize_odf) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_resupply_parse(
    raw: *const u8,
    raw_len: usize,
    context: *mut c_void,
    visitor: Option<RuleVisitor>,
    default_allow: *mut u8,
) -> u8 {
    if default_allow.is_null() {
        return 0;
    }
    // SAFETY: The output pointer is writable for the duration of the call.
    unsafe { *default_allow = 0 };
    // SAFETY: Uses the caller's input view only for this invocation.
    let Some(bytes) = (unsafe { input(raw, raw_len) }) else {
        return 0;
    };
    let Some(policy) = parse(bytes) else { return 0 };
    let Some(visitor) = visitor else { return 0 };
    for rule in &policy.rules {
        // SAFETY: The callback contract keeps context valid and consumes the key synchronously.
        if unsafe {
            visitor(
                context,
                rule.kind,
                rule.key.as_ptr(),
                rule.key.len(),
                u8::from(rule.allow),
            )
        } == 0
        {
            return 0;
        }
    }
    // SAFETY: The output pointer remains writable for the call.
    unsafe { *default_allow = u8::from(policy.default_allow) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_resupply_allowed(
    valid: u8,
    default_allow: u8,
    type_rule: i8,
    odf_rule: i8,
    same_object: u8,
    can_resupply_self: u8,
) -> u8 {
    u8::from(allowed(
        valid != 0,
        default_allow != 0,
        type_rule,
        odf_rule,
        same_object != 0,
        can_resupply_self != 0,
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn byte_normalization_and_quotes() {
        assert_eq!(
            normalize_type(b" \t\"FiGhTeR\" \n"),
            Some(b"fighter".to_vec())
        );
        assert_eq!(
            normalize_odf(b" \"C:\\Fleet\\Ships\\Fed_Scout.ODF\" "),
            Some(b"fed_scout".to_vec())
        );
        for bytes in [b"fighter/craft".as_slice(), b"\"fighter", b"f\xff"] {
            assert_eq!(normalize_type(bytes), None);
        }
        assert_eq!(normalize_odf(b"./"), None);
        assert_eq!(normalize_odf(b"f\xff.odf"), None);
    }

    #[test]
    fn parser_boundaries_duplicates_and_fail_closed() {
        let policy =
            parse(b"\"TYPE:Fighter type:fighter ODF:Fed_Scout.odf odf:fed_scout *\"").unwrap();
        assert!(policy.default_allow);
        assert_eq!(policy.rules.len(), 2);
        assert!(
            parse(b" \"   \" ")
                .is_some_and(|policy| !policy.default_allow && policy.rules.is_empty())
        );
        for bytes in [
            b"type:fighter !type:FIGHTER".as_slice(),
            b"!",
            b"!*",
            b"type:",
            b"odf:bad/name.odf",
            b"type:foo:bar",
            b"type:\"fighter\"",
            b"type:fighter \"odf:fed_scout\"",
        ] {
            assert!(parse(bytes).is_none(), "accepted {bytes:?}");
        }
        let mut boundary = b"type:fighter".to_vec();
        boundary.resize(MAX_INPUT_BYTES, b' ');
        assert!(parse(&boundary).is_some());
        boundary.push(b' ');
        assert!(parse(&boundary).is_none());
        let excessive = b"type:fighter ".repeat(257);
        assert!(parse(&excessive).is_none());
    }

    #[test]
    fn decision_precedence_and_self_override() {
        assert!(allowed(true, true, -1, -1, false, false));
        assert!(!allowed(true, true, 0, -1, false, false));
        assert!(allowed(true, false, 0, 1, false, false));
        assert!(!allowed(true, true, 1, 0, false, false));
        assert!(!allowed(false, true, 1, 1, false, false));
        assert!(allowed(false, false, 0, 0, true, true));
        assert!(!allowed(true, true, 1, 1, true, false));
    }
}
