//! Shield text/path decisions and swarm hardpoint selection. Native code owns
//! strings, CRT conversion, vector access, and exception handling.
use core::ffi::c_void;

type LowerStep = unsafe extern "C" fn(*mut c_void, usize);
type LastStep = unsafe extern "C" fn(*const c_void) -> u8;

/// # Safety
/// `context`, `step`, and `failed` remain valid for this synchronous call.
/// The callback contains native exceptions and sets `failed` on failure.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_visibility_lower(
    length: usize,
    context: *mut c_void,
    step: LowerStep,
    failed: *const u8,
) {
    if failed.is_null() {
        return;
    }
    for index in 0..length {
        unsafe { step(context, index) };
        if unsafe { failed.read() } != 0 {
            break;
        }
    }
}

/// Returns 0 for right, 1 for left, 2 for direct concatenation, and 3 for
/// concatenation with a backslash. Last is called only when both inputs exist.
/// # Safety
/// `context` and `last` remain valid for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_visibility_join(
    left_length: usize,
    right_length: usize,
    context: *const c_void,
    last: LastStep,
) -> u32 {
    if left_length == 0 {
        return 0;
    }
    if right_length == 0 {
        return 1;
    }
    match unsafe { last(context) } {
        b'\\' | b'/' => 2,
        _ => 3,
    }
}

/// # Safety
/// `selected` points to a writable `usize` whenever `size` is nonzero.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_swarm_hardpoint_index(
    size: usize,
    request: usize,
    selected: *mut usize,
) -> u8 {
    if size == 0 {
        return 0;
    }
    if selected.is_null() {
        return 0;
    }
    unsafe { selected.write(request % size) };
    1
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn lower_step(context: *mut c_void, index: usize) {
        let bytes = unsafe { &mut *(context as *mut Vec<u8>) };
        bytes[index] = match bytes[index] {
            b'A'..=b'Z' => bytes[index] + 32,
            other => other,
        };
    }

    #[test]
    fn lower_visits_all_bytes_including_nul_and_high_bytes() {
        let mut bytes = vec![b'A', 0, b'Z', 0xff];
        let failed = 0;
        unsafe {
            a2fo_rs_stage10_visibility_lower(
                bytes.len(),
                (&mut bytes as *mut Vec<u8>).cast(),
                lower_step,
                &failed,
            )
        };
        assert_eq!(bytes, [b'a', 0, b'z', 0xff]);
    }

    unsafe extern "C" fn failing_lower_step(context: *mut c_void, index: usize) {
        let state = unsafe { &mut *(context as *mut (Vec<usize>, u8)) };
        state.0.push(index);
        if index == 2 {
            state.1 = 1;
        }
    }

    #[test]
    fn lower_stops_after_native_failure_in_index_order() {
        let mut state = (Vec::new(), 0u8);
        let failed = &state.1 as *const u8;
        unsafe {
            a2fo_rs_stage10_visibility_lower(
                6,
                (&mut state as *mut (Vec<usize>, u8)).cast(),
                failing_lower_step,
                failed,
            )
        };
        assert_eq!(state.0, [0usize, 1, 2]);
    }

    unsafe extern "C" fn last_step(context: *const c_void) -> u8 {
        let state = unsafe { &mut *(context as *mut (u8, usize)) };
        state.1 += 1;
        state.0
    }

    #[test]
    fn join_guards_last_read_and_selects_separator() {
        let mut state = (b'/', 0usize);
        let ptr = (&mut state as *mut (u8, usize)).cast();
        assert_eq!(
            unsafe { a2fo_rs_stage10_visibility_join(0, 0, ptr, last_step) },
            0
        );
        assert_eq!(
            unsafe { a2fo_rs_stage10_visibility_join(1, 0, ptr, last_step) },
            1
        );
        assert_eq!(state.1, 0);
        assert_eq!(
            unsafe { a2fo_rs_stage10_visibility_join(1, 1, ptr, last_step) },
            2
        );
        state.0 = b'\\';
        assert_eq!(
            unsafe { a2fo_rs_stage10_visibility_join(1, 1, ptr, last_step) },
            2
        );
        state.0 = b'x';
        assert_eq!(
            unsafe { a2fo_rs_stage10_visibility_join(1, 1, ptr, last_step) },
            3
        );
        assert_eq!(state.1, 3);
    }

    #[test]
    fn hardpoint_empty_and_full_width_requests() {
        let mut selected = usize::MAX;
        assert_eq!(
            unsafe { a2fo_rs_stage10_swarm_hardpoint_index(0, usize::MAX, &mut selected) },
            0
        );
        assert_eq!(selected, usize::MAX);
        assert_eq!(
            unsafe { a2fo_rs_stage10_swarm_hardpoint_index(3, usize::MAX, &mut selected) },
            1
        );
        assert_eq!(selected, usize::MAX % 3);
        assert_eq!(
            unsafe { a2fo_rs_stage10_swarm_hardpoint_index(1, usize::MAX, &mut selected) },
            1
        );
        assert_eq!(selected, 0);
    }
}
