use core::ffi::{c_char, c_void};

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct Next {
    found: u8,
    nonempty: u8,
    neutral: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct CountPlan {
    append: u8,
    occupied: u8,
}

pub type Probe = unsafe extern "C" fn(*mut c_void, u32, u32, *mut Next) -> u8;
pub type Field = unsafe extern "C" fn(*mut c_void, u32) -> u8;
pub type Compare = unsafe extern "C" fn(*mut c_void) -> u8;
pub type Resource = unsafe extern "C" fn(*mut c_void, u32, u32, *mut i32) -> u8;

/// # Safety
/// `output` must be writable and separate from callback-owned storage.
/// `context` and `probe` must remain valid for synchronous calls; `probe`
/// must fill each supplied private `Next` as specified and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_count_plan(
    declared: i32,
    context: *mut c_void,
    probe: Probe,
    output: *mut CountPlan,
) {
    let Some(output) = (unsafe { output.as_mut() }) else {
        return;
    };
    let mut plan = CountPlan::default();
    if !(1..4095).contains(&declared) {
        *output = plan;
        return;
    }
    for index in 0..declared as u32 {
        let mut next = Next::default();
        match unsafe { probe(context, 0, index, &mut next) } {
            1 => {
                *output = plan;
                return;
            } // Existing neutral row.
            2 => {
                *output = plan;
                return;
            } // Native callback terminated.
            _ => {}
        }
    }
    let mut next = Next::default();
    if unsafe { probe(context, 1, declared as u32, &mut next) } == 2 {
        *output = plan;
        return;
    }
    if next.found != 0 && next.nonempty != 0 {
        let mut neutral = Next::default();
        if unsafe { probe(context, 2, declared as u32, &mut neutral) } == 0 {
            plan.occupied = 1;
            *output = plan;
            return;
        }
    }
    plan.append = 1;
    *output = plan;
}

/// # Safety
/// `context` and `probe` must remain valid for the synchronous native probe;
/// `probe` must accept a private writable `Next` and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_synthesize(
    context: *mut c_void,
    probe: Probe,
) -> u8 {
    let mut next = Next::default();
    u8::from(unsafe { probe(context, 2, 0, &mut next) } == 0)
}

/// # Safety
/// If the scalar and null guards admit comparison, `context` and `compare`
/// must remain valid for one synchronous call without unwinding. `key` and
/// `output` are checked only for null and are not dereferenced here.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_entry_admit(
    synthetic_index: i32,
    key: *const c_char,
    output: *const c_char,
    found: usize,
    output_size: usize,
    required: usize,
    context: *mut c_void,
    compare: Compare,
) -> u8 {
    if synthetic_index < 0 || key.is_null() || output.is_null() {
        return 0;
    }
    if unsafe { compare(context) } == 0 || (found & 0xff) != 0 {
        return 0;
    }
    u8::from(output_size >= required)
}

/// # Safety
/// `context` and `field` must remain valid for synchronous indexed calls;
/// `field` must obey its native operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_field(
    count: u32,
    context: *mut c_void,
    field: Field,
) -> u8 {
    for index in 0..count {
        match unsafe { field(context, index) } {
            1 => return 1,
            2 => return 2,
            _ => {}
        }
    }
    0
}

/// # Safety
/// If the scalar guards admit parsing, `text` must address
/// `size_with_nul` readable bytes containing a NUL terminator.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_nonnegative(
    text: *const u8,
    size_with_nul: usize,
    consumed: usize,
    parsed: i64,
    range_error: u8,
) -> u8 {
    if text.is_null()
        || size_with_nul <= 1
        || range_error != 0
        || parsed < 0
        || parsed > i32::MAX as i64
    {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(text, size_with_nul) };
    let Some(c_end) = bytes.iter().position(|byte| *byte == 0) else {
        return 0;
    };
    // Match the frozen C++ ordering: tail whitespace is skipped before the
    // no-conversion check compares end with the original C-string start.
    // Thus a nonempty whitespace-only C string reaches its NUL and converts
    // to zero, while the empty C string remains rejected.
    if c_end == 0 || consumed > c_end {
        return 0;
    }
    let mut cursor = consumed;
    while cursor < c_end && matches!(bytes[cursor], b' ' | b'\t' | b'\r' | b'\n') {
        cursor += 1;
    }
    u8::from(cursor == c_end)
}

