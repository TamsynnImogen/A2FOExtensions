//! Portable Turrets module registration and native hook transaction.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

#[repr(C)]
pub struct TurretAim {
    yaw_min: f32,
    yaw_max: f32,
    pitch_min: f32,
    pitch_max: f32,
    yaw_rate: f32,
    pitch_rate: f32,
    rest_yaw: f32,
    rest_pitch: f32,
}

unsafe extern "C" {
    fn a2fo_rs_turret_clamp(value: f32, minimum: f32, maximum: f32) -> f32;
}

/// Safety: `aim` is a writable C POD copy. The clamp helper is the existing
/// migrated turret numeric API, so no duplicate floating algorithm is added.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_normalize(aim: *mut TurretAim) {
    let Some(aim) = (unsafe { aim.as_mut() }) else {
        return;
    };
    aim.yaw_min = unsafe { a2fo_rs_turret_clamp(aim.yaw_min, -180.0, 180.0) };
    aim.yaw_max = unsafe { a2fo_rs_turret_clamp(aim.yaw_max, -180.0, 180.0) };
    if aim.yaw_min > aim.yaw_max {
        core::mem::swap(&mut aim.yaw_min, &mut aim.yaw_max);
    }
    aim.pitch_min = unsafe { a2fo_rs_turret_clamp(aim.pitch_min, -90.0, 90.0) };
    aim.pitch_max = unsafe { a2fo_rs_turret_clamp(aim.pitch_max, -90.0, 90.0) };
    if aim.pitch_min > aim.pitch_max {
        core::mem::swap(&mut aim.pitch_min, &mut aim.pitch_max);
    }
    aim.yaw_rate = unsafe { a2fo_rs_turret_clamp(aim.yaw_rate, 0.0, 3600.0) };
    aim.pitch_rate = unsafe { a2fo_rs_turret_clamp(aim.pitch_rate, 0.0, 3600.0) };
    aim.rest_yaw = unsafe { a2fo_rs_turret_clamp(aim.rest_yaw, aim.yaw_min, aim.yaw_max) };
    aim.rest_pitch = unsafe { a2fo_rs_turret_clamp(aim.rest_pitch, aim.pitch_min, aim.pitch_max) };
}

/// Safety: bytes and outputs remain valid for this synchronous scan; the
/// callback performs original C locale byte classification only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_trim(
    data: *const u8,
    len: usize,
    step: Option<Step>,
    context: *mut c_void,
    first_out: *mut usize,
    last_out: *mut usize,
) {
    let Some(step) = step else {
        return;
    };
    if first_out.is_null() || last_out.is_null() || (data.is_null() && len != 0) {
        return;
    }
    let mut first = 0;
    while first < len {
        if unsafe { step(context, 0, *data.add(first) as usize) } == 0 {
            break;
        }
        first += 1;
    }
    let mut last = len;
    while last > first {
        if unsafe { step(context, 0, *data.add(last - 1) as usize) } == 0 {
            break;
        }
        last -= 1;
    }
    unsafe {
        *first_out = first;
        *last_out = last;
    }
}

/// Safety: `data` remains readable for `len` bytes and the callback performs
/// original locale lowercasing without mutating the borrowed string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_odf_suffix(
    data: *const u8,
    len: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if data.is_null() || len < 4 {
        return 0;
    }
    let offset = len - 4;
    if unsafe { *data.add(offset) } != b'.' {
        return 0;
    }
    for (index, expected) in [b'o', b'd', b'f'].into_iter().enumerate() {
        let byte = unsafe { *data.add(offset + index + 1) };
        if unsafe { step(context, 1, byte as usize) } != expected as usize {
            return 0;
        }
    }
    1
}

