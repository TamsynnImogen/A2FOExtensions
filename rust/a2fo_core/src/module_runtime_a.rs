//! Synchronous orchestration for the wreckage and info.ini module runtimes.
//! Native callbacks retain the SDK, filesystem, Win32 and owned-string boundary.
use core::{
    ffi::{c_int, c_void},
    slice,
};
unsafe extern "C" {
    fn tolower(value: c_int) -> c_int;
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    pub data: *const u8,
    pub size: usize,
}
impl Bytes {
    unsafe fn get<'a>(self) -> Option<&'a [u8]> {
        if self.size == 0 {
            Some(&[])
        } else if self.data.is_null() || self.size > isize::MAX as usize {
            None
        } else {
            Some(unsafe { slice::from_raw_parts(self.data, self.size) })
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Field {
    pub name: Bytes,
    pub value: Bytes,
    pub value_valid: u8,
}
type FieldGet = unsafe extern "C" fn(*mut c_void, u32, *mut Field) -> u8;
type Register = unsafe extern "C" fn(*mut c_void) -> u8;
type Log = unsafe extern "C" fn(*mut c_void, u32);
type InfoStep = unsafe extern "C" fn(*mut c_void, u32, u32) -> u32;
type Resolve = unsafe extern "C" fn(*mut c_void, Bytes, *mut Bytes) -> u8;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct InfoState {
    pub has_speed: u8,
    pub speed: i32,
    pub configured: Bytes,
    pub shared: Bytes,
    pub default_root: Bytes,
}
fn equals_ascii_case(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len()
        && a.iter()
            .zip(b)
            .all(|(&x, &y)| unsafe { tolower(x as c_int) == tolower(y as c_int) })
}
unsafe fn find_field(
    count: u32,
    get: FieldGet,
    context: *mut c_void,
    name: &[u8],
) -> Option<Bytes> {
    for i in 0..count {
        let mut field = Field {
            name: Bytes {
                data: core::ptr::null(),
                size: 0,
            },
            value: Bytes {
                data: core::ptr::null(),
                size: 0,
            },
            value_valid: 0,
        };
        if unsafe { get(context, i, &mut field) } == 0 {
            return None;
        }
        let Some(key) = (unsafe { field.name.get() }) else {
            continue;
        };
        if !equals_ascii_case(key, name) {
            continue;
        }
        return (field.value_valid != 0).then_some(field.value);
    }
    None
}
/// Safety: callbacks and spans are valid for this call. `replacement` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_wreckage_handler(
    enabled: u8,
    valid_event: u8,
    valid_replacement: u8,
    valid_fields: u8,
    field_count: u32,
    seed: u32,
    output_capacity: usize,
    get_field: Option<FieldGet>,
    context: *mut c_void,
    replacement: *mut Bytes,
) -> u32 {
    if enabled == 0
        || valid_event == 0
        || valid_replacement == 0
        || valid_fields == 0
        || replacement.is_null()
    {
        return 0;
    }
    let Some(get) = get_field else {
        return 0;
    };
    let Some(wreckage) = (unsafe { find_field(field_count, get, context, b"wreckage") }) else {
        return 0;
    };
    let chance = unsafe { find_field(field_count, get, context, b"wreckageChance") };
    let Some(w) = (unsafe { wreckage.get() }) else {
        return 0;
    };
    let (cp, cl) = match chance {
        Some(c) => (c.data, c.size),
        None => (core::ptr::null(), 0),
    };
    let mut offset = 0;
    let mut length = 0;
    let status = unsafe {
        crate::wreckage::a2fo_rs_wreckage_decide(
            w.as_ptr(),
            w.len(),
            u8::from(chance.is_some()),
            cp,
            cl,
            seed,
            &mut offset,
            &mut length,
        )
    };
    if status != 1 {
        return status;
    }
    if length == 0 || length >= output_capacity || offset > w.len() || length > w.len() - offset {
        return 0;
    }
    unsafe {
        replacement.write(Bytes {
            data: w.as_ptr().add(offset),
            size: length,
        });
    }
    1
}
/// Safety: callbacks are valid for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_wreckage_init(
    valid_api: u8,
    dispatch_available: u8,
    register: Option<Register>,
    log: Option<Log>,
    context: *mut c_void,
) -> u8 {
    if valid_api == 0 {
        return 0;
    }
    let success = dispatch_available != 0 && register.is_some_and(|f| unsafe { f(context) != 0 });
    if let Some(log) = log {
        unsafe { log(context, if success { 1 } else { 0 }) };
    }
    u8::from(success)
}
/// Safety: callbacks are valid for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_info_init(
    valid_api: u8,
    root_count: u32,
    step: Option<InfoStep>,
    log: Option<Log>,
    context: *mut c_void,
) -> u8 {
    if valid_api == 0 {
        return 0;
    }
    let (Some(step), Some(log)) = (step, log) else {
        return 0;
    };
    let base = if root_count == 0 { u32::MAX } else { 0 };
    if unsafe { step(context, 0, base) } == 0 {
        unsafe { log(context, 0) }; // Data root unavailable.
        return 0;
    }
    unsafe { step(context, 1, 0) }; // Parse base info.ini.
    if root_count > 1 {
        if unsafe { step(context, 0, root_count - 1) } != 0 {
            unsafe { step(context, 2, 0) }; // Select active overlay defaults.
        }
        unsafe {
            step(context, 3, 0);
            log(context, 1)
        }; // Expand base shared setting.
    }
    unsafe {
        step(context, 4, 0);
        log(context, 2);
        step(context, 5, 0);
        log(context, 3)
    };
    if unsafe { step(context, 6, 0) } == 0 {
        return 0;
    }
    unsafe { log(context, 5) };
    1
}
/// Safety: outputs and borrowed strings are valid; resolver returns a borrowed span.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_info_provide(
    enabled: u8,
    ready: u8,
    has_speed_output: u8,
    speed_output: u8,
    state: InfoState,
    normal: Bytes,
    normal_present: u8,
    output_present: u8,
    output_capacity: u32,
    resolve: Option<Resolve>,
    context: *mut c_void,
    has_speed: *mut u32,
    speed: *mut i32,
    output: *mut u8,
) -> u8 {
    if enabled == 0
        || ready == 0
        || has_speed_output == 0
        || speed_output == 0
        || has_speed.is_null()
        || speed.is_null()
    {
        return 0;
    }
    unsafe {
        has_speed.write(u32::from(state.has_speed != 0));
        speed.write(state.speed);
    }
    if output_present != 0 && output_capacity != 0 {
        if output.is_null() {
            return 0;
        }
        unsafe { output.write(0) };
        if state.configured.size != 0 && normal_present != 0 {
            let Some(resolve) = resolve else {
                return 0;
            };
            let mut resolved = Bytes {
                data: core::ptr::null(),
                size: 0,
            };
            if unsafe { resolve(context, normal, &mut resolved) } == 0 {
                return 0;
            }
            let Some(data) = (unsafe { resolved.get() }) else {
                return 0;
            };
            if data.len() >= output_capacity as usize {
                return 0;
            }
            unsafe {
                core::ptr::copy_nonoverlapping(data.as_ptr(), output, data.len());
                output.add(data.len()).write(0);
            }
        }
    }
    1
}

