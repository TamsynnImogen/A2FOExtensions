//! Portable texture request and manual mip policies for the RGB fallback.

use core::ffi::c_void;

type Emit = unsafe extern "C" fn(*mut c_void, u32, *const u8, usize, usize) -> bool;
type Allocate = unsafe extern "C" fn(*mut c_void, usize) -> *mut u8;
type Lower = unsafe extern "C" fn(u8) -> u8;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u32;

#[repr(C)]
pub struct RgbDiscovery {
    roots_found: u32,
    rgb_roots: u32,
    index8_roots: u32,
    compressed_roots: u32,
}

/// Safety: the key and returned span refer to native string storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_dds_alias(
    key: *const u8,
    length: usize,
    basename: *mut RgbSpan,
) -> bool {
    if key.is_null() || basename.is_null() {
        return false;
    }
    let bytes = unsafe { core::slice::from_raw_parts(key, length) };
    let Some(tail) = bytes.strip_prefix(b"dds\\") else {
        return false;
    };
    if tail.is_empty() || tail.contains(&b'\\') {
        return false;
    }
    unsafe {
        (*basename).start = 4;
        (*basename).length = tail.len();
    }
    true
}

/// Safety: callback keeps native root/FindFirstFile cursors and map storage.
/// Rust controls root and folder order and candidate publication precedence.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_discover(
    step: Option<Step>,
    context: *mut c_void,
    discovery: *mut RgbDiscovery,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    if discovery.is_null() {
        return false;
    }
    let Some(count) = (unsafe { step_call(step, context, 0, 0) }) else {
        return false;
    };
    if count > 4096 {
        let _ = unsafe { step_call(step, context, 8, 0) };
        return false;
    }
    for root in 0..count as usize {
        match unsafe { step_call(step, context, 1, root) } {
            Some(0) => continue,
            Some(_) => {}
            None => return false,
        }
        if unsafe { step_call(step, context, 2, 0) }.is_none() {
            return false;
        }
        if unsafe { step_call(step, context, 3, 0) }.is_none() {
            return false;
        }
        let mut has_legacy = false;
        for route in 0..3usize {
            // Compressed, Index8, RGB.
            match unsafe { step_call(step, context, 4, route) } {
                Some(0) => continue,
                Some(_) => {
                    has_legacy = true;
                    unsafe {
                        match route {
                            0 => (*discovery).compressed_roots += 1,
                            1 => (*discovery).index8_roots += 1,
                            _ => (*discovery).rgb_roots += 1,
                        }
                    }
                    if unsafe { step_call(step, context, 5, route) }.is_none() {
                        return false;
                    }
                }
                None => return false,
            }
        }
        for route in 0..4usize {
            // Same-root tie order: root last.
            if unsafe { step_call(step, context, 6, route) }.is_none() {
                return false;
            }
        }
        if unsafe { step_call(step, context, 7, 0) }.is_none() {
            return false;
        }
        if has_legacy {
            unsafe {
                (*discovery).roots_found += 1;
            }
        }
    }
    unsafe { (*discovery).roots_found != 0 }
}

/// Safety: callback owns candidate map cursors and all file/cache mutations.
/// Each callback completes before Rust reads the next native candidate.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_select_root(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    let Some(base_count) = (unsafe { step_call(step, context, 0, 0) }) else {
        return false;
    };
    for index in (0..base_count as usize).rev() {
        match unsafe { step_call(step, context, 1, index) } {
            Some(0) => {}
            Some(_) => break,
            None => return false,
        }
    }
    let Some(count) = (unsafe { step_call(step, context, 2, 0) }) else {
        return false;
    };
    for index in (0..count as usize).rev() {
        match unsafe { step_call(step, context, 3, index) } {
            Some(0) => {}
            Some(_) => match unsafe { step_call(step, context, 4, index) } {
                Some(0) => match unsafe { step_call(step, context, 5, index) } {
                    Some(0) => continue,
                    Some(_) => return true,
                    None => return false,
                },
                Some(_) => {}
                None => return false,
            },
            None => return false,
        }
        match unsafe { step_call(step, context, 6, index) } {
            Some(0) => {}
            Some(_) => return true,
            None => return false,
        }
    }
    false
}

