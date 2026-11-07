//! Hybrid ODF admission and explicit-list traversal. Native callbacks own
//! ParameterDB ABI, CRT comparison, per-row string lifetimes and storage.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;
unsafe fn run(kind: u32, step: Step, ctx: *mut c_void) -> Option<u64> {
    macro_rules! op {
        ($id:expr) => {
            op!($id, 0, 0)
        };
        ($id:expr,$a:expr,$b:expr) => {{
            let value = unsafe { step(ctx, $id, $a, $b) };
            if value == u64::MAX {
                return None;
            }
            value
        }};
    }
    match kind {
        0 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 || op!(3) == 0 || op!(4) == 0 {
                if op!(5) == 0 {
                    op!(6);
                    op!(7);
                }
                return Some(0);
            }
            if op!(8) == 0 {
                return Some(0);
            }
            let hybrid = op!(9) != 0;
            if hybrid && op!(10) == 0 {
                op!(11);
                op!(12);
            }
            Some(u64::from(hybrid))
        }
        1 => {
            for method in 0..4 {
                let found = op!(0, method, 0) != 0;
                op!(1); // destroy the ignored row before next lookup or return
                if found {
                    return Some(1);
                }
            }
            Some(0)
        }
        2 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 {
                return Some(0);
            }
            op!(3); // allocate native list only after the cheap four-key gate
            let capacity = op!(4) as usize;
            let mut counts = [0usize; 4];
            for method in [0usize, 1, 2, 3] {
                for index in 0..capacity {
                    op!(5, method, index); // begin native key/target lifetime
                    if op!(6) != 0 {
                        if op!(7) != 0 {
                            op!(8);
                        } else if op!(9) == 0 || op!(10) == 0 {
                            op!(11);
                        } else {
                            op!(12);
                            if op!(13) != 0 {
                                counts[method] += 1;
                            } else if op!(14) != 0 {
                                op!(15);
                            } else {
                                op!(16);
                            }
                        }
                    }
                    op!(17); // end row even when a valid lookup is rejected
                }
            }
            if op!(18) != 0 {
                op!(19);
                return Some(0);
            }
            op!(20); // native locked publication retains its try/unlock policy
            for (index, count) in counts.into_iter().enumerate() {
                op!(21, index, count);
            }
            op!(22); // original native CRT summary formatting and log
            Some(0)
        }
        _ => Some(0),
    }
}
/// Safety: callbacks own all borrowed objects and contain native exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_hybrid_lists(
    kind: u32,
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    unsafe { run(kind, step, ctx) }.unwrap_or(0)
}
