//! Portable CraftIdentity control. Native callbacks own engine cursors,
//! container storage and checked native operations.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
const THREW: usize = 3;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_copy_identity_rows(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    for index in 0..count {
        if unsafe { step(context, 0, index) } == THREW {
            return;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_register_class(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    for action in 1..=6 {
        unsafe { step(context, action, 0) };
    }
    if unsafe { step(context, 7, 0) } == 0 {
        return;
    }
    for action in 8..=10 {
        if unsafe { step(context, action, 0) } == THREW {
            return;
        }
    }
    match unsafe { step(context, 11, 0) } {
        THREW => return,
        1 => {
            unsafe { step(context, 12, 0) };
            return;
        }
        _ => {}
    }
    if unsafe { step(context, 13, 0) } == 1 {
        unsafe { step(context, 14, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_ensure_identity(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0
        || unsafe { step(context, 1, 0) } == 0
        || unsafe { step(context, 2, 0) } != 0
    {
        return;
    }
    for action in 3..=9 {
        if unsafe { step(context, action, 0) } == THREW {
            return;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_hover_update(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
    selected: i32,
) {
    let Some(step) = step else { return };
    for index in 0..count {
        if selected >= 0 && index == selected as usize {
            continue;
        }
        if unsafe { step(context, 0, index) } != 0 {
            unsafe { step(context, 1, index) };
        }
    }
    if selected >= 0 && (selected as usize) < count {
        unsafe { step(context, 2, selected as usize) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_colour_apply(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == THREW {
        return;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        unsafe { step(context, 2, 0) };
    }
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_weapon_colour(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    let presentation = unsafe { step(context, 0, 0) };
    if (1..=3).contains(&presentation) || presentation == 5 {
        unsafe { step(context, presentation as u32, 0) };
    }
    if unsafe { step(context, 6, 0) } != 0 {
        unsafe { step(context, 7, 0) };
    }
    unsafe { step(context, 8, 0) };
    unsafe { step(context, 9, 0) };
    if presentation == 1 {
        unsafe { step(context, 10, 0) };
    }
    if presentation == 3 {
        unsafe { step(context, 11, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_panel_styles(
    step: Option<Step>,
    context: *mut c_void,
    element_count: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == THREW {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    for panel in 0..2 {
        if unsafe { step(context, 2, panel) } == THREW {
            return;
        }
        for element in 0..element_count {
            if unsafe { step(context, 3, element) } == THREW {
                return;
            }
            unsafe { step(context, 4, 0) };
            for colour in 0..4 {
                if unsafe { step(context, 5, colour) } == THREW {
                    return;
                }
            }
            for field in 0..2 {
                if unsafe { step(context, 6, field) } == THREW {
                    return;
                }
            }
            unsafe { step(context, 7, 0) };
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_refresh_ui(
    step: Option<Step>,
    context: *mut c_void,
    value_rectangles: usize,
    icon_colours: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    for index in 0..2 {
        unsafe { step(context, 1, index) };
    }
    for index in 2..6 {
        if unsafe { step(context, 1, index) } == 0 {
            unsafe { step(context, 2, index) };
        }
    }
    for index in 6..12 {
        unsafe { step(context, 1, index) };
    }
    for index in 0..value_rectangles {
        unsafe { step(context, 3, index) };
    }
    for index in 12..15 {
        unsafe { step(context, 1, index) };
    }
    for index in 0..19 {
        unsafe { step(context, 4, index) };
    }
    for index in 0..icon_colours {
        unsafe { step(context, 5, index) };
        unsafe { step(context, 6, index) };
    }
    for index in 0..6 {
        unsafe { step(context, 7, index) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_resolve_shield_sprites(
    step: Option<Step>,
    context: *mut c_void,
    compact_count: usize,
    legacy_count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } == 0 {
        return 0;
    }
    match unsafe { step(context, 3, 0) } {
        1 => return 1,
        3 => return 0,
        _ => {}
    }
    if unsafe { step(context, 4, 0) } == 0 {
        return 0;
    }
    let mut compact_ready = true;
    for index in 0..compact_count {
        unsafe { step(context, 5, index) };
        if compact_ready {
            compact_ready = unsafe { step(context, 6, index) } != 0;
        }
    }
    if compact_ready {
        unsafe { step(context, 7, 0) };
        return 1;
    }
    unsafe { step(context, 8, 0) };
    for index in 0..legacy_count {
        unsafe { step(context, 9, index) };
        if unsafe { step(context, 10, index) } == 0 {
            unsafe { step(context, 11, index) };
            return 0;
        }
    }
    unsafe { step(context, 12, 0) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_resolve_simple_sprites(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0 {
        return 0;
    }
    match unsafe { step(context, 2, 0) } {
        1 => return 1,
        3 => return 0,
        _ => {}
    }
    if unsafe { step(context, 3, 0) } == 0 {
        return 0;
    }
    for index in 0..count {
        unsafe { step(context, 4, index) };
        if unsafe { step(context, 5, index) } == 0 {
            unsafe { step(context, 6, index) };
            return 0;
        }
    }
    unsafe { step(context, 7, 0) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_format_ammunition_tip(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } == 0 {
        unsafe { step(context, 3, 0) };
    }
    unsafe { step(context, 4, 0) };
    unsafe { step(context, 5, 0) as u8 }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_register_selected_status(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    let first = unsafe { step(context, 1, 0) } != 0;
    let second = unsafe { step(context, 2, 0) } != 0;
    if first || second {
        unsafe { step(context, 4, 0) };
    } else {
        unsafe { step(context, 3, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_register_directional(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    for index in 0..count {
        unsafe { step(context, 2, index) };
    }
    if unsafe { step(context, 3, 0) } != 0 {
        unsafe { step(context, 5, 0) };
    } else {
        unsafe { step(context, 4, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_register_ammunition(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    for index in 0..count {
        if unsafe { step(context, 1, index) } != 0 {
            unsafe { step(context, 2, index) };
        } else {
            unsafe { step(context, 3, index) };
        }
        unsafe { step(context, 4, index) };
        for field in 0..4 {
            unsafe { step(context, 5, index * 4 + field) };
        }
        unsafe { step(context, 6, index) };
    }
    if unsafe { step(context, 7, 0) } != 0 {
        unsafe { step(context, 9, 0) };
    } else {
        unsafe { step(context, 8, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_format_selected_status_tip(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        unsafe { step(context, 2, 0) };
        if unsafe { step(context, 3, 0) } == 0 {
            unsafe { step(context, 4, 0) };
        }
        unsafe { step(context, 5, 0) };
        unsafe { step(context, 6, 0) as u8 }
    } else {
        unsafe { step(context, 7, 0) };
        unsafe { step(context, 8, 0) as u8 }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_all_sprites_live(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    for index in 0..count {
        if unsafe { step(context, 0, index) } == 0 {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_render_panel(
    step: Option<Step>,
    context: *mut c_void,
    name_count: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 1, 0) } != 0 {
        unsafe { step(context, 2, 0) };
        if unsafe { step(context, 3, 0) } != 0 {
            for index in 0..name_count {
                unsafe { step(context, 4, index) };
            }
            unsafe { step(context, 5, 0) };
        }
    }
    unsafe { step(context, 6, 0) };
    for index in (0..name_count).rev() {
        unsafe { step(context, 7, index) };
    }
    if unsafe { step(context, 8, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 9, 0) } == 0 {
        unsafe { step(context, 10, 0) };
        return;
    }
    unsafe { step(context, 11, 0) };
    if unsafe { step(context, 12, 0) } == 0 {
        unsafe { step(context, 13, 0) };
        return;
    }
    for action in 14..=21 {
        unsafe { step(context, action, 0) };
    }
    if unsafe { step(context, 22, 0) } == 0 {
        unsafe { step(context, 23, 0) };
        return;
    }
    for action in 24..=28 {
        unsafe { step(context, action, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_draw_status_bars(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 3, 0) };
        if unsafe { step(context, 4, 0) } != 0 {
            unsafe { step(context, 5, 0) };
        }
    }
    if unsafe { step(context, 6, 0) } != 0
        && unsafe { step(context, 7, 0) } != 0
        && unsafe { step(context, 8, 0) } != 0
    {
        unsafe { step(context, 9, 0) };
    }
    unsafe { step(context, 10, 0) };
    unsafe { step(context, 11, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_component_tooltip(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) };
    if unsafe { step(context, 4, 0) } == 0 {
        unsafe { step(context, 5, 0) };
        unsafe { step(context, 6, 0) };
        return;
    }
    if unsafe { step(context, 7, 0) } != 0 {
        return;
    }
    if unsafe { step(context, 8, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 9, 0) } == 0 {
        return;
    }
    for action in 10..=12 {
        unsafe { step(context, action, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_format_directional_tip(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) as u8 }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_append_directional_tip(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
    if unsafe { step(context, 3, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 4, 0) };
    if unsafe { step(context, 5, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 6, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 7, 0) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_draw_directional_tip(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    unsafe { step(context, 2, 0) };
    if unsafe { step(context, 3, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 4, 0) } == 0 {
        return;
    }
    for action in 5..=8 {
        unsafe { step(context, action, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_configure_native_tip(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) as u8 }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_clear_native_tip(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
    }
    unsafe { step(context, 2, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_present_native_tip(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        unsafe { step(context, 1, 0) };
        return;
    }
    unsafe { step(context, 2, 0) };
    if unsafe { step(context, 3, 0) } != 0 {
        unsafe { step(context, 4, 0) };
    }
    if unsafe { step(context, 5, 0) } == 0 {
        unsafe { step(context, 1, 0) };
        return;
    }
    unsafe { step(context, 6, 0) };
    unsafe { step(context, 7, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_install_hooks(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return 0;
    }
    let mut installed = true;
    for action in 3..=9 {
        let result = unsafe { step(context, action, 0) } != 0;
        installed = result && installed;
    }
    if installed {
        installed = unsafe { step(context, 10, 0) } != 0;
    }
    if installed {
        unsafe { step(context, 11, 0) };
    } else {
        unsafe { step(context, 12, 0) };
    }
    installed as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_module_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 1 {
        return 0;
    }
    let registered = unsafe { step(context, 2, 0) };
    if registered == THREW {
        return 0;
    }
    if registered == 0 && unsafe { step(context, 3, 0) } == THREW {
        return 0;
    }
    for action in 4..=5 {
        if unsafe { step(context, action, 0) } == THREW {
            return 0;
        }
    }
    let ready = unsafe { step(context, 6, 0) };
    if ready == THREW {
        return 0;
    }
    if ready != 0 {
        for action in 7..=9 {
            if unsafe { step(context, action, 0) } == THREW {
                return 0;
            }
        }
    } else {
        if unsafe { step(context, 10, 0) } == THREW {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_draw_shield_rows(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    for index in 0..count {
        if unsafe { step(context, 1, index) } == 0 {
            return;
        }
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return;
    }
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    match unsafe { step(context, 5, 0) } {
        1 => return,
        2 => {
            unsafe { step(context, 6, 0) };
            for index in 0..count {
                if unsafe { step(context, 7, index) } != 0 {
                    unsafe { step(context, 8, index) };
                }
            }
            unsafe { step(context, 9, 0) };
            return;
        }
        _ => {}
    }
    if unsafe { step(context, 10, 0) } != 0 {
        return;
    }
    unsafe { step(context, 11, 0) };
    let mode = unsafe { step(context, 12, 0) };
    match mode {
        2 => {
            unsafe { step(context, 13, 0) };
        }
        3 => {
            unsafe { step(context, 14, 0) };
        }
        5 => {}
        _ => {
            unsafe { step(context, 15, 0) };
        }
    }
    unsafe { step(context, 16, 0) };
    if mode != 5 {
        unsafe { step(context, 17, 0) };
    }
    if mode != 4 {
        unsafe { step(context, 18, 0) };
    }
    unsafe { step(context, 19, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_load_art(
    step: Option<Step>,
    context: *mut c_void,
    root_count: usize,
) {
    let Some(step) = step else { return };
    if root_count > 4096 {
        unsafe { step(context, 0, 0) };
        return;
    }
    for index in 0..root_count {
        match unsafe { step(context, 1, index) } {
            THREW => return,
            0 => continue,
            _ => {}
        }
        let read = unsafe { step(context, 2, 0) };
        if read == THREW {
            return;
        }
        if read == 0 {
            if unsafe { step(context, 6, 0) } == THREW {
                return;
            }
            continue;
        }
        for action in 3..=6 {
            if unsafe { step(context, action, 0) } == THREW {
                return;
            }
        }
    }
    let invalid_mapping = unsafe { step(context, 7, 0) };
    if invalid_mapping == THREW {
        return;
    }
    if invalid_mapping != 0 && unsafe { step(context, 8, 0) } == THREW {
        return;
    }
    unsafe { step(context, 9, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_native_tip_phase(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    unsafe { step(context, 3, 0) };
    if unsafe { step(context, 4, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    if unsafe { step(context, 5, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    for action in 6..=8 {
        unsafe { step(context, action, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_update_shield_tip_rects(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    for index in 0..count {
        if unsafe { step(context, 2, index) } != 0 {
            unsafe { step(context, 3, index) };
        }
    }
    unsafe { step(context, 4, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_animate_shield_ratios(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    for index in 0..count {
        unsafe { step(context, 0, index) };
    }
    if unsafe { step(context, 1, 0) } != 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    unsafe { step(context, 3, 0) };
    for index in 0..count {
        unsafe { step(context, 4, index) };
    }
    unsafe { step(context, 5, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_value_rectangles(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    for index in 0..count {
        if unsafe { step(context, 2, index) } != 0 {
            unsafe { step(context, 3, index) };
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_draw_ammo_rows(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    let mut any_store = false;
    for index in 0..count {
        if !any_store {
            any_store = unsafe { step(context, 2, index) } != 0;
        }
    }
    if !any_store {
        return;
    }
    unsafe { step(context, 3, 0) };
    for index in 0..count {
        if unsafe { step(context, 4, index) } != 0 {
            unsafe { step(context, 5, index) };
        }
    }
    for index in 0..count {
        unsafe { step(context, 6, index) };
    }
    unsafe { step(context, 7, 0) };
    if unsafe { step(context, 8, 0) } == 0 {
        return;
    }
    unsafe { step(context, 9, 0) };
    for index in 0..count {
        if unsafe { step(context, 10, index) } == 0 {
            continue;
        }
        unsafe { step(context, 11, index) };
        let mode = unsafe { step(context, 12, index) };
        match mode {
            1 => {
                unsafe { step(context, 13, index) };
            }
            2 => {
                if unsafe { step(context, 14, index) } != 0 {
                    unsafe { step(context, 15, index) };
                } else {
                    unsafe { step(context, 16, index) };
                    if unsafe { step(context, 17, index) } != 0 {
                        unsafe { step(context, 18, index) };
                    } else {
                        unsafe { step(context, 19, index) };
                    }
                }
            }
            _ => {
                if mode != 3 {
                    unsafe { step(context, 20, index) };
                }
            }
        }
        if unsafe { step(context, 21, index) } != 0 {
            unsafe { step(context, 22, index) };
        }
        if mode == 3 {
            unsafe { step(context, 23, index) };
            if unsafe { step(context, 24, index) } != 0 {
                unsafe { step(context, 25, index) };
            }
            if unsafe { step(context, 26, index) } != 0 {
                unsafe { step(context, 27, index) };
            }
        } else {
            unsafe { step(context, 28, index) };
            if unsafe { step(context, 29, index) } != 0 {
                unsafe { step(context, 31, index) };
            }
            unsafe { step(context, 32, index) };
            unsafe { step(context, 33, index) };
        }
        if unsafe { step(context, 34, index) } != 0 {
            unsafe { step(context, 37, index) };
        }
    }
    unsafe { step(context, 35, 0) };
    unsafe { step(context, 36, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_draw_shield_graphic(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    let mode = unsafe { step(context, 1, 0) };
    match mode {
        1 => {
            unsafe { step(context, 13, 0) };
        }
        2 | 3 => {
            unsafe { step(context, 14, 0) };
        }
        4 => {
            unsafe { step(context, 15, 0) };
        }
        5 => {
            unsafe { step(context, 16, 0) };
        }
        _ => {}
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 3, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 4, 0) };
    for index in 0..count {
        if unsafe { step(context, 5, index) } != 0 {
            unsafe { step(context, 6, index) };
        }
    }
    if unsafe { step(context, 7, 0) } != 0 {
        for index in 0..count {
            unsafe { step(context, 8, index) };
        }
    }
    for index in 0..count {
        unsafe { step(context, 9, index) };
    }
    unsafe { step(context, 10, 0) };
    for index in 0..count {
        if unsafe { step(context, 11, index) } != 0 {
            unsafe { step(context, 12, index) };
        }
    }
    for index in 0..count {
        if unsafe { step(context, 17, index) } == 0 {
            continue;
        }
        if unsafe { step(context, 18, index) } != 0 {
            unsafe { step(context, 19, index) };
            continue;
        }
        if unsafe { step(context, 20, index) } == 0 {
            continue;
        }
        if unsafe { step(context, 21, index) } == 0 {
            continue;
        }
        if unsafe { step(context, 22, index) } == 0 {
            continue;
        }
        if unsafe { step(context, 23, index) } != 0 {
            unsafe { step(context, 24, index) };
        } else {
            unsafe { step(context, 25, index) };
            if unsafe { step(context, 26, index) } != 0 {
                unsafe { step(context, 27, index) };
            }
        }
        unsafe { step(context, 28, index) };
    }
    if unsafe { step(context, 29, 0) } != 0 {
        unsafe { step(context, 30, 0) };
    }
    unsafe { step(context, 31, 0) };
    unsafe { step(context, 32, 0) as u8 }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_join_path(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 3, 0) };
        return;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        unsafe { step(context, 4, 0) };
        return;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 5, 0) };
    } else {
        unsafe { step(context, 6, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_read_art_file(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    let opened = unsafe { step(context, 0, 0) };
    if opened == THREW || opened == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } == THREW {
        return 0;
    }
    let oversized = unsafe { step(context, 2, 0) };
    if oversized == THREW {
        return 0;
    }
    if oversized != 0 {
        unsafe { step(context, 3, 0) };
        return 0;
    }
    if unsafe { step(context, 4, 0) } == THREW {
        return 0;
    }
    if unsafe { step(context, 5, 0) } == THREW {
        return 0;
    }
    unsafe { step(context, 6, 0) as u8 }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_apply_hover_rectangle(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 3, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 4, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 5, 0) };
    unsafe { step(context, 6, 0) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_wireframe_update(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) };
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 3, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_tooltip_show(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
        if unsafe { step(context, 2, 0) } != 0 && unsafe { step(context, 3, 0) } != 0 {
            unsafe { step(context, 4, 0) };
        }
    }
    if unsafe { step(context, 5, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 6, 0) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_tooltip_render(
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
    unsafe { step(context, 4, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_install_tooltip_hook(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    if unsafe { step(context, 3, 0) } == 0 {
        unsafe { step(context, 4, 0) };
        return;
    }
    unsafe { step(context, 5, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_install_icon_hooks(
    step: Option<Step>,
    context: *mut c_void,
    system_count: usize,
    weapon_count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    let mut installed = true;
    for index in 0..system_count {
        let result = unsafe { step(context, 1, index) } != 0;
        installed = result && installed;
    }
    for index in 0..weapon_count {
        let result = unsafe { step(context, 2, index) } != 0;
        installed = result && installed;
    }
    let result = unsafe { step(context, 3, 0) } != 0;
    installed = result && installed;
    if unsafe { step(context, 4, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 5, 0) };
    let result = unsafe { step(context, 6, 0) } != 0;
    installed = result && installed;
    installed as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_install_constructor(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } != 0 {
        if unsafe { step(context, 1, 0) } == 0 {
            return 0;
        }
        return unsafe { step(context, 2, 0) as u8 };
    }
    if unsafe { step(context, 3, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 4, 0) };
    if unsafe { step(context, 5, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 6, 0) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_resolve_energy(
    step: Option<Step>,
    context: *mut c_void,
    export_count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } != 0 {
        return unsafe { step(context, 1, 0) as u8 };
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return 0;
    }
    for index in 0..export_count {
        unsafe { step(context, 3, index) };
    }
    for index in 0..export_count {
        unsafe { step(context, 4, index) };
    }
    let ready = unsafe { step(context, 5, 0) } != 0;
    unsafe { step(context, 6, ready as usize) };
    if ready && unsafe { step(context, 7, 0) } != 0 {
        unsafe { step(context, 8, 0) };
    }
    if ready && unsafe { step(context, 9, 0) } != 0 {
        unsafe { step(context, 10, 0) };
    } else if ready && unsafe { step(context, 11, 0) } != 0 {
        unsafe { step(context, 12, 0) };
    }
    ready as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_resolve_directional(
    step: Option<Step>,
    context: *mut c_void,
    export_count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } != 0 {
        return unsafe { step(context, 1, 0) as u8 };
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return 0;
    }
    for index in 0..export_count {
        unsafe { step(context, 3, index) };
    }
    for index in 0..export_count {
        unsafe { step(context, 4, index) };
    }
    let ready = unsafe { step(context, 5, 0) } != 0;
    unsafe { step(context, 6, ready as usize) };
    ready as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_observer(
    step: Option<Step>,
    context: *mut c_void,
    fallback: u8,
) {
    let Some(step) = step else { return };
    let found = unsafe { step(context, 0, 0) } != 0;
    if !found && fallback != 0 {
        unsafe { step(context, 1, 0) };
    }
    unsafe { step(context, 2, 0) };
    if unsafe { step(context, 3, 0) } != 0 {
        unsafe { step(context, 4, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_experience_progress(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        if unsafe { step(context, 3, 0) } == 0 {
            return 0;
        }
        unsafe { step(context, 4, 0) };
        return 1;
    }
    unsafe { step(context, 5, 0) };
    if unsafe { step(context, 6, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 7, 0) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_value_colour(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    let kind = unsafe { step(context, 0, 0) };
    match kind {
        1 => unsafe { step(context, 1, 0) as u8 },
        2 | 3 => unsafe { step(context, 2, 0) as u8 },
        4 => unsafe { step(context, 3, 0) as u8 },
        5 => unsafe { step(context, 4, 0) as u8 },
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_value_text_colour(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } == 0 {
        return;
    }
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_ship_icon_render(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 3, 0) };
        return;
    }
    for action in 4..=7 {
        unsafe { step(context, action, 0) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_copy_native_string(
    step: Option<Step>,
    context: *mut c_void,
    max_length: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    for length in 0..max_length {
        if unsafe { step(context, 1, length) } == 0 {
            return 0;
        }
        if unsafe { step(context, 2, length) } != 0 {
            unsafe { step(context, 3, length) };
            return 1;
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_release_native_vector(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    for index in 0..count {
        unsafe { step(context, 0, index) };
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_reset_hover(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    let mut presented = false;
    for index in 0..count {
        if !presented {
            presented = unsafe { step(context, 0, index) } != 0;
        }
    }
    if presented {
        unsafe { step(context, 1, 0) };
    }
    unsafe { step(context, 2, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_find_hover_row(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) -> i32 {
    let Some(step) = step else { return -1 };
    for index in 0..count {
        if unsafe { step(context, 0, index) } != 0 {
            return index as i32;
        }
    }
    -1
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Trace {
        actions: Vec<(u32, usize)>,
        visible: [bool; 3],
        result: usize,
    }

    unsafe extern "C" fn hover_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        if action == 0 {
            return trace.visible[index] as usize;
        }
        if action == 1 && index == 0 {
            trace.visible[1] = false;
        }
        0
    }

    #[test]
    fn hover_rechecks_visibility_after_update_and_presents_selected_last() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [true; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_hover_update(Some(hover_step), (&raw mut trace).cast(), 3, 2)
        };
        assert_eq!(trace.actions, [(0, 0), (1, 0), (0, 1), (2, 2)]);
    }

    unsafe extern "C" fn class_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 7 | 13 => 1,
            _ => 0,
        }
    }

    #[test]
    fn class_registration_keeps_subpolicy_and_list_order() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe { a2fo_rs_stage9_craft_register_class(Some(class_step), (&raw mut trace).cast()) };
        assert_eq!(
            trace
                .actions
                .iter()
                .map(|(action, _)| *action)
                .collect::<Vec<_>>(),
            [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14]
        );
    }

    unsafe extern "C" fn identity_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0..=2 => 1,
            _ => 0,
        }
    }

    #[test]
    fn matching_identity_skips_reassignment() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_ensure_identity(Some(identity_step), (&raw mut trace).cast())
        };
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0)]);
    }

    unsafe extern "C" fn colour_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        if action == 0 {
            return trace.result;
        }
        if action == 1 || action == 6 {
            return 1;
        }
        0
    }

    #[test]
    fn weapon_colour_tints_before_native_set_then_reports() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        unsafe { a2fo_rs_stage9_craft_weapon_colour(Some(colour_step), (&raw mut trace).cast()) };
        assert_eq!(
            trace
                .actions
                .iter()
                .map(|(action, _)| *action)
                .collect::<Vec<_>>(),
            [0, 1, 6, 7, 8, 9, 10]
        );
    }

    unsafe extern "C" fn sprite_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 2 | 4 => 1,
            3 => 2,
            _ => 0,
        }
    }

    #[test]
    fn compact_failure_still_looks_up_second_before_legacy_retry() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let ok = unsafe {
            a2fo_rs_stage9_craft_resolve_shield_sprites(
                Some(sprite_step),
                (&raw mut trace).cast(),
                2,
                4,
            )
        };
        assert_eq!(ok, 0);
        assert_eq!(
            trace.actions,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (6, 0),
                (5, 1),
                (8, 0),
                (9, 0),
                (10, 0),
                (11, 0)
            ]
        );
    }

    unsafe extern "C" fn tip_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 5 => 1,
            _ => 0,
        }
    }

    #[test]
    fn ammunition_tip_falls_back_after_missing_verbose_policy() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let formatted = unsafe {
            a2fo_rs_stage9_craft_format_ammunition_tip(Some(tip_step), (&raw mut trace).cast())
        };
        assert_eq!(formatted, 1);
        assert_eq!(
            trace.actions,
            [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0)]
        );
    }

    unsafe extern "C" fn live_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        (index != 1) as usize
    }

    #[test]
    fn sprite_live_scan_stops_at_first_invalid_pointer() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let live = unsafe {
            a2fo_rs_stage9_craft_all_sprites_live(Some(live_step), (&raw mut trace).cast(), 4)
        };
        assert_eq!(live, 0);
        assert_eq!(trace.actions, [(0, 0), (0, 1)]);
    }

    unsafe extern "C" fn simple_sprite_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            1 | 3 => 1,
            5 => (index != 1) as usize,
            _ => 0,
        }
    }

    #[test]
    fn tri_sprite_failure_reports_first_bad_sprite_and_stops() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let ready = unsafe {
            a2fo_rs_stage9_craft_resolve_simple_sprites(
                Some(simple_sprite_step),
                (&raw mut trace).cast(),
                3,
            )
        };
        assert_eq!(ready, 0);
        assert_eq!(
            trace.actions,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (4, 1),
                (5, 1),
                (6, 1)
            ]
        );
    }

    unsafe extern "C" fn refresh_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        if action == 0 {
            return trace.result;
        }
        if action == 1 {
            return (index != 2 && index != 4) as usize;
        }
        0
    }

    #[test]
    fn ui_refresh_fallbacks_keep_interleaved_read_order() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        unsafe {
            a2fo_rs_stage9_craft_refresh_ui(Some(refresh_step), (&raw mut trace).cast(), 4, 5)
        };
        assert_eq!(
            &trace.actions[..10],
            &[
                (0, 0),
                (1, 0),
                (1, 1),
                (1, 2),
                (2, 2),
                (1, 3),
                (1, 4),
                (2, 4),
                (1, 5),
                (1, 6)
            ]
        );
        assert_eq!(trace.actions.last(), Some(&(7, 5)));
        assert_eq!(
            trace
                .actions
                .iter()
                .filter(|(action, _)| *action == 3)
                .count(),
            4
        );
        assert_eq!(
            trace
                .actions
                .iter()
                .filter(|(action, _)| *action == 5)
                .count(),
            5
        );
        assert_eq!(
            trace
                .actions
                .iter()
                .filter(|(action, _)| *action == 6)
                .count(),
            5
        );
    }

    #[test]
    fn ui_refresh_missing_database_skips_all_reads() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_refresh_ui(Some(refresh_step), (&raw mut trace).cast(), 4, 5)
        };
        assert_eq!(trace.actions, [(0, 0)]);
    }

    unsafe extern "C" fn status_bar_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 1 | 2 | 6 | 7 | 8 => 1,
            4 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn status_bar_invalid_shields_still_draws_xp_and_finalizes() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_draw_status_bars(Some(status_bar_step), (&raw mut trace).cast())
        };
        assert_eq!(
            trace.actions,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (6, 0),
                (7, 0),
                (8, 0),
                (9, 0),
                (10, 0),
                (11, 0)
            ]
        );
    }

    unsafe extern "C" fn component_tip_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 1 | 4 => 1,
            8 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn component_tip_format_failure_preserves_binding() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_component_tooltip(
                Some(component_tip_step),
                (&raw mut trace).cast(),
            )
        };
        assert_eq!(
            trace.actions,
            [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (7, 0), (8, 0)]
        );
    }

    unsafe extern "C" fn shield_tip_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 1 => 1,
            3 => trace.visible[0] as usize,
            4 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn append_tip_logs_query_even_when_cursor_misses() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let appended = unsafe {
            a2fo_rs_stage9_craft_append_directional_tip(
                Some(shield_tip_step),
                (&raw mut trace).cast(),
            )
        };
        assert_eq!(appended, 0);
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0), (3, 0)]);
    }

    #[test]
    fn direct_tip_skips_geometry_after_format_failure() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [true, false, false],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_draw_directional_tip(
                Some(shield_tip_step),
                (&raw mut trace).cast(),
            )
        };
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0)]);
    }

    unsafe extern "C" fn native_tip_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 => 1,
            3 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn native_tip_invalid_owner_clears_after_clock_and_owner_read() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        unsafe {
            a2fo_rs_stage9_craft_present_native_tip(Some(native_tip_step), (&raw mut trace).cast())
        };
        assert_eq!(
            trace.actions,
            [(0, 0), (2, 0), (3, 0), (4, 0), (5, 0), (1, 0)]
        );
    }

    unsafe extern "C" fn install_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 1 | 4..=10 => 1,
            _ => 0,
        }
    }

    #[test]
    fn hook_install_continues_all_required_attempts_after_first_failure() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let installed = unsafe {
            a2fo_rs_stage9_craft_install_hooks(Some(install_step), (&raw mut trace).cast())
        };
        assert_eq!(installed, 0);
        assert_eq!(
            trace.actions,
            [
                (0, 0),
                (1, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (6, 0),
                (7, 0),
                (8, 0),
                (9, 0),
                (12, 0)
            ]
        );
    }

    unsafe extern "C" fn init_throw_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 1 => 1,
            4 => THREW,
            _ => 0,
        }
    }

    #[test]
    fn module_init_stops_callbacks_after_native_exception() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let initialized = unsafe {
            a2fo_rs_stage9_craft_module_init(Some(init_throw_step), (&raw mut trace).cast())
        };
        assert_eq!(initialized, 0);
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0)]);
    }

    unsafe extern "C" fn shield_rows_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 2 | 7 => 1,
            1 => (index != trace.result) as usize,
            5 => 2,
            _ => 0,
        }
    }

    #[test]
    fn shield_rows_invalid_facing_stops_later_native_reads() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        unsafe {
            a2fo_rs_stage9_craft_draw_shield_rows(
                Some(shield_rows_step),
                (&raw mut trace).cast(),
                4,
            )
        };
        assert_eq!(trace.actions, [(0, 0), (1, 0), (1, 1)]);
    }

    #[test]
    fn shield_rows_configured_values_visit_four_faces_then_report() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: usize::MAX,
        };
        unsafe {
            a2fo_rs_stage9_craft_draw_shield_rows(
                Some(shield_rows_step),
                (&raw mut trace).cast(),
                4,
            )
        };
        assert_eq!(
            trace
                .actions
                .iter()
                .filter(|(action, _)| *action == 8)
                .count(),
            4
        );
        assert_eq!(trace.actions.last(), Some(&(9, 0)));
    }

    unsafe extern "C" fn art_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            1 => (index != 0) as usize,
            2 => (trace.result != 1) as usize,
            7 => 1,
            _ => 0,
        }
    }

    #[test]
    fn art_scan_releases_failed_read_then_checks_mapping() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        unsafe { a2fo_rs_stage9_craft_load_art(Some(art_step), (&raw mut trace).cast(), 2) };
        assert_eq!(
            trace.actions,
            [(1, 0), (1, 1), (2, 0), (6, 0), (7, 0), (8, 0), (9, 0)]
        );
    }

    #[test]
    fn art_scan_invalid_root_count_logs_without_querying_roots() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe { a2fo_rs_stage9_craft_load_art(Some(art_step), (&raw mut trace).cast(), 4097) };
        assert_eq!(trace.actions, [(0, 0)]);
    }

    unsafe extern "C" fn tip_phase_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 1 | 4 => 1,
            _ => 0,
        }
    }

    #[test]
    fn native_tip_phase_provider_failure_clears_before_return() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_native_tip_phase(Some(tip_phase_step), (&raw mut trace).cast())
        };
        assert_eq!(
            trace.actions,
            [(0, 0), (1, 0), (3, 0), (4, 0), (5, 0), (2, 0)]
        );
    }

    unsafe extern "C" fn ammo_rows_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 2 | 8 => 1,
            10 => (index != 1) as usize,
            12 => trace.result,
            24 | 26 | 34 => 1,
            _ => 0,
        }
    }

    #[test]
    fn ammo_reload_waiting_skips_invalid_middle_row() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 2,
        };
        unsafe {
            a2fo_rs_stage9_craft_draw_ammo_rows(Some(ammo_rows_step), (&raw mut trace).cast(), 3)
        };
        assert_eq!(
            trace
                .actions
                .iter()
                .filter(|(action, _)| *action == 19)
                .count(),
            2
        );
        assert_eq!(
            trace
                .actions
                .iter()
                .filter(|(action, _)| *action == 11)
                .count(),
            2
        );
        assert_eq!(trace.actions.last(), Some(&(36, 0)));
    }

    #[test]
    fn ammo_bar_updates_hit_after_label_and_bar_draws() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 3,
        };
        unsafe {
            a2fo_rs_stage9_craft_draw_ammo_rows(Some(ammo_rows_step), (&raw mut trace).cast(), 1)
        };
        let actions = trace.actions;
        let tail = &actions[actions.len() - 8..];
        assert_eq!(
            tail,
            &[
                (24, 0),
                (25, 0),
                (26, 0),
                (27, 0),
                (34, 0),
                (37, 0),
                (35, 0),
                (36, 0)
            ][..]
        );
    }

    unsafe extern "C" fn graphic_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 2 | 3 | 20 | 21 | 26 | 29 | 32 => 1,
            1 => 1,
            17 => (index == 0) as usize,
            22 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn graphic_sprite_restores_after_depleted_and_fill_draws() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        let drew = unsafe {
            a2fo_rs_stage9_craft_draw_shield_graphic(Some(graphic_step), (&raw mut trace).cast(), 4)
        };
        assert_eq!(drew, 1);
        let actions = trace.actions;
        let draw_start = actions.iter().position(|entry| *entry == (23, 0)).unwrap();
        assert_eq!(
            &actions[draw_start..draw_start + 5],
            &[(23, 0), (25, 0), (26, 0), (27, 0), (28, 0)]
        );
        assert_eq!(
            actions.iter().filter(|(action, _)| *action == 28).count(),
            1
        );
    }

    #[test]
    fn graphic_invalid_texture_window_skips_draw_and_restore() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_draw_shield_graphic(Some(graphic_step), (&raw mut trace).cast(), 1)
        };
        assert!(trace.actions.contains(&(22, 0)));
        assert!(!trace.actions.iter().any(|(action, _)| *action == 28));
    }

    unsafe extern "C" fn art_file_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 2 => 1,
            _ => 0,
        }
    }

    #[test]
    fn oversized_art_file_logs_without_reading_stream() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let read = unsafe {
            a2fo_rs_stage9_craft_read_art_file(Some(art_file_step), (&raw mut trace).cast())
        };
        assert_eq!(read, 0);
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0), (3, 0)]);
    }

    unsafe extern "C" fn wireframe_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 => {
                if trace.visible[0] {
                    1
                } else {
                    trace.result
                }
            }
            2 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn wireframe_restores_rectangle_after_original_update() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        unsafe {
            a2fo_rs_stage9_craft_wireframe_update(Some(wireframe_step), (&raw mut trace).cast())
        };
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0), (3, 0)]);
    }

    #[test]
    fn hover_rectangle_missing_facing_avoids_owner_reads() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [true, false, false],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_apply_hover_rectangle(
                Some(wireframe_step),
                (&raw mut trace).cast(),
            )
        };
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0)]);
    }

    unsafe extern "C" fn tooltip_hook_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 2 | 3 => 1,
            5 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn tooltip_show_substitution_still_checks_missing_gateway() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let shown = unsafe {
            a2fo_rs_stage9_craft_tooltip_show(Some(tooltip_hook_step), (&raw mut trace).cast())
        };
        assert_eq!(shown, 0);
        assert_eq!(
            trace.actions,
            [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0)]
        );
    }

    unsafe extern "C" fn icon_install_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 | 2..=6 => 1,
            _ => 0,
        }
    }

    #[test]
    fn icon_hook_patch_loop_keeps_later_attempts_after_failure() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let installed = unsafe {
            a2fo_rs_stage9_craft_install_icon_hooks(
                Some(icon_install_step),
                (&raw mut trace).cast(),
                2,
                1,
            )
        };
        assert_eq!(installed, 0);
        assert_eq!(
            trace.actions,
            [
                (0, 0),
                (1, 0),
                (1, 1),
                (2, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (6, 0)
            ]
        );
    }

    unsafe extern "C" fn constructor_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            3 => trace.result,
            5 => 1,
            _ => 0,
        }
    }

    #[test]
    fn unsupported_constructor_detour_never_reads_or_patches_bytes() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let installed = unsafe {
            a2fo_rs_stage9_craft_install_constructor(
                Some(constructor_step),
                (&raw mut trace).cast(),
            )
        };
        assert_eq!(installed, 0);
        assert_eq!(trace.actions, [(0, 0), (3, 0)]);
    }

    unsafe extern "C" fn energy_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            2 | 5 | 7 | 9 => 1,
            _ => 0,
        }
    }

    #[test]
    fn energy_exports_copy_only_after_all_lookups_and_keep_warning_order() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let ready = unsafe {
            a2fo_rs_stage9_craft_resolve_energy(Some(energy_step), (&raw mut trace).cast(), 3)
        };
        assert_eq!(ready, 1);
        assert_eq!(
            trace.actions,
            [
                (0, 0),
                (2, 0),
                (3, 0),
                (3, 1),
                (3, 2),
                (4, 0),
                (4, 1),
                (4, 2),
                (5, 0),
                (6, 1),
                (7, 0),
                (8, 0),
                (9, 0),
                (10, 0)
            ]
        );
    }

    unsafe extern "C" fn xp_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            1 | 2 => 1,
            3 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn invalid_collective_xp_does_not_fall_back_to_live_enhancement() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        let valid = unsafe {
            a2fo_rs_stage9_craft_experience_progress(Some(xp_step), (&raw mut trace).cast())
        };
        assert_eq!(valid, 0);
        assert_eq!(trace.actions, [(0, 0), (1, 0), (2, 0), (3, 0)]);
    }

    unsafe extern "C" fn icon_render_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 => 1,
            2 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn visible_ship_icon_restores_native_scope_after_render() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_ship_icon_render(Some(icon_render_step), (&raw mut trace).cast())
        };
        assert_eq!(
            trace.actions,
            [(0, 0), (1, 0), (2, 0), (4, 0), (5, 0), (6, 0), (7, 0)]
        );
    }

    unsafe extern "C" fn native_string_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            0 => 1,
            1 => (index < trace.result) as usize,
            2 => 0,
            _ => 0,
        }
    }

    #[test]
    fn native_string_stops_before_unreadable_byte() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 2,
        };
        let copied = unsafe {
            a2fo_rs_stage9_craft_copy_native_string(
                Some(native_string_step),
                (&raw mut trace).cast(),
                8,
            )
        };
        assert_eq!(copied, 0);
        assert_eq!(
            trace.actions,
            [(0, 0), (1, 0), (2, 0), (1, 1), (2, 1), (1, 2)]
        );
    }

    #[test]
    fn native_vector_releases_entries_then_buffer_then_clears_bounds() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_release_native_vector(
                Some(native_string_step),
                (&raw mut trace).cast(),
                2,
            )
        };
        assert_eq!(trace.actions, [(0, 0), (0, 1), (1, 0), (2, 0)]);
    }

    unsafe extern "C" fn reset_hover_step(
        context: *mut c_void,
        action: u32,
        index: usize,
    ) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        if action == 0 {
            trace.visible[index] as usize
        } else {
            0
        }
    }

    #[test]
    fn hover_reset_clears_once_then_zeros_after_first_presented() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false, true, true],
            result: 0,
        };
        unsafe {
            a2fo_rs_stage9_craft_reset_hover(Some(reset_hover_step), (&raw mut trace).cast(), 3);
        }
        assert_eq!(trace.actions, [(0, 0), (0, 1), (1, 0), (2, 0)]);
    }

    unsafe extern "C" fn find_hover_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        trace.visible[index] as usize
    }

    #[test]
    fn hover_row_search_stops_after_first_hit() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false, true, true],
            result: 0,
        };
        let selected = unsafe {
            a2fo_rs_stage9_craft_find_hover_row(Some(find_hover_step), (&raw mut trace).cast(), 3)
        };
        assert_eq!(selected, 1);
        assert_eq!(trace.actions, [(0, 0), (0, 1)]);
    }

    unsafe extern "C" fn panel_step(context: *mut c_void, action: u32, index: usize) -> usize {
        let trace = unsafe { &mut *context.cast::<Trace>() };
        trace.actions.push((action, index));
        match action {
            1 | 3 | 8 | 12 | 22 => 1,
            9 => trace.result,
            _ => 0,
        }
    }

    #[test]
    fn panel_restores_name_colours_in_reverse_before_selection_read() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 1,
        };
        unsafe { a2fo_rs_stage9_craft_render_panel(Some(panel_step), (&raw mut trace).cast(), 2) };
        let actions = &trace.actions;
        assert_eq!(
            &actions[..10],
            &[
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (4, 1),
                (5, 0),
                (6, 0),
                (7, 1),
                (7, 0),
                (8, 0)
            ]
        );
        assert_eq!(actions.last(), Some(&(28, 0)));
    }

    #[test]
    fn panel_missing_selection_keeps_native_hover_reset_order() {
        let mut trace = Trace {
            actions: Vec::new(),
            visible: [false; 3],
            result: 0,
        };
        unsafe { a2fo_rs_stage9_craft_render_panel(Some(panel_step), (&raw mut trace).cast(), 0) };
        assert_eq!(
            trace.actions,
            [
                (1, 0),
                (2, 0),
                (3, 0),
                (5, 0),
                (6, 0),
                (8, 0),
                (9, 0),
                (10, 0)
            ]
        );
    }
}
