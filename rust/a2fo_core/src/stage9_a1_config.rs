//! A1 config routing and parsing. Native callbacks retain CRT numeric and file semantics.
use core::ffi::c_void;
type Number = unsafe extern "C" fn(*const u8, usize, *mut usize, *mut f64) -> bool;
type Round = unsafe extern "C" fn(f64, *mut i32) -> bool;
type Step = unsafe extern "C" fn(*mut c_void, u32, u32, *mut *const u8, *mut usize) -> bool;
type Classify = unsafe extern "C" fn(u8) -> u8;
type GuiLine =
    unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut *const u8, *mut usize) -> bool;
type GuiInteger = unsafe extern "C" fn(*mut c_void, usize, *mut usize, *mut i32) -> bool;
type GuiCommit = unsafe extern "C" fn(*mut c_void, *const i32);
type GameplayStep = unsafe extern "C" fn(*mut c_void, u32, u32) -> u32;

fn space(byte: u8) -> bool {
    matches!(byte, b' ' | b'\t' | b'\r' | b'\n')
}

/// Safety: statement is readable for length bytes; callbacks are synchronous and do not mutate it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_config_literal(
    statement: *const u8,
    length: usize,
    equals: usize,
    value: *mut i32,
    number: Number,
    round: Round,
) -> bool {
    if statement.is_null() || value.is_null() || equals >= length {
        return false;
    }
    let mut begin = equals + 1;
    while begin < length && space(unsafe { statement.add(begin).read() }) {
        begin += 1;
    }
    let mut end = 0usize;
    let mut parsed = 0.0f64;
    if !unsafe { number(statement.add(begin), length - begin, &mut end, &mut parsed) } {
        return false;
    }
    let mut cursor = begin + end;
    while cursor < length && space(unsafe { statement.add(cursor).read() }) {
        cursor += 1;
    }
    if cursor < length && matches!(unsafe { statement.add(cursor).read() }, b'f' | b'F') {
        cursor += 1;
        while cursor < length && space(unsafe { statement.add(cursor).read() }) {
            cursor += 1;
        }
    }
    if cursor != length && unsafe { statement.add(cursor).read() } != 0 {
        return false;
    }
    unsafe { round(parsed, value) }
}

fn identifier(statement: &[u8], equals: usize) -> &[u8] {
    let mut end = equals;
    while end > 0 && !(statement[end - 1].is_ascii_alphanumeric() || statement[end - 1] == b'_') {
        end -= 1;
    }
    let mut begin = end;
    while begin > 0
        && (statement[begin - 1].is_ascii_alphanumeric() || statement[begin - 1] == b'_')
    {
        begin -= 1;
    }
    &statement[begin..end]
}

/// Safety: source is stable for length bytes and normal points to five writable i32 values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_config_apply(
    source: *const u8,
    length: usize,
    normal: *mut i32,
    number: Number,
    round: Round,
) -> u32 {
    if source.is_null() || normal.is_null() {
        return 0;
    }
    let mut applied = 0u32;
    let mut begin = 0usize;
    while begin < length {
        let mut end = begin;
        while end < length && unsafe { source.add(end).read() } != b';' {
            end += 1;
        }
        let statement = unsafe { core::slice::from_raw_parts(source.add(begin), end - begin) };
        if let Some(equals) = statement.iter().position(|&b| b == b'=') {
            let commands = [
                b"SHOWMETHEMONEY_CREW" as &[u8],
                b"SHOWMETHEMONEY_DILITHIUM",
                b"SHOWMETHEMONEY_METAL",
                b"SHOWMETHEMONEY_TRITANIUM",
                b"SHOWMETHEMONEY_SUPPLIES",
            ];
            for (index, command) in commands.iter().enumerate() {
                if identifier(statement, equals) != *command {
                    continue;
                }
                let mut value = 0i32;
                // End the borrowed statement before entering the native CRT callbacks.
                if unsafe {
                    a2fo_rs_stage9_a1_config_literal(
                        source.add(begin),
                        end - begin,
                        equals,
                        &mut value,
                        number,
                        round,
                    )
                } {
                    unsafe { normal.add(index).write(value) };
                    applied += 1;
                }
                break;
            }
        }
        if end == length {
            break;
        }
        begin = end + 1;
    }
    applied
}