/// Safety: native callback owns ParameterDB, class names, map/vector storage
/// and original exception containment for this synchronous class workflow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_register_class(
    step: Option<Step>,
    context: *mut c_void,
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
    let Some(semantic) = (unsafe { call(step, context, 2, 0) }) else {
        return;
    };
    if semantic != 0 {
        if unsafe { call(step, context, 3, 0) }.is_none() {
            return;
        }
        for field in 0..9 {
            if unsafe { call(step, context, 4, field) }.is_none() {
                return;
            }
        }
        if unsafe { call(step, context, 5, 0) }.is_none() {
            return;
        }
        if !matches!(unsafe { call(step, context, 6, 0) }, Some(1)) {
            return;
        }
        if unsafe { call(step, context, 7, 0) }.is_none() {
            return;
        }
    }
    if unsafe { call(step, context, 8, 0) }.is_none() {
        return;
    }
    for index in 0..=64 {
        if unsafe { call(step, context, 9, index) }.is_none() {
            return;
        }
        let Some(has_turret) = (unsafe { call(step, context, 10, 0) }) else {
            return;
        };
        let Some(has_hardpoint) = (unsafe { call(step, context, 11, 0) }) else {
            return;
        };
        if has_turret == 0 && has_hardpoint == 0 {
            if unsafe { call(step, context, 19, 0) }.is_none() {
                return;
            }
            continue;
        }
        if has_turret == 0 || has_hardpoint == 0 {
            if unsafe { call(step, context, 12, 0) }.is_none() {
                return;
            }
            if unsafe { call(step, context, 19, 0) }.is_none() {
                return;
            }
            continue;
        }
        for op in 13..=14 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                return;
            }
        }
        if !matches!(unsafe { call(step, context, 15, 0) }, Some(1)) {
            if unsafe { call(step, context, 19, 0) }.is_none() {
                return;
            }
            continue;
        }
        if !matches!(unsafe { call(step, context, 16, 0) }, Some(1)) {
            return;
        }
        if unsafe { call(step, context, 19, 0) }.is_none() {
            return;
        }
    }
    let Some(count) = (unsafe { call(step, context, 17, 0) }) else {
        return;
    };
    if count == 0 {
        return;
    }
    unsafe { call(step, context, 18, 0) };
}

/// Safety: native callback owns event admission, weapon/object reads, target
/// registry and checked gateway. Rust directs final precheck policy.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_weapon_trigger(
    step: Option<Step>,
    context: *mut c_void,
    handles: *mut TurretTargetHandles,
    committed: u32,
    precheck: u32,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if handles.is_null() {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let kind = unsafe { step(context, 1, 0) } as u32;
    if kind == committed {
        return 1;
    }
    if kind != precheck {
        return 0;
    }
    for op in 2..=4 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    let Some(ordered) = (unsafe { call(step, context, 5, 0) }) else {
        return 0;
    };
    if ordered != 0 {
        if unsafe { call(step, context, 6, 0) }.is_none() {
            return 0;
        }
        if unsafe { call(step, context, 7, 0) }.is_none() {
            return 0;
        }
        if unsafe { (*handles).target != (*handles).ordered } {
            unsafe { call(step, context, 9, 0) };
            return 0;
        }
    }
    if unsafe { call(step, context, 10, 0) }.is_none() {
        return 0;
    }
    1
}

#[repr(C)]
pub struct TurretTargetHandles {
    target: u32,
    ordered: u32,
}

/// Safety: native callback owns checked gateway and target state; the
/// original gateway is called even while module policy is disabled.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_set_target(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return;
    }
    let Some(ordered) = (unsafe { call(step, context, 1, 0) }) else {
        return;
    };
    if unsafe { call(step, context, 2, usize::from(ordered != 0)) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    unsafe { call(step, context, 4, 0) };
}

/// Safety: native callback owns class/target maps and checked object reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_track_target(
    step: Option<Step>,
    context: *mut c_void,
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
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    let Some(target) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    unsafe { call(step, context, if target != 0 { 5 } else { 6 }, 0) };
}

/// Safety: native callback retains child map and returns an owned scalar
/// pointer in its context after live entity/expiry checks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_inherited_target(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    let Some(found) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    if found == 0 {
        return;
    }
    if !matches!(unsafe { call(step, context, 5, 0) }, Some(0)) {
        return;
    }
    unsafe { call(step, context, 6, 0) };
}

/// Safety: native callback owns checked module and weapon pointers.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_weapon_owner(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    unsafe { call(step, context, 1, 0) };
}

