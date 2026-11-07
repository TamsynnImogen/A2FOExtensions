extern crate std;

use super::*;
use std::vec::Vec;

struct TargetContext {
    slots: [usize; 3],
    trace: Vec<(u32, usize)>,
}
unsafe extern "C" fn target_step(opaque: *mut c_void, op: u32, arg: usize) -> usize {
    let context = unsafe { &mut *opaque.cast::<TargetContext>() };
    context.trace.push((op, arg));
    match op {
        0 => context.slots[arg],
        1 => {
            if arg == 11 { context.slots[0] = 99; }
            usize::from(arg == 11 || arg == 22)
        }
        2 => usize::from(arg == 22),
        _ => panic!("unexpected target op"),
    }
}

#[test]
fn target_uses_captured_candidate_after_classification_mutation() {
    let mut context = TargetContext { slots: [11, 22, 33], trace: Vec::new() };
    let result = unsafe { a2fo_rs_stage9_a1_officer_ui_first_target(
        (&mut context as *mut TargetContext).cast(), 3, Some(target_step)) };
    assert_eq!(result, 22);
    assert_eq!(context.trace,
        [(0, 0), (1, 11), (2, 11), (0, 1), (1, 22), (2, 22)]);
    context.trace.clear();
    assert_eq!(unsafe { a2fo_rs_stage9_a1_officer_ui_first_target(
        (&mut context as *mut TargetContext).cast(), 0, Some(target_step)) }, 0);
    assert!(context.trace.is_empty());
}

struct OwnerContext {
    slots: [usize; 4],
    trace: Vec<(u32, usize)>,
}
unsafe extern "C" fn owner_step(opaque: *mut c_void, op: u32, arg: usize) -> usize {
    let context = unsafe { &mut *opaque.cast::<OwnerContext>() };
    context.trace.push((op, arg));
    match op {
        0 => context.slots[arg],
        1 => usize::from(arg == 11 || arg == 22 || arg == 44),
        2 => {
            if arg == 22 { context.slots[2] = 44; }
            0
        }
        _ => panic!("unexpected owner op"),
    }
}

#[test]
fn owner_rereads_next_slot_after_duplicate_clear() {
    let mut context = OwnerContext { slots: [11, 22, 33, 0],
                                     trace: Vec::new() };
    let mut output = OwnerOut { button: 99, index: 99, duplicates: 99 };
    unsafe { a2fo_rs_stage9_a1_officer_ui_owner(
        (&mut context as *mut OwnerContext).cast(), 4, 2,
        Some(owner_step), &mut output) };
    assert_eq!(output, OwnerOut { button: 11, index: 0, duplicates: 2 });
    assert_eq!(context.trace,
        [(0, 0), (1, 11), (0, 1), (1, 22), (2, 22),
         (0, 2), (1, 44), (2, 44), (0, 3), (1, 0)]);
    context.trace.clear();
    unsafe { a2fo_rs_stage9_a1_officer_ui_owner(
        (&mut context as *mut OwnerContext).cast(), 0, 0,
        Some(owner_step), &mut output) };
    assert_eq!(output, OwnerOut { button: 0, index: 0, duplicates: 0 });
    assert!(context.trace.is_empty());
}

struct FreeContext {
    slots: [usize; 3],
    trace: Vec<(u32, usize)>,
}
unsafe extern "C" fn free_step(opaque: *mut c_void, op: u32, arg: usize) -> usize {
    let context = unsafe { &mut *opaque.cast::<FreeContext>() };
    context.trace.push((op, arg));
    match op {
        3 => {
            if arg == 1 { context.slots[1] = 0; }
            usize::from(arg == 0)
        }
        0 => context.slots[arg],
        _ => panic!("unexpected free op"),
    }
}

#[test]
fn first_free_rereads_slot_and_keeps_null_selection() {
    let mut context = FreeContext { slots: [11, 22, 33], trace: Vec::new() };
    let mut output = FreeOut { button: 99, index: 99 };
    unsafe { a2fo_rs_stage9_a1_officer_ui_first_free(
        (&mut context as *mut FreeContext).cast(), 3,
        Some(free_step), &mut output) };
    assert_eq!(output, FreeOut { button: 0, index: 1 });
    assert_eq!(context.trace, [(3, 0), (3, 1), (0, 1)]);
    context.trace.clear();
    unsafe { a2fo_rs_stage9_a1_officer_ui_first_free(
        (&mut context as *mut FreeContext).cast(), 0,
        Some(free_step), &mut output) };
    assert_eq!(output, FreeOut { button: 0, index: 0 });
    assert!(context.trace.is_empty());
}

#[test]
fn scalar_boundaries_and_pod_layout() {
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_enabled(0, 0, -1), 0);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_enabled(2, 0, 2), 0);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_enabled(1, 1, 3), 1);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_enabled(1, 2, 3), 0);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_enabled(u32::MAX, 0, i32::MAX), 0);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_menu_action(4, 4, 4), 1);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_menu_action(5, 4, 5), 2);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_menu_action(6, 4, 5), 0);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_usable_count(-3, 12), 0);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_usable_count(20, 12), 12);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_usable_count(3, 12), 3);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_displace(1, 1, 1, 0), 1);
    assert_eq!(a2fo_rs_stage9_a1_officer_ui_displace(1, 1, 1, 1), 0);
    assert_eq!(core::mem::size_of::<OwnerOut>(), 3 * core::mem::size_of::<usize>());
    assert_eq!(core::mem::offset_of!(OwnerOut, duplicates),
               2 * core::mem::size_of::<usize>());
    assert_eq!(core::mem::size_of::<FreeOut>(), 2 * core::mem::size_of::<usize>());
}
