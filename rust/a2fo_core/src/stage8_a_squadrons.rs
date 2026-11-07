//! Portable sequencing for the native Squadron module.

use core::ffi::c_void;

#[repr(C)]
struct BytesView {
    data: *const u8,
    len: usize,
}

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

unsafe fn call(step: Step, context: *mut c_void, op: u32, arg: usize) -> Option<usize> {
    let value = unsafe { step(context, op, arg) };
    (value != usize::MAX).then_some(value)
}

/// Safety: the native context owns the SDK table, synthetic game image,
/// registration buffers and hook slots for this synchronous call. The
/// callback captures C++ exceptions and reports a sentinel before unwinding.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    for op in 1..=2 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    for op in 3..=6 {
        match unsafe { call(step, context, op, 0) } {
            Some(1) => {}
            Some(_) => {
                unsafe { call(step, context, 7, 0) };
                return 0;
            }
            None => return 0,
        }
    }
    if unsafe { call(step, context, 8, 0) }.is_none() {
        return 0;
    }
    let Some(layout) = (unsafe { call(step, context, 9, 0) }) else {
        return 0;
    };
    if unsafe { call(step, context, 10, layout) }.is_none() {
        return 0;
    }
    let Some(repair) = (unsafe { call(step, context, 11, 0) }) else {
        return 0;
    };
    if unsafe { call(step, context, 12, repair) }.is_none() {
        return 0;
    }
    match unsafe { call(step, context, 13, 0) } {
        Some(1) => {}
        Some(_) => {
            if unsafe { call(step, context, 14, 0) }.is_none() {
                return 0;
            }
        }
        None => return 0,
    }
    match unsafe { call(step, context, 15, 0) } {
        Some(1) => {}
        Some(_) => {
            if unsafe { call(step, context, 16, 0) }.is_none() {
                return 0;
            }
        }
        None => return 0,
    }
    let Some(rows) = (unsafe { call(step, context, 17, 0) }) else {
        return 0;
    };
    for index in 0..3 {
        if unsafe { call(step, context, 30, index) }.is_none() {
            return 0;
        }
    }
    for index in 0..rows.saturating_mul(2) {
        if unsafe { call(step, context, 31, index) }.is_none() {
            return 0;
        }
    }
    for index in 0..4 {
        if unsafe { call(step, context, 32, index) }.is_none() {
            return 0;
        }
    }
    if unsafe { call(step, context, 33, 0) }.is_none() {
        return 0;
    }
    let mut all_registered = true;
    for op in 18..=20 {
        match unsafe { call(step, context, op, 0) } {
            Some(1) => {}
            Some(_) => all_registered = false,
            None => return 0,
        }
    }
    if unsafe { call(step, context, 34, 0) }.is_none() {
        return 0;
    }
    for index in 0..100 {
        if unsafe { call(step, context, 35, index) }.is_none() {
            return 0;
        }
    }
    match unsafe { call(step, context, 36, 0) } {
        Some(1) => match unsafe { call(step, context, 37, 0) } {
            Some(1) => {}
            Some(_) => all_registered = false,
            None => return 0,
        },
        Some(_) => all_registered = false,
        None => return 0,
    }
    match unsafe { call(step, context, 22, 0) } {
        Some(1) => {}
        Some(_) => all_registered = false,
        None => return 0,
    }
    let craft_registered = match unsafe { call(step, context, 23, 0) } {
        Some(1) => match unsafe { call(step, context, 24, 0) } {
            Some(value) => value != 0,
            None => return 0,
        },
        Some(_) => {
            let Some(legacy) = (unsafe { call(step, context, 25, 0) }) else {
                return 0;
            };
            if legacy != 0 && unsafe { call(step, context, 26, 0) }.is_none() {
                return 0;
            }
            legacy != 0
        }
        None => return 0,
    };
    if !all_registered || !craft_registered {
        if unsafe { call(step, context, 27, 0) }.is_none() {
            return 0;
        }
        if unsafe { call(step, context, 28, 0) }.is_none() {
            return 0;
        }
        return 1;
    }
    if unsafe { call(step, context, 29, 0) }.is_none() {
        return 0;
    }
    1
}

/// Safety: native context owns the borrowed SDK event, copied ODF fields,
/// class maps, parsed definition and original callback exception boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_class(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    for op in 4..=6 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
    match unsafe { call(step, context, 7, 0) } {
        Some(1) => {
            if unsafe { call(step, context, 8, 0) }.is_none() {
                return;
            }
        }
        Some(_) => {}
        None => return,
    }
    if unsafe { call(step, context, 9, 0) }.is_none() {
        return;
    }
    if !matches!(unsafe { call(step, context, 10, 0) }, Some(1)) {
        return;
    }
    match unsafe { call(step, context, 11, 0) } {
        Some(1) => {
            for op in 14..=16 {
                if unsafe { call(step, context, op, 0) }.is_none() {
                    return;
                }
            }
        }
        Some(_) => {
            if unsafe { call(step, context, 12, 0) }.is_none() {
                return;
            }
            unsafe { call(step, context, 13, 0) };
        }
        None => {}
    }
}

/// Safety: native context owns the event, definition map and callback
/// exception boundary. Event constants are copied from the SDK at call time.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_producer(
    step: Option<Step>,
    context: *mut c_void,
    admit: u32,
    starting_effect: u32,
    finishing: u32,
) -> u8 {
    let Some(step) = step else {
        return 1;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 1;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return 1;
    }
    let kind = unsafe { step(context, 12, 0) } as u32;
    if kind == admit {
        match unsafe { call(step, context, 2, 0) } {
            Some(1) => {}
            Some(_) => return 0,
            None => return 2,
        }
        match unsafe { call(step, context, 3, 0) } {
            Some(1) => {
                unsafe { call(step, context, 4, 0) };
                return 0;
            }
            Some(_) => {}
            None => return 2,
        }
        match unsafe { call(step, context, 5, 0) } {
            Some(1) => {}
            Some(_) => {
                unsafe { call(step, context, 6, 0) };
                return 0;
            }
            None => return 2,
        }
        match unsafe { call(step, context, 7, 0) } {
            Some(1) => {
                unsafe { call(step, context, 8, 0) };
                return 0;
            }
            Some(_) => {}
            None => return 2,
        }
        match unsafe { call(step, context, 9, 0) } {
            Some(1) => unsafe { call(step, context, 10, 0) }.map_or(2, |_| 1),
            Some(_) => 1,
            None => 2,
        }
    } else if kind == starting_effect {
        0
    } else if kind == finishing {
        unsafe { call(step, context, 11, 0) }.map_or(2, |_| 1)
    } else {
        1
    }
}

