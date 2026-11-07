//! Portable MissionSelector text and catalog policy. Native string and file
//! ownership stays in C++ through bounded byte views and callbacks.
use core::ffi::c_void;

#[repr(C)]
pub struct Span {
    pub begin: usize,
    pub end: usize,
}
#[repr(C)]
pub struct Trim {
    pub begin: usize,
    pub end: usize,
    pub quoted: bool,
}
pub type ByteClass = unsafe extern "C" fn(u8, u32) -> bool;
pub type Lower = unsafe extern "C" fn(u8) -> u8;
pub type Emit = unsafe extern "C" fn(*mut c_void, u8) -> bool;

/// # Safety
/// `bytes` is readable for `size` bytes; callback does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_trim(
    bytes: *const u8,
    size: usize,
    classify: Option<ByteClass>,
) -> Trim {
    let (Some(classify), false) = (classify, bytes.is_null()) else {
        return Trim {
            begin: 0,
            end: size,
            quoted: false,
        };
    };
    let mut begin = 0;
    while begin < size && unsafe { classify(*bytes.add(begin), 0) } {
        begin += 1;
    }
    let mut end = size;
    while end > begin && unsafe { classify(*bytes.add(end - 1), 0) } {
        end -= 1;
    }
    let mut quoted = false;
    if end - begin >= 2 {
        let first = unsafe { *bytes.add(begin) };
        let last = unsafe { *bytes.add(end - 1) };
        if (first == b'"' && last == b'"') || (first == b'\'' && last == b'\'') {
            quoted = true;
        }
    }
    Trim { begin, end, quoted }
}

/// # Safety
/// `bytes` is writable for `size` bytes; callback does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_lower(
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
        unsafe { *slot = lower(*slot) };
    }
}

/// # Safety
/// Both nonempty byte views are readable for their declared sizes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_join_action(
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
/// Input bytes are readable and `emit` does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_decode_escapes(
    input: *const u8,
    size: usize,
    context: *mut c_void,
    emit: Option<Emit>,
) -> bool {
    let Some(emit) = emit else { return false };
    if input.is_null() {
        return false;
    }
    let mut index = 0;
    while index < size {
        let byte = unsafe { *input.add(index) };
        if byte == b'\\' && index + 1 < size {
            let next = unsafe { *input.add(index + 1) };
            if next == b'n' {
                if !unsafe { emit(context, b'\r') && emit(context, b'\n') } {
                    return false;
                };
                index += 2;
                continue;
            }
            if next == b't' {
                if !unsafe { emit(context, b'\t') } {
                    return false;
                };
                index += 2;
                continue;
            }
        }
        if !unsafe { emit(context, byte) } {
            return false;
        };
        index += 1;
    }
    true
}

/// # Safety
/// `bytes` is readable for `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_file_stem(bytes: *const u8, size: usize) -> Span {
    if bytes.is_null() {
        return Span { begin: 0, end: 0 };
    }
    let mut begin = 0;
    for index in 0..size {
        let byte = unsafe { *bytes.add(index) };
        if byte == b'\\' || byte == b'/' {
            begin = index + 1
        }
    }
    let mut end = size;
    for index in begin..size {
        if unsafe { *bytes.add(index) } == b'.' {
            end = index
        }
    }
    Span { begin, end }
}

/// # Safety
/// Both byte views and output pointers are valid; callback does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_indexed_key(
    value: *const u8,
    size: usize,
    prefix: *const u8,
    prefix_size: usize,
    maximum: u32,
    classify: Option<ByteClass>,
    index: *mut i32,
    end: *mut usize,
) -> bool {
    let Some(classify) = classify else {
        return false;
    };
    if value.is_null() || prefix.is_null() || index.is_null() || end.is_null() || prefix_size > size
    {
        return false;
    }
    for offset in 0..prefix_size {
        if unsafe { *value.add(offset) != *prefix.add(offset) } {
            return false;
        }
    }
    let mut cursor = prefix_size;
    if cursor >= size || !unsafe { classify(*value.add(cursor), 1) } {
        return false;
    }
    let mut parsed = 0u32;
    while cursor < size && unsafe { classify(*value.add(cursor), 1) } {
        let digit = (i32::from(unsafe { *value.add(cursor) } as i8) - i32::from(b'0')) as u32;
        parsed = parsed.wrapping_mul(10).wrapping_add(digit);
        if parsed > maximum {
            return false;
        }
        cursor += 1;
    }
    unsafe {
        *index = parsed as i32;
        *end = cursor
    };
    true
}