/// Safety: callback owns native maps, path strings and TGA preparation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_mapped_route(
    request: u32,
    key_is_tga: bool,
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    if request == 4 {
        return unsafe { step_call(step, context, 0, 0) } == Some(1);
    }
    if !(1..=3).contains(&request) {
        return false;
    }
    match unsafe { step_call(step, context, 1, 0) } {
        Some(0) if request == 1 && key_is_tga => {
            (unsafe { step_call(step, context, 2, 0) }) == Some(1)
        }
        Some(0) | None => false,
        Some(_) if request == 1 && key_is_tga => {
            (unsafe { step_call(step, context, 3, 0) }) == Some(1)
        }
        Some(_) => (unsafe { step_call(step, context, 4, 0) }) == Some(1),
    }
}

/// Safety: callback owns CRT FILE pointers, map strings, logging and any
/// native exceptions. True means call original fopen after Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_fopen_route(
    request: u32,
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return true;
    };
    match request {
        5 => !matches!(unsafe { step_call(step, context, 0, 0) }, Some(1)),
        4 => match unsafe { step_call(step, context, 1, 0) } {
            Some(1) => false,
            Some(_) => {
                let _ = unsafe { step_call(step, context, 2, 0) };
                true
            }
            None => true,
        },
        1..=3 => {
            if unsafe { step_call(step, context, 3, 0) } != Some(1) {
                return true;
            }
            if unsafe { step_call(step, context, 4, 0) } != Some(1) {
                return true;
            }
            unsafe { step_call(step, context, 6, 0) }.is_none()
        }
        _ => true,
    }
}

unsafe fn scan_directory(step: Step, context: *mut c_void, depth: usize) -> bool {
    if depth > 32 {
        return true;
    }
    match unsafe { step_call(step, context, 0, depth) } {
        Some(0) => return true,
        Some(_) => {}
        None => return false,
    }
    loop {
        match unsafe { step_call(step, context, 1, depth) } {
            Some(0) => {}
            Some(1) => {
                if unsafe { step_call(step, context, 2, depth) }.is_none()
                    || !unsafe { scan_directory(step, context, depth + 1) }
                {
                    return false;
                }
            }
            Some(2) => {
                if unsafe { step_call(step, context, 5, depth) }.is_none() {
                    return false;
                }
            }
            None => return false,
            _ => return false,
        }
        match unsafe { step_call(step, context, 3, depth) } {
            Some(0) => break,
            Some(_) => {}
            None => return false,
        }
    }
    unsafe { step_call(step, context, 4, depth) }.is_some()
}

/// Safety: native callback owns each FindFirst/FindNext/FindClose cursor and
/// filesystem output map. Rust reproduces the original depth-first order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_scan_directory(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    unsafe { scan_directory(step, context, 0) }
}

/// Safety: callback owns the native FindFirst/FindNext/FindClose cursor and
/// captures C++ exceptions until this function returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_find_child(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    match unsafe { step_call(step, context, 0, 0) } {
        Some(0) => return false,
        Some(_) => {}
        None => return false,
    }
    let mut found = false;
    loop {
        match unsafe { step_call(step, context, 1, 0) } {
            Some(0) => {}
            Some(_) => {
                found = true;
                break;
            }
            None => return false,
        }
        match unsafe { step_call(step, context, 2, 0) } {
            Some(0) => break,
            Some(_) => {}
            None => return false,
        }
    }
    if unsafe { step_call(step, context, 3, 0) }.is_none() {
        return false;
    }
    found
}

/// Safety: callback owns native map lookup and header I/O. Rust controls
/// complete 1..level chain order and stops at the first invalid mip.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_validate_mip_chain(
    level: u32,
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    for current in 1..=level {
        if unsafe { step_call(step, context, 0, current as usize) } != Some(1) {
            return false;
        }
    }
    true
}

/// Safety: left is readable for left_length bytes when nonempty.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_join_policy(
    left: *const u8,
    left_length: usize,
    right_length: usize,
) -> u32 {
    if left_length == 0 {
        return 0;
    }
    if right_length == 0 {
        return 1;
    }
    if left.is_null() {
        return 0;
    }
    if matches!(unsafe { byte(left, left_length - 1) }, b'\\' | b'/') {
        2
    } else {
        3
    }
}