/// Safety: native callback owns the parent/class maps, vector storage and
/// original allocation failure handling. Rust controls lookup and creation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_ensure_parent(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    let handle = unsafe { step(context, 1, 0) };
    if handle == 0 {
        return;
    }
    if !matches!(unsafe { call(step, context, 2, handle) }, Some(0)) {
        return;
    }
    if unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    if !matches!(unsafe { call(step, context, 4, 0) }, Some(1)) {
        return;
    }
    let Some(count) = (unsafe { call(step, context, 5, 0) }) else {
        return;
    };
    if !matches!(unsafe { call(step, context, 6, count) }, Some(1)) {
        unsafe { call(step, context, 9, 0) };
        return;
    }
    for index in 0..count {
        if !matches!(unsafe { call(step, context, 7, index) }, Some(1)) {
            unsafe { call(step, context, 9, 0) };
            return;
        }
    }
    if !matches!(unsafe { call(step, context, 8, handle) }, Some(1)) {
        unsafe { call(step, context, 9, 0) };
    }
}

/// Safety: native callback owns the mount vector and returns its result in
/// native context storage. Rust controls index traversal and match decision.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_find_mount(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let Some(count) = (unsafe { call(step, context, 0, 0) }) else {
        return;
    };
    for index in 0..count {
        if matches!(unsafe { call(step, context, 1, index) }, Some(1)) {
            unsafe { call(step, context, 2, index) };
            return;
        }
    }
}

/// Safety: native callback owns raw class hierarchy lookup, node storage and
/// original failure logging. Rust controls one-time resolution sequencing.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_resolve_hardpoint(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        return u8::from(unsafe { step(context, 2, 0) } != 0);
    }
    unsafe { step(context, 3, 0) };
    let root = unsafe { step(context, 4, 0) };
    if root != 0 {
        unsafe { step(context, 5, root) };
    }
    if unsafe { step(context, 2, 0) } == 0 {
        unsafe { step(context, 6, 0) };
        return 0;
    }
    1
}

/// Safety: native callback owns SDK event admission, live reads and map
/// storage. Each lookup runs only when reached by the original event branch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_craft_event(
    step: Option<Step>,
    context: *mut c_void,
    cleanup: u32,
    post_load: u32,
    simulate_pre: u32,
    simulate_post: u32,
) {
    let Some(step) = step else {
        return;
    };
    for op in 0..=3 {
        if unsafe { step(context, op, 0) } == 0 {
            return;
        }
    }
    let kind = unsafe { step(context, 4, 0) } as u32;
    if kind == cleanup {
        let handle = unsafe { step(context, 5, 0) };
        unsafe { step(context, 6, handle) };
        unsafe { step(context, 7, handle) };
        return;
    }
    if kind == post_load {
        if unsafe { step(context, 8, 0) } != 0 {
            unsafe { step(context, 9, 0) };
        }
        return;
    }
    if unsafe { step(context, 10, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 11, 0) } == 0 {
        return;
    }
    let handle = unsafe { step(context, 12, 0) };
    if kind == simulate_pre {
        if unsafe { step(context, 13, handle) } != 0 {
            unsafe { step(context, 14, handle) };
        }
        return;
    }
    if kind != simulate_post {
        return;
    }
    if unsafe { step(context, 13, handle) } != 0 {
        unsafe { step(context, 15, handle) };
    }
    if unsafe { step(context, 16, 0) } != 0 {
        unsafe { step(context, 17, 0) };
    }
}

/// Safety: native callback owns the parent map and a caller-owned child
/// snapshot. Rust controls collection, erase and expiry ordering.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_cleanup_parent(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    let count = unsafe { step(context, 1, 0) };
    for index in 0..count {
        let child = unsafe { step(context, 2, index) };
        if child != 0 && unsafe { step(context, 3, child) } == 0 {
            break;
        }
    }
    unsafe { step(context, 4, 0) };
    let count = unsafe { step(context, 5, 0) };
    for index in 0..count {
        let child = unsafe { step(context, 6, index) };
        unsafe { step(context, 7, child) };
        unsafe { step(context, 8, child) };
    }
}

