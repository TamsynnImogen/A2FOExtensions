//! Portable orchestration of texture variant visual registration.
//! Native callbacks retain ParameterDB calls, C++ storage and checked layouts.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

/// Safety: the native context lives through this synchronous call. No native
/// pointer or reference is retained across callback invocations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_register(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) }; // damage threshold
    for system in 0..5 {
        unsafe { step(context, 1, system) }; // target hardpoints
    }
    for index in 1..=16 {
        unsafe { step(context, 2, index) }; // scorch effects
    }
    unsafe { step(context, 3, 0) }; // documented threshold fallback
    for system in 0..5 {
        for index in 1..=64 {
            // The native callback owns the current system/index cursor and
            // performs the paired string and project-ID reads atomically.
            unsafe { step(context, 4, system * 64 + index) };
        }
    }
    unsafe { step(context, 5, 0) }; // policy decision and commit
}

/// Safety: callbacks own the live C++ policy and state cursors. Rust carries
/// only condition flags and never keeps a reference into native storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_update(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    for system in 0..5 {
        let flags = unsafe { step(context, 0, system) };
        let condition = flags & 3;
        let previous = (flags >> 2) & 3;
        if (flags & 16) == 0 {
            unsafe { step(context, 1, 0) };
            if condition == 2 {
                unsafe { step(context, 2, 0) };
            }
            continue;
        }
        let mut selected = unsafe { step(context, 3, 0) } != 0;
        if condition == 2 && previous != 2 {
            selected = unsafe { step(context, 2, 0) } != 0;
            if selected {
                unsafe { step(context, 4, 0) };
            }
        } else if condition == 2 && !selected {
            selected = unsafe { step(context, 2, 0) } != 0;
        }
        if selected {
            unsafe { step(context, 5, 0) };
        }
        if condition == 0 {
            unsafe { step(context, 6, 0) };
        }
        unsafe { step(context, 7, 0) };
    }
}

/// Safety: native callbacks retain the original class/state row and pending
/// map through each synchronous subtree pass. Rust retains only row counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_render_plan(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    for system in 0..5 {
        if unsafe { step(context, 0, system) } == 0 {
            continue;
        }
        let mut index = 0;
        while unsafe { step(context, 3, index) } != 0 {
            unsafe { step(context, 1, index) };
            index += 1;
        }
        unsafe { step(context, 2, 0) };
    }
}

/// Safety: native callbacks retain the DFS stack, visited set, current node,
/// and any class policy iterators. 0 means done, 1 skipped, 2 admitted.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_walk(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    loop {
        match unsafe { step(context, 0, 0) } {
            0 => break,
            2 => {
                unsafe { step(context, 1, 0) };
            }
            _ => {}
        }
    }
}

/// Safety: callbacks own the stack/set and native node links. Rust retains
/// only the bounded sibling counter and visits children in the original order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_subtree(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    loop {
        match unsafe { step(context, 0, 0) } {
            0 => break,
            1 => continue,
            _ => {}
        }
        for _ in 0..4096 {
            if unsafe { step(context, 2, 0) } == 0 {
                break;
            }
            unsafe { step(context, 3, 0) };
        }
    }
}

/// Safety: native callbacks own and advance the original iterator cursor.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_iterate(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    while unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
    }
}

/// Safety: native callbacks own the DFS stack, policy rows, and current name.
/// Each indexed row is looked up afresh by the callback before mutation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_resolve(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    loop {
        match unsafe { step(context, 0, 0) } {
            0 => break,
            2 if unsafe { step(context, 1, 0) } != 0 => {
                for system in 0..5 {
                    let count = unsafe { step(context, 2, system) };
                    for index in 0..count {
                        unsafe { step(context, 3, index) };
                    }
                }
            }
            _ => {}
        }
    }
    unsafe { step(context, 4, 0) };
    for system in 0..5 {
        let count = unsafe { step(context, 5, system) };
        for index in 0..count {
            unsafe { step(context, 6, index) };
        }
    }
}

