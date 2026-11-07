//! Core policy validation and transactions. Native synchronization, SDK layouts
//! and native string/container ownership remain opaque synchronous callbacks.
use core::ffi::c_void;
/// Safety: nonnull input has the original readable C-string contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_owner(value: *const u8) -> *const u8 {
    if value.is_null() || unsafe { value.read() } == 0 {
        c"unnamed extension".as_ptr().cast()
    } else {
        value
    }
}
unsafe fn registration_program(kind: u32, step: StringOp, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        if kind > 1 {
            return Err(());
        }
        if checked(step, ctx, 0, 0)? == 0 || (kind == 0 && checked(step, ctx, 1, 0)? == 0) {
            checked(step, ctx, 2, 0)?;
            return Ok(0);
        }
        checked(step, ctx, 3, 0)?;
        checked(step, ctx, 4, 0)?;
        let open = checked(step, ctx, 5, 0)? != 0 && checked(step, ctx, 6, 0)? != 0;
        if open {
            let state = checked(step, ctx, 7, 0)?;
            let vacant = if kind == 0 { state == 0 } else { state != 0 };
            if vacant {
                checked(step, ctx, 8, 0)?;
                checked(step, ctx, 9, 0)?;
                checked(step, ctx, 10, 0)?;
                return Ok(1);
            }
            checked(step, ctx, 13, 0)?;
        }
        checked(step, ctx, 9, 0)?;
        checked(step, ctx, if open { 12 } else { 11 }, 0)?;
        Ok(0)
    }
}
/// Safety: the native context owns the recursive lock and containers. Callback
/// exceptions remain native and are rethrown after this program returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_registration(
    kind: u32,
    step: StringOp,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { registration_program(kind, step, ctx) }).unwrap_or(2)
}
unsafe fn checked(step: StringOp, ctx: *mut c_void, id: u32, index: usize) -> Result<u64, ()> {
    let value = unsafe { step(ctx, id, index, 0) };
    if value == u64::MAX {
        Err(())
    } else {
        Ok(value)
    }
}
unsafe fn startup_program(phase: u32, step: StringOp, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        if phase == 0 {
            checked(step, ctx, 0, 0)?;
            for _ in 0..100 {
                if checked(step, ctx, 1, 0)? != 0 {
                    break;
                }
                checked(step, ctx, 2, 0)?;
                if checked(step, ctx, 1, 0)? == 0 {
                    checked(step, ctx, 3, 0)?;
                }
            }
            if checked(step, ctx, 4, 0)? == 0 || checked(step, ctx, 5, 0)? == 0 {
                checked(step, ctx, 6, 0)?;
                return Ok(1);
            }
            return Ok(0);
        }
        if phase != 1 {
            return Err(());
        }
        for index in 0..8 {
            if checked(step, ctx, 0, index)? != 0 {
                checked(step, ctx, 1, index)?;
            } else {
                checked(step, ctx, 2, index)?;
            }
        }
        if checked(step, ctx, 10, 0)? != 0 {
            checked(step, ctx, 11, 0)?;
        }
        checked(step, ctx, 12, 0)?;
        checked(step, ctx, 13, 0)?;
        let diagnostics = checked(step, ctx, 14, 0)? as usize;
        for index in 0..diagnostics {
            checked(step, ctx, 15, index)?;
        }
        if checked(step, ctx, 16, 0)? != 0 {
            checked(step, ctx, 17, 0)?;
        }
        checked(step, ctx, 18, 0)?;
        let roots = checked(step, ctx, 19, 0)? as usize;
        for index in 0..roots {
            checked(step, ctx, 20, index)?;
        }
        if checked(step, ctx, 21, 0)? == 0 && checked(step, ctx, 22, 0)? != 0 {
            checked(step, ctx, 23, 0)?;
        }
        if checked(step, ctx, 24, 0)? == 0 {
            checked(step, ctx, 25, 0)?;
        }
        checked(step, ctx, 26, 0)?;
        checked(step, ctx, 27, 0)?;
        let destroyed = checked(step, ctx, 28, 0)? != 0;
        if destroyed || checked(step, ctx, 29, 0)? != 0 {
            checked(step, ctx, 30, 0)?;
        }
        if destroyed && checked(step, ctx, 31, 0)? != 0 {
            checked(step, ctx, 32, 0)?;
        }
        for index in 0..4 {
            if checked(step, ctx, 33, index)? != 0 {
                checked(step, ctx, 34, index)?;
            }
        }
        checked(step, ctx, 35, 0)?;
        let features = checked(step, ctx, 36, 0)? as usize;
        for index in 0..features {
            checked(step, ctx, 37, index)?;
        }
        checked(step, ctx, 38, 0)?;
        checked(step, ctx, 39, 0)?;
        checked(step, ctx, 40, 0)?;
        Ok(0)
    }
}
/// Safety: native callbacks contain C++ exceptions and store them in their
/// matching native context. The caller rethrows in the original catch region.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_startup(
    phase: u32,
    step: StringOp,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { startup_program(phase, step, ctx) }).unwrap_or(2)
}
type Classify = unsafe extern "C" fn(u8, *mut c_void) -> u8;
type StringOp = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;
/// Safety: bytes must satisfy the original bounded C-string/slice contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_validate(
    kind: u32,
    value: *const u8,
    length: usize,
    alnum: Classify,
    context: *mut c_void,
) -> u8 {
    if value.is_null() {
        return 0;
    }
    let limit = match kind {
        0 => 63,
        1 => 127,
        2 => 255,
        3 => {
            if length == 0 || length > 255 {
                return 0;
            }
            length
        }
        _ => return 0,
    };
    if kind != 3 && unsafe { value.read() } == 0 {
        return 0;
    }
    let mut index = 0;
    loop {
        if kind == 3 && index == length {
            return 1;
        }
        let ch = unsafe { value.add(index).read() };
        if kind != 3 && ch == 0 {
            return 1;
        }
        index += 1;
        if index > limit {
            return 0;
        }
        let valid = if kind == 2 {
            ch >= 0x20 && ch != 0x7f
        } else {
            (unsafe { alnum(ch, context) }) != 0
                || ch == b'_'
                || ch == b'-'
                || ((kind == 1 || kind == 3) && ch == b'.')
        };
        if !valid {
            return 0;
        }
    }
}
unsafe fn op(step: StringOp, ctx: *mut c_void, kind: u32) -> u64 {
    unsafe { step(ctx, kind, 0, 0) }
}
unsafe fn bad_component(step: StringOp, ctx: *mut c_void) -> bool {
    let size = unsafe { op(step, ctx, 4) } as usize;
    if size == 0 {
        return true;
    }
    let first = unsafe { step(ctx, 5, 0, 0) } as u8;
    if first == b'.'
        && (size == 1 || (size == 2 && unsafe { step(ctx, 5, 1, 0) } == u64::from(b'.')))
    {
        return true;
    }
    let last = unsafe { step(ctx, 5, size - 1, 0) } as u8;
    last == b'.' || last == b' '
}
unsafe fn append_component(step: StringOp, ctx: *mut c_void) -> bool {
    unsafe {
        if op(step, ctx, 3) != 0 && op(step, ctx, 7) == u64::MAX {
            return false;
        }
        op(step, ctx, 8) != u64::MAX
    }
}
/// Safety: callback owns native strings and input, contains native exceptions,
/// and stores/rethrows them only after this synchronous program returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_relative(step: StringOp, ctx: *mut c_void) -> u8 {
    unsafe {
        op(step, ctx, 0); // output.clear must precede even the first input read
        if op(step, ctx, 1) == 0 {
            return 0;
        }
        let mut index = 0;
        loop {
            let ch = step(ctx, 2, index, 0) as u8;
            if ch == 0 {
                break;
            }
            if op(step, ctx, 3) + op(step, ctx, 4) >= 255 || ch < 0x20 || b":*?\"<>|".contains(&ch)
            {
                op(step, ctx, 0);
                return 0;
            }
            if ch == b'/' || ch == b'\\' {
                if bad_component(step, ctx) {
                    op(step, ctx, 0);
                    return 0;
                }
                if !append_component(step, ctx) {
                    return 0;
                }
                op(step, ctx, 9);
            } else if step(ctx, 10, usize::from(ch), 0) == u64::MAX {
                return 0;
            }
            index += 1;
        }
        if bad_component(step, ctx) {
            op(step, ctx, 0);
            return 0;
        }
        if !append_component(step, ctx) {
            return 0;
        }
        let size = op(step, ctx, 3);
        u8::from(size != 0 && size <= 255)
    }
}

