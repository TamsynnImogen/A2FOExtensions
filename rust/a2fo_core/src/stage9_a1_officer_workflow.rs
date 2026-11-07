use core::ffi::c_void;

/// Synchronous native operation. Queue ops: 0=head, 1=classify current item,
/// 2=read current item's next. Event ops: 0=classify current target,
/// 1=read current completion flag, 2=admit current producer/target,
/// 3=suppress current producer/target effect, 4=clear current producer credit.
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

pub const ADMIT: u32 = 0;
pub const FINISHED: u32 = 1;
pub const DESTROYING: u32 = 2;
pub const FINISHING: u32 = 3;
pub const STARTING_EFFECT: u32 = 4;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SanitizedSave {
    pub completed: u32,
    pub gain: i32,
}

/// # Safety
/// The native callback and context must remain live during this call. Each
/// callback validates its own graph reads, catches C++ exceptions, and cannot
/// retain Rust state. Producer null validation remains with the native caller.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_workflow_queue(
    context: *mut c_void,
    step: Option<Step>,
) -> u32 {
    let Some(step) = step else { return 0 };
    let mut item = unsafe { step(context, 0, 0) };
    let mut count = 0u32;
    for _ in 0..10 {
        if item == 0 {
            break;
        }
        if unsafe { step(context, 1, item) } != 0 {
            count = count.wrapping_add(1);
        }
        // The native classification may mutate this link. Read it afterward.
        item = unsafe { step(context, 2, item) };
    }
    count
}

/// # Safety
/// Native validates the event header, producer, readiness, and event kind
/// before calling. The synchronous callback rereads mutable native event
/// fields at each operation; it catches all C++ exceptions before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_workflow_event(
    kind: u32,
    context: *mut c_void,
    step: Option<Step>,
) -> u8 {
    let Some(step) = step else { return 1 };
    match kind {
        ADMIT => {
            if unsafe { step(context, 0, 0) } != 0 && unsafe { step(context, 1, 0) } == 0 {
                return 0;
            }
            u8::from(unsafe { step(context, 2, 0) } != 0)
        }
        STARTING_EFFECT => u8::from(unsafe { step(context, 3, 0) } == 0),
        DESTROYING => {
            unsafe { step(context, 4, 0) };
            1
        }
        FINISHED | FINISHING => 1,
        _ => 1,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_workflow_save_valid(
    loaded: u8,
    magic: u32,
    version: u32,
    expected_magic: u32,
    expected_version: u32,
) -> u8 {
    u8::from(loaded != 0 && magic == expected_magic && version == expected_version)
}

/// # Safety
/// `output` is private writable caller-owned POD, disjoint from all native
/// graph storage. No callback occurs while it is borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_workflow_sanitize(
    completed: u32,
    gain: i32,
    maximum: i32,
    output: *mut SanitizedSave,
) {
    if let Some(output) = unsafe { output.as_mut() } {
        *output = SanitizedSave {
            completed: completed.min(maximum as u32),
            gain: gain.max(0),
        };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_workflow_retained_policy(
    has_maximum: u8,
    has_gain: u8,
    race_nonempty: u8,
) -> u8 {
    u8::from(has_maximum != 0 || has_gain != 0 || race_nonempty != 0)
}

#[cfg(test)]
mod tests {
    include!("stage9_a1_officer_workflow_tests.rs");
}