/// Safety: name is readable and lower is the native C locale byte primitive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_legacy_directory(
    name: *const u8,
    length: usize,
    lower: Option<Lower>,
) -> bool {
    unsafe {
        a2fo_rs_stage9_rgb_ascii_equal(name, length, b"RGB".as_ptr(), 3, lower)
            || a2fo_rs_stage9_rgb_ascii_equal(name, length, b"Index8".as_ptr(), 6, lower)
            || a2fo_rs_stage9_rgb_ascii_equal(name, length, b"Compressed".as_ptr(), 10, lower)
    }
}

unsafe fn step_call(step: Step, context: *mut c_void, operation: u32, index: usize) -> Option<u32> {
    let result = unsafe { step(context, operation, index) };
    if result == u32::MAX {
        None
    } else {
        Some(result)
    }
}

/// Safety: callback owns native strings and sampled game pointer. Each
/// mutation finishes before the next operation, preserving output aliasing.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_texture_name(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    if unsafe { step_call(step, context, 0, 0) } != Some(1)
        || unsafe { step_call(step, context, 1, 0) } != Some(1)
        || unsafe { step_call(step, context, 2, 0) } != Some(1)
    {
        return false;
    }
    let Some(suffix) = (unsafe { step_call(step, context, 3, 0) }) else {
        return false;
    };
    if suffix == 1 && unsafe { step_call(step, context, 4, 0) }.is_none() {
        return false;
    }
    if suffix == 2 && unsafe { step_call(step, context, 5, 0) }.is_none() {
        return false;
    }
    (unsafe { step_call(step, context, 6, 0) }) == Some(1)
}

/// Safety: native callbacks probe the three owned maps in priority order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_key_exists(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    for route in 0..3 {
        match unsafe { step_call(step, context, 0, route) } {
            Some(0) => {}
            Some(_) => return true,
            None => return false,
        }
    }
    false
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_rgb_direct_index(request: u32) -> u32 {
    match request {
        1 => 0,
        2 => 1,
        3 => 2,
        _ => 3,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_rgb_root_label(route: u32) -> u32 {
    match route {
        1 => 0,
        2 => 1,
        3 => 2,
        4 => 3,
        _ => 4,
    }
}

/// Safety: callback owns CRITICAL_SECTION, native file handles, vector
/// iterators, and all cache storage. Errors remain native until Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_clear_conversion(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    let Some(locked) = (unsafe { step_call(step, context, 0, 0) }) else {
        return false;
    };
    if locked != 0 {
        for op in [1, 2] {
            if unsafe { step_call(step, context, op, 0) }.is_none() {
                return false;
            }
        }
        loop {
            match unsafe { step_call(step, context, 3, 0) } {
                Some(0) => break,
                Some(_) => {}
                None => return false,
            }
            if unsafe { step_call(step, context, 4, 0) }.is_none()
                || unsafe { step_call(step, context, 5, 0) }.is_none()
            {
                return false;
            }
        }
    }
    for op in 6..=9 {
        if unsafe { step_call(step, context, op, 0) }.is_none() {
            return false;
        }
    }
    if locked != 0 {
        let Some(has_directory) = (unsafe { step_call(step, context, 10, 0) }) else {
            return false;
        };
        if has_directory != 0 && unsafe { step_call(step, context, 11, 0) }.is_none() {
            return false;
        }
        for op in 12..=14 {
            if unsafe { step_call(step, context, op, 0) }.is_none() {
                return false;
            }
        }
    } else if unsafe { step_call(step, context, 15, 0) }.is_none() {
        return false;
    }
    true
}

/// Safety: callback retains the native cleanup primitives and global state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_clear_state(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    for op in 0..=21 {
        if unsafe { step_call(step, context, op, 0) }.is_none() {
            return false;
        }
    }
    true
}

/// Safety: callback retains native lock, paths, map/vector storage, and
/// catches C++ exceptions until this Rust frame has returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_prepare_workflow(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    match unsafe { step_call(step, context, 0, 0) } {
        Some(0) => {}
        Some(_) => return true,
        None => return false,
    }
    match unsafe { step_call(step, context, 1, 0) } {
        Some(0) => {}
        Some(_) => return true,
        None => return false,
    }
    match unsafe { step_call(step, context, 2, 0) } {
        Some(0) => {}
        Some(_) => return false,
        None => return false,
    }
    match unsafe { step_call(step, context, 3, 0) } {
        Some(0) => {}
        Some(_) => return true,
        None => return false,
    }
    match unsafe { step_call(step, context, 4, 0) } {
        Some(0) => (unsafe { step_call(step, context, 5, 0) }) == Some(1),
        Some(1) => match unsafe { step_call(step, context, 6, 0) } {
            Some(1) => (unsafe { step_call(step, context, 7, 0) }) == Some(1),
            Some(_) => {
                let _ = unsafe { step_call(step, context, 8, 0) };
                false
            }
            None => false,
        },
        Some(_) => {
            let _ = unsafe { step_call(step, context, 8, 0) };
            false
        }
        None => false,
    }
}

