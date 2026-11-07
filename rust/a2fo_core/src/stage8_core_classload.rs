//! Class ODF capture, alias policy, snapshot publication and event traversal.
//! Native callbacks own engine ABI, maps, strings, locks and exception scopes.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u64;
/// Safety: only pointer identity is examined; callers retain all buffers.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_core_classload_admit(
    key: *const u8,
    output: *mut u8,
    size: u32,
) -> u8 {
    u8::from(!key.is_null() && !output.is_null() && size != 0)
}
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
    match kind {
        0 => {
            if op!(0) == 0 || op!(1) == 0 {
                return Some(0);
            }
            op!(2);
            op!(3);
            let count = op!(4) as usize;
            for index in 0..count {
                op!(5, index);
                if op!(6) != 0 {
                    continue;
                }
                op!(7);
                if op!(8) != 0 {
                    op!(9);
                }
                op!(10);
            }
            Some(0)
        }
        1 => {
            if op!(0) == 0 {
                if op!(1) == 0 && op!(2) != 0 {
                    return Some(1);
                }
                return Some(op!(1));
            }
            op!(3);
            op!(4);
            if op!(5) == 0 {
                return Some(op!(1));
            }
            op!(6);
            op!(7);
            op!(8);
            op!(9);
            if op!(10) != 0 {
                op!(11);
            }
            op!(12);
            if op!(13) != 0 && op!(14) != 0 {
                op!(15);
                op!(16);
            }
            Some(op!(1))
        }
        2 => {
            op!(0);
            op!(1);
            op!(2);
            if op!(3) == 0 || op!(4) == 0 {
                return Some(0);
            }
            op!(5);
            if op!(6) == 0 {
                return Some(0);
            }
            op!(7);
            if op!(8) == 0 {
                return Some(0);
            }
            op!(9);
            if op!(10) == 0 {
                return Some(0);
            }
            op!(11);
            op!(12);
            while op!(13) != 0 {
                if op!(14) == 0 {
                    op!(15);
                }
                op!(16);
            }
            op!(17);
            op!(18);
            op!(19);
            op!(20);
            if op!(21) != 0 {
                op!(22);
                while op!(23) != 0 {
                    op!(24);
                }
                op!(25);
                op!(26);
                while op!(27) != 0 {
                    if op!(28) != 0 {
                        op!(29);
                    }
                    op!(30);
                }
                op!(31); // event/views die before the final native log
            }
            op!(32);
            Some(0)
        }
        _ => Some(0),
    }
}
/// Safety: callbacks contain native exceptions; no native reference enters Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_classload(
    kind: u32,
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    unsafe { run(kind, step, ctx) }.unwrap_or(0)
}