/// # Safety
/// Token is readable for `size` bytes. Returns -1 for fallback, 0 false, 1 true.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_bool_token(token: *const u8, size: usize) -> i32 {
    if token.is_null() {
        return -1;
    }
    let bytes = unsafe { core::slice::from_raw_parts(token, size) };
    match bytes {
        b"1" | b"true" | b"yes" | b"on" => 1,
        b"0" | b"false" | b"no" | b"off" => 0,
        _ => -1,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_mission_integer_admit(value: i64, minimum: i32, maximum: i32) -> bool {
    value >= i64::from(minimum) && value <= i64::from(maximum)
}

/// # Safety
/// `value` is writable for `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_label_basename(value: *mut u8, size: usize) -> usize {
    if value.is_null() {
        return 0;
    }
    let mut begin = 0;
    for index in 0..size {
        let slot = unsafe { value.add(index) };
        let byte = unsafe { *slot };
        if byte == b'/' {
            unsafe { *slot = b'\\' };
            begin = index + 1
        } else if byte == b'\\' {
            begin = index + 1
        }
    }
    begin
}

#[repr(C)]
pub struct Stock {
    pub kind: u32,
    pub begin: usize,
}
/// # Safety
/// Both stem views are readable for `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_humanize_stock(
    value: *const u8,
    lower: *const u8,
    size: usize,
) -> Stock {
    if value.is_null() || lower.is_null() {
        return Stock { kind: 0, begin: 0 };
    }
    let source = unsafe { core::slice::from_raw_parts(value, size) };
    let lowered = unsafe { core::slice::from_raw_parts(lower, size) };
    let prefixes: [&[u8]; 4] = [b"a2_tutorial", b"a2_fed", b"a2_kling", b"a2_borg"];
    for (index, prefix) in prefixes.into_iter().enumerate() {
        if lowered.starts_with(prefix) {
            let mut begin = prefix.len();
            while size.saturating_sub(begin) > 1 && source[begin] == b'0' {
                begin += 1
            }
            return Stock {
                kind: index as u32 + 1,
                begin,
            };
        }
    }
    Stock { kind: 0, begin: 0 }
}

/// # Safety
/// Stem byte views are readable, callbacks do not unwind, and `emit` owns the
/// caller's output storage. `classify` uses 0=lower, 1=upper, 2=digit.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_humanize(
    value: *const u8,
    size: usize,
    classify: Option<ByteClass>,
    context: *mut c_void,
    emit: Option<Emit>,
) -> bool {
    let (Some(classify), Some(emit)) = (classify, emit) else {
        return false;
    };
    if value.is_null() {
        return false;
    }
    let source = unsafe { core::slice::from_raw_parts(value, size) };
    let mut last = None;
    for index in 0..size {
        let current = source[index];
        if current == b'_' || current == b'-' {
            if last.is_some() && last != Some(b' ') {
                if !unsafe { emit(context, b' ') } {
                    return false;
                }
                last = Some(b' ');
            }
            continue;
        }
        if index != 0 {
            let previous = source[index - 1];
            let boundary = (unsafe { classify(previous, 0) } && unsafe { classify(current, 1) })
                || (!unsafe { classify(previous, 2) } && unsafe { classify(current, 2) });
            if boundary && last.is_some() && last != Some(b' ') && !unsafe { emit(context, b' ') } {
                return false;
            }
        }
        if !unsafe { emit(context, current) } {
            return false;
        }
        last = Some(current);
    }
    true
}

