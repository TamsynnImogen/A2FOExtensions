use core::ffi::{c_char, c_void};

pub type TooltipRead = unsafe extern "C" fn(*mut c_void, *const c_char) -> u8;
pub type Step = unsafe extern "C" fn(*mut c_void, u32, u32) -> u8;
pub type ReadReady = unsafe extern "C" fn(*mut c_void) -> u8;

const TOOLTIP_KEYS: [&[u8]; 7] = [
    b"frameLocation\0",
    b"tooltipFrameBottomRect_0\0",
    b"tooltipFrameBottomRect_1\0",
    b"tooltipFrameTopRect_0\0",
    b"tooltipFrameTopRect_1\0",
    b"tooltipFrameLeftRect\0",
    b"tooltipFrameRightRect\0",
];

/// # Safety
/// `context` and `read` must remain valid for all seven synchronous reads.
/// `read` must accept the static NUL-terminated keys and must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_tooltip_frame(
    context: *mut c_void,
    read: TooltipRead,
) -> u8 {
    let mut complete = true;
    for key in TOOLTIP_KEYS {
        let present = unsafe { read(context, key.as_ptr().cast()) } != 0;
        complete = present && complete;
    }
    u8::from(complete)
}

/// # Safety
/// `context` and `step` must remain valid for synchronous button calls;
/// `step` must obey the native operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_control_buttons(
    context: *mut c_void,
    step: Step,
) -> i32 {
    let mut captured = 0;
    for index in 0..12 {
        if unsafe { step(context, 0, index) } == 0 {
            break;
        }
        captured += 1;
    }
    captured
}

/// # Safety
/// If the scalar guards admit calls, `context` and `step` must remain valid
/// for synchronous layout operations; `step` must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_cinematic_layout(
    has_panel: u8,
    use_raw: u8,
    context: *mut c_void,
    step: Step,
) -> u8 {
    if has_panel == 0 {
        return 0;
    }
    if use_raw == 0 {
        for operation in 0..3 {
            if unsafe { step(context, operation, 0) } == 0 {
                return 0;
            }
        }
    }
    for operation in 3..6 {
        if unsafe { step(context, operation, 0) } == 0 {
            return 0;
        }
    }
    1
}