/// Safety: callback retains native cache and vector storage, and captures any
/// C++ exception until Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_resize_workflow(
    step: Option<Step>,
    context: *mut c_void,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    match unsafe { step_call(step, context, 0, 0) } {
        Some(0) => {}
        Some(_) => return true,
        None => return false,
    }
    match unsafe { step_call(step, context, 1, 0) } {
        Some(0) => {}
        Some(_) => return false,
        None => return false,
    }
    match unsafe { step_call(step, context, 2, 0) } {
        Some(1) => {}
        Some(_) => {
            let _ = unsafe { step_call(step, context, 6, 0) };
            return false;
        }
        None => return false,
    }
    let kind = unsafe { step_call(step, context, 3, 0) };
    if matches!(kind, Some(0 | 1)) {
        match unsafe { step_call(step, context, 4, 0) } {
            Some(1) => return (unsafe { step_call(step, context, 5, 0) }) == Some(1),
            Some(_) => {}
            None => return false,
        }
    }
    if kind.is_some() {
        let _ = unsafe { step_call(step, context, 6, 0) };
    }
    false
}

/// Safety: bytes are writable for length bytes. The native callback applies
/// the process C locale's original unsigned-byte lowercasing.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_normalize(
    bytes: *mut u8,
    length: usize,
    lower: Option<Lower>,
) {
    let Some(lower) = lower else {
        return;
    };
    if bytes.is_null() {
        return;
    }
    for index in 0..length {
        let current = unsafe { *bytes.add(index) };
        unsafe {
            *bytes.add(index) = if current == b'/' {
                b'\\'
            } else {
                lower(current)
            };
        }
    }
}

/// Safety: inputs are readable for their lengths. The callback is the native
/// C locale lower primitive and does not mutate either input.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_ascii_equal(
    left: *const u8,
    left_length: usize,
    right: *const u8,
    right_length: usize,
    lower: Option<Lower>,
) -> bool {
    let Some(lower) = lower else {
        return false;
    };
    if left_length != right_length
        || (left.is_null() && left_length != 0)
        || (right.is_null() && right_length != 0)
    {
        return false;
    }
    for index in 0..left_length {
        if unsafe { lower(byte(left, index)) != lower(byte(right, index)) } {
            return false;
        }
    }
    true
}

/// Safety: normalized is readable and key is writable. Returns 0 for an
/// existing TGA suffix, 1 for DDS replacement, 2 for TGA append.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_name_key(
    normalized: *const u8,
    length: usize,
    key: *mut RgbSpan,
) -> u32 {
    if normalized.is_null() || key.is_null() {
        return 2;
    }
    let bytes = unsafe { core::slice::from_raw_parts(normalized, length) };
    let mut start = 0;
    while bytes.get(start..start + 2) == Some(b".\\") {
        start += 2;
    }
    if bytes[start..].starts_with(b"textures\\") {
        start += 9;
    }
    for prefix in [b"rgb\\".as_slice(), b"index8\\", b"compressed\\"] {
        if bytes[start..].starts_with(prefix) {
            start += prefix.len();
            break;
        }
    }
    let value = &bytes[start..];
    unsafe {
        (*key).start = start;
        (*key).length = value.len();
    }
    if value.ends_with(b".dds") {
        1
    } else if value.ends_with(b".tga") {
        0
    } else {
        2
    }
}

unsafe fn byte(data: *const u8, offset: usize) -> u8 {
    unsafe { *data.add(offset) }
}

