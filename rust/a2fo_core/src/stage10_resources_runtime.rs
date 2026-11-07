//! Remaining resource workflows. Operation results are metadata; native-owned
//! contexts retain POD values, strings, locks, containers and exception scopes.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
struct Driver {
    context: *mut c_void,
    step: Step,
}
impl Driver {
    fn op(&self, operation: u32, argument: usize) -> Option<usize> {
        // SAFETY: exported callers provide a synchronous, non-unwinding native
        // primitive and its live context for the entire workflow.
        let result = unsafe { (self.step)(self.context, operation, argument) };
        (result != usize::MAX).then_some(result)
    }
}
fn transaction(d: &Driver, mode: u32) -> Option<bool> {
    match mode {
        0 => {
            // admission: costs before the lazy producer team read
            if d.op(0, 0)? == 0 {
                return Some(false);
            }
            if d.op(1, 0)? != 0 {
                return Some(true);
            }
            d.op(2, 0)?;
            if d.op(3, 0)? != 0 {
                return Some(true);
            }
            if d.op(4, 0)? == 0 {
                return Some(false);
            }
            d.op(5, 0)?;
            Some(d.op(6, 0)? != 0)
        }
        1 => {
            // commit: team first, then costs/snapshot, then locked recheck
            if d.op(0, 0)? == 0 || d.op(1, 0)? != 0 || d.op(4, 0)? == 0 {
                return Some(false);
            }
            d.op(2, 0)?;
            d.op(5, 0)?;
            d.op(7, 0)?;
            Some(true)
        }
        2 => {
            // refunds always admit producer dispatch, even when no credit
            if d.op(0, 0)? == 0 {
                return Some(true);
            }
            let mut kind = 0u32;
            d.op(8, (&mut kind as *mut u32) as usize)?;
            if !(5..=7).contains(&kind) {
                return Some(true);
            }
            if d.op(1, 0)? != 0 || d.op(4, 0)? == 0 || d.op(9, 0)? == 0 {
                return Some(true);
            }
            d.op(2, 0)?;
            d.op(5, 0)?;
            d.op(7, 0)?;
            Some(true)
        }
        _ => Some(false),
    }
}
fn locked(d: &Driver, mode: u32) -> Option<bool> {
    match mode {
        0 => {
            if d.op(0, 0)? == 0 {
                return Some(false);
            }
            d.op(1, 0)?;
            Some(true)
        }
        1 => {
            d.op(2, 0)?;
            Some(true)
        }
        _ => Some(false),
    }
}
fn public(d: &Driver, kind: u32) -> Option<bool> {
    if kind > 4 || d.op(0, 0)? == 0 {
        return Some(false);
    }
    if kind == 4 {
        if d.op(1, 0)? >= 10 || d.op(4, 0)? >= 4 {
            return Some(false);
        }
        d.op(10, 0)?;
        return Some(true);
    }
    if matches!(kind, 1 | 2) && d.op(3, 0)? == 0 {
        return Some(false);
    }
    if kind == 1 && d.op(2, 0)? == 0 {
        return Some(false);
    }
    let resource = d.op(1, 0)?;
    if !(6..10).contains(&resource) {
        return Some(false);
    }
    d.op(5, resource - 6)?;
    match kind {
        0 => {
            d.op(6, 0)?;
        }
        1 | 2 => {
            d.op(7, 0)?;
            d.op(8, 0)?;
        }
        3 => {
            d.op(9, 0)?;
        }
        _ => unreachable!(),
    }
    Some(true)
}
fn panel(d: &Driver) -> Option<bool> {
    d.op(0, 0)?; // resolve bridge, invoke legacy renderer, chain native original
    d.op(1, 0)?;
    d.op(2, 0)?;
    if d.op(3, 0)? == 0 || d.op(4, 0)? == 0 {
        return Some(false);
    }
    for op in 5..=9 {
        d.op(op, 0)?;
    }
    if d.op(10, 0)? != 0 {
        d.op(11, 0)?;
    }
    for op in 12..=14 {
        d.op(op, 0)?;
    }
    Some(true)
}
fn cache(d: &Driver, kind: u32) -> Option<bool> {
    match kind {
        0 => {
            if d.op(0, 0)? != 0 {
                return Some(false);
            }
            for op in 1..=3 {
                d.op(op, 0)?;
            }
        }
        1 => {
            if d.op(0, 0)? != 0 && d.op(1, 0)? != 0 {
                return Some(false);
            }
            for op in 2..=4 {
                d.op(op, 0)?;
            }
        }
        2 => {
            for op in 0..=2 {
                d.op(op, 0)?;
            }
        }
        _ => return Some(false),
    }
    Some(true)
}
fn tooltip(d: &Driver) -> Option<bool> {
    if d.op(0, 0)? != 0 {
        return Some(false);
    }
    d.op(1, 0)?;
    let clear = d.op(2, 0)? != 0;
    d.op(if clear { 3 } else { 5 }, 0)?;
    d.op(if clear { 4 } else { 6 }, 0)?;
    Some(true)
}
fn event(d: &Driver, kind: u32) -> Option<bool> {
    if kind > 1 || d.op(0, 0)? == 0 {
        return Some(false);
    }
    if kind == 0 {
        d.op(1, 0)?;
        d.op(2, 0)?;
        if d.op(3, 0)? != 0 {
            d.op(4, 0)?;
        }
    } else {
        for op in 1..=9 {
            d.op(op, 0)?;
        }
        if d.op(10, 0)? != 0 || d.op(11, 0)? != 0 {
            d.op(12, 0)?;
        }
    }
    Some(true)
}
fn init(d: &Driver) -> Option<bool> {
    if d.op(0, 0)? == 0 {
        return Some(false);
    }
    d.op(1, 0)?;
    d.op(2, 0)?;
    if d.op(3, 0)? == 0 {
        d.op(4, 0)?;
        return Some(false);
    }
    d.op(5, 0)?;
    d.op(6, 0)?;
    match d.op(7, 0)? {
        0 => {
            d.op(8, 0)?;
            Some(false)
        }
        1 => {
            d.op(9, 0)?;
            Some(true)
        }
        _ => {
            d.op(10, 0)?;
            Some(true)
        }
    }
}
fn lookup(d: &Driver, kind: u32) -> Option<bool> {
    if kind <= 1 {
        if d.op(0, 0)? != 0 && d.op(1, 0)? != 0 {
            d.op(2, 0)?;
        }
        if d.op(3, 0)? != 0 {
            let found = if kind == 0 { d.op(4, 0)? } else { 2 };
            if found != 0 && d.op(5, found - 1)? != 0 {
                d.op(6, found - 1)?;
                return Some(true);
            }
        }
        d.op(7, 0)?;
        Some(true)
    } else if kind == 2 {
        if d.op(0, 0)? == 0 {
            return Some(false);
        }
        if d.op(1, 0)? != 0 && d.op(2, 0)? != 0 {
            d.op(3, 0)?;
        }
        let resource = d.op(4, 0)?;
        if resource >= 6 {
            return Some(false);
        }
        d.op(5, resource)?;
        if d.op(6, 0)? != 0 || d.op(7, 0)? == 0 {
            return Some(false);
        }
        d.op(8, 0)?;
        Some(true)
    } else {
        Some(false)
    }
}
macro_rules! export_mode {
    ($name:ident,$run:ident) => {
        /// # Safety
        /// Step is a synchronous non-unwinding native primitive, context remains
        /// live throughout the call; operation contract is the paired adapter.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(context: *mut c_void, step: Option<Step>, mode: u32) -> u8 {
            let Some(step) = step else { return 0 };
            u8::from($run(&Driver { context, step }, mode).unwrap_or(false))
        }
    };
}
macro_rules! export_plain {
    ($name:ident,$run:ident) => {
        /// # Safety
        /// Step is a synchronous non-unwinding native primitive, context remains
        /// live throughout the call; operation contract is the paired adapter.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(context: *mut c_void, step: Option<Step>) -> u8 {
            let Some(step) = step else { return 0 };
            u8::from($run(&Driver { context, step }).unwrap_or(false))
        }
    };
}
export_mode!(a2fo_rs_stage10_resources_transaction, transaction);
export_mode!(a2fo_rs_stage10_resources_locked, locked);
export_mode!(a2fo_rs_stage10_resources_public, public);
export_mode!(a2fo_rs_stage10_resources_cache, cache);
export_mode!(a2fo_rs_stage10_resources_event, event);
export_mode!(a2fo_rs_stage10_resources_lookup, lookup);
export_plain!(a2fo_rs_stage10_resources_panel, panel);
export_plain!(a2fo_rs_stage10_resources_tooltip, tooltip);
export_plain!(a2fo_rs_stage10_resources_init, init);

