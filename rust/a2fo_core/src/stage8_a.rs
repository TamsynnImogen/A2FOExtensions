//! Portable policy for the stage-eight native module adapters.

use alloc::vec::Vec;
use core::ffi::c_void;

type Lower = unsafe extern "C" fn(*mut c_void, u8) -> u8;
type Space = unsafe extern "C" fn(*mut c_void, u8) -> u8;
type Include = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type Load = unsafe extern "C" fn(*mut c_void, Bytes, *mut Bytes) -> u8;
type ShieldStep = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
type ClassStep = unsafe extern "C" fn(*mut c_void, u32, usize, *mut Bytes) -> usize;
type IndexStep =
    unsafe extern "C" fn(*mut c_void, u32, u32, Bytes, Bytes, *mut Bytes, *mut u32) -> u8;

unsafe extern "C" {
    fn a2fo_rs_lead_shield_action(configured: u8, current: f32, effect: i32) -> u32;
    fn a2fo_rs_lead_scan_due(now: u32, last: u32, scanned: u8, interval: u32) -> u8;
}

#[repr(C)]
#[derive(Default)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}

fn trim(mut value: &[u8]) -> &[u8] {
    while value.first().is_some_and(|b| b" \t\r\n".contains(b)) {
        value = &value[1..];
    }
    while value.last().is_some_and(|b| b" \t\r\n".contains(b)) {
        value = &value[..value.len() - 1];
    }
    value
}

fn lowercase(value: &[u8], lower: Lower, context: *mut c_void) -> Vec<u8> {
    value
        .iter()
        .map(|byte| unsafe { lower(context, *byte) })
        .collect()
}

fn normalize(value: &[u8], lower: Lower, context: *mut c_void) -> Vec<u8> {
    let value = trim(value);
    let start = value
        .iter()
        .rposition(|byte| *byte == b'/' || *byte == b'\\')
        .map_or(0, |index| index + 1);
    let mut result = lowercase(&value[start..], lower, context);
    if !(result.is_empty() || result.len() > 4 && result.ends_with(b".odf")) {
        result.extend_from_slice(b".odf");
    }
    result
}

fn parse_bool(value: &[u8], lower: Lower, space: Space, context: *mut c_void) -> Option<bool> {
    let mut value = lowercase(trim(value), lower, context);
    if value.last() == Some(&b';') {
        value.pop();
        value = trim(&value).to_vec();
    }
    match value.as_slice() {
        b"true" | b"yes" | b"on" => return Some(true),
        b"false" | b"no" | b"off" => return Some(false),
        _ => {}
    }
    // strtol in the native policy stops at the first NUL and accepts trailing
    // CRT whitespace. The truth value only depends on whether any parsed digit
    // is nonzero, including values outside native long's range.
    let nul = value
        .iter()
        .position(|byte| *byte == 0)
        .unwrap_or(value.len());
    let value = &value[..nul];
    let mut index = 0;
    while value
        .get(index)
        .is_some_and(|b| unsafe { space(context, *b) } != 0)
    {
        index += 1;
    }
    if value.get(index).is_some_and(|b| *b == b'+' || *b == b'-') {
        index += 1;
    }
    let digits = index;
    let mut nonzero = false;
    while let Some(byte @ b'0'..=b'9') = value.get(index) {
        nonzero |= *byte != b'0';
        index += 1;
    }
    if index == digits
        || !value[index..]
            .iter()
            .all(|b| unsafe { space(context, *b) } != 0)
    {
        return None;
    }
    Some(nonzero)
}

fn strip_comment(line: &[u8]) -> &[u8] {
    let mut quoted = false;
    for index in 0..line.len().saturating_sub(1) {
        if line[index] == b'"' {
            quoted = !quoted;
        }
        if !quoted && line[index] == b'/' && line[index + 1] == b'/' {
            return &line[..index];
        }
    }
    line
}

/// Safety: borrowed byte views and callbacks are valid for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_normalize(
    input: *const u8,
    input_size: usize,
    output: *mut u8,
    output_capacity: usize,
    lower: Option<Lower>,
    context: *mut c_void,
) -> usize {
    let (Some(lower), Some(input)) = (lower, unsafe { bytes(input, input_size) }) else {
        return 0;
    };
    let result = normalize(input, lower, context);
    if !output.is_null() && output_capacity > result.len() {
        unsafe {
            core::ptr::copy_nonoverlapping(result.as_ptr(), output, result.len());
            output.add(result.len()).write(0);
        }
    }
    result.len()
}