/// Safety: native context owns the live SDK event, registry lock and repair
/// state. Rust resamples kind after each callback, matching the native event
/// read order when callbacks mutate the event in place.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_craft(
    step: Option<Step>,
    context: *mut c_void,
    post_load: u32,
    cleanup: u32,
    simulate_post: u32,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    if unsafe { call(step, context, 2, 0) }.is_none() {
        return;
    }
    let event_kind = unsafe { step(context, 3, 0) };
    if event_kind as u32 == post_load {
        return;
    }
    if unsafe { call(step, context, 4, 0) }.is_none() {
        return;
    }
    let event_kind = unsafe { step(context, 3, 0) };
    if event_kind as u32 == cleanup {
        if unsafe { call(step, context, 5, 0) }.is_none() {
            return;
        }
        let Some(repair_ready) = (unsafe { call(step, context, 6, 0) }) else {
            return;
        };
        if repair_ready == 1 && unsafe { call(step, context, 7, 0) }.is_none() {
            return;
        }
        unsafe { call(step, context, 8, 0) };
        return;
    }
    let event_kind = unsafe { step(context, 3, 0) };
    if event_kind as u32 != simulate_post {
        return;
    }
    if unsafe { call(step, context, 9, 0) }.is_none() {
        return;
    }
    let Some(team) = (unsafe { call(step, context, 10, 0) }) else {
        return;
    };
    if (team as i32) < 0 {
        return;
    }
    if !matches!(unsafe { call(step, context, 11, 0) }, Some(1)) {
        return;
    }
    let Some(repair_ready) = (unsafe { call(step, context, 6, 0) }) else {
        return;
    };
    if repair_ready == 1 {
        unsafe { call(step, context, 12, 0) };
    }
}

/// Safety: native context owns the copied ODF fields and yard class map.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_yard_class(
    step: Option<Step>,
    context: *mut c_void,
    field_count: u32,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    for index in 0..field_count as usize {
        if unsafe { call(step, context, 2, index) }.is_none() {
            return;
        }
        match unsafe { call(step, context, 3, 0) } {
            Some(1) => {
                if unsafe { call(step, context, 4, 0) }.is_none() {
                    return;
                }
            }
            Some(_) => {}
            None => return,
        }
    }
}

/// Safety: native context owns the caller buffer and class economics map.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_additional_costs(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..=2 {
        if !matches!(unsafe { call(step, context, op, 0) }, Some(1)) {
            return 0;
        }
    }
    u8::from(matches!(unsafe { call(step, context, 3, 0) }, Some(1)))
}

/// Safety: the native context owns the pending vector and one moved job.
/// Each iteration re-reads vector size after native callbacks, so reentrant
/// enqueues remain visible exactly as in the original loop.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_pending(
    step: Option<Step>,
    context: *mut c_void,
    kind: u32,
    cleanup: u32,
    simulate_post: u32,
) {
    let Some(step) = step else {
        return;
    };
    let handle = unsafe { step(context, 0, 0) };
    if handle == 0 {
        return;
    }
    let mut index = 0usize;
    loop {
        let Some(size) = (unsafe { call(step, context, 1, 0) }) else {
            return;
        };
        if index >= size {
            return;
        }
        if kind == cleanup {
            match unsafe { call(step, context, 2, index) } {
                Some(1) => {
                    if unsafe { call(step, context, 3, index) }.is_none() {
                        return;
                    }
                    if unsafe { call(step, context, 4, 0) }.is_none() {
                        return;
                    }
                    if unsafe { call(step, context, 16, 0) }.is_none() {
                        return;
                    }
                    continue;
                }
                Some(_) => {}
                None => return,
            }
            if matches!(unsafe { call(step, context, 5, index) }, Some(1))
                && unsafe { call(step, context, 6, index) }.is_none()
            {
                return;
            }
            index += 1;
            continue;
        }
        if kind != simulate_post {
            index += 1;
            continue;
        }
        let current = matches!(unsafe { call(step, context, 7, index) }, Some(1));
        if !current && !matches!(unsafe { call(step, context, 8, index) }, Some(1)) {
            index += 1;
            continue;
        }
        if matches!(unsafe { call(step, context, 9, index) }, Some(1))
            && !matches!(unsafe { call(step, context, 10, 0) }, Some(1))
        {
            index += 1;
            continue;
        }
        if unsafe { call(step, context, 3, index) }.is_none() {
            return;
        }
        if matches!(unsafe { call(step, context, 11, 0) }, Some(1)) {
            if unsafe { call(step, context, 12, 0) }.is_none() {
                return;
            }
        } else {
            let Some(launched) = (unsafe { call(step, context, 13, 0) }) else {
                return;
            };
            if launched != 0 {
                if unsafe { call(step, context, 14, 0) }.is_none() {
                    return;
                }
            } else if unsafe { call(step, context, 15, 0) }.is_none() {
                return;
            }
        }
        if unsafe { call(step, context, 16, 0) }.is_none() {
            return;
        }
    }
}

/// Safety: native context owns the definition, member job, rollback guard,
/// native output queue and locked registry. Rust controls admission, member
/// expansion and commit order without borrowing C++ containers.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_spawn(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let team = unsafe { step(context, 1, 0) } as u32 as i32;
    let handle = unsafe { step(context, 2, 0) };
    let method = unsafe { step(context, 3, 0) };
    if team < 0
        || handle == 0
        || method == 0
        || !matches!(unsafe { call(step, context, 4, 0) }, Some(1))
    {
        unsafe { call(step, context, 5, 0) };
        return 0;
    }
    match unsafe { call(step, context, 6, 0) } {
        Some(1) => {
            unsafe { call(step, context, 7, 0) };
            return 0;
        }
        Some(_) => {}
        None => return 0,
    }
    if unsafe { call(step, context, 8, 0) }.is_none() {
        return 0;
    }
    let Some(rows) = (unsafe { call(step, context, 9, 0) }) else {
        return 0;
    };
    for row in 0..rows {
        let Some(count) = (unsafe { call(step, context, 10, row) }) else {
            return 0;
        };
        for _ in 0..count {
            if unsafe { call(step, context, 11, row) }.is_none() {
                return 0;
            }
        }
    }
    let Some(size) = (unsafe { call(step, context, 12, 0) }) else {
        return 0;
    };
    if size == 0 {
        return 0;
    }
    if size > 1 && unsafe { call(step, context, 13, 0) }.is_none() {
        return 0;
    }
    match unsafe { call(step, context, 14, 0) } {
        Some(value) if value != 0 => {}
        Some(_) => {
            unsafe { call(step, context, 15, 0) };
            return 0;
        }
        None => return 0,
    }
    match unsafe { call(step, context, 16, 0) } {
        Some(1) => {}
        Some(_) => {
            unsafe { call(step, context, 17, 0) };
            return 0;
        }
        None => return 0,
    }
    match unsafe { call(step, context, 18, 0) } {
        Some(1) => {}
        Some(_) => {
            unsafe { call(step, context, 19, 0) };
            return 0;
        }
        None => return 0,
    }
    if unsafe { call(step, context, 20, 0) }.is_none() {
        return 0;
    }
    match unsafe { call(step, context, 21, 0) } {
        Some(1) => {}
        Some(_) => {
            unsafe { call(step, context, 22, 0) };
            return 0;
        }
        None => return 0,
    }
    if unsafe { call(step, context, 23, 0) }.is_none() {
        return 0;
    }
    match unsafe { call(step, context, 24, 0) } {
        Some(1) => {
            if unsafe { call(step, context, 25, 0) }.is_none() {
                return 0;
            }
        }
        Some(_) => {
            if unsafe { call(step, context, 26, 0) }.is_none() {
                return 0;
            }
        }
        None => return 0,
    }
    if unsafe { call(step, context, 27, 0) }.is_none() {
        return 0;
    }
    u8::from(matches!(unsafe { call(step, context, 28, 0) }, Some(1)))
}

