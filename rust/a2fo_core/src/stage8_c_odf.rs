//! Portable recursive ODF path, admission, and winner policy.
use core::ffi::c_void;

pub type Lower = unsafe extern "C" fn(u8) -> u8;
pub type Class = unsafe extern "C" fn(u8) -> bool;
pub type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> i64;

/// # Safety
/// `bytes` is writable for `size` bytes; callback does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_lower(bytes: *mut u8, size: usize, lower: Option<Lower>) {
    let Some(lower) = lower else { return };
    if bytes.is_null() {
        return;
    }
    for index in 0..size {
        let byte = unsafe { bytes.add(index) };
        unsafe { *byte = lower(*byte) };
    }
}

/// # Safety
/// `bytes` is writable for `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_slashes(bytes: *mut u8, size: usize, slash: u8) {
    if bytes.is_null() {
        return;
    }
    for index in 0..size {
        let byte = unsafe { bytes.add(index) };
        if unsafe { *byte } == b'/' || unsafe { *byte } == b'\\' {
            unsafe { *byte = slash };
        }
    }
}

/// # Safety
/// `bytes` is readable for `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_trim_slashes(bytes: *const u8, size: usize) -> usize {
    if bytes.is_null() {
        return size;
    }
    let mut end = size;
    while end > 3 {
        let byte = unsafe { *bytes.add(end - 1) };
        if byte != b'\\' && byte != b'/' {
            break;
        }
        end -= 1;
    }
    end
}

/// # Safety
/// Nonempty views are readable for their declared sizes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_join(
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
/// `path` is readable for `size` bytes; classifier does not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_absolute(
    path: *const u8,
    size: usize,
    classify: Option<Class>,
) -> bool {
    let Some(classify) = classify else {
        return false;
    };
    if path.is_null() || size < 3 {
        return false;
    }
    let first = unsafe { *path };
    let second = unsafe { *path.add(1) };
    let third = unsafe { *path.add(2) };
    (unsafe { classify(first) }) && second == b':' && (third == b'\\' || third == b'/')
}

/// # Safety
/// `name` is readable for `size` bytes and was already lowercased by native
/// CRT classification to preserve locale behavior.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_extension(name: *const u8, size: usize) -> bool {
    if name.is_null() || size < 4 {
        return false;
    }
    for (index, expected) in b".odf".iter().enumerate() {
        if unsafe { *name.add(size - 4 + index) } != *expected {
            return false;
        }
    }
    true
}

/// # Safety
/// Header is readable for at least `size` bytes. Zero rejects the header.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_fpq_size(header: *const u8, size: usize) -> u64 {
    if header.is_null() || size < 0x1c {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(header, 0x1c) };
    if &bytes[..4] != b"FPQ\0" {
        return 0;
    }
    let u32_at = |offset: usize| -> u64 {
        u64::from(u32::from_le_bytes([
            bytes[offset],
            bytes[offset + 1],
            bytes[offset + 2],
            bytes[offset + 3],
        ]))
    };
    let total = 0x1c + u32_at(0x10) * 12 + u32_at(0x08) * 12 + u32_at(0x0c) * 25 + u32_at(0x18);
    if !(0x1c..=128 * 1024 * 1024).contains(&total) {
        0
    } else {
        total
    }
}

/// # Safety
/// Callback reads live native fields in the original short-circuit order,
/// captures C++ exceptions as i64::MIN, and never unwinds through Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_precedes(context: *mut c_void, step: Option<Step>) -> bool {
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
    let left_mod = call!(0);
    let right_mod = call!(1);
    if left_mod != right_mod {
        if left_mod == 0 {
            return false;
        }
        if right_mod == 0 {
            return true;
        }
        let left_priority = call!(2);
        let right_priority = call!(3);
        return left_priority < right_priority;
    }
    let left_overlay = call!(4);
    let right_overlay = call!(5);
    if left_overlay != right_overlay {
        return left_overlay > right_overlay;
    }
    let left_primary = call!(6);
    let right_primary = call!(7);
    if left_primary != right_primary {
        return left_primary > right_primary;
    }
    let left_packed = call!(8);
    let right_packed = call!(9);
    if left_packed != right_packed {
        return left_packed < right_packed;
    }
    false
}

