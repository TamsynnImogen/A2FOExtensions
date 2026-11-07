//! Private queue presentation and admission workflows. Game object memory,
//! locks and native hook calls remain in the C++ adapter.
use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct QueueGroup {
    pub target: usize,
    pub count: u32,
    pub physical_count: u32,
    pub pending_count: u32,
    pub first_queue_id: u32,
    pub last_queue_id: u32,
    pub first_pending_index: u32,
    pub last_pending_index: u32,
    pub infinite: bool,
}
impl QueueGroup {
    const fn empty() -> Self {
        Self {
            target: 0,
            count: 0,
            physical_count: 0,
            pending_count: 0,
            first_queue_id: 0,
            last_queue_id: 0,
            first_pending_index: u32::MAX,
            last_pending_index: u32::MAX,
            infinite: false,
        }
    }
}
#[repr(C)]
pub struct QueueView {
    pub groups: [QueueGroup; 10],
    pub count: u32,
}
impl QueueView {
    const fn empty() -> Self {
        Self {
            groups: [QueueGroup::empty(); 10],
            count: 0,
        }
    }
}
#[repr(C)]
pub struct QueueRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}
#[derive(Clone, Copy)]
struct Physical {
    target: usize,
    id: u32,
}
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

unsafe fn scan_physical(
    context: *mut c_void,
    step: Step,
    capacity: u32,
) -> ([Physical; 10], usize) {
    let active = unsafe { step(context, 0, 0) };
    let active_id = unsafe { step(context, 1, 0) } as u32;
    let mut item = unsafe { step(context, 2, 0) };
    let mut physical = [Physical { target: 0, id: 0 }; 10];
    let mut physical_count = 0usize;
    let mut active_in_fifo = false;
    while item != 0 && physical_count < capacity as usize {
        if unsafe { step(context, 3, 0) } == 0 {
            break;
        }
        let target = unsafe { step(context, 4, 0) };
        let id = unsafe { step(context, 5, 0) } as u32;
        if target != 0 {
            physical[physical_count] = Physical { target, id };
            physical_count += 1;
            if active != 0 && active_id != 0 && id == active_id {
                active_in_fifo = true;
            }
        }
        item = unsafe { step(context, 6, 0) };
    }
    if active != 0 && !active_in_fifo && (active_id != 0 || physical_count == 0) {
        let limit = core::cmp::min(physical_count, capacity as usize - 1);
        for index in (1..=limit).rev() {
            physical[index] = physical[index - 1];
        }
        physical[0] = Physical {
            target: active,
            id: active_id,
        };
        physical_count = core::cmp::min(physical_count + 1, capacity as usize);
    }
    (physical, physical_count)
}

fn append(
    view: &mut QueueView,
    target: usize,
    id: u32,
    pending: bool,
    pending_index: u32,
    slots: u32,
    units: u32,
) {
    if target == 0 {
        return;
    }
    let mut group = None;
    if view.count != 0 {
        let previous = &view.groups[(view.count - 1) as usize];
        if previous.target == target && previous.count < units && !previous.infinite {
            group = Some((view.count - 1) as usize);
        }
    }
    let index = if let Some(index) = group {
        index
    } else {
        if view.count >= slots {
            return;
        }
        let index = view.count as usize;
        view.count += 1;
        view.groups[index].target = target;
        index
    };
    let group = &mut view.groups[index];
    group.count += 1;
    if pending {
        group.pending_count += 1;
        if group.first_pending_index == u32::MAX {
            group.first_pending_index = pending_index;
        }
        group.last_pending_index = pending_index;
    } else {
        group.physical_count += 1;
        if id != 0 {
            if group.first_queue_id == 0 {
                group.first_queue_id = id;
            }
            group.last_queue_id = id;
        }
    }
}

/// # Safety
/// Native callback reads one mapped game field at a time and owns the C++
/// pending/continuous snapshots. The output is private C-compatible POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_collect(
    context: *mut c_void,
    step: Option<Step>,
    capacity: u32,
    slots: u32,
    units: u32,
    output: *mut QueueView,
) -> bool {
    let Some(step) = step else { return false };
    if output.is_null() || capacity > 10 || slots > 10 || capacity == 0 || slots == 0 || units == 0
    {
        return false;
    }
    let (physical, physical_count) = unsafe { scan_physical(context, step, capacity) };
    let mut view = QueueView::empty();
    for entry in physical.iter().take(physical_count) {
        append(
            &mut view,
            entry.target,
            entry.id,
            false,
            u32::MAX,
            slots,
            units,
        );
    }
    let pending = unsafe { step(context, 7, 0) } as u32;
    for index in 0..pending {
        let target = unsafe { step(context, 8, index as usize) };
        append(&mut view, target, 0, true, index, slots, units);
    }
    if unsafe { step(context, 9, 0) } != 0 {
        let repeat = unsafe { step(context, 10, 0) };
        if repeat != 0 {
            if view.count != 0 && view.groups[(view.count - 1) as usize].target == repeat {
                view.groups[(view.count - 1) as usize].infinite = true;
            } else if view.count < slots {
                let group = &mut view.groups[view.count as usize];
                view.count += 1;
                group.target = repeat;
                group.count = 1;
                group.infinite = true;
            }
        }
    }
    unsafe { output.write(view) };
    true
}