/// Safety: native context owns a caller-held PendingLaunch and its rollback
/// guard. Rust samples yard/team state only after entity lookup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_launch_next(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let Some(yard) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    if yard == 0 {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(0)) {
        return 0;
    }
    let team = unsafe { step(context, 3, 0) } as u32 as i32;
    let expected = unsafe { step(context, 4, 0) } as u32 as i32;
    if team != expected {
        return 0;
    }
    let Some(method) = (unsafe { call(step, context, 5, 0) }) else {
        return 0;
    };
    if method == 0 || !matches!(unsafe { call(step, context, 6, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 7, 0) }.is_none() {
        return 0;
    }
    match unsafe { call(step, context, 8, 0) } {
        Some(value) if value != 0 => {}
        _ => return 0,
    }
    if !matches!(unsafe { call(step, context, 9, 0) }, Some(1)) {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 10, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 11, 0) }.is_none() {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 12, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 13, 0) }.is_none() {
        return 0;
    }
    u8::from(matches!(unsafe { call(step, context, 14, 0) }, Some(1)))
}

/// Safety: native callback owns checked producer reads, locked registry,
/// checked hook/gateway calls and the original noexcept exception regions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_finish(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    unsafe { step(context, 0, 0) };
    let found = unsafe { step(context, 1, 0) } != 0 && unsafe { step(context, 2, 0) } != 0;
    if !found {
        unsafe { step(context, 3, 0) };
        let result = unsafe { step(context, 4, 0) };
        unsafe { step(context, 5, result) };
        return result;
    }
    let spawned = unsafe { step(context, 6, 0) } != 0;
    if unsafe { step(context, 7, 0) } == 0 {
        unsafe { step(context, 8, 0) };
    }
    for op in 9..=13 {
        unsafe { step(context, op, 0) };
    }
    unsafe { step(context, 14, 0) };
    unsafe { step(context, 15, usize::from(spawned)) };
    0
}

/// Safety: native callback owns the two checked vtable patches and rollback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_install_finish(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        unsafe { step(context, 3, 0) };
        return 0;
    }
    unsafe { step(context, 4, 0) };
    1
}

/// Safety: label and returned field bytes belong to the native synchronous
/// context. The native callback holds the original pod-group mutex.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_capture_pod(
    step: Option<Step>,
    context: *mut c_void,
    label_data: *const u8,
    label_len: usize,
) {
    let Some(step) = step else {
        return;
    };
    if label_len != 0 && label_data.is_null() {
        return;
    }
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let label = if label_len == 0 {
        &[][..]
    } else {
        unsafe { core::slice::from_raw_parts(label_data, label_len) }
    };
    if label == b"shipyard" && unsafe { call(step, context, 2, 0) }.is_none() {
        return;
    }
    if label != b"research" {
        return;
    }
    let Some(view_pointer) = (unsafe { call(step, context, 3, 0) }) else {
        return;
    };
    let view = unsafe { &*(view_pointer as *const BytesView) };
    if view.len != 1 || view.data.is_null() || unsafe { *view.data } != b'1' {
        return;
    }
    if !matches!(unsafe { call(step, context, 4, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 5, 0) }.is_none() {
        return;
    }
    unsafe { call(step, context, 6, 0) };
}

/// Safety: native callback holds the original pod-group mutex, owns handle
/// storage and performs checked object/vtable reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_track_pod(
    step: Option<Step>,
    context: *mut c_void,
    kind: u32,
    cleanup: u32,
    simulate_post: u32,
    post_load: u32,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return;
    }
    if kind == cleanup {
        unsafe { call(step, context, 1, 0) };
    } else if (kind == simulate_post || kind == post_load)
        && matches!(unsafe { call(step, context, 2, 0) }, Some(1))
    {
        unsafe { call(step, context, 3, 0) };
    }
}

