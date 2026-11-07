// Portable SOL reader cursor and record admission. Caller checks native ranges.
#[repr(C)]
pub struct SolCursor {
    pub base: usize,
    pub cursor: usize,
    pub end: usize,
    pub buffer_size: u32,
    pub offset: usize,
    pub remaining: usize,
}

/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_cursor(state: *mut SolCursor) -> bool {
    if state.is_null() {
        return false;
    }
    let state = unsafe { &mut *state };
    if state.base == 0
        || state.cursor < state.base
        || state.end < state.cursor
        || state.end - state.base > state.buffer_size as usize
    {
        return false;
    }
    state.offset = state.cursor - state.base;
    state.remaining = state.end - state.cursor;
    true
}

/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_target(
    state: *const SolCursor,
    offset: usize,
    out: *mut usize,
) -> bool {
    if state.is_null() || out.is_null() {
        return false;
    }
    let state = unsafe { &*state };
    if offset > state.buffer_size as usize {
        return false;
    }
    let Some(target) = state.base.checked_add(offset) else {
        return false;
    };
    if target > state.end {
        return false;
    }
    unsafe {
        *out = target;
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_sol_object_span(
    selected: bool,
    offset: usize,
    buffer_size: u32,
) -> bool {
    selected && offset != 0 && offset <= buffer_size as usize && buffer_size as usize - offset >= 12
}

/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_object_record(
    record: *const u8,
    expected_count: u32,
) -> bool {
    if record.is_null() {
        return false;
    }
    let record = unsafe { core::slice::from_raw_parts(record, 12) };
    let tag = u32::from_le_bytes([record[0], record[1], record[2], record[3]]);
    let size = u32::from_le_bytes([record[4], record[5], record[6], record[7]]);
    let count = u32::from_le_bytes([record[8], record[9], record[10], record[11]]);
    tag == 4 && size == 4 && count == expected_count
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_sol_mission_span(offset: usize, buffer_size: u32) -> bool {
    offset != 0 && offset <= buffer_size as usize && buffer_size as usize - offset >= 48
}

/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_mission_record(record: *const u8) -> bool {
    if record.is_null() {
        return false;
    }
    let record = unsafe { core::slice::from_raw_parts(record, 48) };
    let tag = u32::from_le_bytes([record[0], record[1], record[2], record[3]]);
    let size = u32::from_le_bytes([record[4], record[5], record[6], record[7]]);
    tag == 2 && size == 40 && &record[8..20] == b"EmptyMission" && record[20] == 0
}
/// # Safety
/// Nonempty path views must point to their declared number of readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_join_action(
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
        return 1;
    }
    let last = unsafe { *left.add(left_size - 1) };
    if last == b'\\' || last == b'/' { 2 } else { 3 }
}
/// # Safety
/// `path` points to `size` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_basename_start(path: *const u8, size: usize) -> usize {
    if path.is_null() {
        return 0;
    }
    let mut start = 0;
    for index in 0..size {
        let byte = unsafe { *path.add(index) };
        if byte == b'\\' || byte == b'/' {
            start = index + 1
        }
    }
    start
}
pub type Lower = unsafe extern "C" fn(u8) -> u8;
/// # Safety
/// Caller supplies a writable buffer of `size` bytes and a non-unwinding callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_lower_ascii(
    bytes: *mut u8,
    size: usize,
    lower: Option<Lower>,
) {
    let Some(lower) = lower else { return };
    if bytes.is_null() {
        return;
    }
    for index in 0..size {
        let slot = unsafe { bytes.add(index) };
        let value = unsafe { *slot };
        unsafe { *slot = lower(value) };
    }
}
/// # Safety
/// `value` points to `size` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_has_extension(value: *const u8, size: usize) -> bool {
    if value.is_null() || size <= 4 {
        return false;
    }
    let suffix = unsafe { core::slice::from_raw_parts(value.add(size - 4), 4) };
    suffix == b".sol"
}
pub type ResolveStep = unsafe extern "C" fn(*mut core::ffi::c_void, u32) -> i32;
/// # Safety
/// `request` points to `size` readable bytes; callback does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_resolve_source(
    context: *mut core::ffi::c_void,
    request: *const u8,
    size: usize,
    step: Option<ResolveStep>,
) -> bool {
    let Some(step) = step else { return false };
    if request.is_null() {
        return false;
    }
    if unsafe { step(context, 0) } <= 0 {
        return false;
    }
    let mut direct = size > 2;
    if !direct {
        for index in 0..size {
            let byte = unsafe { *request.add(index) };
            if byte == b'\\' || byte == b'/' {
                direct = true;
                break;
            }
        }
    }
    if direct {
        let available = unsafe { step(context, 1) };
        if available < 0 {
            return false;
        }
        if available > 0 {
            return (unsafe { step(context, 2) }) > 0;
        }
    }
    (unsafe { step(context, 3) }) > 0
}
pub type Root = unsafe extern "C" fn(*mut core::ffi::c_void, u32) -> i32;
/// # Safety
/// Callback must be valid for the supplied context and must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_index_roots(
    context: *mut core::ffi::c_void,
    count: u32,
    root: Option<Root>,
) -> bool {
    let Some(root) = root else { return false };
    for index in 0..count {
        if unsafe { root(context, index) } < 0 {
            return false;
        }
    }
    true
}
pub type DirectoryStep = unsafe extern "C" fn(*mut core::ffi::c_void, u32, *mut bool) -> i32;
/// # Safety
/// Callback owns native directory state and must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_walk_directory(
    context: *mut core::ffi::c_void,
    step: Option<DirectoryStep>,
) -> bool {
    let Some(step) = step else { return false };
    let mut directory = false;
    let first = unsafe { step(context, 0, &mut directory) };
    if first <= 0 {
        return first == 0;
    }
    loop {
        if !directory && unsafe { step(context, 1, &mut directory) } < 0 {
            return false;
        }
        let next = unsafe { step(context, 2, &mut directory) };
        if next < 0 {
            return false;
        }
        if next == 0 {
            break;
        }
    }
    (unsafe { step(context, 3, &mut directory) }) >= 0
}