/// # Safety
/// Native callback owns linked-list reads and checks the incoming producer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_visual_slot(
    context: *mut c_void,
    step: Option<Step>,
    queue_id: u32,
    capacity: u32,
    output: *mut u32,
) -> bool {
    let Some(step) = step else { return false };
    if queue_id == 0 || capacity == 0 || capacity > 10 || output.is_null() {
        return false;
    }
    let (physical, count) = unsafe { scan_physical(context, step, capacity) };
    for (index, item) in physical.iter().take(count).enumerate() {
        if item.id == queue_id {
            unsafe { output.write(index as u32) };
            return true;
        }
    }
    false
}

/// # Safety
/// Callback reads a caller-owned GroupedQueueView without changing it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_units(context: *mut c_void, step: Option<Step>) -> u32 {
    let Some(step) = step else { return 0 };
    let count = unsafe { step(context, 0, 0) } as u32;
    let mut total = 0u32;
    for index in 0..count {
        total = total.wrapping_add(unsafe { step(context, 1, index as usize) } as u32);
    }
    total
}

/// # Safety
/// Callback reads the current caller-owned view in original branch order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_room(
    context: *mut c_void,
    target: usize,
    step: Option<Step>,
) -> bool {
    if target == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    if unsafe { a2fo_rs_c_queue_units(context, Some(step)) } >= 100 {
        return false;
    }
    let count = unsafe { step(context, 0, 0) } as u32;
    if count == 0 {
        return true;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        return false;
    }
    if unsafe { step(context, 3, 0) } == target && unsafe { step(context, 4, 0) } < 10 {
        return true;
    }
    count < 10
}

/// # Safety
/// Callback reads the current caller-owned view in original branch order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_continuous_room(
    context: *mut c_void,
    target: usize,
    step: Option<Step>,
) -> bool {
    if target == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    let count = unsafe { step(context, 0, 0) } as u32;
    if count != 0 && unsafe { step(context, 3, 0) } == target {
        return true;
    }
    unsafe { a2fo_rs_c_queue_room(context, target, Some(step)) }
}

/// # Safety
/// Native callback retains effective-class resolution, barriers and C++
/// queue admission; Rust sequences one visible-slot fill.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_fill(
    context: *mut c_void,
    producer: usize,
    target: usize,
    step: Option<Step>,
) -> u32 {
    if producer == 0 || target == 0 {
        return 0;
    }
    let Some(step) = step else { return 0 };
    let resolved = unsafe { step(context, 0, 0) };
    if resolved == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        return 0;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        return 0;
    }
    unsafe { step(context, 3, 0) };
    let count = unsafe { step(context, 4, 0) } as u32;
    let mut room = 10u32;
    if count != 0 {
        if unsafe { step(context, 5, 0) } != 0 {
            return 0;
        }
        if unsafe { step(context, 6, 0) } == resolved {
            let tail_count = unsafe { step(context, 7, 0) } as u32;
            if tail_count < 10 {
                room = 10 - tail_count;
            } else if count >= 10 {
                return 0;
            }
        } else if count >= 10 {
            return 0;
        }
    }
    let evolve = unsafe { step(context, 8, 0) } != 0;
    let attempt_limit = if evolve { 1 } else { room };
    let mut added = 0;
    for _ in 0..attempt_limit {
        if unsafe { step(context, 9, 0) } == 0 {
            break;
        }
        added += 1;
    }
    added
}

/// # Safety
/// Native callback owns target resolution, events and checked Fleet Ops push.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_push_once(
    context: *mut c_void,
    producer: usize,
    target: usize,
    dispatch: bool,
    resolved: bool,
    step: Option<Step>,
) -> bool {
    if producer == 0 || target == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    if dispatch && !resolved && unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 2, 0) } >= 10 {
        return false;
    }
    if unsafe { step(context, 3, 0) } != 0 {
        return false;
    }
    if dispatch && unsafe { step(context, 4, 0) } == 0 {
        return false;
    }
    let before = unsafe { step(context, 5, 0) };
    unsafe { step(context, 6, 0) };
    (unsafe { step(context, 7, 0) }) > before
}

/// # Safety
/// Native callback owns group snapshots, admission events, and sidecar map.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_logical_one(
    context: *mut c_void,
    producer: usize,
    target: usize,
    resolved: bool,
    step: Option<Step>,
) -> bool {
    if producer == 0 || target == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    if !resolved && unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        return false;
    }
    if unsafe { step(context, 3, 0) } != 0 {
        return false;
    }
    unsafe { step(context, 4, 0) };
    if unsafe { step(context, 5, 0) } == 0 {
        return false;
    }
    let pending = unsafe { step(context, 6, 0) };
    if pending == 0 && unsafe { step(context, 7, 0) } < 10 {
        return unsafe { step(context, 8, 0) } != 0;
    }
    if unsafe { step(context, 9, 0) } == 0 {
        return false;
    }
    (unsafe { step(context, 10, 0) }) != 0
}

/// # Safety
/// Native callback owns each live sidecar snapshot, conflict check and push.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_materialize(
    context: *mut c_void,
    producer: usize,
    step: Option<Step>,
) {
    if producer == 0 {
        return;
    }
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        return;
    }
    for _ in 0..10 {
        if unsafe { step(context, 2, 0) } >= 10 {
            break;
        }
        if unsafe { step(context, 3, 0) } == 0 {
            break;
        }
        if unsafe { step(context, 4, 0) } == 0 {
            break;
        }
        if unsafe { step(context, 5, 0) } != 0 {
            break;
        }
        if unsafe { step(context, 6, 0) } == 0 {
            break;
        }
        if unsafe { step(context, 7, 0) } == 0 {
            break;
        }
    }
}