/// Safety: native callback owns child, target and parent maps; no native
/// reentry occurs until all links have been removed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_unlink_child(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        unsafe { step(context, 5, 0) };
        return;
    }
    if unsafe { step(context, 1, 0) } != 0
        && unsafe { step(context, 2, 0) } != 0
        && unsafe { step(context, 3, 0) } != 0
    {
        unsafe { step(context, 4, 0) };
    }
    unsafe { step(context, 5, 0) };
    unsafe { step(context, 6, 0) };
}

/// Safety: native callback owns CRT prefix/sscanf operations, temporary
/// unsigned-long storage and output pointers. Rust decides admission.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_parse_label(
    step: Option<Step>,
    context: *mut c_void,
    maximum_turrets: u32,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 2, 0) } != 2 {
        return 0;
    }
    let parent = unsafe { step(context, 3, 0) };
    if parent == 0 || parent > u32::MAX as usize {
        return 0;
    }
    let index = unsafe { step(context, 4, 0) };
    if index > maximum_turrets as usize {
        return 0;
    }
    unsafe { step(context, 5, 0) };
    1
}

/// Safety: native callback retains the original checked constructor call,
/// allocation exception capture and shield observer calling convention.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_class_construct(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    unsafe { step(context, 0, 0) };
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    unsafe { step(context, 2, 0) };
}

/// Safety: native callback owns the original vector begin/end cursor and
/// current mount reference. Callbacks preserve that storage during this pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_spawn_pending(
    step: Option<Step>,
    context: *mut c_void,
    active: u32,
    pending: u32,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
    while unsafe { step(context, 3, 0) } != 0 {
        let status = unsafe { step(context, 4, 0) } as u32;
        if status == active {
            if unsafe { step(context, 6, 0) } == 0 {
                unsafe { step(context, 7, 0) };
            }
        } else if status == pending && unsafe { step(context, 9, 0) } != 0 {
            unsafe { step(context, 10, 0) };
            if unsafe { step(context, 11, 0) } == 0 {
                unsafe { step(context, 12, 0) };
            } else {
                let child_handle = unsafe { step(context, 13, 0) };
                let configured = unsafe { step(context, 14, 0) } != 0;
                if child_handle == 0 || !configured {
                    unsafe { step(context, 15, 0) };
                    unsafe { step(context, 16, 0) };
                } else {
                    unsafe { step(context, 17, 0) };
                    if unsafe { step(context, 18, 0) } == 0 {
                        unsafe { step(context, 19, 0) };
                        unsafe { step(context, 16, 0) };
                    } else {
                        unsafe { step(context, 20, 0) };
                        unsafe { step(context, 21, 0) };
                    }
                }
            }
        }
        unsafe { step(context, 8, 0) };
    }
}

/// Safety: native callback owns checked object reads, native maps and the
/// loaded-child state. Rust controls both class lookups and restore order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_attach_loaded(
    step: Option<Step>,
    context: *mut c_void,
    active_status: u32,
) {
    let Some(step) = step else {
        return;
    };
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
    unsafe { step(context, 5, 0) };
    let parent = unsafe { step(context, 6, 0) };
    if unsafe { step(context, 7, 0) } == 0 {
        unsafe { step(context, 8, 0) };
        unsafe { step(context, 9, 0) };
        return;
    }
    if parent == 0 {
        return;
    }
    unsafe { step(context, 10, 0) };
    if unsafe { step(context, 11, 0) } == 0 {
        unsafe { step(context, 12, 0) };
        unsafe { step(context, 13, 0) };
        unsafe { step(context, 9, 0) };
        return;
    }
    if unsafe { step(context, 14, 0) } as u32 == active_status {
        let mounted = unsafe { step(context, 15, 0) };
        let loaded = unsafe { step(context, 4, 0) };
        if mounted != loaded {
            unsafe { step(context, 16, 0) };
            unsafe { step(context, 13, 0) };
            unsafe { step(context, 9, 0) };
            return;
        }
    }
    unsafe { step(context, 17, 0) };
}

