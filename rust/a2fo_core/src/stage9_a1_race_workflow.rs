//! A1 race-loaded admission and presentation workflow, with native STL frames.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32) -> u64;
const ERROR: u64 = u64::MAX;
/// # Safety
/// Callback/context remain live synchronously. Each op performs the documented
/// native leaf and catches exceptions without retaining Rust state. Op16 owns
/// record, input-vector and plan scopes and returns ERROR on unwinding. All
/// mutable native fields are read by the callback at the requested operation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_loaded(context: *mut c_void, step: Step) {
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op) };
            if value == ERROR {
                return;
            }
            value
        }};
    }
    let declared = call!(0) as u32 as i32;
    if !(1..=4096).contains(&declared) {
        call!(1);
        return;
    }
    let race = call!(2) as u32 as i32;
    if race < 0 || race >= declared {
        return;
    }
    if call!(3) != 0 {
        let applied = call!(4) != 0;
        call!(if applied { 5 } else { 6 });
        if !applied {
            call!(7);
        }
    }
    let synthetic = call!(8) as u32 as i32;
    if race != synthetic && call!(9) == 0 {
        call!(10);
    }
    if call!(11) == 0 && race == 0 {
        call!(12);
    }
    if race as usize != call!(13) as usize {
        call!(14);
        return;
    }
    if call!(11) != 0 {
        call!(15);
    }
    call!(16);
}
/// # Safety
/// Native record storage stays live in its original C++ scope. Operations0..5
/// perform ordered native field assignments and return ERROR after capturing a
/// C++ exception. Rust stops before another native operation on that path.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_record(context: *mut c_void, step: Step) {
    for op in 0..6 {
        if unsafe { step(context, op) } == ERROR {
            return;
        }
    }
}
/// # Safety
/// Native owns a live const plan and input-vector/record frames. Callback ops
/// perform checked native writes or presentation logs, catch exceptions and
/// reread mutable registry state at the requested operation. ERROR stops work.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_race_present(context: *mut c_void, step: Step) {
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op) };
            if value == ERROR {
                return;
            }
            value
        }};
    }
    if call!(0) != 0 {
        if call!(1) == 0 && call!(2) == ERROR {
            return;
        }
        if call!(3) == ERROR {
            return;
        }
    }
    if call!(4) != 0 {
        call!(5);
    }
}
