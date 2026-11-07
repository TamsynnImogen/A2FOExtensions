use core::ffi::c_void;

/// Synchronous native callback. Op 0 reads the current array slot by index;
/// op 1 classifies/matches a captured candidate; op 2 clears a captured
/// owner; op 3 asks whether the current popup slot is bound by index.
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct OwnerOut {
    pub button: usize,
    pub index: usize,
    pub duplicates: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FreeOut {
    pub button: usize,
    pub index: usize,
}

/// # Safety
/// Native validates the checked build-items span and owns every array read.
/// Callback/context remain live synchronously and catch native exceptions.
/// Op 1 classifies the captured candidate; op 2 tests race against that same
/// captured candidate even if op 1 mutated the backing array.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_ui_first_target(
    context: *mut c_void,
    capacity: usize,
    step: Option<Step>,
) -> usize {
    let Some(step) = step else { return 0 };
    for index in 0..capacity {
        let candidate = unsafe { step(context, 0, index) };
        if candidate == 0 {
            continue;
        }
        if unsafe { step(context, 1, candidate) } == 0 {
            continue;
        }
        if unsafe { step(context, 2, candidate) } != 0 {
            return candidate;
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_ui_enabled(
    completed: u32,
    queued: u32,
    maximum: i32,
) -> u8 {
    let maximum = maximum.max(0) as u32;
    u8::from(completed < maximum && queued < maximum - completed)
}

/// # Safety
/// Native validates the popup array. `output` is a private writable POD
/// disjoint from callback context and native array. No reference to it is held
/// across callbacks. Op 0 rereads each slot after prior clear callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_ui_owner(
    context: *mut c_void,
    capacity: usize,
    usable: usize,
    step: Option<Step>,
    output: *mut OwnerOut,
) {
    if output.is_null() {
        return;
    }
    let mut plan = OwnerOut {
        button: 0,
        index: capacity,
        duplicates: 0,
    };
    if let Some(step) = step {
        for index in 0..capacity {
            let candidate = unsafe { step(context, 0, index) };
            if unsafe { step(context, 1, candidate) } == 0 {
                continue;
            }
            if plan.button == 0 && index < usable {
                plan.button = candidate;
                plan.index = index;
            } else {
                unsafe { step(context, 2, candidate) };
                plan.duplicates = plan.duplicates.wrapping_add(1);
            }
        }
    }
    unsafe { output.write(plan) };
}

/// # Safety
/// Native validates `usable` against the checked array capacity. Op 3 reads
/// the current bound state; on false, op 0 rereads that same array slot. A
/// null returned button still consumes the first free index, as native did.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_officer_ui_first_free(
    context: *mut c_void,
    usable: usize,
    step: Option<Step>,
    output: *mut FreeOut,
) {
    if output.is_null() {
        return;
    }
    let mut plan = FreeOut {
        button: 0,
        index: usable,
    };
    if let Some(step) = step {
        for index in 0..usable {
            if unsafe { step(context, 3, index) } == 0 {
                plan.button = unsafe { step(context, 0, index) };
                plan.index = index;
                break;
            }
        }
    }
    unsafe { output.write(plan) };
}

/// 0 clear, 1 clear then hide Build controls, 2 root handling.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_ui_menu_action(
    menu: u32,
    build_menu: u32,
    root_menu: u32,
) -> u8 {
    if menu == build_menu {
        1
    } else if menu == root_menu {
        2
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_ui_usable_count(
    rectangle_count: i32,
    capacity: usize,
) -> usize {
    (rectangle_count.max(0) as usize).min(capacity)
}

/// The native caller evaluates these conditions with the original short
/// circuit read order, then passes the resulting booleans here.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_officer_ui_displace(
    different_slot: u8,
    displaced_present: u8,
    bound: u8,
    native_speed: u8,
) -> u8 {
    u8::from(different_slot != 0 && displaced_present != 0 && bound != 0 && native_speed == 0)
}

#[cfg(test)]
mod tests {
    include!("stage9_a1_officer_ui_tests.rs");
}