/// # Safety
/// Native callback owns the live sidecar/continuous maps and Fleet Ops push.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_refill(
    context: *mut c_void,
    producer: usize,
    ready: bool,
    step: Option<Step>,
) {
    if !ready || producer == 0 {
        return;
    }
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        return;
    }
    if unsafe { step(context, 3, 0) } != 0 {
        unsafe { step(context, 4, 0) };
        return;
    }
    if unsafe { step(context, 5, 0) } != 0 {
        return;
    }
    let pending = unsafe { step(context, 6, 0) };
    if unsafe { step(context, 7, 0) } != 0 || pending != 0 {
        return;
    }
    if unsafe { step(context, 8, 0) } != 0 {
        unsafe { step(context, 9, 0) };
    }
}

/// # Safety
/// Native callback owns grouped snapshots and checked queue deletion.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_trim_group(
    context: *mut c_void,
    producer: usize,
    slot: u32,
    step: Option<Step>,
) {
    if producer == 0 {
        return;
    }
    let Some(step) = step else { return };
    for _ in 0..10 {
        let count = unsafe { step(context, 0, 0) } as u32;
        if slot >= count {
            return;
        }
        let units = unsafe { step(context, 1, slot as usize) } as u32;
        if units <= 1 {
            return;
        }
        let pending = unsafe { step(context, 2, 0) } as u32;
        if pending != 0 {
            let last = unsafe { step(context, 3, 0) } as u32;
            if last != u32::MAX {
                if unsafe { step(context, 4, last as usize) } == 0 {
                    return;
                }
                continue;
            }
        }
        let current = unsafe { step(context, 5, 0) } as u32;
        let last = unsafe { step(context, 6, 0) } as u32;
        if last != 0 && last != current {
            unsafe { step(context, 7, last as usize) };
            continue;
        }
        return;
    }
}

/// # Safety
/// Native callback owns icon context, queue IDs, locks and deletion calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_act_delete(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    let mut have_slot = false;
    if unsafe { step(context, 0, 0) } != 0 && unsafe { step(context, 1, 0) } != 0 {
        unsafe { step(context, 2, 0) };
        have_slot = true;
    } else if unsafe { step(context, 0, 0) } != 0 {
        have_slot = unsafe { step(context, 3, 0) } != 0;
        if have_slot {
            unsafe { step(context, 4, 0) };
        }
    }
    if have_slot {
        let count = unsafe { step(context, 5, 0) } as u32;
        let slot = unsafe { step(context, 6, 0) } as u32;
        if slot < count {
            if unsafe { step(context, 7, 0) } != 0 {
                unsafe { step(context, 8, 0) };
                return;
            }
            let pending = unsafe { step(context, 9, 0) } as u32;
            if pending != 0 && unsafe { step(context, 10, 0) } as u32 != u32::MAX {
                unsafe { step(context, 11, 0) };
                return;
            }
            let last = unsafe { step(context, 12, 0) } as u32;
            let current = unsafe { step(context, 13, 0) } as u32;
            if last != 0 && last != current {
                unsafe { step(context, 14, last as usize) };
            }
        }
    }
    unsafe { step(context, 15, 0) };
    unsafe { step(context, 16, 0) };
}

unsafe fn checked(step: Step, context: *mut c_void, op: u32, arg: usize) -> Option<usize> {
    let value = unsafe { step(context, op, arg) };
    (value != usize::MAX).then_some(value)
}

/// # Safety
/// Native callback owns linked-list reads, event dispatch, notifications and
/// catches C++ exceptions into a sentinel for rethrow after FFI returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_clear(
    context: *mut c_void,
    producer: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { checked(step, context, 0, 0) }.is_none() {
        return false;
    }
    if unsafe { checked(step, context, 1, 0) }.is_none() {
        return false;
    }
    let Some(charge) = (unsafe { checked(step, context, 2, 0) }) else {
        return false;
    };
    let Some(active_id) = (unsafe { checked(step, context, 3, 0) }) else {
        return false;
    };
    let Some(mut item) = (unsafe { checked(step, context, 4, 0) }) else {
        return false;
    };
    let mut active_notified = false;
    while item != 0 {
        let Some(target) = (unsafe { checked(step, context, 5, 0) }) else {
            return false;
        };
        let Some(queue_id) = (unsafe { checked(step, context, 6, 0) }) else {
            return false;
        };
        if target != 0 {
            if charge != 0 && unsafe { checked(step, context, 7, 0) }.is_none() {
                return false;
            }
            if unsafe { checked(step, context, 8, 0) }.is_none() {
                return false;
            }
            if queue_id == active_id {
                active_notified = true;
            }
        }
        let Some(next) = (unsafe { checked(step, context, 9, 0) }) else {
            return false;
        };
        item = next;
    }
    if charge == 0 {
        let Some(target) = (unsafe { checked(step, context, 10, 0) }) else {
            return false;
        };
        if target != 0 {
            if unsafe { checked(step, context, 11, 0) }.is_none() {
                return false;
            }
            if !active_notified {
                if unsafe { checked(step, context, 12, 0) }.is_none() {
                    return false;
                }
                active_notified = true;
            }
        }
    }
    if producer != 0
        && active_id != 0
        && !active_notified
        && unsafe { checked(step, context, 13, 0) }.is_none()
    {
        return false;
    }
    if unsafe { checked(step, context, 14, 0) }.is_none() {
        return false;
    }
    unsafe { checked(step, context, 15, 0) }.is_some()
}