/// Safety: native context owns the pod-root iterator, child array, result
/// vector, resolver callback and original mutex for the synchronous DFS.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_attached_peers(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    for op in 0..=2 {
        match unsafe { call(step, context, op, 0) } {
            Some(value) if (op < 2 && value == 0) || (op == 2 && value != 0) => return,
            Some(_) => {}
            None => return,
        }
    }
    // These raw checked object reads cannot throw; a valid u32::MAX handle
    // must not alias the exception sentinel used by storage callbacks.
    let handle = unsafe { step(context, 3, 0) };
    let team_bits = unsafe { step(context, 4, 0) };
    if handle == 0 || (team_bits as u32 as i32) < 0 {
        return;
    }
    if !matches!(unsafe { call(step, context, 5, 0) }, Some(1)) {
        return;
    }
    let Some(root_click) = (unsafe { call(step, context, 6, 0) }) else {
        return;
    };
    if root_click == 0 && !matches!(unsafe { call(step, context, 7, 0) }, Some(1)) {
        return;
    }
    if root_click != 0 && unsafe { call(step, context, 8, 0) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 9, 0) }.is_none() {
        return;
    }
    loop {
        match unsafe { call(step, context, 10, 0) } {
            Some(1) => {}
            Some(_) => return,
            None => return,
        }
        if unsafe { call(step, context, 11, 0) }.is_none() {
            return;
        }
        let Some(root_found) = (unsafe { call(step, context, 12, 0) }) else {
            return;
        };
        if root_found != 1 {
            if unsafe { call(step, context, 13, 0) }.is_none() {
                return;
            }
            continue;
        }
        if unsafe { call(step, context, 14, 0) }.is_none() {
            return;
        }
        let root_team = unsafe { step(context, 15, 0) };
        if root_team != team_bits {
            continue;
        }
        if root_click != 0 {
            let Some(matched) = (unsafe { call(step, context, 34, 0) }) else {
                return;
            };
            if matched != 1 {
                continue;
            }
        }
        let count = unsafe { step(context, 16, 0) };
        if unsafe { call(step, context, 17, 0) }.is_none() {
            return;
        }
        if count > 64 {
            continue;
        }
        if count != 0 {
            let Some(readable) = (unsafe { call(step, context, 18, count) }) else {
                return;
            };
            if readable != 1 {
                continue;
            }
        }
        if unsafe { call(step, context, 19, 0) }.is_none() {
            return;
        }
        for index in 0..count {
            if unsafe { call(step, context, 20, index) }.is_none() {
                return;
            }
            let Some(first) = (unsafe { call(step, context, 21, 0) }) else {
                return;
            };
            if first != 1 {
                continue;
            }
            let Some(expired) = (unsafe { call(step, context, 22, 0) }) else {
                return;
            };
            if expired != 0 {
                continue;
            }
            let Some(team_match) = (unsafe { call(step, context, 23, 0) }) else {
                return;
            };
            if team_match != 1 {
                continue;
            }
            let Some(parent_match) = (unsafe { call(step, context, 24, 0) }) else {
                return;
            };
            if parent_match != 1 {
                continue;
            }
            let child_handle = unsafe { step(context, 25, 0) };
            if child_handle == 0 {
                continue;
            }
            let Some(duplicate) = (unsafe { call(step, context, 26, 0) }) else {
                return;
            };
            if duplicate != 1 {
                continue;
            }
            if unsafe { call(step, context, 27, 0) }.is_none() {
                return;
            }
            let Some(new_member) = (unsafe { call(step, context, 28, 0) }) else {
                return;
            };
            if new_member == 1 && unsafe { call(step, context, 29, 0) }.is_none() {
                return;
            }
        }
        let Some(root_ready) = (unsafe { call(step, context, 30, 0) }) else {
            return;
        };
        if root_ready != 1 {
            if unsafe { call(step, context, 35, 0) }.is_none() {
                return;
            }
            continue;
        }
        let members = unsafe { step(context, 31, 0) };
        for index in 0..members {
            let member = unsafe { step(context, 32, index) };
            if member != handle && unsafe { call(step, context, 33, member) }.is_none() {
                return;
            }
        }
        return;
    }
}

/// Safety: native context holds the original registry mutex and owns the
/// visit, craft reads, repair context and x87-sensitive proximity kernel.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_observe_repair(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return 0;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return 0;
    }
    for op in 2..=4 {
        if !matches!(unsafe { call(step, context, op, 0) }, Some(1)) {
            return 0;
        }
    }
    if unsafe { call(step, context, 5, 0) }.is_none() {
        return 0;
    }
    let Some(count) = (unsafe { call(step, context, 6, 0) }) else {
        return 0;
    };
    for index in 0..count {
        match unsafe { call(step, context, 7, index) } {
            Some(1) => {}
            Some(_) => continue,
            None => return 0,
        }
        if unsafe { call(step, context, 8, 0) }.is_none() {
            return 0;
        }
        if !matches!(unsafe { call(step, context, 9, 0) }, Some(1)) {
            return 0;
        }
        if !matches!(unsafe { call(step, context, 10, 0) }, Some(0)) {
            return 0;
        }
        let team = unsafe { step(context, 11, 0) };
        let expected_team = unsafe { step(context, 20, 0) };
        if team != expected_team {
            return 0;
        }
        let Some(command) = (unsafe { call(step, context, 12, 0) }) else {
            return 0;
        };
        let repair = command == 0x11 || command == 0x35;
        if !repair
            && command != 0
            && command != 3
            && (command != 0x0a || !matches!(unsafe { call(step, context, 13, 0) }, Some(1)))
        {
            return 0;
        }
        let stage = unsafe { step(context, 14, 0) };
        if stage != 0 && !matches!(unsafe { call(step, context, 15, 0) }, Some(1)) {
            return 0;
        }
        let Some(serviced) = (unsafe { call(step, context, 13, 0) }) else {
            return 0;
        };
        if serviced == 0 {
            if !repair {
                return 0;
            }
            if unsafe { call(step, context, 16, 0) }.is_none() {
                return 0;
            }
        } else {
            if !matches!(unsafe { call(step, context, 17, 0) }, Some(1)) {
                return 0;
            }
            if unsafe { call(step, context, 18, 0) }.is_none() {
                return 0;
            }
            if stage != 0 && unsafe { call(step, context, 16, 0) }.is_none() {
                return 0;
            }
        }
    }
    u8::from(matches!(unsafe { call(step, context, 19, 0) }, Some(1)))
}

/// Safety: native callback preserves the checked thiscall bridge and holds
/// the original registry lock while Rust traverses pending launch IDs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_needs_repair(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    let native = unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0
        || unsafe { step(context, 2, 0) } == 0
        || unsafe { step(context, 3, 0) } != 0
        || (native & 0xff) != 0
    {
        return native;
    }
    if unsafe { step(context, 4, 0) } == 0 {
        return native;
    }
    if unsafe { step(context, 5, 0) } == 0 {
        return native;
    }
    if unsafe { step(context, 6, 0) } != 0 {
        return native;
    }
    let count = unsafe { step(context, 7, 0) };
    for index in 0..count {
        if unsafe { step(context, 8, index) } != 0 {
            return native;
        }
    }
    if unsafe { step(context, 9, 0) } != 0 && unsafe { step(context, 10, 0) } != 0 {
        return native;
    }
    1
}

/// Safety: native context captures observation exceptions, owns the
/// registry lock and TLS flag, and calls the original repair Activate bridge.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_activate_repair(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    let observation = (|| -> Option<()> {
        if unsafe { call(step, context, 0, 0)? } == 0 || unsafe { call(step, context, 1, 0)? } == 0
        {
            return Some(());
        }
        let command = unsafe { call(step, context, 2, 0)? };
        if command != 0x11 && command != 0x35 {
            return Some(());
        }
        unsafe { call(step, context, 3, 0)? };
        if unsafe { call(step, context, 4, 0)? } == 0
            || unsafe { call(step, context, 5, 0)? } == 0
            || unsafe { call(step, context, 6, 0)? } == 0
            || unsafe { call(step, context, 7, 0)? } == 0
        {
            return Some(());
        }
        if unsafe { call(step, context, 8, 0)? } == 0 {
            unsafe { call(step, context, 9, 0)? };
        }
        if unsafe { call(step, context, 10, 0)? } != 0 {
            unsafe { call(step, context, 11, 0)? };
        }
        Some(())
    })();
    unsafe { step(context, 16, 0) };
    if observation.is_none() && unsafe { step(context, 17, 0) } != 0 {
        unsafe { step(context, 15, 0) };
    }
    unsafe { step(context, 12, 0) };
    let result = unsafe { step(context, 13, 0) };
    unsafe { step(context, 14, 0) };
    result
}