/// Safety: borrowed byte views and callbacks are valid for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_parse(
    contents: *const u8,
    contents_size: usize,
    lower: Option<Lower>,
    space: Option<Space>,
    include: Option<Include>,
    context: *mut c_void,
) -> u8 {
    let (Some(lower), Some(space), Some(contents)) =
        (lower, space, unsafe { bytes(contents, contents_size) })
    else {
        return 0;
    };
    let mut result = None;
    for raw in contents.split(|byte| *byte == b'\n') {
        let line = trim(strip_comment(raw));
        if line.is_empty() {
            continue;
        }
        if lowercase(line, lower, context).starts_with(b"#include") {
            if let (Some(first), Some(last)) = (
                line.iter().position(|byte| *byte == b'"'),
                line.iter().rposition(|byte| *byte == b'"'),
            ) && last > first
                && let Some(include) = include
            {
                let key = normalize(&line[first + 1..last], lower, context);
                unsafe { include(context, key.as_ptr(), key.len()) };
            }
            continue;
        }
        let Some(equals) = line.iter().position(|byte| *byte == b'=') else {
            continue;
        };
        if lowercase(trim(&line[..equals]), lower, context) != b"alwaysshowshields" {
            continue;
        }
        if let Some(parsed) = parse_bool(&line[equals + 1..], lower, space, context) {
            result = Some(parsed);
        }
    }
    match result {
        None => 0,
        Some(false) => 1,
        Some(true) => 2,
    }
}

unsafe extern "C" fn collect_include(context: *mut c_void, name: *const u8, size: usize) {
    let includes = unsafe { &mut *(context as *mut Vec<Vec<u8>>) };
    if let Some(name) = unsafe { bytes(name, size) } {
        includes.push(name.to_vec());
    }
}

/// Safety: callbacks and borrowed byte views remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_resolve(
    name: *const u8,
    name_size: usize,
    lower: Option<Lower>,
    space: Option<Space>,
    load: Option<Load>,
    context: *mut c_void,
) -> u8 {
    let (Some(lower), Some(space), Some(load), Some(name)) =
        (lower, space, load, unsafe { bytes(name, name_size) })
    else {
        return 0;
    };
    unsafe fn visit(
        name: &[u8],
        lower: Lower,
        space: Space,
        load: Load,
        context: *mut c_void,
        visiting: &mut Vec<Vec<u8>>,
    ) -> u8 {
        let key = normalize(name, lower, context);
        if key.is_empty() || visiting.contains(&key) {
            return 0;
        }
        visiting.push(key.clone());
        let mut contents = Bytes::default();
        let found = unsafe {
            load(
                context,
                Bytes {
                    data: key.as_ptr(),
                    size: key.len(),
                },
                &mut contents,
            )
        };
        if found == 0 {
            visiting.pop();
            return 0;
        }
        let mut includes = Vec::<Vec<u8>>::new();
        let local = unsafe {
            a2fo_rs_stage8_shield_parse(
                contents.data,
                contents.size,
                Some(lower),
                Some(space),
                Some(collect_include),
                &mut includes as *mut Vec<Vec<u8>> as *mut c_void,
            )
        };
        if local != 0 {
            visiting.pop();
            return local;
        }
        for include in includes.iter().rev() {
            let inherited = unsafe { visit(include, lower, space, load, context, visiting) };
            if inherited != 0 {
                visiting.pop();
                return inherited;
            }
        }
        visiting.pop();
        0
    }
    unsafe { visit(name, lower, space, load, context, &mut Vec::new()) }
}

/// Safety: the native craft and callback remain valid for this synchronous pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_update(
    ready: u8,
    craft: usize,
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if ready == 0 || unsafe { step(context, 0, 0) } == 0 || craft == 0 {
        return;
    }
    let class = unsafe { step(context, 1, craft) };
    if unsafe { step(context, 2, class) } == 0 {
        return;
    }
    let shields = f32::from_bits(unsafe { step(context, 3, craft) } as u32);
    unsafe { step(context, 4, class) };
    let effect = unsafe { step(context, 5, craft) } as u32 as i32;
    match unsafe { a2fo_rs_lead_shield_action(1, shields, effect) } {
        1 => {
            unsafe { step(context, 6, craft) };
        }
        2 => {
            unsafe { step(context, 7, craft) };
        }
        _ => {}
    }
}

