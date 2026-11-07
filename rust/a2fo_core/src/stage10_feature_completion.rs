//! Ordered FeaturePack chain scan; native callbacks own vector storage and
//! pointer equality. The scan stops at the first match or callback failure.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> i64;

/// # Safety
/// `context` and `step` remain valid for this synchronous call. The callback
/// does not unwind: it captures native exceptions and returns `i64::MIN`.
/// Operation 0 returns the chain length; operation 1 tests equality at index.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_feature_chain_contains(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let count = unsafe { step(context, 0, 0) };
    if count == i64::MIN || count < 0 {
        return false;
    }
    for index in 0..count as usize {
        let equal = unsafe { step(context, 1, index) };
        if equal == i64::MIN {
            return false;
        }
        if equal != 0 {
            return true;
        }
    }
    false
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Context {
        values: Vec<bool>,
        calls: Vec<(u32, usize)>,
        error_at: Option<usize>,
    }

    unsafe extern "C" fn step(opaque: *mut c_void, op: u32, index: usize) -> i64 {
        let context = unsafe { &mut *opaque.cast::<Context>() };
        context.calls.push((op, index));
        if op == 0 {
            return context.values.len() as i64;
        }
        if context.error_at == Some(index) {
            return i64::MIN;
        }
        i64::from(context.values[index])
    }

    fn scan(context: &mut Context) -> bool {
        unsafe {
            a2fo_rs_stage10_feature_chain_contains((context as *mut Context).cast(), Some(step))
        }
    }

    #[test]
    fn scans_in_order_and_stops_at_first_match() {
        let mut context = Context {
            values: vec![false, true, true],
            calls: vec![],
            error_at: None,
        };
        assert!(scan(&mut context));
        assert_eq!(context.calls, [(0, 0), (1, 0), (1, 1)]);
    }

    #[test]
    fn empty_and_absent_chains() {
        let mut context = Context {
            values: vec![],
            calls: vec![],
            error_at: None,
        };
        assert!(!scan(&mut context));
        assert_eq!(context.calls, [(0, 0)]);
        context.values = vec![false, false];
        context.calls.clear();
        assert!(!scan(&mut context));
        assert_eq!(context.calls, [(0, 0), (1, 0), (1, 1)]);
    }

    #[test]
    fn callback_error_stops_without_later_reads() {
        let mut context = Context {
            values: vec![false, true, true],
            calls: vec![],
            error_at: Some(1),
        };
        assert!(!scan(&mut context));
        assert_eq!(context.calls, [(0, 0), (1, 0), (1, 1)]);
    }

    #[test]
    fn missing_callback_returns_false() {
        assert!(!unsafe { a2fo_rs_stage10_feature_chain_contains(core::ptr::null_mut(), None) });
    }
}
