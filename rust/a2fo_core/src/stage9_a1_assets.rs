use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct View {
    bytes: *const u8,
    size: usize,
    width: i32,
    height: i32,
    indexed: u32,
    legacy: u32,
}

impl View {
    const fn empty() -> Self {
        Self {
            bytes: core::ptr::null(),
            size: 0,
            width: 0,
            height: 0,
            indexed: 0,
            legacy: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CursorDecision {
    standard_found: u8,
    standard_invalid: u8,
    select_found: u8,
    select_invalid: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct TeamPlan {
    parsed_files: u32,
    indexed_values: u32,
    legacy_aliases: u32,
    resolved_colors: u32,
    active: u8,
}

pub type Step = unsafe extern "C" fn(*mut c_void, u32, u32, u32, *mut View) -> u8;

fn valid_root_count(roots: u32) -> bool {
    (1..=4096).contains(&roots)
}

fn first_content(line: &[u8]) -> Option<usize> {
    line.iter()
        .position(|byte| !matches!(*byte, b' ' | b'\t' | b'\r' | b'\n'))
}

fn contains_cursor_flag(line: &[u8]) -> bool {
    line.windows(b"@cursor".len())
        .any(|part| part == b"@cursor")
}

fn cursor_invalid(width: i32, height: i32, normalized: &[u8]) -> bool {
    !contains_cursor_flag(normalized) || width > 40 || height > 40 || width <= 0 || height <= 0
}

/// # Safety
/// A nonempty view must contain a readable `size`-byte span for the returned
/// slice's lifetime. The native callback owns that span.
unsafe fn view_bytes(view: &View) -> &[u8] {
    if view.size == 0 || view.bytes.is_null() {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(view.bytes, view.size) }
    }
}

/// # Safety
/// `context` must be valid for every synchronous `step` call. The callback
/// must follow the native operation contract and must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_cursor_roots(
    roots: u32,
    context: *mut c_void,
    step: Step,
) {
    if !valid_root_count(roots) {
        return;
    }
    for index in 0..roots {
        let mut view = View::empty();
        let begin = unsafe { step(context, 0, index, 0, &mut view) };
        if begin == 2 {
            return;
        }
        if begin == 0 {
            continue;
        }
        for candidate in 0..2 {
            let found = unsafe { step(context, 1, index, candidate, &mut view) };
            if found == 2 {
                return;
            }
            if found == 1 {
                break;
            }
        }
        if unsafe { step(context, 2, index, 0, &mut view) } == 2 {
            return;
        }
    }
}

/// # Safety
/// `output` must be writable and separate from callback-owned storage.
/// `context` and `step` must remain valid for synchronous calls; each nonempty
/// view returned by `step` must cover readable bytes until consumed before
/// the next callback. The callback must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_cursor_lines(
    context: *mut c_void,
    step: Step,
    output: *mut CursorDecision,
) {
    let Some(output) = (unsafe { output.as_mut() }) else {
        return;
    };
    let mut result = CursorDecision::default();
    loop {
        let mut view = View::empty();
        let next = unsafe { step(context, 0, 0, 0, &mut view) };
        if next == 2 {
            break;
        }
        if next == 0 {
            break;
        }
        let first = {
            let line = unsafe { view_bytes(&view) };
            first_content(line).filter(|&index| line[index] != b'#')
        };
        if let Some(first) = first {
            let parsed = unsafe { step(context, 1, 0, first as u32, &mut view) };
            if parsed == 2 {
                break;
            }
            if parsed == 1 {
                // Consume the name before the normalization callback mutates
                // native strings. Do not retain a Rust slice across it.
                let kind = {
                    let name = unsafe { view_bytes(&view) };
                    if name == b"standard_cursor" {
                        1
                    } else if name == b"c_select" {
                        2
                    } else {
                        0
                    }
                };
                if kind != 0 {
                    let normalized = unsafe { step(context, 2, 0, 0, &mut view) };
                    if normalized == 2 {
                        break;
                    }
                    if normalized == 1 {
                        let invalid = u8::from(cursor_invalid(view.width, view.height, unsafe {
                            view_bytes(&view)
                        }));
                        if kind == 1 {
                            result.standard_found = 1;
                            result.standard_invalid = invalid;
                        } else {
                            result.select_found = 1;
                            result.select_invalid = invalid;
                        }
                    }
                }
            }
        }
        if unsafe { step(context, 3, 0, 0, &mut view) } == 2 {
            break;
        }
    }
    *output = result;
}

/// # Safety
/// `context` and `step` must remain valid for all synchronous candidate
/// callbacks, which must obey the native operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_team_find(context: *mut c_void, step: Step) {
    let mut view = View::empty();
    for candidate in 0..5 {
        let status = unsafe { step(context, 0, candidate, 0, &mut view) };
        if status != 0 {
            break;
        }
    }
}

