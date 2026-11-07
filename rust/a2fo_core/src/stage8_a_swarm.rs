//! Portable SwarmSystem module lifecycle and native binding sequence.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

#[repr(C)]
pub struct SwarmScalars {
    count: u32,
    scale: f32,
    minimum_radius: f32,
    maximum_radius: f32,
    minimum_speed: f32,
    maximum_speed: f32,
    turn_rate: f32,
    interaction_chance: f32,
    interaction_time: f32,
    interaction_radius: f32,
    host_clearance: f32,
    member_separation: f32,
    hardpoint_capacity: u32,
    interaction_capacity: u32,
}

fn cpp_min<T: PartialOrd + Copy>(a: T, b: T) -> T {
    if b < a { b } else { a }
}

fn cpp_max<T: PartialOrd + Copy>(a: T, b: T) -> T {
    if a < b { b } else { a }
}

/// Safety: `scalars` points to a writable C-compatible copy for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_normalize(scalars: *mut SwarmScalars) {
    let Some(scalars) = (unsafe { scalars.as_mut() }) else {
        return;
    };
    scalars.scale = cpp_max(0.001, cpp_min(scalars.scale, 100.0));
    scalars.minimum_radius = cpp_max(0.0, cpp_min(scalars.minimum_radius, 100000.0));
    scalars.maximum_radius = cpp_max(
        scalars.minimum_radius,
        cpp_min(scalars.maximum_radius, 100000.0),
    );
    scalars.minimum_speed = cpp_max(0.0, cpp_min(scalars.minimum_speed, 100000.0));
    scalars.maximum_speed = cpp_max(
        scalars.minimum_speed,
        cpp_min(scalars.maximum_speed, 100000.0),
    );
    scalars.turn_rate = cpp_max(0.01, cpp_min(scalars.turn_rate, 1000.0));
    scalars.interaction_chance = cpp_max(0.0, cpp_min(scalars.interaction_chance, 1.0));
    scalars.interaction_time = cpp_max(0.0, cpp_min(scalars.interaction_time, 3600.0));
    scalars.interaction_radius = cpp_max(0.0, cpp_min(scalars.interaction_radius, 100000.0));
    scalars.host_clearance = cpp_max(0.0, cpp_min(scalars.host_clearance, 100000.0));
    scalars.member_separation = cpp_max(0.0, cpp_min(scalars.member_separation, 100000.0));
    scalars.count = cpp_min(scalars.count, 256);
}

unsafe fn call(step: Step, context: *mut c_void, op: u32, arg: usize) -> Option<usize> {
    let value = unsafe { step(context, op, arg) };
    (value != usize::MAX).then_some(value)
}

/// Safety: the native step callback owns API and PE image reads, checked hook
/// patching and storage. Its lifetime covers this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_init(step: Option<Step>, context: *mut c_void) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return 0;
    }

    let mut supported = true;
    for index in 0..27 {
        match unsafe { call(step, context, 3, index) } {
            Some(value) => supported &= value != 0,
            None => return 0,
        }
    }
    if !supported && unsafe { call(step, context, 4, 0) }.is_none() {
        return 0;
    }

    let mut ready = supported;
    if ready {
        ready = match unsafe { call(step, context, 5, 0) } {
            Some(value) => value != 0,
            None => return 0,
        };
        if !ready && unsafe { call(step, context, 6, 0) }.is_none() {
            return 0;
        }
    }
    if ready {
        ready = match unsafe { call(step, context, 7, 0) } {
            Some(value) => value != 0,
            None => return 0,
        };
        if !ready && unsafe { call(step, context, 8, 0) }.is_none() {
            return 0;
        }
    }
    if ready {
        ready = match unsafe { call(step, context, 9, 0) } {
            Some(value) => value != 0,
            None => return 0,
        };
    }
    if unsafe { call(step, context, 10, usize::from(ready)) }.is_none() {
        return 0;
    }
    1
}

/// Safety: native callback retains storage and hook ownership.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_shutdown(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    for op in 0..=6 {
        if unsafe { call(step, context, 11 + op, 0) }.is_none() {
            return;
        }
    }
}

