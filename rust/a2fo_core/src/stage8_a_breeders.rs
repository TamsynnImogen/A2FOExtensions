//! Portable breeder class metadata workflow.

use alloc::vec::Vec;
use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}

#[repr(C)]
pub struct Policy {
    breeder: u8,
    mother: u8,
    mother_radius: f32,
    consume_radius: f32,
    seed: Bytes,
    result: Bytes,
    produces: Bytes,
}

pub(crate) type Lower = unsafe extern "C" fn(*mut c_void, u8) -> u8;
type Field = unsafe extern "C" fn(*mut c_void, u32, *mut Bytes, *mut Bytes) -> u8;
type Float = unsafe extern "C" fn(*mut c_void, Bytes, *mut f32) -> u8;
type Store = unsafe extern "C" fn(*mut c_void, usize, *const Policy) -> u8;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
type Emit = unsafe extern "C" fn(*mut c_void, *const u8, usize);

pub(crate) fn trim(mut input: &[u8]) -> &[u8] {
    while input.first().is_some_and(|b| b" \t\r\n".contains(b)) {
        input = &input[1..];
    }
    while input.last().is_some_and(|b| b" \t\r\n".contains(b)) {
        input = &input[..input.len() - 1];
    }
    input
}

pub(crate) fn unquote(input: &[u8]) -> &[u8] {
    let input = trim(input);
    if input.len() >= 2
        && matches!(
            (input[0], input[input.len() - 1]),
            (b'"', b'"') | (b'\'', b'\'')
        )
    {
        &input[1..input.len() - 1]
    } else {
        input
    }
}

pub(crate) fn lower_bytes(input: &[u8], lower: Lower, context: *mut c_void) -> Vec<u8> {
    input
        .iter()
        .map(|b| unsafe { lower(context, *b) })
        .collect()
}

fn boolean(input: &[u8], lower: Lower, context: *mut c_void) -> bool {
    matches!(
        lower_bytes(unquote(input), lower, context).as_slice(),
        b"1" | b"true" | b"yes" | b"on"
    )
}

pub(crate) fn odf(input: &[u8], lower: Lower, context: *mut c_void) -> Vec<u8> {
    let mut key = lower_bytes(unquote(input), lower, context);
    for byte in &mut key {
        if *byte == b'/' {
            *byte = b'\\';
        }
    }
    if let Some(slash) = key.iter().rposition(|b| *b == b'\\') {
        key.drain(..=slash);
    }
    if key.ends_with(b".odf") {
        key.truncate(key.len() - 4);
    }
    key
}

unsafe fn bytes<'a>(view: Bytes) -> Option<&'a [u8]> {
    if view.data.is_null() && view.size != 0 {
        return None;
    }
    Some(if view.size == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(view.data, view.size) }
    })
}

fn view(input: &[u8]) -> Bytes {
    Bytes {
        data: input.as_ptr(),
        size: input.len(),
    }
}

/// Safety: input and callback stay valid synchronously; emit contains exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_odf_key(
    value: *const u8,
    size: usize,
    lower: Option<Lower>,
    emit: Option<Emit>,
    context: *mut c_void,
) {
    let (Some(lower), Some(emit)) = (lower, emit) else {
        return;
    };
    let Some(value) = (unsafe { bytes(Bytes { data: value, size }) }) else {
        return;
    };
    let result = odf(value, lower, context);
    unsafe { emit(context, result.as_ptr(), result.len()) };
}

