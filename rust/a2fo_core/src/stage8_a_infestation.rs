//! Portable lifecycle and runtime sequencing for the Infestation module.

use crate::stage8_a_breeders::{Lower, lower_bytes, odf, unquote};
use alloc::vec::Vec;
use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
type Field = unsafe extern "C" fn(*mut c_void, u32, *mut Bytes, *mut Bytes) -> u8;
type ParseFloat = unsafe extern "C" fn(*mut c_void, Bytes, *mut f32) -> u8;
type ParseCount = unsafe extern "C" fn(*mut c_void, Bytes, *mut u32) -> u8;
type Project = unsafe extern "C" fn(*mut c_void, *mut u32) -> u8;
type Store = unsafe extern "C" fn(*mut c_void, *const ParsedPolicy) -> u8;
type StoreMother = unsafe extern "C" fn(*mut c_void, u8) -> u8;
type Log = unsafe extern "C" fn(*mut c_void);

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}

#[repr(C)]
pub struct ParsedPolicy {
    enabled: u8,
    delay_seconds: f32,
    damage: f32,
    larva_odf: Bytes,
    larva_count: u32,
    explosion_project_id: u32,
    bypass_shields: u8,
    spawn_radius: f32,
}

unsafe fn slice<'a>(view: Bytes) -> Option<&'a [u8]> {
    if view.data.is_null() {
        return (view.size == 0).then_some(&[]);
    }
    Some(unsafe { core::slice::from_raw_parts(view.data, view.size) })
}

fn view(value: &[u8]) -> Bytes {
    Bytes {
        data: value.as_ptr(),
        size: value.len(),
    }
}

fn field_value(
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
        let (Some(name), Some(value)) = (unsafe { slice(name) }, unsafe { slice(value) }) else {
            continue;
        };
        if name.len() == wanted.len() && name.iter().zip(wanted).all(
            |(left, right)| unsafe { lower(context, *left) } == unsafe { lower(context, *right) },
        ) {
            return value.to_vec();
        }
    }
    Vec::new()
}

fn boolean(input: &[u8], fallback: bool, lower: Lower, context: *mut c_void) -> bool {
    match lower_bytes(unquote(input), lower, context).as_slice() {
        b"1" | b"true" | b"yes" | b"on" => true,
        b"0" | b"false" | b"no" | b"off" => false,
        _ => fallback,
    }
}

unsafe fn positive_or_default(
    input: &[u8],
    fallback: f32,
    parse: ParseFloat,
    context: *mut c_void,
) -> Option<f32> {
    let value = unquote(input);
    if value.is_empty() {
        return Some(fallback);
    }
    let mut parsed = 0.0;
    match unsafe { parse(context, view(value), &mut parsed) } {
        2 => None,
        1 if parsed.is_finite() => Some(parsed.max(0.0)),
        _ => Some(fallback.max(0.0)),
    }
}

unsafe fn count_or_default(
    input: &[u8],
    fallback: u32,
    parse: ParseCount,
    context: *mut c_void,
) -> Option<u32> {
    let value = unquote(input);
    if value.is_empty() {
        return Some(fallback);
    }
    let mut parsed = 0;
    match unsafe { parse(context, view(value), &mut parsed) } {
        2 => None,
        1 => Some(parsed.min(64)),
        _ => Some(fallback),
    }
}