#[repr(C)]
pub struct ScanEntry {
    pub name: *const u8,
    pub size: usize,
    pub attributes: u32,
}
pub type ScanNext = unsafe extern "C" fn(*mut c_void, *mut ScanEntry) -> i32;

/// # Safety
/// Native callbacks retain the FindFirst/FindNext handle and entry strings.
/// Views live until the next callback; C++ exceptions become i64::MIN/-1 and
/// are rethrown after Rust. On an exception the original open-handle behavior
/// is preserved by returning before FindClose.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_scan(
    context: *mut c_void,
    depth: u32,
    precedence: u32,
    directory_flag: u32,
    reparse_flag: u32,
    next: Option<ScanNext>,
    step: Option<Step>,
) -> bool {
    let (Some(next), Some(step)) = (next, step) else {
        return false;
    };
    if depth > 64 {
        return true;
    }
    let found = unsafe { step(context, 0, 0) };
    if found == i64::MIN {
        return false;
    }
    if found == 0 {
        return true;
    }
    let mut contains = false;
    loop {
        let mut entry = ScanEntry {
            name: core::ptr::null(),
            size: 0,
            attributes: 0,
        };
        if unsafe { next(context, &raw mut entry) } < 0 {
            return false;
        }
        if entry.name.is_null() && entry.size != 0 {
            return false;
        }
        let dot = entry.size == 1 && unsafe { *entry.name } == b'.';
        let dotdot = entry.size == 2
            && unsafe { *entry.name } == b'.'
            && unsafe { *entry.name.add(1) } == b'.';
        if !dot && !dotdot {
            if entry.attributes & directory_flag != 0 {
                if entry.attributes & reparse_flag == 0
                    && unsafe { step(context, 3, 0) } == i64::MIN
                {
                    return false;
                }
            } else {
                let odf = unsafe { step(context, 2, 0) };
                if odf == i64::MIN {
                    return false;
                }
                if odf != 0 {
                    contains = true
                }
            }
        }
        let more = unsafe { step(context, 4, 0) };
        if more == i64::MIN {
            return false;
        }
        if more == 0 {
            break;
        }
    }
    if unsafe { step(context, 5, 0) } == i64::MIN {
        return false;
    }
    if contains {
        let previous = unsafe { step(context, 6, 0) };
        if previous == i64::MIN {
            return false;
        }
        if (previous < 0 || i64::from(precedence) > previous)
            && unsafe { step(context, 7, 0) } == i64::MIN
        {
            return false;
        }
    }
    true
}

/// # Safety
/// Callback owns SDK overlay storage and captures all C++ exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_overlays(
    context: *mut c_void,
    count: u32,
    maximum_precedence: u32,
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
    if count > 64 {
        call!(0, 0);
        return true;
    }
    call!(1, count as usize);
    for index in 0..count {
        let accepted = call!(2, index as usize) != 0;
        if !accepted || call!(3, 0) == 0 || call!(4, 0) > i64::from(maximum_precedence) {
            call!(5, index as usize);
            continue;
        }
        call!(6, 0);
    }
    true
}

/// # Safety
/// Callback retains native Delphi lists and C++ map storage, captures native
/// exceptions, and returns only scalar counts/admission decisions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_root_mods(
    context: *mut c_void,
    directories: usize,
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
    for directory in 0..directories {
        let entries = call!(0, directory) as usize;
        for entry in 0..entries {
            if call!(1, entry) == 0 {
                continue;
            }
            if call!(2, 0) == 0 {
                call!(3, 0);
            }
        }
        call!(4, 0);
    }
    true
}

/// # Safety
/// Callback owns native Delphi pointers, paths, and C++ vectors; exceptions
/// are caught before Rust and rethrown after it returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_collect_roots(
    context: *mut c_void,
    count: usize,
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
    for index in 0..count {
        call!(0, index);
        if call!(1, 0) != 0 {
            call!(2, 0);
        } else {
            call!(3, 0);
        }
        call!(4, if index % 2 == 0 { 1 } else { 0 });
    }
    for index in 0..count {
        if call!(5, index) != 0 && (index ^ 1) < count {
            call!(6, index);
        }
    }
    true
}