/// # Safety
/// `context` and `resource` must remain valid for all synchronous calls.
/// The callback must use each supplied private writable `i32` only during
/// that call, obey its native operation contract, and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_resources(
    context: *mut c_void,
    resource: Resource,
) -> u8 {
    let mut applied = true;
    for command in 0..3 {
        let mut normal = 0;
        let mut lots = 0;
        unsafe { resource(context, 0, command, &mut normal) };
        unsafe { resource(context, 1, command, &mut lots) };
        let normal_written = unsafe { resource(context, 2, command, &mut normal) } != 0;
        applied = normal_written && applied;
        let lots_written = unsafe { resource(context, 3, command, &mut lots) } != 0;
        applied = lots_written && applied;
    }
    u8::from(applied)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_race_log_case(incident: i32) -> u8 {
    if incident <= 16 {
        1
    } else if incident == 17 {
        2
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate std;
    use self::std::vec::Vec;

    struct CountTrace {
        visits: Vec<(u32, u32)>,
        neutral: i32,
        next: Next,
    }
    unsafe extern "C" fn probe(ctx: *mut c_void, op: u32, index: u32, out: *mut Next) -> u8 {
        let t = unsafe { &mut *ctx.cast::<CountTrace>() };
        t.visits.push((op, index));
        if op == 0 {
            u8::from(index as i32 == t.neutral)
        } else if op == 1 {
            unsafe { *out = t.next };
            0
        } else {
            t.next.neutral
        }
    }
    #[test]
    fn count_scans_in_order_and_respects_occupied_next_row() {
        let mut t = CountTrace {
            visits: Vec::new(),
            neutral: -1,
            next: Next {
                found: 1,
                nonempty: 1,
                neutral: 0,
            },
        };
        let mut plan = CountPlan::default();
        unsafe {
            a2fo_rs_stage9_a1_race_count_plan(
                3,
                (&mut t as *mut CountTrace).cast(),
                probe,
                &mut plan,
            )
        };
        assert_eq!(t.visits, [(0, 0), (0, 1), (0, 2), (1, 3), (2, 3)]);
        assert_eq!(
            plan,
            CountPlan {
                append: 0,
                occupied: 1
            }
        );
        t.visits.clear();
        t.neutral = 1;
        unsafe {
            a2fo_rs_stage9_a1_race_count_plan(
                3,
                (&mut t as *mut CountTrace).cast(),
                probe,
                &mut plan,
            )
        };
        assert_eq!(t.visits, [(0, 0), (0, 1)]);
        assert_eq!(plan, CountPlan::default());
        t.visits.clear();
        unsafe {
            a2fo_rs_stage9_a1_race_count_plan(
                4095,
                (&mut t as *mut CountTrace).cast(),
                probe,
                &mut plan,
            )
        };
        assert!(t.visits.is_empty());
        t.next = Next {
            found: 1,
            nonempty: 1,
            neutral: 1,
        };
        t.neutral = -1;
        unsafe {
            a2fo_rs_stage9_a1_race_count_plan(
                1,
                (&mut t as *mut CountTrace).cast(),
                probe,
                &mut plan,
            )
        };
        assert_eq!(
            plan,
            CountPlan {
                append: 1,
                occupied: 0
            }
        );
        assert_eq!(t.visits, [(0, 0), (1, 1), (2, 1)]);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_race_synthesize((&mut t as *mut CountTrace).cast(), probe) },
            0
        );
        assert_eq!(t.visits[3], (2, 0));
    }
    #[test]
    fn parse_preserves_c_string_nul_and_ascii_tail_rules() {
        unsafe fn check(data: &[u8], consumed: usize, value: i64) -> u8 {
            unsafe {
                a2fo_rs_stage9_a1_race_nonnegative(data.as_ptr(), data.len(), consumed, value, 0)
            }
        }
        assert_eq!(unsafe { check(b"42 \t\r\n\0", 2, 42) }, 1);
        assert_eq!(unsafe { check(b" \t\r\n\0", 0, 0) }, 1);
        assert_eq!(unsafe { check(b"42\0junk\0", 2, 42) }, 1);
        assert_eq!(unsafe { check(b"42x\0", 2, 42) }, 0);
        assert_eq!(unsafe { check(b"-1\0", 2, -1) }, 0);
        assert_eq!(unsafe { check(b"2147483647\0", 10, 2147483647) }, 1);
        assert_eq!(unsafe { check(b"2147483648\0", 10, 2147483648) }, 0);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_race_nonnegative(c"7".as_ptr().cast::<u8>(), 2, 1, 7, 1,) },
            0
        );
        assert_eq!(unsafe { check(b"\0", 0, 0) }, 0);
        assert_eq!(a2fo_rs_stage9_a1_race_log_case(16), 1);
        assert_eq!(a2fo_rs_stage9_a1_race_log_case(17), 2);
    }
    #[test]
    fn parse_rejects_vtab_formfeed_and_nonwhitespace_tail_after_no_conversion() {
        unsafe fn check(data: &[u8], consumed: usize, value: i64) -> u8 {
            unsafe {
                a2fo_rs_stage9_a1_race_nonnegative(data.as_ptr(), data.len(), consumed, value, 0)
            }
        }
        assert_eq!(unsafe { check(b"\x0b\0", 0, 0) }, 0);
        assert_eq!(unsafe { check(b"\x0c\0", 0, 0) }, 0);
        assert_eq!(unsafe { check(b" \x0b\0", 0, 0) }, 0);
        assert_eq!(unsafe { check(b" \x0c\0", 0, 0) }, 0);
        assert_eq!(unsafe { check(b"\x0b5\0", 2, 5) }, 1);
        assert_eq!(unsafe { check(b"\x0c5\0", 2, 5) }, 1);
        assert_eq!(unsafe { check(b"5\x0b\0", 1, 5) }, 0);
        assert_eq!(unsafe { check(b"5\x0c\0", 1, 5) }, 0);
        assert_eq!(unsafe { check(b"x\0", 0, 0) }, 0);
        assert_eq!(unsafe { check(b"", 0, 0) }, 0);
    }
    struct ResourceTrace {
        calls: Vec<(u32, u32)>,
        writes: u32,
    }
    unsafe extern "C" fn resource(ctx: *mut c_void, op: u32, index: u32, out: *mut i32) -> u8 {
        let t = unsafe { &mut *ctx.cast::<ResourceTrace>() };
        t.calls.push((op, index));
        if op < 2 {
            unsafe { *out = index as i32 + op as i32 };
            1
        } else {
            t.writes += 1;
            u8::from(index != 0 || op != 2)
        }
    }
    #[test]
    fn resource_reads_precede_writes_and_all_writes_run_after_failure() {
        let mut t = ResourceTrace {
            calls: Vec::new(),
            writes: 0,
        };
        let result = unsafe {
            a2fo_rs_stage9_a1_race_resources((&mut t as *mut ResourceTrace).cast(), resource)
        };
        assert_eq!(result, 0);
        assert_eq!(t.writes, 6);
        assert_eq!(
            t.calls,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (0, 1),
                (1, 1),
                (2, 1),
                (3, 1),
                (0, 2),
                (1, 2),
                (2, 2),
                (3, 2)
            ]
        );
    }
    struct FieldTrace {
        visits: Vec<u32>,
        match_at: u32,
        comparisons: u32,
    }
    unsafe extern "C" fn field(ctx: *mut c_void, index: u32) -> u8 {
        let t = unsafe { &mut *ctx.cast::<FieldTrace>() };
        t.visits.push(index);
        u8::from(index == t.match_at)
    }
    unsafe extern "C" fn compare(ctx: *mut c_void) -> u8 {
        let t = unsafe { &mut *ctx.cast::<FieldTrace>() };
        t.comparisons += 1;
        1
    }
    #[test]
    fn field_and_entry_admission_stop_at_original_boundaries() {
        let mut t = FieldTrace {
            visits: Vec::new(),
            match_at: 2,
            comparisons: 0,
        };
        let ctx = (&mut t as *mut FieldTrace).cast();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_race_field(5, ctx, field) }, 1);
        assert_eq!(t.visits, [0, 1, 2]);
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_race_entry_admit(
                    -1,
                    c"race2".as_ptr(),
                    c"out".as_ptr(),
                    0,
                    16,
                    11,
                    ctx,
                    compare,
                )
            },
            0
        );
        assert_eq!(t.comparisons, 0);
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_race_entry_admit(
                    2,
                    c"race2".as_ptr(),
                    c"out".as_ptr(),
                    1,
                    16,
                    11,
                    ctx,
                    compare,
                )
            },
            0
        );
        assert_eq!(t.comparisons, 1);
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_race_entry_admit(
                    2,
                    c"race2".as_ptr(),
                    c"out".as_ptr(),
                    0,
                    10,
                    11,
                    ctx,
                    compare,
                )
            },
            0
        );
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_race_entry_admit(
                    2,
                    c"race2".as_ptr(),
                    c"out".as_ptr(),
                    0,
                    11,
                    11,
                    ctx,
                    compare,
                )
            },
            1
        );
    }
}
