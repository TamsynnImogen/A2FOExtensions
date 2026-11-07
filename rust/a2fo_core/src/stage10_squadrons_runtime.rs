//! Residual squadron traversal and integer policy. Native adapters retain
//! handles, checked reads, locks, storage, exception scopes and provider calls.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
fn op(step: Step, context: *mut c_void, code: u32, arg: usize) -> Option<usize> {
    // SAFETY: callers keep a valid synchronous callback/context for this call.
    let result = unsafe { step(context, code, arg) };
    (result != usize::MAX).then_some(result)
}
/// # Safety
/// The callback owns a valid bar accumulation context and never unwinds.
/// Ops: 0=count, 1=resolve/admit member, 2=prepare native provider values,
/// 3=invoke provider, 4=accumulate, 5=read/sanitize colour weight,
/// 6=restore saved native Craft pointer, 7=divide colour, 8=copy output.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_bar(
    context: *mut c_void,
    step: Option<Step>,
    colour: u32,
) {
    let Some(step) = step else { return };
    let mut index = 0;
    loop {
        let Some(count) = op(step, context, 0, 0) else {
            return;
        };
        if index >= count {
            break;
        }
        let Some(admitted) = op(step, context, 1, index) else {
            return;
        };
        if admitted == 1 {
            if colour != 0 && op(step, context, 5, 0).is_none() {
                return;
            }
            for code in 2..=4 {
                if op(step, context, code, 0).is_none() {
                    return;
                }
            }
        }
        index += 1;
    }
    if op(step, context, 6, 0).is_none() {
        return;
    }
    if colour != 0 && op(step, context, 7, 0).is_none() {
        return;
    }
    op(step, context, 8, 0);
}
/// # Safety
/// Callback exposes bounded count and equality at each index, never unwinds.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_scan(
    context: *mut c_void,
    step: Option<Step>,
) -> u8 {
    let Some(step) = step else { return 0 };
    let Some(count) = op(step, context, 0, 0) else {
        return 0;
    };
    for index in 0..count {
        let Some(equal) = op(step, context, 1, index) else {
            return 0;
        };
        if equal == 1 {
            return 1;
        }
    }
    0
}
/// # Safety
/// Callback holds the native registry lock and a live squad/vector. Ops:
/// 0=count,1=nonzero member other than clicked handle,2=push native member.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_snapshot(
    context: *mut c_void,
    step: Option<Step>,
) {
    let Some(step) = step else { return };
    let Some(count) = op(step, context, 0, 0) else {
        return;
    };
    for index in 0..count {
        let Some(admitted) = op(step, context, 1, index) else {
            return;
        };
        if admitted == 1 && op(step, context, 2, index).is_none() {
            return;
        }
    }
}
/// # Safety
/// Data is an initialized native string byte range; output is a writable i32.
/// Matches fully consumed decimal from_chars followed by a nonnegative check.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_cost(
    data: *const u8,
    size: usize,
    output: *mut i32,
) -> u8 {
    if size == 0 || data.is_null() || output.is_null() {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(data, size) };
    let negative = bytes[0] == b'-';
    let mut index = usize::from(negative);
    if index == size {
        return 0;
    }
    let mut value = 0i32;
    while index < size {
        let digit = bytes[index].wrapping_sub(b'0');
        if digit > 9 || (negative && digit != 0) {
            return 0;
        }
        let Some(next) = value
            .checked_mul(10)
            .and_then(|v| v.checked_add(digit as i32))
        else {
            return 0;
        };
        value = next;
        index += 1;
    }
    unsafe { output.write(value) };
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_squadrons_project(team: i32, project: u32) -> u8 {
    u8::from((0..=63).contains(&team) && (1..=8192).contains(&project))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_squadrons_active(value: i32) -> u8 {
    u8::from(value > 0)
}
#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;
    struct Context {
        calls: Vec<(u32, usize)>,
        count: usize,
    }
    unsafe extern "C" fn step(opaque: *mut c_void, code: u32, arg: usize) -> usize {
        let c = unsafe { &mut *opaque.cast::<Context>() };
        c.calls.push((code, arg));
        if code == 0 {
            c.count
        } else if code == 1 {
            usize::from(arg == 1)
        } else {
            1
        }
    }
    #[test]
    fn ordered_bar_provider_and_restore() {
        let mut c = Context {
            calls: Vec::new(),
            count: 2,
        };
        unsafe { a2fo_rs_stage10_squadrons_bar((&mut c as *mut Context).cast(), Some(step), 1) };
        assert_eq!(
            c.calls,
            [
                (0, 0),
                (1, 0),
                (0, 0),
                (1, 1),
                (5, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (0, 0),
                (6, 0),
                (7, 0),
                (8, 0)
            ]
        );
    }
    #[test]
    fn scan_stops_at_first_match_and_snapshot_keeps_slot_order() {
        let mut c = Context {
            calls: Vec::new(),
            count: 3,
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_squadrons_scan((&mut c as *mut Context).cast(), Some(step)) },
            1
        );
        assert_eq!(c.calls, [(0, 0), (1, 0), (1, 1)]);
        c.calls.clear();
        unsafe { a2fo_rs_stage10_squadrons_snapshot((&mut c as *mut Context).cast(), Some(step)) };
        assert_eq!(c.calls, [(0, 0), (1, 0), (1, 1), (2, 1), (1, 2)]);
    }
    #[test]
    fn callback_error_stops_before_any_later_read_or_provider() {
        unsafe extern "C" fn fail(opaque: *mut c_void, code: u32, arg: usize) -> usize {
            let c = unsafe { &mut *opaque.cast::<Context>() };
            c.calls.push((code, arg));
            if code == 0 { c.count } else { usize::MAX }
        }
        let mut c = Context {
            calls: Vec::new(),
            count: 3,
        };
        unsafe { a2fo_rs_stage10_squadrons_bar((&mut c as *mut Context).cast(), Some(fail), 0) };
        assert_eq!(c.calls, [(0, 0), (1, 0)]);
        c.calls.clear();
        assert_eq!(
            unsafe { a2fo_rs_stage10_squadrons_scan((&mut c as *mut Context).cast(), Some(fail)) },
            0
        );
        assert_eq!(c.calls, [(0, 0), (1, 0)]);
    }
    #[test]
    fn decimal_cost_matches_strict_charconv_admission() {
        for (text, expected) in [
            ("0", Some(0)),
            ("-000", Some(0)),
            ("2147483647", Some(i32::MAX)),
            ("2147483648", None),
            ("-1", None),
            ("+1", None),
            (" 1", None),
            ("1 ", None),
            ("1\0", None),
            ("", None),
            ("-", None),
        ] {
            let mut result = 77;
            let valid =
                unsafe { a2fo_rs_stage10_squadrons_cost(text.as_ptr(), text.len(), &mut result) };
            assert_eq!(valid != 0, expected.is_some());
            assert_eq!(result, expected.unwrap_or(77));
        }
    }
    #[test]
    fn bounded_native_project_and_counter_admission() {
        assert_eq!(a2fo_rs_stage10_squadrons_project(-1, 1), 0);
        assert_eq!(a2fo_rs_stage10_squadrons_project(63, 8192), 1);
        assert_eq!(a2fo_rs_stage10_squadrons_project(64, 8192), 0);
        assert_eq!(a2fo_rs_stage10_squadrons_project(0, 8193), 0);
        assert_eq!(a2fo_rs_stage10_squadrons_active(i32::MIN), 0);
        assert_eq!(a2fo_rs_stage10_squadrons_active(1), 1);
    }
}
