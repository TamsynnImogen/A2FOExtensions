//! Portable M3D module admission, lifecycle and load routing.

use alloc::vec::Vec;
use core::ffi::c_void;

#[repr(C)]
struct BytesView {
    data: *const u8,
    len: usize,
}

#[repr(C)]
struct IndexTotals {
    m3d: u32,
    sod: u32,
    removed: u32,
}

#[repr(C)]
struct FileLength {
    value: i64,
}

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

unsafe fn call(step: Step, context: *mut c_void, op: u32, arg: usize) -> Option<usize> {
    let value = unsafe { step(context, op, arg) };
    (value != usize::MAX).then_some(value)
}

/// Safety: native callback owns API pointers, PE images, hook storage and
/// exception capture for this synchronous startup sequence.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_init(step: Option<Step>, context: *mut c_void) -> u8 {
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
    let mut armada = matches!(unsafe { call(step, context, 4, 0) }, Some(1));
    for op in 5..=9 {
        if !armada {
            break;
        }
        armada = match unsafe { call(step, context, op, 0) } {
            Some(value) => value != 0,
            None => return 0,
        };
    }
    if !armada {
        unsafe { call(step, context, 10, 0) };
        return 0;
    }
    for index in 0..2 {
        match unsafe { call(step, context, 11, index) } {
            Some(1) => {}
            Some(_) => {
                unsafe { call(step, context, 12, 0) };
                return 0;
            }
            None => return 0,
        }
    }
    if unsafe { call(step, context, 13, 0) }.is_none() {
        return 0;
    }
    match unsafe { call(step, context, 14, 0) } {
        Some(1) => {}
        Some(_) => {
            unsafe { call(step, context, 15, 0) };
            return 0;
        }
        None => return 0,
    }
    for index in 0..2 {
        match unsafe { call(step, context, 16, index) } {
            Some(1) => {}
            Some(_) => {
                unsafe { call(step, context, 17, 0) };
                return 0;
            }
            None => return 0,
        }
    }
    for op in 18..=20 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    1
}

/// Safety: native callback keeps module-owned maps and bridge pointers live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_shutdown(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    for op in 0..=10 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
}

/// Safety: native context owns the original source/cache iterators, strings,
/// model resource and gateway. Native callbacks are noexcept as in the
/// original resolver and retain termination on allocation failure.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_resolve(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return unsafe { step(context, 18, 0) };
    }
    unsafe { step(context, 1, 0) };
    if unsafe { step(context, 2, 0) } == 0 {
        return unsafe { step(context, 18, 0) };
    }
    unsafe { step(context, 3, 0) };
    if unsafe { step(context, 4, 0) } != 0 {
        return unsafe { step(context, 18, 0) };
    }
    unsafe { step(context, 5, 0) };
    if unsafe { step(context, 6, 0) } != 0 {
        if unsafe { step(context, 7, 0) } != 0 {
            return unsafe { step(context, 8, 0) };
        }
        unsafe { step(context, 19, 0) };
        unsafe { step(context, 13, 0) };
        return unsafe { step(context, 18, 0) };
    }
    if unsafe { step(context, 9, 0) } == 0 {
        return unsafe { step(context, 18, 0) };
    }
    let geometry = unsafe { step(context, 10, 0) };
    unsafe { step(context, 11, 0) };
    if geometry != 0 {
        if unsafe { step(context, 12, 0) } == 0 {
            unsafe { step(context, 13, 0) };
            unsafe { step(context, 14, 0) };
            return unsafe { step(context, 18, 0) };
        }
        unsafe { step(context, 15, 0) };
        return geometry;
    }
    unsafe { step(context, 13, 0) };
    unsafe { step(context, 17, 0) };
    unsafe { step(context, 18, 0) }
}

