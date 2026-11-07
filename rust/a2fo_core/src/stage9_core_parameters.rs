//! ParameterDB fallback sequencing. Native callbacks retain the foreign ABI,
//! output allocations, policy string lifetimes and original exception scopes.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32) -> u64;
const FAILED: u64 = u64::MAX;

fn invoke(step: Step, context: *mut c_void, operation: u32) -> Option<u64> {
    let value = unsafe { step(context, operation) };
    (value != FAILED).then_some(value)
}

/// # Safety
/// The synchronous native callback contains every C++ exception. Operation 4
/// ends policy-local storage before operation 5 logs a policy exception;
/// operation 5 releases that exception before the caller-default retry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_parameter_get(
    kind: u32,
    step: Step,
    context: *mut c_void,
) -> u8 {
    if kind > 1 {
        return 0;
    }
    let Some(found) = invoke(step, context, 0) else {
        return 0;
    };
    if found != 0 {
        return 1;
    }
    if kind == 0 {
        return match invoke(step, context, 1) {
            Some(applied) => u8::from(applied != 0),
            None => {
                let _ = invoke(step, context, 5);
                0
            }
        };
    }

    let policy = invoke(step, context, 1);
    let mut failed = policy.is_none();
    if policy.is_some_and(|value| value != 0) {
        // The native retry's bool result was deliberately ignored: the
        // original reports success after applying and logging a policy.
        if invoke(step, context, 2).is_some() && invoke(step, context, 3).is_some() {
            let _ = invoke(step, context, 4);
            return 1;
        }
        failed = true;
    }
    if invoke(step, context, 4).is_none() {
        return 0;
    }
    if failed && invoke(step, context, 5).is_none() {
        return 0;
    }
    invoke(step, context, 6).map_or(0, |value| u8::from(value != 0))
}