/// # Safety
/// Callback owns native Win32 archive checks, parser, and C++ containers.
/// All native exceptions are captured and rethrown after the Rust traversal.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_collect_archives(
    context: *mut c_void,
    count: usize,
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
    for root in 0..count {
        if call!(0, root) == 0 {
            call!(7, 0);
            continue;
        }
        if call!(1, 0) == 0 {
            call!(7, 0);
            continue;
        }
        if call!(2, 0) == 0 {
            call!(7, 0);
            continue;
        }
        if call!(3, 0) == 0 {
            call!(7, 0);
            continue;
        }
        let directory_count = call!(4, 0) as usize;
        for index in 0..directory_count {
            call!(5, index);
        }
        call!(6, 0);
        call!(7, 0);
    }
    true
}

/// # Safety
/// Callback performs checked native Delphi-array reads and owns the C++
/// registry map. Native exceptions are captured and rethrown after Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_refresh_ids(
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
    if call!(0, 0) == 0 {
        return false;
    }
    let count = call!(1, 0);
    if count > 1_000_000 || call!(2, 0) == 0 || call!(3, 0) == 0 || call!(4, 0) == 0 {
        return false;
    }
    let signed_capacity = call!(5, 0);
    if signed_capacity <= 0 || signed_capacity > 1_000_000 {
        return false;
    }
    if count > signed_capacity || (count != 0 && call!(6, 0) == 0) {
        return false;
    }
    if call!(7, 0) != 0 || call!(8, 0) != 0 {
        call!(9, 0);
    }
    let first = call!(10, 0) as usize;
    for index in first..count as usize {
        if call!(11, index) != 0 {
            call!(12, index);
        }
    }
    call!(13, 0);
    true
}

/// # Safety
/// Callback owns native project-ID fields and C++ map storage. It checks
/// native ranges before each write and captures exceptions; a failed map
/// insertion retains the original sequence of already-performed writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_ensure_id(context: *mut c_void, step: Option<Step>) -> bool {
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
    if call!(1) == 0 || call!(2) == 0 || call!(3) == 0 {
        return false;
    }
    let project_id = call!(4);
    if project_id != 0 {
        return project_id <= call!(5) && call!(6) != 0;
    }
    if call!(7) != 0 {
        call!(8);
        return true;
    }
    let count = call!(9);
    if count >= call!(10) || call!(11) == 0 || call!(12) == 0 {
        return false;
    }
    call!(13);
    call!(14);
    call!(15);
    call!(16);
    call!(17);
    true
}

/// # Safety
/// Callback owns native hash table, Delphi lists, and C++ entry storage. It
/// catches native exceptions and signals them with i64::MIN.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_hash_entries(
    context: *mut c_void,
    folders: usize,
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
    for folder in 0..folders {
        let count = call!(0, folder) as usize;
        for index in 0..count {
            if call!(1, index) == 0 {
                continue;
            }
            if call!(2, 0) == 0 {
                call!(3, 0);
                continue;
            }
            if call!(4, 0) != 0 {
                continue;
            }
            call!(5, 0);
        }
    }
    true
}

/// # Safety
/// Native callbacks retain all C++ map/group/chain storage and read live
/// fields in call order; no borrowed C++ object crosses the ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_winners(
    context: *mut c_void,
    folders: usize,
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
    for folder in 0..folders {
        let count = call!(0, folder) as usize;
        for index in 0..count {
            if call!(1, index) == 0 {
                continue;
            }
            call!(2, 0);
        }
    }
    call!(3, 0);
    let groups = call!(4, 0) as usize;
    for group in 0..groups {
        if call!(5, group) == 0 {
            call!(6, 0);
            continue;
        }
        let chain = call!(7, 0) as usize;
        let mut has_winner = false;
        for index in 0..chain {
            if call!(8, index) == 0 {
                continue;
            }
            if !has_winner || call!(9, 0) != 0 {
                call!(10, 0);
                has_winner = true;
                continue;
            }
            if call!(11, 0) == 0 && call!(12, 0) != 0 && call!(13, 0) == 0 {
                call!(10, 0);
            }
        }
        if call!(14, 0) != 0 && call!(15, 0) != 0 {
            call!(16, 0);
        }
    }
    call!(17, 0);
    true
}