unsafe fn little_u16(data: *const u8, offset: usize) -> u16 {
    u16::from(unsafe { byte(data, offset) }) | (u16::from(unsafe { byte(data, offset + 1) }) << 8)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_native_tga_header(
    source: *const u8,
    length: usize,
) -> bool {
    if source.is_null() || length < 18 {
        return false;
    }
    unsafe {
        little_u16(source, 12) != 0
            && little_u16(source, 14) != 0
            && byte(source, 1) == 0
            && byte(source, 2) == 2
            && matches!(byte(source, 16), 24 | 32)
    }
}

// Keep the decoder's native field arguments separate so pixel admissions and
// cursor advancement stay in the source order of the original TGA parser.
#[allow(clippy::too_many_arguments)]
unsafe fn read_tga_pixel(
    source: *const u8,
    length: usize,
    cursor: &mut usize,
    kind: u8,
    pixel_depth: u8,
    input_bytes: usize,
    palette_offset: usize,
    color_map_first: u16,
    color_map_length: u16,
    color_map_depth: u8,
    palette_bytes: usize,
) -> Option<[u8; 4]> {
    if *cursor > length || input_bytes > length - *cursor {
        return None;
    }
    let input = *cursor;
    let mut pixel = [0u8, 0, 0, 255];
    let expand = |value: u16| -> u8 {
        let bits = value & 0x1f;
        ((bits << 3) | (bits >> 2)) as u8
    };
    if kind == 1 {
        let mut index = u32::from(unsafe { byte(source, input) });
        if input_bytes == 2 {
            index |= u32::from(unsafe { byte(source, input + 1) }) << 8;
        }
        *cursor += input_bytes;
        if index < u32::from(color_map_first)
            || index >= u32::from(color_map_first) + u32::from(color_map_length)
        {
            return None;
        }
        let entry = palette_offset + (index - u32::from(color_map_first)) as usize * palette_bytes;
        if matches!(color_map_depth, 24 | 32) {
            pixel[0] = unsafe { byte(source, entry) };
            pixel[1] = unsafe { byte(source, entry + 1) };
            pixel[2] = unsafe { byte(source, entry + 2) };
            if color_map_depth == 32 {
                pixel[3] = unsafe { byte(source, entry + 3) };
            }
        } else {
            let packed = unsafe { little_u16(source, entry) };
            pixel[0] = expand(packed);
            pixel[1] = expand(packed >> 5);
            pixel[2] = expand(packed >> 10);
        }
        return Some(pixel);
    }
    if kind == 2 {
        if pixel_depth == 16 {
            let packed = unsafe { little_u16(source, input) };
            pixel[0] = expand(packed);
            pixel[1] = expand(packed >> 5);
            pixel[2] = expand(packed >> 10);
        } else {
            pixel[0] = unsafe { byte(source, input) };
            pixel[1] = unsafe { byte(source, input + 1) };
            pixel[2] = unsafe { byte(source, input + 2) };
            if pixel_depth == 32 {
                pixel[3] = unsafe { byte(source, input + 3) };
            }
        }
        *cursor += input_bytes;
        return Some(pixel);
    }
    pixel[0] = unsafe { byte(source, input) };
    pixel[1] = pixel[0];
    pixel[2] = pixel[0];
    if pixel_depth == 16 {
        pixel[3] = unsafe { byte(source, input + 1) };
    }
    *cursor += input_bytes;
    Some(pixel)
}

/// Safety: source stays readable and unmodified through synchronous native
/// output callbacks. The caller catches and rethrows any C++ allocation error.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_prepare_tga(
    source: *const u8,
    length: usize,
    emit: Option<Emit>,
    context: *mut c_void,
) -> u32 {
    if source.is_null() || length < 18 {
        return 2;
    }
    if unsafe { a2fo_rs_stage9_rgb_native_tga_header(source, length) } {
        return 0;
    }
    let Some(emit) = emit else {
        return 2;
    };
    let id_length = unsafe { byte(source, 0) } as usize;
    let color_map_type = unsafe { byte(source, 1) };
    let image_type = unsafe { byte(source, 2) };
    let color_map_first = unsafe { little_u16(source, 3) };
    let color_map_length = unsafe { little_u16(source, 5) };
    let color_map_depth = unsafe { byte(source, 7) };
    let width = unsafe { little_u16(source, 12) };
    let height = unsafe { little_u16(source, 14) };
    let pixel_depth = unsafe { byte(source, 16) };
    if width == 0 || height == 0 {
        return 2;
    }
    let (kind, rle) = match image_type {
        1 => (1u8, false),
        2 => (2, false),
        3 => (3, false),
        9 => (1, true),
        10 => (2, true),
        11 => (3, true),
        _ => return 2,
    };
    let mut cursor = 18usize + id_length;
    if cursor > length {
        return 2;
    }
    let mut palette_offset = 0usize;
    let mut palette_bytes = 0usize;
    if kind == 1 {
        if color_map_type != 1
            || color_map_length == 0
            || !matches!(pixel_depth, 8 | 16)
            || !matches!(color_map_depth, 15 | 16 | 24 | 32)
        {
            return 2;
        }
        palette_bytes = (color_map_depth as usize).div_ceil(8);
        let palette_size = color_map_length as usize * palette_bytes;
        if palette_size > length - cursor {
            return 2;
        }
        palette_offset = cursor;
        cursor += palette_size;
    } else if color_map_type != 0 {
        return 2;
    }
    let (input_bytes, output_bytes) = if kind == 1 {
        (
            pixel_depth as usize / 8,
            if color_map_depth == 32 { 4 } else { 3 },
        )
    } else if kind == 2 {
        if !matches!(pixel_depth, 16 | 24 | 32) {
            return 2;
        }
        (
            (pixel_depth as usize).div_ceil(8),
            if pixel_depth == 32 { 4 } else { 3 },
        )
    } else {
        if !matches!(pixel_depth, 8 | 16) {
            return 2;
        }
        (
            pixel_depth as usize / 8,
            if pixel_depth == 16 { 4 } else { 3 },
        )
    };
    let pixel_count = (width as usize).wrapping_mul(height as usize);
    const MAXIMUM: usize = 256 * 1024 * 1024;
    if pixel_count > (MAXIMUM - 18) / output_bytes {
        return 2;
    }
    let mut header = [0u8; 18];
    header[2] = 2;
    for (index, slot) in header.iter_mut().enumerate().take(16).skip(8) {
        *slot = unsafe { byte(source, index) };
    }
    header[16] = (output_bytes * 8) as u8;
    header[17] = (unsafe { byte(source, 17) } & 0x30) | if output_bytes == 4 { 8 } else { 0 };
    if !unsafe {
        emit(
            context,
            0,
            header.as_ptr(),
            18,
            18 + pixel_count * output_bytes,
        )
    } {
        return 2;
    }
    let mut emitted = 0usize;
    while emitted < pixel_count {
        let mut packet_count = pixel_count - emitted;
        let mut repeated = false;
        if rle {
            if cursor >= length {
                return 2;
            }
            let packet = unsafe { byte(source, cursor) };
            cursor += 1;
            packet_count = ((packet & 0x7f) + 1) as usize;
            repeated = packet & 0x80 != 0;
            if packet_count > pixel_count - emitted {
                return 2;
            }
        }
        if repeated {
            let Some(pixel) = (unsafe {
                read_tga_pixel(
                    source,
                    length,
                    &mut cursor,
                    kind,
                    pixel_depth,
                    input_bytes,
                    palette_offset,
                    color_map_first,
                    color_map_length,
                    color_map_depth,
                    palette_bytes,
                )
            }) else {
                return 2;
            };
            for _ in 0..packet_count {
                if !unsafe { emit(context, 1, pixel.as_ptr(), output_bytes, 0) } {
                    return 2;
                }
            }
        } else {
            for _ in 0..packet_count {
                let Some(pixel) = (unsafe {
                    read_tga_pixel(
                        source,
                        length,
                        &mut cursor,
                        kind,
                        pixel_depth,
                        input_bytes,
                        palette_offset,
                        color_map_first,
                        color_map_length,
                        color_map_depth,
                        palette_bytes,
                    )
                }) else {
                    return 2;
                };
                if !unsafe { emit(context, 1, pixel.as_ptr(), output_bytes, 0) } {
                    return 2;
                }
            }
        }
        emitted += packet_count;
        if !rle {
            break;
        }
    }
    if emitted == pixel_count { 1 } else { 2 }
}

