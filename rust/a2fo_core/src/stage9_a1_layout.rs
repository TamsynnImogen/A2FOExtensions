//! Portable A1 layout choices using native-owned four-i32 rectangles.
//!
//! Rectangle spans contain four consecutive i32 values ordered left, top,
//! right, bottom. Output spans contain four writable i32 values. The native
//! caller owns all memory and passes it only for the duration of each call.

unsafe fn read_rectangle(rectangle: *const i32) -> [i32; 4] {
    // SAFETY: The caller documents a readable four-i32 span.
    unsafe {
        [
            *rectangle,
            *rectangle.add(1),
            *rectangle.add(2),
            *rectangle.add(3),
        ]
    }
}

unsafe fn write_rectangle(output: *mut i32, rectangle: [i32; 4]) {
    // SAFETY: The caller documents a writable four-i32 span.
    unsafe {
        for (index, value) in rectangle.into_iter().enumerate() {
            *output.add(index) = value;
        }
    }
}

fn union(first: [i32; 4], second: [i32; 4]) -> [i32; 4] {
    [
        first[0].min(second[0]),
        first[1].min(second[1]),
        first[2].max(second[2]),
        first[3].max(second[3]),
    ]
}

/// Plan crew component/text rectangles and the strict crew-dot usability flag.
///
/// # Safety
/// Each rectangle pointer must refer to four readable i32 values; output
/// pointers must refer to four writable i32 values and the flag to one
/// writable byte. Output spans must not overlap inputs or each other.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_layout_crew(
    crew_rectangle: *const i32,
    crew_dot_rectangle: *const i32,
    hidden_rectangle: *const i32,
    custom_identity_text: u8,
    component_output: *mut i32,
    text_output: *mut i32,
    has_crew_dot_output: *mut u8,
) -> bool {
    if crew_rectangle.is_null()
        || crew_dot_rectangle.is_null()
        || hidden_rectangle.is_null()
        || component_output.is_null()
        || text_output.is_null()
        || has_crew_dot_output.is_null()
    {
        return false;
    }
    // SAFETY: All six spans are checked non-null and valid per this function's
    // caller contract.
    let crew = unsafe { read_rectangle(crew_rectangle) };
    // SAFETY: Same documented span contract.
    let dot = unsafe { read_rectangle(crew_dot_rectangle) };
    // SAFETY: Same documented span contract.
    let hidden = unsafe { read_rectangle(hidden_rectangle) };
    let has_dot = dot[2] > dot[0] && dot[3] > dot[1];
    let component = if has_dot { union(crew, dot) } else { crew };
    let text = if custom_identity_text != 0 {
        hidden
    } else {
        crew
    };
    // SAFETY: Caller guarantees disjoint writable output spans.
    unsafe {
        write_rectangle(component_output, component);
        write_rectangle(text_output, text);
        *has_crew_dot_output = u8::from(has_dot);
    }
    true
}

/// Select the visible or hidden native rectangle according to a C++ choice.
///
/// # Safety
/// Both inputs must point to four readable i32 values and output to four
/// writable i32 values; output must not overlap either input.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_layout_select_rectangle(
    visible_rectangle: *const i32,
    hidden_rectangle: *const i32,
    use_hidden: u8,
    output: *mut i32,
) -> bool {
    if visible_rectangle.is_null() || hidden_rectangle.is_null() || output.is_null() {
        return false;
    }
    // SAFETY: The caller documents the rectangle spans.
    let selected = unsafe {
        read_rectangle(if use_hidden != 0 {
            hidden_rectangle
        } else {
            visible_rectangle
        })
    };
    // SAFETY: The caller documents the output span.
    unsafe { write_rectangle(output, selected) };
    true
}