/// Safety: the native context owns ParameterDB, SDK class and all vector and
/// string storage for the duration of this synchronous parse. Callbacks trap
/// native exceptions and report `usize::MAX` before the C++ caller rethrows.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_parse(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return;
    }
    let mut total_members = 0usize;
    for index in 0..64 {
        match unsafe { call(step, context, 2, index) } {
            Some(0) => continue,
            Some(_) => {}
            None => return,
        }
        for field in 3..=4 {
            if unsafe { call(step, context, field, 0) }.is_none() {
                return;
            }
        }
        let Some(has_radius) = (unsafe { call(step, context, 5, 0) }) else {
            return;
        };
        if unsafe { call(step, context, 6, 0) }.is_none() {
            return;
        }
        let Some(has_max_radius) = (unsafe { call(step, context, 7, 0) }) else {
            return;
        };
        if has_radius == 0 && has_max_radius == 0 && unsafe { call(step, context, 36, 0) }.is_none()
        {
            return;
        }
        for field in 8..=19 {
            if unsafe { call(step, context, field, 0) }.is_none() {
                return;
            }
        }
        if unsafe { call(step, context, 20, 0) }.is_none() {
            return;
        }
        let Some(mut count) = (unsafe { call(step, context, 21, 0) }) else {
            return;
        };
        if count == 0 {
            continue;
        }
        if total_members >= 1024 {
            break;
        }
        count = count.min(1024 - total_members);
        if unsafe { call(step, context, 22, count) }.is_none() {
            return;
        }
        for kind in 0..2 {
            if unsafe { call(step, context, 23 + kind, 0) }.is_none() {
                return;
            }
        }
        for kind in 0..2 {
            let Some(name_count) = (unsafe { call(step, context, 25, kind) }) else {
                return;
            };
            if name_count == 0 {
                continue;
            }
            match unsafe { call(step, context, 26, 0) } {
                Some(0) => {
                    if unsafe { call(step, context, 27, kind) }.is_none() {
                        return;
                    }
                    continue;
                }
                Some(_) => {}
                None => return,
            }
            if unsafe { call(step, context, 28, 0) }.is_none() {
                return;
            }
            for name_index in 0..name_count {
                let packed = (kind << 16) | name_index;
                match unsafe { call(step, context, 29, packed) } {
                    Some(0) => {
                        if unsafe { call(step, context, 30, packed) }.is_none() {
                            return;
                        }
                    }
                    Some(_) => {
                        if unsafe { call(step, context, 31, packed) }.is_none() {
                            return;
                        }
                    }
                    None => return,
                }
            }
        }
        if unsafe { call(step, context, 32, 0) }.is_none() {
            return;
        }
        total_members += count;
        if unsafe { call(step, context, 33, 0) }.is_none() {
            return;
        }
    }
    match unsafe { call(step, context, 34, 0) } {
        Some(0) | None => {}
        Some(_) => {
            unsafe { call(step, context, 35, total_members) };
        }
    }
}

/// Safety: the native callback reads the live built-in activation token and
/// owns map destruction and atomic publication at the simulation boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_activation(
    step: Option<Step>,
    context: *mut c_void,
    synchronize: u8,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let mut current = 0u64;
    if unsafe { call(step, context, 0, (&mut current as *mut u64) as usize) }.is_none() {
        return 0;
    }
    if synchronize == 0 && current == 0 {
        return 0;
    }
    let mut stored = 0u64;
    if unsafe { call(step, context, 1, (&mut stored as *mut u64) as usize) }.is_none() {
        return 0;
    }
    if synchronize != 0 && current != stored {
        for op in 2..=8 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                return 0;
            }
        }
        if unsafe { call(step, context, 9, (&current as *const u64) as usize) }.is_none() {
            return 0;
        }
    }
    u8::from(current != 0 && (synchronize != 0 || current == stored))
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_swarm_seed(handle: u32, definition: u32, member: u32) -> u32 {
    let mut value = handle ^ 0x9e37_79b9;
    value ^= definition
        .wrapping_add(0x85eb_ca6b)
        .wrapping_add(value.wrapping_shl(6))
        .wrapping_add(value >> 2);
    value ^= member
        .wrapping_add(0xc2b2_ae35)
        .wrapping_add(value.wrapping_shl(6))
        .wrapping_add(value >> 2);
    value ^= value >> 16;
    value = value.wrapping_mul(0x7feb_352d);
    value ^= value >> 15;
    if value == 0 { 1 } else { value }
}