/// Safety: native callback owns input/output strings, applies the original
/// CRT byte folding and rethrows any C++ string exception after Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_name(
    step: Option<Step>,
    context: *mut c_void,
    kind: u32,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let Some(length) = (unsafe { call(step, context, 0, 0) }) else {
        return 0;
    };
    let mut input = Vec::with_capacity(length);
    for index in 0..length {
        let Some(byte) = (unsafe { call(step, context, 1, index) }) else {
            return 0;
        };
        input.push(byte as u8);
    }
    let mut start = 0;
    let mut end = input.len();
    if (kind == 1 || kind == 3 || kind == 4)
        && let Some(slash) = input
            .iter()
            .rposition(|&byte| byte == b'/' || byte == b'\\')
    {
        start = slash + 1;
    }
    if (kind == 1 || kind == 2 || kind == 4)
        && let Some(relative) = input[start..end].iter().rposition(|&byte| byte == b'.')
    {
        let dot = start + relative;
        if kind == 2 {
            end = dot;
        } else {
            let mut extension = Vec::with_capacity(end - dot);
            for byte in &input[dot..end] {
                let Some(lowered) = (unsafe { call(step, context, 2, *byte as usize) }) else {
                    return 0;
                };
                extension.push(lowered as u8);
            }
            if (kind == 1 && (extension == b".sod" || extension == b".m3d"))
                || (kind == 4
                    && (extension == b".dds"
                        || extension == b".tga"
                        || extension == b".png"
                        || extension == b".bmp"))
            {
                end = dot;
            }
        }
    }
    let mut output = Vec::with_capacity(end - start);
    for &byte in &input[start..end] {
        if kind == 2 || kind == 3 || kind == 4 {
            output.push(byte);
        } else {
            let Some(lowered) = (unsafe { call(step, context, 2, byte as usize) }) else {
                return 0;
            };
            output.push(lowered as u8);
        }
    }
    let view = BytesView {
        data: output.as_ptr(),
        len: output.len(),
    };
    u8::from(matches!(
        unsafe { call(step, context, 3, &view as *const _ as usize) },
        Some(1)
    ))
}

/// Safety: native callback owns both C++ strings and the output; Rust reads
/// only empty/last-byte scalars before choosing the original join operation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_join(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    let Some(left) = (unsafe { call(step, context, 0, 0) }) else {
        return;
    };
    if left == 0 {
        unsafe { call(step, context, 3, 0) };
        return;
    }
    let Some(right) = (unsafe { call(step, context, 1, 0) }) else {
        return;
    };
    if right == 0 {
        unsafe { call(step, context, 4, 0) };
        return;
    }
    let Some(last) = (unsafe { call(step, context, 2, 0) }) else {
        return;
    };
    let op = if last == b'/' as usize || last == b'\\' as usize {
        5
    } else {
        6
    };
    unsafe { call(step, context, op, 0) };
}

/// Safety: native context owns each root, Win32 enumeration vectors, ordered
/// file names and unordered-set iterator. Rust controls overlay traversal.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_index(step: Option<Step>, context: *mut c_void) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let Some(count) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    let mut totals = IndexTotals {
        m3d: 0,
        sod: 0,
        removed: 0,
    };
    for root in 0..count {
        match unsafe { call(step, context, 2, root) } {
            Some(1) => {}
            Some(_) => continue,
            None => return 0,
        }
        let Some(removed) = (unsafe { call(step, context, 3, 0) }) else {
            return 0;
        };
        totals.removed = totals.removed.wrapping_add(removed as u32);
        if !matches!(unsafe { call(step, context, 4, 0) }, Some(1)) {
            continue;
        }
        if unsafe { call(step, context, 5, 0) }.is_none() {
            return 0;
        }
        let Some(sod_count) = (unsafe { call(step, context, 6, 0) }) else {
            return 0;
        };
        for index in 0..sod_count {
            match unsafe { call(step, context, 7, index) } {
                Some(1) => continue,
                Some(_) => {}
                None => return 0,
            }
            if unsafe { call(step, context, 8, index) }.is_none() {
                return 0;
            }
            totals.sod = totals.sod.wrapping_add(1);
        }
        if unsafe { call(step, context, 9, 0) }.is_none() {
            return 0;
        }
        let Some(m3d_count) = (unsafe { call(step, context, 10, 0) }) else {
            return 0;
        };
        for index in 0..m3d_count {
            totals.m3d = totals.m3d.wrapping_add(1);
            if unsafe { call(step, context, 11, index) }.is_none() {
                return 0;
            }
            match unsafe { call(step, context, 12, 0) } {
                Some(1) => {
                    if unsafe { call(step, context, 13, index) }.is_none() {
                        return 0;
                    }
                }
                Some(_) => {}
                None => return 0,
            }
        }
        if unsafe { call(step, context, 14, 0) }.is_none() {
            return 0;
        }
        loop {
            match unsafe { call(step, context, 15, 0) } {
                Some(1) => {
                    if unsafe { call(step, context, 16, 0) }.is_none() {
                        return 0;
                    }
                }
                Some(_) => break,
                None => return 0,
            }
        }
    }
    u8::from(matches!(
        unsafe { call(step, context, 17, &totals as *const _ as usize) },
        Some(1)
    ))
}

