//! Residual renderer path, file, and configuration decisions.
//! Native callbacks retain STL, Win32 handles, INI and exception ownership.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;

fn call(context: *mut c_void, step: Step, operation: u32) -> Option<u64> {
    let value = unsafe { step(context, operation, 0) };
    (value != u64::MAX).then_some(value)
}

/// Return the original C++ expression to evaluate after lazy admission reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_join(
    context: *mut c_void,
    step: Option<Step>,
) -> u32 {
    let Some(step) = step else { return 4 };
    if call(context, step, 0) == Some(1) {
        return 0;
    }
    if call(context, step, 1) == Some(1) {
        return 1;
    }
    if call(context, step, 2) == Some(1) {
        2
    } else {
        3
    }
}

/// The callback owns both handles and 16 KiB native buffers. The close order
/// follows the original early-return and loop paths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_same_file(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if call(context, step, 0) != Some(1) {
        return false;
    }
    if call(context, step, 1) != Some(1) {
        call(context, step, 2);
        return false;
    }
    let mut equal = call(context, step, 3) == Some(1)
        && call(context, step, 4) == Some(1)
        && call(context, step, 5) == Some(1);
    while equal {
        if call(context, step, 6) != Some(1)
            || call(context, step, 7) != Some(1)
            || call(context, step, 8) != Some(1)
            || call(context, step, 9) != Some(1)
        {
            equal = false;
            break;
        }
        if call(context, step, 10) == Some(1) {
            break;
        }
    }
    call(context, step, 11);
    call(context, step, 2);
    equal
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_ensure_directory(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if call(context, step, 0) != Some(0) {
        return false;
    }
    if call(context, step, 1).is_none() {
        return false;
    }
    if call(context, step, 2) == Some(1) {
        return call(context, step, 3) == Some(1);
    }
    if call(context, step, 4) == Some(1) && call(context, step, 5) != Some(1) {
        return false;
    }
    call(context, step, 6) == Some(1) || call(context, step, 7) == Some(1)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_requested(
    context: *mut c_void,
    step: Option<Step>,
) -> u32 {
    let Some(step) = step else { return 0 };
    let Some(applied) = call(context, step, 0) else {
        return 0;
    };
    call(context, step, 1).unwrap_or(applied) as u32
}

/// The first two operations occur outside the native integration catch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_form_show(
    context: *mut c_void,
    step: Option<Step>,
) {
    let Some(step) = step else { return };
    for operation in 0..=2 {
        if call(context, step, operation).is_none() {
            return;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_checkbox(
    context: *mut c_void,
    step: Option<Step>,
) {
    let Some(step) = step else { return };
    if call(context, step, 0).is_none() || call(context, step, 1) != Some(0) {
        return;
    }
    if call(context, step, 2) == Some(1) {
        call(context, step, 3);
    } else if call(context, step, 4) == Some(1) {
        call(context, step, 5);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_combo_change(
    context: *mut c_void,
    step: Option<Step>,
) {
    let Some(step) = step else { return };
    let mut renderer = true;
    for operation in 0..=4 {
        if !renderer {
            break;
        }
        match call(context, step, operation) {
            Some(1) => {}
            Some(_) => renderer = false,
            None => return,
        }
    }
    let mut monitor = true;
    for operation in 5..=9 {
        if !monitor {
            break;
        }
        match call(context, step, operation) {
            Some(1) => {}
            Some(_) => monitor = false,
            None => return,
        }
    }
    if !renderer && !monitor {
        return;
    }
    if renderer {
        let Some(selected) = call(context, step, 10) else {
            return;
        };
        if call(context, step, 11).is_none() {
            return;
        }
        if selected == 0 || selected == 1 {
            let _ = call(context, step, 12);
        }
    } else {
        if call(context, step, 13).is_none() {
            return;
        }
        for operation in 14..=16 {
            if call(context, step, operation).is_none() {
                return;
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_renderer_file_exists(attributes: u32) -> bool {
    attributes != u32::MAX && attributes & 0x10 == 0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_backend_from_text(
    context: *mut c_void,
    step: Option<Step>,
) -> u32 {
    let Some(step) = step else { return 0 };
    if call(context, step, 0) == Some(1) && call(context, step, 1) == Some(1) {
        1
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_renderer_backend_label(backend: u32) -> u32 {
    u32::from(backend == 1)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_write_effect(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    call(context, step, 0) == Some(1)
        && call(context, step, 1) == Some(1)
        && call(context, step, 2) == Some(1)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_native_display(
    context: *mut c_void,
    step: Option<Step>,
) -> i32 {
    let Some(step) = step else { return 0 };
    for operation in 0..=2 {
        if call(context, step, operation) != Some(1) {
            return 0;
        }
    }
    let Some(selected) = call(context, step, 3) else {
        return 0;
    };
    let selected = selected as u32 as i32;
    if (0..=31).contains(&selected) {
        selected
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_renderer_restart_text(
    context: *mut c_void,
    step: Option<Step>,
) {
    let Some(step) = step else { return };
    if call(context, step, 0).is_none() {
        return;
    }
    if call(context, step, 1) == Some(1) {
        call(context, step, 2);
    }
}
