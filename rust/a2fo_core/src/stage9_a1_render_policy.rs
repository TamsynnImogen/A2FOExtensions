//! A1 selection routing and ordered tile iteration. Native owns render calls.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
/// # Safety
/// Callbacks capture native tile/object values and render synchronously without
/// unwinding. Output is a private count, never accessed by callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_render_energy(
    selected: u8,
    capacity: usize,
    context: *mut c_void,
    step: Step,
    drawn: *mut usize,
) -> u8 {
    let mut count = 0usize;
    if selected != 0 {
        count = usize::from(unsafe { step(context, 1, 0) } != 0);
    } else if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 2, 0) };
        let mut active = 0usize;
        for index in 0..capacity {
            if unsafe { step(context, 3, index) } == 0 {
                continue;
            }
            active += 1;
            if unsafe { step(context, 4, index) } != 0 {
                count += 1;
            }
        }
        // Native draws happen before this reporting admission check.
        if active < 2 {
            unsafe { drawn.write(count) };
            return 0;
        }
    }
    unsafe { drawn.write(count) };
    1
}
/// # Safety
/// The callback owns checked tile reads and native array captures; it returns
/// nonzero only for a live tile/object. No pointer into those arrays is borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_render_collect(
    capacity: usize,
    context: *mut c_void,
    step: Step,
) -> usize {
    let mut active = 0;
    for index in 0..capacity {
        if unsafe { step(context, 0, index) } != 0 {
            active += 1;
        }
    }
    active
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_render_multi_admit(active: usize) -> u8 {
    u8::from(active >= 2)
}
/// # Safety
/// Each callback performs native binding/render adaptation for its current
/// slot. It owns all validation, object snapshots and render function addresses.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_render_visit(
    capacity: usize,
    context: *mut c_void,
    step: Step,
) {
    for index in 0..capacity {
        unsafe { step(context, 0, index) };
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[derive(Default)]
    struct C {
        trace: Vec<(u32, usize)>,
        only_one: bool,
    }
    unsafe extern "C" fn step(p: *mut c_void, op: u32, i: usize) -> usize {
        let c = unsafe { &mut *p.cast::<C>() };
        c.trace.push((op, i));
        match op {
            0 | 1 => 1,
            3 => usize::from(i == 0 || (!c.only_one && i == 2)),
            4 => 1,
            _ => 0,
        }
    }
    #[test]
    fn multi_reports_only_after_ordered_draws() {
        let mut c = C::default();
        let mut count = 0;
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_render_energy(0, 3, (&mut c as *mut C).cast(), step, &mut count)
            },
            1
        );
        assert_eq!(count, 2);
        assert_eq!(
            c.trace,
            [(0, 0), (2, 0), (3, 0), (4, 0), (3, 1), (3, 2), (4, 2)]
        );
    }
    #[test]
    fn single_does_not_read_layout_and_one_multi_still_draws() {
        let mut c = C::default();
        let mut count = 0;
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_render_energy(1, 3, (&mut c as *mut C).cast(), step, &mut count)
            },
            1
        );
        assert_eq!(c.trace, [(1, 0)]);
        c.trace.clear();
        c.only_one = true;
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_render_energy(0, 3, (&mut c as *mut C).cast(), step, &mut count)
            },
            0
        );
        assert_eq!(count, 1);
        assert_eq!(c.trace.last(), Some(&(3, 2)));
    }
}