/// # Safety
/// `output` must be writable and separate from callback-owned storage.
/// `context` and `step` must remain valid for all synchronous calls; the
/// callback must obey its native operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_team_load(
    roots: u32,
    context: *mut c_void,
    step: Step,
    output: *mut TeamPlan,
) {
    let Some(output) = (unsafe { output.as_mut() }) else {
        return;
    };
    let mut plan = TeamPlan::default();
    if !valid_root_count(roots) {
        *output = plan;
        return;
    }
    let mut failed = false;
    for index in 0..roots {
        let mut view = View::empty();
        match unsafe { step(context, 0, index, 0, &mut view) } {
            2 => {
                failed = true;
                break;
            }
            1 => {
                plan.indexed_values = plan.indexed_values.wrapping_add(view.indexed);
                plan.legacy_aliases = plan.legacy_aliases.wrapping_add(view.legacy);
                plan.parsed_files = plan.parsed_files.wrapping_add(1);
            }
            _ => {}
        }
    }
    if !failed && plan.legacy_aliases != 0 {
        plan.active = 1;
        for index in 0..16 {
            let mut view = View::empty();
            match unsafe { step(context, 1, index, 0, &mut view) } {
                2 => {
                    failed = true;
                    break;
                }
                1 => plan.resolved_colors += 1,
                _ => {}
            }
        }
    }
    if failed {
        return;
    }
    *output = plan;
}

