//! Private UpgradePods portable workflows. Native object, STL, CRT and
//! engine storage remain in the C++ adapter.
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

#[repr(C)]
pub struct Span {
    pub start: usize,
    pub length: usize,
}

/// # Safety
/// Input is readable for `size` bytes and `output` is writable POD storage.
/// CRT `_stricmp` suffix comparison remains in the native adapter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_trim_name(
    input: *const u8,
    size: usize,
    output: *mut Span,
) -> bool {
    if input.is_null() || output.is_null() {
        return false;
    }
    let bytes = unsafe { core::slice::from_raw_parts(input, size) };
    let Some(first) = bytes.iter().position(|b| !b" \t\r\n".contains(b)) else {
        return false;
    };
    let last = bytes
        .iter()
        .rposition(|b| !b" \t\r\n".contains(b))
        .unwrap_or(first);
    let start = bytes[first..=last]
        .iter()
        .rposition(|b| *b == b'/' || *b == b'\\')
        .map_or(first, |slash| first + slash + 1);
    unsafe {
        output.write(Span {
            start,
            length: last + 1 - start,
        })
    };
    true
}

/// # Safety
/// Both spans are readable for their lengths. C++ retains string allocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_join_mode(
    left: *const u8,
    left_size: usize,
    right: *const u8,
    right_size: usize,
) -> u32 {
    if left_size == 0 {
        return 0;
    }
    if right_size == 0 {
        return 1;
    }
    if left.is_null() || right.is_null() {
        return 0;
    }
    let tail = unsafe { *left.add(left_size - 1) };
    if tail == b'\\' || tail == b'/' { 2 } else { 3 }
}

