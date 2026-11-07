//! Pure residual SOL completion decisions; native code retains allocation,
//! object-layout, and callback ownership.

static SOL_DIRECTORY: &[u8] = b"sol\0";
static BZN_DIRECTORY: &[u8] = b"bzn\0";
static SEARCH_MASK: &[u8] = b"*.sol\0";
static VIRTUAL_PREFIX: &[u8] = b"bzn\\\0";
static VIRTUAL_MASK: &[u8] = b"bzn\\*.sol\0";
static SELECTED_SUFFIX: &[u8] = b" [selected]\0";
static EMPTY_SUFFIX: &[u8] = b"\0";
static POST_MISSION_FIRST: &[u8] =
    b"SOL phase2j object-only bring-up: skipped first native post-mission table\0";
static POST_MISSION_SECOND: &[u8] =
    b"SOL phase2j object-only bring-up: skipped second native post-mission table\0";

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_log_pass16(pass: i32) -> bool {
    pass <= 16
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_directory(index: u32) -> *const core::ffi::c_char {
    if index == 0 {
        SOL_DIRECTORY.as_ptr().cast()
    } else {
        BZN_DIRECTORY.as_ptr().cast()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_search_mask() -> *const core::ffi::c_char {
    SEARCH_MASK.as_ptr().cast()
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_virtual_prefix() -> *const core::ffi::c_char {
    VIRTUAL_PREFIX.as_ptr().cast()
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_virtual_mask() -> *const core::ffi::c_char {
    VIRTUAL_MASK.as_ptr().cast()
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_selected_suffix(selected: bool) -> *const core::ffi::c_char {
    if selected {
        SELECTED_SUFFIX.as_ptr().cast()
    } else {
        EMPTY_SUFFIX.as_ptr().cast()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_post_mission_message(
    second: bool,
) -> *const core::ffi::c_char {
    if second {
        POST_MISSION_SECOND.as_ptr().cast()
    } else {
        POST_MISSION_FIRST.as_ptr().cast()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_mission_present(length: usize) -> bool {
    length != 0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_probe_candidate(version: u32, selected: u32) -> bool {
    version == selected && version > 2177
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_probe_gate(
    operation: u32,
    reader: bool,
    context: bool,
    details: bool,
    mission: bool,
    list: bool,
) -> bool {
    match operation {
        11 => reader && !context,
        12 => reader && !details,
        13 => reader && context && details,
        14 => reader && context && details && mission && list,
        _ => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_caller_version(
    version: u32,
    reader: bool,
    context: bool,
    details: bool,
    mission: bool,
    list: bool,
) -> u32 {
    if reader && context && details && mission && list {
        2177
    } else {
        version
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_compatible_version() -> u32 {
    2177
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_counter_changed(now: i32, before: i32) -> bool {
    now != before
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_spawn_complete(state: i32) -> bool {
    state == 2
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_file_size(size: i64) -> bool {
    size > 0 && size <= 256 * 1024 * 1024
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_version_fits(version: u64) -> bool {
    version <= u32::MAX as u64
}

/// The native caller guarantees that a non-null pointer is readable for its
/// first byte; this policy reads only that byte and does not scan a C string.
///
/// # Safety
/// If `value` is non-null, it must point to at least one readable byte.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_input_present(value: *const u8) -> bool {
    !value.is_null() && unsafe { *value != 0 }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_empty_metadata() -> *const core::ffi::c_char {
    EMPTY_SUFFIX.as_ptr().cast()
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_sol_direct_source_root() -> u32 {
    0
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe fn assert_c_string(value: *const core::ffi::c_char, expected: &[u8]) {
        let bytes = value.cast::<u8>();
        for (index, expected_byte) in expected.iter().copied().enumerate() {
            // SAFETY: Every exported string is a static NUL-terminated byte string.
            assert_eq!(unsafe { *bytes.add(index) }, expected_byte);
        }
        // SAFETY: The exported string has a terminating NUL after `expected`.
        assert_eq!(unsafe { *bytes.add(expected.len()) }, 0);
    }

    #[test]
    fn signed_log_cap_includes_wrapped_negative_values() {
        assert!(a2fo_rs_stage9_sol_log_pass16(i32::MIN));
        assert!(a2fo_rs_stage9_sol_log_pass16(-1));
        assert!(a2fo_rs_stage9_sol_log_pass16(0));
        assert!(a2fo_rs_stage9_sol_log_pass16(15));
        assert!(a2fo_rs_stage9_sol_log_pass16(16));
        assert!(!a2fo_rs_stage9_sol_log_pass16(17));
        assert!(!a2fo_rs_stage9_sol_log_pass16(i32::MAX));
    }

    #[test]
    fn static_directory_and_mask_strings_are_exact_and_terminated() {
        // SAFETY: Returned pointers refer to static strings owned by this module.
        unsafe {
            assert_c_string(a2fo_rs_stage9_sol_directory(0), b"sol");
            assert_c_string(a2fo_rs_stage9_sol_directory(1), b"bzn");
            assert_c_string(a2fo_rs_stage9_sol_directory(u32::MAX), b"bzn");
            assert_c_string(a2fo_rs_stage9_sol_search_mask(), b"*.sol");
            assert_c_string(a2fo_rs_stage9_sol_virtual_prefix(), b"bzn\\");
            assert_c_string(a2fo_rs_stage9_sol_virtual_mask(), b"bzn\\*.sol");
            assert_c_string(a2fo_rs_stage9_sol_selected_suffix(true), b" [selected]");
            assert_c_string(a2fo_rs_stage9_sol_selected_suffix(false), b"");
            assert_c_string(
                a2fo_rs_stage9_sol_post_mission_message(false),
                b"SOL phase2j object-only bring-up: skipped first native post-mission table",
            );
            assert_c_string(
                a2fo_rs_stage9_sol_post_mission_message(true),
                b"SOL phase2j object-only bring-up: skipped second native post-mission table",
            );
        }
    }

    #[test]
    fn probe_gate_matches_all_five_input_flags_for_operations_eleven_through_fourteen() {
        for operation in 11..=14 {
            for bits in 0u32..32 {
                let reader = bits & 1 != 0;
                let context = bits & 2 != 0;
                let details = bits & 4 != 0;
                let mission = bits & 8 != 0;
                let list = bits & 16 != 0;
                let expected = match operation {
                    11 => reader && !context,
                    12 => reader && !details,
                    13 => reader && context && details,
                    14 => reader && context && details && mission && list,
                    _ => false,
                };
                assert_eq!(
                    a2fo_rs_stage9_sol_probe_gate(
                        operation, reader, context, details, mission, list
                    ),
                    expected,
                    "operation={operation}, bits={bits}"
                );
            }
        }
        assert!(!a2fo_rs_stage9_sol_probe_gate(
            10, true, true, true, true, true
        ));
        assert!(!a2fo_rs_stage9_sol_probe_gate(
            15, true, true, true, true, true
        ));
    }

    #[test]
    fn probe_candidate_and_caller_version_preserve_boundaries() {
        assert!(!a2fo_rs_stage9_sol_probe_candidate(2177, 2177));
        assert!(a2fo_rs_stage9_sol_probe_candidate(2178, 2178));
        assert!(!a2fo_rs_stage9_sol_probe_candidate(2178, 2177));
        assert!(!a2fo_rs_stage9_sol_probe_candidate(2177, 2178));
        assert_eq!(a2fo_rs_stage9_sol_compatible_version(), 2177);
        assert_eq!(
            a2fo_rs_stage9_sol_caller_version(3021, true, true, true, true, true),
            2177
        );
        for missing in 0..5 {
            let mut flags = [true; 5];
            flags[missing] = false;
            assert_eq!(
                a2fo_rs_stage9_sol_caller_version(
                    3021, flags[0], flags[1], flags[2], flags[3], flags[4]
                ),
                3021
            );
        }
    }

    #[test]
    fn mission_counter_spawn_size_and_version_decisions_cover_boundaries() {
        assert!(!a2fo_rs_stage9_sol_mission_present(0));
        assert!(a2fo_rs_stage9_sol_mission_present(1));
        assert!(!a2fo_rs_stage9_sol_counter_changed(i32::MIN, i32::MIN));
        assert!(a2fo_rs_stage9_sol_counter_changed(i32::MIN, i32::MAX));
        assert!(a2fo_rs_stage9_sol_counter_changed(1, 0));
        assert!(!a2fo_rs_stage9_sol_spawn_complete(-1));
        assert!(!a2fo_rs_stage9_sol_spawn_complete(1));
        assert!(a2fo_rs_stage9_sol_spawn_complete(2));
        assert!(!a2fo_rs_stage9_sol_spawn_complete(3));

        const MAX_FILE_SIZE: i64 = 256 * 1024 * 1024;
        assert!(!a2fo_rs_stage9_sol_file_size(i64::MIN));
        assert!(!a2fo_rs_stage9_sol_file_size(0));
        assert!(a2fo_rs_stage9_sol_file_size(1));
        assert!(a2fo_rs_stage9_sol_file_size(MAX_FILE_SIZE));
        assert!(!a2fo_rs_stage9_sol_file_size(MAX_FILE_SIZE + 1));
        assert!(!a2fo_rs_stage9_sol_file_size(i64::MAX));

        assert!(a2fo_rs_stage9_sol_version_fits(0));
        assert!(a2fo_rs_stage9_sol_version_fits(u32::MAX as u64));
        assert!(!a2fo_rs_stage9_sol_version_fits(u32::MAX as u64 + 1));
        assert!(!a2fo_rs_stage9_sol_version_fits(u64::MAX));
    }

    #[test]
    fn input_presence_reads_only_the_first_byte() {
        let empty = [0u8, b'x'];
        let first_present = [b'x', 0, b'z'];
        // SAFETY: Each non-null pointer refers to a readable byte array.
        unsafe {
            assert!(!a2fo_rs_stage9_sol_input_present(core::ptr::null()));
            assert!(!a2fo_rs_stage9_sol_input_present(empty.as_ptr()));
            assert!(a2fo_rs_stage9_sol_input_present(first_present.as_ptr()));
        }
    }
}