/// Safety: source remains readable and the native allocation callback returns
/// a writable output buffer of the requested size or null after catching C++.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_resize_tga(
    source: *const u8,
    length: usize,
    target_width: u16,
    target_height: u16,
    allocate: Option<Allocate>,
    context: *mut c_void,
) -> bool {
    let Some(allocate) = allocate else {
        return false;
    };
    if target_width == 0
        || target_height == 0
        || !unsafe { a2fo_rs_stage9_rgb_native_tga_header(source, length) }
    {
        return false;
    }
    let width = unsafe { little_u16(source, 12) } as usize;
    let height = unsafe { little_u16(source, 14) } as usize;
    let pixel_bytes = unsafe { byte(source, 16) } as usize / 8;
    let offset = 18 + unsafe { byte(source, 0) } as usize;
    let target_width = target_width as usize;
    let target_height = target_height as usize;
    if target_width > width
        || target_height > height
        || !width.is_multiple_of(target_width)
        || !height.is_multiple_of(target_height)
        || !(3..=4).contains(&pixel_bytes)
    {
        return false;
    }
    if offset > length || width.wrapping_mul(height) > (length - offset) / pixel_bytes {
        return false;
    }
    const MAXIMUM: usize = 256 * 1024 * 1024;
    let count = target_width.wrapping_mul(target_height);
    if count > (MAXIMUM - 18) / pixel_bytes {
        return false;
    }
    let output = unsafe { allocate(context, 18 + count * pixel_bytes) };
    if output.is_null() {
        return false;
    }
    unsafe {
        *output.add(2) = 2;
    }
    for index in 8..12 {
        unsafe {
            *output.add(index) = byte(source, index);
        }
    }
    for (offset, value) in [(12, target_width as u16), (14, target_height as u16)] {
        unsafe {
            *output.add(offset) = value as u8;
            *output.add(offset + 1) = (value >> 8) as u8;
        }
    }
    unsafe {
        *output.add(16) = byte(source, 16);
        *output.add(17) = byte(source, 17);
    }
    let horizontal = width / target_width;
    let vertical = height / target_height;
    let samples = horizontal as u64 * vertical as u64;
    for y in 0..target_height {
        for x in 0..target_width {
            let destination = 18usize.wrapping_add(
                y.wrapping_mul(target_width)
                    .wrapping_add(x)
                    .wrapping_mul(pixel_bytes),
            );
            for channel in 0..pixel_bytes {
                let mut total = 0u64;
                for source_y in 0..vertical {
                    let row = y * vertical + source_y;
                    for source_x in 0..horizontal {
                        let column = x * horizontal + source_x;
                        let input = offset.wrapping_add(
                            row.wrapping_mul(width)
                                .wrapping_add(column)
                                .wrapping_mul(pixel_bytes),
                        );
                        total += u64::from(unsafe { byte(source, input + channel) });
                    }
                }
                unsafe {
                    *output.add(destination + channel) = ((total + samples / 2) / samples) as u8;
                }
            }
        }
    }
    true
}

