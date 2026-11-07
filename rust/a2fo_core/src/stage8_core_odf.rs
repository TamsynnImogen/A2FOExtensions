//! Core ParameterDB snapshot and Race ODF dispatch decisions. Native callbacks
//! retain opaque engine ABI, CRT formatting, storage, locking and exceptions.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
fn call(ctx: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let result = unsafe { op(ctx, id) };
    (result != u64::MAX).then_some(result)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_odf_snapshot(ctx: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(ctx, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    let found = step!(0) != 0;
    step!(1); // force final NUL after native string query
    step!(2); // bounded memchr, before string assignment or scalar lookups
    if step!(3) == 0 {
        return 0;
    }
    if found {
        step!(4);
        return 1;
    }
    if step!(5) != 0 && step!(6) != 0 {
        step!(7); // native %.9g formatting with GNU++17/O2 float conversion
        if step!(8) != 0 && step!(9) != 0 {
            step!(10);
            return 1;
        }
    }
    if step!(11) != 0 && step!(12) != 0 {
        step!(13);
        return 1;
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_odf_race(ctx: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(ctx, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(21); // native lock guards all three collection copies together
    if step!(22) == 0 {
        return;
    }
    let commands = step!(23);
    for index in 0..commands {
        step!(0x10000 + index as u32); // current command and per-row string
        if step!(24) != 0 {
            step!(25);
        } else if step!(26) != 0 {
            step!(27);
        }
        step!(28); // end per-row string lifetime
    }
    step!(29); // reserve field views before iteration
    let fields = step!(30);
    for index in 0..fields {
        step!(0x20000 + index as u32);
    }
    step!(31); // one mutable event shared by all handlers
    let handlers = step!(32);
    for index in 0..handlers {
        step!(0x30000 + index as u32);
        if step!(33) != 0 {
            step!(34);
        }
    }
}