pub type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> i64;
/// # Safety
/// Callback retains native selector, game and SDK lifetimes. Native exceptions
/// are captured as `i64::MIN` for C++ rethrow after this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_start(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    macro_rules! call {
        ($op:expr,$arg:expr) => {{
            let result = unsafe { step(context, $op, $arg) };
            if result == i64::MIN {
                return;
            }
            result
        }};
    }
    let campaign_present = call!(0, 0) != 0;
    let mission_present = call!(1, 0) != 0;
    if !campaign_present || !mission_present || call!(2, 0) == 0 || call!(3, 0) == 0 {
        call!(4, 0);
        return;
    }
    if call!(5, 0) == 0 {
        call!(6, 0);
        return;
    }
    let campaign = call!(7, 0);
    if !(0..4).contains(&campaign) {
        call!(9, 0);
        return;
    }
    let mission = call!(8, 0);
    if !(0..10).contains(&mission) {
        call!(9, 0);
        return;
    }
    if call!(10, 0) == 0 {
        call!(11, 0);
        return;
    }
    // The native code rereads both fields after the filename-table check.
    let row = (call!(7, 0) as usize)
        .wrapping_mul(10)
        .wrapping_add(call!(8, 0) as usize);
    call!(12, row);
    if call!(13, 0) != 0 && call!(14, row) == 0 {
        call!(15, 0);
        return;
    }
    call!(16, 0);
    call!(17, 0);
    call!(18, 0);
    call!(19, 0);
    let launched = call!(20, 0) != 0;
    call!(21, 0);
    if !launched {
        call!(22, 0);
        call!(23, 0);
        call!(24, 0);
    }
}

/// # Safety
/// Callback retains native SDK, Win32 and GDI resources. Native exceptions
/// are captured as `i64::MIN` and rethrown by the C++ caller after Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_init(context: *mut c_void, step: Option<Step>) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr) => {{
            let result = unsafe { step(context, $op, 0) };
            if result == i64::MIN {
                return false;
            }
            result
        }};
    }
    if call!(0) == 0 {
        call!(5);
        return false;
    }
    for operation in 1..=4 {
        let result = unsafe { step(context, operation, 0) };
        if result == i64::MIN {
            return false;
        }
        if result == 0 {
            call!(5);
            return false;
        }
    }
    call!(6);
    call!(7);
    call!(8);
    if call!(9) == 0 {
        call!(10);
        return false;
    }
    if call!(11) == 0 {
        call!(12);
        return true;
    }
    call!(13);
    true
}

/// # Safety
/// Callback owns native paths, root vector and Win32 file checks. It captures
/// exceptions as `i64::MIN` for C++ rethrow after Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_resolve_asset(
    context: *mut c_void,
    roots: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr,$index:expr) => {{
            let value = unsafe { step(context, $op, $index) };
            if value == i64::MIN {
                return false;
            }
            value
        }};
    }
    if call!(0, 0) == 0 {
        return false;
    }
    if call!(1, 0) != 0 {
        return true;
    }
    for root in (0..roots).rev() {
        call!(2, root);
        for kind in 0..3 {
            if call!(3 + kind, root) != 0 {
                call!(6, root);
                return true;
            }
        }
        call!(6, root);
    }
    false
}

/// # Safety
/// Callback owns native strings and recursive asset lookup and captures
/// exceptions as `i64::MIN` for C++ rethrow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_resolve_thumbnail(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr,$index:expr) => {{
            let value = unsafe { step(context, $op, $index) };
            if value == i64::MIN {
                return false;
            }
            value
        }};
    }
    if call!(0, 0) != 0 && call!(1, 0) != 0 {
        return true;
    }
    call!(2, 0);
    for index in 0..4 {
        if call!(3, index) != 0 {
            return true;
        }
    }
    false
}

#[repr(C)]
pub struct Line {
    pub data: *const u8,
    pub size: usize,
}
pub type Next = unsafe extern "C" fn(*mut c_void, *mut Line) -> i32;