/// Translate a local rectangle into panel screen coordinates.
///
/// # Safety
/// Inputs must point to four readable i32 values and output to four writable
/// i32 values; output must not overlap either input.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_layout_screen_rectangle(
    panel_rectangle: *const i32,
    local_rectangle: *const i32,
    output: *mut i32,
) -> bool {
    if panel_rectangle.is_null() || local_rectangle.is_null() || output.is_null() {
        return false;
    }
    // SAFETY: The caller documents the rectangle spans.
    let panel = unsafe { read_rectangle(panel_rectangle) };
    // SAFETY: The caller documents the rectangle spans.
    let local = unsafe { read_rectangle(local_rectangle) };
    let screen = [
        panel[0] + local[0],
        panel[1] + local[1],
        panel[0] + local[2],
        panel[1] + local[3],
    ];
    // SAFETY: The caller documents the output span.
    unsafe { write_rectangle(output, screen) };
    true
}

/// Translate a screen rectangle into panel-local coordinates.
///
/// # Safety
/// Inputs must point to four readable i32 values and output to four writable
/// i32 values; output must not overlap either input.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_layout_local_rectangle(
    screen_rectangle: *const i32,
    panel_rectangle: *const i32,
    output: *mut i32,
) -> bool {
    if screen_rectangle.is_null() || panel_rectangle.is_null() || output.is_null() {
        return false;
    }
    // SAFETY: The caller documents the rectangle spans.
    let screen = unsafe { read_rectangle(screen_rectangle) };
    // SAFETY: The caller documents the rectangle spans.
    let panel = unsafe { read_rectangle(panel_rectangle) };
    let local = [
        screen[0] - panel[0],
        screen[1] - panel[1],
        screen[2] - panel[0],
        screen[3] - panel[1],
    ];
    // SAFETY: The caller documents the output span.
    unsafe { write_rectangle(output, local) };
    true
}

/// Select a queue slot's translated screen rectangle or the hidden fallback.
///
/// The return value is true exactly when `index` selects a supplied screen
/// rectangle. A false result still writes the hidden rectangle.
///
/// # Safety
/// When `rectangle_count` is nonzero, `screen_rectangles` must point to that
/// many consecutive four-i32 rectangles. The other inputs must point to four
/// readable i32 values and output to four writable values. Output must not
/// overlap any input span.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_layout_queue_rectangle(
    screen_rectangles: *const i32,
    rectangle_count: usize,
    index: usize,
    ship_display_rectangle: *const i32,
    hidden_rectangle: *const i32,
    output: *mut i32,
) -> bool {
    if (rectangle_count != 0 && screen_rectangles.is_null())
        || ship_display_rectangle.is_null()
        || hidden_rectangle.is_null()
        || output.is_null()
    {
        return false;
    }
    if index >= rectangle_count {
        // SAFETY: The caller documents the hidden rectangle and output spans.
        let hidden = unsafe { read_rectangle(hidden_rectangle) };
        unsafe { write_rectangle(output, hidden) };
        return false;
    }
    // SAFETY: The caller guarantees a contiguous array of `rectangle_count`
    // four-i32 rectangles and `index < rectangle_count`.
    let screen = unsafe { read_rectangle(screen_rectangles.add(index * 4)) };
    // SAFETY: The caller documents the ship-display rectangle span.
    let origin = unsafe { read_rectangle(ship_display_rectangle) };
    let local = [
        screen[0] - origin[0],
        screen[1] - origin[1],
        screen[2] - origin[0],
        screen[3] - origin[1],
    ];
    // SAFETY: The caller documents the output span.
    unsafe { write_rectangle(output, local) };
    true
}

#[cfg(test)]
extern crate std;

#[cfg(test)]
mod tests {
    use super::{
        a2fo_rs_stage9_a1_layout_crew, a2fo_rs_stage9_a1_layout_local_rectangle,
        a2fo_rs_stage9_a1_layout_queue_rectangle, a2fo_rs_stage9_a1_layout_screen_rectangle,
        a2fo_rs_stage9_a1_layout_select_rectangle,
    };