pub type MapDetailsStep = unsafe extern "C" fn(*mut core::ffi::c_void, u32, usize) -> usize;
/// # Safety
/// The callback owns native MapDetails storage and does not unwind. A failed
/// native step returns `usize::MAX`; the C++ caller rethrows captured errors.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_create_map_details(
    context: *mut core::ffi::c_void,
    step: Option<MapDetailsStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    let call = |op, index| unsafe { step(context, op, index) };
    let admitted = call(0, 0);
    if admitted == 0 || admitted == usize::MAX {
        return 0;
    }
    let allocated = call(1, 0);
    if allocated == 0 || allocated == usize::MAX {
        return 0;
    }
    let constructed = call(2, 0);
    if constructed == 0 || constructed == usize::MAX {
        return 0;
    }
    for operation in 3..=11 {
        if call(operation, 0) == usize::MAX {
            return 0;
        }
    }
    let starts = call(12, 0);
    if starts == usize::MAX {
        return 0;
    }
    if starts != 0 {
        let count = call(13, 0);
        if count == usize::MAX {
            return 0;
        }
        for index in 0..count {
            if call(14, index) == usize::MAX {
                return 0;
            }
        }
    }
    call(15, 0)
}

/// # Safety
/// The callback owns native reader, map and mission objects. It catches native
/// exceptions and reports `usize::MAX` so the caller can rethrow after Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_load_main(
    context: *mut core::ffi::c_void,
    step: Option<MapDetailsStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr) => {{
            let result = unsafe { step(context, $op, 0) };
            if result == usize::MAX {
                return 0;
            }
            result
        }};
    }
    if call!(0) == 0 {
        return usize::from(call!(1) != 0);
    }
    call!(2); // entry cursor inspection
    call!(3); // count before native call
    let native_loaded = call!(4) != 0;
    call!(5); // cursor after native call
    if call!(6) != 0 {
        let mission = call!(7) != 0;
        call!(8); // reached-bridge diagnostic
        return usize::from(mission && call!(9) != 0);
    }
    if native_loaded {
        call!(10);
        return 1;
    }
    call!(11); // world bounds
    let bridged = call!(12) != 0;
    let mission = call!(13) != 0;
    call!(14); // recovery diagnostic
    usize::from(bridged && mission)
}