/// # Safety
/// `next` returns a byte view valid until the next callback. Native callbacks
/// capture exceptions, return -1 or i64::MIN, and never unwind through Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_parse(
    context: *mut c_void,
    mode: u32,
    next: Option<Next>,
    step: Option<Step>,
) -> bool {
    let (Some(next), Some(step)) = (next, step) else {
        return false;
    };
    let mut active = false;
    loop {
        let mut line = Line {
            data: core::ptr::null(),
            size: 0,
        };
        let status = unsafe { next(context, &raw mut line) };
        if status < 0 {
            return false;
        }
        if status == 0 {
            return true;
        }
        if line.size == 0 {
            continue;
        }
        if line.data.is_null() {
            return false;
        }
        let bytes = unsafe { core::slice::from_raw_parts(line.data, line.size) };
        if bytes.is_empty()
            || bytes[0] == b';'
            || bytes[0] == b'#'
            || (mode != 0 && bytes.starts_with(b"//"))
        {
            continue;
        }
        if mode == 0 {
            if bytes.len() >= 2 && bytes[0] == b'[' && bytes[bytes.len() - 1] == b']' {
                if unsafe { step(context, 0, 0) } == i64::MIN {
                    return false;
                }
                continue;
            }
            if let Some(equals) = bytes.iter().position(|byte| *byte == b'=')
                && unsafe { step(context, 1, equals) } == i64::MIN
            {
                return false;
            }
            continue;
        }
        if !active {
            let matches = unsafe { step(context, 0, 0) };
            if matches == i64::MIN {
                return false;
            }
            if matches != 0 {
                if bytes.contains(&b'{') {
                    active = true
                } else {
                    loop {
                        let status = unsafe { next(context, &raw mut line) };
                        if status < 0 {
                            return false;
                        }
                        if status == 0 {
                            return true;
                        }
                        if line.size == 0 {
                            continue;
                        }
                        if line.data.is_null() {
                            return false;
                        }
                        let brace = unsafe { core::slice::from_raw_parts(line.data, line.size) };
                        if brace.is_empty()
                            || brace[0] == b';'
                            || brace[0] == b'#'
                            || brace.starts_with(b"//")
                        {
                            continue;
                        }
                        active = brace[0] == b'{';
                        break;
                    }
                }
            }
            continue;
        }
        if bytes[0] == b'}' {
            return true;
        }
        if bytes[0] != b'+' {
            continue;
        }
        let Some(open) = bytes.iter().position(|byte| *byte == b'{') else {
            continue;
        };
        let Some(close) = bytes.iter().rposition(|byte| *byte == b'}') else {
            continue;
        };
        if close <= open {
            continue;
        }
        if unsafe { step(context, 1, open) } == i64::MIN {
            return false;
        }
    }
}

/// # Safety
/// Prefix and every key view remain readable through their synchronous probe.
/// Callbacks own map/set storage, capture native exceptions, and never unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_collect_indices(
    context: *mut c_void,
    prefix: *const u8,
    prefix_size: usize,
    maximum: u32,
    classify: Option<ByteClass>,
    next: Option<Next>,
    step: Option<Step>,
) -> bool {
    let (Some(classify), Some(next), Some(step)) = (classify, next, step) else {
        return false;
    };
    loop {
        let mut view = Line {
            data: core::ptr::null(),
            size: 0,
        };
        let status = unsafe { next(context, &raw mut view) };
        if status < 0 {
            return false;
        }
        if status == 0 {
            return true;
        }
        if view.data.is_null() && view.size != 0 {
            return false;
        }
        let mut index = 0;
        let mut end = 0;
        if unsafe {
            a2fo_rs_c_mission_indexed_key(
                view.data,
                view.size,
                prefix,
                prefix_size,
                maximum,
                Some(classify),
                &raw mut index,
                &raw mut end,
            )
        } && end < view.size
            && unsafe { *view.data.add(end) } == b'.'
            && unsafe { step(context, 0, index as usize) } == i64::MIN
        {
            return false;
        }
        if unsafe { step(context, 1, 0) } == i64::MIN {
            return false;
        }
    }
}

#[repr(C)]
pub struct CatalogPlan {
    pub configured_count: usize,
    pub configured_max: i32,
    pub native_campaigns: u32,
    pub native_missions: u32,
    pub maximum_filename: usize,
    pub request_ini: bool,
    pub filenames_valid: bool,
}