/// Safety: field views remain valid synchronously; native callbacks contain exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_class_loaded(
    object_class: usize,
    field_count: u32,
    lower: Option<Lower>,
    field: Option<Field>,
    parse_float: Option<Float>,
    store: Option<Store>,
    context: *mut c_void,
) {
    let (Some(lower), Some(field), Some(parse_float), Some(store)) =
        (lower, field, parse_float, store)
    else {
        return;
    };
    unsafe fn find(
        wanted: &[u8],
        count: u32,
        lower: Lower,
        field: Field,
        context: *mut c_void,
    ) -> Vec<u8> {
        for index in 0..count {
            let mut name = Bytes::default();
            let mut value = Bytes::default();
            if unsafe { field(context, index, &mut name, &mut value) } == 0 {
                continue;
            }
            let (Some(name), Some(value)) = (unsafe { bytes(name) }, unsafe { bytes(value) })
            else {
                continue;
            };
            if name.len() != wanted.len() {
                continue;
            }
            if name.iter().zip(wanted).all(|(left, right)|
                unsafe { lower(context, *left) } == unsafe { lower(context, *right) })
            {
                return value.to_vec();
            }
        }
        Vec::new()
    }
    let breeder = boolean(
        &unsafe { find(b"breederClass", field_count, lower, field, context) },
        lower,
        context,
    );
    let mother = boolean(
        &unsafe { find(b"breederMother", field_count, lower, field, context) },
        lower,
        context,
    );
    let radius = unsafe { find(b"breederRadius", field_count, lower, field, context) };
    let mut mother_radius = 600.0;
    let radius = unquote(&radius);
    if !radius.is_empty() {
        let mut parsed = 0.0;
        let status = unsafe { parse_float(context, view(radius), &mut parsed) };
        if status == 2 {
            return;
        }
        if status == 1 && parsed.is_finite() && parsed > 0.0 {
            mother_radius = parsed;
        }
    }
    let consume = unsafe { find(b"breederConsumeRadius", field_count, lower, field, context) };
    let mut consume_radius = 40.0;
    let consume = unquote(&consume);
    if !consume.is_empty() {
        let mut parsed = 0.0;
        let status = unsafe { parse_float(context, view(consume), &mut parsed) };
        if status == 2 {
            return;
        }
        if status == 1 && parsed.is_finite() && parsed > 0.0 {
            consume_radius = parsed;
        }
    }
    let seed = odf(
        &unsafe { find(b"breederSeed", field_count, lower, field, context) },
        lower,
        context,
    );
    let result = odf(
        &unsafe { find(b"breederResult", field_count, lower, field, context) },
        lower,
        context,
    );
    let produces = odf(
        &unsafe { find(b"breederProduces", field_count, lower, field, context) },
        lower,
        context,
    );
    if !breeder && !mother && seed.is_empty() && result.is_empty() && produces.is_empty() {
        unsafe { store(context, object_class, core::ptr::null()) };
        return;
    }
    let policy = Policy {
        breeder: u8::from(breeder),
        mother: u8::from(mother),
        mother_radius,
        consume_radius,
        seed: view(&seed),
        result: view(&result),
        produces: view(&produces),
    };
    unsafe { store(context, object_class, &policy) };
}

/// Safety: native callbacks contain exceptions; API storage stays native.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    unsafe fn call(step: Step, context: *mut c_void, operation: u32) -> Option<bool> {
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
    let mut all_registered = true;
    for operation in 3..=6 {
        match unsafe { call(step, context, operation) } {
            Some(result) => all_registered &= result,
            None => return 0,
        }
    }
    let masked = match unsafe { call(step, context, 7) } {
        Some(value) => value,
        None => return 0,
    };
    match unsafe { call(step, context, if masked { 8 } else { 9 }) } {
        Some(result) => all_registered &= result,
        None => return 0,
    }
    if !all_registered {
        unsafe { call(step, context, 10) };
        return 0;
    }
    u8::from(unsafe { call(step, context, 11) }.is_some())
}

/// Safety: caller-owned native maps and module handles remain valid for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_shutdown(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    for operation in 0..7 {
        unsafe { step(context, operation, 0) };
    }
}

/// Safety: callback reads each SDK field only after the original admission gate.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_producer_event(
    event: usize,
    required_size: u32,
    admit_kind: u32,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 1;
    };
    if unsafe { step(context, 0, 0) } == 0 || event == 0 {
        return 1;
    }
    if (unsafe { step(context, 1, event) } as u32) < required_size {
        return 1;
    }
    if unsafe { step(context, 2, event) } == 0 || unsafe { step(context, 3, 0) } == 0 {
        return 1;
    }
    if unsafe { step(context, 4, event) } as u32 != admit_kind {
        return 1;
    }
    if unsafe { step(context, 5, event) } == 0 {
        return 1;
    }
    match unsafe { step(context, 6, event) } {
        0 => return 1,
        2 => return 0,
        _ => {}
    }
    let handle = unsafe { step(context, 7, event) };
    if handle == 0 || unsafe { step(context, 8, handle) } != 0 {
        return 0;
    }
    if unsafe { step(context, 9, event) } == 0 {
        unsafe { step(context, 10, 0) };
        return 0;
    }
    unsafe { step(context, 11, event) };
    0
}

