//! Native UI hook sequencing; OS input, engine memory and ABI remain C++.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;
fn call(c: *mut c_void, op: Op, id: u32, arg: u32) -> u64 {
    unsafe { op(c, id, arg) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_edit_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 || call(c, op, 0, 0) == 0 {
        return 0;
    }
    call(c, op, 1, 0); // native ODF index, exceptions contained by caller
    let mut ready = call(c, op, 2, 0) != 0;
    if ready {
        ready = call(c, op, 3, 0) != 0;
    }
    call(c, op, 4, u32::from(ready));
    1 // process-lifetime patch
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_edit_update(c: *mut c_void, op: Op) {
    let activation = call(c, op, 0, 0);
    call(c, op, 1, 0); // activation edge and retained scratch restoration
    call(c, op, 2, 0); // level before
    call(c, op, 3, 0); // native Update
    if activation == 0 || call(c, op, 4, 0) != activation {
        call(c, op, 5, 0); // restore only outside reentrant hook
        return;
    }
    if call(c, op, 6, 0) == 0 {
        return;
    }
    call(c, op, 7, 0); // level after
    call(c, op, 8, 1); // processing guard
    call(c, op, 9, 0); // native exception boundary around transition
    call(c, op, 8, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_edit_transition(
    valid: u8,
    before: i32,
    after: i32,
    c: *mut c_void,
    op: Op,
) {
    if valid == 0 {
        return;
    }
    call(c, op, 0, 0); // restore scratch owned by another menu
    if before == 1 && after == 2 {
        call(c, op, 2, 0);
        return;
    }
    let scratch = call(c, op, 1, 0) != 0;
    if scratch && before == 1 && after == 0 {
        call(c, op, 3, 0);
        return;
    }
    if scratch && !(0..=2).contains(&after) {
        call(c, op, 4, 0);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_edit_enter(c: *mut c_void, op: Op) {
    for id in 0..=2 {
        let result = call(c, op, id, 0);
        if result == 0 || result == u64::MAX {
            return;
        }
    }
    let cycle = call(c, op, 3, 0);
    if cycle == u64::MAX {
        return;
    }
    if cycle != 0 {
        call(c, op, 4, 0);
        return;
    }
    let depth = call(c, op, 5, 0);
    if depth == u64::MAX {
        return;
    }
    if depth != 0 {
        call(c, op, 6, 0);
        return;
    }
    if call(c, op, 7, 0) == u64::MAX {
        return;
    } // breadcrumb push
    let installed = call(c, op, 8, 0);
    if installed == u64::MAX {
        return;
    }
    if installed == 0 {
        call(c, op, 9, 0);
        return;
    }
    call(c, op, 10, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_edit_leave(c: *mut c_void, op: Op) {
    let scratch = call(c, op, 0, 0);
    if scratch == u64::MAX {
        return;
    }
    if scratch == 0 {
        call(c, op, 1, 0);
        return;
    }
    let top = call(c, op, 2, 0);
    if top == u64::MAX {
        return;
    }
    if top != 0 {
        call(c, op, 3, 0);
        return;
    }
    let installed = call(c, op, 4, 0);
    if installed == u64::MAX {
        return;
    }
    if installed == 0 {
        call(c, op, 1, 0);
        return;
    }
    call(c, op, 5, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_edit_shutdown(c: *mut c_void, op: Op) {
    if call(c, op, 0, 0) != 0 {
        call(c, op, 1, 0);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_instant_init(
    valid: u8,
    profile: u8,
    c: *mut c_void,
    op: Op,
) -> u8 {
    if valid == 0 || call(c, op, 0, 0) == 0 {
        return 0;
    }
    let mut ready = call(c, op, 1, 0) != 0; // binary identity and signatures
    if ready {
        ready = call(c, op, 2, 0) != 0;
    } // native SEH guard
    if ready {
        ready = call(c, op, 3, 0) != 0;
    } // missing-race guard
    if ready && profile != 0 {
        for index in 4..=7 {
            if call(c, op, index, 0) == 0 {
                ready = false;
                break;
            }
        }
    } else if ready {
        call(c, op, 9, 0);
    }
    call(c, op, 8, u32::from(ready));
    1 // process-lifetime checked hooks remain resident
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_instant_load(c: *mut c_void, op: Op) {
    call(c, op, 0, 0); // interlocked invocation count
    call(c, op, 1, 0); // save and publish native load scope
    call(c, op, 2, 0); // native LoadSettings
    call(c, op, 3, 0); // restore nested scope
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_instant_reader(c: *mut c_void, op: Op) -> usize {
    if call(c, op, 0, 0) == 0 {
        return call(c, op, 6, 0) as usize;
    }
    call(c, op, 1, 0); // bounded native path read
    let tracked = call(c, op, 2, 0) != 0;
    let redirected = tracked && call(c, op, 3, 0) != 0 && call(c, op, 4, 0) != 0;
    let result = call(c, op, 6, 0) as usize;
    if redirected {
        call(c, op, 7, 0);
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_instant_blob(
    size: u32,
    setup_size: u32,
    c: *mut c_void,
    op: Op,
) -> u8 {
    if call(c, op, 0, 0) == 0 {
        return u8::from(call(c, op, 5, 0) != 0);
    }
    let cursor = call(c, op, 1, 0) != 0;
    let active = call(c, op, 2, 0) != 0;
    if active && size == setup_size && cursor {
        if call(c, op, 3, 0) != 0 {
            return 1;
        }
        call(c, op, 4, 0); // scoped decoder failure diagnostic
    }
    u8::from(call(c, op, 5, 0) != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_instant_dialog(c: *mut c_void, op: Op) -> isize {
    if call(c, op, 0, 0) == 0 {
        return 0;
    }
    let activation = call(c, op, 1, 0);
    if activation == 0 {
        return call(c, op, 4, 0) as isize;
    }
    let click = call(c, op, 2, 0) != 0;
    let before = call(c, op, 3, 0);
    let result = call(c, op, 4, 0) as isize;
    let after = call(c, op, 5, 0);
    if !click || after != before || call(c, op, 6, 0) != activation {
        return result;
    }
    if call(c, op, 7, 0) != 0 && call(c, op, 8, 0) != 0 {
        call(c, op, 9, 0);
    }
    result
}