/// # Safety
/// The callback owns every C++ container and native game read, captures native
/// exceptions as i64::MIN, and remains valid for this synchronous traversal.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_catalog(
    context: *mut c_void,
    plan: CatalogPlan,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr,$arg:expr) => {{
            let value = unsafe { step(context, $op, $arg) };
            if value == i64::MIN {
                return false;
            }
            value
        }};
    }
    if !plan.filenames_valid {
        return true;
    }
    let mut ini = plan.request_ini;
    if ini {
        if plan.configured_count == 0 {
            call!(0, 0);
            ini = false
        } else {
            call!(0, 1);
        }
    }
    let highest = if ini {
        plan.configured_max
    } else if plan.configured_count == 0 {
        plan.native_campaigns as i32 - 1
    } else {
        (plan.native_campaigns as i32 - 1).max(plan.configured_max)
    };
    call!(1, (highest + 1) as usize);
    for campaign in 0..=highest {
        let native = campaign < plan.native_campaigns as i32;
        call!(2, campaign as usize);
        call!(3, 0);
        if native && !ini {
            for mission in 0..plan.native_missions {
                call!(4, mission as usize);
            }
        }
        let count = call!(12, 0) as usize;
        for position in 0..count {
            let mission = call!(5, position);
            if mission < 0 {
                continue;
            }
            let length = call!(6, mission as usize) as usize;
            if length == 0 {
                continue;
            }
            if length >= plan.maximum_filename {
                call!(7, 0);
                continue;
            }
            call!(8, 0);
        }
        if (!ini && native) || call!(10, 0) != 0 || call!(9, 0) != 0 {
            call!(11, 0);
        }
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_mission_index_valid(index: i32, size: i32) -> bool {
    index >= 0 && index < size
}

/// # Safety
/// Callback holds the native selector and its reference lifetimes; it cannot
/// unwind. The three probes retain the original read sequence.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_menu_view(
    context: *mut c_void,
    step: Option<Step>,
) -> u32 {
    let Some(step) = step else { return 0 };
    let mission = unsafe { step(context, 0, 0) } != 0;
    if mission && unsafe { step(context, 1, 0) } != 0 {
        return 1;
    }
    if unsafe { step(context, 2, 0) } != 0 {
        2
    } else {
        0
    }
}

/// # Safety
/// Callback owns the native root iterator and captures all exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_roots(context: *mut c_void, step: Option<Step>) -> bool {
    let Some(step) = step else { return false };
    loop {
        let more = unsafe { step(context, 0, 0) };
        if more == i64::MIN {
            return false;
        }
        if more == 0 {
            return true;
        }
        for operation in 1..=3 {
            if unsafe { step(context, operation, 0) } == i64::MIN {
                return false;
            }
        }
    }
}

/// # Safety
/// Callback owns native catalog, shell and dialog lifetime; all exceptions
/// are converted to i64::MIN and rethrown by C++ after this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_show(context: *mut c_void, step: Option<Step>) -> i64 {
    let Some(step) = step else { return i64::MIN };
    let count = unsafe { step(context, 0, 0) };
    if count == i64::MIN {
        return count;
    }
    let mut has_missions = false;
    for index in 0..count as usize {
        let present = unsafe { step(context, 1, index) };
        if present == i64::MIN {
            return present;
        }
        if present != 0 {
            has_missions = true;
            break;
        }
    }
    if !has_missions {
        if unsafe { step(context, 2, 0) } == i64::MIN {
            return i64::MIN;
        }
        return -1;
    }
    let owner = unsafe { step(context, 3, 0) };
    if owner == i64::MIN {
        return owner;
    }
    if owner == 0 {
        if unsafe { step(context, 4, 0) } == i64::MIN {
            return i64::MIN;
        }
        return -1;
    }
    unsafe { step(context, 5, 0) }
}

/// # Safety
/// Callback owns the native hook gateway. Its show-dialog operation reports
/// caught C++ exceptions as i64::MIN; no exception crosses Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_single_hook(
    context: *mut c_void,
    step: Option<Step>,
) -> i32 {
    let Some(step) = step else { return 0 };
    let result = unsafe { step(context, 0, 0) };
    if result == i64::MIN {
        unsafe { step(context, 2, 0) };
    } else if result != -1 {
        if unsafe { step(context, 1, result as usize) } != i64::MIN {
            return result as i32;
        }
        unsafe { step(context, 2, 0) };
    }
    (unsafe { step(context, 3, 0) }) as i32
}

/// # Safety
/// Callback performs the native atomic and gateway call without unwinding.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_accept_hook(
    context: *mut c_void,
    step: Option<Step>,
) -> i32 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 1 {
        1
    } else {
        (unsafe { step(context, 1, 0) }) as i32
    }
}