/// Safety: native callback owns team/race reads and exact checked native
/// swap calls. Rust decides which swap occurs after ordered reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_sync_ownership(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    let parent_team = unsafe { step(context, 1, 0) } as u32;
    let child_team = unsafe { step(context, 2, 0) } as u32;
    let parent_race = unsafe { step(context, 3, 0) };
    let child_race = unsafe { step(context, 4, 0) };
    if parent_race != 0 && parent_race != child_race {
        unsafe { step(context, 5, 0) };
    } else if parent_team != child_team {
        unsafe { step(context, 6, 0) };
    }
}

/// Safety: native callback retains exact Craft getter/setter addresses and
/// signed value conversion. Rust sequences two live policy comparisons.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_sync_combat_policy(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    let parent_alert = unsafe { step(context, 1, 0) };
    let child_alert = unsafe { step(context, 2, 0) };
    if parent_alert != child_alert {
        unsafe { step(context, 3, 0) };
    }
    let parent_autonomy = unsafe { step(context, 4, 0) };
    let child_autonomy = unsafe { step(context, 5, 0) };
    if parent_autonomy != child_autonomy {
        unsafe { step(context, 6, 0) };
    }
}

/// Safety: native callback owns live command/entity lookups and native
/// AttackOrder storage; target policy uses the existing migrated helper.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_current_attack_order(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    let target = unsafe { step(context, 5, 0) };
    let exists = target != 0 && unsafe { step(context, 6, 0) } != 0;
    let selected = unsafe { step(context, 7, usize::from(exists)) };
    if selected == 0 {
        return;
    }
    unsafe { step(context, 8, 0) };
    unsafe { step(context, 9, 0) };
    unsafe { step(context, 10, 0) };
}

/// Safety: native callback owns command pointer and ordered scalar reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_child_has_attack_order(
    step: Option<Step>,
    context: *mut c_void,
    attack: u32,
    object_form: u32,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } as u32 != attack {
        return 0;
    }
    if unsafe { step(context, 2, 0) } as u32 != object_form {
        return 0;
    }
    for op in 3..=5 {
        if unsafe { step(context, op, 0) } == 0 {
            return 0;
        }
    }
    1
}

/// Safety: native callback owns command storage and checked command gateway.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_mirror_attack_order(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let handle = unsafe { step(context, 0, 0) };
    let target = unsafe { step(context, 1, 0) };
    if handle != 0 && target != 0 {
        if unsafe { step(context, 2, 0) } != 0 {
            return;
        }
        unsafe { step(context, 3, 0) };
        return;
    }
    if unsafe { step(context, 4, 0) } == 0 {
        return;
    }
    unsafe { step(context, 5, 0) };
}

/// Safety: native callback owns current map values and the previously
/// migrated order-target transition. Reads occur again after command dispatch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_update_inherited_order(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    unsafe { step(context, 5, 0) };
    if unsafe { step(context, 6, 0) } != 0 {
        return;
    }
    if unsafe { step(context, 7, 0) } == 0 {
        unsafe { step(context, 8, 0) };
    }
}

/// Safety: native callback retains child state and checked parent lookup;
/// callback mutations preserve that native state for the pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_prepare_linked(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        return;
    }
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    unsafe { step(context, 5, 0) };
}

/// Safety: native callback retains child/mount/class state and invokes the
/// previously migrated numeric aim and transform kernels at original points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_update_linked(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 || unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 20, 0) };
        return;
    }
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    if unsafe { step(context, 5, 0) } != 0 {
        unsafe { step(context, 19, 0) };
        unsafe { step(context, 20, 0) };
        return;
    }
    unsafe { step(context, 6, 0) };
    if unsafe { step(context, 7, 0) } == 0 {
        unsafe { step(context, 20, 0) };
        return;
    }
    unsafe { step(context, 8, 0) };
    if unsafe { step(context, 9, 0) } == 0 {
        return;
    }
    unsafe { step(context, 10, 0) };
    let target = unsafe { step(context, 11, 0) };
    if target != 0 && unsafe { step(context, 12, 0) } == 0 {
        if unsafe { step(context, 13, 0) } != 0 {
            unsafe { step(context, 14, 0) };
        }
    } else if unsafe { step(context, 15, 0) } != 0 {
        unsafe { step(context, 16, 0) };
    }
    unsafe { step(context, 17, 0) };
    unsafe { step(context, 18, 0) };
}