#[repr(C)]
pub struct SwarmEnumeration {
    head: usize,
    node: usize,
    next: usize,
    count: u32,
}

#[repr(C)]
pub struct SwarmObserved {
    handle: u32,
    object_class: usize,
}

#[repr(C)]
pub struct SwarmSimulation {
    epoch: u32,
    object_count: u32,
    last_object_count: u32,
    countdown: u32,
    enumerated_count: u32,
    last_seen_epoch: u32,
    group_count: u32,
    group_definition: u32,
    definition_count: u32,
    agent_count: u32,
}

#[repr(C)]
pub struct SwarmCreation {
    definition_count: u32,
    member_count: u32,
    launch_count: usize,
    random: u32,
    failure_report: i32,
    handle: u32,
    definition_index: u32,
}

/// Safety: callbacks keep the native object, class policy, and host-map
/// cursor live for this synchronous observation. Any native allocation error
/// is reported through the context and handled at the C++ noexcept boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_observe(
    step: Option<Step>,
    context: *mut c_void,
    state: *mut SwarmObserved,
) {
    let Some(step) = step else {
        return;
    };
    if state.is_null() {
        return;
    }
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 2, 0) }.is_none() {
        return;
    }
    let handle = unsafe { (*state).handle };
    let class = unsafe { (*state).object_class };
    if handle == 0 || class == 0 {
        return;
    }
    if !matches!(unsafe { call(step, context, 3, 0) }, Some(1)) {
        return;
    }
    let Some(present) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    if present != 0 {
        match unsafe { call(step, context, 5, 0) } {
            Some(1) => {
                unsafe { call(step, context, 6, 0) };
                return;
            }
            Some(_) => {}
            None => return,
        }
    }
    if unsafe { call(step, context, 7, 0) }.is_none() {
        return;
    }
    if !matches!(unsafe { call(step, context, 8, 0) }, Some(0)) {
        return;
    }
    unsafe { call(step, context, 9, 0) };
}

/// Safety: `state` is writable C POD; callbacks own the live native linked
/// list, sample the next link before observing each host, and never retain a
/// Rust reference to native storage across callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_enumerate(
    step: Option<Step>,
    context: *mut c_void,
    state: *mut SwarmEnumeration,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if state.is_null() {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return 0;
    }
    let count = unsafe { (*state).count };
    for _ in 0..count {
        let node = unsafe { (*state).node };
        if node == 0 || node == unsafe { (*state).head } {
            break;
        }
        if unsafe { call(step, context, 2, 0) }.is_none() {
            return 0;
        }
        if unsafe { call(step, context, 3, 0) }.is_none() {
            return 0;
        }
        let next = unsafe { (*state).next };
        if next == node {
            break;
        }
        unsafe {
            (*state).node = next;
        }
    }
    1
}