/// Safety: native callback owns Win32 enumeration and deletion paths. Rust
/// controls original sidecar-before-SOD admission and deletion order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_remove_caches(
    step: Option<Step>,
    context: *mut c_void,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return 0;
    }
    let Some(count) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    let Some(suffix) = (unsafe { call(step, context, 3, 0) }) else {
        return 0;
    };
    let mut removed = 0u32;
    for index in 0..count {
        if unsafe { call(step, context, 4, index) }.is_none() {
            return 0;
        }
        let Some(length) = (unsafe { call(step, context, 2, index) }) else {
            return 0;
        };
        if length <= suffix {
            continue;
        }
        if unsafe { call(step, context, 9, index) }.is_none() {
            return 0;
        }
        let sod_exists = matches!(unsafe { call(step, context, 5, 0) }, Some(1));
        if !sod_exists || matches!(unsafe { call(step, context, 6, 0) }, Some(1)) {
            let marker_deleted = matches!(unsafe { call(step, context, 7, 0) }, Some(1));
            if marker_deleted || matches!(unsafe { call(step, context, 8, 0) }, Some(1)) {
                removed = removed.wrapping_add(1);
            }
        }
    }
    removed
}

/// Safety: native context owns input/output bytes, guarded parser stream,
/// conversion stats and model. Exceptions are reported to the original
/// noexcept wrapper for its existing log-and-null path.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_load(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    match unsafe { call(step, context, 0, 0) } {
        Some(1) => {}
        Some(_) => {
            unsafe { call(step, context, 1, 0) };
            return 0;
        }
        None => return 0,
    }
    match unsafe { call(step, context, 2, 0) } {
        Some(1) => {}
        Some(_) => {
            unsafe { call(step, context, 3, 0) };
            return 0;
        }
        None => return 0,
    }
    match unsafe { call(step, context, 4, 0) } {
        Some(1) => {}
        Some(_) => {
            unsafe { call(step, context, 5, 0) };
            return 0;
        }
        None => return 0,
    }
    if unsafe { call(step, context, 6, 0) }.is_none() {
        return 0;
    }
    let Some(geometry) = (unsafe { call(step, context, 7, 0) }) else {
        return 0;
    };
    match unsafe { call(step, context, 8, 0) } {
        Some(1) => {
            if unsafe { call(step, context, 9, 0) }.is_none() {
                return 0;
            }
            unsafe { call(step, context, 10, 0) };
            return 0;
        }
        Some(_) => {}
        None => return 0,
    }
    if geometry == 0 {
        if unsafe { call(step, context, 11, 0) }.is_none() {
            return 0;
        }
        unsafe { call(step, context, 10, 0) };
        return 0;
    }
    for op in 12..=15 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    match unsafe { call(step, context, 16, 0) } {
        Some(1) => {
            if unsafe { call(step, context, 17, 0) }.is_none() {
                return 0;
            }
        }
        Some(_) => match unsafe { call(step, context, 21, 0) } {
            Some(1) => {
                if unsafe { call(step, context, 18, 0) }.is_none() {
                    return 0;
                }
            }
            Some(_) => {}
            None => return 0,
        },
        None => return 0,
    }
    for op in 19..=20 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return 0;
        }
    }
    geometry
}

/// Safety: native context owns the input stream, output vector and error
/// string. Rust decides the size cap and exact read/failure order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_read_file(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        unsafe { call(step, context, 1, 0) };
        return 0;
    }
    if unsafe { call(step, context, 2, 0) }.is_none() {
        return 0;
    }
    let Some(pointer) = (unsafe { call(step, context, 3, 0) }) else {
        return 0;
    };
    // The pointer refers to a native-owned POD for this synchronous call.
    let length = unsafe { (*(pointer as *const FileLength)).value };
    if !(0..=256 * 1024 * 1024).contains(&length) {
        unsafe { call(step, context, 4, 0) };
        return 0;
    }
    if unsafe { call(step, context, 5, 0) }.is_none() {
        return 0;
    }
    if unsafe { call(step, context, 6, length as usize) }.is_none() {
        return 0;
    }
    if length != 0 && !matches!(unsafe { call(step, context, 7, length as usize) }, Some(1)) {
        unsafe { call(step, context, 8, 0) };
        return 0;
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_m3d_parser_stage(
    consumed: usize,
    materials: usize,
    nodes: usize,
    animations: usize,
    references: usize,
) -> u32 {
    if consumed <= 14 {
        return 0;
    }
    if materials != 0 && consumed <= materials {
        return 1;
    }
    if nodes != 0 && consumed <= nodes {
        return 2;
    }
    if animations != 0 && consumed <= animations {
        return 3;
    }
    if references != 0 && consumed <= references {
        return 4;
    }
    5
}

/// Safety: native callback owns ConversionStats warning storage and all
/// original CRT message formatting/logging.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_report(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return;
    }
    let Some(count) = (unsafe { call(step, context, 1, 0) }) else {
        return;
    };
    let limit = count.min(4);
    for index in 0..limit {
        if unsafe { call(step, context, 2, index) }.is_none() {
            return;
        }
    }
    if count > limit {
        unsafe { call(step, context, 3, count - limit) };
    }
}