/// # Safety
/// Native callback owns raw item/producer field reads and writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_reconstruct_ids(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    let mut item = unsafe { step(context, 0, 0) };
    let current = if item != 0 {
        (unsafe { step(context, 1, 0) }) as u32
    } else {
        0
    };
    let mut maximum = 0u32;
    for _ in 0..10 {
        if item == 0 {
            break;
        }
        maximum = core::cmp::max(maximum, unsafe { step(context, 2, 0) } as u32);
        item = unsafe { step(context, 3, 0) };
    }
    unsafe { step(context, 4, current as usize) };
    let next = if maximum == u32::MAX {
        1
    } else {
        core::cmp::max(1, maximum + 1)
    };
    unsafe { step(context, 5, next as usize) };
}

/// # Safety
/// Native callback owns save ABI and raw producer fields. `failed` points to
/// a C++ stack flag set when the callback captures an exception.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_save(
    context: *mut c_void,
    producer: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if failed.is_null() {
        return 0;
    }
    if !ready || producer == 0 {
        return unsafe { step(context, 0, 0) };
    }
    let active = unsafe { step(context, 1, 0) } != 0;
    if unsafe { *failed } != 0 {
        return 0;
    }
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) };
    if active {
        unsafe { step(context, 4, 0) };
    }
    let result = unsafe { step(context, 5, 0) };
    if unsafe { *failed } != 0 {
        return result;
    }
    unsafe { step(context, 6, 0) };
    unsafe { step(context, 7, 0) };
    result
}

/// # Safety
/// Native callback owns load ABI, project lookup, sidecar state and catches
/// exceptions. `failed` is a caller-owned C++ exception flag.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_load(
    context: *mut c_void,
    producer: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if failed.is_null() {
        return 0;
    }
    let result = unsafe { step(context, 0, 0) };
    if unsafe { *failed } != 0 {
        return result;
    }
    if producer != 0 {
        unsafe { step(context, 1, 0) };
        if unsafe { *failed } != 0 {
            return result;
        }
    }
    if !ready || producer == 0 || result == 0 {
        return result;
    }
    if unsafe { step(context, 2, 0) } != 0xa2f0_c0de {
        return result;
    }
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, 0) };
    if unsafe { *failed } != 0 {
        return result;
    }
    let target = unsafe { step(context, 5, 0) };
    if unsafe { *failed } != 0 {
        return result;
    }
    if target == 0 {
        unsafe { step(context, 6, 0) };
        return result;
    }
    unsafe { step(context, 7, 0) };
    result
}

/// # Safety
/// Native callback owns checked signatures and hook patch operations. All
/// signatures are evaluated before the first patch, matching bitwise &=.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_install_hooks(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let mut signatures_ok = true;
    for index in 0..12 {
        let result = unsafe { step(context, 0, index) };
        if result == usize::MAX {
            return false;
        }
        if result == 0 {
            signatures_ok = false;
        }
    }
    if !signatures_ok {
        unsafe { step(context, 1, 0) };
        return false;
    }
    for index in 0..10 {
        let result = unsafe { step(context, 2, index) };
        if result == usize::MAX {
            return false;
        }
        if result == 0 {
            unsafe { step(context, 3, 0) };
            return false;
        }
    }
    true
}

/// # Safety
/// Native callback owns critical section, map allocation, checked patches
/// and diagnostic calls. Rust sequences initialization failure paths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_initialize(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let Some(admitted) = (unsafe { checked(step, context, 0, 0) }) else {
        return false;
    };
    if admitted == 0 {
        return false;
    }
    if unsafe { checked(step, context, 1, 0) }.is_none() {
        return false;
    }
    let Some(allocated) = (unsafe { checked(step, context, 2, 0) }) else {
        return false;
    };
    if allocated == 0 {
        unsafe { step(context, 3, 0) };
        return false;
    }
    let Some(signature) = (unsafe { checked(step, context, 4, 0) }) else {
        return false;
    };
    let installed = if signature != 0 {
        let Some(result) = (unsafe { checked(step, context, 5, 0) }) else {
            return false;
        };
        result
    } else {
        0
    };
    if signature == 0 || installed == 0 {
        unsafe { step(context, 6, 0) };
        return false;
    }
    if unsafe { checked(step, context, 7, 0) }.is_none() {
        return false;
    }
    let Some(repeat) = (unsafe { checked(step, context, 8, 0) }) else {
        return false;
    };
    if repeat != 0 {
        if unsafe { checked(step, context, 9, 0) }.is_none() {
            return false;
        }
        let Some(ui) = (unsafe { checked(step, context, 10, 0) }) else {
            return false;
        };
        if ui != 0 {
            unsafe { step(context, 11, 0) };
        } else {
            unsafe { step(context, 12, 0) };
        }
    }
    true
}

/// # Safety
/// Native callback reads icon/parent fields with its original bounds checks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_icon_context(
    context: *mut c_void,
    icon: usize,
    ship_out: usize,
    producer_out: usize,
    slot_out: usize,
    step: Option<Step>,
) -> bool {
    if icon == 0 || ship_out == 0 || producer_out == 0 || slot_out == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 3, 0) } == 0 {
        return false;
    }
    let mut slot = 10usize;
    for index in 0..10 {
        if unsafe { step(context, 4, index) } == icon {
            slot = index;
            break;
        }
    }
    if slot >= 10 {
        return false;
    }
    if unsafe { step(context, 5, 0) } == 0 {
        return false;
    }
    (unsafe { step(context, 6, slot) }) != 0
}

/// # Safety
/// Native callback reads the three native component slots and font bounds.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_text_component(
    context: *mut c_void,
    display: usize,
    step: Option<Step>,
) -> usize {
    if display == 0 {
        return 0;
    }
    let Some(step) = step else { return 0 };
    for index in 0..3 {
        let component = unsafe { step(context, 0, index) };
        if component != 0 && unsafe { step(context, 1, 0) } != 0 {
            return component;
        }
    }
    0
}

