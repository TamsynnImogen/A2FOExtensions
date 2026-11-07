//! A2FOCheats runtime workflow through synchronous native primitives.
use alloc::vec::Vec;
use core::{ffi::c_void, ptr, slice};
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}
impl Bytes {
    fn empty() -> Self {
        Self {
            data: ptr::null(),
            size: 0,
        }
    }
    unsafe fn get<'a>(self) -> Option<&'a [u8]> {
        if self.data.is_null() {
            if self.size == 0 { Some(&[]) } else { None }
        } else {
            Some(unsafe { slice::from_raw_parts(self.data, self.size) })
        }
    }
}
fn span(bytes: &[u8]) -> Bytes {
    Bytes {
        data: bytes.as_ptr(),
        size: bytes.len(),
    }
}
#[repr(C)]
pub struct Value {
    first: Bytes,
    second: Bytes,
    word: usize,
    other: usize,
    number: f32,
    second_number: f32,
    vector: [f32; 12],
}
impl Value {
    fn empty() -> Self {
        Self {
            first: Bytes::empty(),
            second: Bytes::empty(),
            word: 0,
            other: 0,
            number: 0.0,
            second_number: 0.0,
            vector: [0.0; 12],
        }
    }
}
#[repr(C)]
pub struct Grants {
    values: [f32; 5],
}
type Step = unsafe extern "C" fn(*mut c_void, u32, *mut Value) -> usize;
fn strip_comments(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(input.len());
    let (mut line, mut block) = (false, false);
    let mut i = 0;
    while i < input.len() {
        let c = input[i];
        let next = input.get(i + 1).copied().unwrap_or(0);
        if line {
            if c == b'\n' || c == b'\r' {
                line = false;
                out.push(c)
            } else {
                out.push(b' ')
            };
            i += 1;
            continue;
        }
        if block {
            if c == b'*' && next == b'/' {
                out.extend_from_slice(b"  ");
                i += 2;
                block = false;
                continue;
            }
            out.push(if c == b'\n' || c == b'\r' { c } else { b' ' });
            i += 1;
            continue;
        }
        if c == b'/' && next == b'/' {
            out.extend_from_slice(b"  ");
            i += 2;
            line = true;
            continue;
        }
        if c == b'/' && next == b'*' {
            out.extend_from_slice(b"  ");
            i += 2;
            block = true;
            continue;
        }
        out.push(c);
        i += 1;
    }
    out
}
fn ident(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_'
}
fn assignment_name(statement: &[u8], equals: usize) -> &[u8] {
    let mut end = equals;
    while end != 0 && !ident(statement[end - 1]) {
        end -= 1;
    }
    let mut begin = end;
    while begin != 0 && ident(statement[begin - 1]) {
        begin -= 1;
    }
    &statement[begin..end]
}
fn field_index(name: &[u8]) -> Option<usize> {
    const NAMES: [&[u8]; 5] = [
        b"SHOWMETHEMONEY_DILITHIUM",
        b"SHOWMETHEMONEY_TRITANIUM",
        b"SHOWMETHEMONEY_METAL",
        b"SHOWMETHEMONEY_SUPPLIES",
        b"SHOWMETHEMONEY_CREW",
    ];
    NAMES.iter().position(|candidate| *candidate == name)
}
fn path(root: &[u8]) -> Vec<u8> {
    let mut result = root.to_vec();
    if !result.ends_with(b"\\") && !result.ends_with(b"/") {
        result.push(b'\\');
    }
    result.extend_from_slice(b"RTS_CFG.h");
    result
}
fn space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\r' | b'\n')
}
unsafe fn parse_literal(value: &[u8], step: Step, ctx: *mut c_void) -> Option<f32> {
    let logical = value.iter().position(|&c| c == 0).unwrap_or(value.len());
    let mut begin = 0;
    while begin < value.len() && space(value[begin]) {
        begin += 1;
    }
    let mut reply = Value::empty();
    reply.first = span(&value[begin..]);
    if unsafe { step(ctx, 3, &mut reply) } == 0 {
        return None;
    }
    let parsed = reply.number;
    let consumed = reply.word;
    if consumed == 0 || reply.other != 0 || !parsed.is_finite() {
        return None;
    }
    let mut end = begin.checked_add(consumed)?;
    if end > logical {
        return None;
    }
    while end < logical && space(value[end]) {
        end += 1;
    }
    if end < logical && matches!(value[end], b'f' | b'F') {
        end += 1;
        while end < logical && space(value[end]) {
            end += 1;
        }
    }
    if end != logical || !(0.0..=100_000_000.0).contains(&parsed) {
        return None;
    }
    Some(parsed)
}
unsafe fn apply(
    contents: &[u8],
    path: &[u8],
    step: Step,
    ctx: *mut c_void,
    grants: &mut Grants,
) -> u32 {
    let source = strip_comments(contents);
    let mut applied = 0;
    let mut begin = 0;
    while begin < source.len() {
        let semicolon = source[begin..]
            .iter()
            .position(|&c| c == b';')
            .map(|i| begin + i);
        let end = semicolon.unwrap_or(source.len());
        let statement = &source[begin..end];
        if let Some(equals) = statement.iter().position(|&c| c == b'=') {
            let name = assignment_name(statement, equals);
            if let Some(index) = field_index(name) {
                if let Some(value) = unsafe { parse_literal(&statement[equals + 1..], step, ctx) } {
                    grants.values[index] = value;
                    applied += 1;
                } else {
                    let mut reply = Value::empty();
                    reply.first = span(name);
                    reply.second = span(path);
                    unsafe { step(ctx, 4, &mut reply) };
                }
            }
        }
        let Some(semicolon) = semicolon else {
            break;
        };
        begin = semicolon + 1;
    }
    applied
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_cheats_load(
    step: Option<Step>,
    ctx: *mut c_void,
    out: *mut Grants,
) {
    let (Some(step), false) = (step, out.is_null()) else {
        return;
    };
    let mut grants = Grants {
        values: [10_000.0; 5],
    };
    let mut configured = 0u32;
    let count = unsafe { step(ctx, 0, &mut Value::empty()) };
    for index in 0..count {
        let mut reply = Value::empty();
        reply.word = index;
        if unsafe { step(ctx, 1, &mut reply) } == 0 {
            continue;
        }
        let Some(root) = (unsafe { reply.first.get() }) else {
            continue;
        };
        if root.is_empty() {
            continue;
        }
        let file = path(root);
        reply = Value::empty();
        reply.first = span(&file);
        if unsafe { step(ctx, 2, &mut reply) } == 0 {
            continue;
        }
        let Some(contents) = (unsafe { reply.second.get() }) else {
            continue;
        };
        let contents = contents.to_vec();
        let applied = unsafe { apply(&contents, &file, step, ctx, &mut grants) };
        if applied != 0 {
            configured += applied;
            reply = Value::empty();
            reply.first = span(&file);
            reply.word = applied as usize;
            unsafe { step(ctx, 5, &mut reply) };
        }
    }
    let mut reply = Value::empty();
    reply.word = configured as usize;
    reply.vector[..5].copy_from_slice(&grants.values);
    unsafe { *out = grants };
    unsafe { step(ctx, 6, &mut reply) };
}
unsafe fn selection(kind: u32, step: Step, ctx: *mut c_void) -> bool {
    let count = unsafe { step(ctx, 20, &mut Value::empty()) };
    if count == 0 {
        return false;
    }
    let mut acted = false;
    for index in (0..count).rev() {
        let mut v = Value::empty();
        v.word = index;
        let craft = unsafe { step(ctx, 21, &mut v) };
        if craft == 0 {
            continue;
        }
        v = Value::empty();
        v.word = craft;
        match kind {
            0 => {
                if unsafe { step(ctx, 22, &mut v) } == 0 {
                    continue;
                }
                if unsafe { step(ctx, 23, &mut v) } == 0 {
                    continue;
                }
                unsafe { step(ctx, 24, &mut v) };
                acted = true;
            }
            1 => {
                unsafe { step(ctx, 25, &mut v) };
                acted = true;
            }
            2 => {
                let team = unsafe { step(ctx, 26, &mut v) } as u32 as i32;
                if !(0..9).contains(&team) {
                    continue;
                }
                v.other = team as usize;
                if unsafe { step(ctx, 27, &mut v) } == 0 {
                    continue;
                }
                unsafe { step(ctx, 28, &mut v) };
                acted = true;
                break;
            }
            _ => return false,
        }
    }
    acted
}
unsafe fn money(step: Step, ctx: *mut c_void) -> bool {
    if unsafe { step(ctx, 31, &mut Value::empty()) } == 0 {
        unsafe { step(ctx, 42, &mut Value::empty()) };
        return false;
    }
    let team = unsafe { step(ctx, 32, &mut Value::empty()) };
    if team == 0 {
        unsafe { step(ctx, 43, &mut Value::empty()) };
        return false;
    }
    let mut v = Value::empty();
    v.word = team;
    let before_read = unsafe { step(ctx, 33, &mut v) } != 0;
    let before = v.number;
    for operation in 34..=39 {
        v = Value::empty();
        v.word = team;
        unsafe { step(ctx, operation, &mut v) };
    }
    v = Value::empty();
    v.word = team;
    let after_read = unsafe { step(ctx, 40, &mut v) } != 0;
    let after = v.number;
    v = Value::empty();
    v.word = team;
    v.other = usize::from(before_read && after_read);
    v.number = before;
    v.second_number = after;
    unsafe { step(ctx, 41, &mut v) };
    true
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_cheats_command(
    kind: u32,
    step: Option<Step>,
    ctx: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    match kind {
        0..=2 => u8::from(unsafe { selection(kind, step, ctx) }),
        3 => {
            unsafe { step(ctx, 29, &mut Value::empty()) };
            u8::from(unsafe { step(ctx, 30, &mut Value::empty()) } != 0)
        }
        4 => u8::from(unsafe { money(step, ctx) }),
        _ => 0,
    }
}
const COMMANDS: [&[u8]; 5] = [b"showmethemoney", b"m", b"dis", b"crash", b"elim"];
unsafe fn locate(command: &[u8], step: Step, ctx: *mut c_void) -> Option<usize> {
    let count = unsafe { step(ctx, 50, &mut Value::empty()) };
    for i in 0..count {
        let mut v = Value::empty();
        v.word = i;
        v.first = span(command);
        if unsafe { step(ctx, 51, &mut v) } == 0 {
            continue;
        }
        let Some(name) = (unsafe { v.second.get() }) else {
            continue;
        };
        if name.len() == command.len() + 1
            && name[..command.len()] == *command
            && name[command.len()] == 0
        {
            return Some(i);
        }
    }
    None
}
unsafe fn ensure(index: usize, step: Step, ctx: *mut c_void) -> bool {
    let command = COMMANDS[index];
    if let Some(slot) = unsafe { locate(command, step, ctx) } {
        let mut v = Value::empty();
        v.word = slot;
        v.other = index;
        return unsafe { step(ctx, 52, &mut v) } != 0;
    }
    let mut v = Value::empty();
    v.word = index;
    v.first = span(command);
    if unsafe { step(ctx, 53, &mut v) } == 0 {
        return false;
    }
    let Some(slot) = (unsafe { locate(command, step, ctx) }) else {
        return false;
    };
    v = Value::empty();
    v.word = slot;
    v.other = index;
    (unsafe { step(ctx, 54, &mut v) }) != 0
}
unsafe fn register(step: Step, ctx: *mut c_void) -> bool {
    let mut success = true;
    for index in 0..5 {
        if !unsafe { ensure(index, step, ctx) } {
            success = false;
        }
    }
    success
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_cheats_register(
    step: Option<Step>,
    ctx: *mut c_void,
) -> u8 {
    step.map_or(0, |step| u8::from(unsafe { register(step, ctx) }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_cheats_chat_init(step: Option<Step>, ctx: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    unsafe { step(ctx, 60, &mut Value::empty()) };
    if !unsafe { register(step, ctx) } {
        unsafe { step(ctx, 61, &mut Value::empty()) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_cheats_init(
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
    if unsafe { step(ctx, 70, &mut Value::empty()) } == 0 {
        unsafe { step(ctx, 71, &mut Value::empty()) };
        return 0;
    }
    let mut grants = Grants { values: [0.0; 5] };
    unsafe { a2fo_rs_runtime_cheats_load(Some(step), ctx, &mut grants) };
    let repurposed = unsafe { step(ctx, 72, &mut Value::empty()) } != 0;
    let mut signatures = true;
    for index in 0..13 {
        if index == 11 && repurposed {
            continue;
        }
        let mut v = Value::empty();
        v.word = index;
        if unsafe { step(ctx, 73, &mut v) } == 0 {
            signatures = false;
            break;
        }
    }
    if !signatures {
        unsafe { step(ctx, 74, &mut Value::empty()) };
        return 0;
    }
    if !repurposed {
        if unsafe { step(ctx, 75, &mut Value::empty()) } == 0 {
            unsafe { step(ctx, 76, &mut Value::empty()) };
            return 0;
        }
    } else {
        unsafe { step(ctx, 77, &mut Value::empty()) };
    }
    if unsafe { step(ctx, 78, &mut Value::empty()) } == 0 {
        let mut v = Value::empty();
        v.word = usize::from(repurposed);
        unsafe { step(ctx, 79, &mut v) };
        return u8::from(!repurposed);
    }
    if unsafe { step(ctx, 81, &mut Value::empty()) } != 0 && !unsafe { register(step, ctx) } {
        unsafe { step(ctx, 82, &mut Value::empty()) };
    }
    unsafe { step(ctx, 83, &mut Value::empty()) };
    1
}
