const I32_MAX: i64 = i32::MAX as i64;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FinishIn {
    completed: u32,
    upgrade_gain: i32,
    credited_gain: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FinishOut {
    admitted: u8,
    completed: u32,
    upgrade_gain: i32,
    credited_gain: i32,
    current_delta: i32,
    clear_previous: u8,
}

// Native owns the read/write. Match the original i64 intermediate and clamp.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_adjust(current: i32, delta: i32) -> i32 {
    ((current as i64) + (delta as i64)).clamp(0, I32_MAX) as i32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_maximum(value: i32) -> i32 {
    value.clamp(0, 256)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_gain(value: i32) -> i32 {
    value.clamp(0, 1_000_000)
}

// Existing stored upgrade gain is nonnegative. There is deliberately no
// lower clamp: the original C++ only caps the i64 sum at INT32_MAX.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_desired(base: i32, upgrade: i32) -> i32 {
    ((base as i64) + (upgrade as i64)).min(I32_MAX) as i32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_admit(completed: u32, queued: u32, maximum: i32) -> u8 {
    u8::from(completed.wrapping_add(queued) < maximum as u32)
}

/// # Safety
/// If `value` is positive and `output` is nonnull, `output` must point to a
/// writable `i32` for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_active_decrement(
    value: i32,
    output: *mut i32,
) -> u8 {
    if value <= 0 {
        return 0;
    }
    let Some(output) = (unsafe { output.as_mut() }) else {
        return 0;
    };
    *output = value - 1;
    1
}

// The caller supplies scalar state and team identity while retaining the
// original lock. Do not retain native pointers or mutate native storage here.
/// # Safety
/// If nonnull, `input` must point to a readable `FinishIn` and `output` to a
/// separate writable `FinishOut`. Both must remain valid throughout the call;
/// the native caller retains its state lock.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_finish(
    input: *const FinishIn,
    base: i32,
    maximum: i32,
    gain: i32,
    same_team: u8,
    current_team_present: u8,
    output: *mut FinishOut,
) {
    let (Some(input), Some(output)) = ((unsafe { input.as_ref() }), (unsafe { output.as_mut() }))
    else {
        return;
    };
    if input.completed >= maximum as u32 {
        return;
    }
    // Existing caller clamps gain to 0..1,000,000 and stored gains are >= 0.
    let completed = input.completed.wrapping_add(1);
    let upgrade_total = (input.upgrade_gain as i64) + (gain as i64);
    let upgrade_gain = upgrade_total.min(I32_MAX) as i32;
    let desired = a2fo_rs_stage9_a1_officer_desired(base, upgrade_gain);
    let same = same_team != 0;
    let current_delta = if same {
        desired.wrapping_sub(input.credited_gain)
    } else {
        desired
    };
    *output = FinishOut {
        admitted: 1,
        completed,
        upgrade_gain,
        credited_gain: if current_team_present != 0 {
            desired
        } else {
            0
        },
        current_delta,
        clear_previous: u8::from(same),
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    const SENTINEL: FinishOut = FinishOut {
        admitted: 7,
        completed: 8,
        upgrade_gain: 9,
        credited_gain: 10,
        current_delta: 11,
        clear_previous: 12,
    };

    #[test]
    fn pod_layout_matches_c_header() {
        assert_eq!(core::mem::size_of::<FinishIn>(), 12);
        assert_eq!(core::mem::offset_of!(FinishIn, completed), 0);
        assert_eq!(core::mem::offset_of!(FinishIn, credited_gain), 8);
        assert_eq!(core::mem::size_of::<FinishOut>(), 24);
        assert_eq!(core::mem::offset_of!(FinishOut, completed), 4);
        assert_eq!(core::mem::offset_of!(FinishOut, current_delta), 16);
        assert_eq!(core::mem::offset_of!(FinishOut, clear_previous), 20);
    }

    #[test]
    fn adjustment_and_clamps_match_boundaries() {
        assert_eq!(a2fo_rs_stage9_a1_officer_adjust(5, -10), 0);
        assert_eq!(a2fo_rs_stage9_a1_officer_adjust(i32::MAX, 1), i32::MAX);
        assert_eq!(a2fo_rs_stage9_a1_officer_adjust(4, 3), 7);
        assert_eq!(a2fo_rs_stage9_a1_officer_maximum(-1), 0);
        assert_eq!(a2fo_rs_stage9_a1_officer_maximum(257), 256);
        assert_eq!(a2fo_rs_stage9_a1_officer_gain(-1), 0);
        assert_eq!(a2fo_rs_stage9_a1_officer_gain(1_000_001), 1_000_000);
        assert_eq!(a2fo_rs_stage9_a1_officer_desired(i32::MAX, 1), i32::MAX);
        assert_eq!(a2fo_rs_stage9_a1_officer_desired(-5, 3), -2);
    }

    #[test]
    fn admission_wraps_unsigned_sum_and_active_decrement_guards_output() {
        assert_eq!(a2fo_rs_stage9_a1_officer_admit(2, 3, 6), 1);
        assert_eq!(a2fo_rs_stage9_a1_officer_admit(2, 3, 5), 0);
        assert_eq!(a2fo_rs_stage9_a1_officer_admit(u32::MAX, 1, 1), 1);
        let mut output = 77;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_officer_active_decrement(0, &mut output) },
            0
        );
        assert_eq!(output, 77);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_officer_active_decrement(1, &mut output) },
            1
        );
        assert_eq!(output, 0);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_officer_active_decrement(1, core::ptr::null_mut()) },
            0
        );
    }

    #[test]
    fn finish_blocks_without_touching_output() {
        let input = FinishIn {
            completed: 6,
            upgrade_gain: 15,
            credited_gain: 20,
        };
        let mut output = SENTINEL;
        unsafe { a2fo_rs_stage9_a1_officer_finish(&input, 20, 6, 15, 1, 1, &mut output) };
        assert_eq!(output, SENTINEL);
        unsafe {
            a2fo_rs_stage9_a1_officer_finish(core::ptr::null(), 20, 7, 15, 1, 1, &mut output)
        };
        assert_eq!(output, SENTINEL);
    }

    #[test]
    fn finish_saturates_and_respects_team_identity_and_presence() {
        let input = FinishIn {
            completed: 1,
            upgrade_gain: i32::MAX - 5,
            credited_gain: 20,
        };
        let mut output = SENTINEL;
        unsafe { a2fo_rs_stage9_a1_officer_finish(&input, 20, 6, 10, 1, 1, &mut output) };
        assert_eq!(
            output,
            FinishOut {
                admitted: 1,
                completed: 2,
                upgrade_gain: i32::MAX,
                credited_gain: i32::MAX,
                current_delta: i32::MAX - 20,
                clear_previous: 1
            }
        );
        let ordinary = FinishIn {
            completed: 2,
            upgrade_gain: 10,
            credited_gain: 25,
        };
        unsafe { a2fo_rs_stage9_a1_officer_finish(&ordinary, 20, 6, 5, 0, 1, &mut output) };
        assert_eq!(
            output,
            FinishOut {
                admitted: 1,
                completed: 3,
                upgrade_gain: 15,
                credited_gain: 35,
                current_delta: 35,
                clear_previous: 0
            }
        );
        unsafe { a2fo_rs_stage9_a1_officer_finish(&ordinary, 20, 6, 5, 1, 0, &mut output) };
        assert_eq!(
            output,
            FinishOut {
                admitted: 1,
                completed: 3,
                upgrade_gain: 15,
                credited_gain: 0,
                current_delta: 10,
                clear_previous: 1
            }
        );
    }
}
