//! Ordered resource presentation policy. Native callbacks own pointers, STL,
//! localization calls, and exception handling; Rust only selects the path.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

const TOTAL_RESOURCES: u32 = 10;
const PRESENTATIONS: u32 = 4;
const ICON: u32 = 3;

/// # Safety
/// `context` and `step` must remain valid during the call. The callback must
/// not unwind and must return the native pointer/index values described by the
/// operation contract in `stage10_resource_presentation_ffi.h`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_resource_local_team(
    context: *mut c_void,
    step: Step,
) -> usize {
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    let index = unsafe { step(context, 1, 0) } as u32 as i32;
    if !(0..16).contains(&index) {
        return 0;
    }
    unsafe { step(context, 2, index as usize) }
}

/// # Safety
/// `context` and `step` must remain valid during the call. The callback owns
/// all native strings, catches native exceptions, and must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_resource_override(
    resource: u32,
    kind: u32,
    context: *mut c_void,
    step: Step,
) -> u8 {
    // Evaluate output existence first, matching the native guard order.
    if unsafe { step(context, 0, 0) } == 0 || resource >= TOTAL_RESOURCES || kind >= PRESENTATIONS {
        return 0;
    }
    if unsafe { step(context, 1, 0) } == 0 || unsafe { step(context, 2, 0) } != 0 {
        return 0;
    }

    let manager = unsafe { step(context, 3, 0) };
    let lookup = unsafe { step(context, 4, 0) };
    if manager != 0 && lookup != 0 {
        unsafe { step(context, 5, 0) };
    }
    // The native context starts with a null translated pointer. This read
    // still happens when the manager or lookup is absent.
    let translated = unsafe { step(context, 6, 0) } != 0 && unsafe { step(context, 7, 0) } != 0;
    if unsafe { step(context, 8, usize::from(translated)) } == 0 {
        return 0;
    }
    u8::from(unsafe { step(context, 9, 0) } != 0)
}

/// # Safety
/// `context` and `step` must remain valid during the call. Native owns the
/// fallback pointers and thread-local string, and callbacks must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_resource_localize(
    resource: u32,
    kind: u32,
    context: *mut c_void,
    step: Step,
) -> usize {
    if resource >= TOTAL_RESOURCES || kind >= PRESENTATIONS {
        return unsafe { step(context, 0, 0) };
    }
    let fallback = unsafe { step(context, 1, usize::from(kind == ICON)) };
    unsafe { step(context, 2, 0) };
    if unsafe { step(context, 3, 0) } != 0 {
        unsafe { step(context, 4, 0) }
    } else {
        fallback
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;

    #[derive(Default)]
    struct Fixture {
        calls: Vec<(u32, usize)>,
        result: [usize; 10],
        translated_after_lookup: usize,
    }

    unsafe extern "C" fn step(context: *mut c_void, op: u32, arg: usize) -> usize {
        let fixture = unsafe { &mut *context.cast::<Fixture>() };
        fixture.calls.push((op, arg));
        if op == 5 {
            fixture.result[6] = fixture.translated_after_lookup;
        }
        fixture.result[op as usize]
    }

    fn context(fixture: &mut Fixture) -> *mut c_void {
        (fixture as *mut Fixture).cast()
    }

    #[test]
    fn local_team_short_circuit_and_signed_index_boundary() {
        let mut f = Fixture::default();
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_local_team(context(&mut f), step) },
            0
        );
        assert_eq!(f.calls, [(0, 0)]);
        f.result[0] = 1;
        let mut invalid = std::vec![u32::MAX as usize, 16];
        if usize::BITS > 32 {
            invalid.push(0x1_0000_0010_u64 as usize);
        }
        for raw in invalid {
            f.calls.clear();
            f.result[1] = raw;
            let got = unsafe { a2fo_rs_stage10_resource_local_team(context(&mut f), step) };
            assert_eq!(got, 0);
            assert_eq!(f.calls, [(0, 0), (1, 0)]);
        }
        f.calls.clear();
        f.result[1] = 15;
        f.result[2] = 0xabc;
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_local_team(context(&mut f), step) },
            0xabc
        );
        assert_eq!(f.calls, [(0, 0), (1, 0), (2, 15)]);
    }

    #[test]
    fn override_guards_and_native_error_paths() {
        let mut f = Fixture::default();
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_override(10, 4, context(&mut f), step) },
            0
        );
        assert_eq!(f.calls, [(0, 0)]);
        f.result[0] = 1;
        for (resource, kind) in [(10, 0), (0, 4)] {
            f.calls.clear();
            assert_eq!(
                unsafe { a2fo_rs_stage10_resource_override(resource, kind, context(&mut f), step) },
                0
            );
            assert_eq!(f.calls, [(0, 0)]);
        }
        f.calls.clear();
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_override(0, 0, context(&mut f), step) },
            0
        );
        assert_eq!(f.calls, [(0, 0), (1, 0)]); // native key construction failed
        f.result[1] = 1;
        f.calls.clear();
        f.result[2] = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_override(0, 0, context(&mut f), step) },
            0
        );
        assert_eq!(f.calls, [(0, 0), (1, 0), (2, 0)]);
    }

    #[test]
    fn override_lookup_order_mutation_and_assignment_fallback() {
        let mut f = Fixture::default();
        f.result[0] = 1;
        f.result[1] = 1;
        f.result[3] = 0x11;
        f.result[4] = 0x22;
        f.translated_after_lookup = 1;
        f.result[7] = 1;
        f.result[8] = 1;
        f.result[9] = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_override(9, 3, context(&mut f), step) },
            1
        );
        assert_eq!(
            f.calls,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (6, 0),
                (7, 0),
                (8, 1),
                (9, 0)
            ]
        );

        f.calls.clear();
        f.result[3] = 0;
        f.result[6] = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_override(0, 0, context(&mut f), step) },
            1
        );
        assert_eq!(
            f.calls,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (6, 0),
                (8, 0),
                (9, 0)
            ]
        );

        f.calls.clear();
        f.result[3] = 1;
        f.result[4] = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_override(0, 0, context(&mut f), step) },
            1
        );
        assert_eq!(
            f.calls,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (6, 0),
                (8, 0),
                (9, 0)
            ]
        );

        f.calls.clear();
        f.result[4] = 1;
        f.result[7] = 0; // translation exists but is empty
        f.result[8] = 0; // native catches failed assignment and clears output
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_override(0, 0, context(&mut f), step) },
            0
        );
        assert_eq!(
            f.calls,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (6, 0),
                (7, 0),
                (8, 0)
            ]
        );
    }

    #[test]
    fn localize_fallback_precedes_thread_local_and_override() {
        let mut f = Fixture::default();
        f.result[0] = 0x10;
        f.result[1] = 0x20;
        f.result[4] = 0x40;
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_localize(10, 3, context(&mut f), step) },
            0x10
        );
        assert_eq!(f.calls, [(0, 0)]);
        f.calls.clear();
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_localize(0, 3, context(&mut f), step) },
            0x20
        );
        assert_eq!(f.calls, [(1, 1), (2, 0), (3, 0)]);
        f.calls.clear();
        f.result[3] = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage10_resource_localize(9, 2, context(&mut f), step) },
            0x40
        );
        assert_eq!(f.calls, [(1, 0), (2, 0), (3, 0), (4, 0)]);
    }
}