type NormalRead = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> usize;
type NormalLog = unsafe extern "C" fn(*mut c_void, u32, u32, i32);
type NormalInitStep = unsafe extern "C" fn(*mut c_void, u32) -> u8;
/// Safety: callbacks and pointed-to native objects remain valid during the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_normal_allows(
    enabled: u8,
    ready: u8,
    weapon: usize,
    read: Option<NormalRead>,
    log: Option<NormalLog>,
    context: *mut c_void,
) -> u8 {
    if enabled == 0 || ready == 0 || weapon == 0 {
        return 1;
    }
    let Some(read) = read else {
        return 1;
    };
    let class = unsafe { read(context, 0, weapon, 0) };
    if class == 0 || unsafe { read(context, 1, class, 0) } != 0 {
        return 1;
    }
    let project_object = unsafe { read(context, 2, class, 0) };
    let project = unsafe { read(context, 3, project_object, 0) } as u32;
    if project == 0 || project > 0x7fff_ffff {
        return 1;
    }
    let owner = unsafe { read(context, 4, weapon, 0) };
    let team = unsafe { read(context, 5, owner, 0) } as i32;
    if owner == 0 || !(0..=63).contains(&team) {
        return 1;
    }
    let trees = unsafe { read(context, 6, 0, 0) };
    let tree = unsafe { read(context, 7, trees, team as usize) };
    let items = unsafe { read(context, 8, tree, 0) };
    if tree == 0 || items == 0 {
        return 1;
    }
    let item = unsafe { read(context, 9, items, project as usize - 1) };
    if item == 0 {
        if let Some(log) = log {
            unsafe { log(context, 0, project, team) };
        }
        return 1;
    }
    let allowed = unsafe { read(context, 10, tree, project as usize) } & 0xff != 0;
    if let Some(log) = log {
        unsafe { log(context, if allowed { 1 } else { 2 }, project, team) };
    }
    u8::from(allowed)
}
/// Safety: same contract as `a2fo_rs_runtime_normal_allows`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_normal_event(
    valid: u8,
    kind: u32,
    weapon: usize,
    enabled: u8,
    ready: u8,
    read: Option<NormalRead>,
    log: Option<NormalLog>,
    context: *mut c_void,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    if kind == 1 {
        return 1;
    } // COMMITTED
    if kind != 0 {
        return 0;
    } // only PRECHECK consults the tree
    unsafe { a2fo_rs_runtime_normal_allows(enabled, ready, weapon, read, log, context) }
}
/// Safety: callbacks are valid for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_normal_init(
    valid_api: u8,
    step: Option<NormalInitStep>,
    log: Option<Log>,
    context: *mut c_void,
) -> u8 {
    if valid_api == 0 {
        return 0;
    }
    let (Some(step), Some(log)) = (step, log) else {
        return 0;
    };
    if unsafe { step(context, 0) } == 0 {
        return 0;
    } // obtain both modules
    if unsafe { step(context, 1) } == 0 {
        unsafe { log(context, 0) };
        return 0;
    }
    if unsafe { step(context, 2) } == 0 {
        unsafe { log(context, 0) };
        return 0;
    }
    let ready = unsafe { step(context, 3) } != 0;
    unsafe { log(context, if ready { 1 } else { 2 }) };
    1 // handler is resident even when a native signature disables filtering
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct TeamPending {
    handle: u32,
    craft: usize,
    team: i32,
    activation: u64,
}
type TeamNative =
    unsafe extern "C" fn(*mut c_void, u32, usize, usize, usize, *mut TeamPending) -> u64;
type TeamInitStep = unsafe extern "C" fn(*mut c_void, u32) -> u8;
unsafe fn team_live(ready: u8, native: TeamNative, context: *mut c_void) -> u64 {
    if ready == 0 {
        0
    } else {
        unsafe { native(context, 0, 0, 0, 0, core::ptr::null_mut()) }
    }
}
/// Safety: callback remains valid for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_activation(
    ready: u8,
    native: Option<TeamNative>,
    context: *mut c_void,
) -> u64 {
    native.map_or(0, |native| unsafe { team_live(ready, native, context) })
}
fn team_enabled_value(mut value: &[u8]) -> bool {
    while value.first().is_some_and(|&b| unsafe { crate_isspace(b) }) {
        value = &value[1..];
    }
    while value.last().is_some_and(|&b| unsafe { crate_isspace(b) }) {
        value = &value[..value.len() - 1];
    }
    if value.len() >= 2 && value[0] == b'"' && value[value.len() - 1] == b'"' {
        value = &value[1..value.len() - 1];
    }
    value == b"1" || equals_ascii_case(value, b"true")
}
unsafe extern "C" {
    fn isspace(value: c_int) -> c_int;
}
unsafe fn crate_isspace(value: u8) -> bool {
    unsafe { isspace(value as c_int) != 0 }
}
/// Safety: callback spans remain valid through this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_class_loaded(
    ready: u8,
    valid_event: u8,
    weapon_class: usize,
    field_count: u32,
    field: Option<FieldGet>,
    native: Option<TeamNative>,
    context: *mut c_void,
) {
    if ready == 0 || valid_event == 0 || weapon_class == 0 {
        return;
    }
    let (Some(field), Some(native)) = (field, native) else {
        return;
    };
    let mut enabled = false;
    for i in 0..field_count {
        let mut current = Field {
            name: Bytes {
                data: core::ptr::null(),
                size: 0,
            },
            value: Bytes {
                data: core::ptr::null(),
                size: 0,
            },
            value_valid: 0,
        };
        if unsafe { field(context, i, &mut current) } == 0 {
            break;
        }
        let name = unsafe { current.name.get() }.unwrap_or(&[]);
        if equals_ascii_case(name, b"activateOnTeamChange") {
            enabled = team_enabled_value(unsafe { current.value.get() }.unwrap_or(&[]));
        }
    }
    unsafe {
        native(
            context,
            23,
            weapon_class,
            usize::from(enabled),
            0,
            core::ptr::null_mut(),
        )
    };
}
/// Safety: native callback is valid for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_craft_event(
    valid_event: u8,
    kind: u32,
    craft: usize,
    native: Option<TeamNative>,
    context: *mut c_void,
) {
    if valid_event != 0
        && matches!(kind, 2 | 3)
        && let Some(native) = native
    {
        unsafe { native(context, 13, craft, 0, 0, core::ptr::null_mut()) };
    }
}
unsafe fn configured(
    craft: usize,
    native: TeamNative,
    context: *mut c_void,
    output: &mut [usize; 256],
) -> usize {
    let call =
        |op, a, b, c| unsafe { native(context, op, a, b, c, core::ptr::null_mut()) as usize };
    let handle = call(2, craft, 0, 0);
    if call(3, handle, 0, 0) != craft || craft == 0 || call(4, craft, 0, 0) != 0 {
        return 0;
    }
    let carrier = call(5, craft, 0, 0);
    let begin = call(6, carrier, 0, 0);
    let end = call(7, carrier, 0, 0);
    if begin == 0
        || end < begin
        || (end - begin) % core::mem::size_of::<usize>() != 0
        || (end - begin) / core::mem::size_of::<usize>() > 256
        || call(8, begin, end - begin, 0) == 0
    {
        return 0;
    }
    let mut count = 0;
    for i in 0..(end - begin) / core::mem::size_of::<usize>() {
        let weapon = call(9, begin, i, 0);
        if weapon != 0
            && call(10, weapon, 0, 0) == handle
            && call(12, call(11, weapon, 0, 0), 0, 0) != 0
        {
            output[count] = weapon;
            count += 1;
        }
    }
    count
}
/// Safety: callback and native objects remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_ownership(
    craft: usize,
    before: i32,
    activation: u64,
    ready: u8,
    native: Option<TeamNative>,
    context: *mut c_void,
) {
    let Some(native) = native else {
        return;
    };
    if activation == 0
        || activation != unsafe { team_live(ready, native, context) }
        || unsafe { native(context, 22, 0, 0, 0, core::ptr::null_mut()) } != 0
    {
        return;
    }
    let after = unsafe { native(context, 1, craft, 0, 0, core::ptr::null_mut()) } as i32;
    if !(0..=63).contains(&before) || !(0..=63).contains(&after) || before == after {
        return;
    }
    unsafe { native(context, 13, craft, 0, 0, core::ptr::null_mut()) };
    let mut weapons = [0_usize; 256];
    if unsafe { configured(craft, native, context, &mut weapons) } != 0 {
        let pending = TeamPending {
            handle: unsafe { native(context, 2, craft, 0, 0, core::ptr::null_mut()) } as u32,
            craft,
            team: after,
            activation,
        };
        unsafe {
            native(
                context,
                14,
                0,
                0,
                0,
                (&pending as *const TeamPending).cast_mut(),
            )
        };
    }
}
unsafe fn dispatch(ready: u8, native: TeamNative, context: *mut c_void) {
    let call =
        |op, a, b, c| unsafe { native(context, op, a, b, c, core::ptr::null_mut()) as usize };
    call(15, 0, 0, 0); // Native swaps persistent queue into caller-owned pass snapshot.
    let activation = unsafe { team_live(ready, native, context) };
    if activation == 0 {
        return;
    }
    let count = call(16, 0, 0, 0);
    for i in 0..count {
        if activation != unsafe { team_live(ready, native, context) } {
            return;
        }
        let mut item = TeamPending {
            handle: 0,
            craft: 0,
            team: -1,
            activation: 0,
        };
        unsafe { native(context, 17, i, 0, 0, &mut item) };
        if item.activation != activation {
            continue;
        }
        let craft = call(3, item.handle as usize, 0, 0);
        if craft != item.craft || craft == 0 || call(1, craft, 0, 0) as i32 != item.team {
            continue;
        }
        let mut weapons = [0_usize; 256];
        let weapon_count = unsafe { configured(craft, native, context, &mut weapons) };
        let mut requested = false;
        for &weapon in &weapons[..weapon_count] {
            if activation != unsafe { team_live(ready, native, context) } {
                return;
            }
            if call(18, weapon, 0, 0) != 0 {
                continue;
            }
            let class = call(11, weapon, 0, 0);
            let target = if call(19, class, 0, 0) != 0 {
                call(20, weapon, 0, 0)
            } else {
                0
            };
            call(21, weapon, target, 0);
            requested = true;
        }
        if requested && call(26, 0, 0, 0) != 0 {
            call(29, 0, 0, 0);
        }
    }
}
/// Returns whether dispatch was attempted. Native simulation always follows.
/// Safety: callback and pass snapshot are valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_simulate(
    ready: u8,
    elapsed: f32,
    native: Option<TeamNative>,
    context: *mut c_void,
) -> u8 {
    if ready == 0 || !elapsed.is_finite() || elapsed <= 0.0 {
        return 0;
    }
    let Some(native) = native else {
        return 0;
    };
    unsafe { dispatch(ready, native, context) };
    1
}
/// Safety: native callbacks remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_init(
    valid_api: u8,
    step: Option<TeamInitStep>,
    log: Option<Log>,
    context: *mut c_void,
    ready: *mut u8,
) -> u8 {
    if valid_api == 0 || ready.is_null() {
        return 0;
    }
    let (Some(step), Some(log)) = (step, log) else {
        return 0;
    };
    unsafe { ready.write(0) };
    if unsafe { step(context, 0) } == 0 || unsafe { step(context, 1) } == 0 {
        unsafe { log(context, 0) };
        return 0;
    }
    if unsafe { step(context, 2) } == 0 || unsafe { step(context, 3) } == 0 {
        return 0;
    }
    let installed = unsafe { step(context, 4) } != 0
        && unsafe { step(context, 5) } != 0
        && unsafe { step(context, 6) } != 0;
    unsafe {
        ready.write(u8::from(installed));
        log(context, if installed { 1 } else { 2 })
    };
    1
}
/// Safety: native callback is valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_before(
    activation: u64,
    craft: usize,
    native: Option<TeamNative>,
    context: *mut c_void,
) -> i32 {
    if activation == 0 {
        return -1;
    }
    native.map_or(
        -1,
        |native| unsafe { native(context, 1, craft, 0, 0, core::ptr::null_mut()) } as i32,
    )
}
/// Safety: native callback is valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_team_shutdown(
    native: Option<TeamNative>,
    context: *mut c_void,
) {
    if let Some(native) = native {
        unsafe {
            native(context, 28, 0, 0, 0, core::ptr::null_mut());
            native(context, 27, 0, 0, 0, core::ptr::null_mut())
        };
    }
}