/// # Safety
/// The callback owns the selected map and native object/mission lifetime. It
/// does not unwind, and a live object count is reread at each iteration.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_spawn(
    context: *mut core::ffi::c_void,
    step: Option<MapDetailsStep>,
) -> bool {
    let Some(step) = step else { return false };
    let call = |op, index| unsafe { step(context, op, index) };
    if call(0, 0) == 0 {
        call(3, 0);
        return false;
    }
    let previous = call(1, 0);
    if previous == 2 {
        call(2, 0);
        return true;
    }
    if previous != 0 {
        call(3, 0);
        return false;
    }
    if call(4, 0) == 0 {
        call(16, 0);
        call(3, 0);
        return false;
    }
    call(5, 0);
    call(6, 0);
    let mut index = 0;
    while index < call(7, 0) {
        if call(8, index) == 0 {
            call(9, index);
        } else if call(10, index) == 0 {
            call(11, index);
        } else {
            call(12, index);
            call(13, index);
            call(14, index);
        }
        index += 1;
    }
    call(15, 0);
    true
}

/// # Safety
/// Callback owns native parser and string state, catches exceptions and
/// returns `usize::MAX` on failure for the C++ caller to rethrow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_create_selected(
    context: *mut core::ffi::c_void,
    selected: bool,
    step: Option<MapDetailsStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == usize::MAX {
                return 0;
            }
            value
        }};
    }
    if call!(0) == 0 {
        call!(1);
        return 0;
    }
    if call!(2) == 0 {
        call!(3);
        return 0;
    }
    if selected {
        call!(4);
    }
    call!(5);
    if call!(6) != 0 {
        call!(7);
    }
    call!(8);
    call!(9)
}

/// # Safety
/// The native callback owns reader/mission state, catches exceptions, and
/// reports `usize::MAX` for the C++ caller to rethrow after this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_load_stage(
    context: *mut core::ffi::c_void,
    kind: u32,
    step: Option<MapDetailsStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == usize::MAX {
                return 0;
            }
            value
        }};
    }
    if call!(0) == 0 {
        return usize::from(call!(1) != 0);
    }
    match kind {
        0 => {
            let inspected = call!(2) != 0;
            if inspected {
                call!(3);
            }
            call!(4);
            let bridged = call!(5) != 0;
            call!(6);
            usize::from(bridged)
        }
        1 | 3 | 4 => {
            call!(2);
            1
        }
        2 => {
            let inspected = call!(2) != 0;
            if inspected && call!(3) != 0 {
                call!(4);
            }
            let mission = call!(5) != 0;
            call!(6);
            usize::from(mission)
        }
        _ => 0,
    }
}

/// # Safety
/// The callback owns native probe-result storage and catches all exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_probe(
    context: *mut core::ffi::c_void,
    step: Option<MapDetailsStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == usize::MAX {
                return 0;
            }
            value
        }};
    }
    call!(0); // native reader/probe first
    if call!(1) == 0 || call!(2) == 0 {
        return call!(9);
    }
    if call!(11) != 0 {
        call!(3);
    }
    if call!(12) != 0 {
        call!(4);
    }
    if call!(13) != 0 {
        call!(5);
    }
    if call!(14) != 0 {
        call!(6);
    }
    call!(7);
    if call!(14) == 0 {
        call!(8);
    }
    call!(9)
}

pub type InitStep = unsafe extern "C" fn(*mut core::ffi::c_void, u32, usize) -> i32;
/// # Safety
/// Callback performs exact native signature and patch operations, catches
/// exceptions, and returns -1 for the C++ caller to rethrow after Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_module_init(
    context: *mut core::ffi::c_void,
    step: Option<InitStep>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr, $index:expr) => {{
            let value = unsafe { step(context, $op, $index) };
            if value < 0 {
                return false;
            }
            value
        }};
    }
    if call!(0, 0) == 0 || call!(1, 0) == 0 {
        return false;
    }
    let a1_active = call!(2, 0) != 0;
    for index in 0..10 {
        if call!(3, index) == 0 {
            call!(4, 0);
            return false;
        }
    }
    if !a1_active {
        for index in 0..2 {
            if call!(5, index) == 0 {
                call!(6, 0);
                return false;
            }
        }
        for index in 0..3 {
            if call!(7, index) == 0 {
                call!(8, 0);
                return false;
            }
        }
        for index in 0..5 {
            if call!(9, index) == 0 {
                call!(10, 0);
                return false;
            }
        }
        call!(11, 0);
    } else {
        call!(12, 0);
    }
    for index in 0..6 {
        if call!(13, index) == 0 {
            call!(14, 0);
            return false;
        }
    }
    for index in 0..2 {
        if call!(15, index) == 0 {
            call!(16, 0);
            return false;
        }
    }
    for index in 0..2 {
        if call!(17, index) == 0 {
            call!(18, 0);
            return false;
        }
    }
    call!(19, 0);
    if call!(20, 0) != 0 {
        call!(21, 0);
    } else {
        call!(22, 0);
        call!(23, 0);
        let mut reported = 0;
        while call!(24, 0) != 0 {
            if reported >= 16 {
                call!(25, 0);
                break;
            }
            call!(26, 0);
            call!(27, 0);
            reported += 1;
        }
    }
    true
}

