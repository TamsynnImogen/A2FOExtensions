extern crate std;

use super::*;
use std::string::{String, ToString};
use std::vec::Vec;

const MODERN: &str = "_instant_action_build_list";
const LEGACY: &str = "_build_list";

struct Context {
    requested: Option<String>,
    normalized: Vec<u8>,
    legacy: String,
    trace: Vec<(u32, usize, usize)>,
    fail_at: Option<u32>,
}

impl Context {
    fn new(requested: Option<&str>) -> Self {
        Self {
            requested: requested.map(str::to_string),
            normalized: Vec::new(),
            legacy: "old".to_string(),
            trace: Vec::new(),
            fail_at: None,
        }
    }
    fn run(&mut self) -> u8 {
        unsafe { a2fo_rs_stage9_a1_aip_name(
            (self as *mut Context).cast(), MODERN.len(), Some(step)) }
    }
}

unsafe extern "C" fn step(
    opaque: *mut c_void, operation: u32, index: usize, value: usize,
) -> usize {
    let context = unsafe { &mut *opaque.cast::<Context>() };
    context.trace.push((operation, index, value));
    if context.fail_at == Some(operation) { return ERROR; }
    match operation {
        0 => { context.legacy.clear(); 0 }
        1 => usize::from(context.requested.is_some()),
        2 => usize::from(context.requested.as_ref().is_some_and(|s| !s.is_empty())),
        3 => {
            context.normalized = context.requested.as_ref().unwrap().as_bytes().to_vec();
            context.normalized.len()
        }
        4 => context.normalized[index] as usize,
        5 => usize::from((value as u8).is_ascii_alphanumeric()),
        6 => {
            context.normalized[index] = (value as u8).to_ascii_lowercase();
            0
        }
        7 => usize::from(context.normalized[index].is_ascii_digit()),
        8 => usize::from(
            context.normalized.get(index..index + value) == Some(MODERN.as_bytes())),
        9 => {
            context.legacy = String::from_utf8(context.normalized[..index].to_vec()).unwrap();
            0
        }
        10 => { context.legacy.push_str(LEGACY); 0 }
        _ => panic!("unexpected AIP-name callback operation"),
    }
}

#[test]
fn clear_and_initial_guards_precede_string_construction() {
    let mut context = Context::new(None);
    assert_eq!(context.run(), 0);
    assert_eq!(context.legacy, "");
    assert_eq!(context.trace, [(0, 0, 0), (1, 0, 0)]);

    let mut context = Context::new(Some(""));
    assert_eq!(context.run(), 0);
    assert_eq!(context.legacy, "");
    assert_eq!(context.trace, [(0, 0, 0), (1, 0, 0), (2, 0, 0)]);

    let overlong = "A".repeat(193);
    let mut context = Context::new(Some(&overlong));
    assert_eq!(context.run(), 0);
    assert_eq!(context.normalized.len(), 193);
    assert_eq!(context.trace, [(0, 0, 0), (1, 0, 0), (2, 0, 0), (3, 0, 0)]);
}

#[test]
fn accepts_separator_characters_and_scans_digits_right_to_left() {
    let mut context = Context::new(Some("Fed-Alpha_Instant_Action_Build_List42"));
    assert_eq!(context.run(), 1);
    assert_eq!(context.legacy, "fed-alpha_build_list");
    assert_eq!(context.normalized,
               b"fed-alpha_instant_action_build_list42");
    let digit_reads: Vec<_> = context.trace.iter().copied()
        .filter(|entry| entry.0 == 7).collect();
    let length = context.normalized.len();
    assert_eq!(digit_reads,
               [(7, length - 1, 0), (7, length - 2, 0),
                (7, length - 3, 0)]);
    assert_eq!(context.trace[context.trace.len() - 3..],
               [(8, 9, MODERN.len()), (9, 9, 0), (10, 0, 0)]);
}

#[test]
fn invalid_character_stops_before_tolower_or_suffix_work() {
    let mut context = Context::new(Some("fed!_instant_action_build_list"));
    assert_eq!(context.run(), 0);
    assert_eq!(context.legacy, "");
    assert_eq!(context.trace.last(), Some(&(5, 0, b'!' as usize)));
    assert!(!context.trace.iter().any(|entry| entry.0 >= 7));
}

#[test]
fn suffix_requires_a_stem_and_an_exact_native_compare() {
    let mut context = Context::new(Some(MODERN));
    assert_eq!(context.run(), 0);
    assert!(!context.trace.iter().any(|entry| entry.0 == 8));

    let mut context = Context::new(Some("fed_instant_action_build_look"));
    assert_eq!(context.run(), 0);
    assert_eq!(context.trace.last().unwrap().0, 8);
    assert!(!context.trace.iter().any(|entry| entry.0 >= 9));
}

#[test]
fn boundary_length_and_captured_exception_stop_order() {
    let accepted = std::format!("{}{}", "a".repeat(192 - MODERN.len()), MODERN);
    let mut context = Context::new(Some(&accepted));
    assert_eq!(context.run(), 1);
    assert_eq!(context.normalized.len(), 192);
    assert_eq!(context.legacy.len(), 192 - MODERN.len() + LEGACY.len());

    let mut context = Context::new(Some("fed_instant_action_build_list"));
    context.fail_at = Some(9);
    assert_eq!(context.run(), 0);
    assert_eq!(context.trace.last().unwrap().0, 9);
    assert!(!context.trace.iter().any(|entry| entry.0 == 10));
}

#[test]
fn missing_callback_returns_false_without_touching_context() {
    assert_eq!(unsafe { a2fo_rs_stage9_a1_aip_name(
        core::ptr::null_mut(), MODERN.len(), None) }, 0);
}