/// # Safety
/// `context` and `step` must remain valid for all synchronous slot calls;
/// the callback must obey its native operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_team_apply(
    slots: u32,
    context: *mut c_void,
    step: Step,
) -> u32 {
    let mut applied = 0u32;
    for index in 0..slots {
        let mut view = View::empty();
        match unsafe { step(context, 0, index, 0, &mut view) } {
            2 => return applied,
            1 => applied = applied.wrapping_add(1),
            _ => {}
        }
    }
    applied
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_team_log_first8(pass: i32) -> u8 {
    u8::from(pass <= 8)
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate std;
    use self::std::vec::Vec;

    #[test]
    fn cursor_boundaries_and_last_row_inputs() {
        assert_eq!(first_content(b" \t\r\n#comment"), Some(4));
        assert_eq!(first_content(b" \t\r\n"), None);
        assert!(!cursor_invalid(40, 40, b"name @cursor"));
        for (width, height) in [(0, 40), (41, 40), (40, -1), (40, 41)] {
            assert!(cursor_invalid(width, height, b"@cursor"));
        }
        assert!(cursor_invalid(40, 40, b"@CURSOR"));
        assert!(!cursor_invalid(1, 1, b"prefix@cursor suffix"));
        assert!(!valid_root_count(0));
        assert!(valid_root_count(1));
        assert!(valid_root_count(4096));
        assert!(!valid_root_count(4097));
        assert_eq!(a2fo_rs_stage9_a1_team_log_first8(i32::MIN), 1);
        assert_eq!(a2fo_rs_stage9_a1_team_log_first8(8), 1);
        assert_eq!(a2fo_rs_stage9_a1_team_log_first8(9), 0);
    }

    struct RootTrace {
        calls: Vec<(u32, u32, u32)>,
    }
    unsafe extern "C" fn root_step(
        context: *mut c_void,
        operation: u32,
        root: u32,
        candidate: u32,
        _: *mut View,
    ) -> u8 {
        let trace = unsafe { &mut *context.cast::<RootTrace>() };
        trace.calls.push((operation, root, candidate));
        match operation {
            0 => u8::from(root != 1),
            1 => u8::from(root == 2 && candidate == 0),
            _ => 0,
        }
    }

    #[test]
    fn roots_are_low_to_high_and_stop_each_candidate_list_at_first_hit() {
        let mut trace = RootTrace { calls: Vec::new() };
        unsafe {
            a2fo_rs_stage9_a1_cursor_roots(3, (&mut trace as *mut RootTrace).cast(), root_step)
        };
        assert_eq!(
            trace.calls,
            [
                (0, 0, 0),
                (1, 0, 0),
                (1, 0, 1),
                (2, 0, 0),
                (0, 1, 0),
                (0, 2, 0),
                (1, 2, 0),
                (2, 2, 0),
            ]
        );
        trace.calls.clear();
        unsafe {
            a2fo_rs_stage9_a1_cursor_roots(4097, (&mut trace as *mut RootTrace).cast(), root_step)
        };
        assert!(trace.calls.is_empty());
    }

    struct LineTrace {
        rows: [&'static [u8]; 4],
        next: usize,
        name: &'static [u8],
        normalized: &'static [u8],
    }
    unsafe extern "C" fn line_step(
        context: *mut c_void,
        operation: u32,
        _: u32,
        _: u32,
        output: *mut View,
    ) -> u8 {
        let c = unsafe { &mut *context.cast::<LineTrace>() };
        let view = unsafe { &mut *output };
        match operation {
            0 => {
                if c.next == c.rows.len() {
                    return 0;
                }
                let row = c.rows[c.next];
                c.next += 1;
                view.bytes = row.as_ptr();
                view.size = row.len();
                1
            }
            1 => {
                c.name = if c.next == 2 {
                    b"standard_cursor"
                } else {
                    b"c_select"
                };
                view.bytes = c.name.as_ptr();
                view.size = c.name.len();
                1
            }
            2 => {
                c.normalized = if c.next == 4 { b"row @cursor" } else { b"row" };
                view.bytes = c.normalized.as_ptr();
                view.size = c.normalized.len();
                view.width = 40;
                view.height = 1;
                1
            }
            _ => 0,
        }
    }

    #[test]
    fn cursor_last_matching_row_decides_validity() {
        let mut trace = LineTrace {
            rows: [b" # comment", b"standard_cursor", b"c_select", b"c_select"],
            next: 0,
            name: b"",
            normalized: b"",
        };
        let mut result = CursorDecision::default();
        unsafe {
            a2fo_rs_stage9_a1_cursor_lines(
                (&mut trace as *mut LineTrace).cast(),
                line_step,
                &mut result,
            )
        };
        assert_eq!(
            (
                result.standard_found,
                result.standard_invalid,
                result.select_found,
                result.select_invalid
            ),
            (1, 1, 1, 0)
        );
    }

    struct TeamTrace {
        calls: Vec<(u32, u32)>,
    }
    unsafe extern "C" fn team_step(
        context: *mut c_void,
        operation: u32,
        index: u32,
        _: u32,
        output: *mut View,
    ) -> u8 {
        let c = unsafe { &mut *context.cast::<TeamTrace>() };
        c.calls.push((operation, index));
        if operation == 0 {
            if index == 1 {
                let view = unsafe { &mut *output };
                view.indexed = u32::MAX;
                view.legacy = 1;
                return 1;
            }
            if index == 2 {
                let view = unsafe { &mut *output };
                view.indexed = 2;
                view.legacy = 1;
                return 1;
            }
            return 0;
        }
        u8::from(index == 0 || index == 15)
    }

    #[test]
    fn team_layers_wrap_counts_and_scan_all_present_slots() {
        let mut trace = TeamTrace { calls: Vec::new() };
        let mut plan = TeamPlan::default();
        unsafe {
            a2fo_rs_stage9_a1_team_load(
                3,
                (&mut trace as *mut TeamTrace).cast(),
                team_step,
                &mut plan,
            )
        };
        assert_eq!(
            (
                plan.parsed_files,
                plan.indexed_values,
                plan.legacy_aliases,
                plan.resolved_colors,
                plan.active
            ),
            (2, 1, 2, 2, 1)
        );
        assert_eq!(&trace.calls[..3], &[(0, 0), (0, 1), (0, 2)]);
        assert_eq!(trace.calls.len(), 19);
        assert_eq!(trace.calls[18], (1, 15));
        trace.calls.clear();
        let applied = unsafe {
            a2fo_rs_stage9_a1_team_apply(16, (&mut trace as *mut TeamTrace).cast(), team_step)
        };
        assert_eq!(applied, 2);
        assert_eq!(trace.calls.len(), 16);
    }
}