/// # Safety
/// Native callback reads verified UI fields and owns the draw ABI; `rect` is
/// a private scalar POD copied from the native rectangle before adjustment.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_draw_count(
    context: *mut c_void,
    count: u32,
    infinite: bool,
    rect: *mut QueueRect,
    step: Option<Step>,
) -> bool {
    if (!infinite && count <= 1) || rect.is_null() {
        return false;
    }
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return false;
    }
    unsafe { step(context, 2, 0) };
    let rect = unsafe { &mut *rect };
    if rect.right <= rect.left || rect.bottom <= rect.top {
        return false;
    }
    let width = rect.right.wrapping_sub(rect.left);
    let height = rect.bottom.wrapping_sub(rect.top);
    rect.left = rect.left.wrapping_add(core::cmp::max(0, width / 2));
    rect.bottom = rect.top.wrapping_add(core::cmp::max(12, height / 2));
    if unsafe { step(context, 3, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 4, 0) } != 0 {
        unsafe { step(context, 5, 0) };
    }
    unsafe { step(context, 6, 0) };
    unsafe { step(context, 7, 0) };
    unsafe { step(context, 8, 0) };
    if unsafe { step(context, 9, 0) } == 0 {
        return false;
    }
    unsafe { step(context, 10, 0) };
    true
}

/// # Safety
/// Native callback owns fresh grouped snapshot reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_export_room(
    context: *mut c_void,
    producer: usize,
    ready: bool,
    step: Option<Step>,
) -> bool {
    if !ready || producer == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } >= 100 {
        return false;
    }
    let count = unsafe { step(context, 2, 0) } as u32;
    if count < 10 {
        return true;
    }
    if count == 0 {
        return true;
    }
    (unsafe { step(context, 3, 0) }) == 0 && unsafe { step(context, 4, 0) } < 10
}

/// # Safety
/// Native callback owns fresh grouped snapshot reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_export_size(
    context: *mut c_void,
    producer: usize,
    ready: bool,
    step: Option<Step>,
) -> u32 {
    if !ready || producer == 0 {
        return u32::MAX;
    }
    let Some(step) = step else { return u32::MAX };
    (unsafe { step(context, 0, 0) }) as u32
}

/// # Safety
/// Native callback owns target-specific group admission.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_export_can(
    context: *mut c_void,
    producer: usize,
    target: usize,
    ready: bool,
    step: Option<Step>,
) -> bool {
    if !ready || producer == 0 || target == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    unsafe { step(context, 0, 0) };
    (unsafe { step(context, 1, 0) }) != 0
}

/// # Safety
/// Native callback owns keyboard state reads and the synchronized native
/// marker call. Rust selects the marker at the original decision point.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_export_overflow(
    context: *mut c_void,
    producer: usize,
    target: usize,
    ready: bool,
    step: Option<Step>,
) -> bool {
    if !ready || producer == 0 || target == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } < 10 {
        return false;
    }
    unsafe { step(context, 1, 0) };
    let control = unsafe { step(context, 2, 0) } != 0;
    let continuous = control && unsafe { step(context, 3, 0) } != 0;
    let marker = if continuous {
        if unsafe { step(context, 4, 0) } == 0 {
            return false;
        }
        0xa2
    } else if control {
        if unsafe { step(context, 5, 0) } == 0 {
            return false;
        }
        0xa1
    } else {
        if unsafe { step(context, 5, 0) } == 0 {
            return false;
        }
        0xa5
    };
    if unsafe { step(context, 6, marker) } == usize::MAX {
        return false;
    }
    unsafe { step(context, 7, marker) };
    true
}

/// # Safety
/// Native callback owns queue-ID lookup, native deletion and sidecar transfer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_export_cancel(
    context: *mut c_void,
    producer: usize,
    queue_id: u32,
    ready: bool,
    step: Option<Step>,
) -> bool {
    if !ready || producer == 0 || queue_id == 0 {
        return false;
    }
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return false;
    }
    unsafe { step(context, 2, 0) };
    unsafe { step(context, 3, 0) };
    true
}

/// # Safety
/// Native callback owns icon fields and the original COM-like draw thunk.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_icon_render(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    if unsafe { step(context, 3, 0) } == 0 || unsafe { step(context, 4, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    unsafe { step(context, 5, 0) };
    if unsafe { step(context, 6, 0) } == usize::MAX {
        return;
    }
    if unsafe { step(context, 7, 0) } == usize::MAX {
        return;
    }
    if unsafe { step(context, 2, 0) } == usize::MAX {
        return;
    }
    if unsafe { step(context, 8, 0) } != 0 {
        let count = unsafe { step(context, 9, 0) } as u32;
        let infinite = unsafe { step(context, 10, 0) } != 0;
        if count > 1 || infinite {
            unsafe { step(context, 11, 0) };
        }
    }
    unsafe { step(context, 12, 0) };
}

/// # Safety
/// Native callback owns icon fields and temporary thread-local UI context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_icon_simulate(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    if unsafe { step(context, 3, 0) } == 0 || unsafe { step(context, 4, 0) } == 0 {
        unsafe { step(context, 2, 0) };
        return;
    }
    unsafe { step(context, 5, 0) };
    if unsafe { step(context, 6, 0) } == 0 {
        unsafe { step(context, 7, 0) };
    }
    unsafe { step(context, 8, 0) };
    unsafe { step(context, 9, 0) };
    if unsafe { step(context, 2, 0) } == usize::MAX {
        return;
    }
    unsafe { step(context, 10, 0) };
    unsafe { step(context, 11, 0) };
}

