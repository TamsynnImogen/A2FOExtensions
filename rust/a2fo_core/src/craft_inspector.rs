//! Fleet Ops inspector workflows. The native boundary owns draft/session data,
//! Delphi strings/rows, exact SDK calling conventions and raw memory admission.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, u32) -> usize;
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_inspector_draft(
    ctx: *mut c_void,
    flags: *mut u32,
    shields: u8,
    step: Option<Step>,
) -> u8 {
    let Some(s) = step else {
        return 0;
    };
    if flags.is_null() {
        return 0;
    }
    // SAFETY: the caller-owned draft and session live for this synchronous
    // workflow. Raw flags access never keeps a Rust reference across callbacks.
    unsafe {
        s(ctx, 0, 0);
        if s(ctx, 1, 0) == 0 || s(ctx, 1, 1) == 0 {
            s(ctx, 10, 0);
            return 0;
        }
        for i in 0..2 {
            let reset = s(ctx, 2, i);
            if reset == 0 {
                return 0;
            }
            if reset == 2 {
                *flags &= !(1 << i);
            } else if s(ctx, 3, i) != 0 {
                *flags |= 1 << i;
            }
        }
        if shields != 0 {
            for i in 0..4 {
                if s(ctx, 4, i) == 0 || s(ctx, 5, i) == 0 || s(ctx, 6, i) == 0 || s(ctx, 7, i) == 0
                {
                    s(ctx, 10, 1);
                    return 0;
                }
            }
            *flags |= 4;
        }
        if s(ctx, 8, 0) == 0 {
            s(ctx, 10, 2);
            return 0;
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_inspector_build(ctx: *mut c_void, step: Option<Step>) {
    let Some(s) = step else {
        return;
    };
    // SAFETY: each primitive is synchronous and retains all native ownership.
    unsafe {
        if s(ctx, 0, 0) == 0 {
            return;
        }
        s(ctx, 1, 0);
        if s(ctx, 2, 0) == 0 {
            return;
        }
        s(ctx, 3, 0);
        if s(ctx, 4, 0) == 0 {
            s(ctx, 10, 0);
            return;
        }
        s(ctx, 5, 0);
        for i in 0..4 {
            s(ctx, 6, i);
        }
        let shields = s(ctx, 7, 0) != 0;
        let n = if shields { 12 } else { 4 };
        if shields {
            for i in 4..12 {
                s(ctx, 6, i);
            }
        }
        let mut complete = true;
        for i in 0..n {
            if s(ctx, 8, i) == 0 {
                complete = false;
                break;
            }
        }
        s(ctx, 9, u32::from(complete));
        s(
            ctx,
            10,
            if !complete {
                3
            } else if shields {
                1
            } else {
                2
            },
        );
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_inspector_okay(ctx: *mut c_void, step: Option<Step>) {
    let Some(s) = step else {
        return;
    };
    // SAFETY: fine-grained native primitives retain draft, allocation, and UI
    // ownership; the original apply/close calls execute only after validation.
    unsafe {
        if s(ctx, 0, 0) == 0 {
            return;
        }
        if s(ctx, 1, 0) == 0 || s(ctx, 2, 0) == 0 {
            s(ctx, 3, 0);
            return;
        }
        if s(ctx, 4, 0) == 0 {
            s(ctx, 10, 0);
            return;
        }
        if s(ctx, 5, 0) == 0 {
            return;
        }
        if s(ctx, 6, 0) == 0 {
            s(ctx, 10, 1);
            return;
        }
        s(ctx, 7, 0);
        if s(ctx, 8, 0) != 0 && s(ctx, 9, 0) == 0 {
            s(ctx, 12, 0);
            s(ctx, 10, 2);
            return;
        }
        s(ctx, 12, 1);
        s(ctx, 11, 0);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_inspector_destroy(ctx: *mut c_void, step: Option<Step>) {
    let Some(s) = step else {
        return;
    };
    // SAFETY: native callback completes first; then its matching session clears.
    unsafe {
        if s(ctx, 0, 0) != 0 {
            s(ctx, 1, 0);
        }
        if s(ctx, 2, 0) != 0 {
            s(ctx, 3, 0);
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_inspector_init(ctx: *mut c_void, step: Option<Step>) -> u8 {
    let Some(s) = step else {
        return 0;
    };
    // SAFETY: signature/patch primitives preserve all exact native addresses,
    // byte patterns and gateway checks, including partial-install passthrough.
    unsafe {
        if s(ctx, 0, 0) != 0 {
            return 1;
        }
        s(ctx, 1, 0);
        let mut valid = s(ctx, 2, 0) != 0;
        if valid {
            for i in 0..7 {
                if s(ctx, 3, i) == 0 {
                    valid = false;
                    break;
                }
            }
        }
        if !valid {
            s(ctx, 8, 0);
            return 0;
        }
        let mut ready = true;
        for i in 0..3 {
            if s(ctx, 4, i) == 0 || s(ctx, 5, i) == 0 {
                ready = false;
                break;
            }
        }
        s(ctx, 6, u32::from(ready));
        s(ctx, 7, u32::from(ready));
        u8::from(ready)
    }
}
