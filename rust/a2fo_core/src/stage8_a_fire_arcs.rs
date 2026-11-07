//! Portable runtime sequencing for optional three-dimensional fire arcs.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ClassField {
    pub present: u8,
    pub valid: u8,
    pub value: f32,
    pub text: *const u8,
    pub len: usize,
}

#[repr(C)]
pub struct ClassConfig {
    pub mode: i32,
    pub yaw: f32,
    pub pitch: f32,
    pub yaw_angle: f32,
    pub pitch_angle: f32,
    pub cone_angle: f32,
}

/// Safety: the callback writes a valid field at each step and owns the text
/// buffer through the immediate CRT parse operation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_optional_float(
    step: Option<Step>,
    context: *mut c_void,
    field: *const ClassField,
) {
    let Some(step) = step else {
        return;
    };
    if field.is_null() {
        return;
    }
    let native = unsafe { step(context, 0, 0) };
    if native == usize::MAX || native == 1 {
        return;
    }
    let queried = unsafe { step(context, 1, 0) };
    if queried == usize::MAX {
        return;
    }
    let present = unsafe { field.read() }.present != 0;
    if queried != 1 || !present {
        return;
    }
    unsafe { step(context, 2, 0) };
}

/// Safety: the native context owns a valid field and config for the whole
/// synchronous call. Text is inspected before the next callback can mutate it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_class(
    step: Option<Step>,
    context: *mut c_void,
    field: *mut ClassField,
    config: *mut ClassConfig,
) {
    let Some(step) = step else {
        return;
    };
    if field.is_null() || config.is_null() {
        return;
    }
    let mut floats = [(false, 0.0_f32); 7];
    let mut any = false;
    let mut valid = true;
    for (index, destination) in floats.iter_mut().enumerate() {
        if unsafe { step(context, index as u32, 0) } == usize::MAX {
            return;
        }
        let returned = unsafe { field.read() };
        let present = returned.present != 0;
        *destination = (present, returned.value);
        any |= present;
        valid &= !present || returned.valid != 0;
    }
    if unsafe { step(context, 7, 0) } == usize::MAX {
        return;
    }
    let mode_field = unsafe { field.read() };
    let mode_present = mode_field.present != 0;
    let mode_valid = mode_field.valid != 0;
    any |= mode_present;
    if !any {
        return;
    }
    let name = unsafe { step(context, 8, 0) };
    if name == usize::MAX {
        return;
    }
    if name != 1 {
        let alternate = unsafe { step(context, 14, 0) };
        if alternate == usize::MAX {
            return;
        }
        if alternate != 1 && unsafe { step(context, 15, 0) } == usize::MAX {
            return;
        }
    }
    if !valid || !mode_valid {
        unsafe { step(context, 9, 0) };
        return;
    }
    let explicit_mode = mode_present;
    let mut cone = false;
    if mode_present {
        if unsafe { step(context, 13, 0) } == usize::MAX {
            return;
        }
        let mode_field = unsafe { field.read() };
        let mode = if mode_field.text.is_null() {
            &[][..]
        } else {
            unsafe { core::slice::from_raw_parts(mode_field.text, mode_field.len) }
        };
        if mode == b"cone" {
            cone = true;
        } else if mode != b"box" {
            unsafe { step(context, 9, 1) };
            return;
        }
    } else if floats[6].0 {
        cone = true;
    }
    unsafe {
        config.write(ClassConfig {
            mode: if cone { 1 } else { 0 },
            yaw: if floats[2].0 {
                floats[2].1
            } else if floats[0].0 {
                floats[0].1
            } else {
                0.0
            },
            pitch: if floats[3].0 { floats[3].1 } else { 0.0 },
            yaw_angle: if !cone && floats[4].0 {
                floats[4].1
            } else if !cone && floats[1].0 {
                floats[1].1
            } else {
                360.0
            },
            pitch_angle: if !cone && floats[5].0 {
                floats[5].1
            } else {
                180.0
            },
            cone_angle: if cone && floats[6].0 {
                floats[6].1
            } else {
                360.0
            },
        });
    }
    if cone {
        if !floats[6].0 {
            unsafe { step(context, 9, 2) };
            return;
        }
    } else if !floats[1].0 && !floats[4].0 && !floats[5].0 {
        unsafe { step(context, 9, if explicit_mode { 3 } else { 4 }) };
        return;
    }
    if unsafe { step(context, 10, 0) } == usize::MAX {
        return;
    }
    if unsafe { step(context, 11, 0) } == usize::MAX {
        return;
    }
    let class_field = unsafe { field.read() };
    let cannon_imp = class_field.valid != 0
        && class_field.present != 0
        && !class_field.text.is_null()
        && unsafe { core::slice::from_raw_parts(class_field.text, class_field.len) }
            == b"cannonimp";
    unsafe { step(context, 12, usize::from(cannon_imp)) };
}