unsafe fn simulate_inner(
    step: Step,
    context: *mut c_void,
    state: *mut SwarmSimulation,
    elapsed: f32,
) -> bool {
    if unsafe { call(step, context, 2, 0) }.is_none() {
        return false;
    }
    let mut epoch = unsafe { (*state).epoch }.wrapping_add(1);
    if epoch == 0 {
        epoch = 1;
    }
    if unsafe { call(step, context, 3, epoch as usize) }.is_none() {
        return false;
    }
    if epoch == 1
        && unsafe { (*state).epoch } == u32::MAX
        && unsafe { call(step, context, 4, 0) }.is_none()
    {
        return false;
    }

    let Some(object_count_available) = (unsafe { call(step, context, 5, 0) }) else {
        return false;
    };
    if unsafe { call(step, context, 6, 0) }.is_none() {
        return false;
    }
    let object_count_changed = object_count_available != 0
        && unsafe { (*state).object_count != (*state).last_object_count };
    let changed_scan_due = object_count_changed && unsafe { (*state).countdown < 256 - 16 };
    let mut enumerated = false;
    if unsafe { (*state).countdown == 0 } || changed_scan_due {
        enumerated = match unsafe { call(step, context, 7, 0) } {
            Some(1) => true,
            Some(_) => false,
            None => return false,
        };
        if enumerated && unsafe { call(step, context, 8, 0) }.is_none() {
            return false;
        }
        if unsafe { call(step, context, 9, 255) }.is_none() {
            return false;
        }
    } else if unsafe { call(step, context, 10, 0) }.is_none() {
        return false;
    }

    let elapsed = cpp_max(0.0f32, cpp_min(elapsed, 0.25));
    if unsafe { call(step, context, 33, elapsed.to_bits() as usize) }.is_none() {
        return false;
    }
    if unsafe { call(step, context, 11, 0) }.is_none() {
        return false;
    }
    loop {
        match unsafe { call(step, context, 12, 0) } {
            Some(0) => break,
            Some(_) => {}
            None => return false,
        }
        if unsafe { call(step, context, 13, 0) }.is_none() {
            return false;
        }
        let stale = enumerated && unsafe { (*state).last_seen_epoch != epoch };
        if !stale {
            let Some(has_object) = (unsafe { call(step, context, 14, 0) }) else {
                return false;
            };
            if has_object != 0 {
                let Some(expired) = (unsafe { call(step, context, 15, 0) }) else {
                    return false;
                };
                if expired == 0 {
                    let Some(has_policy) = (unsafe { call(step, context, 16, 0) }) else {
                        return false;
                    };
                    if has_policy != 0 {
                        if unsafe { call(step, context, 17, 0) }.is_none() {
                            return false;
                        }
                        let group_count = unsafe { (*state).group_count };
                        for group_index in 0..group_count {
                            if unsafe { call(step, context, 18, group_index as usize) }.is_none() {
                                return false;
                            }
                            if unsafe { call(step, context, 19, 0) }.is_none() {
                                return false;
                            }
                            if unsafe { (*state).group_definition >= (*state).definition_count } {
                                continue;
                            }
                            if unsafe { call(step, context, 20, 0) }.is_none() {
                                return false;
                            }
                            let agent_count = unsafe { (*state).agent_count };
                            for agent_index in 0..agent_count {
                                if unsafe { call(step, context, 21, agent_index as usize) }
                                    .is_none()
                                {
                                    return false;
                                }
                            }
                            if unsafe { call(step, context, 22, 0) }.is_none() {
                                return false;
                            }
                        }
                    }
                }
            }
        }
        if unsafe { call(step, context, 23, 0) }.is_none() {
            return false;
        }
    }

    if unsafe { call(step, context, 24, 0) }.is_none() {
        return false;
    }
    loop {
        match unsafe { call(step, context, 25, 0) } {
            Some(0) => break,
            Some(_) => {}
            None => return false,
        }
        if unsafe { call(step, context, 26, 0) }.is_none() {
            return false;
        }
        let stale = enumerated && unsafe { (*state).last_seen_epoch != epoch };
        let erase = if stale {
            true
        } else {
            let Some(has_object) = (unsafe { call(step, context, 27, 0) }) else {
                return false;
            };
            if has_object == 0 {
                true
            } else {
                let Some(expired) = (unsafe { call(step, context, 28, 0) }) else {
                    return false;
                };
                expired != 0
            }
        };
        if unsafe { call(step, context, if erase { 29 } else { 30 }, 0) }.is_none() {
            return false;
        }
    }
    true
}

/// Safety: native callback owns host/map iterators, policy and agent storage;
/// Rust only orchestrates their synchronous traversal and copies POD fields.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_simulate(
    step: Option<Step>,
    context: *mut c_void,
    state: *mut SwarmSimulation,
    elapsed: f32,
) {
    let Some(step) = step else {
        return;
    };
    if state.is_null() {
        return;
    }
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(0)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let success = unsafe { simulate_inner(step, context, state, elapsed) };
    if !success {
        unsafe { step(context, 32, 0) };
    }
    unsafe { step(context, 31, 0) };
}