/// Safety: native callback validates the live Fleet Ops object and performs
/// the original refcount/mode writes only after Rust admission.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_finalize(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        unsafe { step(context, 1, 0) };
        return 0;
    }
    if unsafe { step(context, 2, 0) } == 0 {
        unsafe { step(context, 3, 0) };
        return 0;
    }
    unsafe { step(context, 4, 0) };
    unsafe { step(context, 5, 0) };
    1
}

/// Safety: native callback owns texture/device state and the original
/// LockSurface/UnlockSurface ABI. Rust controls admission and logging order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_materialize(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    for op in 0..=2 {
        if unsafe { step(context, op, 0) } == 0 {
            return 0;
        }
    }
    if unsafe { step(context, 3, 0) } == usize::MAX {
        return 0;
    }
    if unsafe { step(context, 4, 0) } != 0 {
        unsafe { step(context, 5, 0) };
        unsafe { step(context, 6, 0) };
        return u8::from(unsafe { step(context, 8, 0) } != 0);
    }
    unsafe { step(context, 7, 0) };
    0
}

/// Safety: native context owns live Storm3D nodes, binding vector and mesh
/// map. Rust controls DFS order, per-binding fallback and stage operations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_bind_textures(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    for op in 1..=4 {
        if unsafe { step(context, op, 0) } == 0 {
            unsafe { step(context, 5, 0) };
            return 0;
        }
    }
    if unsafe { step(context, 6, 0) } == 0 {
        return 0;
    }
    loop {
        let pending = unsafe { step(context, 7, 0) };
        let visited = unsafe { step(context, 8, 0) };
        if pending == 0 || visited >= 4096 {
            break;
        }
        unsafe { step(context, 9, 0) };
        if unsafe { step(context, 10, 0) } == 0 {
            continue;
        }
        unsafe { step(context, 11, 0) };
        unsafe { step(context, 12, 0) };
        if unsafe { step(context, 13, 0) } != 0 {
            unsafe { step(context, 14, 0) };
        }
    }
    let count = unsafe { step(context, 15, 0) };
    for index in 0..count {
        if unsafe { step(context, 16, index) } == 0 {
            continue;
        }
        if unsafe { step(context, 17, 0) } != 0 {
            unsafe { step(context, 18, 0) };
        }
        let native_diffuse = unsafe { step(context, 19, 0) };
        if native_diffuse == 0 {
            let diffuse = unsafe { step(context, 21, 0) } != 0;
            if diffuse || unsafe { step(context, 26, 0) } != 0 {
                unsafe { step(context, 20, 0) };
            }
        }
        let diffuse = unsafe { step(context, 21, 0) } != 0;
        if diffuse {
            if unsafe { step(context, 22, 0) } != 0 {
                unsafe { step(context, 23, 0) };
                unsafe { step(context, 24, 0) };
            } else {
                unsafe { step(context, 25, 0) };
            }
        }
        if unsafe { step(context, 26, 0) } != 0 {
            unsafe { step(context, 27, 0) };
        }
        unsafe { step(context, 28, 0) };
        unsafe { step(context, 29, 0) };
        unsafe { step(context, 30, 0) };
    }
    unsafe { step(context, 31, 0) };
    unsafe { step(context, 32, 0) }
}

/// Safety: native context owns texture database pointers, authored/logical
/// names and the raw Texture::Find call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_find_texture(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
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
    if unsafe { step(context, 3, 0) } == 0 {
        return 0;
    }
    let texture = unsafe { step(context, 4, 0) };
    if texture != 0 {
        unsafe { step(context, 5, 0) };
    }
    texture
}

