//! Runtime sequencing for the optional directional shield module.

use alloc::vec::Vec;
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32) -> usize;
type EventStep = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
type Field = unsafe extern "C" fn(*mut c_void, u32, *mut Bytes, *mut Bytes) -> u8;
type Equal = unsafe extern "C" fn(*mut c_void, Bytes, Bytes) -> u8;
type Parse = unsafe extern "C" fn(*mut c_void, u32, *mut f32) -> u8;
type Numeric = unsafe extern "C" fn(*mut c_void, u32, *mut ClassResult) -> u8;
type Action = unsafe extern "C" fn(*mut c_void, u32, *const ClassResult) -> u8;
type Admit = unsafe extern "C" fn(*mut c_void, u32) -> usize;
type Lower = unsafe extern "C" fn(*mut c_void, u8) -> u8;

unsafe extern "C" {
    fn a2fo_rs_shields_active(mode: u32, facing: u32) -> u8;
    fn a2fo_rs_shields_normalize(mode: u32, maximum: *mut f32);
    fn a2fo_rs_shields_valid(mode: u32, maximum: *const f32) -> u8;
    fn a2fo_rs_shields_total(maximum: *const f32) -> f32;
    fn a2fo_rs_shields_suppress(kind: i32, resolved: u8, remaining: f32) -> u8;
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ClassResult {
    mode: u32,
    maximum: [f32; 4],
    native_maximum: f32,
    explicit_total: f32,
    total: f32,
    current_value: f32,
    share: f32,
    automatic_count: u32,
    offending_index: u32,
    native_maximum_declared: u8,
}

fn view(bytes: &[u8]) -> Bytes {
    Bytes {
        data: bytes.as_ptr(),
        size: bytes.len(),
    }
}

unsafe fn slice<'a>(bytes: Bytes) -> Option<&'a [u8]> {
    if bytes.data.is_null() {
        return (bytes.size == 0).then_some(&[]);
    }
    Some(unsafe { core::slice::from_raw_parts(bytes.data, bytes.size) })
}

fn field_value(
    wanted: &[u8],
    count: u32,
    field: Field,
    equal: Equal,
    context: *mut c_void,
) -> Option<Vec<u8>> {
    for index in 0..count {
        let mut name = Bytes::default();
        let mut value = Bytes::default();
        if unsafe { field(context, index, &mut name, &mut value) } == 0 {
            continue;
        }
        let (Some(name_bytes), Some(value_bytes)) =
            (unsafe { slice(name) }, unsafe { slice(value) })
        else {
            continue;
        };
        if name_bytes.len() == wanted.len() && unsafe { equal(context, name, view(wanted)) } != 0 {
            return Some(value_bytes.to_vec());
        }
    }
    None
}

/// Result codes select the original native diagnostic and preserve native
/// numeric values for formatting. Zero means no diagnostic.
/// Safety: SDK field views are readable synchronously; callbacks retain all
/// native policy storage and contain exceptions at their original boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_class(
    event: usize,
    required_size: u32,
    admit: Option<Admit>,
    field: Option<Field>,
    equal: Option<Equal>,
    lower: Option<Lower>,
    parse: Option<Parse>,
    numeric: Option<Numeric>,
    action: Option<Action>,
    context: *mut c_void,
    output: *mut ClassResult,
) -> u32 {
    let (
        Some(admit),
        Some(field),
        Some(equal),
        Some(lower),
        Some(parse),
        Some(numeric),
        Some(action),
    ) = (admit, field, equal, lower, parse, numeric, action)
    else {
        return 0;
    };
    if unsafe { admit(context, 0) } == 0
        || event == 0
        || (unsafe { admit(context, 1) } as u32) < required_size
        || unsafe { admit(context, 2) } == 0
        || output.is_null()
    {
        return 0;
    }
    let field_count = unsafe { admit(context, 3) } as u32;
    let mut result = ClassResult::default();
    let mut mode_number = 0.0;
    // Retain the native unordered-comparison behavior for NaN inputs.
    #[allow(
        clippy::manual_range_contains,
        reason = "preserve native float comparisons"
    )]
    let mode = if unsafe { parse(context, 0, &mut mode_number) } != 0 {
        result.current_value = mode_number;
        if unsafe { numeric(context, 3, &mut result) } == 0
            || mode_number < 0.0
            || mode_number > 5.0
        {
            None
        } else {
            Some(mode_number as u32)
        }
    } else {
        let text = field_value(b"directionalShields", field_count, field, equal, context);
        text.and_then(|value| {
            let folded: Vec<u8> = value
                .iter()
                .map(|byte| unsafe { lower(context, *byte) })
                .collect();
            match folded.as_slice() {
                b"true" | b"yes" | b"on" => Some(1),
                b"false" | b"no" | b"off" => Some(0),
                _ => None,
            }
        })
    };
    let Some(mode) = mode else {
        return u32::from(
            field_value(b"directionalShields", field_count, field, equal, context).is_some(),
        );
    };
    if mode == 0 {
        return 0;
    }
    if !(1..=5).contains(&mode) {
        return 2;
    }
    result.mode = mode;
    let names: [&[u8]; 4] = [
        b"forwardShieldStrength",
        b"aftShieldStrength",
        b"portShieldStrength",
        b"starboardShieldStrength",
    ];
    let mut automatic = [false; 4];
    for index in 0..4 {
        if unsafe { a2fo_rs_shields_active(mode, index as u32) } == 0 {
            result.maximum[index] = 0.0;
            continue;
        }
        let declared = field_value(names[index], field_count, field, equal, context).is_some();
        let mut value = 0.0;
        if declared && unsafe { parse(context, index as u32 + 1, &mut value) } == 0 {
            result.offending_index = index as u32;
            unsafe { *output = result };
            return 3;
        }
        if !declared || value == 0.0 {
            automatic[index] = true;
            result.automatic_count += 1;
        } else {
            result.maximum[index] = value;
            result.current_value = value;
            unsafe { numeric(context, 0, &mut result) };
        }
    }
    result.native_maximum_declared =
        u8::from(field_value(b"maxShields", field_count, field, equal, context).is_some());
    if result.native_maximum_declared != 0
        && (unsafe { parse(context, 5, &mut result.native_maximum) } == 0
            || result.native_maximum <= 0.0)
    {
        unsafe { *output = result };
        return 4;
    }
    if result.automatic_count != 0 {
        if result.native_maximum_declared == 0 {
            unsafe { *output = result };
            return 5;
        }
        match unsafe { numeric(context, 1, &mut result) } {
            1 => {
                unsafe { *output = result };
                return 6;
            }
            2 => return 0,
            _ => {}
        }
        for (index, is_automatic) in automatic.iter().enumerate() {
            if *is_automatic {
                result.maximum[index] = result.share;
            }
        }
    }
    unsafe { a2fo_rs_shields_normalize(mode, result.maximum.as_mut_ptr()) };
    if unsafe { a2fo_rs_shields_valid(mode, result.maximum.as_ptr()) } == 0 {
        unsafe { *output = result };
        return 7;
    }
    result.total = unsafe { a2fo_rs_shields_total(result.maximum.as_ptr()) };
    if result.native_maximum_declared != 0 && unsafe { numeric(context, 2, &mut result) } != 0 {
        unsafe { *output = result };
        return 8;
    }
    if result.native_maximum_declared == 0 && unsafe { action(context, 0, &result) } == 0 {
        unsafe { *output = result };
        return 9;
    }
    if unsafe { action(context, 1, &result) } == 0 {
        unsafe { *output = result };
        return 10;
    }
    unsafe { *output = result };
    11
}

