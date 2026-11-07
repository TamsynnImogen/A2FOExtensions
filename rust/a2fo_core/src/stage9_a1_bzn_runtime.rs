use core::ffi::c_void;

#[repr(C)]
pub struct FieldWidth {
    pub declared: u32,
    pub serialized: u32,
}

pub type DispatchStep = unsafe extern "C" fn(*mut c_void, u32) -> u64;

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_bzn_inspect_width(requested: u32) -> u8 {
    u8::from(requested == 40)
}

/// # Safety
/// `field` must cover eight readable bytes and `out` must point to a writable
/// `FieldWidth` for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_bzn_field_width(
    field: *const u8,
    requested: u32,
    out: *mut FieldWidth,
) {
    let declared = if unsafe { *field } == 2 {
        unsafe { core::ptr::read_unaligned(field.add(4).cast::<u32>()) }
    } else {
        0
    };
    let serialized = if declared == 32 || declared == 40 {
        declared
    } else {
        requested
    };
    unsafe {
        out.write(FieldWidth {
            declared,
            serialized,
        })
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_bzn_pad(a1_stream: u8, serialized: u32, requested: u32) -> u8 {
    u8::from(a1_stream != 0 && serialized < requested)
}

/// # Safety
/// `record` must cover twelve readable copied bytes for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_bzn_count_record(record: *const u8) -> u8 {
    // Match the two native memcpy reads before testing either field.
    let record_type = unsafe { core::ptr::read_unaligned(record.cast::<u32>()) };
    let size = unsafe { core::ptr::read_unaligned(record.add(4).cast::<u32>()) };
    u8::from((record_type & 0xff) == 4 && size == 4)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_bzn_unique_marker(matches: u32, distance: u32) -> u8 {
    u8::from(matches == 1 && distance > 0)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_bzn_consumed(
    _have_before: u8,
    _have_after: u8,
    before: usize,
    after: usize,
) -> usize {
    // The native logging expression uses cursor values even when inspection
    // failed; the two inspection flags do not enter its subtraction.
    after.saturating_sub(before)
}

/// # Safety
/// If `len` exceeds four, `lowered` must cover `len` readable,
/// already-lowercased bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_bzn_sol_suffix(lowered: *const u8, len: usize) -> u8 {
    if len <= 4 {
        return 0;
    }
    let suffix = unsafe { core::slice::from_raw_parts(lowered.add(len - 4), 4) };
    u8::from(suffix == b".sol")
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_bzn_last_start(count: u32) -> u32 {
    if count == 0 { 0 } else { count - 1 }
}

/// Step operations: 0 SOL-handled, 1 stored SOL result, 2 native guard and
/// A1 inspection (0 non-A1, 1 A1, 2 invalid reader),
/// 3 native original result, 4 A1 cursor/log workflow. Native code owns all
/// state and side effects. No Rust reference spans a callback.
///
/// # Safety
/// `context` and `step` must remain valid for every synchronous operation;
/// the callback must obey the listed operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_bzn_table_dispatch(
    context: *mut c_void,
    step: DispatchStep,
) -> usize {
    if unsafe { step(context, 0) } != 0 {
        return unsafe { step(context, 1) } as usize;
    }
    let inspection = unsafe { step(context, 2) };
    if inspection == 2 {
        return 0;
    }
    if inspection == 0 {
        return unsafe { step(context, 3) } as usize;
    }
    unsafe { step(context, 4) };
    1
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate std;
    use self::std::vec::Vec;

    fn field(label: u8, declared: u32, requested: u32) -> FieldWidth {
        let mut bytes = [0u8; 9];
        bytes[1] = label;
        bytes[5..9].copy_from_slice(&declared.to_ne_bytes());
        let mut result = FieldWidth {
            declared: 99,
            serialized: 99,
        };
        unsafe {
            a2fo_rs_stage9_a1_bzn_field_width(
                bytes.as_ptr().wrapping_add(1),
                requested,
                &mut result,
            )
        };
        result
    }

    #[test]
    fn width_marker_and_padding_boundaries() {
        assert_eq!(a2fo_rs_stage9_a1_bzn_inspect_width(40), 1);
        assert_eq!(a2fo_rs_stage9_a1_bzn_inspect_width(32), 0);
        let short = field(2, 32, 40);
        assert_eq!((short.declared, short.serialized), (32, 32));
        assert_eq!(a2fo_rs_stage9_a1_bzn_pad(1, short.serialized, 40), 1);
        let full = field(2, 40, 40);
        assert_eq!((full.declared, full.serialized), (40, 40));
        assert_eq!(a2fo_rs_stage9_a1_bzn_pad(1, full.serialized, 40), 0);
        let invalid = field(2, 20, 40);
        assert_eq!((invalid.declared, invalid.serialized), (20, 40));
        let unlabelled = field(3, 32, 40);
        assert_eq!((unlabelled.declared, unlabelled.serialized), (0, 40));
        assert_eq!(a2fo_rs_stage9_a1_bzn_pad(0, 32, 40), 0);
    }

    #[test]
    fn count_record_low_type_byte_and_marker_admission() {
        let mut bytes = [0u8; 13];
        bytes[1..5].copy_from_slice(&0x1234_5604u32.to_ne_bytes());
        bytes[5..9].copy_from_slice(&4u32.to_ne_bytes());
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_bzn_count_record(bytes.as_ptr().wrapping_add(1)) },
            1
        );
        bytes[5..9].copy_from_slice(&5u32.to_ne_bytes());
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_bzn_count_record(bytes.as_ptr().wrapping_add(1)) },
            0
        );
        bytes[1..5].copy_from_slice(&0x1234_5605u32.to_ne_bytes());
        bytes[5..9].copy_from_slice(&4u32.to_ne_bytes());
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_bzn_count_record(bytes.as_ptr().wrapping_add(1)) },
            0
        );
        assert_eq!(a2fo_rs_stage9_a1_bzn_unique_marker(1, 0), 0);
        assert_eq!(a2fo_rs_stage9_a1_bzn_unique_marker(1, 1), 1);
        assert_eq!(a2fo_rs_stage9_a1_bzn_unique_marker(2, 1), 0);
    }

    #[test]
    fn consumed_uses_offsets_and_guards_underflow() {
        assert_eq!(a2fo_rs_stage9_a1_bzn_consumed(1, 1, 5, 9), 4);
        assert_eq!(a2fo_rs_stage9_a1_bzn_consumed(0, 0, 5, 9), 4);
        assert_eq!(a2fo_rs_stage9_a1_bzn_consumed(1, 0, 9, 5), 0);
        assert_eq!(a2fo_rs_stage9_a1_bzn_consumed(0, 1, 9, 9), 0);
        assert_eq!(a2fo_rs_stage9_a1_bzn_last_start(0), 0);
        assert_eq!(a2fo_rs_stage9_a1_bzn_last_start(1), 0);
        assert_eq!(a2fo_rs_stage9_a1_bzn_last_start(8), 7);
    }

    #[test]
    fn suffix_uses_exact_lowercased_length_including_embedded_nul() {
        for (bytes, expected) in [
            (&b".sol"[..], 0),
            (&b"a.sol"[..], 1),
            (&b"x\0a.sol"[..], 1),
            (&b"a.sox"[..], 0),
            (&b"A.SOL"[..], 0),
            (&b"a.sol\0"[..], 0),
        ] {
            assert_eq!(
                unsafe { a2fo_rs_stage9_a1_bzn_sol_suffix(bytes.as_ptr(), bytes.len()) },
                expected
            );
        }
    }

    struct DispatchTrace {
        calls: Vec<u32>,
        sol: bool,
        inspection: u64,
        sol_result: u64,
        original_result: u64,
    }
    unsafe extern "C" fn dispatch_step(opaque: *mut c_void, op: u32) -> u64 {
        let trace = unsafe { &mut *opaque.cast::<DispatchTrace>() };
        trace.calls.push(op);
        match op {
            0 => u64::from(trace.sol),
            1 => trace.sol_result,
            2 => trace.inspection,
            3 => trace.original_result,
            4 => 0,
            _ => panic!("unexpected callback operation"),
        }
    }

    #[test]
    fn dispatch_calls_only_the_selected_native_workflow() {
        let mut trace = DispatchTrace {
            calls: Vec::new(),
            sol: true,
            inspection: 1,
            sol_result: 17,
            original_result: 29,
        };
        let context = (&mut trace as *mut DispatchTrace).cast();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_bzn_table_dispatch(context, dispatch_step) },
            17
        );
        assert_eq!(trace.calls, [0, 1]);
        trace.calls.clear();
        trace.sol = false;
        trace.inspection = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_bzn_table_dispatch(context, dispatch_step) },
            29
        );
        assert_eq!(trace.calls, [0, 2, 3]);
        trace.calls.clear();
        trace.inspection = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_bzn_table_dispatch(context, dispatch_step) },
            1
        );
        assert_eq!(trace.calls, [0, 2, 4]);
        trace.calls.clear();
        trace.inspection = 2;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_bzn_table_dispatch(context, dispatch_step) },
            0
        );
        assert_eq!(trace.calls, [0, 2]);
    }
}