/// Safety: native callback owns mount hierarchy, checked transform call and
/// output storage; Rust enforces original short-circuit admission.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_mount_world_transform(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..=3 {
        if unsafe { step(context, op, 0) } == 0 {
            return 0;
        }
    }
    let result = unsafe { step(context, 4, 0) };
    let output = unsafe { step(context, 5, 0) };
    u8::from(result == output)
}

/// Safety: native callback owns mission getter/vtable and checked add call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_add_to_mission(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    if unsafe { step(context, 5, 0) } != 0 {
        unsafe { step(context, 6, 0) };
    }
}

/// Safety: native callback owns live expiry check and exact native address.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_expire_craft(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        return;
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return;
    }
    unsafe { step(context, 3, 0) };
}

/// Safety: native callback owns CRT label formatting, class constructor and
/// mission ABI. Rust directs admission and post-construction sequencing.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_construct_turret(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    unsafe { step(context, 2, 0) };
    if unsafe { step(context, 3, 0) } != 0 {
        unsafe { step(context, 4, 0) };
    }
}

/// Safety: native callback owns ParameterDB string storage and CRT tolower.
/// Rust consumes only scalar bytes and writes the output on recognized values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_read_parameter_bool(
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
        return 0;
    }
    let count = unsafe { step(context, 2, 0) };
    let mut one = count == 1;
    let mut yes = count == 3;
    let mut true_word = count == 4;
    let mut zero = count == 1;
    let mut no = count == 2;
    let mut false_word = count == 5;
    for index in 0..count {
        let byte = unsafe { step(context, 3, index) } as u8;
        one &= byte == b'1';
        yes &= b"yes".get(index).copied() == Some(byte);
        true_word &= b"true".get(index).copied() == Some(byte);
        zero &= byte == b'0';
        no &= b"no".get(index).copied() == Some(byte);
        false_word &= b"false".get(index).copied() == Some(byte);
    }
    if one || yes || true_word {
        unsafe { step(context, 4, 1) };
        return 1;
    }
    if zero || no || false_word {
        unsafe { step(context, 4, 0) };
        return 1;
    }
    0
}

/// Safety: native callback owns the SDK classlabel output and applies CRT
/// tolower to the complete 64-byte buffer before Rust compares its value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_is_semantic_turret(
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
        return 0;
    }
    for index in 0..64 {
        unsafe { step(context, 2, index) };
    }
    for (index, expected) in b"turret\0".iter().enumerate() {
        if unsafe { step(context, 3, index) } as u8 != *expected {
            return 0;
        }
    }
    1
}

/// Safety: native callback owns the CRT strtof, errno, finite and isspace
/// kernels, string cursor and output. Rust owns suffix/trailing admission.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_parse_float(
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
        return 0;
    }
    unsafe { step(context, 2, 0) };
    for op in 3..=5 {
        if unsafe { step(context, op, 0) } == 0 {
            return 0;
        }
    }
    let suffix = unsafe { step(context, 6, 0) } as u8;
    if suffix == b'f' || suffix == b'F' {
        unsafe { step(context, 9, 0) };
    }
    loop {
        let byte = unsafe { step(context, 7, 0) } as u8;
        if byte == 0 {
            break;
        }
        if unsafe { step(context, 8, usize::from(byte)) } == 0 {
            return 0;
        }
        unsafe { step(context, 9, 0) };
    }
    unsafe { step(context, 10, 0) };
    1
}

/// Safety: native callback owns Win32 loader calls and function pointer
/// storage; Rust chooses fallback provider and post-link logging order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_resolve_shield_observers(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let primary = unsafe { step(context, 0, 0) };
    let mut export = if primary != 0 {
        unsafe { step(context, 1, 0) }
    } else {
        0
    };
    if export == 0 {
        let fallback = unsafe { step(context, 2, 0) };
        export = if fallback != 0 {
            unsafe { step(context, 3, 0) }
        } else {
            0
        };
    }
    unsafe { step(context, 4, export) };
    if unsafe { step(context, 5, 0) } == 0 {
        unsafe { step(context, 6, 0) };
        return;
    }
    unsafe { step(context, 7, 0) };
}

