//! Selection policy over synchronous, checked native UI operations.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> usize;
/// # Safety
/// Native owns captured buttons and all checked reads/calls. Each callback is
/// synchronous and cannot unwind. No native memory is borrowed by this driver.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_native_ui_scan(
    mode: u8,
    capacity: usize,
    limit: usize,
    ctx: *mut c_void,
    step: Step,
) -> usize {
    let mut completed = 0usize;
    for index in 0..capacity {
        if mode == 2 && completed >= limit {
            break;
        }
        unsafe { step(ctx, 0, index, 0) };
        let admitted = match mode {
            0 => (unsafe { step(ctx, 1, 0, 0) }) != 0,
            1 => {
                (unsafe { step(ctx, 3, 0, 0) }) != 0
                    && unsafe { step(ctx, 4, 0, 0) } == 1
                    && unsafe { step(ctx, 5, 0, 0) } != 0
                    && unsafe { step(ctx, 6, 0, 0) } != 0
            }
            2 => (unsafe { step(ctx, 7, 0, 0) }) != 0,
            3 => (unsafe { step(ctx, 9, 0, 0) }) != 0,
            _ => return 0,
        };
        if !admitted {
            continue;
        }
        if mode <= 1 {
            unsafe { step(ctx, 2, 0, 0) };
            completed += 1;
        } else {
            let written = unsafe { step(ctx, 8, completed, 0) } != 0;
            if mode == 3 {
                return if written { index + 1 } else { usize::MAX };
            }
            if written {
                completed += 1;
            }
        }
    }
    if mode == 3 { 0 } else { completed }
}
/// # Safety
/// Native context owns action/mode captures and output slots. Checked copy and
/// output assignment happen together in op5 only after successful validation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_native_ui_action(ctx: *mut c_void, step: Step) -> u8 {
    if unsafe { step(ctx, 0, 0, 0) } == 0 {
        if unsafe { step(ctx, 1, 0, 0) } == 0 || unsafe { step(ctx, 2, 0, 0) } != 2 {
            return 0;
        }
        if unsafe { step(ctx, 3, 0, 0) } == 0 {
            return 0;
        }
    }
    if unsafe { step(ctx, 4, 0, 0) } == 0 {
        return 0;
    }
    unsafe { step(ctx, 5, 0, 0) };
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_native_ui_transport(command: i32) -> u8 {
    u8::from(command == 12 || command == 13)
}
/// # Safety
/// Op0 reads ModeInfo and returns its presence; op1 reads its captured type.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_native_ui_special(
    command: i32,
    ctx: *mut c_void,
    step: Step,
) -> u8 {
    if a2fo_rs_stage9_a1_native_ui_transport(command) != 0 {
        return 0;
    }
    if unsafe { step(ctx, 0, 0, 0) } != 0 && unsafe { step(ctx, 1, 0, 0) } == 2 {
        return 1;
    }
    u8::from(command == 6)
}
/// # Safety
/// Op0 and op1 implement native class plausibility and classification for the
/// same previously captured class. Exact native vtable bypasses both calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_native_ui_rig(
    present: u8,
    exact: u8,
    ctx: *mut c_void,
    step: Step,
) -> u8 {
    u8::from(
        present != 0
            && (exact != 0
                || (unsafe { step(ctx, 0, 0, 0) } != 0 && unsafe { step(ctx, 1, 0, 0) } != 0)),
    )
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_native_ui_lookup(target_present: u8, id: u32) -> u8 {
    u8::from(target_present == 0 && id != 0)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_native_ui_target(present: u8, same_rig: u8) -> u8 {
    u8::from(present != 0 && same_rig == 0)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_native_ui_scout_id(id: i32) -> u8 {
    u8::from((0..57).contains(&id))
}
#[cfg(test)]
mod tests {
    use super::*;
    #[derive(Default)]
    struct C {
        trace: Vec<(u32, usize)>,
        slot: usize,
        mutate: bool,
        failed: bool,
    }
    unsafe extern "C" fn step(p: *mut c_void, op: u32, a: usize, _: usize) -> usize {
        let c = unsafe { &mut *p.cast::<C>() };
        c.trace.push((op, a));
        match op {
            0 => {
                c.slot = a;
                1
            }
            1 => usize::from(c.slot < 2),
            2 => {
                c.mutate = true;
                0
            }
            3 => 1,
            4 => 1,
            5 => 1,
            6 => usize::from(c.slot == 0 || c.mutate),
            7 | 9 => usize::from(c.slot != 0),
            8 => {
                if !c.failed {
                    c.failed = true;
                    0
                } else {
                    1
                }
            }
            _ => 0,
        }
    }
    #[test]
    fn clears_reread_and_capture_order() {
        let mut c = C::default();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_native_ui_scan(1, 3, 0, (&mut c as *mut C).cast(), step) },
            3
        );
        assert_eq!(
            &c.trace[..7],
            &[(0, 0), (3, 0), (4, 0), (5, 0), (6, 0), (2, 0), (0, 1)]
        );
    }
    #[test]
    fn placement_consumes_only_success_and_stops_at_limit() {
        let mut c = C::default();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_native_ui_scan(2, 8, 1, (&mut c as *mut C).cast(), step) },
            1
        );
        assert_eq!(c.slot, 2);
        assert_eq!(
            c.trace
                .iter()
                .filter(|(op, _)| *op == 8)
                .map(|(_, a)| *a)
                .collect::<Vec<_>>(),
            vec![0, 0]
        );
    }
    #[test]
    fn transport_first_failure_stops() {
        let mut c = C::default();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_native_ui_scan(3, 8, 0, (&mut c as *mut C).cast(), step) },
            usize::MAX
        );
        assert_eq!(c.slot, 1);
    }
    #[test]
    fn classification_short_circuit_and_ids() {
        let mut c = C::default();
        for command in [12, 13] {
            assert_eq!(
                unsafe {
                    a2fo_rs_stage9_a1_native_ui_special(command, (&mut c as *mut C).cast(), step)
                },
                0
            )
        }
        assert!(c.trace.is_empty());
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_native_ui_rig(1, 1, (&mut c as *mut C).cast(), step) },
            1
        );
        assert!(c.trace.is_empty());
        for (id, ok) in [(-1, 0), (0, 1), (56, 1), (57, 0)] {
            assert_eq!(a2fo_rs_stage9_a1_native_ui_scout_id(id), ok)
        }
    }
}