/// # Safety
/// Native callback owns the temporary queue-count field and vtable call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_refresh_buttons(
    context: *mut c_void,
    producer: usize,
    step: Option<Step>,
) {
    if producer == 0 {
        return;
    }
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } < 10 {
        return;
    }
    if unsafe { step(context, 2, 0) } >= 100 {
        return;
    }
    if unsafe { step(context, 3, 0) } >= 10 {
        return;
    }
    if unsafe { step(context, 4, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 5, 0) } == 0 {
        return;
    }
    unsafe { step(context, 6, 0) };
    if unsafe { step(context, 7, 0) } == 0 {
        return;
    }
    if unsafe { step(context, 8, 0) } < 10 {
        return;
    }
    if unsafe { step(context, 9, 0) } == usize::MAX {
        return;
    }
    if unsafe { step(context, 10, 0) } == usize::MAX {
        return;
    }
    if unsafe { step(context, 11, 0) } == usize::MAX {
        return;
    }
    if unsafe { step(context, 12, 0) } == usize::MAX {
        return;
    }
    unsafe { step(context, 13, 0) };
}

/// # Safety
/// Native callback owns vtable bounds, VirtualProtect, slot writes and cache
/// flush. Rust sequences checked admission and publication.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_install_ui(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for op in 0..4 {
        let result = unsafe { step(context, op, 0) };
        if result == 0 || result == usize::MAX {
            return false;
        }
    }
    for op in 4..7 {
        if unsafe { step(context, op, 0) } == usize::MAX {
            return false;
        }
    }
    true
}

