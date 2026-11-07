//! Build tooltip runtime sequencing; native operations stay behind callbacks.
use alloc::vec::Vec;
use core::{
    ffi::{CStr, c_char, c_void},
    ptr,
};
#[repr(C)]
pub struct TooltipMeasure {
    raw: f32,
    native_duration: f32,
    modifier: f32,
    displayed: f32,
    bytes: *const u8,
    size: usize,
}
impl TooltipMeasure {
    fn empty() -> Self {
        Self {
            raw: 0.0,
            native_duration: 0.0,
            modifier: 1.0,
            displayed: 0.0,
            bytes: ptr::null(),
            size: 0,
        }
    }
}
type Step = unsafe extern "C" fn(*mut c_void, u32, u32, *mut TooltipMeasure) -> usize;
unsafe fn call(
    step: Step,
    ctx: *mut c_void,
    op: u32,
    arg: u32,
    value: &mut TooltipMeasure,
) -> usize {
    unsafe { step(ctx, op, arg, value) }
}
fn append(out: &mut Vec<u8>, part: &[u8]) -> bool {
    if part.len() >= 512 - out.len() {
        return false;
    }
    out.extend_from_slice(part);
    true
}
unsafe fn cbytes<'a>(pointer: usize) -> Option<&'a [u8]> {
    if pointer == 0 {
        None
    } else {
        Some(unsafe { CStr::from_ptr(pointer as *const c_char) }.to_bytes())
    }
}
unsafe fn prepare(step: Step, ctx: *mut c_void) -> bool {
    let mut value = TooltipMeasure::empty();
    unsafe { call(step, ctx, 0, 0, &mut value) }; // clear TLS override
    if unsafe { call(step, ctx, 1, 0, &mut value) } as i32 != 1 {
        return false;
    }
    let object_class = unsafe { call(step, ctx, 2, 0, &mut value) };
    let provider = unsafe { call(step, ctx, 3, 0, &mut value) } != 0;
    if object_class == 0 {
        return false;
    }
    let team = unsafe { call(step, ctx, 4, 0, &mut value) } as i32;
    unsafe { call(step, ctx, 5, team as u32, &mut value) };
    let separator = unsafe { call(step, ctx, 6, 0, &mut value) };
    if separator == 0 {
        return false;
    }
    let compact = unsafe { call(step, ctx, 19, 0, &mut value) } == 0;
    let mut out = Vec::new();
    if provider {
        let fallback: [&[u8]; 4] = [
            b"Tritanium\0",
            b"Supply\0",
            b"Credits\0",
            b"Collective connections\0",
        ];
        for i in 0..4 {
            let cost = unsafe { call(step, ctx, 7, i, &mut value) } as i32;
            if cost <= 0 {
                continue;
            }
            let candidate = unsafe { call(step, ctx, 8, i, &mut value) };
            let name = if unsafe { cbytes(candidate) }.is_some_and(|b| !b.is_empty()) {
                candidate as *const c_char
            } else {
                fallback[i as usize].as_ptr().cast::<c_char>()
            };
            let mut token = [0u8; 128];
            if unsafe {
                crate::text_geometry::a2fo_rs_text_resource_token(
                    separator as *const c_char,
                    name,
                    cost,
                    u8::from(compact),
                    token.as_mut_ptr().cast(),
                    token.len(),
                )
            } == 0
            {
                return false;
            }
            let part = unsafe { CStr::from_ptr(token.as_ptr().cast()) }.to_bytes();
            if !append(&mut out, part) {
                return false;
            }
        }
    }
    if unsafe { call(step, ctx, 9, team as u32, &mut value) } == 0 {
        return false;
    }
    value.displayed = crate::text_geometry::a2fo_rs_text_reconcile(
        value.raw,
        value.native_duration,
        value.modifier,
    );
    if value.modifier != 1.0 && unsafe { call(step, ctx, 13, 0, &mut value) } == 0 {
        unsafe { call(step, ctx, 12, 0, &mut value) };
    }
    let mut token = [0u8; 64];
    if unsafe {
        crate::text_geometry::a2fo_rs_text_build_token(
            separator as *const c_char,
            value.displayed,
            u8::from(compact),
            token.as_mut_ptr().cast(),
            token.len(),
        )
    } == 0
    {
        return false;
    }
    if !append(
        &mut out,
        unsafe { CStr::from_ptr(token.as_ptr().cast()) }.to_bytes(),
    ) {
        return false;
    }
    let end = unsafe { call(step, ctx, 10, 0, &mut value) };
    let Some(end) = (unsafe { cbytes(end) }) else {
        return false;
    };
    if !append(&mut out, end) {
        return false;
    }
    value.bytes = out.as_ptr();
    value.size = out.len();
    (unsafe { call(step, ctx, 11, 0, &mut value) }) != 0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_tooltip_prepare(
    step: Option<Step>,
    ctx: *mut c_void,
) -> u8 {
    step.map_or(0, |step| u8::from(unsafe { prepare(step, ctx) }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_tooltip_call(
    gateway: u8,
    enabled: u8,
    ready: u8,
    inside: u8,
    step: Option<Step>,
    ctx: *mut c_void,
) -> u8 {
    if gateway == 0 {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    let mut value = TooltipMeasure::empty();
    if enabled == 0 || ready == 0 || inside != 0 {
        return u8::from(unsafe { call(step, ctx, 21, 0, &mut value) } != 0);
    }
    unsafe {
        call(step, ctx, 22, 0, &mut value);
        prepare(step, ctx)
    };
    let result = unsafe { call(step, ctx, 21, 0, &mut value) } != 0;
    unsafe {
        call(step, ctx, 23, 0, &mut value);
        call(step, ctx, 24, 0, &mut value)
    };
    u8::from(result)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_tooltip_override(
    inside: u8,
    has_override: u8,
    key: *const c_char,
) -> u8 {
    if inside == 0 || has_override == 0 || key.is_null() {
        return 0;
    }
    u8::from(unsafe { CStr::from_ptr(key) }.to_bytes() == b"GUI_CP_END_EXTRA")
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_tooltip_init(
    valid: u8,
    step: Option<Step>,
    ctx: *mut c_void,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    let mut value = TooltipMeasure::empty();
    if unsafe { call(step, ctx, 30, 0, &mut value) } == 0 {
        return 0;
    }
    let mut text = false;
    let mut verbose = false;
    let mut patched = 0u32;
    if unsafe { call(step, ctx, 31, 0, &mut value) } != 0 {
        text = unsafe { call(step, ctx, 32, 0, &mut value) } != 0;
        verbose = unsafe { call(step, ctx, 33, 0, &mut value) } != 0;
        for i in 0..2 {
            patched += u32::from(unsafe { call(step, ctx, 34, i, &mut value) } != 0);
        }
        if !text || !verbose || patched != 2 {
            let flags = u32::from(text) | (u32::from(verbose) << 1) | (patched << 2);
            unsafe { call(step, ctx, 36, flags, &mut value) };
        }
    }
    let ready = text && verbose && patched == 2;
    unsafe { call(step, ctx, 37, u32::from(ready), &mut value) };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_tooltip_shutdown(step: Option<Step>, ctx: *mut c_void) {
    if let Some(step) = step {
        let mut value = TooltipMeasure::empty();
        unsafe { call(step, ctx, 38, 0, &mut value) };
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct XpBytes {
    data: *const u8,
    size: usize,
}
impl XpBytes {
    unsafe fn get<'a>(self) -> Option<&'a [u8]> {
        if self.size == 0 {
            Some(&[])
        } else if self.data.is_null() || self.size > isize::MAX as usize {
            None
        } else {
            Some(unsafe { core::slice::from_raw_parts(self.data, self.size) })
        }
    }
}
fn xp_span(value: &[u8]) -> XpBytes {
    XpBytes {
        data: value.as_ptr(),
        size: value.len(),
    }
}
#[repr(C)]
pub struct XpReply {
    first: XpBytes,
    second: XpBytes,
    third: XpBytes,
    word: u64,
    other: u64,
    third_word: u64,
    number: f64,
    second_number: f64,
}
impl XpReply {
    fn empty() -> Self {
        Self {
            first: xp_span(&[]),
            second: xp_span(&[]),
            third: xp_span(&[]),
            word: 0,
            other: 0,
            third_word: 0,
            number: 0.0,
            second_number: 0.0,
        }
    }
}
type XpEmit = unsafe extern "C" fn(*mut c_void, XpBytes);
type XpField = unsafe extern "C" fn(*mut c_void, u32, *mut XpBytes, *mut XpBytes) -> u8;
type XpParse = unsafe extern "C" fn(*mut c_void, XpBytes, *mut f64, *mut u32) -> u8;
type XpStep = unsafe extern "C" fn(*mut c_void, u32, *mut XpReply) -> u64;
unsafe extern "C" {
    fn tolower(ch: i32) -> i32;
}
fn xp_normal(value: &[u8]) -> Vec<u8> {
    let mut value = value;
    while value.first().is_some_and(|ch| b" \t\r\n\"".contains(ch)) {
        value = &value[1..];
    }
    while value.last().is_some_and(|ch| b" \t\r\n\"".contains(ch)) {
        value = &value[..value.len() - 1];
    }
    let mut out = value.to_vec();
    for ch in &mut out {
        if ch.is_ascii_uppercase() {
            *ch += b'a' - b'A';
        }
    }
    if out.len() > 4 && out.ends_with(b".odf") {
        out.truncate(out.len() - 4);
    }
    if out.len() > 128 || out.iter().any(|ch| b"\\/:\r\n".contains(ch)) {
        out.clear();
    }
    out
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_normal(
    input: XpBytes,
    emit: Option<XpEmit>,
    ctx: *mut c_void,
) {
    if let (Some(input), Some(emit)) = (unsafe { input.get() }, emit) {
        let out = xp_normal(input);
        unsafe { emit(ctx, xp_span(&out)) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_field(
    count: u32,
    key: XpBytes,
    field: Option<XpField>,
    emit: Option<XpEmit>,
    ctx: *mut c_void,
) {
    let (Some(key), Some(field), Some(emit)) = (unsafe { key.get() }, field, emit) else {
        return;
    };
    for i in 0..count {
        let mut name = xp_span(&[]);
        let mut value = xp_span(&[]);
        if unsafe { field(ctx, i, &mut name, &mut value) } == 0 || name.size != key.len() {
            continue;
        }
        let Some(name) = (unsafe { name.get() }) else {
            continue;
        };
        if !name
            .iter()
            .zip(key)
            .all(|(&a, &b)| unsafe { tolower(a as i32) == tolower(b as i32) })
        {
            continue;
        }
        if value.data.is_null() {
            continue;
        }
        unsafe { emit(ctx, value) };
        return;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_number(
    input: XpBytes,
    fallback: f64,
    parse: Option<XpParse>,
    ctx: *mut c_void,
) -> f64 {
    let Some(input) = (unsafe { input.get() }) else {
        return f64::NAN;
    };
    if input.is_empty() {
        return fallback;
    }
    let Some(parse) = parse else {
        return f64::NAN;
    };
    let mut number = 0.0;
    let mut consumed = 0;
    if unsafe { parse(ctx, xp_span(input), &mut number, &mut consumed) } == 0
        || consumed == 0
        || !number.is_finite()
    {
        return f64::NAN;
    }
    let mut tail = input.get(consumed as usize..).unwrap_or(&[]);
    while tail.first().is_some_and(|ch| b" \t\r\n".contains(ch)) {
        tail = &tail[1..];
    }
    if tail.first().is_some_and(|ch| *ch != 0) {
        return f64::NAN;
    }
    number
}
unsafe fn chain(first: &[u8], step: XpStep, ctx: *mut c_void) -> bool {
    let mut current = first.to_vec();
    let mut seen: Vec<Vec<u8>> = Vec::new();
    while !current.is_empty() && seen.len() < 64 {
        if seen.iter().any(|item| item == &current) {
            return false;
        }
        seen.push(current.clone());
        let mut reply = XpReply::empty();
        reply.first = xp_span(&current);
        if unsafe { step(ctx, 0, &mut reply) } == 0 {
            return false;
        }
        reply = XpReply::empty();
        reply.first = xp_span(&current);
        if unsafe { step(ctx, 1, &mut reply) } == 0 || reply.word == 0 {
            return false;
        }
        let Some(next) = (unsafe { reply.second.get() }) else {
            return false;
        };
        let next = next.to_vec();
        let level = reply.word;
        let xp = reply.number;
        reply = XpReply::empty();
        reply.first = xp_span(&current);
        if unsafe { step(ctx, 2, &mut reply) } == 0 {
            reply = XpReply::empty();
            reply.first = xp_span(&current);
            reply.second = xp_span(&next);
            reply.word = level;
            reply.number = xp;
            if unsafe { step(ctx, 3, &mut reply) } == 0 {
                return false;
            }
        }
        current = next;
    }
    current.is_empty()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_prepare_chain(
    first: XpBytes,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u8 {
    match (unsafe { first.get() }, step) {
        (Some(first), Some(step)) => u8::from(unsafe { chain(first, step, ctx) }),
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_family(
    name: XpBytes,
    step: Option<XpStep>,
    emit: Option<XpEmit>,
    ctx: *mut c_void,
) {
    let (Some(name), Some(step), Some(emit)) = (unsafe { name.get() }, step, emit) else {
        return;
    };
    let result = unsafe { family_owned(name, step, ctx) };
    if !result.is_empty() {
        unsafe { emit(ctx, xp_span(&result)) };
    }
}
fn field_value(count: u32, key: &[u8], field: XpField, ctx: *mut c_void) -> Vec<u8> {
    for i in 0..count {
        let mut name = xp_span(&[]);
        let mut value = xp_span(&[]);
        if unsafe { field(ctx, i, &mut name, &mut value) } == 0 || name.size != key.len() {
            continue;
        }
        let Some(name) = (unsafe { name.get() }) else {
            continue;
        };
        if !name
            .iter()
            .zip(key)
            .all(|(&a, &b)| unsafe { tolower(a as i32) == tolower(b as i32) })
        {
            continue;
        }
        if value.data.is_null() {
            continue;
        }
        if let Some(value) = unsafe { value.get() } {
            return value.to_vec();
        }
    }
    Vec::new()
}
fn parsed(value: &[u8], fallback: f64, parse: XpParse, ctx: *mut c_void) -> f64 {
    unsafe { a2fo_rs_runtime_xp_number(xp_span(value), fallback, Some(parse), ctx) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_class(
    valid: u8,
    klass: usize,
    source: XpBytes,
    count: u32,
    field: Option<XpField>,
    parse: Option<XpParse>,
    step: Option<XpStep>,
    ctx: *mut c_void,
) {
    if valid == 0 || klass == 0 {
        return;
    }
    let (Some(source), Some(field), Some(parse), Some(step)) =
        (unsafe { source.get() }, field, parse, step)
    else {
        return;
    };
    let name = xp_normal(source);
    if name.is_empty() {
        return;
    }
    let level = parsed(&field_value(count, b"XPlevel", field, ctx), 0.0, parse, ctx);
    if !level.is_finite() || !(1.0..=64.0).contains(&level) || ((level as u64) as f64) != level {
        return;
    }
    let mut next_xp = parsed(
        &field_value(count, b"nextrankXP", field, ctx),
        0.0,
        parse,
        ctx,
    );
    let mut next = xp_normal(&field_value(count, b"nextrankClass", field, ctx));
    if next.is_empty() || next_xp <= 0.0 {
        next.clear();
        next_xp = 0.0;
    }
    if !next_xp.is_finite() {
        return;
    }
    let original = xp_normal(&field_value(count, b"originalClass", field, ctx));
    let mut reply = XpReply::empty();
    reply.first = xp_span(&name);
    reply.second = xp_span(&next);
    reply.third = xp_span(&original);
    reply.word = level as u64;
    reply.other = klass as u64;
    reply.number = next_xp;
    unsafe { step(ctx, 6, &mut reply) };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_race(
    valid: u8,
    race: usize,
    ready: u8,
    count: u32,
    field: Option<XpField>,
    parse: Option<XpParse>,
    step: Option<XpStep>,
    ctx: *mut c_void,
) {
    if valid == 0 || race == 0 {
        return;
    }
    let (Some(field), Some(parse), Some(step)) = (field, parse, step) else {
        return;
    };
    let enabled = parsed(
        &field_value(count, b"collectiveXP", field, ctx),
        0.0,
        parse,
        ctx,
    );
    let modifier = parsed(
        &field_value(count, b"collectiveXPModifier", field, ctx),
        1.0,
        parse,
        ctx,
    );
    let mut reply = XpReply::empty();
    reply.other = race as u64;
    if (enabled != 0.0 && enabled != 1.0) || !modifier.is_finite() || modifier <= 0.0 {
        unsafe { step(ctx, 7, &mut reply) };
        return;
    }
    reply.word = u64::from(enabled == 1.0);
    reply.number = modifier;
    unsafe { step(ctx, 8, &mut reply) };
    if ready != 0 && enabled == 1.0 && unsafe { step(ctx, 9, &mut reply) } == 0 {
        unsafe { step(ctx, 10, &mut reply) };
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct XpProgress {
    levels_gained: u32,
    consumed: u32,
    level: u32,
    pool: f64,
    threshold: f64,
}
unsafe fn xp_configure(player: u32, step: XpStep, ctx: *mut c_void) -> bool {
    if player >= 9 {
        return false;
    }
    let mut reply = XpReply::empty();
    reply.word = player as u64;
    if unsafe { step(ctx, 20, &mut reply) } == 0 || reply.other == 0 {
        return false;
    }
    let multiplier = reply.number;
    reply = XpReply::empty();
    reply.word = player as u64;
    if unsafe { step(ctx, 21, &mut reply) } == 0 {
        reply = XpReply::empty();
        reply.word = player as u64;
        reply.number = multiplier;
        unsafe {
            step(ctx, 22, &mut reply);
            step(ctx, 23, &mut reply)
        };
    }
    reply = XpReply::empty();
    reply.word = player as u64;
    (unsafe { step(ctx, 21, &mut reply) }) != 0 && reply.other != 0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_configure(
    player: u32,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u8 {
    step.map_or(0, |step| {
        u8::from(unsafe { xp_configure(player, step, ctx) })
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_any_enabled(
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u8 {
    step.map_or(0, |step| u8::from(unsafe { xp_any_enabled(step, ctx) }))
}
unsafe fn xp_any_enabled(step: XpStep, ctx: *mut c_void) -> bool {
    let mut reply = XpReply::empty();
    let count = unsafe { step(ctx, 24, &mut reply) };
    for i in 0..count {
        reply = XpReply::empty();
        reply.word = i;
        if unsafe { step(ctx, 25, &mut reply) } != 0 {
            return true;
        }
    }
    false
}
unsafe fn xp_report(player: u32, progress: XpProgress, step: XpStep, ctx: *mut c_void) {
    let mut reply = XpReply::empty();
    reply.word = player as u64;
    if progress.levels_gained != 0 {
        reply.other = progress.level.wrapping_sub(progress.levels_gained) as u64;
        reply.number = progress.level as f64;
        unsafe { step(ctx, 27, &mut reply) };
    }
    if progress.consumed != 0 || progress.levels_gained != 0 {
        reply = XpReply::empty();
        reply.word = player as u64;
        reply.other = progress.level as u64;
        reply.number = progress.pool;
        reply.second_number = progress.threshold;
        unsafe { step(ctx, 28, &mut reply) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_report(
    player: u32,
    progress: XpProgress,
    step: Option<XpStep>,
    ctx: *mut c_void,
) {
    if let Some(step) = step {
        unsafe { xp_report(player, progress, step, ctx) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_reconcile(
    player: u32,
    syncing: u8,
    promoting: u8,
    step: Option<XpStep>,
    ctx: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if syncing != 0 || promoting != 0 || !unsafe { xp_configure(player, step, ctx) } {
        return;
    }
    let mut reply = XpReply::empty();
    reply.word = player as u64;
    unsafe { step(ctx, 26, &mut reply) };
    let progress = XpProgress {
        levels_gained: reply.word as u32,
        consumed: (reply.other & 1) as u32,
        level: (reply.other >> 1) as u32,
        pool: reply.number,
        threshold: reply.second_number,
    };
    unsafe { xp_report(player, progress, step, ctx) };
}
unsafe fn xp_read(
    step: XpStep,
    ctx: *mut c_void,
    op: u32,
    p: usize,
    offset: u64,
    fallback: u64,
) -> u64 {
    let mut reply = XpReply::empty();
    reply.word = p as u64;
    reply.other = offset;
    reply.third_word = fallback;
    unsafe { step(ctx, op, &mut reply) }
}
unsafe fn xp_u32(step: XpStep, ctx: *mut c_void, p: usize, offset: u64, fallback: u32) -> u32 {
    (unsafe { xp_read(step, ctx, 30, p, offset, fallback as u64) }) as u32
}
unsafe fn xp_ptr(step: XpStep, ctx: *mut c_void, p: usize, offset: u64) -> usize {
    (unsafe { xp_read(step, ctx, 31, p, offset, 0) }) as usize
}
unsafe fn xp_u8(step: XpStep, ctx: *mut c_void, p: usize, offset: u64, fallback: u8) -> u8 {
    (unsafe { xp_read(step, ctx, 32, p, offset, fallback as u64) }) as u8
}
unsafe fn xp_erase(craft: usize, step: XpStep, ctx: *mut c_void) -> i32 {
    let mut reply = XpReply::empty();
    reply.word = craft as u64;
    let id = unsafe { step(ctx, 35, &mut reply) };
    if id == 0 {
        return -1;
    }
    reply = XpReply::empty();
    reply.word = id;
    let mut owner = -1;
    if unsafe { step(ctx, 36, &mut reply) } != 0 {
        owner = reply.word as i32;
        let current = unsafe { reply.first.get() }.unwrap_or(&[]).to_vec();
        reply = XpReply::empty();
        reply.word = owner as u64;
        reply.first = xp_span(&current);
        unsafe { step(ctx, 38, &mut reply) };
        reply = XpReply::empty();
        reply.word = id;
        unsafe { step(ctx, 37, &mut reply) };
    }
    reply = XpReply::empty();
    reply.word = craft as u64;
    unsafe { step(ctx, 39, &mut reply) };
    owner
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_erase(
    craft: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> i32 {
    step.map_or(-1, |step| unsafe { xp_erase(craft, step, ctx) })
}
unsafe fn live(craft: usize, step: XpStep, ctx: *mut c_void) -> bool {
    if unsafe { xp_u32(step, ctx, craft, 0x14, 0) } & 0x40008 != 8 {
        return false;
    }
    if unsafe { xp_u8(step, ctx, craft, 0x27, 1) } != 0 {
        return false;
    }
    (unsafe { xp_u8(step, ctx, craft, 0x113, 1) }) == 0
}
unsafe fn xp_reconcile(player: u32, step: XpStep, ctx: *mut c_void) {
    let syncing = unsafe { step(ctx, 53, &mut XpReply::empty()) } != 0;
    let promoting = unsafe { step(ctx, 54, &mut XpReply::empty()) } != 0;
    unsafe {
        a2fo_rs_runtime_xp_reconcile(
            player,
            u8::from(syncing),
            u8::from(promoting),
            Some(step),
            ctx,
        )
    };
}
unsafe fn family_owned(name: &[u8], step: XpStep, ctx: *mut c_void) -> Vec<u8> {
    let mut reply = XpReply::empty();
    reply.first = xp_span(name);
    if unsafe { step(ctx, 1, &mut reply) } != 0
        && let Some(original) = unsafe { reply.third.get() }
    {
        let original = original.to_vec();
        if !original.is_empty() && original != name && unsafe { chain(&original, step, ctx) } {
            reply = XpReply::empty();
            reply.first = xp_span(&original);
            reply.second = xp_span(name);
            reply.word = 64;
            if unsafe { step(ctx, 4, &mut reply) } != 0 {
                return original;
            }
        }
    }
    let mut current = name.to_vec();
    let mut visited: Vec<Vec<u8>> = Vec::new();
    while !visited.iter().any(|item| item == &current) && visited.len() < 64 {
        visited.push(current.clone());
        reply = XpReply::empty();
        reply.first = xp_span(&current);
        if unsafe { step(ctx, 1, &mut reply) } == 0 {
            return Vec::new();
        }
        if reply.word == 1 {
            return current;
        }
        reply = XpReply::empty();
        reply.first = xp_span(&current);
        if unsafe { step(ctx, 5, &mut reply) } != 1 {
            return Vec::new();
        }
        let Some(parent) = (unsafe { reply.second.get() }) else {
            return Vec::new();
        };
        current = parent.to_vec();
    }
    Vec::new()
}
unsafe fn xp_observe(craft: usize, step: XpStep, ctx: *mut c_void) {
    let owner = unsafe { xp_u32(step, ctx, craft, 0xec, u32::MAX) } as i32;
    if !unsafe { live(craft, step, ctx) }
        || owner < 0
        || !unsafe { xp_configure(owner as u32, step, ctx) }
    {
        let prior = unsafe { xp_erase(craft, step, ctx) };
        if prior >= 0 {
            unsafe { xp_reconcile(prior as u32, step, ctx) };
        }
        return;
    }
    let omit = |step: XpStep, ctx: *mut c_void| unsafe {
        let prior = xp_erase(craft, step, ctx);
        if prior >= 0 {
            xp_reconcile(prior as u32, step, ctx);
        }
    };
    let klass = unsafe { xp_ptr(step, ctx, craft, 0x40) };
    let mut reply = XpReply::empty();
    reply.word = klass as u64;
    if unsafe { step(ctx, 40, &mut reply) } == 0 {
        omit(step, ctx);
        return;
    }
    let Some(current) = (unsafe { reply.first.get() }) else {
        omit(step, ctx);
        return;
    };
    let current = current.to_vec();
    reply = XpReply::empty();
    reply.word = craft as u64;
    let id = unsafe { step(ctx, 35, &mut reply) };
    if id != 0 {
        reply = XpReply::empty();
        reply.word = id;
        if unsafe { step(ctx, 36, &mut reply) } != 0 {
            let same = reply.word == owner as u64
                && unsafe { reply.first.get() }.is_some_and(|name| name == current);
            if same {
                return;
            }
        }
    }
    if !unsafe { chain(&current, step, ctx) } {
        omit(step, ctx);
        return;
    }
    let base = unsafe { family_owned(&current, step, ctx) };
    if base.is_empty() || !unsafe { chain(&base, step, ctx) } {
        reply = XpReply::empty();
        reply.first = xp_span(&current);
        unsafe { step(ctx, 52, &mut reply) };
        omit(step, ctx);
        return;
    }
    let enhancement = unsafe { xp_ptr(step, ctx, craft, 0x1a4) };
    let rank_class = unsafe { xp_ptr(step, ctx, enhancement, 4) };
    if (unsafe { xp_u32(step, ctx, rank_class, 0xac, 0) } as i32) <= 0 {
        omit(step, ctx);
        return;
    }
    let prior = unsafe { xp_erase(craft, step, ctx) };
    let id = unsafe { step(ctx, 42, &mut XpReply::empty()) };
    reply = XpReply::empty();
    reply.word = id;
    reply.other = owner as u64;
    reply.third_word = craft as u64;
    reply.first = xp_span(&base);
    reply.second = xp_span(&current);
    if unsafe { step(ctx, 41, &mut reply) } == 0 {
        return;
    }
    reply = XpReply::empty();
    reply.word = enhancement as u64;
    reply.other = 0x10;
    reply.third_word = 0;
    unsafe { step(ctx, 34, &mut reply) };
    reply = XpReply::empty();
    reply.word = owner as u64;
    reply.first = xp_span(&current);
    unsafe { step(ctx, 44, &mut reply) };
    if prior >= 0 && prior != owner {
        unsafe { xp_reconcile(prior as u32, step, ctx) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_observe(
    craft: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) {
    if let Some(step) = step {
        unsafe { xp_observe(craft, step, ctx) };
    }
}
unsafe fn xp_recalculate(step: XpStep, ctx: *mut c_void) {
    use alloc::collections::BTreeSet;
    if unsafe { step(ctx, 53, &mut XpReply::empty()) } != 0 {
        return;
    }
    let list = unsafe { step(ctx, 56, &mut XpReply::empty()) } as usize;
    let head = unsafe { xp_ptr(step, ctx, list, 4) };
    let count = unsafe { xp_u32(step, ctx, list, 8, 0) };
    if head == 0 || count > 65536 {
        return;
    }
    let mut seen = BTreeSet::new();
    let mut nodes = BTreeSet::new();
    let mut objects = Vec::new();
    let mut node = unsafe { xp_ptr(step, ctx, head, 0) };
    for _ in 0..count {
        if node == 0
            || node == head
            || !nodes.insert(node)
            || unsafe { xp_read(step, ctx, 50, node, 12, 0) } == 0
        {
            return;
        }
        let object = unsafe { xp_ptr(step, ctx, node, 8) };
        if object == 0 || !seen.insert(object) {
            return;
        }
        objects.push(object);
        node = unsafe { xp_ptr(step, ctx, node, 0) };
    }
    if node != head {
        return;
    }
    unsafe { step(ctx, 45, &mut XpReply::empty()) };
    for object in objects {
        unsafe { xp_observe(object, step, ctx) };
    }
    unsafe { step(ctx, 46, &mut XpReply::empty()) };
    loop {
        let object = unsafe { step(ctx, 47, &mut XpReply::empty()) } as usize;
        if object == 0 {
            break;
        }
        if !seen.contains(&object) {
            unsafe { xp_erase(object, step, ctx) };
        }
    }
    let mut reply = XpReply::empty();
    reply.word = 1;
    unsafe {
        step(ctx, 48, &mut reply);
        step(ctx, 49, &mut reply)
    };
    for player in 0..9 {
        unsafe { xp_reconcile(player, step, ctx) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_recalculate(
    syncing: u8,
    step: Option<XpStep>,
    ctx: *mut c_void,
) {
    if syncing != 0 {
        return;
    }
    if let Some(step) = step {
        unsafe { xp_recalculate(step, ctx) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_craft_event(
    valid: u8,
    kind: u32,
    craft: usize,
    seeded: u8,
    step: Option<XpStep>,
    ctx: *mut c_void,
) {
    if valid == 0 || craft == 0 {
        return;
    }
    let Some(step) = step else {
        return;
    };
    if !unsafe { xp_any_enabled(step, ctx) } {
        return;
    }
    if kind == 2 {
        let prior = unsafe { xp_erase(craft, step, ctx) };
        if prior >= 0 {
            unsafe { xp_reconcile(prior as u32, step, ctx) };
        }
        return;
    }
    if kind == 3 {
        let mut reply = XpReply::empty();
        reply.word = 0;
        unsafe { step(ctx, 48, &mut reply) };
        return;
    }
    if seeded == 0 {
        unsafe { xp_recalculate(step, ctx) };
    }
    unsafe { xp_observe(craft, step, ctx) };
    let owner = unsafe { xp_u32(step, ctx, craft, 0xec, u32::MAX) } as i32;
    if owner >= 0 {
        unsafe { xp_reconcile(owner as u32, step, ctx) };
    }
}
unsafe fn player_for_team(team: usize, step: XpStep, ctx: *mut c_void) -> i32 {
    if team == 0 {
        return -1;
    }
    for player in 0..9 {
        let mut reply = XpReply::empty();
        reply.word = player;
        if unsafe { step(ctx, 64, &mut reply) } as usize == team {
            return player as i32;
        }
    }
    -1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_save(
    ready: u8,
    team: usize,
    writer: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    let mut reply = XpReply::empty();
    reply.word = team as u64;
    reply.other = writer as u64;
    let result = unsafe { step(ctx, 62, &mut reply) } as u32;
    if result & 0xff == 0 || ready == 0 {
        return result;
    }
    let player = unsafe { player_for_team(team, step, ctx) };
    if player < 0 || !unsafe { xp_configure(player as u32, step, ctx) } {
        return result;
    }
    reply = XpReply::empty();
    reply.word = player as u64;
    if unsafe { step(ctx, 65, &mut reply) } != 33 {
        return 0;
    }
    let Some(bytes) = (unsafe { reply.first.get() }) else {
        return 0;
    };
    let bytes = bytes.to_vec();
    reply = XpReply::empty();
    reply.word = writer as u64;
    reply.first = xp_span(&bytes);
    u32::from(unsafe { step(ctx, 66, &mut reply) } != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_load(
    ready: u8,
    team: usize,
    reader: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    let mut reply = XpReply::empty();
    reply.word = team as u64;
    reply.other = reader as u64;
    let result = unsafe { step(ctx, 67, &mut reply) } as u32;
    if result & 0xff == 0 || ready == 0 {
        return result;
    }
    let player = unsafe { player_for_team(team, step, ctx) };
    if player < 0 || !unsafe { xp_configure(player as u32, step, ctx) } {
        return result;
    }
    reply = XpReply::empty();
    reply.word = reader as u64;
    let read = unsafe { step(ctx, 68, &mut reply) } != 0;
    let restored = if read {
        let Some(bytes) = (unsafe { reply.first.get() }) else {
            return 0;
        };
        let bytes = bytes.to_vec();
        reply = XpReply::empty();
        reply.word = player as u64;
        reply.first = xp_span(&bytes);
        (unsafe { step(ctx, 69, &mut reply) }) != 0
    } else {
        false
    };
    reply = XpReply::empty();
    reply.word = player as u64;
    if !restored || unsafe { step(ctx, 21, &mut reply) } == 0 || reply.other == 0 {
        unsafe { step(ctx, 70, &mut reply) };
        return 0;
    }
    reply = XpReply::empty();
    reply.word = 0;
    unsafe { step(ctx, 48, &mut reply) };
    reply.word = player as u64;
    unsafe { step(ctx, 71, &mut reply) };
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_clean(step: Option<XpStep>, ctx: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    for player in 0..9 {
        let mut reply = XpReply::empty();
        reply.word = player;
        unsafe { step(ctx, 72, &mut reply) };
    }
    unsafe {
        step(ctx, 73, &mut XpReply::empty());
        step(ctx, 74, &mut XpReply::empty())
    };
}
unsafe fn xp_float(step: XpStep, ctx: *mut c_void, p: usize, offset: u64) -> f32 {
    f32::from_bits(unsafe { xp_read(step, ctx, 33, p, offset, 0) } as u32)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_award(
    ready: u8,
    _class_award: u8,
    killer: usize,
    victim: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> i32 {
    if ready == 0 {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    let owner = unsafe { xp_u32(step, ctx, killer, 0xec, u32::MAX) } as i32;
    if owner < 0 || !unsafe { xp_configure(owner as u32, step, ctx) } {
        return 0;
    }
    if unsafe { step(ctx, 55, &mut XpReply::empty()) } == 0 {
        unsafe { xp_recalculate(step, ctx) };
    }
    unsafe { xp_observe(killer, step, ctx) };
    let klass = unsafe { xp_ptr(step, ctx, victim, 4) };
    let reward = unsafe { xp_float(step, ctx, klass, 0xb4) } as f64;
    let mut reply = XpReply::empty();
    reply.word = owner as u64;
    reply.number = reward;
    unsafe { step(ctx, 75, &mut reply) };
    let progress = XpProgress {
        levels_gained: reply.word as u32,
        consumed: (reply.other & 1) as u32,
        level: (reply.other >> 1) as u32,
        pool: reply.number,
        threshold: reply.second_number,
    };
    unsafe { xp_report(owner as u32, progress, step, ctx) };
    if progress.consumed == 0 {
        unsafe { step(ctx, 76, &mut XpReply::empty()) };
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_level(
    ready: u8,
    craft: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> i32 {
    if ready == 0 {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    let owner = unsafe { xp_u32(step, ctx, craft, 0xec, u32::MAX) } as i32;
    if owner < 0 || !unsafe { xp_configure(owner as u32, step, ctx) } {
        return 0;
    }
    if unsafe { step(ctx, 55, &mut XpReply::empty()) } == 0 {
        unsafe { xp_recalculate(step, ctx) };
    }
    unsafe { xp_observe(craft, step, ctx) };
    let mut reply = XpReply::empty();
    reply.word = craft as u64;
    let id = unsafe { step(ctx, 35, &mut reply) };
    if !unsafe { live(craft, step, ctx) }
        || id == 0
        || unsafe { step(ctx, 54, &mut XpReply::empty()) } != 0
    {
        return 1;
    }
    reply = XpReply::empty();
    reply.word = id;
    let target = unsafe { step(ctx, 77, &mut reply) } as i32;
    let enhancement = unsafe { xp_ptr(step, ctx, craft, 0x1a4) };
    let klass = unsafe { xp_ptr(step, ctx, enhancement, 4) };
    let current = unsafe { xp_u32(step, ctx, klass, 0xac, 0) } as i32;
    let required = unsafe { xp_float(step, ctx, klass, 0xb0) };
    if target == 0 || target <= current || !required.is_finite() || required <= 0.0 {
        reply = XpReply::empty();
        reply.word = enhancement as u64;
        reply.other = 0x10;
        unsafe { step(ctx, 34, &mut reply) };
        return 1;
    }
    reply = XpReply::empty();
    reply.word = enhancement as u64;
    reply.other = 0x10;
    reply.third_word = required.to_bits() as u64;
    if unsafe { step(ctx, 34, &mut reply) } == 0 {
        return 1;
    }
    reply = XpReply::empty();
    reply.word = 1;
    unsafe { step(ctx, 78, &mut reply) };
    reply.word = craft as u64;
    unsafe { step(ctx, 79, &mut reply) };
    reply.word = 0;
    unsafe { step(ctx, 78, &mut reply) };
    unsafe { xp_recalculate(step, ctx) };
    reply = XpReply::empty();
    reply.word = craft as u64;
    let remaining = unsafe { step(ctx, 35, &mut reply) };
    if remaining == id && unsafe { xp_ptr(step, ctx, craft, 0x1a4) } == enhancement {
        reply = XpReply::empty();
        reply.word = enhancement as u64;
        reply.other = 0x10;
        unsafe {
            step(ctx, 34, &mut reply);
            step(ctx, 80, &mut XpReply::empty())
        };
    }
    1
}
unsafe fn xp_tech(player: u32, klass: usize, step: XpStep, ctx: *mut c_void) -> bool {
    let id_ptr = unsafe { xp_ptr(step, ctx, klass, 0x1cc) };
    let id = unsafe { xp_u32(step, ctx, id_ptr, 0, 0) };
    if id == 0 || id > 8192 {
        return false;
    }
    let trees = unsafe { step(ctx, 84, &mut XpReply::empty()) } as usize;
    let tree = unsafe { xp_ptr(step, ctx, trees, player as u64 * 4) };
    let items = unsafe { xp_ptr(step, ctx, tree, 0x0c) };
    if tree == 0 || items == 0 {
        return false;
    }
    if unsafe { xp_ptr(step, ctx, items, (id as u64 - 1) * 4) } == 0 {
        return true;
    }
    let mut reply = XpReply::empty();
    reply.word = tree as u64;
    reply.other = id as u64;
    (unsafe { step(ctx, 85, &mut reply) }) & 0xff != 0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_resolve(
    ready: u8,
    producer: usize,
    base_class: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> usize {
    if ready == 0 {
        return base_class;
    }
    let Some(step) = step else {
        return base_class;
    };
    let owner = unsafe { xp_u32(step, ctx, producer, 0xec, u32::MAX) } as i32;
    if owner < 0 || !unsafe { xp_configure(owner as u32, step, ctx) } {
        return base_class;
    }
    let mut reply = XpReply::empty();
    reply.word = base_class as u64;
    if unsafe { step(ctx, 81, &mut reply) } == 0 {
        return base_class;
    }
    let Some(name) = (unsafe { reply.first.get() }) else {
        return base_class;
    };
    let name = name.to_vec();
    if !unsafe { chain(&name, step, ctx) } {
        return 0;
    }
    let base = unsafe { family_owned(&name, step, ctx) };
    if base.is_empty() || !unsafe { chain(&base, step, ctx) } {
        return 0;
    }
    reply = XpReply::empty();
    reply.word = owner as u64;
    if unsafe { step(ctx, 21, &mut reply) } == 0 {
        return 0;
    }
    let pool_level = reply.third_word;
    reply = XpReply::empty();
    reply.first = xp_span(&name);
    let native = unsafe { step(ctx, 82, &mut reply) };
    reply = XpReply::empty();
    reply.first = xp_span(&base);
    reply.second = xp_span(&name);
    reply.word = pool_level.max(native);
    if unsafe { step(ctx, 83, &mut reply) } == 0 {
        return 0;
    }
    let Some(resolved) = (unsafe { reply.third.get() }) else {
        return 0;
    };
    let resolved = resolved.to_vec();
    reply = XpReply::empty();
    reply.first = xp_span(&resolved);
    let result = unsafe { step(ctx, 0, &mut reply) } as usize;
    reply = XpReply::empty();
    reply.first = xp_span(&base);
    let native_base = unsafe { step(ctx, 0, &mut reply) } as usize;
    if result == 0
        || !unsafe { xp_tech(owner as u32, native_base, step, ctx) }
        || !unsafe { xp_tech(owner as u32, result, step, ctx) }
    {
        return 0;
    }
    if result != base_class {
        reply = XpReply::empty();
        reply.word = owner as u64;
        reply.first = xp_span(&name);
        reply.second = xp_span(&resolved);
        unsafe { step(ctx, 86, &mut reply) };
    }
    result
}
#[repr(C)]
pub struct XpPublicProgress {
    level: u32,
    current: f64,
    required: f64,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_get_progress(
    ready: u8,
    player: u32,
    out: *mut XpPublicProgress,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u8 {
    if ready == 0 || player >= 9 || out.is_null() {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    let mut reply = XpReply::empty();
    reply.word = player as u64;
    if unsafe { step(ctx, 21, &mut reply) } == 0 || reply.other == 0 {
        return 0;
    }
    let level = reply.third_word as u32;
    let current = reply.number;
    reply = XpReply::empty();
    reply.word = player as u64;
    unsafe { step(ctx, 88, &mut reply) };
    unsafe {
        *out = XpPublicProgress {
            level,
            current,
            required: reply.number,
        }
    };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_is_enabled(
    ready: u8,
    object: usize,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u8 {
    if ready == 0 {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    let owner = unsafe { xp_u32(step, ctx, object, 0xec, u32::MAX) } as i32;
    if !(0..9).contains(&owner) {
        return 0;
    }
    let mut reply = XpReply::empty();
    reply.word = owner as u64;
    u8::from(unsafe { step(ctx, 20, &mut reply) } != 0 && reply.other != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_init(
    valid: u8,
    step: Option<XpStep>,
    ctx: *mut c_void,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(ctx, 90, &mut XpReply::empty()) } == 0 {
        return 0;
    }
    for i in 0..3 {
        let mut reply = XpReply::empty();
        reply.word = i;
        if unsafe { step(ctx, 91, &mut reply) } == 0 {
            return 0;
        }
    }
    let mut installed = unsafe { step(ctx, 92, &mut XpReply::empty()) } != 0;
    if installed {
        for i in 0..7 {
            let mut reply = XpReply::empty();
            reply.word = i;
            if unsafe { step(ctx, 93, &mut reply) } == 0 {
                installed = false;
                break;
            }
        }
    }
    if installed {
        unsafe { step(ctx, 94, &mut XpReply::empty()) };
        for i in 0..4 {
            let mut reply = XpReply::empty();
            reply.word = i;
            if unsafe { step(ctx, 95, &mut reply) } == 0 {
                installed = false;
                break;
            }
        }
        if installed {
            for i in 0..3 {
                let mut reply = XpReply::empty();
                reply.word = i;
                if unsafe { step(ctx, 96, &mut reply) } == 0 {
                    installed = false;
                    break;
                }
            }
        }
    }
    let mut reply = XpReply::empty();
    reply.word = u64::from(installed);
    unsafe { step(ctx, 97, &mut reply) };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_xp_shutdown(step: Option<XpStep>, ctx: *mut c_void) {
    if let Some(step) = step {
        unsafe { step(ctx, 98, &mut XpReply::empty()) };
    }
}
