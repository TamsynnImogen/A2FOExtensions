extern crate std;

use super::*;
use std::vec::Vec;

static RESOLVED: &[u8] = b"Localized label\0";
static FALLBACK: &[u8] = b"Fallback label\0";

struct Context {
    manager: bool,
    key_present: bool,
    key_nonempty: bool,
    resolved: usize,
    bad_resolved: bool,
    resolved_nonempty: bool,
    full_match: bool,
    key_gui_prefix: bool,
    mutate_key_on_lookup: Option<bool>,
    fallback: usize,
    trace: Vec<u32>,
}

impl Context {
    fn ready() -> Self {
        Self {
            manager: true,
            key_present: true,
            key_nonempty: true,
            resolved: RESOLVED.as_ptr() as usize,
            bad_resolved: false,
            resolved_nonempty: true,
            full_match: false,
            key_gui_prefix: false,
            mutate_key_on_lookup: None,
            fallback: FALLBACK.as_ptr() as usize,
            trace: Vec::new(),
        }
    }
    fn run(&mut self) -> usize {
        unsafe { a2fo_rs_stage9_a1_display_text_label(
            (self as *mut Context).cast(), Some(step)) }
    }
}

unsafe extern "C" fn step(opaque: *mut c_void, operation: u32) -> usize {
    let context = unsafe { &mut *opaque.cast::<Context>() };
    context.trace.push(operation);
    match operation {
        0 => usize::from(context.manager),
        1 => usize::from(context.key_present),
        2 => usize::from(context.key_nonempty),
        3 => {
            if let Some(prefix) = context.mutate_key_on_lookup {
                context.key_gui_prefix = prefix;
            }
            context.resolved
        }
        4 => usize::from(context.bad_resolved),
        5 => usize::from(context.resolved_nonempty),
        6 => usize::from(context.full_match),
        7 => usize::from(context.key_gui_prefix),
        8 => context.fallback,
        _ => panic!("unexpected native label operation"),
    }
}

#[test]
fn guard_short_circuit_and_fallback_identity() {
    let mut context = Context::ready();
    context.manager = false;
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 8]);

    let mut context = Context::ready();
    context.key_present = false;
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 8]);

    let mut context = Context::ready();
    context.key_nonempty = false;
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 8]);
}

#[test]
fn resolved_validation_stops_at_first_rejection() {
    let mut context = Context::ready();
    context.resolved = 0;
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 8]);

    let mut context = Context::ready();
    context.bad_resolved = true;
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 4, 8]);

    let mut context = Context::ready();
    context.resolved_nonempty = false;
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 4, 5, 8]);
}

#[test]
fn crt_prefix_check_only_after_full_match() {
    let mut context = Context::ready();
    context.key_gui_prefix = true;
    assert_eq!(context.run(), RESOLVED.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 4, 5, 6]);

    let mut context = Context::ready();
    context.full_match = true;
    context.key_gui_prefix = false;
    assert_eq!(context.run(), RESOLVED.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 4, 5, 6, 7]);

    let mut context = Context::ready();
    context.full_match = true;
    context.key_gui_prefix = true;
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 4, 5, 6, 7, 8]);
}

#[test]
fn lookup_mutation_is_seen_by_later_key_comparison() {
    let mut context = Context::ready();
    context.full_match = true;
    context.mutate_key_on_lookup = Some(true);
    assert_eq!(context.run(), FALLBACK.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 4, 5, 6, 7, 8]);
    assert!(context.key_gui_prefix);

    let mut context = Context::ready();
    context.full_match = true;
    context.key_gui_prefix = true;
    context.mutate_key_on_lookup = Some(false);
    assert_eq!(context.run(), RESOLVED.as_ptr() as usize);
    assert_eq!(context.trace, [0, 1, 2, 3, 4, 5, 6, 7]);
    assert!(!context.key_gui_prefix);
}

#[test]
fn null_fallback_and_missing_callback_use_static_empty_c_string() {
    let mut context = Context::ready();
    context.manager = false;
    context.fallback = 0;
    let result = context.run();
    assert_ne!(result, 0);
    assert_eq!(unsafe { *(result as *const u8) }, 0);
    assert_eq!(context.trace, [0, 8]);

    let result = unsafe { a2fo_rs_stage9_a1_display_text_label(
        core::ptr::null_mut(), None) };
    assert_ne!(result, 0);
    assert_eq!(unsafe { *(result as *const u8) }, 0);
}