fn claims_hardpoint(phase: usize, interaction: bool) -> bool {
    if interaction {
        phase == 1 || phase == 2
    } else {
        phase == 3 || phase == 4
    }
}

/// Safety: callbacks own the agent RNG, group vector and selected output.
/// Each other member is sampled in original vector order after the random
/// first candidate is selected.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_choose_hardpoint(
    step: Option<Step>,
    context: *mut c_void,
    hardpoint_count: usize,
    capacity: u32,
    interaction: u8,
    has_group: u8,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if hardpoint_count == 0 {
        return 0;
    }
    let mut random = 0u32;
    if unsafe { call(step, context, 0, (&mut random as *mut u32) as usize) }.is_none() {
        return 0;
    }
    let first = (random as usize) % hardpoint_count;
    for offset in 0..hardpoint_count {
        let candidate = (first + offset) % hardpoint_count;
        if capacity == 0 || has_group == 0 {
            return u8::from(unsafe { call(step, context, 5, candidate) }.is_some());
        }
        let Some(member_count) = (unsafe { call(step, context, 1, 0) }) else {
            return 0;
        };
        let mut claims = 0u32;
        for member in 0..member_count {
            match unsafe { call(step, context, 2, member) } {
                Some(0) => continue,
                Some(_) => {}
                None => return 0,
            }
            let Some(phase) = (unsafe { call(step, context, 3, member) }) else {
                return 0;
            };
            if !claims_hardpoint(phase, interaction != 0) {
                continue;
            }
            let Some(target) = (unsafe { call(step, context, 4, member) }) else {
                return 0;
            };
            if target != candidate {
                continue;
            }
            claims += 1;
            if claims >= capacity {
                break;
            }
        }
        if claims < capacity {
            return u8::from(unsafe { call(step, context, 5, candidate) }.is_some());
        }
    }
    0
}

/// Safety: native callbacks own the agent RNG and sidecar state; the random
/// comparison and spatial math remain original GNU/x87 native kernels.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_choose_target(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let Some(has_interaction) = (unsafe { call(step, context, 0, 0) }) else {
        return;
    };
    if has_interaction != 0 {
        let Some(chance) = (unsafe { call(step, context, 1, 0) }) else {
            return;
        };
        if chance != 0 {
            let Some(available) = (unsafe { call(step, context, 2, 0) }) else {
                return;
            };
            if available != 0 {
                for op in 3..=5 {
                    if unsafe { call(step, context, op, 0) }.is_none() {
                        return;
                    }
                }
                return;
            }
        }
    }
    let Some(return_to) = (unsafe { call(step, context, 6, 0) }) else {
        return;
    };
    if return_to != 0 {
        let Some(chance) = (unsafe { call(step, context, 7, 0) }) else {
            return;
        };
        if chance != 0 {
            if unsafe { call(step, context, 8, 0) }.is_none() {
                return;
            }
            let Some(available) = (unsafe { call(step, context, 9, 0) }) else {
                return;
            };
            if available != 0 {
                for op in 10..=11 {
                    if unsafe { call(step, context, op, 0) }.is_none() {
                        return;
                    }
                }
                return;
            }
            if unsafe { call(step, context, 12, 0) }.is_none() {
                return;
            }
        }
    }
    unsafe { call(step, context, 13, 0) };
}

/// Safety: native callback owns the agent RNG and spatial numeric kernels.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_choose_roaming(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    for op in 0..=3 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
}

