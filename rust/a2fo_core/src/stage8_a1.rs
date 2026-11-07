//! A1 compatibility module text policies; native strings and CRT locale stay
//! native. Hook addresses, assembly splices and engine layouts stay opaque.
use core::ffi::c_void;
type Classify = unsafe extern "C" fn(u8, *mut c_void) -> u8;
type Emit = unsafe extern "C" fn(*mut c_void, u8, u32) -> u8;
type Cmp = unsafe extern "C" fn(*const u8, *const u8, *mut c_void) -> i32;
type NCmp = unsafe extern "C" fn(*const u8, *const u8, usize, *mut c_void) -> i32;
type Length = unsafe extern "C" fn(*const u8, *mut c_void) -> usize;
type Scan = unsafe extern "C" fn(*const u8, *mut c_void) -> *const u8;
type A1Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
fn a1_op(ctx: *mut c_void, op: A1Op, id: u32) -> Option<u64> {
    let value = unsafe { op(ctx, id) };
    (value != u64::MAX).then_some(value)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_neutral_odf(
    value: *const u8,
    neutral: *const u8,
    cmp: Cmp,
    ctx: *mut c_void,
) -> u8 {
    if value.is_null() || unsafe { value.read() } == 0 {
        return 0;
    }
    let mut basename = value;
    let mut cursor = value;
    while unsafe { cursor.read() } != 0 {
        let byte = unsafe { cursor.read() };
        if byte == b'\\' || byte == b'/' {
            basename = unsafe { cursor.add(1) };
        }
        cursor = unsafe { cursor.add(1) };
    }
    let mut length = 0;
    while unsafe { basename.add(length).read() } != 0 {
        length += 1;
    }
    let mut neutral_len = 0;
    while unsafe { neutral.add(neutral_len).read() } != 0 {
        neutral_len += 1;
    }
    if length == neutral_len && unsafe { cmp(basename, neutral, ctx) } == 0 {
        return 1;
    }
    let alternate = c"norace".as_ptr().cast::<u8>();
    u8::from(length == 6 && unsafe { cmp(basename, alternate, ctx) } == 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_scout_path(
    name: *const u8,
    scan: Scan,
    cmp: Cmp,
    ctx: *mut c_void,
) -> u8 {
    if name.is_null() {
        return 0;
    }
    let end = unsafe { scan(name, ctx) };
    if end.is_null() {
        return 0;
    }
    let mut basename = name;
    let mut cursor = name;
    while cursor != end {
        let byte = unsafe { cursor.read() };
        if byte == b'\\' || byte == b'/' {
            basename = unsafe { cursor.add(1) };
        }
        cursor = unsafe { cursor.add(1) };
    }
    u8::from(unsafe { cmp(basename, c"scout.odf".as_ptr().cast(), ctx) } == 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_ascii_equal(
    value: *const u8,
    expected: *const u8,
    lower: Classify,
    ctx: *mut c_void,
) -> u8 {
    if value.is_null() || expected.is_null() {
        return 0;
    }
    let mut lhs = value;
    let mut rhs = expected;
    while unsafe { lhs.read() } != 0 && unsafe { rhs.read() } != 0 {
        let left = unsafe { lhs.read() };
        lhs = unsafe { lhs.add(1) };
        let right = unsafe { rhs.read() };
        rhs = unsafe { rhs.add(1) };
        if unsafe { lower(left, ctx) } != unsafe { lower(right, ctx) } {
            return 0;
        }
    }
    u8::from(unsafe { lhs.read() } == 0 && unsafe { rhs.read() } == 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_contains_ci(
    source: *const u8,
    token: *const u8,
    length: Length,
    lower: Classify,
    ctx: *mut c_void,
) -> u8 {
    if source.is_null() || token.is_null() || unsafe { token.read() } == 0 {
        return 0;
    }
    let token_len = unsafe { length(token, ctx) };
    if token_len == 0 {
        return 0;
    }
    let mut index = 0;
    while unsafe { source.add(index).read() } != 0 {
        let mut matched = true;
        for offset in 0..token_len {
            let current = unsafe { source.add(index + offset).read() };
            if current == 0
                || unsafe { lower(current, ctx) } != unsafe { lower(token.add(offset).read(), ctx) }
            {
                matched = false;
                break;
            }
        }
        if matched {
            return 1;
        }
        index += 1;
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_numbered_family(
    odf: *const u8,
    family: *const u8,
    cmp: Cmp,
    ncmp: NCmp,
    length: Length,
    ctx: *mut c_void,
) -> u8 {
    if odf.is_null()
        || unsafe { odf.read() } == 0
        || family.is_null()
        || unsafe { family.read() } == 0
    {
        return 0;
    }
    let mut basename = odf;
    let mut cursor = odf;
    while unsafe { cursor.read() } != 0 {
        let byte = unsafe { cursor.read() };
        if byte == b'\\' || byte == b'/' {
            basename = unsafe { cursor.add(1) };
        }
        cursor = unsafe { cursor.add(1) };
    }
    let mut normalized = [0u8; 128];
    let mut len = 0;
    while unsafe { basename.add(len).read() } != 0 && len + 1 < normalized.len() {
        let mut byte = unsafe { basename.add(len).read() };
        if byte.is_ascii_uppercase() {
            byte = byte - b'A' + b'a';
        }
        normalized[len] = byte;
        len += 1;
    }
    if len > 4
        && unsafe {
            cmp(
                normalized.as_ptr().add(len - 4),
                c".odf".as_ptr().cast(),
                ctx,
            )
        } == 0
    {
        len -= 4;
        normalized[len] = 0;
    }
    let family_len = unsafe { length(family, ctx) };
    if len < family_len || unsafe { ncmp(normalized.as_ptr(), family, family_len, ctx) } != 0 {
        return 0;
    }
    u8::from(normalized[family_len..len].iter().all(u8::is_ascii_digit))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_join_path(root: *const u8, ctx: *mut c_void, op: A1Op) {
    if root.is_null() || unsafe { root.read() } == 0 {
        return;
    }
    if a1_op(ctx, op, 0).is_none() {
        return;
    } // original string constructor
    if a1_op(ctx, op, 1) == Some(0) && a1_op(ctx, op, 2).is_none() {
        return;
    }
    let _ = a1_op(ctx, op, 3); // original name append
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_mod_chain(ctx: *mut c_void, op: A1Op) -> u8 {
    for id in 10..=12 {
        if a1_op(ctx, op, id) != Some(1) {
            return 0;
        }
    }
    let Some(count) = a1_op(ctx, op, 13) else {
        return 0;
    };
    if count == 0 || count > 4096 {
        return 0;
    }
    for index in 0..count {
        if a1_op(ctx, op, 0x10000 + index as u32).is_none() {
            return 0;
        }
        if a1_op(ctx, op, 14).is_none() {
            return 0;
        }
        if a1_op(ctx, op, 15) == Some(1) {
            let _ = a1_op(ctx, op, 19);
            continue;
        }
        if a1_op(ctx, op, 16).is_none() {
            return 0;
        }
        let valid = a1_op(ctx, op, 17) == Some(1);
        let file = valid && a1_op(ctx, op, 18) == Some(1);
        let _ = a1_op(ctx, op, 19);
        if file {
            return 1;
        }
    }
    0
}
/// Safety: bytes is readable for len; emit owns the original native output and
/// contains any native exception. No native pointer is retained by Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_comments(
    bytes: *const u8,
    len: usize,
    emit: Emit,
    ctx: *mut c_void,
) -> u8 {
    let mut line = false;
    let mut block = false;
    let mut i = 0;
    while i < len {
        let current = unsafe { bytes.add(i).read() };
        let next = if i + 1 < len {
            unsafe { bytes.add(i + 1).read() }
        } else {
            0
        };
        let (value, count) = if line {
            if current == b'\n' || current == b'\r' {
                line = false;
                (current, 1)
            } else {
                (b' ', 1)
            }
        } else if block {
            if current == b'*' && next == b'/' {
                i += 1;
                block = false;
                (b' ', 2)
            } else {
                (
                    if current == b'\n' || current == b'\r' {
                        current
                    } else {
                        b' '
                    },
                    1,
                )
            }
        } else if current == b'/' && next == b'/' {
            i += 1;
            line = true;
            (b' ', 2)
        } else if current == b'/' && next == b'*' {
            i += 1;
            block = true;
            (b' ', 2)
        } else {
            (current, 1)
        };
        if unsafe { emit(ctx, value, count) } == 0 {
            return 0;
        }
        i += 1;
    }
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_a1_identifier(ch: u8) -> u8 {
    u8::from(ch.is_ascii_alphanumeric() || ch == b'_')
}
/// Safety: bytes is readable up to equals, matching the original substr index
/// contract; begin/end are valid distinct outputs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_assignment(
    bytes: *const u8,
    equals: usize,
    begin: *mut usize,
    end: *mut usize,
) {
    let mut last = equals;
    while last != 0 && a2fo_rs_stage8_a1_identifier(unsafe { bytes.add(last - 1).read() }) == 0 {
        last -= 1;
    }
    let mut first = last;
    while first != 0 && a2fo_rs_stage8_a1_identifier(unsafe { bytes.add(first - 1).read() }) != 0 {
        first -= 1;
    }
    unsafe {
        begin.write(first);
        end.write(last);
    }
}
/// Safety: source is the original readable NUL string; output is writable for
/// size bytes. Aliases retain the original initial output[0]=0 before reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_token(
    source: *const u8,
    output: *mut u8,
    size: usize,
    space: Classify,
    ctx: *mut c_void,
) {
    if output.is_null() || size == 0 {
        return;
    }
    unsafe {
        output.write(0);
    }
    if source.is_null() {
        return;
    }
    let mut first = 0;
    while unsafe { source.add(first).read() } != 0
        && unsafe { space(source.add(first).read(), ctx) } != 0
    {
        first += 1;
    }
    let mut end = first;
    while unsafe { source.add(end).read() } != 0 {
        end += 1;
    }
    while end > first && unsafe { space(source.add(end - 1).read(), ctx) } != 0 {
        end -= 1;
    }
    if end - first >= 2 {
        let head = unsafe { source.add(first).read() };
        let tail = unsafe { source.add(end - 1).read() };
        if (head == b'"' && tail == b'"') || (head == b'\'' && tail == b'\'') {
            first += 1;
            end -= 1;
            while first < end && unsafe { space(source.add(first).read(), ctx) } != 0 {
                first += 1;
            }
            while end > first && unsafe { space(source.add(end - 1).read(), ctx) } != 0 {
                end -= 1;
            }
        }
    }
    let mut len = 0;
    while first < end && len + 1 < size {
        let ch = unsafe { source.add(first).read() };
        first += 1;
        unsafe {
            output.add(len).write(ch.to_ascii_lowercase());
        }
        len += 1;
    }
    unsafe {
        output.add(len).write(0);
    }
}
/// Safety: bytes is the native copy of the string_view, writable for len;
/// native CRT callbacks cannot throw and preserve the process locale.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_bool(
    bytes: *mut u8,
    len: usize,
    space: Classify,
    lower: Classify,
    ctx: *mut c_void,
) -> u32 {
    let mut first = 0;
    while first < len && unsafe { space(bytes.add(first).read(), ctx) } != 0 {
        first += 1;
    }
    let mut end = len;
    while end > first && unsafe { space(bytes.add(end - 1).read(), ctx) } != 0 {
        end -= 1;
    }
    if first == end {
        return 0;
    }
    for i in first..end {
        unsafe {
            bytes.add(i).write(lower(bytes.add(i).read(), ctx));
        }
    }
    for (result, tokens) in [
        (2, [b"1" as &[u8], b"true", b"yes", b"on", b"enabled"]),
        (1, [b"0" as &[u8], b"false", b"no", b"off", b"disabled"]),
    ] {
        for token in tokens {
            if token.len() == end - first
                && token
                    .iter()
                    .enumerate()
                    .all(|(i, ch)| unsafe { bytes.add(first + i).read() } == *ch)
            {
                return result;
            }
        }
    }
    0
}

type Readable = unsafe extern "C" fn(*const u8, usize, *mut c_void) -> u8;
/// Safety: output is writable for size; readable performs the original native
/// VirtualQuery preflight before each source byte read. Input aliases retain
/// the original output clear before the first read.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_printable(
    source: *const u8,
    output: *mut u8,
    size: usize,
    readable: Readable,
    ctx: *mut c_void,
) {
    if output.is_null() || size == 0 {
        return;
    }
    unsafe {
        output.write(0);
    }
    if source.is_null() {
        return;
    }
    let mut len = 0;
    while len + 1 < size && unsafe { readable(source.wrapping_add(len), 1, ctx) } != 0 {
        let ch = unsafe { source.wrapping_add(len).read() };
        if ch == 0 {
            break;
        }
        unsafe {
            output
                .add(len)
                .write(if (0x20..0x7f).contains(&ch) { ch } else { b'?' });
        }
        len += 1;
    }
    unsafe {
        output.add(len).write(0);
    }
}
fn race_space(ch: u8) -> bool {
    matches!(ch, b' ' | b'\t' | b'\r' | b'\n')
}
/// Safety: source has the original native readable text contract; output is
/// writable for size bytes. The native array is cleared before source reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_race(source: *const u8, output: *mut u8, size: usize) {
    if output.is_null() {
        return;
    }
    for i in 0..size {
        unsafe {
            output.add(i).write(0);
        }
    }
    if source.is_null() {
        return;
    }
    let mut first = 0;
    while race_space(unsafe { source.add(first).read() }) {
        first += 1;
    }
    let mut len = 0;
    while unsafe { source.add(first + len).read() } != 0 && len + 1 < size {
        len += 1;
    }
    while len != 0 && race_space(unsafe { source.add(first + len - 1).read() }) {
        len -= 1;
    }
    for i in 0..len {
        unsafe {
            output
                .add(i)
                .write(source.add(first + i).read().to_ascii_lowercase());
        }
    }
}