/// Safety: native context owns the visit snapshot and checked paid-job
/// cancellation callback. Rust preserves erase-before-callback ordering.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_end_repair(
    step: Option<Step>,
    context: *mut c_void,
    cancel_native: u8,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    for op in 1..=4 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
    if cancel_native == 0 {
        return;
    }
    if !matches!(unsafe { call(step, context, 5, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 6, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 7, 0) }, Some(0)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 8, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 9, 0) }, Some(1)) {
        return;
    }
    unsafe { call(step, context, 10, 0) };
}

/// Safety: native context owns the live ordered visit iterator, result
/// object, registry lock and paid-job completion/cancellation callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_finish_repair(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return;
    }
    loop {
        match unsafe { call(step, context, 1, 0) } {
            Some(1) => {}
            Some(_) => return,
            None => return,
        }
        let Some(visit_yard) = (unsafe { call(step, context, 2, 0) }) else {
            return;
        };
        let Some(yard_handle) = (unsafe { call(step, context, 3, 0) }) else {
            return;
        };
        if visit_yard != yard_handle {
            if unsafe { call(step, context, 6, 0) }.is_none() {
                return;
            }
            continue;
        }
        let Some(has_queue) = (unsafe { call(step, context, 4, 0) }) else {
            return;
        };
        if has_queue != 1 {
            if unsafe { call(step, context, 6, 0) }.is_none() {
                return;
            }
            continue;
        }
        let Some(matches_queue) = (unsafe { call(step, context, 5, 0) }) else {
            return;
        };
        if matches_queue != 1 {
            if unsafe { call(step, context, 6, 0) }.is_none() {
                return;
            }
            continue;
        }
        let mut committed = false;
        let Some(has_result) = (unsafe { call(step, context, 7, 0) }) else {
            return;
        };
        let observed = if has_result == 1 {
            let Some(value) = (unsafe { call(step, context, 8, 0) }) else {
                return;
            };
            value == 1
        } else {
            false
        };
        let waiting = if observed {
            let Some(value) = (unsafe { call(step, context, 9, 0) }) else {
                return;
            };
            value != 0
        } else {
            false
        };
        if has_result == 1 && observed && !waiting {
            if unsafe { call(step, context, 10, 0) }.is_none() {
                return;
            }
            let Some(value) = (unsafe { call(step, context, 11, 0) }) else {
                return;
            };
            committed = value == 1;
        }
        if !committed {
            if unsafe { call(step, context, 12, 0) }.is_none() {
                return;
            }
            unsafe { call(step, context, 13, 0) };
            return;
        }
        for op in 14..=18 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                return;
            }
        }
        return;
    }
}

/// Safety: native context owns the live ordered visit map, affected-ID
/// snapshot, registry locks, paid bridge and x87 retry arithmetic kernel.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_update_repair(
    step: Option<Step>,
    context: *mut c_void,
    kind: u32,
    cleanup: u32,
    simulate_post: u32,
    seconds_bits: u32,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    let Some(needs_bridge) = (unsafe { call(step, context, 1, 0) }) else {
        return;
    };
    if needs_bridge == 1 {
        if unsafe { call(step, context, 2, 0) }.is_none() {
            return;
        }
        let Some(ready) = (unsafe { call(step, context, 3, 0) }) else {
            return;
        };
        if ready == 1 && unsafe { call(step, context, 4, 0) }.is_none() {
            return;
        }
        let Some(missing) = (unsafe { call(step, context, 5, 0) }) else {
            return;
        };
        if missing == 1 {
            let Some(unreported) = (unsafe { call(step, context, 6, 0) }) else {
                return;
            };
            if unreported == 1 && unsafe { call(step, context, 7, 0) }.is_none() {
                return;
            }
        }
    }
    let handle = unsafe { step(context, 8, 0) };
    if unsafe { call(step, context, 9, 0) }.is_none() {
        return;
    }
    loop {
        match unsafe { call(step, context, 10, 0) } {
            Some(1) => {}
            Some(_) => break,
            None => return,
        }
        if unsafe { call(step, context, 12, 0) }.is_none() {
            return;
        }
        let Some(first) = (unsafe { call(step, context, 13, 0) }) else {
            return;
        };
        let mut affected = first == 1;
        if !affected {
            let Some(next) = (unsafe { call(step, context, 14, handle) }) else {
                return;
            };
            affected = next == 1;
        }
        if !affected {
            let Some(next) = (unsafe { call(step, context, 15, handle) }) else {
                return;
            };
            affected = next == 1;
        }
        if !affected && kind == cleanup {
            let Some(next) = (unsafe { call(step, context, 16, handle) }) else {
                return;
            };
            affected = next == 1;
        }
        if affected && unsafe { call(step, context, 17, 0) }.is_none() {
            return;
        }
        if unsafe { call(step, context, 18, 0) }.is_none() {
            return;
        }
    }
    let Some(count) = (unsafe { call(step, context, 19, 0) }) else {
        return;
    };
    for index in 0..count {
        let Some(present) = (unsafe { call(step, context, 20, index) }) else {
            return;
        };
        if present != 1 {
            continue;
        }
        if kind == cleanup {
            let Some(same_yard) = (unsafe { call(step, context, 21, handle) }) else {
                return;
            };
            if same_yard == 1 {
                if unsafe { call(step, context, 22, 0) }.is_none() {
                    return;
                }
                continue;
            }
        }
        match unsafe { call(step, context, 23, 0) } {
            Some(1) => {}
            Some(_) => {
                if unsafe { call(step, context, 24, 0) }.is_none() {
                    return;
                }
                continue;
            }
            None => return,
        }
        if unsafe { call(step, context, 25, 0) }.is_none() {
            return;
        }
        let queue_id = unsafe { step(context, 26, 0) };
        if queue_id != 0 {
            let Some(exists) = (unsafe { call(step, context, 27, 0) }) else {
                return;
            };
            if exists != 1 && unsafe { call(step, context, 28, 0) }.is_none() {
                return;
            }
            continue;
        }
        let Some(waiting) = (unsafe { call(step, context, 29, 0) }) else {
            return;
        };
        if waiting == 1 {
            continue;
        }
        let Some(push) = (unsafe { call(step, context, 30, 0) }) else {
            return;
        };
        if push != 1 {
            continue;
        }
        let Some(cancel) = (unsafe { call(step, context, 31, 0) }) else {
            return;
        };
        if cancel != 1 || kind != simulate_post {
            continue;
        }
        if unsafe { call(step, context, 32, 0) }.is_none() {
            return;
        }
        let Some(found) = (unsafe { call(step, context, 33, 0) }) else {
            return;
        };
        let mut proceed = found == 1;
        if proceed {
            let Some(representative) = (unsafe { call(step, context, 34, handle) }) else {
                return;
            };
            proceed = representative == 1;
        }
        if proceed {
            let Some(full) = (unsafe { call(step, context, 35, 0) }) else {
                return;
            };
            if full == 1 {
                if unsafe { call(step, context, 36, 0) }.is_none() {
                    return;
                }
                proceed = false;
            }
        }
        if unsafe { call(step, context, 37, 0) }.is_none() {
            return;
        }
        if !proceed {
            continue;
        }
        let seconds = f32::from_bits(seconds_bits);
        if seconds.is_finite() && seconds > 0.0 && unsafe { call(step, context, 38, 0) }.is_none() {
            return;
        }
        let Some(retrying) = (unsafe { call(step, context, 39, 0) }) else {
            return;
        };
        if retrying == 1 {
            continue;
        }
        if unsafe { call(step, context, 40, 0) }.is_none() {
            return;
        }
        let Some(reserved) = (unsafe { call(step, context, 41, 0) }) else {
            return;
        };
        if reserved != 1 {
            continue;
        }
        if unsafe { call(step, context, 42, 0) }.is_none() {
            return;
        }
        let target = unsafe { step(context, 43, 0) };
        if target != 0 && unsafe { call(step, context, 44, 0) }.is_none() {
            return;
        }
        let pushed = unsafe { step(context, 45, 0) };
        if pushed == 0 {
            if unsafe { call(step, context, 46, 0) }.is_none() {
                return;
            }
            continue;
        }
        for op in 47..=49 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                return;
            }
        }
    }
}