/// Safety: the callback validates the event before reading its fields and
/// copies weapon/target handles before native policy operations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_trigger(
    step: Option<Step>,
    context: *mut c_void,
    from_event: u8,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if from_event != 0 {
        let enabled = unsafe { step(context, 0, 0) };
        if enabled == usize::MAX {
            return 0;
        }
        if enabled == 0 {
            return 1;
        }
        let valid = unsafe { step(context, 1, 0) };
        if valid != 1 {
            return 0;
        }
        let kind = unsafe { step(context, 2, 0) };
        if kind == usize::MAX {
            return 0;
        }
        if kind == 1 {
            return 1;
        }
        if kind != 0 {
            return 0;
        }
    }
    let policy = unsafe { step(context, 3, 0) };
    if policy == usize::MAX {
        return 0;
    }
    if policy == 2 {
        let deferred = unsafe { step(context, 9, 0) };
        if deferred == usize::MAX {
            return 0;
        }
        if deferred != 0 {
            if unsafe { step(context, 4, 0) } == usize::MAX {
                return 0;
            }
            return 1;
        }
    }
    let active = unsafe { step(context, 5, 0) };
    if active == usize::MAX {
        return 0;
    }
    if active != 1 {
        return 1;
    }
    let configured = unsafe { step(context, 6, 0) };
    if configured == usize::MAX {
        return 0;
    }
    if configured != 1 {
        return 1;
    }
    let evaluation = unsafe { step(context, 7, 0) };
    if evaluation == usize::MAX {
        return 0;
    }
    let allowed = evaluation != 0;
    if unsafe { step(context, 8, usize::from(allowed)) } == usize::MAX {
        return 0;
    }
    u8::from(allowed)
}

/// Safety: native callbacks retain the configured class policy across the
/// synchronous native CanFireAt call and restore its temporary bypass there.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_can_fire_at(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == usize::MAX {
        return 0;
    }
    let configured = unsafe { step(context, 1, 0) };
    if configured == usize::MAX {
        return 0;
    }
    if configured == 0 {
        return u8::from(unsafe { step(context, 2, 0) } != 0);
    }
    let native = unsafe { step(context, 3, 0) };
    if native != 1 {
        return 0;
    }
    let activation = unsafe { step(context, 4, 0) };
    if activation == usize::MAX {
        return 0;
    }
    if activation == 0 {
        return 1;
    }
    let allowed = unsafe { step(context, 5, 0) };
    if allowed == usize::MAX {
        return 0;
    }
    if unsafe { step(context, 6, 0) } == usize::MAX {
        return 0;
    }
    if unsafe { step(context, 7, allowed) } == usize::MAX {
        return 0;
    }
    u8::from(allowed != 0)
}

/// Safety: native callbacks own the engine pointers and copied transform for
/// this synchronous render pass. The current list node is reread after draw.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_render_weapon(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 2, 0) } == usize::MAX {
        return 0;
    }
    if unsafe { step(context, 3, 0) } == usize::MAX {
        return 0;
    }
    let mut hardpoints = 0;
    for _ in 0..64 {
        if unsafe { step(context, 4, 0) } != 1 {
            break;
        }
        if unsafe { step(context, 5, 0) } == 1 {
            hardpoints += 1;
        }
        if unsafe { step(context, 6, 0) } != 1 {
            break;
        }
    }
    if hardpoints == 0 && unsafe { step(context, 7, 0) } == usize::MAX {
        return 0;
    }
    hardpoints + 1
}

/// Safety: the native context owns its bounded line array during this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_render_lines(
    step: Option<Step>,
    context: *mut c_void,
    target_inside: u8,
) {
    let Some(step) = step else {
        return;
    };
    let count = unsafe { step(context, 0, 0) };
    if count == usize::MAX {
        return;
    }
    for index in 0..count.min(128) {
        let style = unsafe { step(context, 1, index) };
        if style == usize::MAX {
            return;
        }
        let colour = if target_inside != 0 {
            2
        } else if style == 1 {
            1
        } else {
            0
        };
        if unsafe { step(context, 2, colour) } == usize::MAX {
            return;
        }
    }
}

/// Safety: native callbacks retain the weapon vector and read each slot only
/// at its original iteration point, after prior draw callbacks complete.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_render_all(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let count = unsafe { step(context, 0, 0) };
    if count == 0 || count == usize::MAX {
        return;
    }
    let mut configured = 0;
    let mut hardpoints = 0;
    for index in 0..(count - 1) {
        let result = unsafe { step(context, 1, index) };
        if result == usize::MAX {
            return;
        }
        if result & 1 != 0 {
            configured += 1;
            hardpoints += result >> 1;
        }
    }
    if configured != 0 {
        unsafe { step(context, 2, (configured << 16) | hardpoints) };
    }
}

/// Safety: native render and first-use logging operations are synchronous.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_render_hovered(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let result = unsafe { step(context, 0, 0) };
    if result == 0 || result == usize::MAX {
        return;
    }
    unsafe { step(context, 1, result - 1) };
}