/// # Safety
/// When the native path invokes `step`, `context` and `step` must remain
/// valid for synchronous rectangle and button operations without unwinding.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_cinematic_buttons(
    layout_complete: u8,
    use_raw: u8,
    raw_buttons: u8,
    context: *mut c_void,
    step: Step,
) -> u8 {
    // The rectangle queries precede the layout-completeness test in native.
    let rectangles = if use_raw != 0 {
        raw_buttons != 0
    } else {
        (unsafe { step(context, 0, 0) } != 0) && (unsafe { step(context, 1, 0) } != 0)
    };
    if layout_complete == 0 || !rectangles {
        return 0;
    }
    for operation in 2..8 {
        if unsafe { step(context, operation, 0) } == 0 {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_gui_tooltip_fallbacks(has_background: u8, has_text: u8) -> u8 {
    u8::from(has_background == 0) | (u8::from(has_text == 0) << 1)
}

/// # Safety
/// If the input guards admit pieces, `context` and `piece` must remain valid
/// for synchronous operations; `piece` must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_speed_background(
    inputs_complete: u8,
    piece_count: i32,
    context: *mut c_void,
    piece: Step,
) -> u8 {
    if inputs_complete == 0 || !(1..=16).contains(&piece_count) {
        return 0;
    }
    for index in 0..piece_count as u32 {
        if unsafe { piece(context, 0, index) } == 0 {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_gui_background_count_valid(count: i32) -> u8 {
    u8::from((1..=16).contains(&count))
}

/// # Safety
/// `context` and `piece` must remain valid for `count` synchronous calls;
/// `piece` must obey the native operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_background_pieces(
    count: u32,
    context: *mut c_void,
    piece: Step,
) -> u8 {
    for index in 0..count {
        if unsafe { piece(context, 1, index) } == 0 {
            return 0;
        }
    }
    1
}

/// # Safety
/// `context` and `row` must remain valid for three synchronous row calls;
/// `row` must obey the native operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_resource_strip(
    context: *mut c_void,
    row: Step,
) -> u8 {
    for index in 0..3 {
        if unsafe { row(context, 0, index) } == 0 {
            return 0;
        }
    }
    1
}

/// # Safety
/// If `complete` is nonzero, `context` and `read_ready` must remain valid
/// for one synchronous call, which must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_ready(
    complete: u8,
    context: *mut c_void,
    read_ready: ReadReady,
) -> u8 {
    if complete == 0 {
        return 0;
    }
    u8::from(unsafe { read_ready(context) } != 0)
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate std;
    use self::std::vec::Vec;

    struct Trace {
        calls: Vec<u32>,
        fail_at: u32,
        mutation: bool,
    }
    unsafe extern "C" fn read(ctx: *mut c_void, key: *const c_char) -> u8 {
        let t = unsafe { &mut *ctx.cast::<Trace>() };
        let key = unsafe { core::ffi::CStr::from_ptr(key) }.to_bytes();
        let index = TOOLTIP_KEYS
            .iter()
            .position(|candidate| &candidate[..candidate.len() - 1] == key)
            .unwrap() as u32;
        t.calls.push(index);
        if index == 0 {
            t.mutation = true;
        }
        u8::from(index != t.fail_at)
    }
    unsafe extern "C" fn step(ctx: *mut c_void, op: u32, index: u32) -> u8 {
        let t = unsafe { &mut *ctx.cast::<Trace>() };
        t.calls.push(op * 100 + index);
        u8::from(op * 100 + index != t.fail_at)
    }
    #[test]
    fn tooltip_reads_all_seven_even_after_missing_and_mutation() {
        let mut t = Trace {
            calls: Vec::new(),
            fail_at: 1,
            mutation: false,
        };
        let result =
            unsafe { a2fo_rs_stage9_a1_gui_tooltip_frame((&mut t as *mut Trace).cast(), read) };
        assert_eq!(result, 0);
        assert!(t.mutation);
        assert_eq!(t.calls, [0, 1, 2, 3, 4, 5, 6]);
    }
    #[test]
    fn control_capture_stops_at_first_gap_and_respects_cap() {
        let mut t = Trace {
            calls: Vec::new(),
            fail_at: 3,
            mutation: false,
        };
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_control_buttons((&mut t as *mut Trace).cast(), step) },
            3
        );
        assert_eq!(t.calls, [0, 1, 2, 3]);
        t.calls.clear();
        t.fail_at = u32::MAX;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_control_buttons((&mut t as *mut Trace).cast(), step) },
            12
        );
        assert_eq!(t.calls.len(), 12);
    }
    #[test]
    fn cinematic_short_circuit_and_rectangle_read_order() {
        let mut t = Trace {
            calls: Vec::new(),
            fail_at: 101,
            mutation: false,
        };
        let ctx = (&mut t as *mut Trace).cast();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_cinematic_layout(1, 0, ctx, step) },
            1
        );
        assert_eq!(t.calls, [0, 100, 200, 300, 400, 500]);
        t.calls.clear();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_cinematic_buttons(0, 0, 0, ctx, step) },
            0
        );
        assert_eq!(t.calls, [0, 100]);
        t.calls.clear();
        t.fail_at = 400;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_cinematic_buttons(1, 1, 1, ctx, step) },
            0
        );
        assert_eq!(t.calls, [200, 300, 400]);
    }
    #[test]
    fn tooltip_fallback_pair_and_speed_count_boundaries() {
        assert_eq!(a2fo_rs_stage9_a1_gui_tooltip_fallbacks(1, 1), 0);
        assert_eq!(a2fo_rs_stage9_a1_gui_tooltip_fallbacks(0, 1), 1);
        assert_eq!(a2fo_rs_stage9_a1_gui_tooltip_fallbacks(1, 0), 2);
        assert_eq!(a2fo_rs_stage9_a1_gui_tooltip_fallbacks(0, 0), 3);
        let mut t = Trace {
            calls: Vec::new(),
            fail_at: u32::MAX,
            mutation: false,
        };
        let ctx = (&mut t as *mut Trace).cast();
        for count in [0, 17] {
            assert_eq!(
                unsafe { a2fo_rs_stage9_a1_gui_speed_background(1, count, ctx, step) },
                0
            );
            assert!(t.calls.is_empty());
        }
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_speed_background(0, 2, ctx, step) },
            0
        );
        t.fail_at = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_speed_background(1, 3, ctx, step) },
            0
        );
        assert_eq!(t.calls, [0, 1]);
    }
    #[test]
    fn readiness_short_circuits_native_read() {
        struct ReadyTrace {
            reads: u32,
            value: u8,
        }
        unsafe extern "C" fn read_ready(context: *mut c_void) -> u8 {
            let trace = unsafe { &mut *context.cast::<ReadyTrace>() };
            trace.reads += 1;
            trace.value
        }
        let mut trace = ReadyTrace { reads: 0, value: 1 };
        let context = (&mut trace as *mut ReadyTrace).cast();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_ready(0, context, read_ready) },
            0
        );
        assert_eq!(trace.reads, 0);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_ready(1, context, read_ready) },
            1
        );
        assert_eq!(trace.reads, 1);
        trace.value = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_ready(1, context, read_ready) },
            0
        );
        assert_eq!(trace.reads, 2);
    }
    #[test]
    fn resource_row_then_piece_drivers_stop_after_mutated_failure() {
        let mut t = Trace {
            calls: Vec::new(),
            fail_at: 1,
            mutation: false,
        };
        let ctx = (&mut t as *mut Trace).cast();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_resource_strip(ctx, step) },
            0
        );
        assert_eq!(t.calls, [0, 1]);
        t.calls.clear();
        t.fail_at = 101;
        assert_eq!(a2fo_rs_stage9_a1_gui_background_count_valid(0), 0);
        assert_eq!(a2fo_rs_stage9_a1_gui_background_count_valid(16), 1);
        assert_eq!(a2fo_rs_stage9_a1_gui_background_count_valid(17), 0);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_gui_background_pieces(3, ctx, step) },
            0
        );
        assert_eq!(t.calls, [100, 101]);
    }
}