unsafe fn defaults_program(
    kind: u32,
    count: u32,
    step: StringOp,
    ctx: *mut c_void,
) -> Result<u64, ()> {
    unsafe {
        if kind > 1 {
            return Err(());
        }
        if (kind == 0 && checked(step, ctx, 0, 0)? == 0)
            || checked(step, ctx, 1, 0)? == 0
            || count == 0
            || count > 64
        {
            checked(step, ctx, 2, 0)?;
            return Ok(0);
        }
        if kind == 0 {
            checked(step, ctx, 3, 0)?;
        }
        let copy = (|| -> Result<bool, ()> {
            for i in 0..count as usize {
                if checked(step, ctx, 4, i)? == 0 || checked(step, ctx, 5, i)? == 0 {
                    checked(step, ctx, 6, 0)?;
                    return Ok(false);
                }
                if checked(step, ctx, 7, i)? == 0 {
                    checked(step, ctx, 8, 0)?;
                    return Ok(false);
                }
                checked(step, ctx, 32, 0)?;
            }
            Ok(true)
        })();
        match copy {
            Ok(true) => {}
            Ok(false) => return Ok(0),
            Err(()) => {
                checked(step, ctx, 9, 0)?;
                return Ok(0);
            }
        }
        if kind == 0 {
            checked(step, ctx, 10, 0)?;
            checked(step, ctx, 11, 0)?;
            let open = checked(step, ctx, 12, 0)? != 0 && checked(step, ctx, 13, 0)? != 0;
            let registered = if open {
                if checked(step, ctx, 14, 0)? == 0 {
                    checked(step, ctx, 15, 0)?;
                    true
                } else {
                    checked(step, ctx, 16, 0)?;
                    false
                }
            } else {
                false
            };
            checked(step, ctx, 17, 0)?;
            checked(
                step,
                ctx,
                if registered {
                    18
                } else if open {
                    20
                } else {
                    19
                },
                count as usize,
            )?;
            return Ok(u64::from(registered));
        }
        let commit = (|| -> Result<(bool, bool), ()> {
            checked(step, ctx, 11, 0)?;
            let open = checked(step, ctx, 12, 0)? != 0 && checked(step, ctx, 13, 0)? != 0;
            let mut conflict = false;
            if open {
                checked(step, ctx, 21, 0)?;
                while checked(step, ctx, 22, 0)? != 0 {
                    if checked(step, ctx, 23, 0)? != 0 {
                        conflict = true;
                        break;
                    }
                    checked(step, ctx, 24, 0)?;
                }
                if !conflict {
                    checked(step, ctx, 25, 0)?;
                    checked(step, ctx, 26, 0)?;
                    checked(step, ctx, 21, 0)?;
                    while checked(step, ctx, 22, 0)? != 0 {
                        checked(step, ctx, 27, 0)?;
                        checked(step, ctx, 28, 0)?;
                        checked(step, ctx, 24, 0)?;
                    }
                    checked(step, ctx, 29, 0)?;
                }
            }
            checked(step, ctx, 30, 0)?;
            Ok((open, open && !conflict))
        })();
        match commit {
            Ok((open, registered)) => {
                checked(
                    step,
                    ctx,
                    if registered {
                        18
                    } else if open {
                        20
                    } else {
                        19
                    },
                    count as usize,
                )?;
                Ok(u64::from(registered))
            }
            Err(()) => {
                checked(step, ctx, 30, 0)?;
                checked(step, ctx, 31, 0)?;
                Ok(0)
            }
        }
    }
}
/// Safety: native context holds the original lock, containers, and captured
/// exception; no exception crosses Rust and caught native copy errors are
/// reported only after their original local objects have been destroyed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_defaults(
    kind: u32,
    count: u32,
    step: StringOp,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { defaults_program(kind, count, step, ctx) }).unwrap_or(2)
}