/// Safety: the native callback keeps API pointers and hook storage live for
/// this synchronous call, contains C++ exceptions, and uses `usize::MAX` as
/// its exception sentinel.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    unsafe fn call(step: Step, context: *mut c_void, operation: u32) -> Option<bool> {
        let value = unsafe { step(context, operation) };
        (value != usize::MAX).then_some(value != 0)
    }
    if !matches!(unsafe { call(step, context, 0) }, Some(true)) {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 1) }, Some(true)) {
        return 0;
    }
    for operation in 2..=6 {
        if !matches!(unsafe { call(step, context, operation) }, Some(true)) {
            return 0;
        }
    }
    if !matches!(unsafe { call(step, context, 7) }, Some(true))
        || !matches!(unsafe { call(step, context, 8) }, Some(true))
    {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 9) }, Some(true))
        || !matches!(unsafe { call(step, context, 10) }, Some(true))
    {
        return 1;
    }
    if !matches!(unsafe { call(step, context, 11) }, Some(true))
        || !matches!(unsafe { call(step, context, 12) }, Some(true))
    {
        return 1;
    }
    if unsafe { call(step, context, 13) }.is_none() {
        return 0;
    }
    let registered = match unsafe { call(step, context, 14) } {
        Some(true) => match unsafe { call(step, context, 15) } {
            Some(value) => value,
            None => return 0,
        },
        Some(false) => false,
        None => return 0,
    };
    if !registered {
        if unsafe { call(step, context, 16) }.is_none() {
            return 0;
        }
        return 1;
    }
    if unsafe { call(step, context, 17) }.is_none() {
        return 0;
    }
    match unsafe { call(step, context, 18) } {
        Some(false) => {
            if unsafe { call(step, context, 19) }.is_none() {
                return 0;
            }
        }
        None => return 0,
        Some(true) => {}
    }
    1
}