#[repr(C)]
pub struct RgbSpan {
    start: usize,
    length: usize,
}

#[repr(C)]
pub struct RgbMip {
    stem_length: usize,
    level: u32,
}

#[repr(C)]
pub struct RgbTgaHeader {
    width: u16,
    height: u16,
    color_map_type: u8,
    image_type: u8,
    color_map_depth: u8,
    pixel_depth: u8,
}

/// Safety: source contains at least length readable bytes; summary is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_header_summary(
    source: *const u8,
    length: usize,
    summary: *mut RgbTgaHeader,
) -> bool {
    if source.is_null() || length < 18 || summary.is_null() {
        return false;
    }
    unsafe {
        (*summary).width = little_u16(source, 12);
        (*summary).height = little_u16(source, 14);
        (*summary).color_map_type = byte(source, 1);
        (*summary).image_type = byte(source, 2);
        (*summary).color_map_depth = byte(source, 7);
        (*summary).pixel_depth = byte(source, 16);
        (*summary).width != 0 && (*summary).height != 0 && (*summary).pixel_depth != 0
    }
}

/// Safety: both summaries are readable POD copies.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_same_pixel_format(
    left: *const RgbTgaHeader,
    right: *const RgbTgaHeader,
) -> bool {
    if left.is_null() || right.is_null() {
        return false;
    }
    unsafe {
        (*left).color_map_type == (*right).color_map_type
            && (*left).image_type == (*right).image_type
            && (*left).color_map_depth == (*right).color_map_depth
            && (*left).pixel_depth == (*right).pixel_depth
    }
}