/// # Safety
/// Callback owns the native map iterator, virtual mask and game calls; it
/// captures native exceptions for the caller to rethrow after Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_discovery(
    context: *mut core::ffi::c_void,
    step: Option<InitStep>,
) {
    let Some(step) = step else { return };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value < 0 {
                return;
            }
            value
        }};
    }
    if call!(0) == 0 {
        return;
    }
    call!(1);
    if call!(2) == 0 {
        return;
    }
    if call!(3) != 0 {
        call!(4);
    }
    call!(5);
    while call!(6) != 0 {
        if call!(7) != 0 {
            call!(9);
            call!(10);
            break;
        }
        call!(8);
    }
}

/// # Safety
/// Callback owns native filename, module and MapDetails objects; it catches
/// exceptions and reports `usize::MAX` for native rethrow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_map_hook(
    context: *mut core::ffi::c_void,
    selected: bool,
    step: Option<MapDetailsStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == usize::MAX {
                return 0;
            }
            value
        }};
    }
    if selected {
        call!(0);
    }
    if call!(1) == 0 {
        return 0;
    }
    if call!(2) != 0 {
        if selected {
            call!(3);
        } else {
            call!(6);
        }
        call!(4)
    } else {
        call!(5)
    }
}

/// # Safety
/// Callback owns native object/AI reads and catches exceptions before FFI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_ai_guard(
    context: *mut core::ffi::c_void,
    step: Option<MapDetailsStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == usize::MAX {
                return 0;
            }
            value
        }};
    }
    if call!(0) == 0 || call!(1) == 0 {
        return 0;
    }
    if call!(2) == 0 {
        if call!(3) != 0 {
            call!(4);
        }
        return 0;
    }
    usize::from(call!(5) != 0)
}

/// # Safety
/// Callback retains native nebula and hook lifetime. Native exceptions end
/// in its original noexcept boundary and never cross Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_nebula_guard(
    context: *mut core::ffi::c_void,
    step: Option<MapDetailsStep>,
) -> bool {
    let Some(step) = step else { return false };
    let call = |op| unsafe { step(context, op, 0) };
    if call(0) != 0 && call(1) != 0 && call(2) != 0 {
        call(3);
        return call(4) != 0;
    }
    call(5) != 0 && call(6) != 0
}

/// # Safety
/// The callback writes only caller-owned native output buffers and does not
/// unwind across this FFI call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_summary(
    context: *mut core::ffi::c_void,
    selected: bool,
    path: bool,
    path_size: u32,
    object_count: bool,
    start_count: bool,
    step: Option<InitStep>,
) -> bool {
    let Some(step) = step else { return false };
    if !selected {
        return false;
    }
    if path && path_size != 0 && unsafe { step(context, 0, 0) } < 0 {
        return false;
    }
    if object_count && unsafe { step(context, 1, 0) } < 0 {
        return false;
    }
    if start_count && unsafe { step(context, 2, 0) } < 0 {
        return false;
    }
    true
}

/// # Safety
/// Native callback owns map/string/atomic/module state and captures exceptions
/// as -1 for the C++ caller to rethrow after Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_sol_lifecycle(
    context: *mut core::ffi::c_void,
    kind: u32,
    step: Option<InitStep>,
) -> bool {
    let Some(step) = step else { return false };
    let count = match kind {
        0 => 8,
        1 => 17,
        _ => return false,
    };
    for operation in 0..count {
        if unsafe { step(context, operation, 0) } < 0 {
            return false;
        }
    }
    true
}