/// # Safety
/// Callback owns the native list and campaign storage and captures C++
/// exceptions. List message callbacks may mutate storage, so sizes are read
/// again at the same points as the source loop.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_populate_missions(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr,$arg:expr) => {{
            let value = unsafe { step(context, $op, $arg) };
            if value == i64::MIN {
                return false;
            }
            value
        }};
    }
    call!(0, 0);
    if call!(1, 0) == 0 {
        call!(2, 0);
        return true;
    }
    call!(3, 0);
    let mut index = 0;
    while index < (call!(4, 0) as usize) {
        call!(5, index);
        index += 1;
    }
    call!(6, 0);
    let count = call!(7, 0) as usize;
    if count != 0 {
        for index in 0..count {
            if call!(8, index) != 0 {
                call!(9, index);
                break;
            }
        }
        call!(10, 0);
    }
    call!(11, 0);
    true
}

/// # Safety
/// Callback owns the native campaign list and catches exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_populate_campaigns(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let mut index = 0;
    loop {
        let size = unsafe { step(context, 0, 0) };
        if size == i64::MIN {
            return false;
        }
        if index >= size as usize {
            break;
        }
        if unsafe { step(context, 1, index) } == i64::MIN {
            return false;
        }
        index += 1;
    }
    if unsafe { step(context, 2, 0) } == i64::MIN {
        return false;
    }
    (unsafe { step(context, 3, 0) }) != i64::MIN
}

/// # Safety
/// Callback retains selector and native window lifetimes, captures C++
/// exceptions, and performs each Win32/GDI operation at the indicated step.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_details(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == i64::MIN {
                return false;
            }
            value
        }};
    }
    let campaign = call!(0) != 0;
    let mission = call!(1) != 0;
    if !campaign || !mission {
        call!(2);
        return true;
    }
    call!(3);
    if call!(4) != 0 {
        call!(5);
    }
    if call!(6) == 0 {
        call!(7);
    }
    call!(8);
    call!(9);
    call!(10);
    true
}

/// # Safety
/// Callback owns SDK root access and vector storage, catches native
/// exceptions, and does not unwind through Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_cache_roots(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == i64::MIN {
        return false;
    }
    let valid = unsafe { step(context, 1, 0) };
    if valid == i64::MIN {
        return false;
    }
    if valid == 0 {
        return true;
    }
    let count = unsafe { step(context, 2, 0) };
    if count == i64::MIN {
        return false;
    }
    for index in 0..count as usize {
        if unsafe { step(context, 3, index) } == i64::MIN {
            return false;
        }
    }
    true
}

/// # Safety
/// Callback probes each game byte before reading it, retains the native
/// pointer and string storage, and reports exceptions as i64::MIN.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_safe_string(
    context: *mut c_void,
    maximum: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == i64::MIN {
        return false;
    }
    for index in 0..maximum {
        let readable = unsafe { step(context, 1, index) };
        if readable == i64::MIN || readable == 0 {
            return false;
        }
        let byte = unsafe { step(context, 2, index) };
        if byte == i64::MIN {
            return false;
        }
        if byte == 0 {
            return true;
        }
        if byte < 0x20 && byte != i64::from(b'\t') {
            return false;
        }
        if unsafe { step(context, 3, index) } == i64::MIN {
            return false;
        }
    }
    false
}

/// # Safety
/// Callback performs checked native progression reads and does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_unlocked(
    context: *mut c_void,
    campaign: i32,
    mission: i32,
    limit: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if mission < 0 || mission >= limit {
        return false;
    }
    if campaign == 0 {
        if unsafe { step(context, 0, 0) } == 0 {
            return mission == 0;
        }
        let count = unsafe { step(context, 1, 0) };
        return count > 0 && mission < count as i32;
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return mission == 0;
    }
    let maximum = unsafe { step(context, 3, 0) };
    maximum >= 0 && i64::from(mission) <= maximum
}

/// # Safety
/// Callback owns the native mission table and saved pointer. It performs
/// checked reads/writes and does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_restore(
    context: *mut c_void,
    table_size: usize,
    step: Option<Step>,
) {
    let Some(step) = step else { return };
    let row = unsafe { step(context, 0, 0) } as usize;
    if row >= table_size {
        return;
    }
    if unsafe { step(context, 1, 0) } != 0 && unsafe { step(context, 2, row) } != 0 {
        unsafe { step(context, 3, row) };
    }
    unsafe { step(context, 4, 0) };
}