/// Safety: native context owns live node pointers, DFS storage and binding
/// maps. Rust preserves sibling-then-child traversal and note limits.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_hierarchy_audit(
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
    loop {
        let pending = unsafe { step(context, 2, 0) };
        let visited = unsafe { step(context, 3, 0) };
        if pending == 0 || visited >= 4096 {
            break;
        }
        unsafe { step(context, 4, 0) };
        if unsafe { step(context, 5, 0) } == 0 {
            continue;
        }
        unsafe { step(context, 6, 0) };
        if unsafe { step(context, 7, 0) } != 0 {
            unsafe { step(context, 8, 0) };
        }
        unsafe { step(context, 9, 0) };
        unsafe { step(context, 10, 0) };
    }
    unsafe { step(context, 11, 0) };
    let count = unsafe { step(context, 12, 0) };
    for index in 0..count {
        if unsafe { step(context, 13, index) } == 0 {
            unsafe { step(context, 14, 0) };
            if unsafe { step(context, 15, 0) } < 12 {
                unsafe { step(context, 16, 0) };
            }
            continue;
        }
        if unsafe { step(context, 17, 0) } != 0 {
            unsafe { step(context, 18, 0) };
            if unsafe { step(context, 15, 0) } < 12 {
                unsafe { step(context, 19, 0) };
            }
        }
    }
    unsafe { step(context, 20, 0) };
    let notes = unsafe { step(context, 15, 0) };
    for index in 0..notes {
        unsafe { step(context, 21, index) };
    }
}

/// Safety: native context owns authored-name storage, live node names and
/// delayed rename entries. Rust preserves DFS and all admission gates.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_restore_names(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
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
    for index in 0..count {
        if unsafe { step(context, 3, index) } != 0 {
            unsafe { step(context, 4, index) };
        }
    }
    if unsafe { step(context, 5, 0) } == 0 {
        return 0;
    }
    loop {
        let pending = unsafe { step(context, 6, 0) };
        let visited = unsafe { step(context, 7, 0) };
        if pending == 0 || visited >= 4096 {
            break;
        }
        unsafe { step(context, 8, 0) };
        if unsafe { step(context, 9, 0) } == 0 {
            continue;
        }
        if unsafe { step(context, 10, 0) } != 0
            && unsafe { step(context, 11, 0) } != 0
            && unsafe { step(context, 12, 0) } != 0
            && unsafe { step(context, 13, 0) } <= unsafe { step(context, 14, 0) }
            && unsafe { step(context, 15, 0) } != 0
        {
            unsafe { step(context, 16, 0) };
        }
        unsafe { step(context, 17, 0) };
        unsafe { step(context, 18, 0) };
    }
    let renames = unsafe { step(context, 19, 0) };
    for index in 0..renames {
        unsafe { step(context, 20, index) };
    }
    unsafe { step(context, 21, 0) };
    renames
}

/// Safety: native context owns live tree pointers, note storage, optional
/// mesh registration callback and MeshUpdate bridge. Rust preserves read
/// order across registration/rebuild callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_inspect_tree(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        unsafe { step(context, 1, 0) };
        return;
    }
    if unsafe { step(context, 2, 0) } == 0 {
        unsafe { step(context, 3, 0) };
        return;
    }
    let can_rebuild = unsafe { step(context, 4, 0) } != 0;
    loop {
        let pending = unsafe { step(context, 5, 0) };
        let visited = unsafe { step(context, 6, 0) };
        if pending == 0 || visited >= 4096 {
            break;
        }
        unsafe { step(context, 7, 0) };
        if unsafe { step(context, 8, 0) } == 0 {
            continue;
        }
        unsafe { step(context, 9, 0) };
        unsafe { step(context, 10, 0) };
        unsafe { step(context, 11, 0) };
        if unsafe { step(context, 12, 0) } == 0 {
            continue;
        }
        unsafe { step(context, 13, 0) };
        unsafe { step(context, 14, 0) };
        let mut renderer = unsafe { step(context, 15, 0) } != 0;
        if !renderer && can_rebuild {
            unsafe { step(context, 16, 0) };
            renderer = unsafe { step(context, 17, 0) } != 0;
            if renderer {
                unsafe { step(context, 18, 0) };
            }
        }
        unsafe { step(context, 19, usize::from(renderer)) };
        if unsafe { step(context, 20, 0) } < 12 {
            unsafe { step(context, 21, usize::from(!renderer && can_rebuild)) };
        }
    }
    unsafe { step(context, 22, 0) };
    unsafe { step(context, 23, 0) };
    let notes = unsafe { step(context, 20, 0) };
    for index in 0..notes {
        unsafe { step(context, 24, index) };
    }
}

/// Safety: native callback owns the parser-created Storm3D database and its
/// exact destructor/operator-delete ABI; Rust controls cleanup branches.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_m3d_cleanup_database(
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
        unsafe { step(context, 2, 0) };
        unsafe { step(context, 3, 0) };
        return;
    }
    unsafe { step(context, 4, 0) };
    unsafe { step(context, 5, 0) };
    unsafe { step(context, 3, 0) };
    unsafe { step(context, 6, 0) };
}
