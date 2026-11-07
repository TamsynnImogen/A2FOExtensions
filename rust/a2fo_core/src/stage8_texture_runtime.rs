//! Portable faction rendering control. Native callbacks retain game memory,
//! C++ string/map storage, texture lookup and checked writes.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

unsafe extern "C" {
    fn a2fo_rs_gameplay_node_flags(current: u32, selected: u8) -> u32;
}

/// Safety: `context` is a live native policy context for the synchronous call.
/// Each callback finishes before the next step; Rust retains no native borrow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_texture_race_policy(
    step: Option<Step>,
    context: *mut c_void,
    name_found: u8,
    suffix_found: u8,
) {
    let Some(step) = step else {
        return;
    };
    if name_found != 0 && unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
    } else {
        unsafe { step(context, 2, name_found as usize) };
    }
    if suffix_found == 0 {
        unsafe { step(context, 4, 0) };
        return;
    }
    if unsafe { step(context, 3, 0) } == 0 {
        unsafe { step(context, 6, 0) };
    } else if unsafe { step(context, 7, 0) } == 0 {
        unsafe { step(context, 4, 0) };
    } else {
        unsafe { step(context, 5, 0) };
    }
}

/// Safety: the native callback copies per-row pointer values before any
/// foreign texture lookup and owns all checked reads, cache and mesh writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_texture_apply_meshes(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else {
        return;
    };
    for index in 0..count {
        let variant = unsafe { step(context, 0, index) };
        if variant != 0 {
            unsafe { step(context, 1, variant) };
        } else if unsafe { step(context, 2, 0) } == 0 {
            let original = unsafe { step(context, 3, 0) };
            unsafe { step(context, 1, original) };
        }
    }
}

/// Safety: the native callback owns row snapshots and checked node access.
/// The existing migrated numeric kernel supplies the exact flag result.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_texture_apply_nodes(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else {
        return;
    };
    for index in 0..count {
        let current = unsafe { step(context, 1, index) } as u32;
        let selected = unsafe { step(context, 0, index) } != 0;
        let next = unsafe { a2fo_rs_gameplay_node_flags(current, selected as u8) };
        if next != current {
            unsafe { step(context, 2, next as usize) };
        }
    }
}