/// # Safety
/// Step is a synchronous non-unwinding native primitive with a live context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_resources_key(
    context: *mut c_void,
    step: Option<Step>,
    resource: u32,
    kind: u32,
) -> u8 {
    let Some(step) = step else { return 0 };
    if resource >= 10 || kind >= 4 {
        return 0;
    }
    let d = Driver { context, step };
    u8::from(d.op(0, 0) == Some(1) && d.op(1, 0) == Some(1))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_resources_added_valid(index: usize, kind: u32) -> u8 {
    u8::from(index < 4 && kind < 4)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;
    struct Context {
        calls: Vec<(u32, usize)>,
        answers: [usize; 16],
        failure: Option<u32>,
    }
    unsafe extern "C" fn step(opaque: *mut c_void, op: u32, arg: usize) -> usize {
        let c = unsafe { &mut *opaque.cast::<Context>() };
        c.calls.push((op, if op == 8 { 0 } else { arg }));
        if op == 8 && arg != 0 && c.failure != Some(op) {
            unsafe {
                (arg as *mut u32).write(c.answers[op as usize] as u32);
            }
            return 0;
        }
        if c.failure == Some(op) {
            usize::MAX
        } else {
            c.answers[op as usize]
        }
    }
    fn run(mode: u32, answers: &[(usize, usize)]) -> Context {
        let mut c = Context {
            calls: Vec::new(),
            answers: [0; 16],
            failure: None,
        };
        for &(n, v) in answers {
            c.answers[n] = v;
        }
        unsafe {
            a2fo_rs_stage10_resources_transaction((&mut c as *mut Context).cast(), Some(step), mode)
        };
        c
    }
    #[test]
    fn payment_lazy_read_order_and_bypass() {
        assert_eq!(run(0, &[(0, 1), (1, 1)]).calls, [(0, 0), (1, 0)]);
        assert_eq!(
            run(0, &[(0, 1), (3, 1)]).calls,
            [(0, 0), (1, 0), (2, 0), (3, 0)]
        );
        assert_eq!(
            run(0, &[(0, 1), (4, 1)]).calls,
            [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (6, 0)]
        );
        assert_eq!(
            run(1, &[(0, 1), (4, 1)]).calls,
            [(0, 0), (1, 0), (4, 0), (2, 0), (5, 0), (7, 0)]
        );
    }
    #[test]
    fn refund_gates_are_lazy() {
        assert_eq!(
            run(2, &[(0, 1), (8, u32::MAX as usize)]).calls,
            [(0, 0), (8, 0)]
        );
        assert_eq!(run(2, &[(0, 1), (8, 4)]).calls, [(0, 0), (8, 0)]);
        assert_eq!(
            run(2, &[(0, 1), (8, 5), (1, 1)]).calls,
            [(0, 0), (8, 0), (1, 0)]
        );
        assert_eq!(
            run(2, &[(0, 1), (8, 7), (4, 1), (9, 1)]).calls,
            [
                (0, 0),
                (8, 0),
                (1, 0),
                (4, 0),
                (9, 0),
                (2, 0),
                (5, 0),
                (7, 0)
            ]
        );
    }
    #[test]
    fn locked_recheck_and_failures_stop_mutation() {
        let mut c = Context {
            calls: Vec::new(),
            answers: [0; 16],
            failure: None,
        };
        let mut d = Driver {
            context: (&mut c as *mut Context).cast(),
            step,
        };
        assert_eq!(locked(&d, 0), Some(false));
        assert_eq!(c.calls, [(0, 0)]);
        c.calls.clear();
        c.answers[0] = 1;
        assert_eq!(locked(&d, 0), Some(true));
        assert_eq!(c.calls, [(0, 0), (1, 0)]);
        c.calls.clear();
        c.failure = Some(0);
        assert_eq!(locked(&d, 0), None);
        assert_eq!(c.calls, [(0, 0)]);
        d.context = (&mut c as *mut Context).cast();
        c.calls.clear();
        c.failure = None;
        assert_eq!(locked(&d, 1), Some(true));
        assert_eq!(c.calls, [(2, 0)]);
    }
    #[test]
    fn public_set_admission_precedes_mapping() {
        let mut c = Context {
            calls: Vec::new(),
            answers: [0; 16],
            failure: None,
        };
        c.answers[0] = 1;
        c.answers[3] = 1;
        c.answers[1] = 6;
        let d = Driver {
            context: (&mut c as *mut Context).cast(),
            step,
        };
        assert_eq!(public(&d, 1), Some(false));
        assert_eq!(c.calls, [(0, 0), (3, 0), (2, 0)]);
        c.calls.clear();
        c.answers[2] = 1;
        assert_eq!(public(&d, 1), Some(true));
        assert_eq!(
            c.calls,
            [(0, 0), (3, 0), (2, 0), (1, 0), (5, 0), (7, 0), (8, 0)]
        );
    }
    #[test]
    fn renderer_chains_original_before_runtime_admission() {
        let mut c = Context {
            calls: Vec::new(),
            answers: [0; 16],
            failure: None,
        };
        let d = Driver {
            context: (&mut c as *mut Context).cast(),
            step,
        };
        assert_eq!(panel(&d), Some(false));
        assert_eq!(c.calls, [(0, 0), (1, 0), (2, 0), (3, 0)]);
        c.calls.clear();
        c.answers[3] = 1;
        c.answers[4] = 1;
        c.answers[10] = 1;
        assert_eq!(panel(&d), Some(true));
        assert_eq!(
            c.calls.iter().map(|x| x.0).collect::<Vec<_>>(),
            (0..=14).collect::<Vec<_>>()
        );
    }
    #[test]
    fn cache_equality_read_only_when_generation_is_fresh() {
        let mut c = Context {
            calls: Vec::new(),
            answers: [0; 16],
            failure: None,
        };
        let d = Driver {
            context: (&mut c as *mut Context).cast(),
            step,
        };
        assert_eq!(cache(&d, 1), Some(true));
        assert_eq!(c.calls, [(0, 0), (2, 0), (3, 0), (4, 0)]);
        c.calls.clear();
        c.answers[0] = 1;
        c.answers[1] = 1;
        assert_eq!(cache(&d, 1), Some(false));
        assert_eq!(c.calls, [(0, 0), (1, 0)]);
    }
}