/// Safety: native callbacks own the agent, group and native movement math.
/// Rust controls phase transitions and read order through copied scalars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_update(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let Some(refreshed) = (unsafe { call(step, context, 2, 0) }) else {
        return;
    };
    if refreshed == 0 && unsafe { call(step, context, 3, 0) }.is_none() {
        return;
    }
    let Some(phase) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    if phase == 2 || phase == 4 {
        let Some(finished) = (unsafe { call(step, context, 5, 0) }) else {
            return;
        };
        if finished != 0 {
            unsafe { call(step, context, 3, 0) };
        }
        return;
    }
    if unsafe { call(step, context, 6, usize::from(phase == 1)) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 7, 0) }.is_none() {
        return;
    }
    let Some(has_exclusion) = (unsafe { call(step, context, 8, 0) }) else {
        return;
    };
    if has_exclusion != 0 {
        let Some(contact) = (unsafe { call(step, context, 9, 0) }) else {
            return;
        };
        if contact != 0 && unsafe { call(step, context, 10, 0) }.is_none() {
            return;
        }
    }
    if unsafe { call(step, context, 11, usize::from(has_exclusion != 0)) }.is_none() {
        return;
    }
    let Some(current_phase) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    if current_phase == 0 {
        let Some(constrained) = (unsafe { call(step, context, 12, 0) }) else {
            return;
        };
        if constrained != 0
            && unsafe { call(step, context, 13, usize::from(has_exclusion != 0)) }.is_none()
        {
            return;
        }
    }
    if !matches!(unsafe { call(step, context, 14, 0) }, Some(1)) {
        return;
    }
    let Some(phase) = (unsafe { call(step, context, 4, 0) }) else {
        return;
    };
    if phase == 1 {
        let Some(has_time) = (unsafe { call(step, context, 15, 0) }) else {
            return;
        };
        if has_time == 0 {
            unsafe { call(step, context, 3, 0) };
        } else {
            unsafe { call(step, context, 16, 0) };
        }
    } else if phase == 3 {
        unsafe { call(step, context, 17, 0) };
    } else {
        unsafe { call(step, context, 18, 0) };
    }
}

/// Safety: native callback owns hardpoint vectors and transform SDK calls;
/// Rust chooses the live phase path before any hardpoint access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_refresh_target(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let Some(phase) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    if phase == 1 || phase == 2 {
        if unsafe { call(step, context, 2, 0) }.is_none() {
            return 0;
        }
    } else if phase == 3 || phase == 4 {
        if unsafe { call(step, context, 3, 0) }.is_none() {
            return 0;
        }
    } else {
        return 1;
    }
    if !matches!(unsafe { call(step, context, 4, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 5, 0) }.is_none() {
        return 0;
    }
    let Some(exclusion) = (unsafe { call(step, context, 6, 0) }) else {
        return 0;
    };
    if exclusion != 0 && unsafe { call(step, context, 7, 0) }.is_none() {
        return 0;
    }
    1
}

/// Safety: native callback owns the group and exact GNU/x87 spatial kernels.
/// Rust controls the bounded pair/pass and post-correction traversal.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_separate(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    let Some(count) = (unsafe { call(step, context, 0, 0) }) else {
        return;
    };
    if count < 2 {
        return;
    }
    let passes = if count <= 16 {
        3
    } else if count <= 64 {
        2
    } else {
        1
    };
    for _ in 0..passes {
        for left in 0..count - 1 {
            match unsafe { call(step, context, 1, left) } {
                Some(0) => continue,
                Some(_) => {}
                None => return,
            }
            for right in left + 1..count {
                match unsafe { call(step, context, 2, right) } {
                    Some(0) => continue,
                    Some(_) => {}
                    None => return,
                }
                let Some(apart) = (unsafe { call(step, context, 3, 0) }) else {
                    return;
                };
                if apart == 0 && unsafe { call(step, context, 4, 0) }.is_none() {
                    return;
                }
            }
        }
    }
    for member in 0..count {
        match unsafe { call(step, context, 5, member) } {
            Some(0) => continue,
            Some(_) => {}
            None => return,
        }
        let Some(exclusion) = (unsafe { call(step, context, 6, 0) }) else {
            return;
        };
        if exclusion != 0 && unsafe { call(step, context, 7, 0) }.is_none() {
            return;
        }
        let Some(phase) = (unsafe { call(step, context, 8, 0) }) else {
            return;
        };
        if phase != 0 {
            continue;
        }
        let Some(constrained) = (unsafe { call(step, context, 9, usize::from(exclusion != 0)) })
        else {
            return;
        };
        if constrained != 0
            && unsafe { call(step, context, 10, usize::from(exclusion != 0)) }.is_none()
        {
            return;
        }
    }
}

