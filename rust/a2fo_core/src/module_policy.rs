use alloc::collections::{BTreeMap, BTreeSet};
use alloc::{format, vec::Vec};
use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ByteArray {
    data: *const Bytes,
    size: usize,
}
#[repr(C)]
pub struct RuleInput {
    has_section: u8,
    required: ByteArray,
    rejected: ByteArray,
    active: ByteArray,
    diagnostics: ByteArray,
}
#[repr(C)]
pub struct InstalledInput {
    name: Bytes,
    filename: Bytes,
    path: Bytes,
}
#[repr(C)]
pub struct EntryOutput {
    key: Bytes,
    name: Bytes,
    filename: Bytes,
    path: Bytes,
    state: u32,
    installed: u8,
}
type BytesVisitor = unsafe extern "C" fn(*mut c_void, Bytes);
type RuleVisitor = unsafe extern "C" fn(*mut c_void, u32, Bytes);
type EntryVisitor = unsafe extern "C" fn(*mut c_void, *const EntryOutput);
type MetaVisitor = unsafe extern "C" fn(*mut c_void, u8, u8);

unsafe extern "C" {
    fn isspace(ch: i32) -> i32;
    fn isdigit(ch: i32) -> i32;
    fn tolower(ch: i32) -> i32;
}
fn space(ch: u8) -> bool {
    unsafe { isspace(i32::from(ch)) != 0 }
}
fn digit(ch: u8) -> bool {
    unsafe { isdigit(i32::from(ch)) != 0 }
}
fn lower(value: &[u8]) -> Vec<u8> {
    value
        .iter()
        .map(|&ch| unsafe { tolower(i32::from(ch)) as u8 })
        .collect()
}
fn trim(value: &[u8]) -> &[u8] {
    let start = value
        .iter()
        .position(|&ch| !space(ch))
        .unwrap_or(value.len());
    let end = value
        .iter()
        .rposition(|&ch| !space(ch))
        .map_or(start, |i| i + 1);
    &value[start..end]
}
fn unquote(value: &[u8]) -> &[u8] {
    let mut value = trim(value);
    if value.len() >= 2
        && ((value[0] == b'"' && value[value.len() - 1] == b'"')
            || (value[0] == b'\'' && value[value.len() - 1] == b'\''))
    {
        value = &value[1..value.len() - 1];
    }
    trim(value)
}
fn normalize(value: &[u8]) -> Vec<u8> {
    let mut name = unquote(value);
    if name.len() >= 4 && lower(&name[name.len() - 4..]) == b".dll" {
        name = trim(&name[..name.len() - 4]);
    }
    if name.is_empty()
        || name == b"."
        || name == b".."
        || name
            .iter()
            .any(|ch| matches!(ch, b'\\' | b'/' | b':' | b'"' | b'\''))
    {
        return Vec::new();
    }
    name.to_vec()
}
fn key(value: &[u8]) -> Vec<u8> {
    lower(&normalize(value))
}
fn indexed_key(value: &[u8], prefix: &[u8]) -> bool {
    let value = lower(trim(value));
    value.len() > prefix.len()
        && value.starts_with(prefix)
        && value[prefix.len()..].iter().all(|&ch| digit(ch))
}
fn section(value: &[u8]) -> Option<Vec<u8>> {
    let value = trim(value);
    (value.len() >= 2 && value[0] == b'[' && value[value.len() - 1] == b']')
        .then(|| lower(trim(&value[1..value.len() - 1])))
}
fn view(value: &[u8]) -> Bytes {
    Bytes {
        data: value.as_ptr(),
        size: value.len(),
    }
}
unsafe fn bytes<'a>(value: Bytes) -> Option<&'a [u8]> {
    if value.data.is_null() {
        return (value.size == 0).then_some(&[]);
    }
    // SAFETY: caller promises the view remains readable for this synchronous call.
    Some(unsafe { core::slice::from_raw_parts(value.data, value.size) })
}
unsafe fn array<'a, T>(data: *const T, size: usize) -> Option<&'a [T]> {
    if data.is_null() {
        return (size == 0).then_some(&[]);
    }
    Some(unsafe { core::slice::from_raw_parts(data, size) })
}
unsafe fn strings(input: ByteArray) -> Option<Vec<Vec<u8>>> {
    let items = unsafe { array(input.data, input.size)? };
    items
        .iter()
        .map(|&v| unsafe { bytes(v).map(Vec::from) })
        .collect()
}
fn append_unique(values: &mut Vec<Vec<u8>>, keys: &mut BTreeSet<Vec<u8>>, value: &[u8]) {
    let name = normalize(value);
    let name_key = key(&name);
    if !name_key.is_empty() && keys.insert(name_key) {
        values.push(name);
    }
}
#[derive(Default)]
struct Rules {
    has_section: bool,
    required: Vec<Vec<u8>>,
    rejected: Vec<Vec<u8>>,
    active: Vec<Vec<u8>>,
    diagnostics: Vec<Vec<u8>>,
}
fn parse(contents: &[u8]) -> Rules {
    let mut result = Rules::default();
    let (mut req, mut rej, mut act) = (BTreeSet::new(), BTreeSet::new(), BTreeSet::new());
    let mut in_modules = false;
    let mut lines: Vec<&[u8]> = contents.split(|&ch| ch == b'\n').collect();
    if contents.is_empty() || contents.last() == Some(&b'\n') {
        lines.pop();
    }
    for (index, mut line) in lines.into_iter().enumerate() {
        if line.last() == Some(&b'\r') {
            line = &line[..line.len() - 1];
        }
        let cleaned = trim(line);
        if cleaned.is_empty() || matches!(cleaned[0], b';' | b'#') {
            continue;
        }
        if let Some(name) = section(cleaned) {
            in_modules = name == b"modules";
            result.has_section |= in_modules;
            continue;
        }
        if !in_modules {
            continue;
        }
        let Some(equals) = cleaned.iter().position(|&ch| ch == b'=') else {
            continue;
        };
        let item_key = &cleaned[..equals];
        let value = unquote(&cleaned[equals + 1..]);
        let required = indexed_key(item_key, b"required");
        let rejected = indexed_key(item_key, b"reject");
        let active = indexed_key(item_key, b"active");
        if !(required || rejected || active) || value.is_empty() {
            continue;
        }
        let name = normalize(value);
        if name.is_empty() {
            let mut diagnostic =
                format!("Invalid module reference on info.ini line {}: ", index + 1).into_bytes();
            diagnostic.extend_from_slice(value);
            result.diagnostics.push(diagnostic);
            continue;
        }
        if required {
            append_unique(&mut result.required, &mut req, &name);
        } else if rejected {
            append_unique(&mut result.rejected, &mut rej, &name);
        } else {
            append_unique(&mut result.active, &mut act, &name);
        }
    }
    result
}
struct Installed {
    name: Vec<u8>,
    filename: Vec<u8>,
    path: Vec<u8>,
}
struct Entry {
    key: Vec<u8>,
    name: Vec<u8>,
    filename: Vec<u8>,
    path: Vec<u8>,
    state: u32,
    installed: bool,
}
struct Policy {
    managed: bool,
    valid: bool,
    entries: Vec<Entry>,
    diagnostics: Vec<Vec<u8>>,
}
fn evaluate(rules: &[Rules], installed: &[Installed]) -> Policy {
    let mut result = Policy {
        managed: false,
        valid: true,
        entries: Vec::new(),
        diagnostics: Vec::new(),
    };
    let (mut required, mut rejected, mut active) =
        (BTreeMap::new(), BTreeMap::new(), BTreeMap::new());
    for rule in rules {
        result.managed |= rule.has_section;
        result.diagnostics.extend(rule.diagnostics.iter().cloned());
        for name in &rule.required {
            required.entry(key(name)).or_insert_with(|| name.clone());
        }
        for name in &rule.rejected {
            rejected.entry(key(name)).or_insert_with(|| name.clone());
        }
        if rule.has_section {
            active.clear();
            for name in &rule.active {
                active.entry(key(name)).or_insert_with(|| name.clone());
            }
        }
    }
    let mut inventory = BTreeMap::new();
    for module in installed {
        let module_key = key(if module.name.is_empty() {
            &module.filename
        } else {
            &module.name
        });
        if !module_key.is_empty() {
            inventory.entry(module_key).or_insert(module);
        }
    }
    let mut emitted = BTreeSet::new();
    for (module_key, module) in inventory {
        let name = if module.name.is_empty() {
            normalize(&module.filename)
        } else {
            module.name.clone()
        };
        let req = required.contains_key(&module_key);
        let rej = rejected.contains_key(&module_key);
        let compat = module_key == b"a2fom3d";
        let state = if req && rej {
            result.valid = false;
            let mut d = b"Module is both required and rejected: ".to_vec();
            d.extend(&name);
            result.diagnostics.push(d);
            4
        } else if rej {
            3
        } else if req {
            2
        } else if !result.managed || active.contains_key(&module_key) || compat {
            if result.managed && compat && !active.contains_key(&module_key) {
                let mut d = b"Auto-enabled compatibility module: ".to_vec();
                d.extend(&name);
                result.diagnostics.push(d);
            }
            1
        } else {
            0
        };
        emitted.insert(module_key.clone());
        result.entries.push(Entry {
            key: module_key,
            name,
            filename: module.filename.clone(),
            path: module.path.clone(),
            state,
            installed: true,
        });
    }
    for (module_key, name) in &required {
        if emitted.contains(module_key) {
            continue;
        }
        let conflict = rejected.contains_key(module_key);
        let mut d = if conflict {
            b"Missing module is both required and rejected: ".to_vec()
        } else {
            b"Required module is not installed: ".to_vec()
        };
        d.extend(name);
        result.diagnostics.push(d);
        result.valid = false;
        emitted.insert(module_key.clone());
        result.entries.push(Entry {
            key: module_key.clone(),
            name: name.clone(),
            filename: Vec::new(),
            path: Vec::new(),
            state: if conflict { 4 } else { 6 },
            installed: false,
        });
    }
    for (module_key, name) in &active {
        if emitted.contains(module_key) || rejected.contains_key(module_key) {
            continue;
        }
        result.entries.push(Entry {
            key: module_key.clone(),
            name: name.clone(),
            filename: Vec::new(),
            path: Vec::new(),
            state: 5,
            installed: false,
        });
    }
    result.entries.sort_by(|a, b| a.key.cmp(&b.key));
    result
}
fn split_lines(contents: &[u8]) -> Vec<Vec<u8>> {
    let mut lines = Vec::new();
    for mut line in contents.split(|&ch| ch == b'\n') {
        if line.last() == Some(&b'\r') {
            line = &line[..line.len() - 1];
        }
        lines.push(line.to_vec());
    }
    lines
}
fn active_assignment(line: &[u8]) -> bool {
    let cleaned = trim(line);
    !cleaned.is_empty()
        && !matches!(cleaned[0], b';' | b'#')
        && cleaned
            .iter()
            .position(|&ch| ch == b'=')
            .is_some_and(|i| indexed_key(&cleaned[..i], b"active"))
}
fn insert_active(output: &mut Vec<Vec<u8>>, active: &[Vec<u8>], inserted: &mut bool) {
    if *inserted {
        return;
    }
    let mut keys = BTreeSet::new();
    let mut index = 0;
    for raw in active {
        let name = normalize(raw);
        let name_key = key(&name);
        if name_key.is_empty() || !keys.insert(name_key) {
            continue;
        }
        let mut line = format!("active{index} = \"").into_bytes();
        line.extend(name);
        line.push(b'"');
        output.push(line);
        index += 1;
    }
    *inserted = true;
}
fn rewrite(contents: &[u8], active: &[Vec<u8>]) -> Vec<u8> {
    let newline: &[u8] = if contents.windows(2).any(|v| v == b"\r\n") {
        b"\r\n"
    } else {
        b"\n"
    };
    let mut output = Vec::new();
    let (mut found, mut in_modules, mut inserted) = (false, false, false);
    for line in split_lines(contents) {
        if let Some(name) = section(&line) {
            if in_modules {
                insert_active(&mut output, active, &mut inserted);
            }
            in_modules = name == b"modules";
            found |= in_modules;
            output.push(line);
            continue;
        }
        if in_modules && active_assignment(&line) {
            continue;
        }
        output.push(line);
    }
    if in_modules {
        insert_active(&mut output, active, &mut inserted);
    }
    if !found {
        while output.last().is_some_and(Vec::is_empty) {
            output.pop();
        }
        if !output.is_empty() {
            output.push(Vec::new());
        }
        output.push(b"[modules]".to_vec());
        insert_active(&mut output, active, &mut inserted);
    }
    let mut result = output.join(newline);
    if !output.is_empty() && (contents.is_empty() || contents.last() == Some(&b'\n')) {
        result.extend(newline);
    }
    result
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_policy_normalize(
    input: Bytes,
    context: *mut c_void,
    visitor: BytesVisitor,
) -> u8 {
    let Some(input) = (unsafe { bytes(input) }) else {
        return 0;
    };
    let value = normalize(input);
    unsafe {
        visitor(context, view(&value));
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_policy_key(
    input: Bytes,
    context: *mut c_void,
    visitor: BytesVisitor,
) -> u8 {
    let Some(input) = (unsafe { bytes(input) }) else {
        return 0;
    };
    let value = key(input);
    unsafe {
        visitor(context, view(&value));
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_policy_parse(
    input: Bytes,
    context: *mut c_void,
    visitor: RuleVisitor,
) -> u8 {
    let Some(input) = (unsafe { bytes(input) }) else {
        return 0;
    };
    let rules = parse(input);
    if rules.has_section {
        unsafe {
            visitor(context, 0, view(&[]));
        }
    }
    for (kind, items) in [
        (1, rules.required),
        (2, rules.rejected),
        (3, rules.active),
        (4, rules.diagnostics),
    ] {
        for item in items {
            unsafe {
                visitor(context, kind, view(&item));
            }
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_policy_evaluate(
    rules: *const RuleInput,
    rule_count: usize,
    installed: *const InstalledInput,
    installed_count: usize,
    context: *mut c_void,
    meta: MetaVisitor,
    entry: EntryVisitor,
    diagnostic: BytesVisitor,
) -> u8 {
    let Some(raw_rules) = (unsafe { array(rules, rule_count) }) else {
        return 0;
    };
    let Some(raw_installed) = (unsafe { array(installed, installed_count) }) else {
        return 0;
    };
    let mut parsed = Vec::new();
    for raw in raw_rules {
        let (Some(required), Some(rejected), Some(active), Some(diagnostics)) = (
            unsafe { strings(raw.required) },
            unsafe { strings(raw.rejected) },
            unsafe { strings(raw.active) },
            unsafe { strings(raw.diagnostics) },
        ) else {
            return 0;
        };
        parsed.push(Rules {
            has_section: raw.has_section != 0,
            required,
            rejected,
            active,
            diagnostics,
        });
    }
    let mut modules = Vec::new();
    for raw in raw_installed {
        let (Some(name), Some(filename), Some(path)) = (
            unsafe { bytes(raw.name) },
            unsafe { bytes(raw.filename) },
            unsafe { bytes(raw.path) },
        ) else {
            return 0;
        };
        modules.push(Installed {
            name: name.to_vec(),
            filename: filename.to_vec(),
            path: path.to_vec(),
        });
    }
    let policy = evaluate(&parsed, &modules);
    unsafe {
        meta(context, policy.managed.into(), policy.valid.into());
    }
    for item in &policy.entries {
        let output = EntryOutput {
            key: view(&item.key),
            name: view(&item.name),
            filename: view(&item.filename),
            path: view(&item.path),
            state: item.state,
            installed: item.installed.into(),
        };
        unsafe {
            entry(context, &output);
        }
    }
    for item in &policy.diagnostics {
        unsafe {
            diagnostic(context, view(item));
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_policy_rewrite(
    input: Bytes,
    active: ByteArray,
    context: *mut c_void,
    visitor: BytesVisitor,
) -> u8 {
    let (Some(input), Some(active)) = (unsafe { bytes(input) }, unsafe { strings(active) }) else {
        return 0;
    };
    let value = rewrite(input, &active);
    unsafe {
        visitor(context, view(&value));
    }
    1
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn names_preserve_non_utf8_and_reject_paths() {
        assert_eq!(normalize(b"  '\xff.DLL'  "), b"\xff");
        assert!(normalize(b"../x.dll").is_empty());
        assert_eq!(key(b" A2FoM3D.DlL "), b"a2fom3d");
    }
    #[test]
    fn parse_and_evaluate_inheritance() {
        let parent = parse(b"[modules]\nrequired0 = Alpha.dll\nactive0=Beta\nreject0=bad/path\n");
        let child = parse(b"[MODULES]\nactive0 = Gamma\n");
        assert_eq!(parent.diagnostics.len(), 1);
        let policy = evaluate(
            &[parent, child],
            &[
                Installed {
                    name: b"Alpha".to_vec(),
                    filename: b"Alpha.dll".to_vec(),
                    path: vec![],
                },
                Installed {
                    name: b"Beta".to_vec(),
                    filename: vec![],
                    path: vec![],
                },
            ],
        );
        assert!(policy.managed && policy.valid);
        assert_eq!(
            policy.entries.iter().map(|e| e.state).collect::<Vec<_>>(),
            vec![2, 0, 5]
        );
    }
    #[test]
    fn rewrite_preserves_other_lines_and_crlf() {
        assert_eq!(
            rewrite(
                b"[mod]\r\nx=y\r\n[modules]\r\nrequired0=A\r\nactive0=B\r\n",
                &[b"A.dll".to_vec(), b"a".to_vec()]
            ),
            b"[mod]\r\nx=y\r\n[modules]\r\nrequired0=A\r\n\r\nactive0 = \"A\"\r\n"
        );
    }
}