/// Safety: native callback owns ParameterDB call, fixed buffer and C++
/// string exception boundaries. Rust orders admission and output handling.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_read_parameter_string(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..=4 {
        if unsafe { step(context, op, 0) } == 0 {
            return 0;
        }
    }
    let found = unsafe { step(context, 5, 0) };
    unsafe { step(context, 6, 0) };
    if found & 0xff == 0 || unsafe { step(context, 7, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 8, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 9, 0) } == 0 {
        return 0;
    }
    u8::from(unsafe { step(context, 10, 0) } != 0)
}

/// Safety: native callback owns temporary string and native CRT parse
/// storage. Rust preserves read-then-parse short-circuit order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_read_parameter_float(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    u8::from(unsafe { step(context, 1, 0) } != 0)
}

/// Safety: native callback owns C++ string mutation and CRT case kernel;
/// Rust sequences trim, suffix recognition and resize.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_normalize_odf(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let Some(suffix) = (unsafe { call(step, context, 2, 0) }) else {
        return;
    };
    if suffix != 0 {
        unsafe { call(step, context, 3, 0) };
    }
}

unsafe fn call(step: Step, context: *mut c_void, op: u32, arg: usize) -> Option<usize> {
    let value = unsafe { step(context, op, arg) };
    (value != usize::MAX).then_some(value)
}

/// Safety: the native callback owns API/PE pointers, checked patches and
/// original exception capture for this synchronous startup transaction.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_turrets_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    for op in 1..=3 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    if !matches!(unsafe { call(step, context, 4, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 5, 0) }.is_none() {
        return 0;
    }

    let Some(alias) = (unsafe { call(step, context, 6, 0) }) else {
        return 0;
    };
    let Some(defaults) = (unsafe { call(step, context, 7, 0) }) else {
        return 0;
    };
    let Some(masked) = (unsafe { call(step, context, 8, 0) }) else {
        return 0;
    };
    let Some(trigger) = (unsafe { call(step, context, if masked != 0 { 9 } else { 10 }, 0) })
    else {
        return 0;
    };
    let Some(craft) = (unsafe { call(step, context, 11, 0) }) else {
        return 0;
    };
    if alias == 0 || defaults == 0 || trigger == 0 || craft == 0 {
        unsafe { call(step, context, 12, 0) };
        return 0;
    }

    let mut supported = true;
    for index in 0..9 {
        match unsafe { call(step, context, 13, index) } {
            Some(value) => supported &= value != 0,
            None => return 0,
        }
    }
    let mut ready = supported;
    if !supported {
        if unsafe { call(step, context, 14, 0) }.is_none() {
            return 0;
        }
    } else {
        let Some(stock) = (unsafe { call(step, context, 15, 0) }) else {
            return 0;
        };
        let class_installed = if stock != 0 {
            let Some(installed) = (unsafe { call(step, context, 16, 0) }) else {
                return 0;
            };
            if installed == 0 {
                false
            } else {
                match unsafe { call(step, context, 17, 0) } {
                    Some(value) => value != 0,
                    None => return 0,
                }
            }
        } else {
            let Some(detour) = (unsafe { call(step, context, 18, 0) }) else {
                return 0;
            };
            if detour == 0 {
                false
            } else {
                let Some(patched) = (unsafe { call(step, context, 19, 0) }) else {
                    return 0;
                };
                if patched == 0 {
                    false
                } else {
                    if unsafe { call(step, context, 20, 0) }.is_none() {
                        return 0;
                    }
                    true
                }
            }
        };
        let Some(weapon_installed) = (unsafe { call(step, context, 21, 0) }) else {
            return 0;
        };
        ready = class_installed && weapon_installed != 0;
        if !ready && unsafe { call(step, context, 22, 0) }.is_none() {
            return 0;
        }
    }
    if unsafe { call(step, context, 23, usize::from(ready)) }.is_none() {
        return 0;
    }
    1
}