/// Safety: bytes is a readable/writable native string of len bytes; callback
/// preserves the native CRT locale. No storage is retained across this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_text(
    kind: u32,
    bytes: *mut u8,
    len: usize,
    slash: u8,
    lower: Classify,
    ctx: *mut c_void,
) {
    for i in 0..len {
        let ch = unsafe { bytes.add(i).read() };
        if kind == 0 {
            unsafe {
                bytes.add(i).write(lower(ch, ctx));
            }
        } else if kind == 1 && (ch == b'/' || ch == b'\\') {
            unsafe {
                bytes.add(i).write(slash);
            }
        }
    }
}
/// Safety: left is readable for len bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_join(
    left: *const u8,
    len: usize,
    right: usize,
) -> u32 {
    if len == 0 {
        0
    } else if right == 0 {
        1
    } else if matches!(unsafe { left.add(len - 1).read() }, b'/' | b'\\') {
        2
    } else {
        3
    }
}
fn trim_space(ch: u8) -> bool {
    matches!(ch, b' ' | b'\t' | b'\r' | b'\n')
}
/// Safety: bytes is readable for len bytes; first/end are valid distinct native
/// size_t outputs, and end is exclusive as used by native substr.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_trim(
    bytes: *const u8,
    len: usize,
    first: *mut usize,
    end: *mut usize,
) {
    let mut start = 0;
    while start < len && trim_space(unsafe { bytes.add(start).read() }) {
        start += 1;
    }
    let mut last = len;
    while last > start && trim_space(unsafe { bytes.add(last - 1).read() }) {
        last -= 1;
    }
    unsafe {
        first.write(start);
        end.write(last);
    }
}
/// Safety: bytes is readable for len bytes. Input has already passed the
/// original native CRT lowercasing step; no new locale rule is introduced.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_suffix(bytes: *const u8, len: usize, kind: u32) -> u8 {
    if len < 4 || (kind == 1 && len == 4) {
        return 0;
    }
    let suffix = if kind == 0 { b".sod" } else { b".odf" };
    for (i, expected) in suffix.iter().enumerate() {
        if unsafe { bytes.add(len - 4 + i).read() } != *expected {
            return 0;
        }
    }
    1
}
/// Safety: bytes is readable for len bytes, after slash normalization.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_basename(bytes: *const u8, len: usize) -> usize {
    for i in (0..len).rev() {
        if unsafe { bytes.add(i).read() } == b'\\' {
            return i + 1;
        }
    }
    0
}

