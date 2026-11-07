//! Portable menu path and switch ordering. Native callbacks own strings, CRT
//! case conversion, Win32 atomic reads and original exception scopes.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, u64) -> u64;

struct Flow {
    context: *mut c_void,
    step: Step,
    failed: *const u8,
}

impl Flow {
    fn call(&self, id: u32, arg: u64) -> Option<u64> {
        let value = unsafe { (self.step)(self.context, id, arg) };
        (unsafe { self.failed.read() } == 0).then_some(value)
    }

    fn run(&self, kind: u32) -> Option<u64> {
        match kind {
            0 => {
                if self.call(0, 0)? != 0 {
                    self.call(1, 0)?;
                }
                if self.call(2, 0)? != 0 {
                    self.call(3, 0)?;
                }
                Some(0)
            }
            1 => {
                if self.call(0, 0)? != 0 {
                    return Some(0);
                }
                self.call(1, 0)?;
                if self.call(2, 0)? == 0
                    && self.call(3, 0)? != 0
                    && self.call(4, 0)? == 0
                    && self.call(5, 0)? != 0
                {
                    self.call(6, 0)?;
                }
                self.call(7, 0)?;
                Some(1)
            }
            _ => Some(0),
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_core_menu_join_path(
    left_length: usize,
    right_length: usize,
    left_last: u8,
) -> u32 {
    if left_length == 0 {
        0
    } else if right_length == 0 {
        1
    } else if left_last == b'\\' || left_last == b'/' {
        2
    } else {
        3
    }
}

/// # Safety
/// The callback, context and failure byte remain valid during this synchronous
/// call. The callback contains C++ exceptions and preserves native byte rules.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_menu_ascii_lower(
    length: usize,
    context: *mut c_void,
    step: Step,
    failed: *const u8,
) {
    if failed.is_null() {
        return;
    }
    let flow = Flow {
        context,
        step,
        failed,
    };
    for index in 0..length {
        if flow.call(0, index as u64).is_none() {
            break;
        }
    }
}

/// # Safety
/// The callback, context and failure byte remain valid during this synchronous
/// call. Native callbacks contain exceptions and own all mutable state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_menu_flow(
    kind: u32,
    context: *mut c_void,
    step: Step,
    failed: *const u8,
) -> u64 {
    if failed.is_null() {
        return 0;
    }
    Flow {
        context,
        step,
        failed,
    }
    .run(kind)
    .unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Trace {
        ids: Vec<u32>,
        values: [u64; 8],
        failed: u8,
        fail_at: Option<u32>,
        indices: Vec<u64>,
    }

    unsafe extern "C" fn step(context: *mut c_void, id: u32, arg: u64) -> u64 {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.ids.push(id);
        trace.indices.push(arg);
        if trace.fail_at == Some(id) {
            trace.failed = 1;
        }
        trace.values[id as usize]
    }

    fn trace(values: [u64; 8]) -> Trace {
        Trace {
            ids: Vec::new(),
            values,
            failed: 0,
            fail_at: None,
            indices: Vec::new(),
        }
    }

    fn run(kind: u32, trace: &mut Trace) -> u64 {
        unsafe {
            a2fo_rs_stage10_core_menu_flow(kind, (trace as *mut Trace).cast(), step, &trace.failed)
        }
    }

    #[test]
    fn path_decision_preserves_empty_and_separator_cases() {
        assert_eq!(a2fo_rs_stage10_core_menu_join_path(0, 0, 0), 0);
        assert_eq!(a2fo_rs_stage10_core_menu_join_path(0, 3, 0), 0);
        assert_eq!(a2fo_rs_stage10_core_menu_join_path(3, 0, b'/'), 1);
        assert_eq!(a2fo_rs_stage10_core_menu_join_path(3, 2, b'/'), 2);
        assert_eq!(a2fo_rs_stage10_core_menu_join_path(3, 2, b'\\'), 2);
        assert_eq!(a2fo_rs_stage10_core_menu_join_path(3, 2, 0), 3);
        assert_eq!(a2fo_rs_stage10_core_menu_join_path(3, 2, 0xff), 3);
    }

    #[test]
    fn suffix_reads_second_flag_after_first_append() {
        let mut t = trace([1, 0, 1, 0, 0, 0, 0, 0]);
        run(0, &mut t);
        assert_eq!(t.ids, [0, 1, 2, 3]);
        let mut t = trace([0, 0, 1, 0, 0, 0, 0, 0]);
        run(0, &mut t);
        assert_eq!(t.ids, [0, 2, 3]);
    }

    #[test]
    fn suffix_failure_stops_before_second_atomic_read() {
        let mut t = trace([1, 0, 1, 0, 0, 0, 0, 0]);
        t.fail_at = Some(1);
        run(0, &mut t);
        assert_eq!(t.ids, [0, 1]);
    }

    #[test]
    fn wide_suffix_decision_preserves_short_circuit_and_pop_order() {
        let mut empty = trace([1, 0, 0, 0, 0, 0, 0, 0]);
        assert_eq!(run(1, &mut empty), 0);
        assert_eq!(empty.ids, [0]);

        let mut gap = trace([0, 0, 0, 1, 0, 1, 0, 0]);
        assert_eq!(run(1, &mut gap), 1);
        assert_eq!(gap.ids, [0, 1, 2, 3, 4, 5, 6, 7]);

        let mut no_gap = trace([0, 0, 0, 0, 0, 1, 0, 0]);
        run(1, &mut no_gap);
        assert_eq!(no_gap.ids, [0, 1, 2, 3, 7]);
    }

    #[test]
    fn ascii_iteration_keeps_byte_indices_and_stops_on_failure() {
        let mut t = trace([0; 8]);
        unsafe {
            a2fo_rs_stage10_core_menu_ascii_lower(
                4,
                (&mut t as *mut Trace).cast(),
                step,
                &t.failed,
            );
        }
        assert_eq!(t.ids, [0, 0, 0, 0]);
        assert_eq!(t.indices, [0, 1, 2, 3]);

        let mut t = trace([0; 8]);
        t.fail_at = Some(0);
        unsafe {
            a2fo_rs_stage10_core_menu_ascii_lower(
                4,
                (&mut t as *mut Trace).cast(),
                step,
                &t.failed,
            );
        }
        assert_eq!(t.indices, [0]);
    }

    #[test]
    fn ascii_callback_keeps_nul_and_high_bytes_in_place() {
        struct Bytes {
            bytes: [u8; 5],
            failed: u8,
        }
        unsafe extern "C" fn native_lower(context: *mut c_void, _: u32, index: u64) -> u64 {
            let bytes = unsafe { &mut *context.cast::<Bytes>() };
            let byte = &mut bytes.bytes[index as usize];
            if byte.is_ascii_uppercase() {
                *byte += 32;
            }
            0
        }
        let mut bytes = Bytes {
            bytes: [b'A', 0, 0x80, b'Z', 0xff],
            failed: 0,
        };
        unsafe {
            a2fo_rs_stage10_core_menu_ascii_lower(
                bytes.bytes.len(),
                (&mut bytes as *mut Bytes).cast(),
                native_lower,
                &bytes.failed,
            );
        }
        assert_eq!(bytes.bytes, [b'a', 0, 0x80, b'z', 0xff]);
    }
}
