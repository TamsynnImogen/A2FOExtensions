use core::ffi::c_char;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Area {
    x: i32,
    y: i32,
    width: i32,
    height: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Rect {
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}

fn rectangle(area: Area) -> Rect {
    Rect {
        left: area.x,
        top: area.y,
        right: area.x.wrapping_add(area.width).wrapping_sub(1),
        bottom: area.y.wrapping_add(area.height).wrapping_sub(1),
    }
}

/// Borrow a caller-provided byte span for one synchronous policy decision.
///
/// # Safety
/// When `len` is nonzero, `pointer` must point to `len` initialized readable
/// bytes that remain alive and unmodified for the returned borrow's lifetime.
/// A null pointer is accepted only when `len` is zero.
unsafe fn bytes<'a>(pointer: *const u8, len: usize) -> Option<&'a [u8]> {
    if len == 0 {
        return Some(&[]);
    }
    if pointer.is_null() {
        return None;
    }
    Some(unsafe { core::slice::from_raw_parts(pointer, len) })
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_ui_restore(pending: u8, matches: u8, menu: u32) -> u8 {
    u8::from(pending != 0 && matches != 0 && menu == 2)
}

#[unsafe(no_mangle)]
/// Set the UI mode outputs from the legacy/control and dimensions flags.
///
/// # Safety
/// If any output pointer is null, the function returns without writing.
/// Otherwise each output pointer must be properly aligned and writable for its
/// pointee type, and the four output spans must be nonoverlapping.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_decide(
    speed: u8,
    control: u8,
    width: u8,
    height: u8,
    legacy: *mut u8,
    apply: *mut u8,
    reference_width: *mut i32,
    reference_height: *mut i32,
) {
    if legacy.is_null()
        || apply.is_null()
        || reference_width.is_null()
        || reference_height.is_null()
    {
        return;
    }
    let is_legacy = speed != 0 && control != 0;
    let use_reference = is_legacy && width == 0 && height == 0;
    unsafe {
        legacy.write(u8::from(is_legacy));
        apply.write(u8::from(use_reference));
        reference_width.write(if use_reference { 640 } else { 0 });
        reference_height.write(if use_reference { 480 } else { 0 });
    }
}

#[unsafe(no_mangle)]
/// Convert an area to its inclusive rectangle.
///
/// # Safety
/// If either pointer is null, the function returns without writing. Otherwise
/// `area` must be properly aligned and readable for one `Area`, and `out` must
/// be properly aligned and writable for one `Rect`. The spans must not overlap.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_rect(area: *const Area, out: *mut Rect) {
    if let (Some(area), Some(out)) = (unsafe { area.as_ref() }, unsafe { out.as_mut() }) {
        *out = rectangle(*area);
    }
}

#[unsafe(no_mangle)]
/// Report whether an area has positive width and height.
///
/// # Safety
/// A null `area` returns zero. Otherwise it must be properly aligned and
/// readable for one `Area` for the duration of this call.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_usable(area: *const Area) -> u8 {
    unsafe { area.as_ref() }.map_or(0, |area| u8::from(area.width > 0 && area.height > 0))
}

#[unsafe(no_mangle)]
/// Compute the bounding union of three areas.
///
/// # Safety
/// If any pointer is null, the function returns without writing. Otherwise
/// each non-null input must be properly aligned and readable for one `Area`;
/// `out` must be properly aligned and writable for one `Area`. The output span
/// must not overlap any input span. Input spans may overlap one another.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_union(
    first: *const Area,
    second: *const Area,
    third: *const Area,
    out: *mut Area,
) {
    let (Some(first), Some(second), Some(third), Some(out)) = (
        unsafe { first.as_ref() },
        unsafe { second.as_ref() },
        unsafe { third.as_ref() },
        unsafe { out.as_mut() },
    ) else {
        return;
    };
    let left = if first.x < second.x {
        if first.x < third.x { first.x } else { third.x }
    } else if second.x < third.x {
        second.x
    } else {
        third.x
    };
    let top = if first.y < second.y {
        if first.y < third.y { first.y } else { third.y }
    } else if second.y < third.y {
        second.y
    } else {
        third.y
    };
    let first_right = first.x.wrapping_add(first.width);
    let second_right = second.x.wrapping_add(second.width);
    let third_right = third.x.wrapping_add(third.width);
    let right = if first_right > second_right {
        if first_right > third_right {
            first_right
        } else {
            third_right
        }
    } else if second_right > third_right {
        second_right
    } else {
        third_right
    };
    let first_bottom = first.y.wrapping_add(first.height);
    let second_bottom = second.y.wrapping_add(second.height);
    let third_bottom = third.y.wrapping_add(third.height);
    let bottom = if first_bottom > second_bottom {
        if first_bottom > third_bottom {
            first_bottom
        } else {
            third_bottom
        }
    } else if second_bottom > third_bottom {
        second_bottom
    } else {
        third_bottom
    };
    *out = Area {
        x: left,
        y: top,
        width: right.wrapping_sub(left),
        height: bottom.wrapping_sub(top),
    };
}