unsafe fn transaction_program(
    kind: u32,
    initialized: bool,
    step: StringOp,
    ctx: *mut c_void,
) -> Result<u64, ()> {
    unsafe {
        if kind == 0 {
            let copy = (|| -> Result<(), ()> {
                checked(step, ctx, 0, 0)?;
                checked(step, ctx, 1, 0)?;
                for op in 10..34 {
                    checked(step, ctx, op, 0)?;
                }
                checked(step, ctx, 2, 0)?;
                checked(step, ctx, 3, 0)?;
                Ok(())
            })();
            if copy.is_err() {
                checked(step, ctx, 3, 0)?;
                checked(step, ctx, 4, 0)?;
            }
            return Ok(0);
        }
        if kind != 1 {
            return Err(());
        }
        checked(step, ctx, 0, 0)?;
        if checked(step, ctx, 1, 0)? == 0 || checked(step, ctx, 2, 0)? == 0 {
            checked(step, ctx, 4, 0)?;
            return Ok(0);
        }
        if !initialized {
            for op in 10..32 {
                checked(step, ctx, op, 0)?;
            }
        }
        checked(step, ctx, 3, 0)?;
        checked(step, ctx, 4, 0)?;
        if !initialized {
            checked(step, ctx, 5, 0)?;
        }
        Ok(0)
    }
}
/// Safety: synchronous native context owns snapshots and the recursive lock;
/// exception cleanup destroys snapshots before unlocking as in native unwinding.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_transaction(
    kind: u32,
    initialized: u8,
    step: StringOp,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { transaction_program(kind, initialized != 0, step, ctx) }).unwrap_or(2)
}