/// Safety: native generation storage callbacks remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_activation(
    current: u64,
    previous: u64,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if current != previous {
        for operation in 0..3 {
            unsafe { step(context, operation, 0) };
        }
    }
    u8::from(current != 0)
}

/// Safety: native SDK reads happen only after admission; maps stay native.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_craft_event(
    event: usize,
    required_size: u32,
    cleanup_kind: u32,
    postload_kind: u32,
    simulate_kind: u32,
    builtin: u8,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let enabled = unsafe { step(context, 0, 0) } != 0;
    if event == 0 || (unsafe { step(context, 1, event) } as u32) < required_size {
        return;
    }
    if unsafe { step(context, 2, event) } == 0 || unsafe { step(context, 3, 0) } == 0 {
        return;
    }
    let handle = unsafe { step(context, 4, event) };
    let kind = unsafe { step(context, 5, event) } as u32;
    if builtin != 0 && (!enabled || kind == postload_kind) {
        if kind == cleanup_kind || kind == postload_kind {
            unsafe { step(context, 6, handle) };
            unsafe { step(context, 7, handle) };
        }
        return;
    }
    if kind == cleanup_kind {
        if unsafe { step(context, 8, handle) } != 0 {
            let seed = unsafe { step(context, 9, handle) };
            if seed != 0 {
                unsafe { step(context, 10, seed) };
            }
            unsafe { step(context, 11, handle) };
        }
        unsafe { step(context, 7, handle) };
        return;
    }
    if kind != simulate_kind || unsafe { step(context, 12, event) } != 0 {
        return;
    }
    match unsafe { step(context, 13, event) } {
        1 => {
            unsafe { step(context, 14, event) };
        }
        2 => {
            unsafe { step(context, 15, event) };
        }
        _ => {}
    }
}

/// Safety: native policy and transform storage is caller-owned throughout.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_begin_request(
    breeder: usize,
    target_class: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if breeder == 0 || target_class == 0 || unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    let handle = unsafe { step(context, 1, breeder) };
    if handle == 0 || unsafe { step(context, 2, handle) } != 0 {
        return 0;
    }
    let mother = unsafe { step(context, 3, breeder) };
    if mother == 0 {
        unsafe { step(context, 4, 0) };
        return 0;
    }
    let seed_class = unsafe { step(context, 5, 0) };
    let result_class = unsafe { step(context, 6, 0) };
    let result_policy = unsafe { step(context, 7, result_class) } != 0;
    let produces_class = if result_policy {
        unsafe { step(context, 8, 0) }
    } else {
        0
    };
    if seed_class == 0
        || result_class == 0
        || !result_policy
        || unsafe { step(context, 9, 0) } == 0
        || produces_class == 0
        || produces_class != target_class
    {
        unsafe { step(context, 10, 0) };
        return 0;
    }
    if unsafe { step(context, 11, mother) } == 0 {
        unsafe { step(context, 12, 0) };
        return 0;
    }
    let seed = unsafe { step(context, 13, seed_class) };
    if seed == 0 {
        unsafe { step(context, 14, 0) };
        return 0;
    }
    let seed_handle = unsafe { step(context, 15, seed) };
    if seed_handle == 0 {
        unsafe { step(context, 16, seed) };
        unsafe { step(context, 17, 0) };
        return 0;
    }
    unsafe { step(context, 18, seed_handle) };
    unsafe { step(context, 19, seed) };
    unsafe { step(context, 20, handle) };
    1
}

