//! Runtime sequencing for weapon damage and point-defense hooks. Engine
//! addresses, state containers, patch operations and ABI thunks stay native.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;
fn call(c: *mut c_void, op: Op, id: u32, arg: u32) -> u64 {
    unsafe { op(c, id, arg) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 {
        return 0;
    }
    if call(c, op, 0, 0) == 0 {
        return 0;
    } // acquire native modules
    call(c, op, 1, 1); // publish ready during checked install
    if call(c, op, 2, 0) == 0 {
        // preflight
        call(c, op, 1, 0);
        call(c, op, 6, 0);
        return 1;
    }
    call(c, op, 3, 0); // continuation pointers
    for i in 0..10 {
        // eight checked hooks, then two jumps
        if call(c, op, 4, i) == 0 {
            call(c, op, 1, 0);
            call(c, op, 6, 0);
            return 1;
        }
    }
    call(c, op, 5, 0);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_register(valid: u8, c: *mut c_void, op: Op) {
    if valid == 0 {
        return;
    }
    let mut present = false;
    for i in 0..64 {
        present |= call(c, op, 0, i) != 0;
    }
    let overflow = call(c, op, 1, 0) != 0;
    present |= overflow;
    present |= call(c, op, 2, 0) != 0;
    present |= call(c, op, 3, 0) != 0;
    if !present {
        return;
    }
    if overflow {
        call(c, op, 4, 0);
        return;
    }
    let status = call(c, op, 5, 0) as u32;
    if status != 0 {
        call(c, op, 6, status);
        return;
    }
    if call(c, op, 7, 0) == 0 {
        call(c, op, 8, 0);
        return;
    }
    call(c, op, 9, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_select(c: *mut c_void, op: Op) -> u32 {
    if call(c, op, 0, 0) == 0 {
        return call(c, op, 1, 0) as u32;
    }
    if call(c, op, 2, 0) == 0 {
        return call(c, op, 3, 0) as u32;
    }
    call(c, op, 4, 0) as u32
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_update(c: *mut c_void, op: Op) {
    if call(c, op, 0, 0) == 0 || call(c, op, 1, 0) == 0 || call(c, op, 2, 0) == 0 {
        return;
    }
    let advanced = call(c, op, 3, 0) as u32;
    call(c, op, 4, advanced);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_simulate(c: *mut c_void, op: Op) -> usize {
    let policy = call(c, op, 0, 0) != 0;
    call(c, op, 1, 0); // timer before, fire count if policy
    let result = call(c, op, 2, 0) as usize;
    if policy {
        call(c, op, 3, 0);
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_reset(c: *mut c_void, op: Op) -> usize {
    if call(c, op, 0, 0) == 0 {
        return call(c, op, 3, 0) as usize;
    }
    call(c, op, 1, 0); // save native delay and substitute selected delay
    let result = call(c, op, 3, 0) as usize;
    call(c, op, 2, 0); // restore native delay
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_save(c: *mut c_void, op: Op) -> usize {
    let native = call(c, op, 0, 0) as usize;
    if native & 0xff == 0 || call(c, op, 1, 0) == 0 {
        return native;
    }
    if call(c, op, 2, 0) != 0 && call(c, op, 3, 0) != 0 {
        1
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_point_load(c: *mut c_void, op: Op) -> usize {
    let native = call(c, op, 0, 0) as usize;
    if native & 0xff == 0 || call(c, op, 1, 0) == 0 {
        return native;
    }
    if call(c, op, 2, 0) == 0 {
        return native;
    }
    if call(c, op, 3, 0) == 0 {
        call(c, op, 4, 0);
        return 0;
    }
    call(c, op, 5, 0);
    native
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_point_timer(ready: u8, timer: f32) -> u8 {
    u8::from(ready == 0 || !timer.is_finite() || timer <= 0.0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_damage_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 || call(c, op, 0, 0) == 0 {
        return 0;
    }
    call(c, op, 1, 0); // native lock initialization
    let mut ready = call(c, op, 2, 0) != 0; // signature preflight
    if ready {
        call(c, op, 3, 0);
        ready = call(c, op, 4, 0) != 0;
    }
    if ready {
        ready = call(c, op, 5, 0) != 0;
    }
    call(c, op, 7, u32::from(ready));
    if ready {
        ready = call(c, op, 6, 0) != 0;
    }
    call(c, op, 7, u32::from(ready));
    if ready {
        let connected = call(c, op, 8, 0) != 0;
        call(c, op, 9, 0);
        if connected {
            call(c, op, 10, 0);
        }
    } else {
        call(c, op, 11, 0);
    }
    1 // checked hooks are process lifetime; failed install is pass through
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_damage_register(valid: u8, c: *mut c_void, op: Op) {
    if valid == 0 || call(c, op, 0, 0) == 0 {
        return;
    } // allocate/lookups
    let inherited = call(c, op, 1, 0) != 0;
    let shields = call(c, op, 2, 0) != 0;
    let hull = call(c, op, 3, 0) != 0;
    let shield_modifier = call(c, op, 4, 0) != 0;
    let hull_modifier = call(c, op, 5, 0) != 0;
    if !(inherited || shields || hull || shield_modifier || hull_modifier) {
        return;
    }
    if call(c, op, 6, 0) != 0 {
        call(c, op, 7, 0);
    }
    if call(c, op, 8, 0) != 0 {
        call(c, op, 9, 0);
    }
    call(c, op, 10, 0); // scalar defaults and native log text
    if call(c, op, 11, 0) == 0 {
        call(c, op, 12, 0);
        return;
    }
    call(c, op, 13, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_damage_hit(c: *mut c_void, op: Op) -> usize {
    if call(c, op, 0, 0) == 0 {
        return call(c, op, 5, 0) as usize;
    }
    let directional = call(c, op, 1, 0) != 0;
    if call(c, op, 2, 0) != 0 {
        call(c, op, 3, 0);
    }
    let result = call(c, op, 5, 0) as usize;
    call(c, op, 6, 0); // TLS restore even without policy
    if directional {
        call(c, op, 7, 0);
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_damage_refresh(ready: u8, c: *mut c_void, op: Op) -> u8 {
    u8::from(ready != 0 && call(c, op, 0, 0) != 0)
}