/// Safety: step synchronously reads each root (op 0) and performs native llround (op 1).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_config_defaults(
    context: *mut c_void,
    count: u32,
    step: Step,
) -> u32 {
    let mut configured = 0u32;
    if count <= 4096 {
        for index in 0..count {
            let mut unused = core::ptr::null();
            let mut amount = 0usize;
            if !unsafe { step(context, 0, index, &mut unused, &mut amount) } {
                return u32::MAX;
            }
            configured = configured.wrapping_add(amount as u32);
        }
    }
    for index in 0..5 {
        let mut unused = core::ptr::null();
        let mut amount = 0usize;
        if !unsafe { step(context, 1, index, &mut unused, &mut amount) } {
            return u32::MAX;
        }
    }
    configured
}

fn trim(mut bytes: &[u8], classify: Classify) -> &[u8] {
    while !bytes.is_empty() && unsafe { classify(bytes[0]) } != 0 {
        bytes = &bytes[1..];
    }
    while !bytes.is_empty() && unsafe { classify(bytes[bytes.len() - 1]) } != 0 {
        bytes = &bytes[..bytes.len() - 1];
    }
    bytes
}
fn equal_ci(bytes: &[u8], target: &[u8], lower: Classify) -> bool {
    bytes.len() == target.len()
        && bytes
            .iter()
            .zip(target)
            .all(|(&b, &t)| unsafe { lower(b) } == t)
}
fn boolean(value: &[u8], classify: Classify, lower: Classify) -> Option<bool> {
    let value = trim(value, classify);
    for word in [b"1" as &[u8], b"true", b"yes", b"on", b"enabled"] {
        if equal_ci(value, word, lower) {
            return Some(true);
        }
    }
    for word in [b"0" as &[u8], b"false", b"no", b"off", b"disabled"] {
        if equal_ci(value, word, lower) {
            return Some(false);
        }
    }
    None
}

/// Safety: contents is readable for length bytes; safe_mode is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_config_ini(
    contents: *const u8,
    length: usize,
    safe_mode: *mut bool,
    classify: Classify,
    lower: Classify,
) -> bool {
    if contents.is_null() || safe_mode.is_null() {
        return false;
    }
    let bytes = unsafe { core::slice::from_raw_parts(contents, length) };
    let mut section = false;
    let mut found = false;
    for mut line in bytes.split(|&b| b == b'\n') {
        if line.last() == Some(&b'\r') {
            line = &line[..line.len() - 1];
        }
        line = trim(line, classify);
        if line.is_empty() || line[0] == b';' || line[0] == b'#' {
            continue;
        }
        if line.len() >= 2 && line[0] == b'[' && line[line.len() - 1] == b']' {
            section = equal_ci(trim(&line[1..line.len() - 1], classify), b"a1compat", lower);
            continue;
        }
        if !section {
            continue;
        }
        if let Some(equals) = line.iter().position(|&b| b == b'=')
            && equal_ci(trim(&line[..equals], classify), b"safemode", lower)
            && let Some(value) = boolean(&line[equals + 1..], classify, lower)
        {
            unsafe { safe_mode.write(value) };
            found = true;
        }
    }
    found
}

/// Safety: step owns native paths and file reads, returns a stable borrowed contents span per call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_config_ini_roots(
    context: *mut c_void,
    count: u32,
    safe_mode: *mut bool,
    step: Step,
    classify: Classify,
    lower: Classify,
) -> bool {
    if count == 0 || count > 4096 {
        return false;
    }
    let mut any = false;
    for index in 0..count {
        let mut contents = core::ptr::null();
        let mut length = 0usize;
        if !unsafe { step(context, 2, index, &mut contents, &mut length) } {
            return false;
        }
        if !contents.is_null()
            && length != 0
            && unsafe { a2fo_rs_stage9_a1_config_ini(contents, length, safe_mode, classify, lower) }
        {
            any = true;
        }
    }
    any
}

