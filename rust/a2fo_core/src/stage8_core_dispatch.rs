//! Core weapon-class, weapon-trigger and craft-event dispatch workflow.
//! The operation callback retains native containers, event layouts and exceptions.
use core::ffi::c_void;

type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;

fn step(ctx: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let value = unsafe { op(ctx, id) };
    (value != u64::MAX).then_some(value)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_dispatch_weapon_class(ctx: *mut c_void, op: Op) {
    // Preserve the native short-circuit order before entering the try region.
    if step(ctx, op, 0) != Some(1) || step(ctx, op, 1) != Some(1) || step(ctx, op, 2) != Some(1) {
        return;
    }
    if step(ctx, op, 3).is_none() {
        return;
    }
    while step(ctx, op, 4) == Some(1) {
        if step(ctx, op, 5).is_none() {
            return;
        }
        match step(ctx, op, 6) {
            Some(1) => {
                if step(ctx, op, 7).is_none() {
                    return;
                }
            }
            Some(_) => {}
            None => return,
        }
        if step(ctx, op, 8).is_none() {
            return;
        }
    }
    if step(ctx, op, 9).is_none() {
        return;
    }
    if step(ctx, op, 10).is_none() {
        return;
    }
    while step(ctx, op, 11) == Some(1) {
        if step(ctx, op, 12).is_none() {
            return;
        }
    }
    if step(ctx, op, 13).is_none() {
        return;
    }
    if step(ctx, op, 14).is_none() {
        return;
    }
    while step(ctx, op, 15) == Some(1) {
        match step(ctx, op, 16) {
            Some(1) => {
                if step(ctx, op, 17).is_none() {
                    return;
                }
            }
            Some(_) => {}
            None => return,
        }
        if step(ctx, op, 18).is_none() {
            return;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_dispatch_weapon_trigger(
    ctx: *mut c_void,
    op: Op,
    kind: u32,
    precheck_kind: u32,
) -> u8 {
    if step(ctx, op, 0).is_none() {
        return 0;
    }
    if step(ctx, op, 1).is_none() {
        return 0;
    }
    while step(ctx, op, 2) == Some(1) {
        let present = match step(ctx, op, 3) {
            Some(value) => value != 0,
            None => return 0,
        };
        if present {
            let accepted = match step(ctx, op, 4) {
                Some(value) => value != 0,
                None => return 0,
            };
            if kind == precheck_kind && !accepted {
                return 0;
            }
        }
        if step(ctx, op, 5).is_none() {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_dispatch_craft(ctx: *mut c_void, op: Op) {
    if step(ctx, op, 0).is_none() {
        return;
    }
    if step(ctx, op, 1).is_none() {
        return;
    }
    while step(ctx, op, 2) == Some(1) {
        match step(ctx, op, 3) {
            Some(1) => {
                if step(ctx, op, 4).is_none() {
                    return;
                }
            }
            Some(_) => {}
            None => return,
        }
        if step(ctx, op, 5).is_none() {
            return;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    struct Mock {
        calls: Vec<u32>,
        handlers: usize,
        accepted: u64,
        present: bool,
    }
    unsafe extern "C" fn callback(ctx: *mut c_void, id: u32) -> u64 {
        let mock = unsafe { &mut *ctx.cast::<Mock>() };
        mock.calls.push(id);
        match id {
            0 | 1 => 1,
            2 | 15 => {
                if mock.handlers > 0 {
                    mock.handlers -= 1;
                    1
                } else {
                    0
                }
            }
            3 | 16 => u64::from(mock.present),
            4 if mock.calls.contains(&14) => mock.accepted,
            4 | 11 => 0,
            6 => 0,
            _ => 0,
        }
    }
    #[test]
    fn original_trigger_kind_controls_veto() {
        let mut mock = Mock {
            calls: Vec::new(),
            handlers: 1,
            accepted: 0,
            present: true,
        };
        let ptr = (&raw mut mock).cast::<c_void>();
        assert_eq!(
            unsafe { a2fo_rs_stage8_core_dispatch_weapon_trigger(ptr, callback, 7, 7) },
            0
        );
        assert_eq!(mock.calls, [0, 1, 2, 3, 4]);
        mock.calls.clear();
        mock.handlers = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage8_core_dispatch_weapon_trigger(ptr, callback, 8, 7) },
            1
        );
        assert_eq!(mock.calls, [0, 1, 2, 3, 4, 5, 2]);
    }
    #[test]
    fn class_admission_precedes_field_and_handler_work() {
        let mut mock = Mock {
            calls: Vec::new(),
            handlers: 0,
            accepted: 1,
            present: false,
        };
        let ptr = (&raw mut mock).cast::<c_void>();
        unsafe { a2fo_rs_stage8_core_dispatch_weapon_class(ptr, callback) };
        assert_eq!(mock.calls, [0, 1, 2]);
    }
}
