//! Nebula module text control. Native CRT and string operations stay in C++.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_nebula_join(left: usize, right: usize, last: u8) -> u8 {
    if left == 0 {
        0
    } else if right == 0 {
        1
    } else if last == b'\\' || last == b'/' {
        2
    } else {
        3
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_trim(
    bytes: *mut u8,
    len: usize,
    step: Step,
    context: *mut c_void,
    first: *mut usize,
    span: *mut usize,
) {
    if bytes.is_null() || first.is_null() || span.is_null() {
        return;
    }
    let mut begin = 0;
    while begin < len && unsafe { step(context, 0, begin) } != 0 {
        begin += 1;
    }
    let mut end = len;
    while end > begin && unsafe { step(context, 0, end - 1) } != 0 {
        end -= 1;
    }
    unsafe {
        *first = begin;
        *span = end - begin;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_slashes(bytes: *mut u8, len: usize) {
    if bytes.is_null() {
        return;
    }
    for i in 0..len {
        let byte = unsafe { bytes.add(i) };
        if unsafe { *byte } == b'/' {
            unsafe {
                *byte = b'\\';
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_nebula_query_admit(
    db: *const c_void,
    command: *const u8,
    output: *const c_void,
    armada: *const c_void,
) -> u8 {
    u8::from(!db.is_null() && !command.is_null() && !output.is_null() && !armada.is_null())
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_nebula_query_found(found: usize, first: u8) -> u8 {
    u8::from((found & 0xff) != 0 && first != 0)
}

/// Ops: 0 readable byte, 1 read byte, 2 append same byte, 3 successful finish.
/// The finish callback does not read the source byte again.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_copy(
    step: Step,
    context: *mut c_void,
    cap: usize,
    failed: *const u8,
) -> u8 {
    if failed.is_null() {
        return 0;
    }
    for i in 0..cap {
        if unsafe { step(context, 0, i) } == 0 || unsafe { *failed } != 0 {
            return 0;
        }
        if unsafe { step(context, 1, i) } == 0 {
            let result = unsafe { step(context, 3, i) };
            return u8::from(result != 0 && unsafe { *failed } == 0);
        }
        unsafe {
            step(context, 2, i);
        }
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_lower(
    step: Step,
    context: *mut c_void,
    len: usize,
    failed: *const u8,
) {
    if failed.is_null() {
        return;
    }
    for i in 0..len {
        unsafe {
            step(context, 4, i);
        }
        if unsafe { *failed } != 0 {
            return;
        }
    }
}

/// Ops: 0 native isspace at offset, 1 native strtof at offset (returns end
/// offset), 2 native isfinite on parsed value, 3 write parsed value at index.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_floats(
    bytes: *const u8,
    values: *mut f32,
    count: usize,
    step: Step,
    context: *mut c_void,
) -> u8 {
    if bytes.is_null() || values.is_null() || count == 0 {
        return 0;
    }
    let mut cursor = 0;
    for i in 0..count {
        loop {
            let byte = unsafe { *bytes.add(cursor) };
            if byte == 0 {
                break;
            }
            if unsafe { step(context, 0, cursor) } == 0 && unsafe { *bytes.add(cursor) } != b',' {
                break;
            }
            cursor += 1;
        }
        if unsafe { *bytes.add(cursor) } == 0 {
            return 0;
        }
        let end = unsafe { step(context, 1, cursor) };
        if end == cursor || unsafe { step(context, 2, 0) } == 0 {
            return 0;
        }
        unsafe {
            step(context, 3, i);
        }
        cursor = end;
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_vector(step: Step, context: *mut c_void) {
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe {
            step(context, 1, 0);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn join_and_query_boundaries() {
        assert_eq!(a2fo_rs_stage10_nebula_join(0, 2, 0), 0);
        assert_eq!(a2fo_rs_stage10_nebula_join(2, 0, 0), 1);
        assert_eq!(a2fo_rs_stage10_nebula_join(2, 2, b'/'), 2);
        assert_eq!(a2fo_rs_stage10_nebula_join(2, 2, b'x'), 3);
        assert_eq!(a2fo_rs_stage10_nebula_query_found(0x100, 1), 0);
    }
    unsafe extern "C" fn space(ctx: *mut c_void, _: u32, i: usize) -> usize {
        usize::from(unsafe { *((ctx as *const u8).add(i)) } == b' ')
    }
    #[test]
    fn trim_then_slashes() {
        let mut bytes = *b" /ab/ ";
        let mut first = 99;
        let mut span = 99;
        unsafe {
            a2fo_rs_stage10_nebula_trim(
                bytes.as_mut_ptr(),
                bytes.len(),
                space,
                bytes.as_mut_ptr().cast(),
                &mut first,
                &mut span,
            );
        }
        assert_eq!((first, span), (1, 4));
        unsafe {
            a2fo_rs_stage10_nebula_slashes(bytes.as_mut_ptr().add(first), span);
        }
        assert_eq!(&bytes[first..first + span], b"\\ab\\");
    }
    struct CopyContext {
        bytes: [u8; 4],
        seen: [u8; 4],
        count: usize,
        finishes: usize,
    }
    unsafe extern "C" fn copy_step(ctx: *mut c_void, op: u32, i: usize) -> usize {
        let c = unsafe { &mut *(ctx as *mut CopyContext) };
        match op {
            0 => 1,
            1 => c.bytes[i] as usize,
            2 => {
                c.seen[c.count] = c.bytes[i];
                c.count += 1;
                1
            }
            3 => {
                c.finishes += 1;
                1
            }
            _ => 0,
        }
    }
    #[test]
    fn copy_stops_at_cap_and_finishes_at_nul() {
        let mut c = CopyContext {
            bytes: *b"abc\0",
            seen: [0; 4],
            count: 0,
            finishes: 0,
        };
        let failed = 0;
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_nebula_copy(
                    copy_step,
                    (&mut c as *mut CopyContext).cast(),
                    3,
                    &failed,
                )
            },
            0
        );
        assert_eq!(c.count, 3);
        assert_eq!(c.finishes, 0);
        c.count = 0;
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_nebula_copy(
                    copy_step,
                    (&mut c as *mut CopyContext).cast(),
                    4,
                    &failed,
                )
            },
            1
        );
        assert_eq!(c.count, 3);
        assert_eq!(c.finishes, 1);
    }
    struct FloatContext {
        values: [f32; 3],
        parsed: f32,
    }
    unsafe extern "C" fn float_step(ctx: *mut c_void, op: u32, i: usize) -> usize {
        let c = unsafe { &mut *(ctx as *mut FloatContext) };
        match op {
            0 => usize::from(i == 2),
            1 => {
                c.parsed = 5.0;
                i + 1
            }
            2 => 1,
            3 => {
                c.values[i] = c.parsed;
                1
            }
            _ => 0,
        }
    }
    #[test]
    fn floats_reject_empty_after_valid_prefix_without_rollback() {
        let mut c = FloatContext {
            values: [9.0; 3],
            parsed: 0.0,
        };
        let input = b"5, \0";
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_nebula_floats(
                    input.as_ptr(),
                    c.values.as_mut_ptr(),
                    2,
                    float_step,
                    (&mut c as *mut FloatContext).cast(),
                )
            },
            0
        );
        assert_eq!(c.values, [5.0, 9.0, 9.0]);
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_nebula_floats(
                    input.as_ptr(),
                    c.values.as_mut_ptr(),
                    0,
                    float_step,
                    (&mut c as *mut FloatContext).cast(),
                )
            },
            0
        );
    }
    struct DelimiterContext {
        bytes: [u8; 4],
        values: [f32; 2],
        parsed: f32,
        comma_space_calls: usize,
        second_parse_offset: usize,
    }
    unsafe extern "C" fn changing_space(ctx: *mut c_void, op: u32, i: usize) -> usize {
        let c = unsafe { &mut *(ctx as *mut DelimiterContext) };
        match op {
            0 => {
                if i == 1 {
                    c.comma_space_calls += 1;
                    c.bytes[i] = b'x';
                }
                0
            }
            1 => {
                if i != 0 {
                    c.second_parse_offset = i;
                }
                c.parsed = if i == 0 { 5.0 } else { 7.0 };
                i + 1
            }
            2 => 1,
            3 => {
                c.values[i] = c.parsed;
                1
            }
            _ => 0,
        }
    }
    #[test]
    fn comma_invokes_space_and_uses_mutated_byte_for_comma_check() {
        let mut c = DelimiterContext {
            bytes: *b"5,7\0",
            values: [0.0; 2],
            parsed: 0.0,
            comma_space_calls: 0,
            second_parse_offset: usize::MAX,
        };
        let bytes = c.bytes.as_ptr();
        let values = c.values.as_mut_ptr();
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_nebula_floats(
                    bytes,
                    values,
                    2,
                    changing_space,
                    (&mut c as *mut DelimiterContext).cast(),
                )
            },
            1
        );
        assert_eq!(c.comma_space_calls, 1);
        assert_eq!(c.second_parse_offset, 1);
        assert_eq!(c.values, [5.0, 7.0]);
    }
}