/// Safety: callback bytes remain valid until this synchronous call returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_class_policy(
    object_class: usize,
    step: Option<ClassStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if object_class == 0 {
        return 0;
    }
    let mut name = Bytes::default();
    let enabled = unsafe { step(context, 0, object_class, &mut name) };
    if enabled == usize::MAX {
        return 0;
    }
    if enabled != 0 {
        return 1;
    }
    let resolved = unsafe { step(context, 1, object_class, &mut name) };
    if resolved != 0 {
        return 0;
    }
    if unsafe { step(context, 2, object_class, &mut name) } == usize::MAX {
        return 0;
    }
    if unsafe { step(context, 3, object_class, &mut name) } == usize::MAX {
        return 0;
    }
    let Some(name) = (unsafe { bytes(name.data, name.size) }) else {
        return 0;
    };
    let name = name.to_vec();
    let mut view = view(&name);
    let result = unsafe { step(context, 4, object_class, &mut view) };
    if result == usize::MAX {
        return 0;
    }
    if result != 0 && unsafe { step(context, 5, object_class, &mut view) } == usize::MAX {
        return 0;
    }
    u8::from(result != 0)
}

/// Safety: value and callbacks remain valid for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_parameter(
    value: *const u8,
    value_size: usize,
    lower: Option<Lower>,
    space: Option<Space>,
    context: *mut c_void,
) -> u8 {
    let (Some(lower), Some(space), Some(value)) =
        (lower, space, unsafe { bytes(value, value_size) })
    else {
        return 0;
    };
    let value = lowercase(value, lower, context);
    let value = trim(&value);
    if value.is_empty() {
        return 0;
    }
    if matches!(value, b"true" | b"yes" | b"on") {
        return 1;
    }
    let mut index = 0;
    while value
        .get(index)
        .is_some_and(|b| unsafe { space(context, *b) } != 0)
    {
        index += 1;
    }
    if value.get(index).is_some_and(|b| *b == b'+' || *b == b'-') {
        index += 1;
    }
    let mut nonzero = false;
    while let Some(byte @ b'0'..=b'9') = value.get(index) {
        nonzero |= *byte != b'0';
        index += 1;
    }
    u8::from(nonzero)
}

/// Safety: callbacks contain native exceptions and remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_register(
    ready: u8,
    object_class: usize,
    parameter_db: usize,
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if ready == 0 || object_class == 0 || parameter_db == 0 {
        return;
    }
    if unsafe { step(context, 0, parameter_db) } != 0 {
        unsafe { step(context, 1, object_class) };
    } else {
        unsafe { step(context, 2, object_class) };
    }
}

/// Safety: native list reads and updates are synchronous callback operations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_scan(
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let owner = unsafe { step(context, 0, 0) };
    if owner == usize::MAX {
        return;
    }
    let sentinel = unsafe { step(context, 1, owner) };
    if sentinel == 0 {
        return;
    }
    let mut node = unsafe { step(context, 2, sentinel) };
    let mut visited = 0;
    while node != 0 && node != sentinel && visited < 65536 {
        let next = unsafe { step(context, 3, node) };
        let object = unsafe { step(context, 4, node) };
        if object != 0 {
            unsafe { step(context, 5, object) };
        }
        visited += 1;
        if next == node {
            break;
        }
        node = next;
    }
    unsafe { step(context, 6, visited) };
}

/// Safety: native effect snapshot is caller-owned for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_activation(
    activation: u64,
    previous: u64,
    step: Option<ShieldStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if activation != previous {
        unsafe { step(context, 0, 0) }; // Store generation before cleanup.
        let count = unsafe { step(context, 1, 0) }; // Move effect map to native snapshot.
        for index in 0..count {
            let id = unsafe { step(context, 2, index) } as u32 as i32;
            unsafe { step(context, 3, id as u32 as usize) };
        }
        unsafe { step(context, 4, 0) }; // Reset observed/scan state.
    }
    u8::from(activation != 0)
}