/// Safety: `event` remains live for the call. The callback reads SDK fields
/// only after their original admission and owns all policy and state maps.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_craft_event(
    event: usize,
    required_size: u32,
    cleanup_kind: u32,
    postload_kind: u32,
    simulate_pre_kind: u32,
    simulate_post_kind: u32,
    step: Option<EventStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if event == 0 || (unsafe { step(context, 0, event) } as u32) < required_size {
        return;
    }
    let craft = unsafe { step(context, 1, event) };
    if craft == 0 {
        return;
    }
    let kind = unsafe { step(context, 2, event) } as u32;
    if kind == cleanup_kind {
        for operation in 3..=5 {
            unsafe { step(context, operation, craft) };
        }
        return;
    }
    if unsafe { step(context, 6, 0) } == 0
        || unsafe { step(context, 7, 0) } == 0
        || unsafe { step(context, 8, 0) } == 0
    {
        return;
    }
    if kind == postload_kind {
        unsafe { step(context, 4, craft) };
        unsafe { step(context, 5, craft) };
        if unsafe { step(context, 9, craft) } == 0 {
            unsafe { step(context, 10, craft) };
        }
        return;
    }
    if unsafe { step(context, 11, craft) } == 0 {
        return;
    }
    if kind != simulate_pre_kind && kind != simulate_post_kind {
        return;
    }
    let state = unsafe { step(context, 12, craft) };
    unsafe { step(context, 13, state) };
}