/// Safety: native callback performs guarded linked-list reads only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_native_job_exists(
    step: Option<Step>,
    context: *mut c_void,
    job_id: u32,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let mut node = unsafe { step(context, 0, 0) };
    for _ in 0..10 {
        if node == 0 {
            break;
        }
        if unsafe { step(context, 1, 0) } as u32 == job_id {
            return 1;
        }
        node = unsafe { step(context, 2, 0) };
    }
    0
}

/// Safety: callback owns the native image, checked vtable slot, protection
/// state and inline hook. Rust sequences validation and partial installation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_install_repair(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    // The native source forms both pointers before testing the API and slot.
    unsafe { step(context, 0, 0) };
    for op in 1..=5 {
        if unsafe { step(context, op, 0) } != 1 {
            unsafe { step(context, 6, 0) };
            return 0;
        }
    }
    if unsafe { step(context, 7, 0) } != 1 {
        return 0;
    }
    let installed = unsafe { step(context, 8, 0) } == 1;
    if installed {
        unsafe { step(context, 9, 0) };
    } else {
        unsafe { step(context, 10, 0) };
    }
    unsafe { step(context, 11, 0) };
    u8::from(installed)
}

/// Safety: native context owns the selection, validated peer vector and
/// gateway. It captures exceptions and performs its original catch fallback
/// after Rust returns; no native exception crosses this boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_select(
    step: Option<Step>,
    context: *mut c_void,
    mode: i32,
    clear_selection: i32,
    selection_limit: u32,
) {
    let Some(step) = step else {
        return;
    };
    let operation = if mode == 2 {
        if clear_selection == 0 {
            let Some(selected) = (unsafe { call(step, context, 0, 0) }) else {
                return;
            };
            if selected != 0 { 1 } else { 0 }
        } else {
            0
        }
    } else {
        mode
    };
    if operation != 0 && operation != 1 {
        unsafe { call(step, context, 1, 0) };
        return;
    }
    if unsafe { call(step, context, 2, 0) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    let Some(initial_count) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    if initial_count == 0 && unsafe { call(step, context, 5, 0) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 6, 0) }.is_none() {
        return;
    }
    let Some(peer_count) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    if peer_count == 0 {
        unsafe { call(step, context, 1, 0) };
        return;
    }
    if unsafe { call(step, context, 7, operation as usize) }.is_none() {
        return;
    }
    for index in 0..peer_count {
        let Some(valid) = (unsafe { call(step, context, 8, index) }) else {
            return;
        };
        if valid != 1 {
            return;
        }
    }
    if operation == 0 {
        let Some(stored_count) = (unsafe { call(step, context, 9, 0) }) else {
            return;
        };
        if stored_count > selection_limit as usize {
            return;
        }
        let Some(readable) = (unsafe { call(step, context, 10, 0) }) else {
            return;
        };
        if readable != 1 {
            return;
        }
        let count = if clear_selection != 0 {
            0
        } else {
            stored_count
        };
        let Some(clicked_present) = (unsafe { call(step, context, 11, count) }) else {
            return;
        };
        let mut needed = usize::from(clicked_present == 0);
        for index in 0..peer_count {
            let Some(peer_present) = (unsafe { call(step, context, 12, index) }) else {
                return;
            };
            if peer_present == 0 {
                needed += 1;
            }
        }
        if needed > selection_limit as usize - count {
            unsafe { call(step, context, 13, 0) };
            return;
        }
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let Some(pod_group) = (unsafe { call(step, context, 14, 0) }) else {
        return;
    };
    if pod_group == 1 && operation == 0 {
        let Some(selected) = (unsafe { call(step, context, 0, 0) }) else {
            return;
        };
        if selected == 0 {
            return;
        }
    }
    for index in 0..peer_count {
        if unsafe { call(step, context, 15, (index << 1) | operation as usize) }.is_none() {
            return;
        }
    }
    for op in 16..=17 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
}

/// Safety: native callback owns the selected-handle buffer and registry lock.
/// It copies each ID into stable context storage before Rust advances.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_logical_selection(
    step: Option<Step>,
    context: *mut c_void,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 2, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 3, 0) } != 1 {
        return 0;
    }
    let count = unsafe { step(context, 10, 0) };
    if unsafe { step(context, 4, 0) } != 1 {
        return 0;
    }
    let mut seen_count = 0;
    let mut logical_count = 0;
    for index in 0..count {
        if unsafe { step(context, 5, index) } != 1 {
            continue;
        }
        if unsafe { step(context, 6, 0) } == 1 {
            let mut duplicate = false;
            for prior in 0..seen_count {
                if unsafe { step(context, 7, prior) } == 1 {
                    duplicate = true;
                    break;
                }
            }
            if duplicate {
                continue;
            }
            unsafe { step(context, 8, seen_count) };
            seen_count += 1;
        }
        unsafe { step(context, 9, logical_count) };
        logical_count += 1;
    }
    logical_count as u32
}