/// Safety: callback op 0 calls the original renderer before any module gate.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_render_icon(
    step: Option<Step>,
    context: *mut c_void,
    system_icon: u8,
) {
    let Some(step) = step else {
        return;
    };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } != 1 {
        return;
    }
    if system_icon == 0 {
        unsafe { step(context, 2, 0) };
        if unsafe { step(context, 3, 0) } == 1 {
            unsafe { step(context, 4, 0) };
        }
    } else {
        if unsafe { step(context, 5, 0) } != 1 {
            return;
        }
        if unsafe { step(context, 6, 0) } != 1 {
            return;
        }
        if unsafe { step(context, 7, 0) } != 1 {
            return;
        }
        unsafe { step(context, 2, 0) };
        unsafe { step(context, 8, 0) };
    }
}

/// Safety: native colour reads and commit use callback-owned storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_refresh_colours(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let state = unsafe { step(context, 0, 0) };
    if state == 0 || state == usize::MAX {
        return;
    }
    if state == 2 {
        for operation in 1..=3 {
            if unsafe { step(context, operation, 0) } == usize::MAX {
                return;
            }
        }
    }
    unsafe { step(context, 4, 0) };
}

/// Safety: the native callback checks icon admission before reading fields,
/// and validates the weapon vector before indexing it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_select_weapon(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 1 {
        return 0;
    }
    let index = unsafe { step(context, 2, 0) };
    if index == 0 || index == usize::MAX {
        return 0;
    }
    let count = unsafe { step(context, 3, 0) };
    if count == 0 || count == usize::MAX || index >= count {
        return 0;
    }
    u8::from(unsafe { step(context, 4, index - 1) } == 1)
}

unsafe fn load_enabled(step: Step, context: *mut c_void) -> Option<bool> {
    let count = unsafe { step(context, 20, 0) };
    if count == usize::MAX {
        return None;
    }
    if count > 4096 {
        if unsafe { step(context, 22, 0) } == usize::MAX {
            return None;
        }
        return Some(true);
    }
    let mut enabled = true;
    let mut configured = false;
    for index in 0..count {
        let status = unsafe { step(context, 21, (index << 1) | usize::from(enabled)) };
        match status {
            0 => return None,
            2 => {
                enabled = true;
                configured = true;
            }
            3 => {
                enabled = false;
                configured = true;
            }
            1 | 4 => {}
            _ => return None,
        }
    }
    let state = usize::from(enabled) | (usize::from(configured) << 1);
    if unsafe { step(context, 23, state) } == usize::MAX {
        return None;
    }
    Some(enabled)
}

/// Safety: callback owns native API/module handles, retains them during the
/// synchronous pass and reports C++ exceptions as `usize::MAX` for rethrow
/// after Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_fire_arcs_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    unsafe fn call(step: Step, context: *mut c_void, op: u32) -> Option<bool> {
        let value = unsafe { step(context, op, 0) };
        (value != usize::MAX).then_some(value != 0)
    }
    if !matches!(unsafe { call(step, context, 0) }, Some(true))
        || !matches!(unsafe { call(step, context, 1) }, Some(true))
    {
        return 0;
    }
    match unsafe { load_enabled(step, context) } {
        Some(false) => {
            if unsafe { call(step, context, 3) }.is_none() {
                return 0;
            }
            return 1;
        }
        Some(true) => {}
        None => return 0,
    }
    if !matches!(unsafe { call(step, context, 4) }, Some(true)) {
        if unsafe { call(step, context, 5) }.is_none() {
            return 0;
        }
        return 0;
    }
    let trigger = match unsafe { call(step, context, 6) } {
        Some(value) => value,
        None => return 0,
    };
    let class = match unsafe { call(step, context, 7) } {
        Some(value) => value,
        None => return 0,
    };
    if !class || !trigger {
        if unsafe { call(step, context, 5) }.is_none() {
            return 0;
        }
        return 0;
    }
    let hooks_available = match unsafe { call(step, context, 8) } {
        Some(value) => value,
        None => return 0,
    };
    let mut ready = false;
    if hooks_available {
        let signatures = match unsafe { call(step, context, 9) } {
            Some(value) => value,
            None => return 0,
        };
        if !signatures {
            if unsafe { call(step, context, 10) }.is_none() {
                return 0;
            }
        } else {
            let first = match unsafe { call(step, context, 11) } {
                Some(value) => value,
                None => return 0,
            };
            let second = match unsafe { call(step, context, 12) } {
                Some(value) => value,
                None => return 0,
            };
            let third = match unsafe { call(step, context, 13) } {
                Some(value) => value,
                None => return 0,
            };
            ready = first && second && third;
            if !ready && unsafe { call(step, context, 14) }.is_none() {
                return 0;
            }
        }
    }
    let set_ready = unsafe { step(context, 15, usize::from(ready)) };
    if set_ready == usize::MAX {
        return 0;
    }
    if ready {
        let chained = match unsafe { call(step, context, 16) } {
            Some(value) => value,
            None => return 0,
        };
        if unsafe { call(step, context, if chained { 17 } else { 18 }) }.is_none() {
            return 0;
        }
    } else if unsafe { call(step, context, 19) }.is_none() {
        return 0;
    }
    1
}