/// Safety: the callback owns all native effect, policy, and Craft storage.
/// The original gateway and its temporary native shield presentation remain
/// within one C++ callback frame.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_create_hit(
    shield_type: i32,
    step: Option<EventStep>,
    context: *mut c_void,
) -> i32 {
    let Some(step) = step else {
        return -1;
    };
    let policy = if unsafe { step(context, 0, 0) } != 0 {
        unsafe { step(context, 1, 0) }
    } else {
        0
    };
    let state = if policy != 0 {
        unsafe { step(context, 2, 0) }
    } else {
        0
    };
    let mut facing = 0;
    let mut resolved = false;
    if policy != 0 && unsafe { step(context, 3, 0) } != 0 {
        facing = unsafe { step(context, 4, 0) } as u32;
        resolved = true;
    } else if shield_type == 0 && state != 0 && unsafe { step(context, 5, 0) } != 0 {
        facing = unsafe { step(context, 6, 0) } as u32;
        resolved = true;
    } else if policy != 0 && unsafe { step(context, 7, 0) } != 0 {
        facing = unsafe { step(context, 4, 0) } as u32;
        resolved = true;
    }
    if shield_type == 0 && policy != 0 && state != 0 {
        let remaining = if resolved {
            f32::from_bits(unsafe { step(context, 8, facing as usize) } as u32)
        } else {
            0.0
        };
        if unsafe { a2fo_rs_shields_suppress(shield_type, u8::from(resolved), remaining) } != 0 {
            return -1;
        }
        if resolved {
            let effect_id = unsafe { step(context, 9, facing as usize) } as u32 as i32;
            if effect_id >= 0 {
                unsafe { step(context, 10, effect_id as usize) };
            }
            return effect_id;
        }
    }
    (unsafe { step(context, 11, 0) }) as u32 as i32
}

/// Safety: native callback owns the tracked iterator, gateway and effect
/// storage for this synchronous pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_update_effect(
    step: Option<EventStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let original = unsafe { step(context, 0, 0) } != 0;
    let tracked = unsafe { step(context, 1, 0) } != 0;
    if !original
        || !tracked
        || unsafe { step(context, 2, 0) } == 0
        || unsafe { step(context, 3, 0) } == 0
    {
        if original {
            unsafe { step(context, 12, 0) };
        }
        return;
    }
    unsafe { step(context, 4, 0) };
    unsafe { step(context, 5, 0) };
    let policy = unsafe { step(context, 6, 0) };
    if policy != 0 {
        unsafe { step(context, 7, 0) };
    }
    let state = if policy != 0 {
        unsafe { step(context, 8, 0) }
    } else {
        0
    };
    if policy == 0 || state == 0 {
        unsafe { step(context, 11, 0) };
        unsafe { step(context, 12, 0) };
        return;
    }
    let remaining = f32::from_bits(unsafe { step(context, 9, 0) } as u32);
    if unsafe { a2fo_rs_shields_suppress(0, 1, remaining) } != 0 {
        unsafe { step(context, 10, 0) };
        unsafe { step(context, 11, 0) };
        return;
    }
    unsafe { step(context, 12, 0) };
    unsafe { step(context, 13, 0) };
}

/// Safety: callback owns the native effect map and original gateway.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_stop_effect(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    unsafe { step(context, 0) };
    if unsafe { step(context, 1) } != 0 {
        unsafe { step(context, 2) };
    }
}

/// Safety: the native callback owns the SDK damage-scope layout, policy
/// storage, native reads and numeric presentation kernel.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_begin_damage(
    craft: usize,
    scope: usize,
    required_size: u32,
    step: Option<EventStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if scope == 0 || (unsafe { step(context, 0, scope) } as u32) < required_size {
        return 0;
    }
    unsafe { step(context, 1, scope) };
    if unsafe { step(context, 2, 0) } == 0 || unsafe { step(context, 3, 0) } == 0 || craft == 0 {
        return 0;
    }
    if unsafe { step(context, 4, craft) } == 0 {
        return 0;
    }
    if unsafe { step(context, 5, craft) } == 0 {
        return 0;
    }
    if unsafe { step(context, 6, craft) } == 0 {
        return 0;
    }
    unsafe { step(context, 7, craft) };
    unsafe { step(context, 8, craft) };
    if unsafe { step(context, 9, craft) } == 0 {
        return 0;
    }
    unsafe { step(context, 10, scope) };
    unsafe { step(context, 11, scope) };
    1
}

