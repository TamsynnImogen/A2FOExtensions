//! Residual bounded native-tree and canvas traversal controllers.
//! Native callbacks own checked fields, HWND/COM calls, storage and exceptions.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
/// # Safety
/// Callback/context remain live synchronously; callbacks never unwind.
/// Ops 0=node readable/non-head, 1=signed key bits, 2=value word, 3=child (0 left,1 right).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_ui_effect_tree(
    context: *mut c_void,
    step: Step,
    wanted: i32,
) -> usize {
    for _ in 0..128 {
        if unsafe { step(context, 0, 0) } == 0 {
            return 0;
        }
        let key = unsafe { step(context, 1, 0) } as u32 as i32;
        if wanted == key {
            return unsafe { step(context, 2, 0) };
        }
        unsafe {
            step(context, 3, usize::from(wanted > key));
        }
    }
    0
}
/// # Safety
/// Ops 0=lookup current, 1=parent word, 2=current word, 3=advance to parent.
/// All callbacks retain native references for the duration of each operation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_ui_descendant(
    context: *mut c_void,
    step: Step,
    parent: usize,
) -> u8 {
    for _ in 0..64 {
        if unsafe { step(context, 0, 0) } == 0 {
            return 0;
        }
        let next = unsafe { step(context, 1, 0) };
        if next == 0 || next == unsafe { step(context, 2, 0) } {
            return 0;
        }
        if next == parent {
            return 1;
        }
        unsafe {
            step(context, 3, 0);
        }
    }
    0
}
/// # Safety
/// Ops 0=source scale valid,1=scale point,2=native MapWindowPoints,
/// 3=target scale valid,4=inverse scale point. Points stay native/writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_ui_map_points(
    context: *mut c_void,
    step: Step,
    count: usize,
) {
    if unsafe { step(context, 0, 0) } != 0 {
        for index in 0..count {
            unsafe {
                step(context, 1, index);
            }
        }
    }
    unsafe {
        step(context, 2, 0);
    }
    if unsafe { step(context, 3, 0) } != 0 {
        for index in 0..count {
            unsafe {
                step(context, 4, index);
            }
        }
    }
}
/// # Safety
/// Native reverse iterator: 0=not end,1=logical match,2=scoped identity match,
/// 3=selected HWND word,4=advance. Predicate errors return usize::MAX and stop.
/// The selected HWND is raw data and may use any word value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_ui_lease(context: *mut c_void, step: Step) -> usize {
    while unsafe { step(context, 0, 0) } != 0 {
        match unsafe { step(context, 1, 0) } {
            usize::MAX => return 0,
            1 => match unsafe { step(context, 2, 0) } {
                usize::MAX => return 0,
                1 => return unsafe { step(context, 3, 0) },
                _ => {}
            },
            _ => {}
        }
        unsafe {
            step(context, 4, 0);
        }
    }
    0
}
#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;
    struct Trace {
        calls: Vec<(u32, usize)>,
        index: usize,
        match_at: usize,
    }
    unsafe extern "C" fn tree(p: *mut c_void, op: u32, arg: usize) -> usize {
        let c = unsafe { &mut *p.cast::<Trace>() };
        c.calls.push((op, arg));
        match op {
            0 => 1,
            1 => {
                if c.index == c.match_at {
                    (-3i32) as u32 as usize
                } else {
                    5
                }
            }
            2 => usize::MAX,
            3 => {
                c.index += 1;
                0
            }
            _ => 0,
        }
    }
    #[test]
    fn tree_preserves_signed_branch_and_raw_result() {
        let mut c = Trace {
            calls: Vec::new(),
            index: 0,
            match_at: 1,
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_ui_effect_tree((&mut c as *mut Trace).cast(), tree, -3) },
            usize::MAX
        );
        assert_eq!(c.calls, [(0, 0), (1, 0), (3, 0), (0, 0), (1, 0), (2, 0)]);
    }
    #[test]
    fn tree_cycle_stops_at_128_nodes() {
        let mut c = Trace {
            calls: Vec::new(),
            index: 0,
            match_at: 999,
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_ui_effect_tree((&mut c as *mut Trace).cast(), tree, -3) },
            0
        );
        assert_eq!(c.index, 128);
    }
    unsafe extern "C" fn points(p: *mut c_void, op: u32, arg: usize) -> usize {
        let c = unsafe { &mut *p.cast::<Trace>() };
        c.calls.push((op, arg));
        1
    }
    #[test]
    fn mapping_orders_both_passes_around_native_call() {
        let mut c = Trace {
            calls: Vec::new(),
            index: 0,
            match_at: 0,
        };
        unsafe { a2fo_rs_stage10_ui_map_points((&mut c as *mut Trace).cast(), points, 2) };
        assert_eq!(
            c.calls,
            [(0, 0), (1, 0), (1, 1), (2, 0), (3, 0), (4, 0), (4, 1)]
        );
    }
    unsafe extern "C" fn ancestry(p: *mut c_void, op: u32, arg: usize) -> usize {
        let c = unsafe { &mut *p.cast::<Trace>() };
        c.calls.push((op, arg));
        match op {
            0 => 1,
            1 => c.index + 1,
            2 => c.index,
            3 => {
                c.index += 1;
                0
            }
            _ => 0,
        }
    }
    #[test]
    fn ancestor_bound_does_not_probe_65th_record() {
        let mut c = Trace {
            calls: Vec::new(),
            index: 0,
            match_at: 0,
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_ui_descendant((&mut c as *mut Trace).cast(), ancestry, 65) },
            0
        );
        assert_eq!(c.index, 64);
    }
    unsafe extern "C" fn lease(p: *mut c_void, op: u32, arg: usize) -> usize {
        let c = unsafe { &mut *p.cast::<Trace>() };
        c.calls.push((op, arg));
        match op {
            0 => 1,
            1 => 1,
            2 => usize::MAX,
            4 => {
                c.index += 1;
                0
            }
            _ => 0,
        }
    }
    #[test]
    fn lease_error_stops_before_advance_or_result() {
        let mut c = Trace {
            calls: Vec::new(),
            index: 0,
            match_at: 0,
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_ui_lease((&mut c as *mut Trace).cast(), lease) },
            0
        );
        assert_eq!(c.calls, [(0, 0), (1, 0), (2, 0)]);
    }
}