fn has_suffix(value: &[u8], suffix: &[u8]) -> bool {
    value.ends_with(suffix)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_tga_filename(name: *const u8, length: usize) -> bool {
    if name.is_null() {
        return false;
    }
    unsafe { core::slice::from_raw_parts(name, length) }.ends_with(b".tga")
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_dds_filename(name: *const u8, length: usize) -> bool {
    if name.is_null() {
        return false;
    }
    unsafe { core::slice::from_raw_parts(name, length) }.ends_with(b".dds")
}

/// Safety: `normalized` is readable for `length` bytes and `key` is writable.
/// The caller performs original filename/mode admission and normalization.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_request(
    normalized: *const u8,
    length: usize,
    key: *mut RgbSpan,
) -> u32 {
    if normalized.is_null() || key.is_null() {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(normalized, length) };
    let mut start = 0;
    while bytes.get(start..start + 2) == Some(b".\\") {
        start += 2;
    }
    let remaining = &bytes[start..];
    let mut key_start =
        if let Some(position) = remaining.windows(9).rposition(|x| x == b"textures\\") {
            start + position + 9
        } else if let Some(position) = remaining.iter().rposition(|&x| x == b'\\') {
            start + position + 1
        } else {
            start
        };
    let mut value = &bytes[key_start..];
    let request = if value.starts_with(b"rgb\\") {
        key_start += 4;
        value = &bytes[key_start..];
        if value.is_empty() { 0 } else { 1 }
    } else if value.starts_with(b"index8\\") {
        key_start += 7;
        value = &bytes[key_start..];
        if value.is_empty() { 0 } else { 2 }
    } else if value.starts_with(b"compressed\\") {
        key_start += 11;
        value = &bytes[key_start..];
        if value.is_empty() { 0 } else { 3 }
    } else if !value.is_empty() && has_suffix(value, b".tga") {
        4
    } else if has_suffix(value, b".dds") {
        5
    } else {
        0
    };
    unsafe {
        (*key).start = key_start;
        (*key).length = value.len();
    }
    request
}

/// Safety: `key` is readable for `length` bytes and `mip` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_manual_mip(
    key: *const u8,
    length: usize,
    mip: *mut RgbMip,
) -> bool {
    if mip.is_null() {
        return false;
    }
    unsafe {
        (*mip).stem_length = 0;
        (*mip).level = 0;
    }
    if key.is_null() || length <= 6 {
        return false;
    }
    let bytes = unsafe { core::slice::from_raw_parts(key, length) };
    if !has_suffix(bytes, b".tga") {
        return false;
    }
    let extension = length - 4;
    let Some(separator) = bytes[..extension].iter().rposition(|&x| x == b'_') else {
        return false;
    };
    let slash = bytes.iter().rposition(|&x| x == b'\\' || x == b'/');
    if slash.is_some_and(|position| separator <= position) || separator + 1 == extension {
        return false;
    }
    let mut parsed = 0u32;
    for &byte in &bytes[separator + 1..extension] {
        if !byte.is_ascii_digit() {
            return false;
        }
        parsed = parsed * 10 + u32::from(byte - b'0');
        if parsed > 8 {
            return false;
        }
    }
    if parsed == 0 {
        return false;
    }
    unsafe {
        (*mip).stem_length = separator;
        (*mip).level = parsed;
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_rgb_mip_step(
    base_width: u16,
    base_height: u16,
    previous_width: u16,
    previous_height: u16,
    current_width: u16,
    current_height: u16,
    same_format: bool,
) -> bool {
    same_format
        && u64::from(current_width) * u64::from(base_height)
            == u64::from(base_width) * u64::from(current_height)
        && current_width < previous_width
        && current_height < previous_height
}

/// Safety: both output pointers are writable. Outputs are touched only on success.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_rgb_synthesis_dimensions(
    child_root: u32,
    inherited_root: u32,
    base_width: u16,
    base_height: u16,
    level: u32,
    expected_width: *mut u16,
    expected_height: *mut u16,
) -> bool {
    if expected_width.is_null()
        || expected_height.is_null()
        || child_root <= inherited_root
        || level == 0
        || level > 8
    {
        return false;
    }
    let divisor = 1u32 << level;
    if u32::from(base_width) % divisor != 0 || u32::from(base_height) % divisor != 0 {
        return false;
    }
    unsafe {
        *expected_width = (u32::from(base_width) / divisor) as u16;
        *expected_height = (u32::from(base_height) / divisor) as u16;
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_rgb_inherited_is_large_enough(
    header_read: bool,
    inherited_width: u16,
    inherited_height: u16,
    expected_width: u16,
    expected_height: u16,
) -> bool {
    header_read && inherited_width >= expected_width && inherited_height >= expected_height
}