/// Safety: the callback retains the native scope, policy and sidecar state
/// for the synchronous damage-completion pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_end_damage(
    scope: usize,
    required_size: u32,
    step: Option<EventStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if scope == 0
        || (unsafe { step(context, 0, scope) } as u32) < required_size
        || unsafe { step(context, 1, scope) } == 0
        || unsafe { step(context, 2, scope) } == 0
        || unsafe { step(context, 3, scope) } >= 4
    {
        return;
    }
    let policy = unsafe { step(context, 4, scope) };
    let state = unsafe { step(context, 5, scope) };
    if policy != 0 && state != 0 {
        for operation in 6..=10 {
            unsafe { step(context, operation, scope) };
        }
    }
    unsafe { step(context, 11, scope) };
    unsafe { step(context, 12, scope) };
}

/// Safety: the callback owns native editor-state, policy and Craft layouts.
/// Existing Rust shield-policy helpers remain the only mode semantics.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_set_state(
    craft: usize,
    values: usize,
    required_size: u32,
    step: Option<EventStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let existing = unsafe { step(context, 0, craft) };
    if unsafe { step(context, 1, 0) } == 0
        || unsafe { step(context, 2, 0) } == 0
        || craft == 0
        || values == 0
        || (unsafe { step(context, 3, values) } as u32) < required_size
        || unsafe { step(context, 4, 0) } != 0
        || existing == 0
        || unsafe { step(context, 5, values) } == 0
        || unsafe { step(context, 6, craft) } == 0
    {
        return 0;
    }
    if unsafe { step(context, 7, values) } == 0 {
        return 0;
    }
    let mode = unsafe { step(context, 8, 0) } as u32;
    for facing in 0..4 {
        if unsafe { a2fo_rs_shields_active(mode, facing) } == 0
            && f32::from_bits(unsafe { step(context, 9, facing as usize) } as u32) != 0.0
        {
            return 0;
        }
    }
    if unsafe { step(context, 10, craft) } == 0 {
        return 0;
    }
    if unsafe { step(context, 11, craft) } == 0 {
        return 0;
    }
    unsafe { step(context, 12, values) };
    unsafe { step(context, 13, craft) };
    1
}

/// Safety: callback owns the native bridge flags and diagnostic logger.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_connect(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1) } != 0 {
        return 1;
    }
    unsafe { step(context, 2) };
    unsafe { step(context, 3) };
    1
}

/// Query kinds: enabled, mode, current, maximum. Float results are returned
/// as bits so the native ABI thunk retains its original C++ signature.
/// Safety: callback owns native policy and sidecar storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_query(
    craft: usize,
    facing: u32,
    kind: u32,
    step: Option<EventStep>,
    context: *mut c_void,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, 0) } == 0 || unsafe { step(context, 1, 0) } == 0 {
        return 0;
    }
    if kind >= 2 && (craft == 0 || facing >= 4) {
        return 0;
    }
    if unsafe { step(context, 2, craft) } == 0 {
        return 0;
    }
    match kind {
        0 => 1,
        1 => (unsafe { step(context, 3, 0) }) as u32,
        3 => (unsafe { step(context, 4, facing as usize) }) as u32,
        2 => {
            if unsafe { step(context, 5, craft) } == 0 {
                0
            } else {
                (unsafe { step(context, 6, facing as usize) }) as u32
            }
        }
        _ => 0,
    }
}

/// Safety: callback retains SDK output and native sidecar storage throughout
/// this synchronous query.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_get_state(
    craft: usize,
    values: usize,
    required_size: u32,
    step: Option<EventStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if values == 0
        || (unsafe { step(context, 0, values) } as u32) < required_size
        || unsafe { step(context, 1, 0) } == 0
        || unsafe { step(context, 2, 0) } == 0
        || unsafe { step(context, 3, 0) } != 0
    {
        return 0;
    }
    let policy = unsafe { step(context, 4, craft) };
    let state = if policy != 0 {
        unsafe { step(context, 5, craft) }
    } else {
        0
    };
    if state == 0 {
        return 0;
    }
    unsafe { step(context, 6, craft) };
    unsafe { step(context, 7, values) };
    u8::from(unsafe { step(context, 8, values) } != 0)
}

