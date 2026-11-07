//! Portable gameplay policy behind POD C ABI. Native containers remain in C++.
use alloc::{collections::BTreeMap, vec::Vec};
use core::ffi::c_void;
use core::slice;
unsafe extern "C" {
    fn isspace(value: i32) -> i32;
    fn isalnum(value: i32) -> i32;
    fn tolower(value: i32) -> i32;
}
fn crt_space(value: u8) -> bool {
    unsafe { isspace(value as i32) != 0 }
}
fn crt_alnum(value: u8) -> bool {
    unsafe { isalnum(value as i32) != 0 }
}
fn crt_lower(value: u8) -> u8 {
    unsafe { tolower(value as i32) as u8 }
}
fn eq_crt(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len() && a.iter().zip(b).all(|(x, y)| crt_lower(*x) == *y)
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Rect {
    x: f32,
    y: f32,
    width: f32,
    height: f32,
}
#[repr(C)]
pub struct Fill {
    source: Rect,
    destination: Rect,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OptionalNumber {
    present: u8,
    valid: u8,
    value: f32,
}
#[repr(C)]
pub struct CycleState {
    next_delay_index: usize,
    idle_reset_remaining: f32,
    fire_count: u64,
}
#[repr(C)]
pub struct ShieldConfig {
    display_mode: i32,
    positions: [i32; 4],
    configured: u8,
}
#[repr(C)]
#[derive(Default)]
pub struct ShieldReport {
    valid: u32,
    invalid: u32,
    display_found: u8,
    position_found: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    length: usize,
}
impl Bytes {
    fn value(self) -> &'static [u8] {
        bytes(self.data, self.length)
    }
}
#[repr(C)]
pub struct Field {
    name: Bytes,
    value: Bytes,
}
#[repr(C)]
pub struct Member {
    row: u32,
    count: u32,
    odf: Bytes,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Economics {
    costs: [i32; 10],
    seconds: f32,
}
type TextCallback = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type MemberCallback = unsafe extern "C" fn(*mut c_void, u32, *const u8, usize, u32);
type ClassCallback = unsafe extern "C" fn(*mut c_void, *const u8, usize) -> u32;
type EconomicsCallback = unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut Economics) -> u8;
fn trim_four(mut b: &[u8]) -> &[u8] {
    let space = |v| matches!(v, b' ' | b'\t' | b'\r' | b'\n');
    while !b.is_empty() && space(b[0]) {
        b = &b[1..];
    }
    while !b.is_empty() && space(b[b.len() - 1]) {
        b = &b[..b.len() - 1];
    }
    b
}
fn value_text(b: &[u8]) -> Vec<u8> {
    let trimmed = trim_four(b);
    if trimmed.len() >= 2
        && ((trimmed[0] == b'"' && trimmed[trimmed.len() - 1] == b'"')
            || (trimmed[0] == b'\'' && trimmed[trimmed.len() - 1] == b'\''))
    {
        trimmed[1..trimmed.len() - 1].to_vec()
    } else {
        trimmed.to_vec()
    }
}
fn odf_key(b: &[u8]) -> Vec<u8> {
    let mut result = value_text(b);
    result.make_ascii_lowercase();
    if result.len() > 4 && result.ends_with(b".odf") {
        result.truncate(result.len() - 4);
    }
    if result.is_empty()
        || result.len() > 127
        || result
            .iter()
            .any(|v| !v.is_ascii_lowercase() && !v.is_ascii_digit() && *v != b'_' && *v != b'-')
    {
        return Vec::new();
    }
    result
}
fn message(prefix: &[u8], suffix: &[u8]) -> Vec<u8> {
    let mut out = prefix.to_vec();
    out.extend_from_slice(suffix);
    out
}
fn row_message(prefix: &[u8], row: u32, suffix: &[u8]) -> Vec<u8> {
    let mut out = prefix.to_vec();
    if row >= 10 {
        out.push(b'0' + (row / 10) as u8);
    }
    out.push(b'0' + (row % 10) as u8);
    out.extend_from_slice(suffix);
    out
}
fn integer(value: &[u8], max: u32) -> Option<u32> {
    if value.is_empty() {
        return None;
    }
    let mut out = 0u32;
    for b in value {
        let digit = b.wrapping_sub(b'0');
        if digit > 9 {
            return None;
        }
        out = out.checked_mul(10)?.checked_add(digit as u32)?;
        if out > max {
            return None;
        }
    }
    Some(out)
}
fn shape(odf: &[u8], members: &[(u32, Vec<u8>, u32)]) -> Option<&'static [u8]> {
    if odf.is_empty() || odf_key(odf) != odf {
        return Some(b"squadron ODF must be a canonical basename");
    }
    if members.is_empty() || members.len() > 16 {
        return Some(b"a squadron needs 1 to 16 member rows");
    }
    let mut total = 0u32;
    let mut previous = None;
    for (row, member, count) in members {
        if *row >= 16 || previous.is_some_and(|p| *row <= p) {
            return Some(b"member rows must be unique, ascending indices 0 through 15");
        }
        if member.is_empty() || odf_key(member) != *member {
            return Some(b"member ODF must be a canonical basename");
        }
        if member == odf {
            return Some(b"a squadron cannot contain itself");
        }
        if *count == 0 || *count > 32 - total {
            return Some(b"member counts must be positive and total at most 32");
        }
        total += *count;
        previous = Some(*row);
    }
    None
}
fn report_error(context: *mut c_void, callback: TextCallback, error: &[u8]) {
    unsafe { callback(context, error.as_ptr(), error.len()) };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_odf_key(
    input: Bytes,
    context: *mut c_void,
    output: Option<TextCallback>,
) {
    if let Some(cb) = output {
        let value = odf_key(input.value());
        unsafe { cb(context, value.as_ptr(), value.len()) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_definition_size(
    members: *const Member,
    count: usize,
) -> u32 {
    if members.is_null() && count != 0 {
        return 0;
    }
    let mut total = 0u32;
    for member in if count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(members, count) }
    } {
        let Some(value) = total.checked_add(member.count) else {
            return 0;
        };
        total = value;
    }
    total
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_definition_size_add(
    total: u32,
    count: u32,
    overflow: *mut u8,
) -> u32 {
    match total.checked_add(count) {
        Some(value) => value,
        None => {
            if !overflow.is_null() {
                unsafe { *overflow = 1 };
            }
            0
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_parse_squadron(
    odf: Bytes,
    fields: *const Field,
    count: usize,
    context: *mut c_void,
    set_odf: Option<TextCallback>,
    add_member: Option<MemberCallback>,
    reinforce: *mut u8,
    set_error: Option<TextCallback>,
) {
    let (Some(set_odf), Some(add_member), Some(set_error)) = (set_odf, add_member, set_error)
    else {
        return;
    };
    if fields.is_null() && count != 0 {
        return;
    }
    let def_odf = odf_key(odf.value());
    let mut values = BTreeMap::<Vec<u8>, Vec<u8>>::new();
    let fields = if count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(fields, count) }
    };
    for field in fields {
        let mut name = trim_four(field.name.value()).to_vec();
        name.make_ascii_lowercase();
        if name != b"classlabel" && !name.starts_with(b"squad") {
            continue;
        }
        if values
            .insert(name.clone(), value_text(field.value.value()))
            .is_some()
        {
            report_error(
                context,
                set_error,
                &message(b"duplicate effective field: ", &name),
            );
            return;
        }
    }
    if !values
        .get(b"classlabel" as &[u8])
        .is_some_and(|v| v.eq_ignore_ascii_case(b"squadron"))
    {
        report_error(context, set_error, b"classLabel must be squadron");
        return;
    }
    let mut rows = BTreeMap::<u32, (Option<Vec<u8>>, Option<Vec<u8>>)>::new();
    let mut reinforce_value = 1u8;
    for (name, value) in &values {
        if name == b"classlabel" {
            continue;
        }
        if name == b"squadreinforceatyard" {
            if value != b"0" && value != b"1" {
                report_error(context, set_error, b"squadReinforceAtYard must be 0 or 1");
                return;
            }
            reinforce_value = yes(value == b"1");
            continue;
        }
        let is_count = name.starts_with(b"squadmembercount");
        let prefix: &[u8] = if is_count {
            b"squadmembercount"
        } else {
            b"squadmember"
        };
        let suffix = name.strip_prefix(prefix);
        let index = suffix.and_then(|s| integer(s, 15));
        if index.is_none() || row_message(b"", index.unwrap_or(0), b"") != suffix.unwrap_or(&[]) {
            report_error(
                context,
                set_error,
                &message(b"unknown squadron field or invalid row index: ", name),
            );
            return;
        }
        let entry = rows.entry(index.unwrap_or(0)).or_default();
        if is_count {
            entry.1 = Some(value.clone());
        } else {
            entry.0 = Some(value.clone());
        }
    }
    let mut members = Vec::new();
    for (row, (member, count_text)) in rows {
        let (Some(member), Some(count_text)) = (member, count_text) else {
            report_error(
                context,
                set_error,
                &row_message(
                    b"member row ",
                    row,
                    b" needs both squadMember and squadMemberCount",
                ),
            );
            return;
        };
        let Some(count) = integer(&count_text, 32).filter(|v| *v != 0) else {
            report_error(
                context,
                set_error,
                &row_message(b"invalid count for member row ", row, b""),
            );
            return;
        };
        members.push((row, odf_key(&member), count));
    }
    if let Some(error) = shape(&def_odf, &members) {
        report_error(context, set_error, error);
        return;
    }
    unsafe {
        set_odf(context, def_odf.as_ptr(), def_odf.len());
    }
    for (row, member, count) in members {
        unsafe { add_member(context, row, member.as_ptr(), member.len(), count) };
    }
    if !reinforce.is_null() {
        unsafe { *reinforce = reinforce_value };
    }
}
fn members_owned(members: *const Member, count: usize) -> Vec<(u32, Vec<u8>, u32)> {
    if members.is_null() {
        return Vec::new();
    }
    unsafe { slice::from_raw_parts(members, count) }
        .iter()
        .map(|m| (m.row, m.odf.value().to_vec(), m.count))
        .collect()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_validate_squadron(
    odf: Bytes,
    members: *const Member,
    count: usize,
    context: *mut c_void,
    resolver: Option<ClassCallback>,
    set_error: Option<TextCallback>,
) {
    let Some(set_error) = set_error else {
        return;
    };
    let owned = members_owned(members, count);
    if let Some(error) = shape(odf.value(), &owned) {
        report_error(context, set_error, error);
        return;
    }
    let Some(resolve) = resolver else {
        report_error(context, set_error, b"member class resolver is required");
        return;
    };
    for (_, member, _) in owned {
        let result = unsafe { resolve(context, member.as_ptr(), member.len()) };
        let prefix: &[u8] = match result {
            1 => continue,
            0 => b"missing member ODF: ",
            3 => b"nested squadrons are unsupported: ",
            _ => b"member is not a supported mobile Craft: ",
        };
        report_error(context, set_error, &message(prefix, &member));
        return;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_economics_squadron(
    odf: Bytes,
    members: *const Member,
    count: usize,
    context: *mut c_void,
    resolver: Option<EconomicsCallback>,
    output: *mut Economics,
    set_error: Option<TextCallback>,
) {
    let Some(set_error) = set_error else {
        return;
    };
    let owned = members_owned(members, count);
    if let Some(error) = shape(odf.value(), &owned) {
        report_error(context, set_error, error);
        return;
    }
    let Some(resolve) = resolver else {
        report_error(
            context,
            set_error,
            b"member economics resolver/output is required",
        );
        return;
    };
    if output.is_null() {
        report_error(
            context,
            set_error,
            b"member economics resolver/output is required",
        );
        return;
    }
    let mut total = Economics::default();
    let mut seconds = crate::legacy_float::Seconds::new();
    for (_, member, count) in owned {
        seconds.before_row();
        let mut item = Economics::default();
        if unsafe { resolve(context, member.as_ptr(), member.len(), &mut item) } == 0 {
            report_error(
                context,
                set_error,
                &message(b"missing member economics: ", &member),
            );
            return;
        }
        if !item.seconds.is_finite() || item.seconds < 0.0 {
            report_error(
                context,
                set_error,
                &message(b"invalid member build time: ", &member),
            );
            return;
        }
        if seconds.add(item.seconds, count as u64) {
            report_error(context, set_error, b"squad build time overflow");
            return;
        }
        for i in 0..10 {
            let cost = item.costs[i] as i64;
            if cost < 0 {
                report_error(
                    context,
                    set_error,
                    &message(b"negative member cost: ", &member),
                );
                return;
            }
            let sum = total.costs[i] as i64 + cost * count as i64;
            if sum > i32::MAX as i64 {
                report_error(context, set_error, b"squad resource cost overflow");
                return;
            }
            total.costs[i] = sum as i32;
        }
    }
    total.seconds = seconds.value();
    unsafe { *output = total };
}
fn identifier(b: u8) -> bool {
    crt_alnum(b) || b == b'_'
}
fn assignment_key(statement: &[u8], equals: usize) -> &[u8] {
    let mut end = equals;
    while end > 0 && !identifier(statement[end - 1]) {
        end -= 1;
    }
    let mut start = end;
    while start > 0 && identifier(statement[start - 1]) {
        start -= 1;
    }
    &statement[start..end]
}
fn parse_small_integer(statement: &[u8], equals: usize) -> Option<i32> {
    let mut cursor = equals + 1;
    while cursor < statement.len() && crt_space(statement[cursor]) {
        cursor += 1;
    }
    let sign =
        if cursor < statement.len() && (statement[cursor] == b'+' || statement[cursor] == b'-') {
            let result = if statement[cursor] == b'-' { -1 } else { 1 };
            cursor += 1;
            result
        } else {
            1
        };
    if cursor == statement.len() || !statement[cursor].is_ascii_digit() {
        return None;
    }
    let mut value = 0;
    while cursor < statement.len() && statement[cursor].is_ascii_digit() {
        value = value * 10 + (statement[cursor] - b'0') as i32;
        if value > 1000 {
            return None;
        }
        cursor += 1;
    }
    while cursor < statement.len() && crt_space(statement[cursor]) {
        cursor += 1;
    }
    if cursor != statement.len() {
        None
    } else {
        Some(sign * value)
    }
}
fn parse_shield_statement(statement: &[u8], config: &mut ShieldConfig, report: &mut ShieldReport) {
    let Some(equals) = statement.iter().position(|b| *b == b'=') else {
        return;
    };
    let key = assignment_key(statement, equals);
    let names: [&[u8]; 4] = [
        b"directionalshieldforwardposition",
        b"directionalshieldaftposition",
        b"directionalshieldportposition",
        b"directionalshieldstarboardposition",
    ];
    let display = eq_crt(key, b"directionalshielddisplaymode");
    let facing = names.iter().position(|name| eq_crt(key, name));
    if !display && facing.is_none() {
        return;
    }
    let Some(value) = parse_small_integer(statement, equals) else {
        report.invalid += 1;
        return;
    };
    if display {
        if !(1..=2).contains(&value) {
            report.invalid += 1;
            return;
        }
        config.display_mode = value;
        report.display_found = 1;
    } else {
        if !(0..=3).contains(&value) {
            report.invalid += 1;
            return;
        }
        let i = facing.unwrap_or(0);
        config.positions[i] = value;
        config.configured = 1;
        report.position_found[i] = 1;
    }
    report.valid += 1;
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_parse_shield_config(
    source: *const u8,
    length: usize,
    config: *mut ShieldConfig,
    report: *mut ShieldReport,
) {
    if report.is_null() {
        return;
    }
    let out = unsafe { &mut *report };
    *out = ShieldReport::default();
    if config.is_null() || (source.is_null() && length != 0) {
        out.invalid = 1;
        return;
    }
    let input = bytes(source, length);
    let mut clean = Vec::with_capacity(length);
    let mut i = 0;
    let mut line = false;
    let mut block = false;
    while i < input.len() {
        let cur = input[i];
        let next = input.get(i + 1).copied().unwrap_or(0);
        if line {
            if cur == b'\n' || cur == b'\r' {
                line = false;
                clean.push(cur);
            } else {
                clean.push(b' ');
            }
        } else if block {
            if cur == b'*' && next == b'/' {
                clean.extend_from_slice(b"  ");
                i += 1;
                block = false;
            } else {
                clean.push(if cur == b'\n' || cur == b'\r' {
                    cur
                } else {
                    b' '
                });
            }
        } else if cur == b'/' && next == b'/' {
            clean.extend_from_slice(b"  ");
            i += 1;
            line = true;
        } else if cur == b'/' && next == b'*' {
            clean.extend_from_slice(b"  ");
            i += 1;
            block = true;
        } else {
            clean.push(cur);
        }
        i += 1;
    }
    let cfg = unsafe { &mut *config };
    for statement in clean.split(|b| *b == b';') {
        parse_shield_statement(statement, cfg, out);
    }
}
fn valid_mapping(cfg: &ShieldConfig) -> bool {
    let mut used = [false; 4];
    for p in cfg.positions {
        if !(0..4).contains(&p) || used[p as usize] {
            return false;
        }
        used[p as usize] = true;
    }
    true
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_valid_shield_mapping(config: *const ShieldConfig) -> u8 {
    yes(unsafe { config.as_ref() }.is_some_and(valid_mapping))
}
fn usable(r: &Rect) -> bool {
    r.x.is_finite()
        && r.y.is_finite()
        && r.width.is_finite()
        && r.height.is_finite()
        && r.width > 0.0
        && r.height > 0.0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_remap_shield(
    config: *const ShieldConfig,
    input: *const Rect,
    indices: *mut usize,
) -> u8 {
    if config.is_null() || input.is_null() || indices.is_null() {
        return 0;
    }
    let cfg = unsafe { &*config };
    if cfg.configured == 0 || !valid_mapping(cfg) {
        return 0;
    }
    let rects = unsafe { slice::from_raw_parts(input, 4) };
    let mut horizontal = [0usize; 2];
    let mut vertical = [0usize; 2];
    let mut hc = 0;
    let mut vc = 0;
    for (i, r) in rects.iter().enumerate() {
        if !usable(r) {
            return 0;
        }
        if r.width >= r.height {
            if hc >= 2 {
                return 0;
            }
            horizontal[hc] = i;
            hc += 1;
        } else {
            if vc >= 2 {
                return 0;
            }
            vertical[vc] = i;
            vc += 1;
        }
    }
    if hc != 2 || vc != 2 {
        return 0;
    }
    if rects[horizontal[0]].y + rects[horizontal[0]].height * 0.5
        > rects[horizontal[1]].y + rects[horizontal[1]].height * 0.5
    {
        horizontal.swap(0, 1);
    }
    if rects[vertical[0]].x + rects[vertical[0]].width * 0.5
        > rects[vertical[1]].x + rects[vertical[1]].width * 0.5
    {
        vertical.swap(0, 1);
    }
    let slots = [horizontal[0], vertical[1], horizontal[1], vertical[0]];
    for i in 0..4 {
        unsafe { *indices.add(i) = slots[cfg.positions[i] as usize] };
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_scale_shield(
    positions: *mut Rect,
    authored: *const u8,
    width: f32,
    height: f32,
) {
    if positions.is_null() || authored.is_null() {
        return;
    }
    for i in 0..4 {
        if unsafe { *authored.add(i) } != 0 {
            continue;
        }
        let p = unsafe { &mut *positions.add(i) };
        p.x *= width / 128.0;
        p.y *= height / 128.0;
        p.width *= width / 128.0;
        p.height *= height / 128.0;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_shield_value(
    mode: u32,
    current: f32,
    maximum: f32,
    first: *mut f64,
    second: *mut f64,
) -> u8 {
    if first.is_null()
        || second.is_null()
        || (mode != 1 && mode != 2)
        || !current.is_finite()
        || !maximum.is_finite()
        || maximum <= 0.0
    {
        return 0;
    }
    let clamped = if current > 0.0 {
        current.min(maximum)
    } else {
        0.0
    };
    unsafe {
        *first = rounded_shield_value(clamped, maximum, mode == 1);
        *second = rounded_shield_value(maximum, maximum, false);
    }
    1
}
fn yes(value: bool) -> u8 {
    u8::from(value)
}
fn bytes<'a>(ptr: *const u8, len: usize) -> &'a [u8] {
    if len == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(ptr, len) }
    }
}
fn ascii_space(b: u8) -> bool {
    crt_space(b)
}
fn trim_space(mut b: &[u8]) -> &[u8] {
    while !b.is_empty() && ascii_space(b[0]) {
        b = &b[1..];
    }
    while !b.is_empty() && ascii_space(b[b.len() - 1]) {
        b = &b[..b.len() - 1];
    }
    b
}
fn ends_ci(a: &[u8], suffix: &[u8]) -> bool {
    a.len() >= suffix.len()
        && a[a.len() - suffix.len()..]
            .iter()
            .zip(suffix)
            .all(|(x, y)| crt_lower(*x) == crt_lower(*y))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_passive_weapon_class(
    text: *const u8,
    length: usize,
) -> u8 {
    if text.is_null() && length != 0 {
        return 0;
    }
    let value = trim_space(bytes(text, length));
    yes(value.len() == 13
        && value
            .iter()
            .zip(b"utilityweapon")
            .all(|(x, y)| crt_lower(*x) == *y))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_node_name(
    input: *const u8,
    length: usize,
    output: *mut u8,
    output_length: *mut usize,
) -> u8 {
    if (input.is_null() && length != 0) || output.is_null() || output_length.is_null() {
        return 0;
    }
    let value = trim_space(bytes(input, length));
    if value.is_empty()
        || value.len() > 63
        || value
            .iter()
            .any(|b| !crt_alnum(*b) && *b != b'_' && *b != b'-')
    {
        return 0;
    }
    for (i, b) in value.iter().enumerate() {
        unsafe { *output.add(i) = crt_lower(*b) };
    }
    unsafe { *output_length = value.len() };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_faction_suffix(
    input: *const u8,
    length: usize,
    output: *mut u8,
    output_length: *mut usize,
) -> u8 {
    if (input.is_null() && length != 0) || output.is_null() || output_length.is_null() {
        return 0;
    }
    let value = trim_space(bytes(input, length));
    if value.len() > 32
        || value
            .iter()
            .any(|b| !crt_alnum(*b) && *b != b'_' && *b != b'-')
    {
        return 0;
    }
    unsafe {
        core::ptr::copy_nonoverlapping(value.as_ptr(), output, value.len());
        *output_length = value.len();
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_texture_name(
    input: *const u8,
    input_length: usize,
    suffix: *const u8,
    suffix_length: usize,
    output: *mut u8,
) -> usize {
    if (input.is_null() && input_length != 0)
        || (suffix.is_null() && suffix_length != 0)
        || output.is_null()
    {
        return 0;
    }
    let value = trim_space(bytes(input, input_length));
    let add = bytes(suffix, suffix_length);
    let mut length = value.len();
    if ends_ci(value, b".tga") || ends_ci(value, b".dds") {
        length -= 4;
    }
    for (i, b) in value[..length].iter().enumerate() {
        unsafe { *output.add(i) = if *b == b'/' { b'\\' } else { *b } };
    }
    if !add.is_empty() && !ends_ci(unsafe { slice::from_raw_parts(output, length) }, add) {
        unsafe { core::ptr::copy_nonoverlapping(add.as_ptr(), output.add(length), add.len()) };
        length += add.len();
    }
    length
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_texture_path(
    input: *const u8,
    input_length: usize,
    extension: *const u8,
    extension_length: usize,
    output: *mut u8,
) -> usize {
    if (input.is_null() && input_length != 0)
        || (extension.is_null() && extension_length != 0)
        || output.is_null()
    {
        return 0;
    }
    let value = trim_space(bytes(input, input_length));
    let add = bytes(extension, extension_length);
    let has_prefix = value.len() >= 9
        && value[..8]
            .iter()
            .zip(b"textures")
            .all(|(x, y)| crt_lower(*x) == *y)
        && (value[8] == b'\\' || value[8] == b'/');
    let mut length = 0;
    if !has_prefix {
        unsafe { core::ptr::copy_nonoverlapping(b"Textures\\".as_ptr(), output, 9) };
        length = 9;
    }
    for (i, b) in value.iter().enumerate() {
        unsafe { *output.add(length + i) = if *b == b'/' { b'\\' } else { *b } };
    }
    length += value.len();
    if !add.is_empty() && !ends_ci(unsafe { slice::from_raw_parts(output, length) }, add) {
        unsafe { core::ptr::copy_nonoverlapping(add.as_ptr(), output.add(length), add.len()) };
        length += add.len();
    }
    length
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_build_cycle(
    numbered: *const OptionalNumber,
    numbered_count: usize,
    legacy: f32,
    save: OptionalNumber,
    reset_time: OptionalNumber,
    delays: *mut f32,
    delay_count: *mut usize,
    save_index: *mut usize,
    reset: *mut f32,
    numbered_used: *mut u8,
) -> u32 {
    if delays.is_null()
        || delay_count.is_null()
        || save_index.is_null()
        || reset.is_null()
        || numbered_used.is_null()
    {
        return 1;
    }
    let count = numbered_count.min(64);
    let inputs = if numbered.is_null() {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(numbered, count) }
    };
    let mut contiguous = 0;
    while contiguous < inputs.len() && inputs[contiguous].present != 0 {
        let v = inputs[contiguous];
        if v.valid == 0 || !v.value.is_finite() || v.value < 0.0 {
            return 3;
        }
        contiguous += 1;
    }
    if inputs[contiguous..].iter().any(|v| v.present != 0) {
        return 2;
    }
    let effective_count = if contiguous == 0 { 1 } else { contiguous };
    if contiguous == 0 && (!legacy.is_finite() || legacy < 0.0) {
        return 1;
    }
    let mut save_value = 0;
    if save.present != 0 {
        if save.valid == 0
            || !save.value.is_finite()
            || save.value < 0.0
            || save.value > u32::MAX as f32
            || save.value as usize >= effective_count
            || (save.value as usize as f32) != save.value
        {
            return 4;
        }
        save_value = save.value as usize;
    }
    let mut reset_value = 0.0;
    if reset_time.present != 0 {
        if reset_time.valid == 0 || !reset_time.value.is_finite() || reset_time.value < 0.0 {
            return 5;
        }
        reset_value = reset_time.value;
    }
    unsafe {
        if contiguous == 0 {
            *delays = legacy;
        } else {
            for (i, input) in inputs.iter().enumerate().take(contiguous) {
                *delays.add(i) = input.value;
            }
        }
        *delay_count = effective_count;
        *save_index = save_value;
        *reset = reset_value;
        *numbered_used = yes(contiguous != 0);
    }
    0
}
fn normalize_cycle(count: usize, reset: f32, state: &mut CycleState) {
    if count == 0 || state.next_delay_index >= count {
        state.next_delay_index = 0;
    }
    if !state.idle_reset_remaining.is_finite() || state.idle_reset_remaining < 0.0 {
        state.idle_reset_remaining = 0.0;
    }
    if reset <= 0.0 {
        state.idle_reset_remaining = 0.0;
    } else if state.idle_reset_remaining > reset {
        state.idle_reset_remaining = reset;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_normalize_cycle(
    _delays: *const f32,
    count: usize,
    _save: usize,
    reset: f32,
    state: *mut CycleState,
) {
    if let Some(s) = unsafe { state.as_mut() } {
        normalize_cycle(count, reset, s);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_consume_cycle(
    delays: *const f32,
    count: usize,
    save: usize,
    reset: f32,
    state: *mut CycleState,
) -> f32 {
    if delays.is_null() || count == 0 || state.is_null() {
        return 0.0;
    }
    let s = unsafe { &mut *state };
    normalize_cycle(count, reset, s);
    let delay = unsafe { *delays.add(s.next_delay_index) };
    if s.next_delay_index + 1 < count {
        s.next_delay_index += 1;
    } else {
        s.next_delay_index = save.min(count - 1);
    }
    s.idle_reset_remaining = reset;
    s.fire_count = s.fire_count.wrapping_add(1);
    delay
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_update_cycle(
    delays: *const f32,
    count: usize,
    reset: f32,
    state: *mut CycleState,
    advanced: u8,
    after: f32,
    elapsed: f32,
) {
    if delays.is_null()
        || count == 0
        || state.is_null()
        || reset <= 0.0
        || advanced == 0
        || !after.is_finite()
        || after > 0.0
        || !elapsed.is_finite()
        || elapsed <= 0.0
    {
        return;
    }
    let s = unsafe { &mut *state };
    s.idle_reset_remaining -= elapsed;
    if !s.idle_reset_remaining.is_finite() || s.idle_reset_remaining <= 0.0 {
        s.next_delay_index = 0;
        s.idle_reset_remaining = 0.0;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_identity_index(
    index: i32,
    count: usize,
    output: *mut usize,
) -> u8 {
    if !output.is_null() {
        unsafe { *output = 0 };
    }
    if index < 0 || index as usize >= count {
        return 0;
    }
    if !output.is_null() {
        unsafe { *output = index as usize };
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_parse_nonnegative(
    text: *const u8,
    length: usize,
    output: *mut i32,
) -> u8 {
    if output.is_null() || (text.is_null() && length != 0) {
        return 0;
    }
    let bytes = if length == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(text, length) }
    };
    let mut first = 0;
    let mut end = length;
    let space = |b| matches!(b, b' ' | b'\t' | b'\r' | b'\n');
    while first < end && space(bytes[first]) {
        first += 1;
    }
    while end > first && space(bytes[end - 1]) {
        end -= 1;
    }
    if first == end {
        return 0;
    }
    let mut cursor = first;
    let negative = bytes[cursor] == b'-';
    if negative {
        cursor += 1;
    }
    if cursor == end {
        return 0;
    }
    let mut value = 0i32;
    while cursor < end {
        let digit = bytes[cursor].wrapping_sub(b'0');
        if digit > 9 {
            return 0;
        }
        if negative && digit != 0 {
            return 0;
        }
        let Some(next) = value
            .checked_mul(10)
            .and_then(|v| v.checked_add(digit as i32))
        else {
            return 0;
        };
        value = next;
        cursor += 1;
    }
    unsafe { *output = value };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_can_afford(amounts: *const i64, costs: *const i32) -> u8 {
    if amounts.is_null() || costs.is_null() {
        return 0;
    }
    for i in 0..4 {
        if unsafe { *amounts.add(i) < *costs.add(i) as i64 } {
            return 0;
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_debit(amounts: *mut i64, costs: *const i32) {
    if amounts.is_null() || costs.is_null() {
        return;
    }
    for i in 0..4 {
        unsafe { *amounts.add(i) = (*amounts.add(i)).wrapping_sub(*costs.add(i) as i64) };
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_saturating_add(value: i64, delta: i64) -> i64 {
    value.saturating_add(delta)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_credit(amounts: *mut i64, costs: *const i32) {
    if amounts.is_null() || costs.is_null() {
        return;
    }
    for i in 0..4 {
        unsafe { *amounts.add(i) = (*amounts.add(i)).saturating_add(*costs.add(i) as i64) };
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_node_flags(current: u32, selected: u8) -> u32 {
    if selected != 0 {
        current & !3
    } else {
        current | 3
    }
}
fn mix(handle: u32, index: usize, sequence: u32, salt: u32) -> u32 {
    let mut v = handle ^ 0x9e3779b9;
    v ^= (index as u32).wrapping_add(1).wrapping_mul(0x85ebca6b);
    v ^= sequence.wrapping_add(1).wrapping_mul(0xc2b2ae35);
    v ^= salt.wrapping_mul(0x27d4eb2d);
    v ^= v >> 16;
    v = v.wrapping_mul(0x7feb352d);
    v ^= v >> 15;
    v = v.wrapping_mul(0x846ca68b);
    v ^ (v >> 16)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_subsystem_condition(
    operational: u8,
    forced: u8,
    maximum: i32,
    current: f64,
    disable: f32,
) -> u8 {
    if maximum <= 0 || !current.is_finite() || !disable.is_finite() || operational != 0 {
        return if operational != 0 { 0 } else { 1 };
    }
    if current <= 0.0
        || (forced == 0 && disable <= 0.0 && crate::legacy_float::below_maximum(current, maximum))
    {
        2
    } else {
        1
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_mesh_choice(
    handle: u32,
    index: usize,
    sequence: u32,
    count: usize,
) -> usize {
    if count == 0 {
        0
    } else {
        mix(handle, index, sequence, 0) as usize % count
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_rebuild_scale(maximum: i32, current: f64) -> f32 {
    if maximum <= 0 || !current.is_finite() || current <= 0.0 {
        return 0.0;
    }
    crate::legacy_float::rebuild(current, maximum)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_repair_sample(
    handle: u32,
    index: usize,
    sequence: u32,
    component: usize,
) -> f32 {
    ((mix(handle, index, sequence, component.wrapping_add(1) as u32) & 0xffffff) as f64
        / 16777215.0) as f32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_damage_policy_active(
    mesh: usize,
    threshold: f32,
    scorch: usize,
    hardpoints: usize,
) -> u8 {
    yes(mesh != 0 || (threshold.is_finite() && threshold > 0.0 && scorch != 0 && hardpoints != 0))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_icon_state(
    operational: u8,
    forced: u8,
    maximum: i32,
    current: f64,
    disable: f32,
    output: *mut u8,
) -> u8 {
    if output.is_null() || maximum <= 0 || !current.is_finite() || !disable.is_finite() {
        return 0;
    }
    let value = if current <= 0.0 {
        4
    } else if operational == 0 {
        if forced != 0 || disable > 0.0 || !crate::legacy_float::below_maximum(current, maximum) {
            3
        } else {
            4
        }
    } else {
        let ratio = crate::legacy_float::ratio(current, maximum);
        if !ratio.is_finite() {
            return 0;
        }
        if ratio <= 0.25 {
            2
        } else if ratio <= 0.5 {
            1
        } else {
            0
        }
    };
    unsafe { *output = value };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_tint_colour(
    native: *const f32,
    configured: *const f32,
    output: *mut f32,
) {
    if native.is_null() || configured.is_null() || output.is_null() {
        return;
    }
    let n = unsafe { slice::from_raw_parts(native, 3) };
    let c = unsafe { slice::from_raw_parts(configured, 3) };
    let o = unsafe { slice::from_raw_parts_mut(output, 3) };
    if n.iter().any(|v| !v.is_finite()) || c.iter().any(|v| !v.is_finite()) {
        o.copy_from_slice(n);
        return;
    }
    let ni = n.iter().copied().fold(n[0], f32::max).clamp(0.0, 1.0);
    let cp = c.iter().copied().fold(c[0], f32::max).clamp(0.0, 1.0);
    if ni <= 0.0 || cp <= 0.0 {
        o.fill(0.0);
        return;
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        use crate::shield_wide::Wide;
        let scale = Wide::new(ni).div(Wide::new(cp));
        for i in 0..3 {
            let v = Wide::new(c[i]).mul(scale);
            o[i] = if v.lt(Wide::new(0.)) {
                0.
            } else if v.gt(Wide::new(1.)) {
                1.
            } else {
                v.float()
            };
        }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let scale = ni / cp;
        for i in 0..3 {
            o[i] = (c[i] * scale).clamp(0.0, 1.0);
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_weapon_presentation(kind: u8, technology: u8, hide: u8) -> u8 {
    if kind == 2 {
        3
    } else if technology == 2 {
        if hide != 0 { 2 } else { 1 }
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_gameplay_weapon_colour_source(weapon: u8, system: u8) -> u8 {
    if weapon != 0 {
        2
    } else if system != 0 {
        1
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_centered_fill(
    source: *const Rect,
    destination: *const Rect,
    ratio: f32,
    horizontal: u8,
    output: *mut Fill,
) {
    if source.is_null() || destination.is_null() || output.is_null() {
        return;
    }
    let mut s = unsafe { *source };
    let mut d = unsafe { *destination };
    let r = if !ratio.is_finite() || ratio <= 0.0 {
        0.0
    } else if ratio >= 1.0 {
        1.0
    } else {
        ratio
    };
    if horizontal != 0 {
        (s.x, s.width) = crate::legacy_float::centered_axis(s.x, s.width, r);
        (d.x, d.width) = crate::legacy_float::centered_axis(d.x, d.width, r);
    } else {
        (s.y, s.height) = crate::legacy_float::centered_axis(s.y, s.height, r);
        (d.y, d.height) = crate::legacy_float::centered_axis(d.y, d.height, r);
    }
    unsafe {
        *output = Fill {
            source: s,
            destination: d,
        }
    };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_gameplay_segment_at(
    rectangles: *const Rect,
    x: f32,
    y: f32,
) -> i32 {
    if rectangles.is_null() || !x.is_finite() || !y.is_finite() {
        return -1;
    }
    for (i, r) in unsafe { slice::from_raw_parts(rectangles, 4) }
        .iter()
        .enumerate()
    {
        if !r.x.is_finite()
            || !r.y.is_finite()
            || !r.width.is_finite()
            || r.width <= 0.0
            || !r.height.is_finite()
            || r.height <= 0.0
        {
            continue;
        }
        if x >= r.x
            && crate::legacy_float::within_end(x, r.x, r.width)
            && y >= r.y
            && crate::legacy_float::within_end(y, r.y, r.height)
        {
            return i as i32;
        }
    }
    -1
}

#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn rounded_shield_value(current: f32, maximum: f32, percent: bool) -> f64 {
    unsafe extern "C" {
        fn floor(value: f64) -> f64;
    }
    let value = if percent {
        (current / maximum * 100.0) as f64
    } else {
        current as f64
    };
    // SAFETY: C floor has the same finite numeric contract as std::floor.
    unsafe { floor(value + 0.5) }
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn rounded_shield_value(current: f32, maximum: f32, percent: bool) -> f64 {
    // Mirror the original MinGW expression's x87 and explicit double spills.
    // Slots: f32 inputs at0/4/8, f64half16, old/newCW24/26, f64temp32/result40.
    let mut slots = [0_u64; 6];
    slots[0] = current.to_bits() as u64 | ((maximum.to_bits() as u64) << 32);
    slots[1] = 100.0_f32.to_bits() as u64;
    slots[2] = 0.5_f64.to_bits();
    let mode = u32::from(percent);
    // SAFETY: all offsets are within the live 48-byte array. x87 stack is
    // clobbered in full, starts/ends empty, and the caller's CW is restored.
    unsafe {
        core::arch::asm!(
            "fnstcw word ptr [{data}+24]", "mov ax, word ptr [{data}+24]",
            "and ax, 0xf3ff", "or ax, 0x400", "mov word ptr [{data}+26], ax",
            "fld dword ptr [{data}]", "cmp {mode:e}, 0", "je 2f",
            "fdiv dword ptr [{data}+4]", "fmul dword ptr [{data}+8]", "2:",
            "fstp qword ptr [{data}+32]", "fld qword ptr [{data}+32]",
            "fadd qword ptr [{data}+16]", "fstp qword ptr [{data}+32]",
            "fld qword ptr [{data}+32]", "fldcw word ptr [{data}+26]",
            "frndint", "fldcw word ptr [{data}+24]", "fstp qword ptr [{data}+40]",
            data=in(reg) slots.as_mut_ptr(), mode=in(reg) mode,
            out("eax") _,out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack)
        );
    }
    f64::from_bits(slots[5])
}