/// # Safety
/// Callback owns native Win32/Delphi resources and C++ collections. Every
/// callback is exception-shielded; storage is held by C++ across this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_register(context: *mut c_void, step: Option<Step>) -> bool {
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
    if call!(0, 0) == 0 || call!(1, 0) == 0 {
        return false;
    }
    let roots = call!(2, 0) as usize;
    for index in 0..roots {
        call!(3, index);
    }
    let overlays = call!(4, 0) as usize;
    for index in 0..overlays {
        call!(5, index);
    }
    for root in 0..roots {
        call!(6, root);
        for overlay in 0..overlays {
            call!(7, overlay);
        }
    }
    let archives = call!(8, 0) as usize;
    for index in 0..archives {
        call!(9, index);
    }
    let existing = call!(10, 0) as usize;
    for index in 0..existing {
        call!(11, index);
    }
    let candidates = call!(12, 0) as usize;
    for _ in 0..candidates {
        call!(13, 0);
    }
    let available = call!(14, 0) as usize;
    if call!(15, 0) > 0 {
        call!(16, 0);
    }
    call!(17, 0);
    let remaining = call!(18, 0) as usize;
    let mut added = 0usize;
    for _ in 0..remaining {
        if added >= available {
            break;
        }
        if call!(19, 0) != 0 {
            added += 1;
        }
    }
    if call!(20, 0) == 0 {
        call!(21, 0);
        return true;
    }
    if call!(22, 0) != 0 {
        call!(23, 0);
    }
    let created = call!(24, 0) as usize;
    for index in 0..created {
        call!(25, index);
    }
    call!(26, 0);
    call!(27, 0);
    call!(28, 0);
    true
}

/// # Safety
/// C++ retains the temporary directory-list RAII owner and native arguments.
/// Exceptions are captured by the callback and rethrown after RAII teardown.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_index_scan(
    context: *mut c_void,
    roots: usize,
    archives: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for root in 0..roots {
        if unsafe { step(context, 0, root) } == i64::MIN {
            return false;
        }
    }
    for archive in 0..archives {
        if unsafe { step(context, 1, archive) } == i64::MIN {
            return false;
        }
    }
    true
}

/// # Safety
/// Callback performs the original Interlocked operations, GetCurrentThreadId,
/// Sleep, and native registration. It catches exceptions before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_register_once(
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
    let thread = call!(0);
    let mut waited = 0u32;
    loop {
        let previous = call!(1);
        if previous == 0 {
            call!(2);
            break;
        }
        if previous == 2 {
            return true;
        }
        if call!(3) == thread {
            return true;
        }
        if waited >= 30_000 {
            call!(4);
            return false;
        }
        call!(5);
        waited += 1;
    }
    let ok = call!(6) != 0;
    call!(7);
    ok
}

/// # Safety
/// Native callback owns immutable published winner table, native lock and
/// output pointer. All C++ exceptions are caught before crossing into Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_lookup(context: *mut c_void, step: Option<Step>) -> bool {
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
        call!(1);
    }
    if call!(2) == 0 {
        return false;
    }
    call!(3);
    if call!(4) != 2 {
        call!(5);
        call!(6);
    }
    if call!(4) != 2 {
        return false;
    }
    if call!(7) == 0 || call!(8) == 0 {
        return false;
    }
    call!(9);
    if call!(10) != 0 {
        call!(11);
    }
    if call!(12) != 0 {
        call!(13);
    }
    call!(14);
    true
}

#[repr(C)]
pub struct Span {
    pub first: usize,
    pub length: usize,
}

/// # Safety
/// Name is readable for size bytes; span is a writable POD output.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_trim_name(
    name: *const u8,
    size: usize,
    span: *mut Span,
) -> bool {
    if name.is_null() || span.is_null() {
        return false;
    }
    let bytes = unsafe { core::slice::from_raw_parts(name, size) };
    let Some(first) = bytes.iter().position(|b| !b" \t\r\n".contains(b)) else {
        return false;
    };
    let last = bytes
        .iter()
        .rposition(|b| !b" \t\r\n".contains(b))
        .unwrap_or(first);
    unsafe {
        *span = Span {
            first,
            length: last - first + 1,
        }
    };
    true
}

