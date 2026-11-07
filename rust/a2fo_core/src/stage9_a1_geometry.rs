//! Pure integer policy for A1 native UI rectangles and preferred children.
//!
//! Rectangle pointers refer to four readable `i32` fields ordered
//! left, top, right, bottom. Output pointers refer to four writable `i32`
//! fields in the same order. The preferred-child callback is synchronous;
//! the helper keeps no borrowed data across it.

unsafe fn rectangle_values(rectangle: *const i32) -> (i32, i32, i32, i32) {
    // SAFETY: The caller guarantees four readable consecutive i32 values.
    unsafe {
        (
            *rectangle,
            *rectangle.add(1),
            *rectangle.add(2),
            *rectangle.add(3),
        )
    }
}

/// Write the minimal integer union of two native four-field rectangles.
///
/// # Safety
/// `first` and `second` must each point to four readable i32 values, and
/// `output` must point to four writable i32 values. The output span must not
/// overlap either input span.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_geometry_union(
    first: *const i32,
    second: *const i32,
    output: *mut i32,
) {
    if first.is_null() || second.is_null() || output.is_null() {
        return;
    }
    // SAFETY: The caller supplies valid four-i32 spans as documented.
    let (first_left, first_top, first_right, first_bottom) = unsafe { rectangle_values(first) };
    // SAFETY: The caller supplies valid four-i32 spans as documented.
    let (second_left, second_top, second_right, second_bottom) =
        unsafe { rectangle_values(second) };
    let values = [
        first_left.min(second_left),
        first_top.min(second_top),
        first_right.max(second_right),
        first_bottom.max(second_bottom),
    ];
    // SAFETY: The caller supplies a writable four-i32 output span disjoint
    // from both inputs.
    unsafe {
        for (index, value) in values.into_iter().enumerate() {
            *output.add(index) = value;
        }
    }
}

/// Apply the strict native rectangle usability rule.
///
/// # Safety
/// `rectangle` must point to four readable i32 values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_geometry_usable(rectangle: *const i32) -> bool {
    if rectangle.is_null() {
        return false;
    }
    // SAFETY: The caller supplies a readable four-i32 rectangle.
    let (left, top, right, bottom) = unsafe { rectangle_values(rectangle) };
    right > left && bottom > top
}

/// Check cursor-test preflight, allowing degenerate but non-inverted bounds.
///
/// # Safety
/// `rectangle` must point to four readable i32 values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_geometry_cursor_preflight(
    rectangle: *const i32,
) -> bool {
    if rectangle.is_null() {
        return false;
    }
    // SAFETY: The caller supplies a readable four-i32 rectangle.
    let (left, top, right, bottom) = unsafe { rectangle_values(rectangle) };
    right >= left && bottom >= top
}

/// Check whether a cursor point lies in a non-inverted rectangle inclusively.
///
/// # Safety
/// `rectangle` must point to four readable i32 values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_geometry_contains(
    rectangle: *const i32,
    x: i32,
    y: i32,
) -> bool {
    if rectangle.is_null() {
        return false;
    }
    // SAFETY: The caller supplies a readable four-i32 rectangle.
    let (left, top, right, bottom) = unsafe { rectangle_values(rectangle) };
    right >= left && bottom >= top && x >= left && x <= right && y >= top && y <= bottom
}

pub type PreferredChildCallback = unsafe extern "C" fn(*mut core::ffi::c_void, u32) -> usize;

/// Ask native code for child 1, then child 0 only when child 1 is absent.
///
/// # Safety
/// `context` must satisfy the callback's contract, and `callback` must remain
/// valid for the synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_geometry_preferred_child(
    context: *mut core::ffi::c_void,
    callback: Option<PreferredChildCallback>,
) -> usize {
    let Some(callback) = callback else {
        return 0;
    };
    // SAFETY: The caller supplies a valid synchronous callback/context pair.
    let second = unsafe { callback(context, 1) };
    if second != 0 {
        return second;
    }
    // SAFETY: The callback/context pair remains valid for this synchronous
    // fallback call; no Rust borrow is retained between calls.
    unsafe { callback(context, 0) }
}

#[cfg(test)]
extern crate std;

#[cfg(test)]
mod tests {
    use super::{
        a2fo_rs_stage9_a1_geometry_contains, a2fo_rs_stage9_a1_geometry_cursor_preflight,
        a2fo_rs_stage9_a1_geometry_preferred_child, a2fo_rs_stage9_a1_geometry_union,
        a2fo_rs_stage9_a1_geometry_usable,
    };

    #[test]
    fn rectangle_union_uses_integer_minimum_and_maximum() {
        let first = [i32::MIN, 8, -4, i32::MAX];
        let second = [i32::MAX, i32::MIN, 9, 12];
        let mut output = [0; 4];
        // SAFETY: Both inputs and the distinct output are four-i32 arrays.
        unsafe {
            a2fo_rs_stage9_a1_geometry_union(first.as_ptr(), second.as_ptr(), output.as_mut_ptr());
        }
        assert_eq!(output, [i32::MIN, i32::MIN, 9, i32::MAX]);
    }