/// Safety: native callbacks own the host, definition/group vectors, agent
/// allocations and ST3D calls. Rust directs bounded creation and logging.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_create_host(
    step: Option<Step>,
    context: *mut c_void,
    state: *mut SwarmCreation,
) {
    let Some(step) = step else {
        return;
    };
    if state.is_null() {
        return;
    }
    for op in 0..=6 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
    let definition_count = unsafe { (*state).definition_count };
    for definition in 0..definition_count {
        if unsafe { call(step, context, 7, definition as usize) }.is_none() {
            return;
        }
        if unsafe { call(step, context, 8, 0) }.is_none() {
            return;
        }
        let member_count = unsafe { (*state).member_count };
        for member in 0..member_count {
            let seed = a2fo_rs_stage8_swarm_seed(
                unsafe { (*state).handle },
                unsafe { (*state).definition_index },
                member,
            );
            if unsafe { call(step, context, 9, seed as usize) }.is_none() {
                return;
            }
            let Some(initialized) = (unsafe { call(step, context, 10, 0) }) else {
                return;
            };
            if initialized == 0 {
                if unsafe { call(step, context, 11, 0) }.is_none() {
                    return;
                }
                let report = unsafe { (*state).failure_report };
                if report <= 8 {
                    if unsafe { call(step, context, 12, 0) }.is_none() {
                        return;
                    }
                } else if report == 9 && unsafe { call(step, context, 13, 0) }.is_none() {
                    return;
                }
                if unsafe { call(step, context, 32, 0) }.is_none() {
                    return;
                }
                break;
            }
            if unsafe { call(step, context, 14, 0) }.is_none() {
                return;
            }
            let Some(avoid) = (unsafe { call(step, context, 15, 0) }) else {
                return;
            };
            if avoid != 0 {
                let Some(logged) = (unsafe { call(step, context, 16, 0) }) else {
                    return;
                };
                let mut logged_active = false;
                if logged == 0 {
                    let Some(positive) = (unsafe { call(step, context, 17, 0) }) else {
                        return;
                    };
                    if positive != 0 {
                        if unsafe { call(step, context, 18, 0) }.is_none() {
                            return;
                        }
                        logged_active = true;
                    }
                }
                if !logged_active {
                    let Some(missing_logged) = (unsafe { call(step, context, 19, 0) }) else {
                        return;
                    };
                    if missing_logged == 0 {
                        let Some(nonpositive) = (unsafe { call(step, context, 31, 0) }) else {
                            return;
                        };
                        if nonpositive != 0 && unsafe { call(step, context, 20, 0) }.is_none() {
                            return;
                        }
                    }
                }
            }
            if unsafe { call(step, context, 21, 0) }.is_none() {
                return;
            }
            if unsafe { call(step, context, 22, 0) }.is_none() {
                return;
            }
            let launch_count = unsafe { (*state).launch_count };
            let launch_index =
                (member as usize).wrapping_add(unsafe { (*state).random } as usize) % launch_count;
            if unsafe { call(step, context, 23, launch_index) }.is_none() {
                return;
            }
            if unsafe { call(step, context, 24, 0) }.is_none() {
                return;
            }
            let Some(exclusion) = (unsafe { call(step, context, 25, 0) }) else {
                return;
            };
            if exclusion != 0 && unsafe { call(step, context, 26, 0) }.is_none() {
                return;
            }
            for op in 27..=28 {
                if unsafe { call(step, context, op, 0) }.is_none() {
                    return;
                }
            }
        }
        let Some(nonempty) = (unsafe { call(step, context, 29, 0) }) else {
            return;
        };
        if nonempty != 0 && unsafe { call(step, context, 30, 0) }.is_none() {
            return;
        }
    }
}

