//! Native observer workflows for animation hooks. Raw engine memory, SDK
//! callbacks, channel ownership and i386 thunks remain in C++.
use core::{ffi::c_void, slice};
type Op = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;
fn call(c: *mut c_void, op: Op, id: u32, arg: u32) -> u64 {
    unsafe { op(c, id, arg) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hardpoints_live(
    ready: u8,
    activation: u64,
    previous: *mut u64,
    c: *mut c_void,
    op: Op,
) -> u8 {
    if ready == 0 || activation == 0 {
        return 0;
    }
    if !previous.is_null() && unsafe { *previous } != activation {
        call(c, op, 0, 0);
        unsafe { *previous = activation };
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hardpoints_query(inside: u8, c: *mut c_void, op: Op) -> usize {
    if inside != 0 || call(c, op, 0, 0) == 0 {
        return call(c, op, 3, 0) as usize;
    }
    call(c, op, 1, 0); // recursion guard
    call(c, op, 2, 0); // visible node and channel pose
    let result = call(c, op, 3, 0) as usize; // native original
    call(c, op, 4, 0); // clear recursion guard
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hardpoints_evaluate(c: *mut c_void, op: Op) {
    call(c, op, 0, 0); // resolve controller
    if call(c, op, 1, 0) != 0 {
        call(c, op, 2, 0); // invalidate cached native pose
        if call(c, op, 3, 0) != 0 {
            return;
        } // controlled channels applied
    }
    if call(c, op, 4, 0) == 0 {
        return;
    } // save native times, set start/first frame
    let cache_hit = call(c, op, 5, 0) != 0;
    let applied = cache_hit || call(c, op, 6, 0) != 0;
    call(c, op, 7, 0); // restore native times
    if !cache_hit && applied {
        call(c, op, 8, 0);
    } // commit only after evaluation
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hardpoints_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 || call(c, op, 0, 0) == 0 {
        return 0;
    }
    let mut installed = false;
    if call(c, op, 1, 0) != 0 {
        let first = call(c, op, 2, 0) != 0;
        let second = call(c, op, 3, 0) != 0; // attempt even after first fails
        installed = first && second;
        if !installed {
            call(c, op, 4, 0);
        }
    }
    call(c, op, 5, u32::from(installed));
    call(c, op, 6, u32::from(installed));
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_live(
    activation: u64,
    previous: *mut u64,
    c: *mut c_void,
    op: Op,
) -> u8 {
    if !previous.is_null() && unsafe { *previous } != activation {
        unsafe { *previous = activation };
        call(c, op, 0, 0);
    }
    u8::from(activation != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_install(c: *mut c_void, op: Op) -> u8 {
    const EXCEPTION: u64 = u64::MAX;
    let identity = call(c, op, 16, 0);
    if identity == EXCEPTION {
        return 2;
    }
    if identity == 0 {
        return 0;
    }
    let helpers = call(c, op, 1, 0);
    if helpers == EXCEPTION {
        return 2;
    }
    if helpers == 0 {
        return 0;
    }
    for index in 0..helpers as u32 {
        let result = call(c, op, 2, index);
        if result == EXCEPTION {
            return 2;
        }
        if result == 0 {
            return 0;
        }
    }
    let hooks = call(c, op, 3, 0);
    if hooks == EXCEPTION {
        return 2;
    }
    if hooks == 0 {
        return 0;
    }
    for index in 0..hooks as u32 {
        let result = call(c, op, 4, index);
        if result == EXCEPTION {
            return 2;
        }
        if result == 0 {
            return 0;
        }
    }
    for index in 0..hooks as u32 {
        let result = call(c, op, 5, index);
        if result == EXCEPTION {
            return 2;
        }
        if result == 0 {
            return 0;
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    const EXCEPTION: u64 = u64::MAX;
    if valid == 0 {
        return 0;
    }
    let image = call(c, op, 0, 0);
    if image == 0 || image == EXCEPTION {
        return 0;
    }
    match unsafe { a2fo_rs_animations_install(c, op) } {
        2 => {
            call(c, op, 11, 0);
            return 1;
        }
        0 => {
            call(c, op, 10, 0);
            return 1;
        }
        _ => {}
    }
    let fields = call(c, op, 6, 0);
    if fields == EXCEPTION {
        call(c, op, 11, 0);
        return 1;
    }
    if fields == 0 {
        call(c, op, 11, 0);
        return 1;
    }
    let chunks = call(c, op, 7, 0);
    if chunks == EXCEPTION {
        call(c, op, 11, 0);
        return 1;
    }
    let mut registered = true;
    for index in 0..chunks as u32 {
        let result = call(c, op, 8, index);
        if result == EXCEPTION {
            call(c, op, 11, 0);
            return 1;
        }
        registered = (result != 0) && registered;
    }
    if registered {
        for id in [9, 13, 14, 15] {
            let result = call(c, op, id, 0);
            if result == EXCEPTION {
                call(c, op, 11, 0);
                return 1;
            }
            if result == 0 {
                registered = false;
                break;
            }
        }
    }
    call(c, op, 12, u32::from(registered)); // publish ready and log
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_emit(
    event: *const u8,
    length: usize,
    c: *mut c_void,
    op: Op,
) {
    if event.is_null() {
        return;
    }
    let name = unsafe { slice::from_raw_parts(event, length) };
    let paired = match name {
        b"production_begin" => (1, 1),
        b"production_complete" | b"production_cancel" => (1, 0),
        b"repair_begin" => (2, 1),
        b"repair_end" => (2, 0),
        _ => (0, 0),
    };
    if paired.0 != 0 {
        if call(c, op, 0, paired.0 * 2 + paired.1) == 0 {
            return;
        } // condition edge
        if call(c, op, 1, paired.0) != 0 {
            return;
        } // combined binding owns edge
        if call(c, op, 3, 0) != 0 {
            return;
        }
    }
    call(c, op, 2, 0); // direct event clip
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_evaluate_hook(c: *mut c_void, op: Op) {
    if call(c, op, 0, 0) != 0 && call(c, op, 1, 0) != 0 {
        return;
    }
    call(c, op, 2, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_craft_event(
    kind: u32,
    valid: u8,
    active: u8,
    c: *mut c_void,
    op: Op,
) {
    if valid == 0 {
        return;
    }
    if active == 0 {
        if kind == 2 || kind == 3 {
            call(c, op, 0, 0);
        }
        return;
    }
    match kind {
        2 => {
            call(c, op, 1, 0);
        }
        3 => {
            call(c, op, 2, 0);
        }
        0 | 1 => {
            if call(c, op, 3, kind) == 0 {
                return;
            }
            if kind == 0 {
                call(c, op, 4, 0);
            } else {
                call(c, op, 5, 0);
            }
        }
        _ => (),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_producer_event(
    kind: u32,
    valid: u8,
    active: u8,
    c: *mut c_void,
    op: Op,
) {
    if valid == 0 || active == 0 {
        return;
    }
    match kind {
        4 => {
            call(c, op, 0, 0);
        }
        1 => {
            call(c, op, 1, 0);
        }
        5 => {
            call(c, op, 2, 0);
        }
        2 => {
            call(c, op, 3, 0);
        }
        _ => (),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_read(ready: u8, c: *mut c_void, op: Op) {
    if call(c, op, 0, 0) == 0 {
        return;
    } // native read, always first
    if ready != 0 {
        call(c, op, 1, 0);
    } // retain source frames
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_clone(ready: u8, c: *mut c_void, op: Op) -> usize {
    if ready != 0 {
        call(c, op, 0, 0);
    } // hold source before native clone
    let result = call(c, op, 1, 0) as usize;
    if ready != 0 && result != 0 {
        call(c, op, 2, 0);
    } // publish cloned frames
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_destroy(c: *mut c_void, op: Op) {
    call(c, op, 0, 0); // drop owner before native destroy
    call(c, op, 1, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_animate(c: *mut c_void, op: Op) {
    call(c, op, 0, 0); // push explicit instance
    call(c, op, 1, 0); // native animation
    call(c, op, 2, 0); // restore prior instance
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_animations_observer(c: *mut c_void, op: Op) {
    let activation = call(c, op, 0, 0);
    if call(c, op, 1, 0) == 0 {
        return;
    } // native callback always forwards
    if call(c, op, 2, 0) == 0 || activation != call(c, op, 3, 0) || call(c, op, 4, 0) == 0 {
        return;
    }
    call(c, op, 5, 0); // admitted event update
}