/// Safety: callbacks own the module API and checked native hook installation.
/// The registration and installation order is fixed by the existing module.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_init(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        unsafe { step(context, 1, 0) };
    }
    let borg = unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, borg) };
    let faction = unsafe { step(context, 4, 0) };
    unsafe { step(context, 5, faction) };
    let subsystem = unsafe { step(context, 6, 0) } != 0 && unsafe { step(context, 7, 0) } != 0;
    unsafe { step(context, 8, subsystem as usize) };
}

/// Safety: the native callback owns each map and clears them in original order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_shutdown(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    for operation in 0..14 {
        unsafe { step(context, operation, 0) };
    }
}

/// Safety: native callbacks evaluate gates at each original read point and
/// own class storage, hierarchy enumeration and exception handling.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_prepare_render(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
    }
    if unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 3, 0) };
    }
    if unsafe { step(context, 4, 0) } != 0 {
        unsafe { step(context, 5, 0) };
    }
}

/// Safety: native callbacks retain craft/race map ownership and perform the
/// checked engine read after the admission gate.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_update_admission(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
    }
    if unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 3, 0) };
    }
}

/// Safety: native callbacks own string/cache storage and texture lookup. Rust
/// retains only the cache and file-existence decisions across callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_find_variant(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } != 0 {
        return unsafe { step(context, 1, 0) };
    }
    let has_name = unsafe { step(context, 2, 0) } != 0;
    let dds = unsafe { step(context, 3, 0) } != 0;
    let tga = !dds && unsafe { step(context, 4, 0) } != 0;
    if has_name && (dds || tga) {
        unsafe { step(context, 5, dds as usize) };
    }
    unsafe { step(context, 6, 0) }
}

/// Safety: native callback owns its indexed string, vector, or visual state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_repeat(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    for index in 0..count {
        unsafe { step(context, 0, index) };
    }
}

/// Safety: callback returns a count from native storage at each index.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_sum(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) -> usize {
    let Some(step) = step else { return 0 };
    let mut total = 0usize;
    for index in 0..count {
        total = total.wrapping_add(unsafe { step(context, 0, index) });
    }
    total
}

/// Safety: native callbacks own the availability vector and mutable visual
/// state; the existing numeric kernel is called by the native choice callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_select_mesh(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else { return 0 };
    let count = unsafe { step(context, 0, 0) };
    if count == 0 {
        unsafe { step(context, 1, 0) };
        return 0;
    }
    let choice = unsafe { step(context, 2, count) };
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, choice) }
}

/// Safety: native callbacks own the policy vector and selected index. Rust
/// carries only scalar bounds and returns the native pointer unmodified.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_selected_mesh(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    let selected = unsafe { step(context, 1, 0) };
    let count = unsafe { step(context, 2, 0) };
    if selected < count {
        unsafe { step(context, 3, selected) }
    } else {
        0
    }
}

/// Safety: native callbacks own the race map and rebuild operation. A failed
/// erase or assignment makes the subsequent callback inert until C++ rethrows.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_erase_race_name(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
    }
}

/// Safety: native callbacks own the original map lookup, assignment and
/// rebuild. Rust carries only the equality decision.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_set_race_name(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
}

/// Safety: the filename generator has already run in C++. Native callbacks
/// read each gate at its original point and perform the DDS probe in C++.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_texture_borg_result(
    step: Option<Step>,
    context: *mut c_void,
    native_result: i32,
) -> i32 {
    let Some(step) = step else {
        return native_result;
    };
    if native_result == 0
        || unsafe { step(context, 0, 0) } == 0
        || unsafe { step(context, 1, 0) } == 0
        || unsafe { step(context, 2, 0) } == 0
    {
        return native_result;
    }
    if unsafe { step(context, 3, 0) } != 0 {
        0
    } else {
        native_result
    }
}
