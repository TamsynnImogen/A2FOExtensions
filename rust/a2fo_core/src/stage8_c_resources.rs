// Resource event and admission workflows; native views remain caller owned.
use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ResourceView {
    pub data: *const u8,
    pub size: usize,
}
pub type Read = unsafe extern "C" fn(*mut c_void, u32, u32, *mut ResourceView) -> bool;
pub type Compare = unsafe extern "C" fn(*mut c_void, *const u8, *const u8, usize) -> bool;
pub type Parse = unsafe extern "C" fn(*mut c_void, u32) -> i32;
pub type Presentation = unsafe extern "C" fn(*mut c_void, u32) -> bool;

/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_find_field(
    context: *mut c_void,
    count: u32,
    name: *const u8,
    name_size: usize,
    read: Option<Read>,
    compare: Option<Compare>,
    out: *mut ResourceView,
) -> bool {
    let (Some(read), Some(compare)) = (read, compare) else {
        return false;
    };
    if name.is_null() || out.is_null() {
        return false;
    }
    for index in 0..count {
        let mut field = ResourceView {
            data: core::ptr::null(),
            size: 0,
        };
        if !unsafe { read(context, index, 0, &mut field) } {
            return false;
        }
        if field.data.is_null() || field.size != name_size {
            continue;
        }
        if !unsafe { compare(context, field.data, name, name_size) } {
            continue;
        }
        let mut value = ResourceView {
            data: core::ptr::null(),
            size: 0,
        };
        if !unsafe { read(context, index, 1, &mut value) } {
            return false;
        }
        if value.data.is_null() && value.size != 0 {
            continue;
        }
        unsafe { *out = value };
        return true;
    }
    false
}
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_parse4(
    context: *mut c_void,
    parse: Option<Parse>,
) -> bool {
    let Some(parse) = parse else { return false };
    for index in 0..4 {
        if unsafe { parse(context, index) } < 0 {
            return false;
        }
    }
    true
}
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_presentation40(
    context: *mut c_void,
    callback: Option<Presentation>,
) {
    let Some(callback) = callback else { return };
    for field in 0..40 {
        if !unsafe { callback(context, field) } {
            break;
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_index(resource: u32) -> i32 {
    if (6..10).contains(&resource) {
        (resource - 6) as i32
    } else {
        -1
    }
}
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_nonzero4(values: *const i64) -> bool {
    if values.is_null() {
        return false;
    }
    (0..4).any(|i| unsafe { *values.add(i) != 0 })
}
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_nonzero_costs(values: *const i32) -> bool {
    if values.is_null() {
        return false;
    }
    (0..4).any(|i| unsafe { *values.add(i) != 0 })
}
pub type RectangleRead = unsafe extern "C" fn(*mut c_void, u32, u32) -> bool;
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_load_rectangles(
    context: *mut c_void,
    enabled: bool,
    read: Option<RectangleRead>,
    found: *const bool,
) -> u32 {
    let Some(read) = read else { return 0 };
    if !enabled {
        return 0;
    }
    for index in 0..6 {
        unsafe { read(context, 0, index) };
    }
    for index in 0..4 {
        unsafe { read(context, 1, index) };
    }
    if found.is_null() {
        return 0;
    }
    (0..4).filter(|&index| unsafe { *found.add(index) }).count() as u32
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ResourceRectangle {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ResourceContext {
    pub resource_display: usize,
    pub text_component: usize,
    pub rectangle: ResourceRectangle,
}
pub type ContextRead = unsafe extern "C" fn(*mut c_void, u32, *mut ResourceContext) -> bool;
fn usable(rectangle: ResourceRectangle) -> bool {
    rectangle.right > rectangle.left && rectangle.bottom > rectangle.top
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_usable_rectangle(rectangle: ResourceRectangle) -> bool {
    usable(rectangle)
}
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_find_context(
    context: *mut c_void,
    found: *const bool,
    read: Option<ContextRead>,
    out_index: *mut u32,
    out: *mut ResourceContext,
) -> bool {
    let Some(read) = read else { return false };
    if found.is_null() || out_index.is_null() || out.is_null() {
        return false;
    }
    for index in 0..6 {
        if !unsafe { *found.add(index as usize) } {
            continue;
        }
        let mut candidate = ResourceContext::default();
        if !unsafe { read(context, index, &mut candidate) } {
            continue;
        }
        if candidate.resource_display == 0
            || candidate.text_component == 0
            || !usable(candidate.rectangle)
        {
            continue;
        }
        unsafe {
            *out_index = index;
            *out = candidate
        };
        return true;
    }
    false
}
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_hover(
    rectangles: *const ResourceRectangle,
    x: i32,
    y: i32,
) -> i32 {
    if rectangles.is_null() {
        return -1;
    }
    for index in 0..4 {
        let rectangle = unsafe { *rectangles.add(index) };
        if usable(rectangle)
            && x >= rectangle.left
            && x <= rectangle.right
            && y >= rectangle.top
            && y <= rectangle.bottom
        {
            return index as i32;
        }
    }
    -1
}
pub type PanelDraw = unsafe extern "C" fn(*mut c_void, u32);
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_draw_panel(
    context: *mut c_void,
    draw: Option<PanelDraw>,
) {
    let Some(draw) = draw else { return };
    for index in 0..4 {
        unsafe { draw(context, index) }
    }
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct RawRectangle {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}
pub type RawRead =
    unsafe extern "C" fn(*mut c_void, u32, u32, *mut RawRectangle, *mut RawRectangle);
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_panel_plan(
    context: *mut c_void,
    index: u32,
    context_index: u32,
    rectangle: ResourceRectangle,
    native_found: *const bool,
    read: Option<RawRead>,
    out_context: *mut RawRectangle,
    out_target: *mut RawRectangle,
) -> bool {
    let Some(read) = read else { return false };
    if index >= 4
        || !usable(rectangle)
        || context_index >= 6
        || native_found.is_null()
        || !unsafe { *native_found.add(context_index as usize) }
        || out_context.is_null()
        || out_target.is_null()
    {
        return false;
    }
    let mut raw_context = RawRectangle::default();
    let mut raw_target = RawRectangle::default();
    unsafe {
        read(
            context,
            index,
            context_index,
            &mut raw_context,
            &mut raw_target,
        )
    };
    if raw_context.width <= 0
        || raw_context.height <= 0
        || raw_target.width <= 0
        || raw_target.height <= 0
    {
        return false;
    }
    unsafe {
        *out_context = raw_context;
        *out_target = raw_target
    };
    true
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_amount_plan(
    index: u32,
    rectangle: ResourceRectangle,
) -> bool {
    index < 4 && usable(rectangle)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_cache_fresh(
    team: usize,
    old_team: usize,
    race: usize,
    old_race: usize,
    generation: u32,
    old_generation: u32,
) -> bool {
    team == old_team && race == old_race && generation == old_generation
}
pub type PresentationFill = unsafe extern "C" fn(*mut c_void, u32, u32, u32);
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_fill_presentation(
    context: *mut c_void,
    fill: Option<PresentationFill>,
) {
    let Some(fill) = fill else { return };
    for index in 0..4 {
        for kind in 0..4 {
            unsafe { fill(context, 0, index, kind) }
        }
    }
    for resource in 0..6 {
        for kind in 0..4 {
            unsafe { fill(context, if kind == 3 { 2 } else { 1 }, resource, kind) }
        }
    }
}
pub type KeyMatch = unsafe extern "C" fn(*mut c_void, u32) -> bool;
/// # Safety
/// Caller supplies valid bounded pointers and callbacks that do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_find_native_key(
    context: *mut c_void,
    check: Option<KeyMatch>,
) -> i32 {
    let Some(check) = check else { return -1 };
    for resource in 0..6 {
        if unsafe { check(context, resource) } {
            return resource as i32;
        }
    }
    -1
}
pub type InstallStep = unsafe extern "C" fn(*mut c_void, u32, u32) -> i32;
/// # Safety
/// Callback must remain valid and cannot unwind across this FFI call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_install_runtime(
    context: *mut c_void,
    step: Option<InstallStep>,
) -> u32 {
    let Some(step) = step else { return 0 };
    for index in 0..3 {
        let status = unsafe { step(context, 0, index) };
        if status < 0 {
            return 3;
        }
        if status == 0 {
            return 0;
        }
    }
    for index in 0..3 {
        let status = unsafe { step(context, 1, index) };
        if status < 0 {
            return 3;
        }
        if status == 0 {
            return 1;
        }
    }
    2
}
/// # Safety
/// Callback must remain valid and cannot unwind across this FFI call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_install_panel(
    context: *mut c_void,
    step: Option<InstallStep>,
) -> u32 {
    let Some(step) = step else { return 0 };
    for index in 0..7 {
        let status = unsafe { step(context, 0, index) };
        if status < 0 {
            return 3;
        }
        if status == 0 {
            return 0;
        }
    }
    let status = unsafe { step(context, 1, 0) };
    if status < 0 {
        return 3;
    }
    if status == 0 {
        return 1;
    }
    2
}
#[repr(C)]
pub struct PresentationInstall {
    pub tooltip: bool,
    pub verbose: bool,
    pub officer: bool,
    pub patched: u32,
}
/// # Safety
/// Callback must remain valid and cannot unwind; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_install_presentation(
    context: *mut c_void,
    step: Option<InstallStep>,
    out: *mut PresentationInstall,
) -> u32 {
    let Some(step) = step else { return 0 };
    if out.is_null() {
        return 0;
    }
    for index in 0..4 {
        let status = unsafe { step(context, 0, index) };
        if status < 0 {
            return 5;
        }
        if status == 0 {
            return 0;
        }
    }
    for index in 0..10 {
        let status = unsafe { step(context, 1, index) };
        if status < 0 {
            return 5;
        }
        if status == 0 {
            return 1;
        }
    }
    let status = unsafe { step(context, 2, 0) };
    if status < 0 {
        return 5;
    }
    if status == 0 {
        return 2;
    }
    let tooltip_status = unsafe { step(context, 3, 0) };
    if tooltip_status < 0 {
        return 5;
    }
    let verbose_status = unsafe { step(context, 3, 1) };
    if verbose_status < 0 {
        return 5;
    }
    let tooltip = tooltip_status != 0;
    let verbose = verbose_status != 0;
    let mut patched = 0;
    for index in 0..10 {
        let kind = if index < 5 { 0 } else { 1 };
        let status = unsafe { step(context, 4, index * 2 + kind) };
        if status < 0 {
            return 5;
        }
        if status != 0 {
            patched += 1
        }
    }
    let officer_status = unsafe { step(context, 5, 0) };
    if officer_status < 0 {
        return 5;
    }
    let officer = officer_status != 0;
    unsafe {
        *out = PresentationInstall {
            tooltip,
            verbose,
            officer,
            patched,
        }
    };
    if tooltip && verbose && officer && patched == 10 {
        4
    } else {
        3
    }
}
#[repr(C)]
pub struct Start {
    pub setting: u32,
    pub normal: [i64; 4],
    pub lots: [i64; 4],
}
pub type StartStep = unsafe extern "C" fn(*mut c_void, u32, *mut Start) -> bool;
/// # Safety
/// `out` is writable for four values; callback is non-unwinding.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_starting_amounts(
    context: *mut c_void,
    step: Option<StartStep>,
    out: *mut i64,
) -> bool {
    let Some(step) = step else { return false };
    if out.is_null() {
        return false;
    }
    let mut values = Start {
        setting: 0,
        normal: [0; 4],
        lots: [0; 4],
    };
    for operation in 0..3 {
        if !unsafe { step(context, operation, &mut values) } {
            return false;
        }
    }
    let selected = if values.setting == 1 {
        values.lots
    } else {
        values.normal
    };
    for (index, value) in selected.into_iter().enumerate() {
        unsafe { *out.add(index) = value };
    }
    true
}
pub type SnapshotStep = unsafe extern "C" fn(*mut c_void, u32) -> i32;
/// # Safety
/// Callback is valid and non-unwinding for the supplied context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_snapshot(
    context: *mut c_void,
    step: Option<SnapshotStep>,
) -> bool {
    let Some(step) = step else { return false };
    let found = unsafe { step(context, 0) };
    if found < 0 {
        return false;
    }
    if found > 0 {
        return true;
    }
    if unsafe { step(context, 1) } <= 0 {
        return false;
    }
    (unsafe { step(context, 2) }) > 0
}
pub type ClassStep = unsafe extern "C" fn(*mut c_void, u32) -> i32;
/// # Safety
/// Callback remains valid and cannot unwind across FFI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_class_costs(
    context: *mut c_void,
    provider_missing: bool,
    step: Option<ClassStep>,
) -> bool {
    let Some(step) = step else { return false };
    if provider_missing && unsafe { step(context, 0) } < 0 {
        return false;
    }
    let supplied = unsafe { step(context, 1) };
    if supplied < 0 {
        return false;
    }
    if supplied > 0 {
        return true;
    }
    (unsafe { step(context, 2) }) > 0
}
pub type PresentationEmpty = unsafe extern "C" fn(*mut c_void, u32, u32) -> i32;
pub type PresentationLog = unsafe extern "C" fn(*mut c_void, u32) -> bool;
/// # Safety
/// Callbacks must remain valid and cannot unwind across FFI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_log_presentations(
    context: *mut c_void,
    empty: Option<PresentationEmpty>,
    log: Option<PresentationLog>,
) -> bool {
    let (Some(empty), Some(log)) = (empty, log) else {
        return false;
    };
    for resource in 0..10 {
        let mut all_empty = true;
        for kind in 0..4 {
            let status = unsafe { empty(context, resource, kind) };
            if status < 0 {
                return false;
            }
            if status > 0 {
                all_empty = false;
                break;
            }
        }
        if !all_empty && !unsafe { log(context, resource) } {
            return false;
        }
    }
    true
}
pub type RegistrationField = unsafe extern "C" fn(*mut c_void, u32, u32);
/// # Safety
/// Callback remains valid and cannot unwind across FFI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_registration_fields(
    context: *mut c_void,
    fill: Option<RegistrationField>,
) {
    let Some(fill) = fill else { return };
    for index in 0..4 {
        unsafe { fill(context, 0, index) }
    }
    for index in 0..40 {
        unsafe { fill(context, 1, index) }
    }
    unsafe { fill(context, 2, 0) };
}
pub type InitStep = unsafe extern "C" fn(*mut c_void, u32) -> i32;
/// # Safety
/// Callback remains valid and cannot unwind across FFI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_module_init(
    context: *mut c_void,
    step: Option<InitStep>,
) -> u32 {
    let Some(step) = step else { return 0 };
    for operation in 0..3 {
        let status = unsafe { step(context, operation) };
        if status < 0 {
            return 3;
        }
        if status == 0 {
            return 0;
        }
    }
    let status = unsafe { step(context, 3) };
    if status < 0 {
        return 3;
    }
    if status == 0 {
        return 1;
    }
    if unsafe { step(context, 4) } < 0 {
        return 3;
    }
    if unsafe { step(context, 5) } < 0 {
        return 3;
    }
    2
}
/// # Safety
/// `costs` points to four readable 32-bit values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_zero_costs(costs: *const i32) -> bool {
    if costs.is_null() {
        return false;
    }
    (0..4).all(|index| unsafe { *costs.add(index) == 0 })
}
/// # Safety
/// Both pointers address four readable 64-bit values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_resources_equal_amounts(
    left: *const i64,
    right: *const i64,
) -> bool {
    if left.is_null() || right.is_null() {
        return false;
    }
    (0..4).all(|index| unsafe { *left.add(index) == *right.add(index) })
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_refund_kind(kind: u32) -> bool {
    kind == 5 || kind == 6 || kind == 7
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_set_amount(amount: i64) -> bool {
    amount >= 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_clamp_amount(amount: i64) -> i64 {
    amount.max(0)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_finite_colour(red: f32, green: f32, blue: f32) -> bool {
    red.is_finite() && green.is_finite() && blue.is_finite()
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_tooltip_fresh(
    component: usize,
    old_component: usize,
    team: usize,
    old_team: usize,
    hovered: i32,
    old_hovered: i32,
) -> bool {
    component == old_component && team == old_team && hovered == old_hovered
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_cache_refresh(
    team: usize,
    old_generation: u32,
    generation: u32,
    runtime: bool,
) -> bool {
    runtime && (team == 0 || old_generation != generation)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_resources_panel_config_stale(
    loaded: bool,
    old_parameter_db: usize,
    parameter_db: usize,
) -> bool {
    !loaded || old_parameter_db != parameter_db
}
