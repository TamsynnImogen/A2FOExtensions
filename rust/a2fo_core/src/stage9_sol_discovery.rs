//! Pure SOL discovery policy helpers for a native-owned dispatch context.
//!
//! Pointer arguments are byte spans owned and kept alive by the synchronous
//! native caller. For nonzero lengths, the pointer must be non-null and valid
//! for reads of exactly `len` bytes. No slice, allocation, callback, or
//! borrowed pointer escapes these functions. Directory walking, native string
//! construction, resizing, and exception handling remain native-owned.

/// Whether the signed discovery pass is within the native log cap.
///
/// This intentionally preserves signed comparison behavior: wrapped negative
/// interlocked values satisfy `pass <= 8`.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_discovery_log_pass(pass: i32) -> bool {
    pass <= 8
}

/// Find the last slash or backslash in a native-owned byte span.
///
/// Returns the separator's index, or `usize::MAX` when no separator exists.
/// The caller can use the index to construct its parent string using native
/// string operations, preserving native allocation and exception semantics.
///
/// # Safety
/// If `len` is nonzero, `bytes` must be non-null and readable for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_parent_end(bytes: *const u8, len: usize) -> usize {
    for index in (0..len).rev() {
        // SAFETY: The caller guarantees a readable span for nonzero `len`.
        if matches!(unsafe { *bytes.add(index) }, b'/' | b'\\') {
            return index;
        }
    }
    usize::MAX
}

/// Check a leaf that native code has already lowercased with its locale helper.
///
/// The helper accepts only exactly three bytes equal to `bzn`; it does not
/// lowercase or otherwise transform the input.
///
/// # Safety
/// If `len` is nonzero, `bytes` must be non-null and readable for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_is_bzn_leaf(bytes: *const u8, len: usize) -> bool {
    if len != 3 {
        return false;
    }
    // SAFETY: The caller guarantees a readable span for the exact length 3.
    unsafe { *bytes == b'b' && *bytes.add(1) == b'z' && *bytes.add(2) == b'n' }
}

#[cfg(test)]
extern crate std;

#[cfg(test)]
mod tests {
    use super::{
        a2fo_rs_stage9_sol_discovery_log_pass, a2fo_rs_stage9_sol_is_bzn_leaf,
        a2fo_rs_stage9_sol_parent_end,
    };

    #[test]
    fn discovery_log_pass_uses_signed_eight_pass_cap() {
        assert!(a2fo_rs_stage9_sol_discovery_log_pass(7));
        assert!(a2fo_rs_stage9_sol_discovery_log_pass(8));
        assert!(!a2fo_rs_stage9_sol_discovery_log_pass(9));
        assert!(a2fo_rs_stage9_sol_discovery_log_pass(i32::MIN));
        assert!(a2fo_rs_stage9_sol_discovery_log_pass(-1));
        assert!(!a2fo_rs_stage9_sol_discovery_log_pass(i32::MAX));
    }

    #[test]
    fn parent_end_finds_last_either_separator_without_string_rules() {
        let empty: [u8; 0] = [];
        let no_separator = b"bzn";
        let slash = b"root/sol/map.sol";
        let backslash = b"root\\bzn\\map.sol";
        let leading = b"/root";
        let only_separator = b"\\";
        let trailing = b"root/bzn/";
        let mixed = b"root/sol\\map.sol";
        let embedded_nul = b"root\0bzn/map.sol";
        // SAFETY: Each pointer and length describe a live byte array.
        unsafe {
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(empty.as_ptr(), empty.len()),
                usize::MAX
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(no_separator.as_ptr(), no_separator.len()),
                usize::MAX
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(slash.as_ptr(), slash.len()),
                8
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(backslash.as_ptr(), backslash.len()),
                8
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(leading.as_ptr(), leading.len()),
                0
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(only_separator.as_ptr(), only_separator.len()),
                0
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(trailing.as_ptr(), trailing.len()),
                8
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(mixed.as_ptr(), mixed.len()),
                8
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(embedded_nul.as_ptr(), embedded_nul.len()),
                8
            );
            assert_eq!(
                a2fo_rs_stage9_sol_parent_end(core::ptr::null(), 0),
                usize::MAX
            );
        }
    }

    #[test]
    fn bzn_leaf_requires_the_exact_pre_lowercased_three_byte_span() {
        let bzn = b"bzn";
        let upper = b"BZN";
        let mixed = b"bzN";
        let short = b"bz";
        let long = b"bznx";
        let nul = b"bz\0";
        // SAFETY: Each pointer and length describe a live byte array.
        unsafe {
            assert!(a2fo_rs_stage9_sol_is_bzn_leaf(bzn.as_ptr(), bzn.len()));
            assert!(!a2fo_rs_stage9_sol_is_bzn_leaf(upper.as_ptr(), upper.len()));
            assert!(!a2fo_rs_stage9_sol_is_bzn_leaf(mixed.as_ptr(), mixed.len()));
            assert!(!a2fo_rs_stage9_sol_is_bzn_leaf(short.as_ptr(), short.len()));
            assert!(!a2fo_rs_stage9_sol_is_bzn_leaf(long.as_ptr(), long.len()));
            assert!(!a2fo_rs_stage9_sol_is_bzn_leaf(nul.as_ptr(), nul.len()));
            assert!(!a2fo_rs_stage9_sol_is_bzn_leaf(core::ptr::null(), 0));
        }
    }
}