/// # Safety
/// Callback owns ifstream/ostringstream lifetime and file operations; `size`
/// and `failed` are readable caller-owned scalar storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_read_file(
    context: *mut c_void,
    step: Option<Step>,
    size: *const i64,
    maximum: i64,
    failed: *const u8,
) -> bool {
    let Some(step) = step else { return false };
    if size.is_null() || failed.is_null() {
        return false;
    }
    unsafe { step(context, 0, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0 || unsafe { *failed } != 0 {
        return false;
    }
    unsafe { step(context, 2, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    if unsafe { *size } < 0 || unsafe { *size } > maximum {
        unsafe { step(context, 3, 0) };
        return false;
    }
    unsafe { step(context, 4, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    let result = unsafe { step(context, 5, 0) } != 0;
    result && (unsafe { *failed }) == 0
}

/// # Safety
/// Callback owns the native API, path/file storage, existing Rust-backed
/// config parser wrapper, logs and exception capture. `failed` points to a
/// caller-owned exception flag.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_load_config(
    context: *mut c_void,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    unsafe { step(context, 0, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    let roots = unsafe { step(context, 1, 0) } as u32;
    if unsafe { *failed } != 0 {
        return;
    }
    if roots > 4096 {
        unsafe { step(context, 2, 0) };
        return;
    }
    let mut configured = false;
    for index in 0..roots {
        if unsafe { step(context, 3, index as usize) } == 0 {
            if unsafe { *failed } != 0 {
                return;
            }
            continue;
        }
        if unsafe { *failed } != 0 {
            return;
        }
        if unsafe { step(context, 4, 0) } == 0 {
            if unsafe { *failed } != 0 {
                return;
            }
            continue;
        }
        if unsafe { *failed } != 0 {
            return;
        }
        let status = unsafe { step(context, 5, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
        if status == 1 {
            unsafe { step(context, 6, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            configured = true;
        } else if status == 2 {
            unsafe { step(context, 7, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
        }
    }
    unsafe { step(context, 8, usize::from(configured)) };
}

/// # Safety
/// Native callback reads live class tiers/tags/systems and calls the original
/// Fleet Ops gateway. No class layout crosses the boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_same_type(
    context: *mut c_void,
    pod: usize,
    other: usize,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if failed.is_null() {
        return 0;
    }
    if pod != 0 && other != 0 {
        let own = unsafe { step(context, 0, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
        let own_tier = unsafe { step(context, 1, 0) } as u32;
        if unsafe { *failed } != 0 {
            return 0;
        }
        let other_tier = unsafe { step(context, 2, 0) } as u32;
        if unsafe { *failed } != 0 {
            return 0;
        }
        let own_pod = unsafe { step(context, 3, 0) } != 0;
        if unsafe { *failed } != 0 {
            return 0;
        }
        if own_pod && unsafe { step(context, 4, 0) } != 0 {
            if unsafe { *failed } != 0 {
                return 0;
            }
            let own_system = unsafe { step(context, 5, 0) } as u32;
            if unsafe { *failed } != 0 {
                return 0;
            }
            let other_system = unsafe { step(context, 6, 0) } as u32;
            if unsafe { *failed } != 0 {
                return 0;
            }
            if own_tier >= 3
                && other_tier == 2
                && own_system == other_system
                && unsafe { step(context, 7, 0) } != 0
            {
                return 1;
            }
            if unsafe { *failed } != 0 {
                return 0;
            }
        }
        if own_tier > 3 || other_tier > 3 {
            if unsafe { step(context, 3, 0) } == 0 || unsafe { *failed } != 0 {
                return 0;
            }
            if unsafe { step(context, 4, 0) } == 0 || unsafe { *failed } != 0 {
                return 0;
            }
            let own_system = unsafe { step(context, 5, 0) } as u32;
            if unsafe { *failed } != 0 {
                return 0;
            }
            let other_system = unsafe { step(context, 6, 0) } as u32;
            if unsafe { *failed } != 0 {
                return 0;
            }
            return usize::from(own_tier == other_tier && own_system == other_system);
        }
        let _ = own;
    }
    unsafe { step(context, 8, 0) }
}

/// # Safety
/// Native callback owns the class-sidecar lock, tier lists, and gateway.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_can_build(
    context: *mut c_void,
    station: usize,
    item: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if failed.is_null() {
        return 0;
    }
    if station != 0 && item != 0 && ready {
        let found = unsafe { step(context, 0, 0) } != 0;
        if unsafe { *failed } != 0 {
            return 0;
        }
        if found {
            for tier in 2..15 {
                for slot in 0..58 {
                    if unsafe { step(context, 1, tier * 58 + slot) } != 0 {
                        if unsafe { *failed } != 0 {
                            return 0;
                        }
                        if unsafe { step(context, 2, tier * 58 + slot) } == item {
                            if unsafe { *failed } != 0 {
                                return 0;
                            }
                            unsafe { step(context, 3, 0) };
                            return 1;
                        }
                    }
                    if unsafe { *failed } != 0 {
                        return 0;
                    }
                }
            }
        }
        unsafe { step(context, 3, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    unsafe { step(context, 4, 0) }
}

/// # Safety
/// Native callback owns class fields, sidecar storage and diagnostic strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_pod_class(
    context: *mut c_void,
    class: usize,
    step: Option<Step>,
    failed: *const u8,
) {
    if class == 0 {
        return;
    }
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    let tier = unsafe { step(context, 0, 0) } as u32;
    if unsafe { *failed } != 0 {
        unsafe { step(context, 6, 0) };
        return;
    }
    let maximum = unsafe { step(context, 1, 0) } as u32;
    if unsafe { *failed } != 0 {
        unsafe { step(context, 6, 0) };
        return;
    }
    let mut declared = tier;
    if tier > maximum {
        declared = 3;
        unsafe { step(context, 2, 0) };
        if unsafe { *failed } != 0 {
            unsafe { step(context, 6, 0) };
            return;
        }
    }
    if tier > 3 {
        unsafe { step(context, 3, 0) };
        if unsafe { *failed } != 0 {
            unsafe { step(context, 6, 0) };
            return;
        }
    }
    unsafe { step(context, 4, declared as usize) };
    if unsafe { *failed } != 0 {
        unsafe { step(context, 6, 0) };
        return;
    }
    if declared > 3 {
        unsafe { step(context, 5, declared as usize) };
        if unsafe { *failed } != 0 {
            unsafe { step(context, 6, 0) };
        }
    }
}

/// # Safety
/// Native callback owns both independent exception/catch blocks and hybrid
/// publication operations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_station_class(
    context: *mut c_void,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    if unsafe { step(context, 0, 0) } == 0 {
        unsafe { step(context, 1, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
    }
    if unsafe { *failed } != 0 {
        return;
    }
    if unsafe { step(context, 2, 0) } == 0 {
        if unsafe { *failed } == 0 {
            unsafe { step(context, 4, 0) };
        }
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    if unsafe { step(context, 3, 0) } == 0 && unsafe { *failed } == 0 {
        unsafe { step(context, 4, 0) };
    }
}

/// # Safety
/// Native callback owns station allocation and its original catch diagnostic.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_station_constructor(
    context: *mut c_void,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    if unsafe { step(context, 0, 0) } == 0 && unsafe { *failed } == 0 {
        unsafe { step(context, 1, 0) };
    }
}

/// # Safety
/// Native callback owns cleanup, original gateway and station sidecar erase.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_station_destructor(
    context: *mut c_void,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if failed.is_null() {
        return 0;
    }
    unsafe { step(context, 0, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    unsafe { step(context, 1, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    let result = unsafe { step(context, 2, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    unsafe { step(context, 3, 0) };
    result
}

/// # Safety
/// Native callback owns pod/station sidecars and catch diagnostic.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_pod_attach(
    context: *mut c_void,
    step: Option<Step>,
    failed: *const u8,
) {
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    if unsafe { step(context, 0, 0) } == 0 {
        if unsafe { *failed } == 0 {
            unsafe { step(context, 2, 0) };
        }
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    if unsafe { step(context, 1, 0) } == 0 && unsafe { *failed } == 0 {
        unsafe { step(context, 2, 0) };
    }
}

/// # Safety
/// Native callback owns ParameterDB strings, exact CRT name normalization,
/// class lookup, station storage, diagnostics, and exception capture. Rust
/// sequences indexed tier admission and sidecar publication.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_tier_lists(
    context: *mut c_void,
    station: usize,
    db: usize,
    step: Option<Step>,
    failed: *const u8,
) {
    if station == 0 || db == 0 {
        return;
    }
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    if unsafe { step(context, 0, 0) } == 0 {
        if unsafe { *failed } == 0 {
            unsafe { step(context, 1, 0) };
        }
        return;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    let maximum = unsafe { step(context, 2, 0) } as u32;
    if unsafe { *failed } != 0 {
        return;
    }
    let mut extended = false;
    for command_tier in 0..maximum.saturating_sub(1) {
        let level = command_tier + 2;
        let capacity = if level == 2 { 57 } else { 58 };
        unsafe { step(context, 3, command_tier as usize) };
        if unsafe { *failed } != 0 {
            return;
        }
        let mut supplied = false;
        let mut indexed = false;
        let mut replacements = [0usize; 58];
        let mut replacement_count = 0usize;
        for index in 0..capacity {
            if unsafe { step(context, 4, index) } == 0 {
                if unsafe { *failed } != 0 {
                    return;
                }
                continue;
            }
            if unsafe { *failed } != 0 {
                return;
            }
            supplied = true;
            if unsafe { step(context, 5, 0) } == 0 {
                if unsafe { *failed } != 0 {
                    return;
                }
                unsafe { step(context, 6, index) };
                if unsafe { *failed } != 0 {
                    return;
                }
                indexed = true;
                continue;
            }
            if unsafe { *failed } != 0 {
                return;
            }
            let have_id = unsafe { step(context, 7, 0) } != 0;
            if unsafe { *failed } != 0 {
                return;
            }
            let id = unsafe { step(context, 8, 0) } as u32;
            if unsafe { *failed } != 0 {
                return;
            }
            let mut item = 0;
            if have_id && id != 0 {
                item = unsafe { step(context, 9, 0) };
                if unsafe { *failed } != 0 {
                    return;
                }
            }
            if item == 0 {
                item = unsafe { step(context, 10, 0) };
                if unsafe { *failed } != 0 {
                    return;
                }
            }
            if item == 0 {
                unsafe { step(context, 11, 0) };
                if unsafe { *failed } != 0 {
                    return;
                }
                continue;
            }
            if unsafe { step(context, 12, 0) } == 0 {
                if unsafe { *failed } != 0 {
                    return;
                }
                unsafe { step(context, 13, 0) };
                if unsafe { *failed } != 0 {
                    return;
                }
                continue;
            }
            if unsafe { *failed } != 0 {
                return;
            }
            if unsafe { step(context, 14, 0) } as u32 != level {
                if unsafe { *failed } != 0 {
                    return;
                }
                unsafe { step(context, 15, 0) };
                if unsafe { *failed } != 0 {
                    return;
                }
                continue;
            }
            if unsafe { *failed } != 0 {
                return;
            }
            unsafe { step(context, 16, index) };
            if unsafe { *failed } != 0 {
                return;
            }
            indexed = true;
            if !replacements[..replacement_count].contains(&item) {
                replacements[replacement_count] = item;
                replacement_count += 1;
                unsafe { step(context, 17, 0) };
                if unsafe { *failed } != 0 {
                    return;
                }
            }
        }
        if !supplied {
            continue;
        }
        if !indexed {
            unsafe { step(context, 18, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            continue;
        }
        unsafe { step(context, 19, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
        let replaced = if level == 2 {
            (unsafe { step(context, 20, 0) }) != 0
        } else if level == 3 {
            (unsafe { step(context, 21, 0) }) != 0
        } else {
            true
        };
        if unsafe { *failed } != 0 {
            return;
        }
        if !replaced {
            continue;
        }
        if command_tier < 15 {
            if command_tier >= 2 && replacement_count != 0 {
                extended = true;
            }
            unsafe { step(context, 22, usize::from(extended)) };
            if unsafe { *failed } != 0 {
                return;
            }
        }
        unsafe { step(context, 23, replacement_count) };
        if unsafe { *failed } != 0 {
            return;
        }
    }
    if extended {
        if unsafe { step(context, 24, 0) } == 0 {
            if unsafe { *failed } == 0 {
                unsafe { step(context, 25, 0) };
            }
            return;
        }
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 26, 0) };
        if unsafe { *failed } != 0 {
            return;
        }
        unsafe { step(context, 27, 0) };
    }
}

/// # Safety
/// Native callback owns pod and station pointer reads, lock-scoped sidecar
/// references, active-list writes, InterlockedIncrement and diagnostics.
/// Rust retains only scalar class identities and tiers across callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_refresh_station(
    context: *mut c_void,
    station: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    if station == 0 || !ready {
        return;
    }
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    macro_rules! call {
        ($op:expr,$arg:expr) => {{
            let value = unsafe { step(context, $op, $arg) };
            if unsafe { *failed } != 0 {
                return;
            }
            value
        }};
    }
    if call!(0, 0) == 0 || call!(1, 0) == 0 || call!(2, 0) == 0 {
        return;
    }
    let raw = call!(3, 0) as u32 as i32;
    if raw < 0 {
        return;
    }
    let count = core::cmp::min(raw as usize, 58);
    let pods = call!(4, 0);
    if count != 0 && (pods == 0 || call!(5, count) == 0) {
        return;
    }
    let mut attached = [0usize; 5];
    let mut attached_tiers = [0u32; 5];
    for index in 0..count {
        if call!(6, index) == 0 || call!(7, 0) == 0 {
            continue;
        }
        let class = call!(8, 0);
        if call!(9, 0) == 0 {
            continue;
        }
        let system = call!(10, 0);
        if system >= 5 {
            continue;
        }
        let tier = call!(11, 0) as u32;
        if attached[system] == 0 || tier > attached_tiers[system] {
            attached[system] = class;
            attached_tiers[system] = tier;
        }
    }
    let found = call!(12, 0) != 0;
    let mut logged_slot = 58usize;
    let mut logged_from = 0u32;
    let mut logged_to = 0u32;
    if found {
        for slot in 0..58 {
            let mut candidate = call!(13, slot);
            let mut system = 5usize;
            if call!(14, 0) != 0 {
                system = call!(15, 0);
            }
            if system >= 5 {
                for tier in 0..15 {
                    let configured = call!(16, tier * 58 + slot);
                    if call!(17, 0) == 0 {
                        continue;
                    }
                    system = call!(18, 0);
                    let _ = configured;
                    break;
                }
            }
            let attached_tier = if system < 5 {
                attached_tiers[system]
            } else {
                0
            };
            if attached_tier >= 2 {
                if attached_tier >= 3 {
                    candidate = 0;
                }
                let next = (attached_tier - 1) as usize;
                if next < 15 && call!(19, next * 58 + slot) != 0 {
                    candidate = call!(20, next * 58 + slot);
                }
                if candidate == 0 && attached_tier >= 3 {
                    candidate = attached[system];
                }
            }
            call!(21, candidate);
            let candidate_tier = call!(22, 0) as u32;
            if candidate_tier > 3 && logged_slot == 58 {
                logged_slot = slot;
                logged_from = attached_tier;
                logged_to = candidate_tier;
            }
        }
        call!(23, 0);
    }
    call!(24, 0);
    if logged_slot != 58 {
        call!(26, logged_slot);
        call!(27, logged_from as usize);
        call!(28, logged_to as usize);
        if (call!(25, 0) as i32) <= 12 {
            call!(29, 0);
        }
    }
}

/// # Safety
/// Native callback iterates the live unordered map under its critical
/// section and retains multiplier bits for the original engine gateway.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_recompute(
    context: *mut c_void,
    manager: usize,
    system: u32,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    if manager == 0 || system > 4 || !ready {
        return;
    }
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            value
        }};
    }
    call!(0);
    let mut selected = false;
    let mut tier = 3u32;
    while call!(1) != 0 {
        if call!(2) == manager && call!(3) == system as usize {
            if !selected {
                tier = call!(5) as u32;
                selected = true;
            } else if call!(4) as u32 > tier {
                tier = call!(5) as u32;
            }
        }
        call!(6);
    }
    call!(7);
    if selected {
        call!(8);
    }
}

/// # Safety
/// Native callback owns class pointer validation, manager lookup, sidecar
/// map mutation and exact float-bit transport to the engine.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_attach_pod(
    context: *mut c_void,
    pod: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    if pod == 0 || !ready {
        return;
    }
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if unsafe { *failed } != 0 {
                return;
            }
            value
        }};
    }
    call!(0);
    if call!(1) == 0 {
        return;
    }
    let tier = call!(2) as u32;
    if tier < 3 {
        return;
    }
    let system = call!(3) as u32;
    if system > 4 {
        return;
    }
    call!(4);
    if call!(5) == 0 {
        return;
    }
    call!(6);
    call!(7);
    if tier > 3 {
        call!(8);
    }
    call!(9);
}

/// # Safety
/// Native callback validates writable ranges and writes original station
/// item/flag storage. Rust selects indexed replacements in source order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_apply_items(
    context: *mut c_void,
    items: usize,
    flags: bool,
    capacity: usize,
    step: Option<Step>,
    failed: *const u8,
) -> bool {
    let Some(step) = step else { return false };
    if failed.is_null() {
        return false;
    }
    if items == 0 || capacity == 0 || unsafe { step(context, 0, 0) } == 0 {
        if unsafe { *failed } == 0 {
            unsafe { step(context, 1, 0) };
        }
        return false;
    }
    if unsafe { *failed } != 0 {
        return false;
    }
    if flags && unsafe { step(context, 2, 0) } == 0 {
        if unsafe { *failed } == 0 {
            unsafe { step(context, 3, 0) };
        }
        return false;
    }
    if unsafe { *failed } != 0 {
        return false;
    }
    for index in 0..capacity {
        if unsafe { step(context, 4, index) } != 0 {
            if unsafe { *failed } != 0 {
                return false;
            }
            unsafe { step(context, 5, index) };
            if unsafe { *failed } != 0 {
                return false;
            }
        }
    }
    true
}

/// # Safety
/// Native callback owns the station range check, unique_ptr allocation,
/// map publication and instance pointer write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_init_station(
    context: *mut c_void,
    station: usize,
    class: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) {
    if station == 0 || !ready {
        return;
    }
    let Some(step) = step else { return };
    if failed.is_null() {
        return;
    }
    if unsafe { step(context, 0, 0) } == 0 || unsafe { *failed } != 0 {
        return;
    }
    if class == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    let found = unsafe { step(context, 2, 0) } != 0;
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 3, 0) };
    if unsafe { *failed } != 0 || !found {
        return;
    }
    unsafe { step(context, 4, 0) };
    if unsafe { *failed } != 0 {
        return;
    }
    unsafe { step(context, 5, 0) };
}

/// # Safety
/// Native callback owns station pointer checks, map erase, original detach
/// gateway, multiplier update and rollback diagnostic.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_detach_pod(
    context: *mut c_void,
    pod: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if failed.is_null() {
        return 0;
    }
    if pod != 0 && unsafe { step(context, 0, 0) } != 0 {
        if unsafe { *failed } != 0 {
            return 0;
        }
        unsafe { step(context, 1, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    let mut tracked = false;
    if pod != 0 && ready {
        tracked = unsafe { step(context, 2, 0) } != 0;
        if unsafe { *failed } != 0 {
            return 0;
        }
        if tracked {
            unsafe { step(context, 3, 0) };
            if unsafe { *failed } != 0 {
                return 0;
            }
        }
        unsafe { step(context, 4, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    let result = unsafe { step(context, 5, 0) };
    if unsafe { *failed } != 0 {
        return 0;
    }
    if tracked {
        unsafe { step(context, 6, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    if unsafe { step(context, 7, 0) } == 0 && unsafe { *failed } == 0 {
        unsafe { step(context, 8, 0) };
    }
    result
}

/// # Safety
/// Native callback owns API pointers, checked PE reads, hook installation,
/// critical section and C++ exception capture. Rust preserves all fourteen
/// short-circuit signature probes and nine install/publication steps.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_initialize(
    context: *mut c_void,
    step: Option<Step>,
    failed: *const u8,
) -> bool {
    let Some(step) = step else { return false };
    if failed.is_null() {
        return false;
    }
    if unsafe { step(context, 0, 0) } == 0 {
        if unsafe { *failed } == 0 {
            unsafe { step(context, 1, 0) };
        }
        return false;
    }
    if unsafe { *failed } != 0 {
        return false;
    }
    for index in 0..14 {
        if unsafe { step(context, 2, index) } == 0 {
            if unsafe { *failed } == 0 {
                unsafe { step(context, 3, 0) };
            }
            return false;
        }
        if unsafe { *failed } != 0 {
            return false;
        }
    }
    unsafe { step(context, 4, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    unsafe { step(context, 5, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    unsafe { step(context, 6, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    for index in 0..9 {
        if unsafe { step(context, 7, index) } == 0 {
            if unsafe { *failed } == 0 {
                unsafe { step(context, 8, 0) };
            }
            return false;
        }
        if unsafe { *failed } != 0 {
            return false;
        }
    }
    unsafe { step(context, 9, 0) };
    if unsafe { *failed } != 0 {
        return false;
    }
    unsafe { step(context, 10, 0) };
    (unsafe { *failed }) == 0
}

/// # Safety
/// Native callback owns class-sidecar erase and original destructor gateway.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_upgrade_class_dtor(
    context: *mut c_void,
    class: usize,
    ready: bool,
    step: Option<Step>,
    failed: *const u8,
) -> usize {
    let Some(step) = step else { return 0 };
    if failed.is_null() {
        return 0;
    }
    if class != 0 && ready {
        unsafe { step(context, 0, 0) };
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    unsafe { step(context, 1, 0) }
}