/// Safety: native list reads, numeric comparisons, and class storage stay native.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_nearest_mother(
    breeder: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if breeder == 0 || unsafe { step(context, 0, breeder) } != 0 {
        return 0;
    }
    let team = unsafe { step(context, 1, breeder) } as u32 as i32;
    unsafe { step(context, 2, breeder) };
    let head = unsafe { step(context, 3, 0) };
    if head == 0 {
        return 0;
    }
    let count = unsafe { step(context, 4, 0) } as u32;
    let mut node = unsafe { step(context, 5, head) };
    let mut best = 0;
    let mut best_handle = 0;
    for _ in 0..count {
        if node == 0 || node == head {
            break;
        }
        let next = unsafe { step(context, 6, node) };
        let candidate = unsafe { step(context, 7, node) };
        if candidate != 0
            && unsafe { step(context, 8, candidate) } == 0
            && candidate != breeder
            && unsafe { step(context, 9, candidate) } as u32 as i32 == team
            && unsafe { step(context, 10, candidate) } != 0
            && unsafe { step(context, 11, candidate) } != 0
        {
            let handle = unsafe { step(context, 12, candidate) } as u32;
            let comparison = if best == 0 {
                0
            } else {
                unsafe { step(context, 13, best) }
            };
            if best == 0 || comparison & 1 != 0 || (comparison & 2 != 0 && handle < best_handle) {
                best = candidate;
                best_handle = handle;
                unsafe { step(context, 14, candidate) };
            }
        }
        if next == node {
            break;
        }
        node = next;
    }
    best
}

/// Safety: native waiting snapshot and engine operations stay caller-owned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_metamorphose(
    breeder: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let seed = unsafe { step(context, 0, 0) };
    if breeder == 0
        || seed == 0
        || unsafe { step(context, 1, breeder) } != 0
        || unsafe { step(context, 2, seed) } != 0
    {
        return 0;
    }
    let result_class = unsafe { step(context, 3, 0) };
    let target_class = unsafe { step(context, 4, 0) };
    let result_policy = unsafe { step(context, 5, result_class) } != 0;
    if result_class == 0
        || target_class == 0
        || !result_policy
        || unsafe { step(context, 6, 0) } == 0
        || unsafe { step(context, 7, 0) } != target_class
    {
        unsafe { step(context, 8, 0) };
        return 0;
    }
    if unsafe { step(context, 9, breeder) } == 0 {
        return 0;
    }
    let replacement = unsafe { step(context, 10, result_class) };
    if replacement == 0 {
        unsafe { step(context, 11, 0) };
        return 0;
    }
    unsafe { step(context, 12, replacement) };
    unsafe { step(context, 13, breeder) };
    unsafe { step(context, 14, seed) };
    unsafe { step(context, 15, 0) };
    1
}

/// Safety: native waiting iterator and committed snapshot stay caller-owned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_update_waiting(
    breeder: usize,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let handle = unsafe { step(context, 0, breeder) };
    if unsafe { step(context, 1, handle) } == 0 {
        return;
    }
    let seed = unsafe { step(context, 2, handle) };
    if seed == 0 || unsafe { step(context, 3, seed) } != 0 {
        unsafe { step(context, 4, 0) };
        unsafe { step(context, 5, handle) };
        return;
    }
    unsafe { step(context, 6, breeder) };
    if unsafe { step(context, 7, seed) } != 0 {
        // Erase before the native swap, which can reenter CLEANUP.
        if unsafe { step(context, 8, handle) } == usize::MAX {
            return;
        }
        unsafe { step(context, 5, handle) };
        if unsafe { step(context, 9, breeder) } == 0 && unsafe { step(context, 10, breeder) } == 0 {
            unsafe { step(context, 11, handle) };
        }
        return;
    }
    if unsafe { step(context, 12, 0) } != 0 {
        unsafe { step(context, 13, 0) };
        unsafe { step(context, 14, seed) };
    }
}

/// Safety: native policy and retry-map callbacks remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_breeder_update_active(
    breeder: usize,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0
        || unsafe { step(context, 1, breeder) } == 0
        || unsafe { step(context, 2, breeder) } == 0
        || unsafe { step(context, 3, breeder) } == 0
    {
        return;
    }
    let handle = unsafe { step(context, 4, breeder) };
    let retry = unsafe { step(context, 5, handle) };
    if retry == usize::MAX || retry != 0 {
        return;
    }
    unsafe { step(context, 6, handle) };
    let target = unsafe { step(context, 7, 0) };
    if target == 0 {
        return;
    }
    if unsafe { step(context, 8, 0) } == 0 {
        return;
    }
    let before = unsafe { step(context, 9, breeder) } as u32;
    unsafe { step(context, 10, target) };
    let after = unsafe { step(context, 11, breeder) } as u32;
    let current = unsafe { step(context, 12, breeder) };
    if after > before || current == target {
        unsafe { step(context, 13, handle) };
    }
}