#[unsafe(no_mangle)]
/// Translate a child area from combined-area coordinates into panel coordinates.
///
/// # Safety
/// If any pointer is null, the function returns without writing. Otherwise
/// each non-null input must be properly aligned and readable for one `Area`;
/// `out` must be properly aligned and writable for one `Area`. The output span
/// must not overlap any input span. Input spans may overlap one another.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_child_union(
    panel: *const Area,
    child: *const Area,
    combined: *const Area,
    out: *mut Area,
) {
    let (Some(panel), Some(child), Some(combined), Some(out)) = (
        unsafe { panel.as_ref() },
        unsafe { child.as_ref() },
        unsafe { combined.as_ref() },
        unsafe { out.as_mut() },
    ) else {
        return;
    };
    *out = Area {
        x: panel.x.wrapping_sub(combined.x).wrapping_add(child.x),
        y: panel.y.wrapping_sub(combined.y).wrapping_add(child.y),
        width: child.width,
        height: child.height,
    };
}

#[unsafe(no_mangle)]
/// Translate a child area from parent-relative coordinates.
///
/// # Safety
/// If any pointer is null, the function returns without writing. Otherwise
/// each non-null input must be properly aligned and readable for one `Area`;
/// `out` must be properly aligned and writable for one `Area`. The output span
/// must not overlap either input span. Input spans may overlap one another.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_child_parent(
    parent: *const Area,
    child: *const Area,
    out: *mut Area,
) {
    let (Some(parent), Some(child), Some(out)) = (
        unsafe { parent.as_ref() },
        unsafe { child.as_ref() },
        unsafe { out.as_mut() },
    ) else {
        return;
    };
    *out = Area {
        x: parent.x.wrapping_add(child.x),
        y: parent.y.wrapping_add(child.y),
        width: child.width,
        height: child.height,
    };
}

#[unsafe(no_mangle)]
/// Compute inclusive rectangles for the component and display areas.
///
/// # Safety
/// If any pointer is null, the function returns without writing. Otherwise
/// Each non-null input must be properly aligned and readable for one `Area`;
/// each non-null output must be properly aligned and writable for one `Rect`.
/// The output spans must not overlap each other or either input span. Input
/// spans may overlap one another.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_race_icon(
    component: *const Area,
    display: *const Area,
    component_out: *mut Rect,
    display_out: *mut Rect,
) {
    let (Some(component), Some(display), Some(component_out), Some(display_out)) = (
        unsafe { component.as_ref() },
        unsafe { display.as_ref() },
        unsafe { component_out.as_mut() },
        unsafe { display_out.as_mut() },
    ) else {
        return;
    };
    *component_out = rectangle(*component);
    *display_out = rectangle(*display);
}

#[unsafe(no_mangle)]
/// Compute the area for a speed-control slot.
///
/// # Safety
/// If any pointer is null, the function returns without writing. Otherwise
/// each non-null input must be properly aligned and readable for one `Area`;
/// `out` must be properly aligned and writable for one `Area`. The output span
/// must not overlap either input span. Input spans may overlap one another.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_speed_slot(
    button: *const Area,
    separator: *const Area,
    gap: i32,
    slot: u32,
    out: *mut Area,
) {
    let (Some(button), Some(separator), Some(out)) = (
        unsafe { button.as_ref() },
        unsafe { separator.as_ref() },
        unsafe { out.as_mut() },
    ) else {
        return;
    };
    let single_advance = button.width.wrapping_add(gap);
    if slot < 5 {
        *out = Area {
            x: button
                .x
                .wrapping_add((slot as i32).wrapping_mul(single_advance)),
            y: button.y,
            width: button.width,
            height: button.height,
        };
        return;
    }
    let separator_offset = 5i32.wrapping_mul(single_advance);
    if slot == 5 {
        *out = Area {
            x: separator.x.wrapping_add(separator_offset),
            y: separator.y,
            width: separator.width,
            height: separator.height,
        };
        return;
    }
    let offset = separator_offset
        .wrapping_add(separator.width)
        .wrapping_add(gap)
        .wrapping_add(((slot - 6) as i32).wrapping_mul(single_advance));
    *out = Area {
        x: button.x.wrapping_add(offset),
        y: button.y,
        width: button.width,
        height: button.height,
    };
}

