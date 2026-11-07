//! A1 startup admission, fatal registration, optional installation and final
//! readiness workflow. Native callbacks retain SDK/CRT/Win32 and hook bodies.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, u64) -> u64;
fn call(ctx: *mut c_void, op: Op, id: u32, value: u64) -> Option<u64> {
    let result = unsafe { op(ctx, id, value) };
    (result != u64::MAX).then_some(result)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_lifecycle_init(ctx: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(ctx, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr,$value:expr) => {
            match call(ctx, op, $id, $value) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 0..=30 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(31);
    step!(32);
    step!(33);
    if step!(34) == 0 {
        step!(35);
        step!(36);
        return 0;
    }
    for id in (37..=55).step_by(2) {
        if step!(id) == 0 {
            step!(id + 1);
            step!(36);
            return 0;
        }
    }
    step!(57);
    step!(58);
    if step!(59) == 0 {
        step!(60);
        step!(36);
        return 0;
    }
    if step!(61) == 0 {
        step!(62);
        step!(36);
        return 0;
    }
    step!(63);
    step!(64);
    if step!(65) != 0 {
        step!(66);
    }
    step!(67);
    let safe_mode = step!(68) != 0;
    if safe_mode {
        step!(69);
    }
    if step!(70) == 0 {
        for id in 71..=76 {
            step!(id);
        }
        return 0;
    }
    step!(77);
    if step!(78) != 0 {
        step!(79);
    }
    for id in (80..=92).step_by(2) {
        if step!(id) == 0 {
            step!(id + 1);
        }
    }
    let mut resource_ready = step!(94) != 0 && step!(95) != 0;
    if resource_ready {
        for id in 96..=101 {
            if step!(id) == 0 {
                resource_ready = false;
                break;
            }
        }
    }
    step!(102, u64::from(resource_ready));
    step!(103);
    if step!(104) == 0 {
        step!(104, 1);
    }
    for id in (105..=115).step_by(2) {
        if step!(id) == 0 {
            step!(id + 1);
        }
    }
    step!(117);
    step!(118);
    if !safe_mode {
        let producer = step!(119);
        step!(120, producer);
        if producer == 0 {
            step!(121);
        }
        for id in (122..=142).step_by(2) {
            if step!(id) == 0 {
                step!(id + 1);
            }
        }
    } else {
        step!(144);
    }
    let ship = step!(145);
    if ship == 0 {
        step!(146);
    }
    let current_resource_ready = step!(147) != 0;
    let final_resource_ready = current_resource_ready && ship != 0;
    step!(148, u64::from(final_resource_ready));
    if !final_resource_ready {
        step!(149);
    }
    step!(150);
    1
}