/// Safety: source/key are stable spans;
/// line materializes a native NUL-terminated string before integer callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_config_gui_rect(
    source: *const u8,
    length: usize,
    key: *const u8,
    key_length: usize,
    context: *mut c_void,
    line: GuiLine,
    integer: GuiInteger,
    commit: GuiCommit,
) -> bool {
    if source.is_null() || key.is_null() || key_length == 0 {
        return false;
    }
    let mut found = false;
    let mut begin = 0usize;
    while begin < length {
        let mut end = begin;
        while end < length && unsafe { source.add(end).read() } != b'\n' {
            end += 1;
        }
        let mut statement = core::ptr::null();
        let mut statement_length = 0usize;
        if !unsafe {
            line(
                context,
                source.add(begin),
                end - begin,
                &mut statement,
                &mut statement_length,
            )
        } {
            return false;
        }
        let bytes = unsafe { core::slice::from_raw_parts(statement, statement_length) };
        if let Some(equals) = bytes.iter().position(|&b| b == b'=') {
            let id = identifier(bytes, equals);
            let target = unsafe { core::slice::from_raw_parts(key, key_length) };
            if id == target {
                let mut cursor = equals + 1;
                let mut parsed = [0i32; 4];
                let mut valid = true;
                for slot in &mut parsed {
                    while cursor < statement_length
                        && space(unsafe { statement.add(cursor).read() })
                    {
                        cursor += 1;
                    }
                    let mut consumed = 0usize;
                    if !unsafe { integer(context, cursor, &mut consumed, slot) } {
                        valid = false;
                        break;
                    }
                    cursor += consumed;
                }
                if valid {
                    unsafe { commit(context, parsed.as_ptr()) };
                    found = true;
                }
            }
        }
        if end == length {
            break;
        }
        begin = end + 1;
    }
    found
}

/// Safety: callback owns path, contents, and LegacyGameplayConfigAreas storage.
/// Operations are root preparation, candidate read, keyed rectangle parse, commit.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_config_gameplay(
    context: *mut c_void,
    roots: u32,
    step: GameplayStep,
) -> bool {
    if roots == 0 || roots > 4096 {
        return false;
    }
    for root in (0..roots).rev() {
        let state = unsafe { step(context, 0, root) };
        if state == u32::MAX {
            return false;
        }
        if state == 0 {
            continue;
        }
        for candidate in 0..3 {
            let state = unsafe { step(context, 1, candidate) };
            if state == u32::MAX {
                return false;
            }
            if state == 0 {
                continue;
            }
            let read =
                |field: u32, index: u32| unsafe { step(context, 2, field | index << 8) == 1 };
            if !(0..5).all(|field| read(field, 0)) {
                return false;
            }
            let cinematic_buttons = read(5, 0) && read(6, 0);
            let wireframe = read(7, 0);
            let race_icon = read(8, 0) && read(9, 0);
            let background = read(10, 0);
            let mut pieces = 0;
            if background {
                while pieces < 16 && read(11, pieces) {
                    pieces += 1;
                }
            }
            let name_background = read(12, 0);
            let crew_background = read(13, 0);
            let officer_background = read(14, 0);
            let class_text = read(15, 0) || read(16, 0);
            let name_text = read(17, 0) || read(18, 0);
            let crew_text = read(19, 0);
            let officer_text = read(20, 0);
            let identity_labels = read(21, 0) && read(22, 0);
            let construction = read(23, 0);
            let energy = read(24, 0);
            let mut multi = true;
            for index in 0..8 {
                if !read(25, index) {
                    multi = false;
                    break;
                }
            }
            multi = multi && read(26, 0) && read(27, 0) && read(28, 0) && read(29, 0);
            let flags = u32::from(cinematic_buttons)
                | u32::from(wireframe) << 1
                | u32::from(race_icon) << 2
                | u32::from(background) << 3
                | u32::from(name_background) << 4
                | u32::from(crew_background) << 5
                | u32::from(officer_background) << 6
                | u32::from(class_text && name_text && crew_text && officer_text) << 7
                | u32::from(identity_labels) << 8
                | u32::from(construction) << 9
                | u32::from(energy) << 10
                | u32::from(multi) << 11
                | pieces << 16;
            return unsafe { step(context, 3, flags) } == 1;
        }
    }
    false
}