#[unsafe(no_mangle)]
/// Parse the numeric suffix of a key with the requested prefix.
///
/// # Safety
/// When either length is nonzero, its pointer must reference that many
/// initialized readable bytes for the duration of the call. A null pointer is
/// accepted only with a zero corresponding length. The two spans may overlap;
/// invalid/null non-empty spans return `-1`.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_index(
    key: *const u8,
    key_len: usize,
    prefix: *const u8,
    prefix_len: usize,
    maximum: i32,
) -> i32 {
    let (Some(key), Some(prefix)) = (unsafe { bytes(key, key_len) }, unsafe {
        bytes(prefix, prefix_len)
    }) else {
        return -1;
    };
    if maximum < 0 || key.len() <= prefix.len() || !key.starts_with(prefix) {
        return -1;
    }
    let mut value = 0i32;
    for &digit in &key[prefix.len()..] {
        if !digit.is_ascii_digit() {
            return -1;
        }
        let Some(next) = value
            .checked_mul(10)
            .and_then(|v| v.checked_add(i32::from(digit - b'0')))
        else {
            // The original signed expression has undefined overflow; only
            // defined input domains are compared by the native fixture.
            return -1;
        };
        value = next;
        if value > maximum {
            return -1;
        }
    }
    value
}

const RECT_ALIASES: &[(&[u8], &[u8])] = &[
    (b"infoPanelArea_0", b"infoPanelArea\0"),
    (b"infoPanelArea_1", b"infoPanelArea\0"),
    (b"infoPanelArea_2", b"infoPanelArea\0"),
    (b"infoBlackArea_0", b"infoBlackArea\0"),
    (b"infoBlackArea_1", b"infoBlackArea\0"),
    (b"infoBlackArea_2", b"infoBlackArea\0"),
    (b"infoLowBackgroundPanelArea", b"infoBackgroundPanelArea\0"),
    (
        b"infoMiddleBackgroundPanelArea",
        b"infoBackgroundPanelArea\0",
    ),
    (b"infoCrewNum", b"infoSingleCrewTextInformationArea\0"),
    (b"infoBuildCrew", b"infoSingleCrewTextInformationArea\0"),
    (b"infoCrewNumIcon", b"infoSingleCrewDotArea\0"),
    (b"infoBuildCrewIcon", b"infoSingleCrewDotArea\0"),
    (b"infoOfficers", b"infoSingleOfficerTextInformationArea\0"),
    (
        b"infoBuildOfficers",
        b"infoSingleOfficerTextInformationArea\0",
    ),
    (b"infoBuildClass", b"infoSingleClassTextArea\0"),
    (b"infoBuildName", b"infoSingleNameTextArea\0"),
    (b"infoBuildWireframe", b"infoSingleWireframeIconArea\0"),
    (b"infoBuildShieldBar", b"infoSingleShieldBarArea\0"),
    (b"infoProgressBar", b"infoSingleConstructionBarArea\0"),
    (
        b"infoSingleConstructionArea",
        b"infoSingleConstructionBarArea\0",
    ),
    (b"infoSystemIcon_0", b"infoSystem_0\0"),
    (b"infoSystemIcon_1", b"infoSystem_1\0"),
    (b"infoSystemIcon_2", b"infoSystem_2\0"),
    (b"infoSystemIcon_3", b"infoSystem_3\0"),
    (b"infoSystemIcon_4", b"infoSystem_4\0"),
    (b"infoBuildSystemIcon_0", b"infoSystem_0\0"),
    (b"infoBuildSystemIcon_1", b"infoSystem_1\0"),
    (b"infoBuildSystemIcon_2", b"infoSystem_2\0"),
    (b"infoBuildSystemIcon_3", b"infoSystem_3\0"),
    (b"infoBuildSystemIcon_4", b"infoSystem_4\0"),
];

#[unsafe(no_mangle)]
/// Return the static rectangle-field alias for a key, if one exists.
///
/// # Safety
/// When `len` is nonzero, `key` must reference that many initialized readable
/// bytes for the duration of the call. A null pointer is accepted only when
/// `len` is zero.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_rect_alias(
    key: *const u8,
    len: usize,
) -> *const c_char {
    let Some(key) = (unsafe { bytes(key, len) }) else {
        return core::ptr::null();
    };
    for &(source, target) in RECT_ALIASES {
        if key == source {
            return target.as_ptr().cast();
        }
    }
    core::ptr::null()
}

#[unsafe(no_mangle)]
/// Return the static string-field alias for a key, if one exists.
///
/// # Safety
/// When `len` is nonzero, `key` must reference that many initialized readable
/// bytes for the duration of the call. A null pointer is accepted only when
/// `len` is zero.
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_string_alias(
    key: *const u8,
    len: usize,
) -> *const c_char {
    let Some(key) = (unsafe { bytes(key, len) }) else {
        return core::ptr::null();
    };
    if key == b"infoLowBackgroundPanel" || key == b"infoMiddleBackgroundPanel" {
        return c"infoBackgroundPanel".as_ptr();
    }
    core::ptr::null()
}
