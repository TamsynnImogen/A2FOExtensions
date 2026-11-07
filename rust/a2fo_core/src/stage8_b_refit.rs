//! Refit main-menu initialization order. Native callbacks own SDK, Win32,
//! hook, artwork, and scene resources; this sequence owns admission and gates.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
const ERROR: u64 = u64::MAX;

fn call(context: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let value = unsafe { op(context, id) };
    (value != ERROR).then_some(value)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_init(context: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id) {
                Some(value) => value,
                None => return 1,
            }
        };
    }
    if step!(0) == 0 {
        return 0;
    } // API shape and required callbacks
    if step!(1) != 0 {
        return 1;
    } // prior initialization
    step!(2); // commit initialized and API before reading root
    if step!(3) == 0 {
        return 1;
    }
    if step!(4) == 0 {
        step!(5);
        return 1;
    }
    if step!(6) == 0 || step!(7) != 0 || step!(8) != 0 || step!(9) == 0 {
        step!(10);
        return 1;
    }
    let action_message = step!(11);
    let restore_message = step!(12);
    if action_message == 0 || restore_message == 0 || step!(13) == 0 {
        step!(14);
        return 1;
    }
    if step!(15) == 0 {
        step!(16);
        return 1;
    }
    step!(17); // publish native hook gateway
    step!(18); // live scene setting
    if step!(19) != 0 {
        step!(20);
    } // width invalid diagnostic
    step!(21); // configure scene
    if step!(22) == 0 {
        step!(23);
    }
    step!(24); // setup setting
    let loading = step!(25);
    step!(26); // animation setting read even when loading disabled
    if loading != 0 && step!(27) == 0 {
        step!(28);
    }
    step!(29); // background mode diagnostic
    if step!(30) == 0 {
        step!(31);
    }
    step!(32); // publish readiness
    step!(33);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_attach(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(0) == 0 {
        return;
    } // no active session and hook ready
    step!(1); // allocate session and assign form
    for id in 2..=7 {
        // native handle, window, thread, class, and model checks
        if step!(id) == 0 {
            step!(8);
            return;
        }
    }
    step!(9); // advance nonzero generation
    step!(10); // copy resolved artwork
    if step!(11) == 0 {
        step!(12);
        return;
    } // install subclass before hiding controls
    step!(13); // publish session and active window
    step!(14); // hide native visuals
    step!(15); // active/shown counters
    step!(16); // invalidation
    step!(17); // attached diagnostic
    step!(18); // start setup observer
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_restore(
    destroying: u8,
    context: *mut c_void,
    op: Op,
) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(0) != 0 {
        return;
    }
    step!(1); // mark restoring
    step!(2); // kill handover timer
    if step!(3) != 0 {
        step!(4);
    }
    if destroying == 0 {
        step!(5);
    } // restore controls
    if step!(6) != 0 {
        step!(7);
    } // drop active session and window
    if destroying == 0 {
        step!(8);
    } // invalidate
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_present_gate(c: *mut c_void, op: Op) -> u8 {
    for id in 0..=5 {
        match call(c, op, id) {
            Some(value) if value != 0 => {}
            _ => return 0,
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_shutdown(c: *mut c_void, op: Op) {
    for id in 0..=5 {
        if call(c, op, id).is_none() {
            return;
        }
    }
    match call(c, op, 6) {
        Some(value) if value != 0 => {
            let _ = call(c, op, 7);
        }
        None => return,
        _ => {}
    }
    let _ = call(c, op, 8);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_queue(c: *mut c_void, op: Op) -> u8 {
    if !matches!(call(c, op, 0), Some(value) if value != 0) {
        return 0;
    }
    match call(c, op, 1) {
        Some(value) if value != 0 => 1,
        Some(_) => {
            let _ = call(c, op, 2);
            0
        }
        None => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_timer(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 1,
            }
        };
    }
    if step!(0) == 0 {
        return 0;
    }
    let prepared = step!(1) != 0;
    if !prepared && step!(2) != 0 && step!(3) != 0 {
        return 1;
    }
    step!(4); // cancel handover timer
    step!(5); // capture deferred row and clear it
    if !prepared {
        step!(6);
    }
    if step!(7) == 0 {
        step!(6);
        step!(8);
        return 1;
    }
    step!(9); // action count
    step!(10); // native action call
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_action(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(11) == 0 {
        return;
    }
    if step!(12) == 0 || step!(7) == 0 {
        step!(8);
        return;
    }
    // Native take returns row + 1 so zero remains the no-action sentinel.
    let row = step!(13);
    if row == 0 {
        return;
    }
    if row == 2 && step!(14) != 0 {
        step!(15); // defer row, set deadline
        if step!(16) != 0 {
            return;
        }
        step!(17); // timer failure: clear defer and end handover
    }
    step!(9);
    step!(10);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_paint(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(0); // BeginPaint, even when it returns a null HDC
    if step!(1) != 0 {
        step!(2);
        return;
    } // handover suppresses native menu draw
    let mut painted = false;
    if step!(3) != 0 {
        step!(4); // scene frame
        painted = step!(5) != 0;
        if painted {
            step!(6); // frame-drawn acknowledgement
            if step!(7) != 0 {
                step!(8);
            } // requested choice overlay
        }
    }
    step!(2); // EndPaint
    if !painted {
        step!(9);
        step!(10);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_mouse(kind: u32, c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(0); // snapshot hot and pressed before geometry
    step!(1); // native geometry and x/y extraction
    let scene = step!(2); // choice index + 1
    if kind == 1 && scene != 0 {
        step!(3);
        step!(4);
        return;
    }
    if kind == 2 && step!(5) != 0 {
        if scene != 0 && step!(6) != 0 {
            step!(7);
        }
        step!(8);
        step!(4);
        return;
    }
    match kind {
        0 => {
            step!(9);
            step!(10);
        }
        1 => {
            step!(11);
        }
        2 => {
            if step!(12) != 0 {
                step!(13);
            }
        }
        _ => return,
    }
    if kind != 0 || step!(14) != 0 {
        step!(4);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_key(
    key: u32,
    lp: u32,
    c: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 1,
            }
        };
    }
    // VK_UP, VK_DOWN, VK_TAB, VK_RETURN, VK_SPACE are stable Win32 values.
    if key == 0x26 || key == 0x28 || key == 0x09 {
        let direction = if key == 0x26 || (key == 0x09 && step!(15) != 0) {
            0
        } else {
            1
        };
        if direction == 0 {
            step!(16);
        } else {
            step!(17);
        }
        step!(4);
        return 1;
    }
    if key == 0x0d || key == 0x20 {
        if lp & (1 << 30) == 0 && step!(18) != 0 {
            step!(13);
        }
        return 1;
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_present(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(0) != 0 {
        step!(1);
    } else {
        step!(2);
    }
    if step!(3) == 0 || step!(4) == 0 || step!(5) == 0 {
        return 0;
    }
    step!(6); // native BITMAPINFO descriptor
    if step!(7) == 0 {
        return 0;
    }
    step!(8); // color mode
    step!(9); // native pixels
    step!(10); // restore DC before reporting success
    if step!(11) == 0 {
        return 0;
    }
    step!(12); // acknowledge the drawn frame
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_dispatch(
    message: u32,
    restore_message: u32,
    action_message: u32,
    c: *mut c_void,
    op: Op,
) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    // Win32 WM_* values from winuser.h; the native subclass/ABI stays C++.
    if message == 0x0082 {
        step!(0);
        step!(1);
        return;
    } // NCDESTROY
    if message == restore_message {
        step!(2);
        return;
    }
    if message == 0x0113 && step!(3) != 0 {
        return;
    } // TIMER
    if message == action_message {
        step!(4);
        return;
    }
    if message == 0x000f {
        step!(5);
        return;
    } // PAINT
    if message == 0x0014 {
        step!(12);
        return;
    } // ERASEBKGND
    if message == 0x0005 {
        step!(6);
    } // SIZE
    let deferred = step!(7) != 0;
    let mouse = message == 0x0200 || message == 0x0201 || message == 0x0202;
    if deferred && (message == 0x0100 || mouse) {
        return;
    }
    if mouse {
        step!(8);
        return;
    }
    if message == 0x02a3 || message == 0x001f || message == 0x0008 {
        step!(9);
    }
    if message == 0x0100 && step!(10) != 0 {
        return;
    }
    step!(11);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_guarded_call(c: *mut c_void, op: Op) -> u8 {
    if !matches!(call(c, op, 0), Some(value) if value != 0) {
        return 0;
    }
    u8::from(matches!(call(c, op, 1), Some(value) if value != 0))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_status(c: *mut c_void, op: Op) -> u8 {
    if !matches!(call(c, op, 0), Some(value) if value != 0) {
        return 0;
    }
    if !matches!(call(c, op, 1), Some(value) if value != 0) {
        return 0;
    }
    u8::from(call(c, op, 2).is_some())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_refit_end_departure(c: *mut c_void, op: Op) {
    if call(c, op, 0).is_some() {
        let _ = call(c, op, 1);
    }
}
