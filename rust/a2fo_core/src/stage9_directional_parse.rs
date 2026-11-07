//! Residual DirectionalShields field and float text policy.
//! Native callbacks retain ODF storage, CRT comparison and string assignment.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, u32, *mut u64) -> u8;

fn call(context: *mut c_void, step: Step, operation: u32, index: u32) -> Option<u64> {
    let mut result = 0;
    if unsafe { step(context, operation, index, &mut result) } == 1 {
        Some(result)
    } else {
        None
    }
}

/// Safety: native callback binds each ODF field only after the three pointer
/// admissions and name length read. No Rust reference spans a callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_directional_field_value(
    context: *mut c_void,
    step: Option<Step>,
    count: u32,
) -> bool {
    let Some(step) = step else { return false };
    for operation in 0..3 {
        if call(context, step, operation, 0) != Some(1) {
            return false;
        }
    }
    if call(context, step, 3, 0).is_none() {
        return false;
    }
    for index in 0..count {
        if call(context, step, 4, index).is_none() {
            return false;
        }
        let mut matched = true;
        for operation in 5..=8 {
            match call(context, step, operation, index) {
                Some(1) => {}
                Some(_) => {
                    matched = false;
                    break;
                }
                None => return false,
            }
        }
        if !matched {
            continue;
        }
        return call(context, step, 9, index) == Some(1);
    }
    false
}

fn suffix_space(value: u8) -> bool {
    matches!(value, b' ' | b'\t' | b'\r' | b'\n')
}

/// Safety: `tail` points within a live native std::string, and `length`
/// includes its terminal NUL. The function makes no foreign callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_directional_float_suffix(
    tail: *const u8,
    length: usize,
    consumed: bool,
    bits: u32,
) -> bool {
    let parsed = f32::from_bits(bits);
    if !consumed || !parsed.is_finite() || tail.is_null() || length == 0 {
        return false;
    }
    let mut index = 0;
    while index < length && suffix_space(unsafe { *tail.add(index) }) {
        index += 1;
    }
    if index < length && matches!(unsafe { *tail.add(index) }, b'f' | b'F') {
        index += 1;
    }
    while index < length && suffix_space(unsafe { *tail.add(index) }) {
        index += 1;
    }
    index < length && unsafe { *tail.add(index) } == 0 && parsed >= 0.0
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Fields {
        trace: Vec<(u32, u32)>,
        admit: bool,
        fail_comparison: bool,
    }

    unsafe extern "C" fn step(
        context: *mut c_void,
        operation: u32,
        index: u32,
        output: *mut u64,
    ) -> u8 {
        let state = unsafe { &mut *context.cast::<Fields>() };
        state.trace.push((operation, index));
        if state.fail_comparison && operation == 7 && index == 2 {
            return 0;
        }
        let answer = match (operation, index) {
            (0, _) => u64::from(state.admit),
            (1..=3, _) | (4, _) => 1,
            (5, 0) => 0, // null name: skip length and comparison
            (5, _) => 1,
            (6, 1) => 0, // size mismatch: skip CRT comparison
            (6, _) => 1,
            (7, _) => 1,
            (8, 2) => 0, // matching name with invalid value
            (8..=9, _) => 1,
            _ => 0,
        };
        unsafe { *output = answer };
        1
    }

    #[test]
    fn field_admission_and_first_valid_duplicate() {
        let mut state = Fields {
            trace: Vec::new(),
            admit: false,
            fail_comparison: false,
        };
        let context = (&mut state as *mut Fields).cast();
        assert!(!unsafe { a2fo_rs_stage9_directional_field_value(context, Some(step), 5) });
        assert_eq!(state.trace, [(0, 0)]);

        state.admit = true;
        state.trace.clear();
        assert!(unsafe { a2fo_rs_stage9_directional_field_value(context, Some(step), 5) });
        assert_eq!(
            state.trace,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (4, 1),
                (5, 1),
                (6, 1),
                (4, 2),
                (5, 2),
                (6, 2),
                (7, 2),
                (8, 2),
                (4, 3),
                (5, 3),
                (6, 3),
                (7, 3),
                (8, 3),
                (9, 3),
            ]
        );
    }

    #[test]
    fn callback_failure_stops_before_later_fields_or_assignment() {
        let mut state = Fields {
            trace: Vec::new(),
            admit: true,
            fail_comparison: true,
        };
        let context = (&mut state as *mut Fields).cast();
        assert!(!unsafe { a2fo_rs_stage9_directional_field_value(context, Some(step), 5) });
        assert_eq!(state.trace.last(), Some(&(7, 2)));
        assert!(
            !state
                .trace
                .iter()
                .any(|&(operation, index)| index >= 3 || (index == 2 && operation >= 8))
        );
    }

    #[test]
    fn float_suffix_ascii_and_embedded_nul() {
        let valid: &[&[u8]] = &[b"\0", b" \t f \r\n\0", b"F\0", b"\0hidden\0"];
        for tail in valid {
            assert!(unsafe {
                a2fo_rs_stage9_directional_float_suffix(
                    tail.as_ptr(),
                    tail.len(),
                    true,
                    1.25f32.to_bits(),
                )
            });
        }
        for tail in [b"ff\0".as_slice(), b"\x0bf\0", b"f X\0"] {
            assert!(!unsafe {
                a2fo_rs_stage9_directional_float_suffix(
                    tail.as_ptr(),
                    tail.len(),
                    true,
                    1.25f32.to_bits(),
                )
            });
        }
        assert!(unsafe {
            a2fo_rs_stage9_directional_float_suffix(
                c"".as_ptr().cast(),
                1,
                true,
                (-0.0f32).to_bits(),
            )
        });
        for (consumed, bits) in [
            (false, 1.0f32.to_bits()),
            (true, (-1.0f32).to_bits()),
            (true, f32::INFINITY.to_bits()),
            (true, f32::NAN.to_bits()),
        ] {
            assert!(!unsafe {
                a2fo_rs_stage9_directional_float_suffix(c"".as_ptr().cast(), 1, consumed, bits)
            });
        }
    }
}
