//! Ordered admission of RGB discovery records. Native callbacks own the map
//! cursors, strings, and mutations; Rust only chooses the callback sequence.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u32;

fn traverse(mut call: impl FnMut(u32) -> u32, mode: u32) -> bool {
    if !matches!(mode, 3 | 5 | 6 | 7) {
        return false;
    }
    loop {
        match call(0) {
            0 => return true,
            1 => {}
            _ => return false,
        }
        if mode == 3 || mode == 6 {
            match call(1) {
                0 => continue,
                1 => {}
                _ => return false,
            }
        }
        if call(2) != 1 {
            return false;
        }
        if mode == 3 && call(3) != 1 {
            return false;
        }
    }
}

/// Safety: `step` synchronously owns and advances native map cursors. It must
/// keep its context valid for the call, catch native exceptions, and return
/// `u32::MAX` on failure. It may mutate only maps other than its source map.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_rgb_traverse(
    mode: u32,
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    traverse(|operation| unsafe { step(context, operation, 0) }, mode)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dds_publishes_exact_before_alias_and_skips_non_dds() {
        let mut calls = Vec::new();
        let mut next = 0;
        let mut predicate = 0;
        assert!(traverse(
            |operation| {
                calls.push(operation);
                match operation {
                    0 => {
                        next += 1;
                        if next <= 3 { 1 } else { 0 }
                    }
                    1 => {
                        predicate += 1;
                        if predicate == 2 { 0 } else { 1 }
                    }
                    _ => 1,
                }
            },
            3
        ));
        assert_eq!(calls, [0, 1, 2, 3, 0, 1, 0, 1, 2, 3, 0]);
    }

    #[test]
    fn folder_and_append_publish_every_record() {
        for mode in [5, 7] {
            let mut calls = Vec::new();
            let mut next = 0;
            assert!(traverse(
                |operation| {
                    calls.push(operation);
                    if operation == 0 {
                        next += 1;
                        if next <= 2 { 1 } else { 0 }
                    } else {
                        1
                    }
                },
                mode
            ));
            assert_eq!(calls, [0, 2, 0, 2, 0]);
        }
    }

    #[test]
    fn tga_checks_each_candidate_and_stops_at_failure() {
        let mut calls = Vec::new();
        let mut next = 0;
        assert!(!traverse(
            |operation| {
                calls.push(operation);
                match operation {
                    0 => {
                        next += 1;
                        1
                    }
                    1 if next == 1 => 0,
                    2 if next == 3 => u32::MAX,
                    _ => 1,
                }
            },
            6
        ));
        assert_eq!(calls, [0, 1, 0, 1, 2, 0, 1, 2]);
    }

    #[test]
    fn invalid_mode_and_cursor_error_do_not_publish() {
        assert!(!traverse(|_| panic!("invalid mode called callback"), 8));
        assert!(!traverse(|_| u32::MAX, 5));
    }
}