/// Safety: each SDK view is readable for its positive length during the
/// synchronous call. Native callbacks contain C++ exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_weapon_policy(
    weapon_class: usize,
    field_count: u32,
    lower: Option<Lower>,
    field: Option<Field>,
    parse_float: Option<ParseFloat>,
    parse_count: Option<ParseCount>,
    project: Option<Project>,
    store: Option<Store>,
    log: Option<Log>,
    context: *mut c_void,
) {
    let (
        Some(lower),
        Some(field),
        Some(parse_float),
        Some(parse_count),
        Some(project),
        Some(store),
        Some(log),
    ) = (lower, field, parse_float, parse_count, project, store, log)
    else {
        return;
    };
    if weapon_class == 0 {
        return;
    }
    macro_rules! get {
        ($name:expr) => {
            field_value($name, field_count, lower, field, context)
        };
    }
    let enabled = boolean(&get!(b"noxterInfestation"), false, lower, context);
    if !enabled {
        unsafe { store(context, core::ptr::null()) };
        return;
    }
    let Some(delay_seconds) =
        (unsafe { positive_or_default(&get!(b"infestationDelay"), 10.0, parse_float, context) })
    else {
        return;
    };
    let Some(damage) =
        (unsafe { positive_or_default(&get!(b"infestationDamage"), 0.0, parse_float, context) })
    else {
        return;
    };
    let larva_odf = odf(&get!(b"infestationLarva"), lower, context);
    let Some(larva_count) =
        (unsafe { count_or_default(&get!(b"infestationLarvaCount"), 1, parse_count, context) })
    else {
        return;
    };
    let bypass_shields = boolean(&get!(b"infestationBypassShields"), true, lower, context);
    let Some(spawn_radius) = (unsafe {
        positive_or_default(&get!(b"infestationSpawnRadius"), 20.0, parse_float, context)
    }) else {
        return;
    };
    let mut explosion_project_id = 0;
    if unsafe { project(context, &mut explosion_project_id) } == 2 {
        return;
    }
    let policy = ParsedPolicy {
        enabled: 1,
        delay_seconds,
        damage,
        larva_odf: view(&larva_odf),
        larva_count,
        explosion_project_id,
        bypass_shields: u8::from(bypass_shields),
        spawn_radius,
    };
    if unsafe { store(context, &policy) } == 1 {
        unsafe { log(context) };
    }
}

/// Safety: SDK field views are valid synchronously; native map storage is
/// performed only through the callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_mother_metadata(
    object_class: usize,
    field_count: u32,
    lower: Option<Lower>,
    field: Option<Field>,
    store: Option<StoreMother>,
    context: *mut c_void,
) {
    let (Some(lower), Some(field), Some(store)) = (lower, field, store) else {
        return;
    };
    if object_class == 0 {
        return;
    }
    let mother = boolean(
        &field_value(b"breederMother", field_count, lower, field, context),
        false,
        lower,
        context,
    );
    unsafe { store(context, u8::from(mother)) };
}

/// Safety: callback checks each SDK field only after the original admission.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_class_event(
    event: usize,
    required_size: u32,
    object_class: u8,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0
        || event == 0
        || (unsafe { step(context, 1, event) } as u32) < required_size
    {
        return;
    }
    if object_class != 0 && unsafe { step(context, 2, event) } == 0 {
        return;
    }
    unsafe { step(context, 3, event) };
}

/// Safety: callback owns native generation state and its transient maps.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_activation(
    current: u64,
    previous: u64,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if current != previous {
        unsafe { step(context, 0, 0) };
        unsafe { step(context, 1, 0) };
        unsafe { step(context, 2, 0) };
    }
    u8::from(current != 0)
}

/// Safety: callback reads the native API and captures/rethrows exceptions
/// after this function returns. `usize::MAX` is the exception sentinel.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_init(
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
    if !matches!(unsafe { call(step, context, 1) }, Some(true)) {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 2) }, Some(true)) {
        return 0;
    }
    let explosion_ready = match unsafe { call(step, context, 3) } {
        Some(ready) => ready,
        None => return 0,
    };
    if !explosion_ready && unsafe { call(step, context, 4) }.is_none() {
        return 0;
    }
    if unsafe { call(step, context, 5) }.is_none() {
        return 0;
    }
    let mut registered = true;
    for operation in 6..=9 {
        match unsafe { call(step, context, operation) } {
            Some(value) => registered &= value,
            None => return 0,
        }
    }
    if !registered {
        if unsafe { call(step, context, 10) }.is_none() {
            return 0;
        }
        unsafe { call(step, context, 11) };
        return 0;
    }
    u8::from(unsafe { call(step, context, 12) }.is_some())
}