/// Safety: native callbacks contain exceptions and preserve borrowed API state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_init(
    step: Option<ShieldStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    unsafe fn call(step: ShieldStep, context: *mut c_void, operation: u32) -> Option<bool> {
        let result = unsafe { step(context, operation, 0) };
        (result != usize::MAX).then_some(result != 0)
    }
    if !matches!(unsafe { call(step, context, 0) }, Some(true)) {
        return 0;
    }
    match unsafe { call(step, context, 1) } {
        Some(true) => {}
        Some(false) => {
            unsafe { call(step, context, 2) };
            return 0;
        }
        None => return 0,
    }
    match unsafe { call(step, context, 3) } {
        Some(true) => {}
        Some(false) => {
            unsafe { call(step, context, 2) };
            return 0;
        }
        None => return 0,
    }
    for (install, failure_log) in [(4, 5), (6, 7), (8, 9)] {
        match unsafe { call(step, context, install) } {
            Some(true) => {}
            Some(false) => {
                if unsafe { call(step, context, failure_log) }.is_none() {
                    return 0;
                }
                return 1;
            }
            None => return 0,
        }
    }
    if unsafe { call(step, context, 10) }.is_none() {
        return 0;
    }
    u8::from(unsafe { call(step, context, 11) }.is_some())
}

/// Safety: native state remains owned by the caller throughout this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_shutdown(
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    for operation in 0..=11 {
        unsafe { step(context, operation, 0) };
    }
}

/// Safety: the native gateway and state callbacks are synchronous.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_render(
    ready: u8,
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if ready != 0 && unsafe { step(context, 0, 0) } != 0 {
        let now = unsafe { step(context, 1, 0) } as u32;
        let interval = if unsafe { step(context, 2, 0) } != 0 {
            1000
        } else {
            100
        };
        let last = unsafe { step(context, 3, 0) } as u32;
        let scanned = unsafe { step(context, 4, 0) } != 0;
        if unsafe { a2fo_rs_lead_scan_due(now, last, u8::from(scanned), interval) } != 0 {
            unsafe { step(context, 5, now as usize) };
        }
    }
    unsafe { step(context, 6, 0) };
}

/// Safety: native craft and storage callbacks remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_cleanup(
    ready: u8,
    craft: usize,
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if ready == 0 || craft == 0 {
        return;
    }
    for operation in 0..3 {
        unsafe { step(context, operation, craft) };
    }
}

/// Safety: native effect and map callbacks remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_create(
    craft: usize,
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let effect = unsafe { step(context, 0, craft) } as u32 as i32;
    if effect < 0 {
        if unsafe { step(context, 1, craft) } == 1 {
            unsafe { step(context, 2, craft) };
        }
        return;
    }
    let inserted = unsafe { step(context, 3, craft) };
    if inserted == usize::MAX {
        unsafe { step(context, 4, effect as u32 as usize) };
        unsafe { step(context, 7, craft) };
        return;
    }
    if inserted == 0 {
        unsafe { step(context, 4, effect as u32 as usize) };
        return;
    }
    if unsafe { step(context, 5, craft) } == usize::MAX
        || unsafe { step(context, 6, craft) } == usize::MAX
    {
        unsafe { step(context, 4, effect as u32 as usize) };
        unsafe { step(context, 7, craft) };
    }
}

/// Safety: native map and effect callbacks remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_remove(
    craft: usize,
    step: Option<ShieldStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, craft) } == 0 {
        return;
    }
    let effect = unsafe { step(context, 1, craft) };
    unsafe { step(context, 2, effect) };
}

/// Safety: native signature and checked patch callbacks contain exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_install_publish(
    step: Option<ShieldStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        if unsafe { step(context, 2, 0) } == 0 {
            return 0;
        }
        return u8::from(unsafe { step(context, 3, 0) } != 0);
    }
    if unsafe { step(context, 4, 0) } == 0 || unsafe { step(context, 5, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 6, 0) };
    1
}