/// Safety: native callback holds the UI install mutex, validates executable
/// image addresses and owns the vtable/protection writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_install_ui(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 1 {
        return;
    }
    for index in 0..6 {
        if unsafe { step(context, 1, index) } != 1 {
            unsafe { step(context, 20, 0) };
            return;
        }
    }
    if unsafe { step(context, 2, 0) } == 0 {
        unsafe { step(context, 20, 1) };
        return;
    }
    if unsafe { step(context, 3, 0) } == 0 {
        unsafe { step(context, 20, 2) };
        return;
    }
    if unsafe { step(context, 4, 0) } != 1 {
        unsafe { step(context, 20, 3) };
        return;
    }
    if unsafe { step(context, 5, 0) } != 1 {
        unsafe { step(context, 20, 4) };
        return;
    }
    unsafe { step(context, 6, 0) };
    unsafe { step(context, 7, 0) };
    if unsafe { step(context, 8, 0) } != 1 {
        unsafe { step(context, 9, 0) };
        unsafe { step(context, 20, 5) };
        return;
    }
    if unsafe { step(context, 10, 0) } != 1 {
        unsafe { step(context, 20, 6) };
        return;
    }
    for op in 11..=14 {
        unsafe { step(context, op, 0) };
    }
}

/// Safety: native callback owns stack vtable copies for the synchronous
/// render, TLS binding and checked UI object reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_render_panel(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 1 {
        unsafe { step(context, 2, 0) };
        return;
    }
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    for index in 0..16 {
        unsafe { step(context, 5, index) };
    }
    unsafe { step(context, 6, 0) };
    unsafe { step(context, 2, 0) };
    let count = unsafe { step(context, 7, 0) };
    for index in 0..count {
        unsafe { step(context, 8, index) };
    }
    unsafe { step(context, 9, 0) };
    if unsafe { step(context, 10, 0) } == 1 {
        unsafe { step(context, 11, 0) };
    } else {
        let logical = unsafe { step(context, 12, 0) };
        for index in 0..logical.min(16) {
            unsafe { step(context, 13, index) };
        }
    }
}

/// Safety: native callback owns the yard build-item iterator and validated
/// object reads. The map lookup intentionally precedes the null-yard gate.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_repair_compatible(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return 0;
    }
    for op in 1..=6 {
        match unsafe { call(step, context, op, 0) } {
            Some(1) => {}
            _ => return 0,
        }
    }
    1
}

/// Safety: native callback owns the normalized key, class-map storage and
/// copied stable label bytes for the duration of this synchronous lookup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_resolve_member(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let Some(key_ready) = (unsafe { call(step, context, 0, 0) }) else {
        return 0;
    };
    if key_ready == 0 {
        return 0;
    }
    let Some(cached) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    if cached == 0 {
        let Some(found) = (unsafe { call(step, context, 2, 0) }) else {
            return 0;
        };
        if found == 0 {
            return 0;
        }
    }
    let Some(squadron) = (unsafe { call(step, context, 3, 0) }) else {
        return 0;
    };
    if squadron != 0 {
        return 3;
    }
    let Some(view_address) = (unsafe { call(step, context, 4, 0) }) else {
        return 0;
    };
    let view = unsafe { &*(view_address as *const BytesView) };
    if view.len != 0 && view.data.is_null() {
        return 0;
    }
    let label = if view.len == 0 {
        &[][..]
    } else {
        unsafe { core::slice::from_raw_parts(view.data, view.len) }
    };
    if label == b"squadron" {
        return 3;
    }
    if matches!(
        label,
        b"background_obj"
            | b"sensor"
            | b"turret"
            | b"shipyard"
            | b"research"
            | b"researchstation"
            | b"starbase"
            | b"station"
            | b"planet"
            | b"asteroid"
            | b"nebula"
            | b"weapon"
            | b"ordnance"
            | b"projectile"
    ) {
        return 2;
    }
    if !label.is_empty() && unsafe { call(step, context, 5, 0) }.is_none() {
        return 0;
    }
    1
}

/// Safety: native callback owns SDK field views and copied C++ string
/// storage; each field is inspected at its original index.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_copy_fields(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let Some(valid) = (unsafe { call(step, context, 0, 0) }) else {
        return;
    };
    if valid == 0 {
        return;
    }
    let count = unsafe { step(context, 1, 0) };
    if unsafe { call(step, context, 2, count) }.is_none() {
        return;
    }
    for index in 0..count {
        let Some(valid) = (unsafe { call(step, context, 3, index) }) else {
            return;
        };
        if valid == 1 && unsafe { call(step, context, 4, index) }.is_none() {
            return;
        }
    }
}

/// Safety: native callback owns mutable string storage and retains the CRT
/// tolower primitive, including its active locale behavior.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_lower_label(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let count = unsafe { step(context, 0, 0) };
    for index in 0..count {
        if unsafe { call(step, context, 1, index) }.is_none() {
            return;
        }
    }
}

/// Safety: native callback owns the fields, CRT case-folding comparison and
/// caller-owned return string. Borrowed matched bytes stay stable until the
/// substring callback returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_field_value(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let Some(named) = (unsafe { call(step, context, 0, 0) }) else {
        return;
    };
    if named == 0 {
        return;
    }
    let Some(count) = (unsafe { call(step, context, 1, 0) }) else {
        return;
    };
    for index in 0..count {
        let Some(matched) = (unsafe { call(step, context, 2, index) }) else {
            return;
        };
        if matched == 0 {
            continue;
        }
        let Some(address) = (unsafe { call(step, context, 3, index) }) else {
            return;
        };
        let view = unsafe { &*(address as *const BytesView) };
        if view.len != 0 && view.data.is_null() {
            return;
        }
        let bytes = if view.len == 0 {
            &[][..]
        } else {
            unsafe { core::slice::from_raw_parts(view.data, view.len) }
        };
        let mut start = 0;
        let mut end = bytes.len();
        while start < end && matches!(bytes[start], b' ' | b'\t' | b'\r' | b'\n') {
            start += 1;
        }
        while end > start && matches!(bytes[end - 1], b' ' | b'\t' | b'\r' | b'\n') {
            end -= 1;
        }
        if start == end {
            unsafe { call(step, context, 7, 0) };
            return;
        }
        let quoted = end - start >= 2
            && ((bytes[start] == b'"' && bytes[end - 1] == b'"')
                || (bytes[start] == b'\'' && bytes[end - 1] == b'\''));
        if unsafe { call(step, context, 4, start) }.is_none() {
            return;
        }
        if unsafe { call(step, context, 5, end - start) }.is_none() {
            return;
        }
        if quoted {
            unsafe { call(step, context, 6, 0) };
        }
        return;
    }
}