/// Safety: callback owns native maps and API/module handles.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_shutdown(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    for operation in 0..5 {
        unsafe { step(context, operation, 0) };
    }
}

/// Safety: native callback performs checked list and object reads, retaining
/// the production x87 distance/comparison kernel.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_nearest_mother(
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    let head = unsafe { step(context, 0, 0) };
    if head == 0 {
        return 0;
    }
    let count = unsafe { step(context, 1, 0) } as u32;
    let mut node = unsafe { step(context, 2, head) };
    let mut best = 0;
    let mut best_handle = 0;
    for _ in 0..count {
        if node == 0 || node == head {
            break;
        }
        let next = unsafe { step(context, 3, node) };
        let candidate = unsafe { step(context, 4, node) };
        if candidate != 0
            && unsafe { step(context, 5, candidate) } == 0
            && unsafe { step(context, 6, candidate) } != 0
            && unsafe { step(context, 7, candidate) } != 0
        {
            unsafe { step(context, 8, candidate) };
            let handle = unsafe { step(context, 9, candidate) } as u32;
            let comparison = if best == 0 {
                0
            } else {
                unsafe { step(context, 10, best) }
            };
            if best == 0 || comparison & 1 != 0 || (comparison & 2 != 0 && handle < best_handle) {
                best = candidate;
                best_handle = handle;
                unsafe { step(context, 11, candidate) };
            }
        }
        if next == node {
            break;
        }
        node = next;
    }
    best
}

/// Safety: native callback performs checked Craft/Yard reads and entity lookup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_repair_admitted(
    craft: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if craft == 0 {
        return 0;
    }
    let command = unsafe { step(context, 0, craft) } as u32;
    if command != 0x11 && command != 0x35 {
        return 0;
    }
    let stage = unsafe { step(context, 1, craft) } as u32;
    let yard_handle = unsafe { step(context, 2, craft) } as u32;
    if stage == 0 || yard_handle == 0 {
        return 0;
    }
    let yard = unsafe { step(context, 3, yard_handle as usize) };
    if yard == 0
        || unsafe { step(context, 4, yard) } != 0
        || unsafe { step(context, 5, yard) } != unsafe { step(context, 6, craft) }
    {
        return 0;
    }
    u8::from(unsafe { step(context, 7, yard) } != 0)
}

/// Safety: native callback resolves the Craft and reads its live team.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_attacker_handle(
    infector_handle: u32,
    infector_team: i32,
    step: Option<Step>,
    context: *mut c_void,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    let attacker = unsafe { step(context, 0, infector_handle as usize) };
    if attacker == 0
        || unsafe { step(context, 1, attacker) } != 0
        || unsafe { step(context, 2, attacker) } as u32 as i32 != infector_team
    {
        0
    } else {
        infector_handle
    }
}

/// Safety: callback owns the native route iterator; native distance and
/// refresh comparisons retain the production x87 behavior.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_update_route(
    larva: usize,
    elapsed_seconds: f32,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let handle = unsafe { step(context, 0, larva) };
    if unsafe { step(context, 1, handle) } == 0 {
        return;
    }
    if unsafe { step(context, 2, larva) } == 0 {
        unsafe { step(context, 3, handle) };
        return;
    }
    if elapsed_seconds.is_finite() && elapsed_seconds > 0.0 {
        unsafe { step(context, 4, 0) };
    }
    if unsafe { step(context, 5, 0) } != 0 {
        return;
    }
    unsafe { step(context, 6, 0) };
    let mother_handle = unsafe { step(context, 7, 0) } as u32;
    let mut mother = if mother_handle == 0 {
        0
    } else {
        unsafe { step(context, 8, mother_handle as usize) }
    };
    if mother == 0
        || unsafe { step(context, 9, mother) } != 0
        || unsafe { step(context, 10, mother) } == 0
        || unsafe { step(context, 11, mother) } == 0
    {
        mother = unsafe { step(context, 12, larva) };
        unsafe { step(context, 13, mother) };
    }
    if mother == 0 {
        return;
    }
    if unsafe { step(context, 14, mother) } != 0 {
        unsafe { step(context, 15, handle) };
        return;
    }
    unsafe { step(context, 16, larva) };
}