/// Safety: the gateway callback executes first and retains its native ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_after_gateway(
    object: usize,
    require_object: u8,
    step: Option<ShieldStep>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    let result = unsafe { step(context, 0, 0) };
    if unsafe { step(context, 1, 0) } != 0 && (require_object == 0 || object != 0) {
        unsafe { step(context, 2, object) };
    }
    result
}

fn join_path(left: &[u8], right: &[u8]) -> Vec<u8> {
    if left.is_empty() {
        return right.to_vec();
    }
    if right.is_empty() {
        return left.to_vec();
    }
    let mut result = left.to_vec();
    if !matches!(left.last(), Some(b'\\' | b'/')) {
        result.push(b'\\');
    }
    result.extend_from_slice(right);
    result
}

fn view(bytes: &[u8]) -> Bytes {
    Bytes {
        data: bytes.as_ptr(),
        size: bytes.len(),
    }
}

/// Safety: callback outputs are borrowed only until the next native call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_shield_index(
    lower: Option<Lower>,
    step: Option<IndexStep>,
    context: *mut c_void,
) -> u8 {
    let (Some(lower), Some(step)) = (lower, step) else {
        return 0;
    };
    unsafe fn call(
        step: IndexStep,
        context: *mut c_void,
        operation: u32,
        index: u32,
        first: &[u8],
        second: &[u8],
    ) -> Option<(Vec<u8>, u32)> {
        let mut output = Bytes::default();
        let mut value = 0;
        if unsafe {
            step(
                context,
                operation,
                index,
                view(first),
                view(second),
                &mut output,
                &mut value,
            )
        } == 0
        {
            return None;
        }
        Some((unsafe { bytes(output.data, output.size) }?.to_vec(), value))
    }
    unsafe fn directory(
        path: &[u8],
        depth: usize,
        lower: Lower,
        step: IndexStep,
        context: *mut c_void,
    ) -> bool {
        if path.is_empty() || depth > 24 {
            return true;
        }
        let Some((_, opened)) = (unsafe { call(step, context, 2, 0, path, &[]) }) else {
            return false;
        };
        if opened == 0 {
            return true;
        }
        loop {
            let Some((name, attributes)) = (unsafe { call(step, context, 3, 0, &[], &[]) }) else {
                return false;
            };
            let is_directory = attributes & 0x10 != 0;
            if name != b"." && name != b".." && !(is_directory && attributes & 0x400 != 0) {
                let odf = if is_directory {
                    false
                } else {
                    let key = lowercase(&name, lower, context);
                    key.len() > 4 && key.ends_with(b".odf")
                };
                if (is_directory || odf) && unsafe { call(step, context, 4, 0, &[], &[]) }.is_none()
                {
                    return false;
                }
            }
            let Some((_, has_next)) = (unsafe { call(step, context, 8, 0, &[], &[]) }) else {
                return false;
            };
            if has_next == 0 {
                break;
            }
        }
        let Some((_, selected)) = (unsafe { call(step, context, 5, 0, &[], &[]) }) else {
            return false;
        };
        let mut children = Vec::with_capacity(selected as usize);
        for index in 0..selected {
            let Some(child) = (unsafe { call(step, context, 6, index, &[], &[]) }) else {
                return false;
            };
            children.push(child);
        }
        for (name, attributes) in children {
            let child_path = join_path(path, &name);
            if attributes & 0x10 != 0 {
                if !unsafe { directory(&child_path, depth + 1, lower, step, context) } {
                    return false;
                }
            } else {
                let key = lowercase(&name, lower, context);
                if unsafe { call(step, context, 7, 0, &key, &child_path) }.is_none() {
                    return false;
                }
            }
        }
        true
    }
    let Some((_, count)) = (unsafe { call(step, context, 0, 0, &[], &[]) }) else {
        return 0;
    };
    for index in 0..count {
        let Some((root, _)) = (unsafe { call(step, context, 1, index, &[], &[]) }) else {
            return 0;
        };
        if !root.is_empty()
            && !unsafe { directory(&join_path(&root, b"odf"), 0, lower, step, context) }
        {
            return 0;
        }
    }
    1
}

unsafe fn bytes<'a>(pointer: *const u8, size: usize) -> Option<&'a [u8]> {
    if pointer.is_null() && size != 0 {
        return None;
    }
    Some(if size == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(pointer, size) }
    })
}