/// Safety: native callback owns host/group iteration, ST3D instances,
/// camera, and exact GNU/x87 transform composition and logging kernels.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_render(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    loop {
        match unsafe { call(step, context, 2, 0) } {
            Some(0) => break,
            Some(_) => {}
            None => return,
        }
        let Some(object) = (unsafe { call(step, context, 3, 0) }) else {
            return;
        };
        if object == 0 {
            if unsafe { call(step, context, 8, 0) }.is_none() {
                return;
            }
            continue;
        }
        let Some(expired) = (unsafe { call(step, context, 4, 0) }) else {
            return;
        };
        if expired != 0 {
            if unsafe { call(step, context, 8, 0) }.is_none() {
                return;
            }
            continue;
        }
        let Some(visible) = (unsafe { call(step, context, 5, 0) }) else {
            return;
        };
        if visible == 0 {
            let Some(logged) = (unsafe { call(step, context, 6, 0) }) else {
                return;
            };
            if logged == 0 && unsafe { call(step, context, 7, 0) }.is_none() {
                return;
            }
            if unsafe { call(step, context, 8, 0) }.is_none() {
                return;
            }
            continue;
        }
        let Some(transformed) = (unsafe { call(step, context, 9, 0) }) else {
            return;
        };
        if transformed == 0 {
            if unsafe { call(step, context, 8, 0) }.is_none() {
                return;
            }
            continue;
        }
        let Some(groups) = (unsafe { call(step, context, 11, 0) }) else {
            return;
        };
        for group in 0..groups {
            if unsafe { call(step, context, 12, group) }.is_none() {
                return;
            }
            let Some(agents) = (unsafe { call(step, context, 13, 0) }) else {
                return;
            };
            for agent in 0..agents {
                match unsafe { call(step, context, 14, agent) } {
                    Some(0) => continue,
                    Some(_) => {}
                    None => return,
                }
                let Some(rendered) = (unsafe { call(step, context, 15, 0) }) else {
                    return;
                };
                if rendered != 0 {
                    let Some(logged) = (unsafe { call(step, context, 16, 0) }) else {
                        return;
                    };
                    if logged == 0 && unsafe { call(step, context, 17, 0) }.is_none() {
                        return;
                    }
                } else {
                    let Some(logged) = (unsafe { call(step, context, 18, 0) }) else {
                        return;
                    };
                    if logged == 0 && unsafe { call(step, context, 19, 0) }.is_none() {
                        return;
                    }
                }
            }
        }
        if unsafe { call(step, context, 8, 0) }.is_none() {
            return;
        }
    }
}

/// Safety: the native callback owns both original checked gateways, live
/// activation state and ABI arguments at this synchronous noexcept hook.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_hook(step: Option<Step>, context: *mut c_void) {
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
    let Some(logged) = (unsafe { call(step, context, 3, 0) }) else {
        return;
    };
    if logged == 0 && unsafe { call(step, context, 4, 0) }.is_none() {
        return;
    }
    unsafe { call(step, context, 5, 0) };
}

/// Safety: native callback owns ST3D instance bytes, checked engine calls,
/// model string and exact GNU/x87 scale comparison.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_instance_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..=1 {
        match unsafe { call(step, context, op, 0) } {
            Some(0) => {}
            Some(_) | None => return 0,
        }
    }
    for op in 2..=4 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    let Some(logical) = (unsafe { call(step, context, 5, 0) }) else {
        return 0;
    };
    let Some(visible) = (unsafe { call(step, context, 6, 0) }) else {
        return 0;
    };
    if logical == 0 && visible == 0 {
        unsafe { call(step, context, 7, 0) };
        return 0;
    }
    let Some(scale) = (unsafe { call(step, context, 8, 0) }) else {
        return 0;
    };
    if scale != 0 && unsafe { call(step, context, 9, 0) }.is_none() {
        return 0;
    }
    for op in 10..=11 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    1
}

/// Safety: `data` is readable for `len` bytes, outputs are writable, and the
/// callback performs the original C locale byte classification without
/// changing the borrowed native string during this synchronous scan.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_swarm_trim(
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
        let byte = unsafe { *data.add(first) };
        if unsafe { step(context, 0, byte as usize) } == 0 {
            break;
        }
        first += 1;
    }
    let mut last = len;
    while last > first {
        let byte = unsafe { *data.add(last - 1) };
        if unsafe { step(context, 0, byte as usize) } == 0 {
            break;
        }
        last -= 1;
    }
    unsafe {
        *first_out = first;
        *last_out = last;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_swarm_count_default(value: u32) -> i32 {
    value.min(i32::MAX as u32) as i32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_swarm_count_value(value: i32) -> u32 {
    value.min(256) as u32
}
