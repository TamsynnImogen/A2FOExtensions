//! FOFS dispatch publication and single-owner registration decisions.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32) -> u64;
unsafe fn run(kind: u32, step: Step, ctx: *mut c_void) -> Option<u64> {
    macro_rules! op {
        ($id:expr) => {{
            let value = unsafe { step(ctx, $id) };
            if value == u64::MAX {
                return None;
            }
            value
        }};
    }
    if kind == 0 {
        if op!(0) == 0 {
            op!(1);
        }
        if op!(2) != 0 {
            op!(3);
        } else {
            op!(4);
        }
        if op!(5) != 0 && op!(6) != 0 {
            return Some(op!(7));
        }
        return Some(op!(8));
    }
    if kind != 1 && kind != 2 {
        return Some(0);
    }
    if op!(0) == 0 || op!(1) == 0 {
        return Some(0);
    }
    if kind == 1 && (op!(14) == 0 || op!(16) == 0) {
        op!(15);
    } else {
        op!(2);
    }
    op!(3);
    let registered = op!(4) != 0 && op!(5) != 0 && op!(6) != 0;
    if registered {
        op!(7);
        op!(8);
        op!(9);
    }
    op!(10); // copy owner under the original lock
    op!(11); // original manual/RAII release occurs before either log
    op!(if registered { 12 } else { 13 });
    Some(u64::from(registered))
}
/// Safety: callbacks preserve native locks, acquire loads, handler ABI and exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_lookup(
    kind: u32,
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    unsafe { run(kind, step, ctx) }.unwrap_or(0)
}