/// # Safety
/// Callback retains the native filename/table storage and performs checked
/// writes. It does not unwind through Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_retain(
    context: *mut c_void,
    length: usize,
    row: usize,
    table_size: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    unsafe { step(context, 0, 0) };
    let capacity = unsafe { step(context, 1, 0) } as usize;
    if length == 0 || length >= capacity || row >= table_size {
        return false;
    }
    if unsafe { step(context, 2, 0) } == 0 {
        return false;
    }
    unsafe { step(context, 3, row) };
    true
}

/// # Safety
/// Callback owns the native image and path objects and catches C++ exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_background(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == i64::MIN {
                return false;
            }
            value
        }};
    }
    if call!(0) != 0 {
        return true;
    }
    call!(1);
    if call!(2) == 0 {
        call!(6);
        return true;
    }
    if call!(3) == 0 {
        call!(4);
        call!(6);
        return true;
    }
    call!(5);
    call!(6);
    true
}

/// # Safety
/// Callback owns the native image and path objects and catches C++ exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_thumbnail(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    macro_rules! call {
        ($op:expr) => {{
            let value = unsafe { step(context, $op, 0) };
            if value == i64::MIN {
                return false;
            }
            value
        }};
    }
    call!(0);
    if call!(1) == 0 {
        call!(7);
        return true;
    }
    if call!(2) != 0 {
        call!(3);
        call!(7);
        return true;
    }
    if call!(4) == 0 {
        call!(7);
        return true;
    }
    if call!(5) == 0 {
        call!(6);
        call!(7);
        return true;
    }
    call!(8);
    call!(7);
    true
}

/// # Safety
/// Callback owns native GDI resources and does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_drawing_init(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    for pair in 0..4 {
        let check = pair * 2;
        if unsafe { step(context, check, 0) } == 0 {
            unsafe { step(context, check + 1, 0) };
        }
    }
    if unsafe { step(context, 8, 0) } == 0 && unsafe { step(context, 9, 0) } == 0 {
        unsafe { step(context, 10, 0) };
    }
}

/// # Safety
/// Callback owns native hook, GDI and root lifetimes and does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_shutdown(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } != 0 {
        unsafe { step(context, 2, 0) };
    }
    for check in [3, 5, 7, 9] {
        if unsafe { step(context, check, 0) } != 0 {
            unsafe { step(context, check + 1, 0) };
        }
    }
    unsafe { step(context, 11, 0) };
    unsafe { step(context, 12, 0) };
    unsafe { step(context, 13, 0) };
}

#[repr(C)]
pub struct Command {
    pub identifier: i32,
    pub notification: i32,
    pub campaign_list: i32,
    pub mission_list: i32,
    pub start: i32,
    pub back: i32,
    pub cancel: i32,
    pub selection_changed: i32,
    pub double_click: i32,
    pub clicked: i32,
    pub list_error: i32,
}

/// # Safety
/// Callback owns native list, dialog and selector lifetimes. It captures C++
/// exceptions as i64::MIN for rethrow by the caller after Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_command(
    context: *mut c_void,
    codes: Command,
    step: Option<Step>,
) -> i32 {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr,$arg:expr) => {{
            let value = unsafe { step(context, $op, $arg) };
            if value == i64::MIN {
                return -1;
            }
            value
        }};
    }
    if codes.identifier == codes.campaign_list && codes.notification == codes.selection_changed {
        let row = call!(0, 0);
        if row != i64::from(codes.list_error) {
            let data = call!(1, row as usize);
            if data >= 0 && data < call!(2, 0) {
                call!(3, data as usize);
            }
        }
        return 1;
    }
    if codes.identifier == codes.mission_list
        && (codes.notification == codes.selection_changed
            || codes.notification == codes.double_click)
    {
        let row = call!(4, 0);
        if row != i64::from(codes.list_error) {
            let data = call!(5, row as usize);
            call!(6, data as usize);
            if codes.notification == codes.double_click {
                call!(7, 0);
            }
        }
        return 1;
    }
    if codes.identifier == codes.start && codes.notification == codes.clicked {
        call!(7, 0);
        return 1;
    }
    if codes.identifier == codes.back && codes.notification == codes.clicked {
        call!(8, 0);
        return 1;
    }
    if codes.identifier == codes.cancel {
        call!(9, 0);
        return 1;
    }
    0
}