/// Safety: callback retains infection strings, native transform math,
/// class/entity storage, and engine construction for the synchronous pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_spawn_larvae(
    count: u32,
    infector_team: i32,
    has_larva_odf: u8,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if count == 0 || has_larva_odf == 0 || infector_team < 0 {
        return;
    }
    let larva_class = unsafe { step(context, 0, 0) };
    if larva_class == 0 {
        unsafe { step(context, 1, 0) };
        return;
    }
    let mother = unsafe { step(context, 2, 0) };
    if mother != 0 {
        unsafe { step(context, 3, mother) };
        unsafe { step(context, 4, mother) };
    }
    let mut spawned = 0;
    for index in 0..count {
        let larva = unsafe { step(context, 5, index as usize) };
        if larva == 0 {
            continue;
        }
        spawned += 1;
        let handle = unsafe { step(context, 6, larva) } as u32;
        if handle == 0 {
            continue;
        }
        unsafe { step(context, 7, handle as usize) };
        if mother != 0 {
            unsafe { step(context, 8, larva) };
        }
    }
    unsafe { step(context, 9, spawned as usize) };
}

/// Safety: callback retains the infection snapshot and native damage block.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_hatch(
    craft: usize,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, craft) } == 0 {
        unsafe { step(context, 1, 0) };
        return;
    }
    unsafe { step(context, 2, 0) };
    let attacker = unsafe { step(context, 3, 0) };
    unsafe { step(context, 4, attacker) };
    unsafe { step(context, 5, 0) };
}

/// Safety: event admission precedes native field reads; callback keeps the
/// infection iterator and moved snapshot in C++ storage for the pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_craft_event(
    event: usize,
    required_size: u32,
    cleanup_kind: u32,
    postload_kind: u32,
    simulate_kind: u32,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let enabled = unsafe { step(context, 0, 0) } != 0;
    if event == 0
        || (unsafe { step(context, 1, event) } as u32) < required_size
        || unsafe { step(context, 2, event) } == 0
    {
        return;
    }
    let handle = unsafe { step(context, 3, event) };
    let kind = unsafe { step(context, 4, event) } as u32;
    if kind == cleanup_kind || kind == postload_kind {
        unsafe { step(context, 5, handle) };
        unsafe { step(context, 6, handle) };
        return;
    }
    if !enabled || kind != simulate_kind {
        return;
    }
    unsafe { step(context, 7, event) };
    if unsafe { step(context, 8, handle) } == 0 {
        return;
    }
    if unsafe { step(context, 9, event) } != 0 {
        unsafe { step(context, 10, handle) };
        unsafe { step(context, 11, handle) };
        return;
    }
    let elapsed = f32::from_bits(unsafe { step(context, 12, event) } as u32);
    if elapsed.is_finite() && elapsed > 0.0 {
        unsafe { step(context, 13, event) };
    }
    if unsafe { step(context, 14, handle) } != 0 {
        return;
    }
    unsafe { step(context, 15, handle) };
    unsafe { step(context, 16, handle) };
    unsafe { step(context, 17, event) };
}

