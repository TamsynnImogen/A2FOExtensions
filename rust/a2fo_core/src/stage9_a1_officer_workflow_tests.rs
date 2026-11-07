extern crate std;

use super::*;
use std::vec::Vec;

struct QueueContext {
    head: usize,
    next: [usize; 5],
    classified: [bool; 5],
    trace: Vec<(u32, usize)>,
    mutate_on_first: bool,
}

unsafe extern "C" fn queue_step(opaque: *mut c_void, op: u32, item: usize) -> usize {
    let context = unsafe { &mut *opaque.cast::<QueueContext>() };
    context.trace.push((op, item));
    match op {
        0 => context.head,
        1 => {
            if item == 1 && context.mutate_on_first {
                context.next[1] = 3;
                context.mutate_on_first = false;
            }
            usize::from(context.classified[item])
        }
        2 => context.next[item],
        _ => panic!("unexpected queue operation"),
    }
}

#[test]
fn queue_reads_next_after_classification_mutation() {
    let mut context = QueueContext {
        head: 1,
        next: [0, 2, 0, 0, 0],
        classified: [false, true, true, true, false],
        trace: Vec::new(),
        mutate_on_first: true,
    };
    let count = unsafe { a2fo_rs_stage9_a1_officer_workflow_queue(
        (&mut context as *mut QueueContext).cast(), Some(queue_step)) };
    assert_eq!(count, 2);
    assert_eq!(context.trace, [(0, 0), (1, 1), (2, 1), (1, 3), (2, 3)]);
}

#[test]
fn queue_stops_after_ten_items_and_empty_head() {
    let mut context = QueueContext {
        head: 1,
        next: [0, 1, 0, 0, 0],
        classified: [false, true, false, false, false],
        trace: Vec::new(),
        mutate_on_first: false,
    };
    let pointer = (&mut context as *mut QueueContext).cast();
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_queue(
        pointer, Some(queue_step)) }, 10);
    assert_eq!(context.trace.len(), 21);
    context.head = 0;
    context.trace.clear();
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_queue(
        pointer, Some(queue_step)) }, 0);
    assert_eq!(context.trace, [(0, 0)]);
}

struct EventContext {
    classified: bool,
    completion_ready: bool,
    producer: usize,
    target: usize,
    admitted: bool,
    suppressed: bool,
    cleared: bool,
    seen_producer: usize,
    seen_target: usize,
    trace: Vec<u32>,
    mutate_after_classification: bool,
}

unsafe extern "C" fn event_step(opaque: *mut c_void, op: u32, item: usize) -> usize {
    let context = unsafe { &mut *opaque.cast::<EventContext>() };
    assert_eq!(item, 0);
    context.trace.push(op);
    match op {
        0 => {
            if context.mutate_after_classification {
                context.completion_ready = false;
                context.producer = 91;
                context.target = 92;
            }
            usize::from(context.classified)
        }
        1 => usize::from(context.completion_ready),
        2 => {
            context.seen_producer = context.producer;
            context.seen_target = context.target;
            usize::from(context.admitted)
        }
        3 => usize::from(context.suppressed),
        4 => { context.cleared = true; 0 },
        _ => panic!("unexpected event operation"),
    }
}

fn event_context() -> EventContext {
    EventContext {
        classified: true, completion_ready: true,
        producer: 11, target: 12, admitted: true, suppressed: false,
        cleared: false, seen_producer: 0, seen_target: 0,
        trace: Vec::new(), mutate_after_classification: false,
    }
}

#[test]
fn admit_rereads_completion_and_current_event_fields() {
    let mut context = event_context();
    let pointer = (&mut context as *mut EventContext).cast();
    context.mutate_after_classification = true;
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_event(
        ADMIT, pointer, Some(event_step)) }, 0);
    assert_eq!(context.trace, [0, 1]);
    assert_eq!((context.seen_producer, context.seen_target), (0, 0));

    context.trace.clear();
    context.completion_ready = true;
    context.mutate_after_classification = false;
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_event(
        ADMIT, pointer, Some(event_step)) }, 1);
    assert_eq!(context.trace, [0, 1, 2]);
    assert_eq!((context.seen_producer, context.seen_target), (91, 92));

    context.trace.clear();
    context.classified = false;
    context.admitted = false;
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_event(
        ADMIT, pointer, Some(event_step)) }, 0);
    assert_eq!(context.trace, [0, 2]);
}

#[test]
fn event_other_kinds_preserve_native_order_and_returns() {
    let mut context = event_context();
    let pointer = (&mut context as *mut EventContext).cast();
    context.suppressed = true;
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_event(
        STARTING_EFFECT, pointer, Some(event_step)) }, 0);
    assert_eq!(context.trace, [3]);
    context.trace.clear();
    context.suppressed = false;
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_event(
        STARTING_EFFECT, pointer, Some(event_step)) }, 1);
    assert_eq!(context.trace, [3]);
    context.trace.clear();
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_event(
        DESTROYING, pointer, Some(event_step)) }, 1);
    assert!(context.cleared);
    assert_eq!(context.trace, [4]);
    for kind in [FINISHED, FINISHING, 99] {
        context.trace.clear();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_workflow_event(
            kind, pointer, Some(event_step)) }, 1);
        assert!(context.trace.is_empty());
    }
}

#[test]
fn save_validation_sanitize_and_retention_boundaries() {
    let magic = 0x514f3141;
    assert_eq!(a2fo_rs_stage9_a1_officer_workflow_save_valid(
        1, magic, 1, magic, 1), 1);
    for (loaded, actual_magic, version) in [
        (0, magic, 1), (1, 0, 1), (1, magic, 2),
    ] {
        assert_eq!(a2fo_rs_stage9_a1_officer_workflow_save_valid(
            loaded, actual_magic, version, magic, 1), 0);
    }
    let mut output = SanitizedSave { completed: 99, gain: 99 };
    unsafe { a2fo_rs_stage9_a1_officer_workflow_sanitize(
        8, -5, 6, &mut output) };
    assert_eq!(output, SanitizedSave { completed: 6, gain: 0 });
    unsafe { a2fo_rs_stage9_a1_officer_workflow_sanitize(
        8, 7, -1, &mut output) };
    assert_eq!(output, SanitizedSave { completed: 8, gain: 7 });
    unsafe { a2fo_rs_stage9_a1_officer_workflow_sanitize(
        1, 2, 3, core::ptr::null_mut()) };
    assert_eq!(a2fo_rs_stage9_a1_officer_workflow_retained_policy(0, 0, 0), 0);
    for flags in [(1, 0, 0), (0, 1, 0), (0, 0, 1)] {
        assert_eq!(a2fo_rs_stage9_a1_officer_workflow_retained_policy(
            flags.0, flags.1, flags.2), 1);
    }
    assert_eq!(core::mem::size_of::<SanitizedSave>(), 8);
    assert_eq!(core::mem::offset_of!(SanitizedSave, gain), 4);
}