/// # Safety
/// Name is readable for size bytes and was normalized to backslashes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_basename_start(name: *const u8, size: usize) -> usize {
    if name.is_null() {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(name, size) };
    bytes.iter().rposition(|&b| b == b'\\').map_or(0, |i| i + 1)
}

/// # Safety
/// Native callback retains all name, hash, project-ID, and lock storage;
/// native exceptions are captured before crossing the FFI boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_resolve(context: *mut c_void, step: Option<Step>) -> u32 {
    let Some(step) = step else { return 0 };
    macro_rules! call {
        ($op:expr,$arg:expr) => {{
            let value = unsafe { step(context, $op, $arg) };
            if value == i64::MIN {
                return 0;
            }
            value
        }};
    }
    if call!(0, 0) == 0 || call!(1, 0) == 0 || call!(2, 0) == 0 {
        return 0;
    }
    if call!(3, 0) != 0 {
        call!(4, 0);
    }
    if call!(5, 0) == 0 && call!(6, 0) != 0 && call!(7, 0) != 0 {
        let count = call!(8, 0) as usize;
        for index in 0..count {
            if call!(9, index) == 0 {
                continue;
            }
            if call!(10, 0) == 0 || call!(11, 0) != 0 {
                call!(12, 0);
            }
            if call!(13, 0) == 0 && (call!(14, 0) == 0 || call!(15, 0) != 0) {
                call!(16, 0);
            }
        }
        call!(17, 0);
    }
    if call!(5, 0) == 0 {
        call!(18, 0);
    }
    if call!(5, 0) == 0 {
        call!(19, 0);
        return 0;
    }
    if call!(20, 0) == 0 {
        return 0;
    }
    if call!(21, 0) != 0 {
        call!(22, 0);
    }
    call!(23, 0) as u32
}

/// # Safety
/// Callback owns native module handles, validation, critical section and SDK
/// registration. C++ catches and rethrows any native exception after Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_init(context: *mut c_void, step: Option<Step>) -> bool {
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
    for op in 0..7 {
        if call!(op) == 0 {
            return false;
        }
    }
    call!(7);
    call!(8);
    call!(9);
    if call!(10) == 0 || call!(11) == 0 {
        call!(12);
        return false;
    }
    call!(13);
    if call!(14) == 0 {
        call!(15);
        return false;
    }
    call!(16);
    call!(17);
    call!(18);
    call!(19);
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_odf_list_count(count: i32) -> bool {
    count > 0 && count <= 1_000_000
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_odf_list_bounds_count(count: i32, minimum: i32, maximum: i32) -> bool {
    count >= minimum && count <= maximum
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_odf_list_capacity(capacity: i32, count: i32) -> bool {
    capacity >= count
}

/// # Safety
/// Callback owns native bucket table, visited set and output vector. Pointer
/// range checks remain native and C++ exceptions become i64::MIN.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_hash_bucket(
    context: *mut c_void,
    index: u32,
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
    let buckets = call!(1);
    let count = call!(2);
    let item_count = call!(3);
    if buckets == 0
        || buckets > 0x100000
        || u64::from(index) >= buckets as u64
        || count == 0
        || call!(4) == 0
    {
        return false;
    }
    let mut entry = call!(5);
    let mut length = 0u64;
    while entry != 0 {
        if length >= item_count as u64 || call!(6) == 0 || call!(7) == 0 {
            call!(8);
            return false;
        }
        call!(9);
        length += 1;
        entry = call!(10);
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_odf_bucket_admit(count: u32, has_buckets: bool) -> bool {
    count != 0 && count <= 0x100000 && has_buckets
}

/// # Safety
/// Callback performs checked native PE reads, signature comparisons and
/// diagnostics, preserving original short-circuit read order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_odf_validate(context: *mut c_void, step: Option<Step>) -> bool {
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
    if call!(0) == 0 {
        call!(1);
        return false;
    }
    if call!(2) == 0 {
        call!(3);
        return false;
    }
    if call!(4) == 0
        || call!(5) == 0
        || !((call!(6) == 0x51f6475c && call!(7) == 0x00322000) || call!(8) != 0)
    {
        call!(9);
        return false;
    }
    if call!(10) == 0 || call!(11) == 0 || call!(12) == 0 || call!(13) == 0 {
        call!(14);
        return false;
    }
    true
}
