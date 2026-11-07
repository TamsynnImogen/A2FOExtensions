//! Portable residual SOL source and presentation decisions.
use core::ffi::c_void;

pub type Step = unsafe extern "C" fn(*mut c_void, u32) -> i32;
pub type ReadSlot = unsafe extern "C" fn(*mut c_void) -> i32;

/// The native callback owns root strings and directory enumeration. A negative
/// result means its native exception was captured and must be rethrown there.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_root_directories(
    context: *mut c_void,
    step: Step,
) -> bool {
    for directory in 0..2 {
        // SAFETY: Native caller supplies a live non-unwinding callback.
        if unsafe { step(context, directory) } < 0 {
            return false;
        }
    }
    true
}

/// Native operation 0 checks the extension; operation 1 replaces the winner.
/// The basename remains native-owned across both synchronous callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_index_entry(context: *mut c_void, step: Step) -> bool {
    // SAFETY: Native caller supplies a live non-unwinding callback.
    let accepted = unsafe { step(context, 0) };
    if accepted <= 0 {
        return false;
    }
    // SAFETY: Native caller owns the map update and captures exceptions.
    (unsafe { step(context, 1) }) >= 0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_file_exists(
    attributes: u32,
    invalid_attributes: u32,
    directory_flag: u32,
) -> bool {
    attributes != invalid_attributes && (attributes & directory_flag) == 0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_title_filename(title_size: usize) -> bool {
    title_size == 0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_type_empty(type_size: usize) -> bool {
    type_size == 0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_start_count(
    slot_context: *mut c_void,
    slot_present: bool,
    read_slot: ReadSlot,
    starts: usize,
) -> u32 {
    let mut allocated = 8usize;
    if slot_present {
        // Preserve the native short-circuit's distinct live reads: >0,
        // <=64, and assignment. The native callback owns the raw slot.
        if unsafe { read_slot(slot_context) } > 0 && unsafe { read_slot(slot_context) } <= 64 {
            allocated = unsafe { read_slot(slot_context) } as usize;
        }
    }
    starts.min(allocated) as u32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_nonnegative_counter(value: i32) -> u32 {
    value.max(0) as u32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_count_size(value: usize) -> u32 {
    value as u32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_increment_counter(value: u32) -> u32 {
    value.wrapping_add(1)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_sum_failures(missing: u32, construct: u32) -> u32 {
    missing.wrapping_add(construct)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_log_first16(count: u32) -> bool {
    count <= 16
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_log_index32(index: usize) -> bool {
    index < 32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_label_odf(instance_size: usize) -> bool {
    instance_size == 0
}

/// Safety: failed points to the live native u32 counter whenever added is false.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_record_mission_add(added: bool, failed: *mut u32) {
    if !added {
        unsafe {
            failed.write(failed.read().wrapping_add(1));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::vec;
    use std::vec::Vec;

    struct Calls {
        seen: Vec<u32>,
        fail_at: Option<u32>,
        accepted: bool,
    }
    unsafe extern "C" fn step(raw: *mut c_void, operation: u32) -> i32 {
        // SAFETY: Every test passes a live Calls instance.
        let calls = unsafe { &mut *raw.cast::<Calls>() };
        calls.seen.push(operation);
        if calls.fail_at == Some(operation) {
            -1
        } else if operation == 0 {
            i32::from(calls.accepted)
        } else {
            0
        }
    }
    #[test]
    fn directory_order_and_failure_stop() {
        let mut calls = Calls {
            seen: vec![],
            fail_at: None,
            accepted: true,
        };
        // SAFETY: Live context and non-unwinding callback.
        assert!(unsafe {
            a2fo_rs_stage9_sol_root_directories((&mut calls as *mut Calls).cast(), step)
        });
        assert_eq!(calls.seen, vec![0, 1]);
        calls.seen.clear();
        calls.fail_at = Some(0);
        assert!(!unsafe {
            a2fo_rs_stage9_sol_root_directories((&mut calls as *mut Calls).cast(), step)
        });
        assert_eq!(calls.seen, vec![0]);
    }
    #[test]
    fn admission_replaces_only_accepted_entry() {
        let mut calls = Calls {
            seen: vec![],
            fail_at: None,
            accepted: false,
        };
        assert!(!unsafe {
            a2fo_rs_stage9_sol_index_entry((&mut calls as *mut Calls).cast(), step)
        });
        assert_eq!(calls.seen, vec![0]);
        calls.seen.clear();
        calls.accepted = true;
        assert!(unsafe { a2fo_rs_stage9_sol_index_entry((&mut calls as *mut Calls).cast(), step) });
        assert_eq!(calls.seen, vec![0, 1]);
        calls.seen.clear();
        calls.fail_at = Some(1);
        assert!(!unsafe {
            a2fo_rs_stage9_sol_index_entry((&mut calls as *mut Calls).cast(), step)
        });
        assert_eq!(calls.seen, vec![0, 1]);
    }
    #[test]
    fn slot_fallback_and_clamp() {
        unsafe extern "C" fn read(raw: *mut c_void) -> i32 {
            unsafe { *raw.cast::<i32>() }
        }
        for (slots, expected) in [(-1, 8), (0, 8), (1, 1), (8, 8), (64, 64), (65, 8)] {
            let mut slot = slots;
            let ptr = (&mut slot as *mut i32).cast();
            assert_eq!(
                unsafe { a2fo_rs_stage9_sol_start_count(ptr, true, read, 100) },
                expected
            );
            assert_eq!(
                unsafe { a2fo_rs_stage9_sol_start_count(ptr, true, read, 0) },
                0
            );
            assert_eq!(
                unsafe { a2fo_rs_stage9_sol_start_count(ptr, true, read, 3) },
                expected.min(3)
            );
        }
        let mut slot = 64;
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_sol_start_count((&mut slot as *mut i32).cast(), false, read, 100)
            },
            8
        );
    }
    #[test]
    fn presentation_and_summary_edges() {
        assert!(a2fo_rs_stage9_sol_file_exists(0, u32::MAX, 0x10));
        assert!(!a2fo_rs_stage9_sol_file_exists(u32::MAX, u32::MAX, 0x10));
        assert!(!a2fo_rs_stage9_sol_file_exists(0x10, u32::MAX, 0x10));
        assert!(a2fo_rs_stage9_sol_title_filename(0));
        assert!(!a2fo_rs_stage9_sol_title_filename(5));
        assert!(a2fo_rs_stage9_sol_type_empty(0));
        assert!(a2fo_rs_stage9_sol_label_odf(0));
        assert_eq!(a2fo_rs_stage9_sol_nonnegative_counter(-1), 0);
        assert_eq!(a2fo_rs_stage9_sol_nonnegative_counter(12), 12);
        assert_eq!(a2fo_rs_stage9_sol_count_size(9), 9);
        assert_eq!(a2fo_rs_stage9_sol_increment_counter(u32::MAX), 0);
        assert_eq!(a2fo_rs_stage9_sol_sum_failures(u32::MAX, 1), 0);
        assert!(a2fo_rs_stage9_sol_log_first16(16));
        assert!(!a2fo_rs_stage9_sol_log_first16(17));
        assert!(a2fo_rs_stage9_sol_log_index32(31));
        assert!(!a2fo_rs_stage9_sol_log_index32(32));
    }
}