unsafe fn overlay_program(precedence: u32, step: StringOp, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        if checked(step, ctx, 0, 0)? == 0 || precedence > 1 {
            checked(step, ctx, 1, 0)?;
            return Ok(0);
        }
        checked(step, ctx, 2, 0)?;
        let open = checked(step, ctx, 3, 0)? != 0 && checked(step, ctx, 4, 0)? != 0;
        let mut registered = false;
        if open {
            checked(step, ctx, 5, 0)?;
            let mut found = false;
            while checked(step, ctx, 6, 0)? != 0 {
                if checked(step, ctx, 7, 0)? != 0 {
                    found = true;
                    break;
                }
                checked(step, ctx, 8, 0)?;
            }
            if !found && checked(step, ctx, 9, 0)? < 64 {
                checked(step, ctx, 10, 0)?;
                registered = true;
            } else if found {
                checked(step, ctx, 11, 0)?;
            }
        }
        checked(step, ctx, 12, 0)?;
        let log = if registered {
            13
        } else if !open {
            14
        } else if checked(step, ctx, 15, 0)? != 0 {
            16
        } else {
            17
        };
        checked(step, ctx, log, 0)?;
        Ok(u64::from(registered))
    }
}
/// Safety: native context retains native strings, vector and lock ownership.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_overlay(
    precedence: u32,
    step: StringOp,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { overlay_program(precedence, step, ctx) }).unwrap_or(2)
}

unsafe fn field_handler_program(count: u32, step: StringOp, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        if checked(step, ctx, 0, 0)? == 0
            || count > 64
            || (count != 0 && checked(step, ctx, 1, 0)? == 0)
        {
            checked(step, ctx, 2, 0)?;
            return Ok(0);
        }
        checked(step, ctx, 3, 0)?;
        let copy = (|| -> Result<bool, ()> {
            checked(step, ctx, 4, 0)?;
            for i in 0..count as usize {
                if checked(step, ctx, 5, i)? == 0 {
                    checked(step, ctx, 6, 0)?;
                    return Ok(false);
                }
                checked(step, ctx, 7, 0)?;
            }
            checked(step, ctx, 8, 0)?;
            Ok(true)
        })();
        match copy {
            Ok(true) => {}
            Ok(false) => return Ok(0),
            Err(()) => {
                checked(step, ctx, 9, 0)?;
                return Ok(0);
            }
        }
        checked(step, ctx, 10, 0)?;
        let admitted = checked(step, ctx, 11, 0)? != 0
            && checked(step, ctx, 12, 0)? != 0
            && checked(step, ctx, 13, 0)? < 64;
        if admitted {
            let len = checked(step, ctx, 14, 0)? as usize;
            for i in 0..len {
                checked(step, ctx, 15, i)?;
            }
            checked(step, ctx, 16, 0)?;
        }
        checked(step, ctx, 17, 0)?;
        checked(step, ctx, 18, usize::from(admitted))?;
        Ok(u64::from(admitted))
    }
}
/// Safety: native context owns SDK-specific registrations, sorted fields and
/// recursive lock; copy errors are caught in the original native region.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_field_handler(
    count: u32,
    step: StringOp,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { field_handler_program(count, step, ctx) }).unwrap_or(2)
}