/// Safety: callback performs checked weapon/Craft reads and native owner lookup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_target_valid(
    weapon: usize,
    target: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let policy = unsafe { step(context, 0, weapon) };
    if policy == 0 || unsafe { step(context, 1, policy) } == 0 || target == 0 {
        return 0;
    }
    if unsafe { step(context, 2, target) } == 0 {
        return 0;
    }
    let owner = unsafe { step(context, 3, weapon) };
    if unsafe { step(context, 4, owner) } == 0 {
        return 0;
    }
    let owner_team = unsafe { step(context, 5, owner) } as u32 as i32;
    let target_team = unsafe { step(context, 6, target) } as u32 as i32;
    if owner_team < 0 || target_team < 0 || owner_team == target_team {
        return 0;
    }
    let target_handle = unsafe { step(context, 7, target) };
    if unsafe { step(context, 8, target_handle) } != 0 {
        return 0;
    }
    unsafe { step(context, 9, owner) };
    1
}

/// Safety: C++ retains the infection object/map and captures callback
/// exceptions; UINTPTR_MAX stops later native actions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_trigger(
    event: usize,
    required_size: u32,
    precheck_kind: u32,
    committed_kind: u32,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 1;
    };
    if unsafe { step(context, 0, 0) } == 0
        || event == 0
        || (unsafe { step(context, 1, event) } as u32) < required_size
    {
        return 1;
    }
    let valid = unsafe { step(context, 2, event) };
    if valid == usize::MAX {
        return 1;
    }
    let kind = unsafe { step(context, 3, event) } as u32;
    if kind == precheck_kind {
        let policy = unsafe { step(context, 4, event) };
        return u8::from(policy == 0 || valid != 0);
    }
    if kind != committed_kind || valid == 0 {
        return 1;
    }
    if unsafe { step(context, 5, 0) } == 0 || unsafe { step(context, 6, 0) } == 0 {
        return 1;
    }
    for operation in 7..=13 {
        if unsafe { step(context, operation, event) } == usize::MAX {
            return 1;
        }
    }
    1
}

/// Safety: callback performs checked native image reads and original logging.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_signatures(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    let mut supported = true;
    for index in 0..9 {
        if unsafe { step(context, index, 0) } == 0 {
            unsafe { step(context, 9 + index, 0) };
            supported = false;
        }
    }
    u8::from(supported)
}

/// Safety: callback performs native mission lookup and vtable dispatch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_add_to_mission(
    object: usize,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if object == 0 || unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    let mission = unsafe { step(context, 1, 0) };
    let add = unsafe { step(context, 2, mission) };
    if mission != 0 && add != 0 {
        unsafe { step(context, 3, object) };
    }
}

/// Safety: callback calls the native constructor and owns the resulting Craft.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_construct(
    object_class: usize,
    team: i32,
    step: Option<Step>,
    context: *mut c_void,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if object_class == 0 || unsafe { step(context, 0, 0) } == 0 || team < 0 {
        return 0;
    }
    let object = unsafe { step(context, 1, object_class) };
    if object != 0 {
        unsafe { step(context, 2, object) };
    }
    object
}

/// Safety: callback checks native expiry and dispatches the synchronized move.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_move(
    craft: usize,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if craft == 0 || unsafe { step(context, 0, craft) } != 0 {
        return;
    }
    unsafe { step(context, 1, craft) };
}

/// Safety: callback resolves the native effect class and virtual build entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_explosion(
    project_id: u32,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { step(context, 0, 0) } == 0 || project_id == 0 {
        return;
    }
    let class = unsafe { step(context, 1, project_id as usize) };
    let build = unsafe { step(context, 2, class) };
    if build == 0 {
        return;
    }
    unsafe { step(context, 3, build) };
}

/// Safety: callback performs native Craft validation and builds the original
/// synthetic DamageInfo block before calling the engine.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_damage(
    craft: usize,
    amount: f32,
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if craft == 0 || unsafe { step(context, 0, craft) } == 0 || !amount.is_finite() || amount <= 0.0
    {
        return;
    }
    unsafe { step(context, 1, craft) };
}

/// Safety: callback retains the output pointer and native ParameterDB call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_infestation_project_id(
    parameter_db: usize,
    command: usize,
    output: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if parameter_db == 0 || command == 0 || output == 0 || unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 1, output) };
    u8::from((unsafe { step(context, 2, parameter_db) } & 0xff) != 0)
}
