//! Portable configuration policies. All strings are byte sequences: game ODFs
//! and FPQ names are not required to be UTF-8.
use alloc::{collections::BTreeSet, vec::Vec};
use core::{ffi::c_void, slice};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}
type Emit = unsafe extern "C" fn(*mut c_void, u32, usize, *const u8, usize);
#[repr(C)]
#[derive(Clone, Copy)]
pub struct RaceInput {
    slot: i32,
    has_slot: u8,
    has_interface: u8,
    has_key: u8,
    has_name: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct RaceEntry {
    slot: i32,
    replace_key: u8,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Color {
    red: f32,
    green: f32,
    blue: f32,
    present: u8,
}
#[repr(C)]
#[derive(Default)]
pub struct ColorReport {
    indexed: u32,
    legacy: u32,
}
#[repr(C)]
pub struct ArtReport {
    valid: u32,
    invalid: u32,
    found: u32,
    emissive_bump_multiplier: f32,
    bump_light_bias: f32,
    emissive_diffuse_restore: f32,
}
fn bytes(span: Bytes) -> &'static [u8] {
    if span.size == 0 {
        return &[];
    }
    if span.data.is_null() || span.size > isize::MAX as usize {
        return &[];
    }
    unsafe { slice::from_raw_parts(span.data, span.size) }
}
fn input(ptr: *const u8, len: usize) -> &'static [u8] {
    bytes(Bytes {
        data: ptr,
        size: len,
    })
}
fn send(emit: Option<Emit>, context: *mut c_void, kind: u32, index: usize, value: &[u8]) {
    if let Some(callback) = emit {
        unsafe {
            callback(context, kind, index, value.as_ptr(), value.len());
        }
    }
}
unsafe extern "C" {
    fn tolower(value: i32) -> i32;
    fn isspace(value: i32) -> i32;
    fn isalnum(value: i32) -> i32;
    fn isdigit(value: i32) -> i32;
}
fn lower(b: u8) -> u8 {
    unsafe { tolower(b as i32) as u8 }
}
fn space(b: u8) -> bool {
    unsafe { isspace(b as i32) != 0 }
}
fn alnum(b: u8) -> bool {
    unsafe { isalnum(b as i32) != 0 }
}
fn digit(b: u8) -> bool {
    unsafe { isdigit(b as i32) != 0 }
}
fn lowered(b: &[u8]) -> Vec<u8> {
    b.iter().map(|&v| lower(v)).collect()
}
fn trim(mut b: &[u8]) -> &[u8] {
    while b.first().is_some_and(|v| space(*v)) {
        b = &b[1..];
    }
    while b.last().is_some_and(|v| space(*v)) {
        b = &b[..b.len() - 1];
    }
    b
}
fn find(h: &[u8], n: &[u8]) -> Option<usize> {
    h.windows(n.len()).position(|w| w == n)
}
fn rfind(h: &[u8], needle: u8) -> Option<usize> {
    h.iter().rposition(|v| *v == needle)
}
fn u32_at(b: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_le_bytes(b.get(at..at + 4)?.try_into().ok()?))
}
fn component(b: &[u8]) -> bool {
    !b.is_empty() && b != b"." && b != b".." && !b.iter().any(|v| matches!(v, b'/' | b'\\' | 0))
}
fn fpq_parse(
    b: &[u8],
    version: &mut u32,
    files: &mut u32,
    emit: Option<Emit>,
    ctx: *mut c_void,
) -> Result<(), &'static [u8]> {
    if b.len() < 28 || &b[..4] != b"FPQ\0" {
        return Err(b"missing FPQ header");
    }
    *version = u32_at(b, 4).ok_or(b"truncated FPQ header" as &[u8])?;
    let folder_count = u32_at(b, 8).ok_or(b"truncated FPQ header" as &[u8])?;
    *files = u32_at(b, 12).ok_or(b"truncated FPQ header" as &[u8])?;
    let capacity = u32_at(b, 16).ok_or(b"truncated FPQ header" as &[u8])?;
    let name_size = u32_at(b, 24).ok_or(b"truncated FPQ header" as &[u8])?;
    if *version != 1 && *version != 2 {
        return Err(b"unsupported FPQ version");
    }
    if capacity == 0 || !capacity.is_power_of_two() || capacity <= *files {
        return Err(b"invalid FPQ hash capacity");
    }
    if folder_count > 1_000_000 || *files > 1_000_000 || name_size > 128 * 1024 * 1024 {
        return Err(b"unreasonable FPQ metadata size");
    }
    let folders_at = (capacity as usize)
        .checked_mul(12)
        .and_then(|n| 28usize.checked_add(n))
        .ok_or(b"FPQ metadata layout overflow" as &[u8])?;
    let files_at = folders_at
        .checked_add(
            (folder_count as usize)
                .checked_mul(12)
                .ok_or(b"FPQ metadata layout overflow" as &[u8])?,
        )
        .ok_or(b"FPQ metadata layout overflow" as &[u8])?;
    let names_at = files_at
        .checked_add(
            (*files as usize)
                .checked_mul(25)
                .ok_or(b"FPQ metadata layout overflow" as &[u8])?,
        )
        .ok_or(b"FPQ metadata layout overflow" as &[u8])?;
    let end = names_at
        .checked_add(name_size as usize)
        .ok_or(b"FPQ metadata layout overflow" as &[u8])?;
    if end > b.len() {
        return Err(b"truncated FPQ metadata");
    }
    let name = |offset: u32, len: u32| -> Option<&[u8]> {
        if offset > name_size || len > name_size - offset {
            return None;
        }
        let value = &b[names_at + offset as usize..names_at + offset as usize + len as usize];
        component(value).then_some(value)
    };
    let mut folders: Vec<Vec<u8>> = Vec::with_capacity(folder_count as usize);
    for i in 0..folder_count as usize {
        let at = folders_at + i * 12;
        let parent = u32_at(b, at).ok_or(b"invalid FPQ folder record" as &[u8])?;
        let part = name(
            u32_at(b, at + 4).ok_or(b"invalid FPQ folder record" as &[u8])?,
            u32_at(b, at + 8).ok_or(b"invalid FPQ folder record" as &[u8])?,
        )
        .ok_or(b"invalid FPQ folder record" as &[u8])?;
        let full = if parent == u32::MAX {
            part.to_vec()
        } else if let Some(prefix) = folders.get(parent as usize) {
            let mut joined = prefix.clone();
            joined.push(b'\\');
            joined.extend_from_slice(part);
            joined
        } else {
            return Err(b"FPQ folder references a missing parent");
        };
        folders.push(full);
    }
    let mut unique = BTreeSet::new();
    for i in 0..*files as usize {
        let at = files_at + i * 25;
        let offset = u32_at(b, at + 12).ok_or(b"invalid FPQ file record" as &[u8])?;
        let length = u32_at(b, at + 16).ok_or(b"invalid FPQ file record" as &[u8])?;
        let folder = u32_at(b, at + 21).ok_or(b"invalid FPQ file record" as &[u8])?;
        let part = name(offset, length).ok_or(b"invalid FPQ file record" as &[u8])?;
        let part = lowered(part);
        if !part.ends_with(b".odf") || folder == u32::MAX {
            continue;
        }
        let Some(path) = folders.get(folder as usize) else {
            return Err(b"FPQ file references a missing folder");
        };
        let key = lowered(path);
        if key == b"odf" || key.starts_with(b"odf\\") {
            unique.insert(key);
        }
    }
    for (i, value) in unique.iter().enumerate() {
        send(emit, ctx, 0, i, value);
    }
    Ok(())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_fpq(
    data: *const u8,
    len: usize,
    version: *mut u32,
    files: *mut u32,
    emit: Option<Emit>,
    ctx: *mut c_void,
) -> u8 {
    if version.is_null() || files.is_null() {
        return 0;
    }
    let (v, f) = unsafe { (&mut *version, &mut *files) };
    match fpq_parse(input(data, len), v, f, emit, ctx) {
        Ok(()) => 1,
        Err(error) => {
            send(emit, ctx, 1, 0, error);
            0
        }
    }
}
fn normalize_path(b: &[u8]) -> Vec<u8> {
    b.iter()
        .map(|v| {
            if *v == b'/' || *v == b'\\' {
                b'\\'
            } else {
                lower(*v)
            }
        })
        .collect()
}
fn odf_basename(b: &[u8]) -> Vec<u8> {
    let value = normalize_path(b);
    if let Some(slash) = rfind(&value, b'\\') {
        if slash + 1 >= value.len() {
            return Vec::new();
        }
        let dir = &value[..slash];
        if dir != b"odf" && !dir.starts_with(b"odf\\") {
            return Vec::new();
        }
        return if value[slash + 1..].ends_with(b".odf") {
            value[slash + 1..].to_vec()
        } else {
            Vec::new()
        };
    }
    if value.ends_with(b".odf") {
        value
    } else {
        Vec::new()
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_config_odf_basename(path: Bytes, emit: Option<Emit>, ctx: *mut c_void) {
    send(emit, ctx, 0, 0, &odf_basename(bytes(path)));
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_config_odf_alias_equivalent(path: Bytes, alias: Bytes) -> u8 {
    (normalize_path(bytes(path)) == normalize_path(bytes(alias))) as u8
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_race(
    inputs: *const RaceInput,
    count: usize,
    declared: usize,
    entries: *mut RaceEntry,
    fallback: *mut u8,
    playable: *mut i32,
) {
    if (inputs.is_null() || entries.is_null()) && count != 0
        || fallback.is_null()
        || playable.is_null()
    {
        return;
    }
    let list = if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(inputs, count) }
    };
    let out = if count == 0 {
        &mut []
    } else {
        unsafe { slice::from_raw_parts_mut(entries, count) }
    };
    let mut occupied = BTreeSet::new();
    let mut bad = false;
    for row in list {
        let index = row.slot as usize;
        if row.has_slot == 0 || row.slot < 0 || index >= declared || occupied.contains(&index) {
            bad = true;
            break;
        }
        occupied.insert(index);
    }
    unsafe {
        *fallback = bad as u8;
        *playable = 0;
    }
    for e in out.iter_mut() {
        *e = RaceEntry {
            slot: -1,
            replace_key: 0,
        };
    }
    if !bad {
        return;
    }
    let mut next = 0i32;
    for (row, e) in list.iter().zip(out.iter_mut()) {
        if row.has_interface != 0 {
            e.slot = next;
            next = next.wrapping_add(1);
            e.replace_key = (row.has_key == 0 && row.has_name != 0) as u8;
        }
    }
    unsafe {
        *playable = next;
    }
}
fn strip_comments(b: &[u8], quoted: bool) -> Vec<u8> {
    let mut out = Vec::with_capacity(b.len());
    let mut line = false;
    let mut block = false;
    let mut quote = false;
    let mut escaped = false;
    let mut i = 0;
    while i < b.len() {
        let c = b[i];
        let n = b.get(i + 1).copied().unwrap_or(0);
        if line {
            if c == b'\n' || c == b'\r' {
                line = false;
                out.push(c);
            } else {
                out.push(b' ');
            }
        } else if block {
            if c == b'*' && n == b'/' {
                out.extend_from_slice(b"  ");
                i += 1;
                block = false;
            } else {
                out.push(if c == b'\n' || c == b'\r' { c } else { b' ' });
            }
        } else if !quote && c == b'/' && n == b'/' {
            out.extend_from_slice(b"  ");
            i += 1;
            line = true;
        } else if !quote && c == b'/' && n == b'*' {
            out.extend_from_slice(b"  ");
            i += 1;
            block = true;
        } else {
            out.push(c);
            if quoted {
                if quote {
                    if escaped {
                        escaped = false;
                    } else if c == b'\\' {
                        escaped = true;
                    } else if c == b'"' {
                        quote = false;
                    }
                } else if c == b'"' {
                    quote = true;
                }
            }
        }
        i += 1;
    }
    out
}
fn ident(b: u8) -> bool {
    alnum(b) || b == b'_'
}
fn upgrade(b: &[u8], tier: &mut u32) -> u32 {
    let source = strip_comments(b, false);
    let mut status = 0;
    let mut candidate = *tier;
    for stmt in source.split(|v| *v == b';') {
        let Some(eq) = stmt.iter().position(|v| *v == b'=') else {
            continue;
        };
        let mut end = eq;
        while end > 0 && !ident(stmt[end - 1]) {
            end -= 1;
        }
        let mut begin = end;
        while begin > 0 && ident(stmt[begin - 1]) {
            begin -= 1;
        }
        if &stmt[begin..end] != b"upgradePodMaximumTier" {
            continue;
        }
        let value = trim(&stmt[eq + 1..]);
        let mut at = 0;
        let mut num = 0u32;
        while at < value.len() && digit(value[at]) {
            num = num * 10 + (value[at] - b'0') as u32;
            if num > 1000 {
                break;
            }
            at += 1;
        }
        if at > 0 && at == value.len() && (3..=16).contains(&num) {
            candidate = num;
            status = 1;
        } else {
            status = 2;
        }
    }
    if status == 1 {
        *tier = candidate;
    }
    status
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_upgrade(source: Bytes, tier: *mut u32) -> u32 {
    if tier.is_null() {
        2
    } else {
        upgrade(bytes(source), unsafe { &mut *tier })
    }
}
fn refit_normalize(b: &[u8]) -> Vec<u8> {
    let mut v = trim(b);
    if v.len() >= 2
        && ((v[0] == b'"' && v[v.len() - 1] == b'"') || (v[0] == b'\'' && v[v.len() - 1] == b'\''))
    {
        v = &v[1..v.len() - 1];
    }
    v.to_vec()
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_config_refit_normalize(
    value: Bytes,
    emit: Option<Emit>,
    ctx: *mut c_void,
) {
    send(emit, ctx, 0, 0, &refit_normalize(bytes(value)));
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_refit_parse(
    keys: *const Bytes,
    values: *const Bytes,
    count: usize,
    emit: Option<Emit>,
    ctx: *mut c_void,
) {
    if count > 0 && (keys.is_null() || values.is_null()) {
        return;
    }
    let k = if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(keys, count) }
    };
    let v = if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(values, count) }
    };
    let lookup = |name: &[u8]| -> Option<&[u8]> {
        k.iter()
            .zip(v)
            .find(|(key, _)| lowered(bytes(**key)) == name)
            .map(|(_, val)| bytes(*val))
    };
    let mut seen: Vec<Vec<u8>> = Vec::new();
    for i in 0..16 {
        let mut name = b"refititem".to_vec();
        let mut digits = [0u8; 2];
        let d = if i >= 10 {
            digits[0] = b'1';
            digits[1] = b'0' + (i - 10) as u8;
            &digits[..]
        } else {
            digits[0] = b'0' + i as u8;
            &digits[..1]
        };
        name.extend_from_slice(d);
        if let Some(value) = lookup(&name) {
            let value = refit_normalize(value);
            if !value.is_empty() && !seen.iter().any(|p| lowered(p) == lowered(&value)) {
                send(emit, ctx, 0, seen.len(), &value);
                seen.push(value);
            }
        }
    }
    for (kind, key) in [
        b"refithardpoint".as_slice(),
        b"buildhardpoint",
        b"classlabel",
    ]
    .iter()
    .enumerate()
    {
        if let Some(value) = lookup(key) {
            send(emit, ctx, kind as u32 + 1, 0, &refit_normalize(value));
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_config_refit_supported(refit: Bytes, build: Bytes, label: Bytes) -> u8 {
    (!bytes(refit).is_empty()
        && !bytes(build).is_empty()
        && lowered(bytes(label)) == b"shipyard"
        && lowered(bytes(refit)) == lowered(bytes(build))) as u8
}
fn edit_normalize(value: &[u8]) -> Vec<u8> {
    let source = trim(value);
    let basename = source
        .rsplit(|v| *v == b'/' || *v == b'\\')
        .next()
        .unwrap_or(&[]);
    let mut result = lowered(basename);
    if !result.is_empty() && !result.ends_with(b".odf") {
        result.extend_from_slice(b".odf");
    }
    result
}
fn edit_value(line: &[u8], equals: usize) -> Option<Vec<u8>> {
    let source = line.get(equals + 1..)?;
    let source = source
        .iter()
        .position(|v| !space(*v))
        .map(|at| &source[at..])?;
    if source[0] != b'"' {
        let mut value = trim(source);
        if value.last() == Some(&b';') {
            value = trim(&value[..value.len() - 1]);
        }
        return (!value.is_empty()).then(|| value.to_vec());
    }
    let mut value = Vec::new();
    let mut escaped = false;
    for &c in &source[1..] {
        if escaped {
            value.push(match c {
                b'n' => b'\n',
                b'r' => b'\r',
                b't' => b'\t',
                _ => c,
            });
            escaped = false;
        } else if c == b'\\' {
            escaped = true;
        } else if c == b'"' {
            return Some(value);
        } else {
            value.push(c);
        }
    }
    None
}
fn edit_numbered(id: &[u8], prefix: &[u8]) -> Option<usize> {
    if !id.starts_with(prefix) || id.len() <= prefix.len() {
        return None;
    }
    let mut number = 0usize;
    for &d in &id[prefix.len()..] {
        if !digit(d) {
            return None;
        }
        number = number * 10 + (d - b'0') as usize;
        if number > 12 {
            return None;
        }
    }
    (number != 0).then_some(number - 1)
}
fn edit_bool(value: &[u8]) -> Option<bool> {
    let v = lowered(value);
    match v.as_slice() {
        b"true" | b"yes" | b"on" => return Some(true),
        b"false" | b"no" | b"off" => return Some(false),
        _ => {}
    }
    // The legacy numeric fallback uses strtol(lowered.c_str()). An embedded
    // NUL terminates that spelling, while the named true/false checks above
    // still compare the complete std::string.
    let mut b = match v.iter().position(|x| *x == 0) {
        Some(end) => &v[..end],
        None => v.as_slice(),
    };
    while b.first().is_some_and(|x| space(*x)) {
        b = &b[1..];
    }
    if b.first().is_some_and(|x| *x == b'+' || *x == b'-') {
        b = &b[1..];
    }
    if b.is_empty() || !b.iter().all(|x| digit(*x)) {
        return None;
    }
    Some(b.iter().any(|x| *x != b'0'))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_edit_submenu(items: *const Bytes, count: usize) -> u8 {
    if items.is_null() && count != 0 {
        return 0;
    }
    let list = if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(items, count) }
    };
    list.iter().any(|v| !bytes(*v).is_empty()) as u8
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_config_edit_normalize(
    value: Bytes,
    emit: Option<Emit>,
    ctx: *mut c_void,
) {
    send(emit, ctx, 0, 0, &edit_normalize(bytes(value)));
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_config_edit_parse(
    contents: Bytes,
    source_name: Bytes,
    emit: Option<Emit>,
    ctx: *mut c_void,
) -> u8 {
    let name = edit_normalize(bytes(source_name));
    if name.is_empty() {
        send(emit, ctx, 5, 0, b"empty ODF filename");
        return 0;
    }
    send(emit, ctx, 0, 0, &name);
    let source = strip_comments(bytes(contents), true);
    let mut begin = 0;
    while begin <= source.len() {
        let end = source[begin..]
            .iter()
            .position(|v| *v == b'\n' || *v == b'\r')
            .map(|x| begin + x)
            .unwrap_or(source.len());
        let line = &source[begin..end];
        if let Some(eq) = line.iter().position(|v| *v == b'=') {
            let mut id_end = eq;
            while id_end > 0 && space(line[id_end - 1]) {
                id_end -= 1;
            }
            let mut id_begin = id_end;
            while id_begin > 0 && ident(line[id_begin - 1]) {
                id_begin -= 1;
            }
            let id = lowered(&line[id_begin..id_end]);
            if let Some(value) = edit_value(line, eq) {
                if id == b"menutitle" {
                    send(emit, ctx, 1, 0, &value);
                } else if let Some(i) = edit_numbered(&id, b"builditem") {
                    send(emit, ctx, 2, i, &edit_normalize(&value));
                } else if let Some(i) = edit_numbered(&id, b"item") {
                    send(emit, ctx, 3, i, &value);
                } else if id == b"forcetoneutral"
                    && let Some(v) = edit_bool(&value)
                {
                    send(emit, ctx, 4, 0, &[v as u8]);
                }
            }
        }
        if end == source.len() {
            break;
        }
        begin = end + 1;
        if begin < source.len() && source[end] == b'\r' && source[begin] == b'\n' {
            begin += 1;
        }
    }
    1
}
type ParseFloat = unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut usize, *mut f32) -> u8;
fn parse_float(
    parser: Option<ParseFloat>,
    ctx: *mut c_void,
    source: &[u8],
) -> Option<(f32, usize)> {
    let callback = parser?;
    let mut used = 0;
    let mut value = 0.0;
    (unsafe { callback(ctx, source.as_ptr(), source.len(), &mut used, &mut value) } != 0
        && used <= source.len())
    .then_some((value, used))
}
fn rgb(value: &[u8], parser: Option<ParseFloat>, ctx: *mut c_void) -> Option<[f32; 3]> {
    let mut input = trim(value);
    if let Some(nul) = input.iter().position(|v| *v == 0) {
        input = &input[..nul];
    }
    let mut components = [0.0; 3];
    for item in &mut components {
        input = input
            .iter()
            .position(|v| !space(*v))
            .map(|i| &input[i..])
            .unwrap_or(&[]);
        let (number, used) = parse_float(parser, ctx, input)?;
        *item = number;
        input = &input[used..];
        if input.first().is_some_and(|v| *v == b'f' || *v == b'F') {
            input = &input[1..];
        }
    }
    input = trim(input);
    if input.first() == Some(&b';') {
        input = trim(&input[1..]);
    }
    input.is_empty().then_some(components)
}
fn team_layer(
    source: &[u8],
    parser: Option<ParseFloat>,
    ctx: *mut c_void,
    output: &mut [Color],
    report: &mut ColorReport,
) {
    let names: [&[u8]; 13] = [
        b"white", b"red", b"blue", b"green", b"yellow", b"purple", b"cyan", b"brown", b"orange",
        b"pink", b"magenta", b"gray", b"black",
    ];
    let mut indexed = [None; 16];
    let mut legacy = [None; 13];
    let clean = strip_comments(source, false);
    let mut begin = 0;
    while begin <= clean.len() {
        let end = clean[begin..]
            .iter()
            .position(|v| *v == b'\n' || *v == b'\r')
            .map(|v| begin + v)
            .unwrap_or(clean.len());
        let line = &clean[begin..end];
        if let Some(eq) = line.iter().position(|v| *v == b'=') {
            let key = lowered(trim(&line[..eq]));
            if let Some([red, green, blue]) = rgb(&line[eq + 1..], parser, ctx) {
                let value = Color {
                    red,
                    green,
                    blue,
                    present: 1,
                };
                if key.len() == 9 && key.starts_with(b"mpcolor") && digit(key[7]) && digit(key[8]) {
                    let i =
                        ((key[7] - b'0') as usize * 10 + (key[8] - b'0') as usize).wrapping_sub(1);
                    if i < 16 {
                        indexed[i] = Some(value);
                    }
                } else if let Some(i) = names.iter().position(|n| *n == key) {
                    legacy[i] = Some(value);
                } else if key == b"grey" {
                    legacy[11] = Some(value);
                }
            }
        }
        if end == clean.len() {
            break;
        }
        begin = end + 1;
        if begin < clean.len() && clean[end] == b'\r' && clean[begin] == b'\n' {
            begin += 1;
        }
    }
    for i in 0..16 {
        if let Some(color) = indexed[i] {
            output[i] = color;
            report.indexed += 1;
        } else if i < 13
            && let Some(color) = legacy[i]
        {
            output[i] = color;
            report.legacy += 1;
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_team(
    source: Bytes,
    colors: *mut Color,
    report: *mut ColorReport,
    parser: Option<ParseFloat>,
    ctx: *mut c_void,
) {
    if colors.is_null() || report.is_null() {
        return;
    }
    team_layer(
        bytes(source),
        parser,
        ctx,
        unsafe { slice::from_raw_parts_mut(colors, 16) },
        unsafe { &mut *report },
    );
}
fn art_setting(id: &[u8]) -> usize {
    match lowered(id).as_slice() {
        b"a2fo_emissive_suffix" | b"emissivetexturesuffix" => 0,
        b"a2fo_bump_suffix" | b"bumptexturesuffix" => 1,
        b"a2fo_specular_suffix" | b"speculartexturesuffix" => 2,
        b"a2fo_emissive_bump_multiplier" | b"emissivebumpmultiplier" => 3,
        b"a2fo_bump_light_bias" | b"bumplightbias" => 4,
        b"a2fo_emissive_diffuse_restore" | b"emissivediffuserestore" => 5,
        _ => 6,
    }
}
fn art_quoted(source: &[u8], mut at: usize) -> Option<Vec<u8>> {
    while at < source.len() && space(source[at]) {
        at += 1;
    }
    if source.get(at) != Some(&b'"') {
        return None;
    }
    at += 1;
    let mut parsed = Vec::new();
    while at < source.len() && source[at] != b'"' {
        let c = source[at];
        if !(0x20..=0x7e).contains(&c) || c == b'\\' {
            return None;
        }
        parsed.push(c);
        if parsed.len() > 64 {
            return None;
        }
        at += 1;
    }
    if at == source.len() {
        return None;
    }
    at += 1;
    while at < source.len() && !matches!(source[at], b'\n' | b'\r' | b';') {
        if !space(source[at]) {
            return None;
        }
        at += 1;
    }
    parsed
        .iter()
        .all(|v| alnum(*v) || *v == b'_' || *v == b'-')
        .then_some(parsed)
}
fn art_float(
    source: &[u8],
    mut at: usize,
    max: f32,
    parser: Option<ParseFloat>,
    ctx: *mut c_void,
) -> Option<f32> {
    while at < source.len() && space(source[at]) && !matches!(source[at], b'\n' | b'\r') {
        at += 1;
    }
    if at == source.len() {
        return None;
    }
    let (value, used) = parse_float(parser, ctx, &source[at..])?;
    if value < 0.0 || value > max {
        return None;
    }
    at += used;
    if source.get(at).is_some_and(|v| *v == b'f' || *v == b'F') {
        at += 1;
    }
    while at < source.len() && !matches!(source[at], b'\n' | b'\r' | b';') {
        if !space(source[at]) {
            return None;
        }
        at += 1;
    }
    Some(value)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_art(
    source: Bytes,
    report: *mut ArtReport,
    emit: Option<Emit>,
    output_ctx: *mut c_void,
    parser: Option<ParseFloat>,
    parse_ctx: *mut c_void,
) {
    if report.is_null() {
        return;
    }
    let report = unsafe { &mut *report };
    let source = strip_comments(bytes(source), true);
    let mut at = 0;
    while at < source.len() {
        if !ident(source[at]) || digit(source[at]) {
            at += 1;
            continue;
        }
        let begin = at;
        at += 1;
        while at < source.len() && ident(source[at]) {
            at += 1;
        }
        let setting = art_setting(&source[begin..at]);
        if setting == 6 {
            continue;
        }
        let mut value_at = at;
        while value_at < source.len()
            && space(source[value_at])
            && !matches!(source[value_at], b'\n' | b'\r')
        {
            value_at += 1;
        }
        if source.get(value_at) == Some(&b'=') {
            value_at += 1;
        }
        let valid = if setting >= 3 {
            let max = [8.0, 1.0, 2.0][setting - 3];
            if let Some(value) = art_float(&source, value_at, max, parser, parse_ctx) {
                match setting {
                    3 => report.emissive_bump_multiplier = value,
                    4 => report.bump_light_bias = value,
                    _ => report.emissive_diffuse_restore = value,
                };
                true
            } else {
                false
            }
        } else if let Some(value) = art_quoted(&source, value_at) {
            send(emit, output_ctx, setting as u32, 0, &value);
            true
        } else {
            false
        };
        if valid {
            report.valid += 1;
            report.found |= 1 << setting;
        } else {
            report.invalid += 1;
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_config_disable_bump(source: Bytes, found: *mut u8) -> u8 {
    if !found.is_null() {
        unsafe {
            *found = 0;
        }
    }
    let lowered = lowered(bytes(source));
    let Some(open) = find(&lowered, b"<disable_bump>") else {
        return 0;
    };
    let after = open + b"<disable_bump>".len();
    let Some(close) = find(&lowered[after..], b"</disable_bump>") else {
        return 0;
    };
    let value = trim(&lowered[after..after + close]);
    if value != b"true" && value != b"false" && value != b"1" && value != b"0" {
        return 0;
    }
    if !found.is_null() {
        unsafe {
            *found = 1;
        }
    }
    (value == b"true" || value == b"1") as u8
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_config_texture_suffix(
    name: Bytes,
    suffix: Bytes,
    emit: Option<Emit>,
    ctx: *mut c_void,
) {
    let name = bytes(name);
    let suffix = bytes(suffix);
    let mut result = name.to_vec();
    if !suffix.is_empty() {
        let slash = name.iter().rposition(|v| *v == b'/' || *v == b'\\');
        let dot = rfind(name, b'.');
        let at = match dot {
            Some(i) if slash.is_none_or(|s| i > s + 1) => i,
            _ => name.len(),
        };
        result.splice(at..at, suffix.iter().copied());
    }
    send(emit, ctx, 0, 0, &result);
}
