//! ODF ownership observer sequencing. Engine calls, native state containers,
//! checked signatures and x87-sensitive craft state remain in C++.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;
fn call(c: *mut c_void, op: Op, id: u32, arg: u32) -> u64 {
    unsafe { op(c, id, arg) }
}
const FAILED: u64 = u64::MAX;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_odf_enabled(
    current: u64,
    previous: *mut u64,
    c: *mut c_void,
    op: Op,
) -> u8 {
    if !previous.is_null() && unsafe { *previous } != current {
        call(c, op, 0, 0); // generation reset of pending renderer refresh
        unsafe { *previous = current };
    }
    u8::from(current != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_odf_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 || call(c, op, 0, 0) == 0 || call(c, op, 1, 0) == 0 || call(c, op, 2, 0) == 0 {
        return 0;
    }
    let craft = if call(c, op, 3, 0) != 0 {
        call(c, op, 4, 0)
    } else {
        call(c, op, 5, 0)
    };
    if craft == 0 {
        return 0;
    }
    call(c, op, 6, 0);
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_odf_race(valid: u8, c: *mut c_void, op: Op) {
    if valid == 0 {
        return;
    }
    let found = call(c, op, 0, 0); // read name, then suffix fields
    if found == FAILED {
        return;
    }
    let name = found & 1 != 0;
    let suffix = found & 2 != 0;
    if suffix {
        let normalized = call(c, op, 1, 0);
        if normalized == FAILED {
            return;
        }
        if normalized == 0 {
            call(c, op, 2, 0);
            return;
        }
    } else if name {
        let borg = call(c, op, 3, 0);
        if borg == FAILED {
            return;
        }
        if borg != 0 {
            call(c, op, 4, 0);
        }
    }
    let empty = call(c, op, 5, 0);
    if empty == FAILED {
        return;
    }
    if empty != 0 {
        call(c, op, 6, 0);
        return;
    }
    call(c, op, 7, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_odf_swap(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 {
        return 0;
    }
    for id in 0..=2 {
        let result = call(c, op, id, 0);
        if result == 0 || result == FAILED {
            return 0;
        }
    }
    let same = call(c, op, 3, 0);
    if same == FAILED || same != 0 {
        return 0;
    }
    let mut target = call(c, op, 4, 0);
    if target == FAILED {
        return 0;
    }
    if target == 0 {
        let suffix = call(c, op, 5, 0);
        if suffix == FAILED {
            return 0;
        }
        if suffix != 0 {
            target = call(c, op, 6, 0);
        }
    }
    let same = call(c, op, 7, 0);
    if same == FAILED || same != 0 {
        return 0;
    }
    if target == 0 {
        call(c, op, 8, 0);
        return 0;
    }
    let transform = call(c, op, 9, 0);
    if transform == FAILED || transform == 0 {
        return 0;
    }
    let constructed = call(c, op, 10, 0);
    if constructed == FAILED {
        return 0;
    }
    if constructed == 0 {
        call(c, op, 11, 0);
        return 0;
    }
    if call(c, op, 12, 0) == FAILED {
        return 0;
    }
    let mission = call(c, op, 13, 0);
    if mission == FAILED {
        return 0;
    }
    if mission == 0 {
        call(c, op, 14, 0);
        return 0;
    }
    if call(c, op, 15, 0) == FAILED || call(c, op, 16, 0) == FAILED {
        return 0;
    }
    u8::from(call(c, op, 17, 0) != FAILED)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_odf_craft(
    valid: u8,
    enabled: u8,
    kind: u32,
    c: *mut c_void,
    op: Op,
) {
    if valid == 0 {
        return;
    }
    match kind {
        0 => {
            // SIMULATE_POST, caller passes native event-kind mapping
            if enabled == 0 {
                return;
            }
            if call(c, op, 0, 0) == FAILED {
                return;
            }
            let existing = call(c, op, 1, 0);
            if existing == FAILED {
                return;
            }
            if existing == 0 {
                if call(c, op, 2, 0) == FAILED {
                    return;
                }
                let odf = call(c, op, 3, 0);
                if odf == FAILED || odf == 0 {
                    return;
                }
                if call(c, op, 4, 0) == FAILED
                    || call(c, op, 5, 0) == FAILED
                    || call(c, op, 6, 0) == FAILED
                {
                    return;
                }
                let swap = call(c, op, 7, 0);
                if swap != FAILED && swap != 0 {
                    call(c, op, 8, 0);
                }
                return;
            }
            let same = call(c, op, 9, 0);
            if same == FAILED || same != 0 {
                return;
            }
            call(c, op, 10, 0);
        }
        1 => {
            call(c, op, 11, 0);
        } // CLEANUP
        2 => {
            call(c, op, 12, 0);
        } // POST_LOAD
        _ => {}
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_odf_shutdown(c: *mut c_void, op: Op) {
    for id in 0..7 {
        call(c, op, id, 0);
    }
}
