//! Portable order controllers for selected CraftIdentity traversals.
//! Native callbacks retain CRT comparisons, whitespace classification,
//! provider calls, rectangle arithmetic, and storage mutations.

use core::ffi::{c_long, c_void};

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u32;
type IsSpace = unsafe extern "C" fn(u8) -> bool;

#[repr(C)]
pub struct TrimSpan {
    begin: usize,
    end: usize,
}

fn first_match(count: usize, mut step: impl FnMut(usize) -> u32) -> bool {
    (0..count).any(|index| step(index) == 1)
}

fn trim(bytes: &[u8], mut is_space: impl FnMut(u8) -> bool) -> TrimSpan {
    let mut begin = 0;
    while begin < bytes.len() && is_space(bytes[begin]) {
        begin += 1;
    }
    let mut end = bytes.len();
    while end > begin && is_space(bytes[end - 1]) {
        end -= 1;
    }
    TrimSpan { begin, end }
}

fn display_tail(bytes: &[u8], parsed: c_long, mut is_space: impl FnMut(u8) -> bool) -> bool {
    for &byte in bytes {
        if byte == 0 {
            return (1..=2).contains(&parsed);
        }
        if !is_space(byte) {
            return false;
        }
    }
    false
}

fn each_visible(count: usize, mut step: impl FnMut(u32, usize) -> u32) -> bool {
    for index in 0..count {
        match step(0, index) {
            0 => continue,
            1 => {}
            _ => return false,
        }
        if step(1, index) != 1 {
            return false;
        }
    }
    true
}

/// # Safety
/// callback and context remain valid synchronously; no native
/// exception may cross this C ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_identity_first_match(
    count: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else { return false };
    first_match(count, |index| unsafe { step(context, 0, index) })
}

/// # Safety
/// bytes points to `length` readable bytes, span is writable, and
/// is_space uses the same native CRT classification as the source loop.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_identity_trim(
    bytes: *const u8,
    length: usize,
    is_space: Option<IsSpace>,
    span: *mut TrimSpan,
) -> bool {
    let Some(is_space) = is_space else {
        return false;
    };
    if bytes.is_null() || span.is_null() {
        return false;
    }
    let result = trim(
        unsafe { core::slice::from_raw_parts(bytes, length) },
        |byte| unsafe { is_space(byte) },
    );
    unsafe { *span = result };
    true
}

/// # Safety
/// bytes points to `length` readable bytes including a terminating
/// NUL; native strtol has already parsed the prefix.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_identity_display_tail(
    bytes: *const u8,
    length: usize,
    parsed: c_long,
    is_space: Option<IsSpace>,
) -> bool {
    let Some(is_space) = is_space else {
        return false;
    };
    if bytes.is_null() {
        return false;
    }
    display_tail(
        unsafe { core::slice::from_raw_parts(bytes, length) },
        parsed,
        |byte| unsafe { is_space(byte) },
    )
}

/// # Safety
/// callback reads native bool values; no native exception may cross
/// this C ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_identity_any(
    count: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else { return false };
    first_match(count, |index| unsafe { step(context, 0, index) })
}

/// # Safety
/// callback owns native hover storage; operation 0 checks visibility,
/// operation 1 updates exactly one visible component. It returns 0 on a
/// formatting failure so no later component or cursor update is performed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_identity_each_visible(
    count: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else { return false };
    each_visible(count, |operation, index| unsafe {
        step(context, operation, index)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn first_match_stops_before_later_provider_calls() {
        let mut visited = Vec::new();
        assert!(first_match(5, |index| {
            visited.push(index);
            if index == 2 { 1 } else { 0 }
        }));
        assert_eq!(visited, [0, 1, 2]);
    }

    #[test]
    fn trim_uses_full_string_including_embedded_nul() {
        let span = trim(b" \0x ", |byte| byte == b' ');
        assert_eq!((span.begin, span.end), (1, 3));
    }

    #[test]
    fn display_tail_admits_only_whitespace_then_nul_and_range() {
        let space = |byte| byte == b' ' || byte == b'\t';
        assert!(display_tail(b" \t\0ignored", 2, space));
        assert!(!display_tail(b" x\0", 2, space));
        assert!(!display_tail(b" \0", 3, space));
        assert!(!display_tail(b" ", 1, space));
    }

    #[test]
    fn hover_skips_invisible_and_stops_on_partial_failure() {
        let mut calls = Vec::new();
        assert!(!each_visible(5, |operation, index| {
            calls.push((operation, index));
            if operation == 0 && index == 1 {
                return 0;
            }
            if operation == 1 && index == 3 {
                return 0;
            }
            1
        }));
        assert_eq!(
            calls,
            [(0, 0), (1, 0), (0, 1), (0, 2), (1, 2), (0, 3), (1, 3)]
        );
    }
}
