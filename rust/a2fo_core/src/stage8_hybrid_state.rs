//! Hybrid live queue/class classification. All native addresses, read probes,
//! storage, and signed-field loads remain in synchronous C++ callbacks.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u64;
unsafe fn run(kind: u32, step: Step, ctx: *mut c_void) -> Option<u64> {
    macro_rules! op {
        ($id:expr) => {
            op!($id, 0)
        };
        ($id:expr,$a:expr) => {{
            let value = unsafe { step(ctx, $id, $a) };
            if value == u64::MAX {
                return None;
            }
            value
        }};
    }
    let result = match kind {
        0 => {
            if op!(0) == 0 || op!(1) == 0 {
                return Some(0);
            }
            op!(2);
            if op!(3) == 0 {
                return Some(0);
            }
            op!(4);
            if op!(5) == 0 {
                op!(6);
                if op!(7) != 0 {
                    op!(8);
                }
            }
            op!(9) != 0 && op!(10) != 0
        }
        1 => op!(0) != 0 && op!(1) != 0,
        2 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 {
                return Some(0);
            }
            op!(3);
            if op!(4) != 0 {
                return Some(1);
            }
            op!(5);
            for _ in 0..op!(6) {
                if op!(7) == 0 || op!(8) == 0 {
                    break;
                }
                op!(9);
                if op!(10) != 0 {
                    return Some(1);
                }
                op!(11);
            }
            false
        }
        3 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 {
                return Some(0);
            }
            for index in 0..op!(3) as usize {
                if op!(4, index) != 0 {
                    return Some(1);
                }
            }
            false
        }
        4 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 {
                return Some(0);
            }
            op!(3);
            op!(4);
            if op!(5) == 0 || op!(6) == 0 {
                return Some(0);
            }
            if op!(7) != 0 && op!(8) != 0 {
                return Some(u64::from(op!(9) != 0));
            }
            let count = op!(10) as usize;
            for index in 0..count {
                if op!(11, index) != 0 {
                    return Some(0);
                }
            }
            for index in 0..count {
                if op!(12, index) != 0 {
                    return Some(1);
                }
            }
            if op!(13) != 0 {
                return Some(1);
            }
            op!(14);
            if op!(15) == 0 {
                return Some(0);
            }
            op!(16);
            op!(17) != 0 && op!(18) != 0
        }
        5 => {
            if op!(0) == 0 || op!(1) == 0 {
                return Some(0);
            }
            if op!(2) != 0 {
                return Some(1);
            }
            if op!(3) == 0 || op!(4) == 0 {
                return Some(0);
            }
            let existing_pod = op!(5);
            let target_pod = op!(6);
            if existing_pod == 0 || target_pod == 0 {
                return Some(0);
            }
            let existing_family = op!(7);
            let target_family = op!(8);
            if existing_family != target_family {
                return Some(0);
            }
            let existing_level = op!(9) as u32 as i32;
            let target_level = op!(10) as u32 as i32;
            target_level < existing_level
        }
        6 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 {
                return Some(0);
            }
            op!(3);
            op!(4);
            let count = op!(5);
            op!(6) != 0 && op!(7) != 0 && op!(8) == 0 && count < op!(9)
        }
        7 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 || op!(3) == 0 {
                return Some(0);
            }
            op!(4);
            op!(5);
            let count = op!(6);
            let active = op!(7);
            op!(8) != 0 && op!(9) != 0 && (active != 0 || count != 0)
        }
        8 => {
            if op!(0) == 0 {
                return Some(0);
            }
            op!(1);
            let count = op!(2) as u32 as i32;
            if count != 1 || op!(3) == 0 || op!(4) == 0 {
                return Some(0);
            }
            op!(5);
            op!(6);
            op!(7);
            if op!(8) == 0 || op!(9) == 0 || (op!(10) == 0 && op!(11) == 0) {
                return Some(0);
            }
            return Some(op!(12));
        }
        _ => false,
    };
    Some(u64::from(result))
}
/// Safety: native callbacks contain all exceptions and retain native ABI/layout.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_hybrid_state(
    kind: u32,
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    unsafe { run(kind, step, ctx) }.unwrap_or(0)
}
