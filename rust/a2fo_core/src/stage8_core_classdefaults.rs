//! Classlabel default lookup and typed application. Native callbacks retain
//! locks, STL storage, CRT conversions and exception ownership.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32) -> u64;
unsafe fn run(kind: u32, step: Step, ctx: *mut c_void) -> Option<u8> {
    macro_rules! op {
        ($id:expr) => {{
            let value = unsafe { step(ctx, $id) };
            if value == u64::MAX {
                return None;
            }
            value
        }};
    }
    match kind {
        0 => {
            if op!(0) == 0 || op!(1) == 0 || op!(2) == 0 {
                return Some(0);
            }
            op!(3); // lock only after validating all caller inputs
            if op!(4) == 0 || op!(5) == 0 || op!(6) == 0 {
                return Some(0);
            }
            op!(7); // original classlabel assignment precedes value assignment
            op!(8);
        }
        1..=4 => {
            if op!(0) == 0 || (kind == 4 && op!(1) == 0) {
                return Some(0);
            }
            if op!(2) == 0 {
                return Some(0);
            }
            if kind == 4 && op!(3) == 0 {
                return Some(0);
            }
            if kind == 3 {
                op!(4); // native CRT-compatible lowercase string
                let truth = op!(5) != 0 || op!(6) != 0 || op!(7) != 0 || op!(8) != 0;
                op!(if truth { 9 } else { 10 });
            } else {
                op!(4); // native atoi / strtof / memcpy
            }
            op!(11); // log only after writing the caller's output
        }
        _ => return Some(0),
    }
    Some(1)
}
/// Safety: synchronous callbacks own native storage and contain all exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_classdefault(
    kind: u32,
    step: Step,
    ctx: *mut c_void,
) -> u8 {
    unsafe { run(kind, step, ctx) }.unwrap_or(0)
}