/// Safety: callback owns policy and CraftState maps and keeps its temporary
/// native sidecar state live until the synchronous insertion completes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_state_for_craft(
    craft: usize,
    create: u8,
    step: Option<EventStep>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if craft == 0 {
        return 0;
    }
    let existing = unsafe { step(context, 0, craft) };
    if existing != 0 {
        return existing;
    }
    if create == 0 {
        return 0;
    }
    if unsafe { step(context, 1, craft) } == 0 {
        return 0;
    }
    unsafe { step(context, 2, craft) };
    let inserted = unsafe { step(context, 3, craft) };
    if inserted == 0 {
        return 0;
    }
    unsafe { step(context, 4, inserted) };
    inserted
}

/// Safety: callback retains the native identity-provider function pointer
/// and SDK output storage for this synchronous restoration pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_restore(
    craft: usize,
    step: Option<EventStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { step(context, 0, craft) } == 0 || unsafe { step(context, 1, craft) } == 0 {
        return 0;
    }
    if unsafe { step(context, 2, craft) } != 0 {
        return 1;
    }
    unsafe { step(context, 3, craft) };
    0
}

/// Safety: callback owns both native effect containers and advances/erases
/// their live iterators synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_forget_effects(
    step: Option<EventStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    unsafe { step(context, 0, 0) };
    while unsafe { step(context, 1, 0) } != 0 {
        if unsafe { step(context, 2, 0) } != 0 {
            unsafe { step(context, 3, 0) };
        } else {
            unsafe { step(context, 4, 0) };
        }
    }
    let mut index = unsafe { step(context, 5, 0) };
    while index > 0 {
        index -= 1;
        if unsafe { step(context, 6, index) } != 0 {
            unsafe { step(context, 7, index) };
        }
    }
}

/// Safety: callback owns the native effect map and gateway; iteration and
/// erase occur in the same synchronous native context as the frozen source.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_clear_tracked(
    remaining: f32,
    step: Option<EventStep>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if unsafe { a2fo_rs_shields_suppress(0, 1, remaining) } == 0 {
        return 0;
    }
    unsafe { step(context, 0, 0) };
    let mut stopped = 0;
    while unsafe { step(context, 1, 0) } != 0 {
        if unsafe { step(context, 2, 0) } != 0 {
            unsafe { step(context, 3, 0) };
            stopped += 1;
        } else {
            unsafe { step(context, 4, 0) };
        }
    }
    stopped
}

/// Safety: callback owns the fixed native active-scope array and its count.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_push_scope(
    scope: usize,
    craft: usize,
    step: Option<EventStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if scope == 0 || craft == 0 {
        return;
    }
    if unsafe { step(context, 0, 0) } >= unsafe { step(context, 1, 0) } {
        return;
    }
    unsafe { step(context, 2, 0) };
}

/// Safety: callback owns the fixed native active-scope array and performs
/// each original shift and trailing reset synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_remove_scope(
    scope: usize,
    step: Option<EventStep>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if scope == 0 {
        return;
    }
    let count = unsafe { step(context, 0, 0) };
    for index in (0..count).rev() {
        if unsafe { step(context, 1, index) } != scope {
            continue;
        }
        for move_index in index + 1..count {
            unsafe { step(context, 2, move_index) };
        }
        unsafe { step(context, 3, 0) };
        return;
    }
}

/// Safety: callback owns the fixed native scope array and writes only the
/// caller-owned facing output after a matching Craft is found.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_directional_active_facing(
    craft: usize,
    facing: usize,
    step: Option<EventStep>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if craft == 0 || facing == 0 {
        return 0;
    }
    let count = unsafe { step(context, 0, 0) };
    for index in (0..count).rev() {
        if unsafe { step(context, 1, index) } == craft {
            unsafe { step(context, 2, index) };
            return 1;
        }
    }
    0
}