    #[test]
    fn crew_dot_union_and_custom_text_hide_choices() {
        let crew = [12, 14, 50, 28];
        let dot = [7, 10, 15, 18];
        let hidden = [-1000, -1000, -1000, -1000];
        let mut component = [0; 4];
        let mut text = [0; 4];
        let mut has_dot = 0;
        // SAFETY: All pointers describe live, separate four-i32 arrays.
        assert!(unsafe {
            a2fo_rs_stage9_a1_layout_crew(
                crew.as_ptr(),
                dot.as_ptr(),
                hidden.as_ptr(),
                1,
                component.as_mut_ptr(),
                text.as_mut_ptr(),
                &mut has_dot,
            )
        });
        assert_eq!(component, [7, 10, 50, 28]);
        assert_eq!(text, hidden);
        assert_eq!(has_dot, 1);

        let degenerate_dot = [15, 10, 15, 18];
        // SAFETY: All pointers describe live, separate four-i32 arrays.
        unsafe {
            a2fo_rs_stage9_a1_layout_crew(
                crew.as_ptr(),
                degenerate_dot.as_ptr(),
                hidden.as_ptr(),
                0,
                component.as_mut_ptr(),
                text.as_mut_ptr(),
                &mut has_dot,
            );
        }
        assert_eq!(component, crew);
        assert_eq!(text, crew);
        assert_eq!(has_dot, 0);
    }

    #[test]
    fn identity_rectangle_selector_preserves_visible_and_hidden_values() {
        let visible = [2, 3, 12, 13];
        let hidden = [-1, -1, -1, -1];
        let mut output = [0; 4];
        // SAFETY: Inputs and output are separate four-i32 arrays.
        unsafe {
            assert!(a2fo_rs_stage9_a1_layout_select_rectangle(
                visible.as_ptr(),
                hidden.as_ptr(),
                0,
                output.as_mut_ptr()
            ));
            assert_eq!(output, visible);
            assert!(a2fo_rs_stage9_a1_layout_select_rectangle(
                visible.as_ptr(),
                hidden.as_ptr(),
                1,
                output.as_mut_ptr()
            ));
            assert_eq!(output, hidden);
        }
    }

    #[test]
    fn screen_and_local_coordinate_plans_keep_axis_order() {
        let panel = [100, 40, 500, 300];
        let local = [5, 7, 21, 30];
        let expected_screen = [105, 47, 121, 70];
        let mut output = [0; 4];
        // SAFETY: Inputs and output are separate four-i32 arrays.
        unsafe {
            assert!(a2fo_rs_stage9_a1_layout_screen_rectangle(
                panel.as_ptr(),
                local.as_ptr(),
                output.as_mut_ptr()
            ));
            assert_eq!(output, expected_screen);
            assert!(a2fo_rs_stage9_a1_layout_local_rectangle(
                expected_screen.as_ptr(),
                panel.as_ptr(),
                output.as_mut_ptr()
            ));
            assert_eq!(output, local);
        }
    }

    #[test]
    fn queue_planner_translates_valid_slots_and_hides_excess_slots() {
        let screen = [100, 40, 120, 60, 125, 40, 145, 60];
        let origin = [90, 30, 400, 300];
        let hidden = [-20, -20, -20, -20];
        let mut output = [0; 4];
        // SAFETY: The screen buffer holds two contiguous rectangles; all
        // other inputs and output are separate four-i32 arrays.
        unsafe {
            assert!(a2fo_rs_stage9_a1_layout_queue_rectangle(
                screen.as_ptr(),
                2,
                1,
                origin.as_ptr(),
                hidden.as_ptr(),
                output.as_mut_ptr()
            ));
            assert_eq!(output, [35, 10, 55, 30]);
            assert!(!a2fo_rs_stage9_a1_layout_queue_rectangle(
                screen.as_ptr(),
                2,
                2,
                origin.as_ptr(),
                hidden.as_ptr(),
                output.as_mut_ptr()
            ));
            assert_eq!(output, hidden);
        }
    }
}