/// # Safety
/// Callback performs native shell state and dialog operations without
/// unwinding. The caller's source string remains owned by C++.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_exit(context: *mut c_void, step: Option<Step>) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) };
    } else {
        unsafe { step(context, 2, 0) };
    }
    if unsafe { step(context, 3, 0) } == 0 {
        unsafe { step(context, 4, 0) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_mission_copy_length(source: usize, capacity: usize) -> usize {
    source.min(capacity.saturating_sub(1))
}

#[repr(C)]
pub struct ShellRect {
    pub origin_x: i32,
    pub origin_y: i32,
    pub client_left: i32,
    pub client_top: i32,
    pub client_right: i32,
    pub client_bottom: i32,
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    pub filled: u32,
}

/// # Safety
/// The POD output is writable, and the callback owns native Win32 handles
/// and fills the input fields only after both checked Win32 operations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_shell_rect(
    context: *mut c_void,
    geometry: *mut ShellRect,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if geometry.is_null() {
        return false;
    }
    if unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return false;
    }
    unsafe { step(context, 2, 0) };
    let rect = unsafe { &mut *geometry };
    rect.left = rect.origin_x;
    rect.top = rect.origin_y;
    rect.right = rect
        .origin_x
        .wrapping_add(rect.client_right.wrapping_sub(rect.client_left));
    rect.bottom = rect
        .origin_y
        .wrapping_add(rect.client_bottom.wrapping_sub(rect.client_top));
    rect.filled = 1;
    rect.right > rect.left && rect.bottom > rect.top
}

#[repr(C)]
pub struct Layout {
    pub width: i32,
    pub height: i32,
    pub image_height: i32,
    pub image_width: i32,
    pub has_height_image: bool,
    pub has_width_image: bool,
    pub rectangles: [[i32; 4]; 7],
}

/// # Safety
/// `layout` points to one writable POD structure. Image dimensions are
/// supplied by native GDI+ reads before this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_mission_layout(layout: *mut Layout) {
    if layout.is_null() {
        return;
    }
    let l = unsafe { &mut *layout };
    let margin = 18i32;
    let gap = 12i32;
    let header = 54i32;
    let button_height = if l.has_height_image {
        l.image_height.clamp(16, 64)
    } else {
        30
    };
    let button_width = if l.has_width_image {
        l.image_width.clamp(80, 320)
    } else {
        150
    };
    let footer_y = l.height.wrapping_sub(margin).wrapping_sub(button_height);
    let content_top = margin + header;
    let content_height = footer_y.wrapping_sub(gap).wrapping_sub(content_top);
    let campaign_width = (l.width.wrapping_mul(22) / 100).max(150);
    let mission_width = (l.width.wrapping_mul(29) / 100).max(190);
    let details_x = margin
        .wrapping_add(campaign_width)
        .wrapping_add(gap)
        .wrapping_add(mission_width)
        .wrapping_add(gap);
    let details_width = l
        .width
        .wrapping_sub(details_x)
        .wrapping_sub(margin)
        .max(180);
    let preview_height = (content_height.wrapping_mul(46) / 100).max(130);
    l.rectangles[0] = [margin, content_top, campaign_width, content_height];
    l.rectangles[1] = [
        margin.wrapping_add(campaign_width).wrapping_add(gap),
        content_top,
        mission_width,
        content_height,
    ];
    l.rectangles[2] = [details_x, content_top, details_width, preview_height];
    l.rectangles[3] = [
        details_x,
        content_top.wrapping_add(preview_height).wrapping_add(gap),
        details_width,
        content_height
            .wrapping_sub(preview_height)
            .wrapping_sub(gap),
    ];
    l.rectangles[4] = [margin, margin, l.width.wrapping_sub(margin * 2), header - 8];
    l.rectangles[5] = [
        l.width.wrapping_sub(margin).wrapping_sub(button_width),
        footer_y,
        button_width,
        button_height,
    ];
    l.rectangles[6] = [
        l.width
            .wrapping_sub(margin)
            .wrapping_sub(button_width.wrapping_mul(2))
            .wrapping_sub(gap),
        footer_y,
        button_width,
        button_height,
    ];
}