    #[test]
    fn usability_is_strict_while_cursor_preflight_allows_degenerate_bounds() {
        let normal = [-4, -3, 0, 2];
        let vertical_line = [1, 1, 1, 9];
        let point = [3, 4, 3, 4];
        let inverted_x = [4, 0, 3, 5];
        let inverted_y = [0, 5, 4, 4];
        // SAFETY: Each input is a four-i32 array.
        unsafe {
            assert!(a2fo_rs_stage9_a1_geometry_usable(normal.as_ptr()));
            assert!(!a2fo_rs_stage9_a1_geometry_usable(vertical_line.as_ptr()));
            assert!(!a2fo_rs_stage9_a1_geometry_usable(point.as_ptr()));
            assert!(!a2fo_rs_stage9_a1_geometry_usable(inverted_x.as_ptr()));
            assert!(!a2fo_rs_stage9_a1_geometry_usable(inverted_y.as_ptr()));
            assert!(a2fo_rs_stage9_a1_geometry_cursor_preflight(normal.as_ptr()));
            assert!(a2fo_rs_stage9_a1_geometry_cursor_preflight(
                vertical_line.as_ptr()
            ));
            assert!(a2fo_rs_stage9_a1_geometry_cursor_preflight(point.as_ptr()));
            assert!(!a2fo_rs_stage9_a1_geometry_cursor_preflight(
                inverted_x.as_ptr()
            ));
            assert!(!a2fo_rs_stage9_a1_geometry_cursor_preflight(
                inverted_y.as_ptr()
            ));
        }
    }

    #[test]
    fn cursor_rectangle_contains_edges_and_rejects_outside_or_inverted_points() {
        let rectangle = [-2, 4, 5, 8];
        let degenerate = [7, 9, 7, 9];
        let inverted = [7, 9, 6, 10];
        // SAFETY: Each input is a four-i32 array.
        unsafe {
            for point in [(-2, 4), (5, 4), (-2, 8), (5, 8), (0, 6)] {
                assert!(a2fo_rs_stage9_a1_geometry_contains(
                    rectangle.as_ptr(),
                    point.0,
                    point.1,
                ));
            }
            for point in [(-3, 4), (6, 4), (0, 3), (0, 9)] {
                assert!(!a2fo_rs_stage9_a1_geometry_contains(
                    rectangle.as_ptr(),
                    point.0,
                    point.1,
                ));
            }
            assert!(a2fo_rs_stage9_a1_geometry_contains(
                degenerate.as_ptr(),
                7,
                9
            ));
            assert!(!a2fo_rs_stage9_a1_geometry_contains(
                inverted.as_ptr(),
                6,
                9
            ));
        }
    }

    struct ChildState {
        values: [usize; 2],
        calls: [u32; 2],
        call_count: usize,
    }

    unsafe extern "C" fn child_callback(
        context: *mut core::ffi::c_void,
        child_index: u32,
    ) -> usize {
        // SAFETY: Tests pass a live exclusive pointer to ChildState.
        let state = unsafe { &mut *context.cast::<ChildState>() };
        state.calls[state.call_count] = child_index;
        state.call_count += 1;
        state.values[child_index as usize]
    }

    #[test]
    fn preferred_child_checks_second_then_first_only_when_needed() {
        let mut state = ChildState {
            values: [11, 22],
            calls: [u32::MAX; 2],
            call_count: 0,
        };
        // SAFETY: The callback and context point to this live test state.
        let selected = unsafe {
            a2fo_rs_stage9_a1_geometry_preferred_child(
                (&mut state as *mut ChildState).cast(),
                Some(child_callback),
            )
        };
        assert_eq!(selected, 22);
        assert_eq!(state.calls[..state.call_count], [1]);

        state.values[1] = 0;
        state.calls = [u32::MAX; 2];
        state.call_count = 0;
        // SAFETY: The callback and context point to this live test state.
        let selected = unsafe {
            a2fo_rs_stage9_a1_geometry_preferred_child(
                (&mut state as *mut ChildState).cast(),
                Some(child_callback),
            )
        };
        assert_eq!(selected, 11);
        assert_eq!(state.calls[..state.call_count], [1, 0]);

        state.values[0] = 0;
        state.calls = [u32::MAX; 2];
        state.call_count = 0;
        // SAFETY: The callback and context point to this live test state.
        let selected = unsafe {
            a2fo_rs_stage9_a1_geometry_preferred_child(
                (&mut state as *mut ChildState).cast(),
                Some(child_callback),
            )
        };
        assert_eq!(selected, 0);
        assert_eq!(state.calls[..state.call_count], [1, 0]);
    }
}