/// # Safety
/// Native callback reads the live producer queue and performs the checked
/// engine push. No engine pointer is interpreted by Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_push_refit(
    context: *mut c_void,
    producer: usize,
    target: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) -> u32 {
    if !ready || producer == 0 || target == 0 {
        return 0;
    }
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } >= 10 || unsafe { *failed } != 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 0 || unsafe { *failed } != 0 {
        return 0;
    }
    if unsafe { step(context, 2, 0) } == 0 || unsafe { *failed } != 0 {
        return 0;
    }
    let mut ids = [0u32; 10];
    let mut count = 0usize;
    let mut item = unsafe { step(context, 3, 0) };
    while item != 0 && count < 10 {
        ids[count] = unsafe { step(context, 4, 0) } as u32;
        count += 1;
        item = unsafe { step(context, 5, 0) };
    }
    let before = unsafe { step(context, 0, 0) } as u32;
    unsafe { step(context, 6, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    let after = unsafe { step(context, 0, 0) } as u32;
    if after <= before {
        return 0;
    }
    item = unsafe { step(context, 3, 0) };
    for _ in 0..10 {
        if item == 0 {
            break;
        }
        let id = unsafe { step(context, 4, 0) } as u32;
        if !ids[..count].contains(&id) {
            return id;
        }
        item = unsafe { step(context, 5, 0) };
    }
    0
}

/// # Safety
/// Native callback owns live queue fields, event dispatch, and engine calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_finish(
    context: *mut c_void,
    producer: usize,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    let mut target = unsafe { step(context, 0, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    if target == 0 {
        target = unsafe { step(context, 1, 0) };
    }
    if unsafe { *failed } != 0 {
        return 0;
    }
    unsafe { step(context, 2, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    let native = producer == 0 || target == 0 || unsafe { step(context, 3, 0) } != 0;
    if unsafe { *failed } != 0 {
        return 0;
    }
    let result = if native {
        unsafe { step(context, 4, 0) }
    } else {
        0
    };
    if unsafe { *failed } != 0 {
        return 0;
    }
    if producer != 0 && target != 0 {
        unsafe { step(context, 5, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
        unsafe { step(context, 6, result) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    unsafe { step(context, 7, 0) };
    result
}

/// # Safety
/// Native callback owns producer state and cancellation engine calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_cancel(
    context: *mut c_void,
    producer: usize,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    unsafe { step(context, 0, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    let id = unsafe { step(context, 1, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    let target = unsafe { step(context, 2, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    if target != 0 {
        unsafe { step(context, 3, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    if producer != 0 && id != 0 {
        unsafe { step(context, 4, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    let result = unsafe { step(context, 5, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    unsafe { step(context, 6, 0) };
    result
}

/// # Safety
/// Native callback owns event notification, sidecars and engine destructor.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_dtor(
    context: *mut c_void,
    producer: usize,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if producer != 0 {
        unsafe { step(context, 0, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
        unsafe { step(context, 1, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    unsafe { step(context, 2, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    unsafe { step(context, 3, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    unsafe { step(context, 4, 0) }
}

/// # Safety
/// Native callback owns live queue fields, notifications and engine deletion.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_delete_native(
    context: *mut c_void,
    producer: usize,
    id: u32,
    step: Option<Step>,
    failed: *const u8,
) {
    if producer == 0 || id == 0 {
        return;
    }
    let Some(step) = step else { return };
    let current = unsafe { step(context, 0, 0) } as u32;
    if unsafe { *failed } != 0 {
        return;
    }
    let mut target = unsafe { step(context, 1, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    if target == 0 && id == current {
        target = unsafe { step(context, 2, 0) };
    }
    if unsafe { *failed } != 0 {
        return;
    }
    if id != current && unsafe { step(context, 3, 0) } != 0 && target != 0 {
        unsafe { step(context, 4, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
    }
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 5, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 6, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 7, 0) };
}

/// # Safety
/// Native callback owns the producer lock, timer state, and engine gateway.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_simulate(
    context: *mut c_void,
    producer: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) };
    if unsafe { *failed } != 0 || !ready || producer == 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } < 10 {
        if unsafe { *failed } != 0 {
            return;
        }
        if unsafe { step(context, 2, 0) } != 0 {
            if unsafe { *failed } != 0 {
                return;
            }
            unsafe { step(context, 3, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
        }
    }
    if unsafe { *failed } != 0 {
        return;
    }
    let active = unsafe { step(context, 4, 0) } != 0;
    if unsafe { *failed } != 0 {
        return;
    }
    let mut retry = false;
    if active && unsafe { step(context, 5, 0) } < 10 {
        if unsafe { *failed } != 0 {
            return;
        }
        if unsafe { step(context, 6, 0) } >= 30 {
            if unsafe { *failed } != 0 {
                return;
            }
            unsafe { step(context, 7, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            retry = true;
        }
    }
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 8, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    if retry {
        unsafe { step(context, 9, 0) };
    }
}

/// # Safety
/// Native callback owns command gateway, live queue snapshots and continuous
/// state storage. Rust sequences the synchronized marker workflow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_dequeue_command(
    context: *mut c_void,
    object: usize,
    command: u32,
    target: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    if command == 0xa3 {
        if unsafe { step(context, 0, 0) } == 0 && unsafe { *failed } == 0 {
            unsafe { step(context, 1, 0) };
        }
        return;
    }
    if command == 0xa4 {
        if unsafe { step(context, 2, 0) } == 0 && unsafe { *failed } == 0 {
            unsafe { step(context, 3, 0) };
        }
        return;
    }
    let fill = command == 0xa1;
    let continuous = command == 0xa2;
    let extended = command == 0xa5;
    if !fill && !continuous && !extended {
        unsafe { step(context, 4, 0) };
        return;
    }
    if !ready || object == 0 || target == 0 {
        unsafe { step(context, 5, 0) };
        return;
    }
    if extended {
        if unsafe { step(context, 6, 0) } != 0 {
            if unsafe { *failed } != 0 {
                return;
            }
            let repeat = unsafe { step(context, 7, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            unsafe { step(context, 8, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            if repeat == target {
                unsafe { step(context, 9, 0) };
                return;
            }
        }
        if unsafe { *failed } != 0 {
            return;
        }
        let added = unsafe { step(context, 10, 0) } != 0;
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, if added { 11 } else { 12 }, 0) };
        return;
    }
    if !continuous || unsafe { step(context, 13, 0) } != 0 {
        if unsafe { *failed } != 0 {
            return;
        }
        let added = unsafe { step(context, 14, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 15, added) };
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 8, 0) };
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 8, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    let effective = unsafe { step(context, 16, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    if effective == 0 {
        unsafe { step(context, 17, 0) };
        return;
    }
    let count = unsafe { step(context, 18, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    let backing = count != 0 && unsafe { step(context, 19, 0) } == effective;
    if unsafe { *failed } != 0 {
        return;
    }
    if !backing && unsafe { step(context, 20, 0) } == 0 {
        if unsafe { *failed } == 0 {
            unsafe { step(context, 21, 0) };
        }
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 22, 0) };
}

/// # Safety
/// Native callback owns keyboard state, sidecars and engine gateway.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_queue_command(
    context: *mut c_void,
    command: u32,
    target: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    if command == 0x19 && target != 0 {
        if unsafe { step(context, 0, 0) } != 0 || unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 1, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
    }
    if command != 0x19 || target == 0 || !ready {
        unsafe { step(context, 2, command as usize) };
        return;
    }
    unsafe { step(context, 3, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    if unsafe { step(context, 4, 0) } != 0 {
        if unsafe { *failed } != 0 {
            return;
        }
        let continuous = unsafe { step(context, 5, 0) } != 0;
        if unsafe { *failed } != 0 {
            return;
        }
        let marker = if continuous { 0xa2 } else { 0xa1 };
        unsafe { step(context, 6, marker) };
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 7, usize::from(continuous)) };
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    let pending = unsafe { step(context, 8, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    if pending != 0 || unsafe { step(context, 9, 0) } >= 10 {
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 10, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
        let active = unsafe { step(context, 11, 0) } != 0;
        if unsafe { *failed } != 0 {
            return;
        }
        let toggle = active && unsafe { step(context, 12, 0) } == target;
        if unsafe { *failed } != 0 {
            return;
        }
        if !toggle && unsafe { step(context, 13, 0) } == 0 {
            if unsafe { *failed } != 0 {
                return;
            }
            if unsafe { step(context, 14, 0) } == 0 {
                if unsafe { *failed } == 0 {
                    unsafe { step(context, 15, 0) };
                }
                return;
            }
        }
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 6, 0xa5) };
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 2, command as usize) };
}

/// # Safety
/// Native callback owns the synchronized engine gateway, lock, sidecars,
/// project ID lookup and diagnostic formatting. Rust owns admission order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_producer_push(
    context: *mut c_void,
    producer: usize,
    base: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    if producer != 0 && base != 0 && unsafe { step(context, 0, 0) } != 0 {
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    let mut target = base;
    if producer != 0 && base != 0 {
        target = unsafe { step(context, 1, 0) };
        if unsafe { *failed } != 0 || target == 0 {
            return;
        }
    }
    if !ready || producer == 0 || target == 0 {
        if producer != 0
            && target != 0
            && (unsafe { step(context, 2, 0) } == 0 || unsafe { *failed } != 0)
        {
            return;
        }
        let before = if producer != 0 {
            unsafe { step(context, 3, 0) }
        } else {
            0
        };
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 4, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
        if producer != 0 && target != 0 {
            let after = unsafe { step(context, 3, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            unsafe { step(context, 5, usize::from(after > before)) };
        }
        return;
    }
    unsafe { step(context, 6, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    let conflict = unsafe { step(context, 7, 0) } != 0;
    if unsafe { *failed } != 0 {
        return;
    }
    let barrier = unsafe { step(context, 8, 0) } != 0;
    if unsafe { *failed } != 0 {
        return;
    }
    let evolve = unsafe { step(context, 9, 0) } != 0;
    if unsafe { *failed } != 0 {
        return;
    }
    let mut suppress = conflict || barrier;
    let mut toggle = false;
    unsafe { step(context, 10, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    let lock = unsafe { step(context, 11, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    if lock != usize::MAX {
        let active = unsafe { step(context, 12, 0) } != 0;
        if unsafe { *failed } != 0 {
            return;
        }
        if active {
            let old_target = unsafe { step(context, 13, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            if old_target == base && !evolve {
                toggle = true;
                suppress = true;
            }
        }
        if unsafe { step(context, 14, usize::from(active)) } == usize::MAX {
            suppress = conflict || barrier;
        }
    } else {
        suppress = conflict || barrier;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    let before = unsafe { step(context, 15, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    let mut native = false;
    let mut extended = false;
    let mut full = false;
    if !suppress {
        unsafe { step(context, 16, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
        if unsafe { step(context, 17, 0) } == 0 {
            if unsafe { *failed } != 0 {
                return;
            }
            full = true;
            suppress = true;
        } else if unsafe { step(context, 18, 0) } == 0 {
            if unsafe { *failed } != 0 {
                return;
            }
            suppress = true;
        } else {
            if unsafe { *failed } != 0 {
                return;
            }
            let pending = unsafe { step(context, 19, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            if pending == 0 && before < 10 {
                unsafe { step(context, 20, 0) };
                if unsafe { *failed } != 0 {
                    return;
                }
                native = unsafe { step(context, 21, 0) } > before;
            } else {
                extended = unsafe { step(context, 22, 0) } != 0;
                suppress = !extended;
            }
            if unsafe { *failed } != 0 {
                return;
            }
        }
    }
    unsafe { step(context, 23, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 24, usize::from(native)) };
    if unsafe { *failed } != 0 {
        return;
    }
    if unsafe { step(context, 25, 0) } != 0 {
        if unsafe { *failed } != 0 {
            return;
        }
        let outcome = if conflict {
            0
        } else if barrier {
            1
        } else if toggle {
            2
        } else if full {
            3
        } else if extended {
            4
        } else if native {
            5
        } else if suppress {
            6
        } else {
            7
        };
        unsafe { step(context, 26, outcome) };
    }
}

/// # Safety
/// Native callback owns object handle, lock and pending vector allocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_append_pending(
    context: *mut c_void,
    producer: usize,
    target: usize,
    ready: bool,
    step: Option<Step>,
) -> bool {
    if producer == 0 || target == 0 || !ready {
        return false;
    }
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    let size = unsafe { step(context, 1, 0) };
    if size == usize::MAX {
        return false;
    }
    if size >= 100 {
        unsafe { step(context, 3, 0) };
        return false;
    }
    (unsafe { step(context, 2, 0) }) != 0
}

/// # Safety
/// Native callback owns live map iterator, lock and vector erase.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_erase_pending(
    context: *mut c_void,
    producer: usize,
    index: u32,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) -> bool {
    if producer == 0 || !ready {
        return false;
    }
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == 0 || unsafe { *failed } != 0 {
        return false;
    }
    let size = unsafe { step(context, 1, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    if index as usize >= size {
        unsafe { step(context, 5, 0) };
        return false;
    }
    unsafe { step(context, 2, index as usize) };
    if unsafe { *failed } != 0 {
        return false;
    }
    if unsafe { step(context, 3, 0) } != 0 {
        if unsafe { *failed } != 0 {
            return false;
        }
        unsafe { step(context, 4, 0) };
        if unsafe { *failed } != 0 {
            return false;
        }
    }
    unsafe { step(context, 5, 0) };
    true
}

/// # Safety
/// Native callback reads the current linked queue item one field at a time.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_find_target(
    context: *mut c_void,
    producer: usize,
    id: u32,
    step: Option<Step>,
) -> usize {
    if producer == 0 {
        return 0;
    }
    let Some(step) = step else { return 0 };
    let mut item = unsafe { step(context, 0, 0) };
    while item != 0 {
        if unsafe { step(context, 1, 0) } as u32 == id {
            return unsafe { step(context, 2, 0) };
        }
        item = unsafe { step(context, 3, 0) };
    }
    0
}

/// # Safety
/// Native callback owns the pending map iterator, critical section and copied
/// C++ snapshot. Rust owns the 32-bit truncation and 100-entry traversal.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_queue_snapshot_pending(
    context: *mut c_void,
    producer: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    if producer == 0 || !ready {
        return;
    }
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 || unsafe { *failed } != 0 {
        return;
    }
    let size = unsafe { step(context, 1, 0) } as u32;
    if unsafe { *failed } != 0 {
        return;
    }
    let count = core::cmp::min(size, 100);
    unsafe { step(context, 2, count as usize) };
    if unsafe { *failed } != 0 {
        return;
    }
    for index in 0..count {
        unsafe { step(context, 3, index as usize) };
        if unsafe { *failed } != 0 {
            return;
        }
    }
    unsafe { step(context, 4, 0) };
}