/// Safety: left and top point to distinct writable i32 fields of a native
/// copied rectangle for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_badge_bounds(
    left: *mut i32,
    top: *mut i32,
    right: i32,
    bottom: i32,
) -> u8 {
    if left.is_null() || top.is_null() {
        return 0;
    }
    let old_left = unsafe { *left };
    let old_top = unsafe { *top };
    if right <= old_left || bottom <= old_top {
        return 0;
    }
    unsafe {
        *left = old_left.max(right.wrapping_sub(42));
        *top = old_top.max(bottom.wrapping_sub(18));
    }
    1
}

/// Safety: native callback holds the registry lock and owns the copied
/// fixed-capacity squad snapshot. Rust selects its ordered live members.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_live_squad(
    step: Option<Step>,
    context: *mut c_void,
    capacity: u32,
) {
    let Some(step) = step else {
        return;
    };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 2, 0) } != unsafe { step(context, 3, 0) } {
        return;
    }
    unsafe { step(context, 4, 0) };
    let size = unsafe { step(context, 5, 0) };
    let mut count = 0usize;
    for index in 0..size {
        if unsafe { step(context, 6, index) } != 0 && count < capacity as usize {
            unsafe { step(context, 7, count) };
            count += 1;
        }
    }
    unsafe { step(context, 8, count) };
}

/// Safety: native callback owns TLS bar bindings and checked original provider
/// calls; its accumulation region may delegate to the stage10 Rust controller.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_bar(
    step: Option<Step>,
    context: *mut c_void,
    output: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return output as usize;
    };
    let active = unsafe { step(context, 0, 0) };
    let mut found = false;
    for index in 0..active {
        if unsafe { step(context, 1, index) } == 1 {
            found = true;
            break;
        }
    }
    if !found {
        return output as usize;
    }
    unsafe { step(context, 2, 0) };
    if unsafe { step(context, 3, 0) } == 1 {
        unsafe { step(context, 4, 0) }
    } else {
        unsafe { step(context, 5, 0) }
    }
}

/// Safety: native callback owns the checked panel slot, original renderer
/// and page protection. Rust controls the install and restore order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_install_panel(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    unsafe { step(context, 0, 0) };
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } != 1
        || unsafe { step(context, 3, 0) } != 1
        || unsafe { step(context, 4, 0) } != 1
    {
        return 0;
    }
    for op in 5..=7 {
        unsafe { step(context, op, 0) };
    }
    1
}

/// Safety: native callback checks the original call site and owns its TLS
/// logical handle buffer. The physical native getter was called first.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_ui_projection(
    step: Option<Step>,
    context: *mut c_void,
    physical: usize,
    handles: u8,
) -> usize {
    let Some(step) = step else {
        return physical;
    };
    if unsafe { step(context, 0, 0) } == 0 || unsafe { step(context, 1, 0) } == 0 {
        return physical;
    }
    let logical = unsafe { step(context, 2, 0) };
    if logical == 0 {
        return physical;
    }
    if handles != 0 {
        unsafe { step(context, 3, 0) }
    } else {
        logical
    }
}

/// Safety: native callback performs checked executable-image signature reads
/// at the original fixed addresses; Rust owns the short-circuit probe order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_preflight(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for index in 0..6 {
        if unsafe { step(context, 0, index) } != 1 {
            return 0;
        }
    }
    for op in 1..=8 {
        if unsafe { step(context, op, 0) } != 1 {
            return 0;
        }
    }
    1
}

/// Safety: native callback owns class-map storage, class memory, copied ODF
/// fields and the unchanged from_chars numeric primitive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_capture_economics(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    for op in 0..=1 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    for index in 0..6 {
        if unsafe { call(step, context, 4, index) }.is_none() {
            return;
        }
    }
    for index in 0..4 {
        let Some(valid) = (unsafe { call(step, context, 5, index) }) else {
            return;
        };
        if valid == 1 && unsafe { call(step, context, 6, index) }.is_none() {
            return;
        }
    }
    unsafe { call(step, context, 7, 0) };
}

/// Safety: native callback relooks up each class and economics entry after
/// the resolver callback, which may dispatch reentrant class-load events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_resolve_economics_member(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..=3 {
        match unsafe { call(step, context, op, 0) } {
            Some(1) => {}
            _ => return 0,
        }
    }
    1
}

/// Safety: native callback owns the reentrancy guard, copied definition,
/// class/map storage and output commit. Existing Rust gameplay arithmetic
/// remains the numeric kernel; this function sequences its runtime use.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_resolve_economics(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    match unsafe { call(step, context, 0, 0) } {
        Some(1) => return 1,
        Some(_) => {}
        None => return 0,
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return 0;
    }
    if unsafe { call(step, context, 4, 0) }.is_none() {
        return 0;
    }
    let Some(error_empty) = (unsafe { call(step, context, 5, 0) }) else {
        return 0;
    };
    if error_empty == 0 {
        unsafe { call(step, context, 7, 0) };
        return 0;
    }
    let Some(writable) = (unsafe { call(step, context, 6, 0) }) else {
        return 0;
    };
    if writable == 0 {
        unsafe { call(step, context, 7, 0) };
        return 0;
    }
    for op in 8..=11 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    1
}

/// Safety: native callback performs checked executable-image signature
/// reads in the original pod layout probe order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_pod_layout(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..4 {
        if unsafe { step(context, op, 0) } != 1 {
            return 0;
        }
    }
    1
}

/// Safety: native callback owns checked craft reads and class-map storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_pod_root(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..5 {
        if unsafe { step(context, op, 0) } != 1 {
            return 0;
        }
    }
    1
}

/// Safety: native callback owns the copied rectangle, live squad snapshot,
/// text object and native draw/CRT formatting operation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_squadrons_badge(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    for op in 0..5 {
        if unsafe { step(context, op, 0) } != 1 {
            return;
        }
    }
    unsafe { step(context, 5, 0) };
}
