//! Multi-yard and muzzle runtime decisions; callbacks retain native ABI work.
use alloc::vec::Vec;
use core::{ffi::c_void, slice};
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    size: usize,
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
fn span(value: &[u8]) -> Bytes {
    Bytes {
        data: value.as_ptr(),
        size: value.len(),
    }
}
fn trim(mut value: &[u8]) -> &[u8] {
    while value.first().is_some_and(|ch| b" \t\r\n".contains(ch)) {
        value = &value[1..]
    }
    while value.last().is_some_and(|ch| b" \t\r\n".contains(ch)) {
        value = &value[..value.len() - 1]
    }
    value
}
type YardConfig = unsafe extern "C" fn(*mut c_void, u32, *mut Bytes) -> u32;
type YardRead = unsafe extern "C" fn(*mut c_void, usize, u32) -> u32;
type YardNative = unsafe extern "C" fn(*mut c_void, u32, usize, u32) -> usize;
type YardLog = unsafe extern "C" fn(*mut c_void, u32);
type YardField = unsafe extern "C" fn(*mut c_void, u32, *mut Bytes) -> u8;
type YardCompare = unsafe extern "C" fn(*mut c_void, Bytes, Bytes) -> u8;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Candidate {
    handle: u32,
    queued: u32,
}
unsafe fn yard_config(count: u32, config: YardConfig, log: YardLog, ctx: *mut c_void) -> bool {
    if count == 0 || count > 256 {
        return false;
    }
    let mut enabled = false;
    for i in 0..count {
        let mut value = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        match unsafe { config(ctx, i, &mut value) } {
            0 => {}
            1 => match unsafe { value.get() } {
                Some(b"1") => enabled = true,
                Some(b"0") => enabled = false,
                _ => {
                    unsafe { log(ctx, 0) };
                    return false;
                }
            },
            _ => return false,
        }
    }
    enabled
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_config(
    count: u32,
    config: Option<YardConfig>,
    log: Option<YardLog>,
    ctx: *mut c_void,
) -> u8 {
    match (config, log) {
        (Some(config), Some(log)) => u8::from(unsafe { yard_config(count, config, log, ctx) }),
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_class(
    valid: u8,
    object_class: usize,
    count: u32,
    field: Option<YardField>,
    compare: Option<YardCompare>,
    native: Option<YardNative>,
    ctx: *mut c_void,
) {
    if valid == 0 || object_class == 0 {
        return;
    }
    let (Some(field), Some(compare), Some(native)) = (field, compare, native) else {
        return;
    };
    let mut yard = false;
    for i in 0..count {
        let (mut name, mut value) = (
            Bytes {
                data: core::ptr::null(),
                size: 0,
            },
            Bytes {
                data: core::ptr::null(),
                size: 0,
            },
        );
        if unsafe { field(ctx, i * 2, &mut name) } == 0
            || unsafe { field(ctx, i * 2 + 1, &mut value) } == 0
        {
            continue;
        }
        let (Some(name), Some(value)) = (unsafe { name.get() }, unsafe { value.get() }) else {
            continue;
        };
        if unsafe { compare(ctx, span(name), span(b"classLabel")) } != 0 {
            let value = trim(value);
            yard = unsafe { compare(ctx, span(value), span(b"shipyard")) } != 0
                || unsafe { compare(ctx, span(value), span(b"\"shipyard\"")) } != 0;
        }
    }
    unsafe { native(ctx, 13, object_class, u32::from(yard)) };
}
unsafe fn one_kind(
    active: bool,
    objects: usize,
    n: i32,
    min: i32,
    read: YardRead,
    native: YardNative,
    ctx: *mut c_void,
) -> bool {
    if !active
        || n < min
        || !(0..=30).contains(&n)
        || unsafe { read(ctx, objects, 0x10000 + (n as u32) * 4) } == 0
    {
        return false;
    }
    let team = unsafe { native(ctx, 0, 0, 0) } as i32;
    let first = unsafe { read(ctx, read(ctx, objects, 0x20000) as usize, 3) };
    if first == 0 {
        return false;
    }
    for i in 0..n as u32 {
        let object = unsafe { read(ctx, objects, 0x20000 + i) } as usize;
        if object == 0
            || team < 0
            || unsafe { read(ctx, object, 4) } as i32 != team
            || unsafe { read(ctx, object, 5) } != 0
            || unsafe { read(ctx, object, 6) } & 4 == 0
        {
            return false;
        }
        let id = unsafe { read(ctx, object, 7) };
        if id == 0
            || unsafe { native(ctx, 1, 0, id) } != object
            || unsafe { native(ctx, 2, read(ctx, object, 3) as usize, 0) } == 0
            || unsafe { read(ctx, object, 3) } != first
        {
            return false;
        }
        for j in 0..i {
            if unsafe { read(ctx, objects, 0x20000 + j) } as usize == object {
                return false;
            }
        }
    }
    true
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_one_kind(
    active: u8,
    objects: usize,
    n: i32,
    min: i32,
    read: Option<YardRead>,
    native: Option<YardNative>,
    ctx: *mut c_void,
) -> u8 {
    match (read, native) {
        (Some(r), Some(f)) => {
            u8::from(unsafe { one_kind(active != 0, objects, n, min, r, f, ctx) })
        }
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_gate(
    mode: u32,
    arr: usize,
    _n: i32,
    popup: usize,
    read: Option<YardRead>,
    native: Option<YardNative>,
    ctx: *mut c_void,
) -> i32 {
    let (Some(read), Some(native)) = (read, native) else {
        return 0;
    };
    if unsafe { read(ctx, arr, 0x1000c) } == 0 {
        return 0;
    }
    let count = unsafe { read(ctx, arr, 1) } as i32;
    if count == 1 {
        return 1;
    }
    if mode == 1 && unsafe { read(ctx, popup, 8) } != 2 {
        return 0;
    }
    i32::from(unsafe {
        one_kind(
            native(ctx, 10, 0, 0) != 0,
            read(ctx, arr, 2) as usize,
            count,
            2,
            read,
            native,
            ctx,
        )
    })
}
fn valid(rows: &[Candidate]) -> bool {
    !rows.is_empty()
        && rows.len() <= 30
        && rows.iter().enumerate().all(|(i, row)| {
            row.handle != 0 && !rows[..i].iter().any(|prev| prev.handle == row.handle)
        })
}
fn choose_build(rows: &[Candidate], last: u32) -> Option<usize> {
    if !valid(rows) {
        return None;
    }
    let start = rows
        .iter()
        .position(|row| row.handle == last)
        .map_or(0, |i| (i + 1) % rows.len());
    let mut best = start;
    for offset in 1..rows.len() {
        let i = (start + offset) % rows.len();
        if rows[i].queued < rows[best].queued {
            best = i;
        }
    }
    Some(best)
}
fn choose_cancel(rows: &[Candidate]) -> Option<usize> {
    if !valid(rows) {
        return None;
    }
    let mut best = 0;
    for i in 1..rows.len() {
        if rows[i].queued > rows[best].queued {
            best = i;
        }
    }
    Some(best)
}
unsafe fn rows(
    objects: usize,
    n: i32,
    read: YardRead,
    native: YardNative,
    ctx: *mut c_void,
) -> Vec<Candidate> {
    let mut result = Vec::with_capacity(n as usize);
    for i in 0..n as u32 {
        let object = unsafe { read(ctx, objects, 0x20000 + i) } as usize;
        let extension = unsafe { native(ctx, 3, object, 0) } as u32;
        let queued = if extension != u32::MAX {
            extension
        } else {
            (unsafe { native(ctx, 18, object, 0) }) as u32
        };
        result.push(Candidate {
            handle: unsafe { read(ctx, object, 7) },
            queued,
        });
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_route(
    objects: usize,
    n: i32,
    klass: usize,
    read: Option<YardRead>,
    native: Option<YardNative>,
    ctx: *mut c_void,
) -> i32 {
    let (Some(read), Some(native)) = (read, native) else {
        return 0;
    };
    if !(0..=30).contains(&n)
        || (n != 0 && unsafe { read(ctx, objects, 0x10000 + n as u32 * 4) } == 0)
    {
        return 0;
    }
    let mut choice = None;
    if klass != 0
        && unsafe { one_kind(native(ctx, 10, 0, 0) != 0, objects, n, 2, read, native, ctx) }
    {
        let rows = unsafe { rows(objects, n, read, native, ctx) };
        // The native lock and last-handle word are retained across this pass.
        let last = unsafe { native(ctx, 14, 0, 0) } as u32;
        choice = choose_build(&rows, last);
        if let Some(i) = choice {
            unsafe { native(ctx, 15, 0, rows[i].handle) };
        }
        unsafe { native(ctx, 17, 0, 0) };
    }
    for i in 0..n as u32 {
        if choice.is_none_or(|best| best == i as usize) {
            let object = unsafe { read(ctx, objects, 0x20000 + i) } as usize;
            unsafe { native(ctx, 4, object, klass as u32) };
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_queue(
    arr: usize,
    _n: i32,
    filter: u8,
    read: Option<YardRead>,
    native: Option<YardNative>,
    ctx: *mut c_void,
) -> i32 {
    let (Some(read), Some(native)) = (read, native) else {
        return -1;
    };
    if filter != 0 && unsafe { read(ctx, 0, 9) } != 0x13 {
        return -1;
    }
    if unsafe { read(ctx, arr, 0x1000c) } == 0 {
        return -1;
    }
    let n = unsafe { read(ctx, arr, 1) } as i32;
    let objects = unsafe { read(ctx, arr, 2) } as usize;
    if !unsafe { one_kind(native(ctx, 10, 0, 0) != 0, objects, n, 2, read, native, ctx) } {
        return -1;
    }
    let rows = unsafe { rows(objects, n, read, native, ctx) };
    let Some(choice) = choose_cancel(&rows) else {
        return -1;
    };
    if filter != 0 {
        unsafe { native(ctx, 16, arr, choice as u32) };
    }
    rows[choice].queued as i32
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_selection(
    active: u8,
    selection: usize,
    object: usize,
    read: Option<YardRead>,
    native: Option<YardNative>,
    ctx: *mut c_void,
) -> i32 {
    let (Some(read), Some(native)) = (read, native) else {
        return 0;
    };
    if active == 0 || selection == 0 {
        return 0;
    }
    let n = unsafe { read(ctx, selection, 10) };
    if n == 0 || n > 30 {
        return 0;
    }
    let team = unsafe { native(ctx, 0, 0, 0) } as i32;
    if !unsafe { owned(object, team, read, native, ctx) } {
        return 0;
    }
    let mut objects = Vec::with_capacity(n as usize);
    for i in 0..n {
        objects.push(unsafe { native(ctx, 1, 0, read(ctx, selection, 0x30000 + i)) });
    }
    let first = objects[0];
    i32::from(
        unsafe {
            one_kind(
                true,
                objects.as_ptr() as usize,
                n as i32,
                1,
                read,
                native,
                ctx,
            )
        } && unsafe { read(ctx, first, 3) } == unsafe { read(ctx, object, 3) },
    )
}
unsafe fn owned(
    object: usize,
    team: i32,
    read: YardRead,
    native: YardNative,
    ctx: *mut c_void,
) -> bool {
    if object == 0
        || team < 0
        || unsafe { read(ctx, object, 4) } as i32 != team
        || unsafe { read(ctx, object, 5) } != 0
        || unsafe { read(ctx, object, 6) } & 4 == 0
    {
        return false;
    }
    let id = unsafe { read(ctx, object, 7) };
    id != 0
        && unsafe { native(ctx, 1, 0, id) } == object
        && unsafe { native(ctx, 2, read(ctx, object, 3) as usize, 0) } != 0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_init(
    valid: u8,
    native: Option<YardNative>,
    config: Option<YardConfig>,
    log: Option<YardLog>,
    ctx: *mut c_void,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    let (Some(native), Some(config), Some(log)) = (native, config, log) else {
        return 0;
    };
    unsafe { native(ctx, 5, 0, 0) };
    let count = unsafe { native(ctx, 12, 0, 0) } as u32;
    if !unsafe { yard_config(count, config, log, ctx) } {
        unsafe { log(ctx, 1) };
        return 1;
    }
    if unsafe { native(ctx, 6, 0, 0) } == 0 {
        unsafe { log(ctx, 2) };
        return 0;
    }
    if unsafe { native(ctx, 7, 0, 0) } == 0 {
        return 0;
    }
    unsafe { native(ctx, 8, 0, 0) };
    for i in 0..7 {
        if unsafe { native(ctx, 9, 0, i) } == 0 {
            unsafe { log(ctx, 3) };
            return 1;
        }
    }
    unsafe {
        native(ctx, 10, 0, 1);
        log(ctx, 4)
    };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_yard_shutdown(
    native: Option<YardNative>,
    ctx: *mut c_void,
) {
    if let Some(native) = native {
        unsafe { native(ctx, 11, 0, 0) };
    }
}

type MuzzleField = unsafe extern "C" fn(*mut c_void, u32, *mut Bytes, *mut Bytes) -> u8;
type MuzzleNumber = unsafe extern "C" fn(*mut c_void, Bytes, *mut f32, *mut u32) -> u8;
type MuzzleColour = unsafe extern "C" fn(*mut c_void, Bytes, *mut f32, *mut u8) -> u8;
type MuzzleStep = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> usize;
type MuzzleStore = unsafe extern "C" fn(*mut c_void, u32, Bytes, f32, f32, *const f32);
unsafe extern "C" {
    fn tolower(ch: i32) -> i32;
    fn isspace(ch: i32) -> i32;
}
fn equal_case(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len()
        && a.iter()
            .zip(b)
            .all(|(&x, &y)| unsafe { tolower(x as i32) == tolower(y as i32) })
}
unsafe fn field(name: &[u8], count: u32, get: MuzzleField, ctx: *mut c_void) -> Option<Vec<u8>> {
    let mut value = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    for i in 0..count {
        let mut found = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if unsafe { get(ctx, i, &mut found, &mut value) } == 0 {
            continue;
        }
        if found.size != name.len() {
            continue;
        }
        if !equal_case(unsafe { found.get() }?, name) {
            continue;
        }
        if value.data.is_null() && value.size != 0 {
            return None;
        }
        let mut value = trim(unsafe { value.get() }?).to_vec();
        if value.len() >= 2 && value[0] == b'"' && value[value.len() - 1] == b'"' {
            value.remove(0);
            value.pop();
        }
        return Some(value);
    }
    None
}
unsafe fn number(
    name: &[u8],
    count: u32,
    max: f32,
    default: f32,
    get: MuzzleField,
    parse: MuzzleNumber,
    ctx: *mut c_void,
) -> Option<f32> {
    let Some(value) = (unsafe { field(name, count, get, ctx) }) else {
        return Some(default);
    };
    if value.contains(&0) {
        return None;
    }
    let mut result = 0f32;
    let mut consumed = 0u32;
    if unsafe { parse(ctx, span(&value), &mut result, &mut consumed) } == 0
        || !result.is_finite()
        || result <= 0.0
        || result > max
    {
        return None;
    }
    let mut tail = value.get(consumed as usize..)?;
    if matches!(tail.first(), Some(b'f' | b'F')) {
        tail = &tail[1..];
    }
    if !tail.iter().all(|&ch| unsafe { isspace(ch as i32) != 0 }) {
        return None;
    }
    Some(result)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_muzzle_class(
    valid: u8,
    count: u32,
    get: Option<MuzzleField>,
    parse: Option<MuzzleNumber>,
    colour: Option<MuzzleColour>,
    store: Option<MuzzleStore>,
    ctx: *mut c_void,
) {
    if valid == 0 {
        return;
    }
    let (Some(get), Some(parse), Some(colour), Some(store)) = (get, parse, colour, store) else {
        return;
    };
    unsafe { store(ctx, 0, span(&[]), 0.0, 0.0, core::ptr::null()) };
    let Some(sprite) = (unsafe { field(b"muzzleFlashSprite", count, get, ctx) }) else {
        return;
    };
    if sprite.is_empty() {
        return;
    }
    if sprite.len() > 255 || sprite.contains(&0) {
        unsafe { store(ctx, 1, span(&[]), 0.0, 0.0, core::ptr::null()) };
        return;
    }
    let Some(size) = (unsafe { number(b"muzzleFlashSize", count, 10000.0, 8.0, get, parse, ctx) })
    else {
        unsafe { store(ctx, 1, span(&[]), 0.0, 0.0, core::ptr::null()) };
        return;
    };
    let Some(duration) =
        (unsafe { number(b"muzzleFlashDuration", count, 10.0, 0.12, get, parse, ctx) })
    else {
        unsafe { store(ctx, 1, span(&[]), 0.0, 0.0, core::ptr::null()) };
        return;
    };
    let mut channels = [1f32; 3];
    if let Some(value) = unsafe { field(b"muzzleFlashColor", count, get, ctx) } {
        let mut eof = 0u8;
        if unsafe { colour(ctx, span(&value), channels.as_mut_ptr(), &mut eof) } == 0
            || eof == 0
            || channels
                .iter()
                .any(|c| !c.is_finite() || *c < 0.0 || *c > 1.0)
        {
            unsafe { store(ctx, 1, span(&[]), 0.0, 0.0, core::ptr::null()) };
            return;
        }
    }
    unsafe { store(ctx, 2, span(&sprite), size, duration, channels.as_ptr()) };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_muzzle_emit(
    ready: u8,
    enabled: u8,
    weapon: usize,
    node: usize,
    step: Option<MuzzleStep>,
    ctx: *mut c_void,
) {
    if ready == 0 || enabled == 0 || weapon == 0 || node == 0 {
        return;
    }
    let Some(step) = step else {
        return;
    };
    let policy = unsafe { step(ctx, 0, weapon, 0) };
    if policy == 0 {
        return;
    }
    let owner = unsafe { step(ctx, 1, weapon, 0) };
    if owner == 0 || unsafe { step(ctx, 2, owner, 0) } != 0 || unsafe { step(ctx, 3, node, 0) } == 0
    {
        return;
    }
    let database = unsafe { step(ctx, 4, 0, 0) };
    if database == 0 {
        return;
    }
    if unsafe { step(ctx, 5, database, policy) } == 0 {
        unsafe { step(ctx, 6, policy, 0) };
        return;
    }
    unsafe { step(ctx, 7, policy, owner) };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_muzzle_init(
    valid: u8,
    step: Option<MuzzleStep>,
    ctx: *mut c_void,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    unsafe { step(ctx, 8, 0, 0) };
    if unsafe { step(ctx, 9, 0, 0) } == 0 {
        unsafe { step(ctx, 10, 0, 0) };
        return 0;
    }
    if unsafe { step(ctx, 11, 0, 0) } == 0 {
        return 0;
    }
    let active = unsafe { step(ctx, 12, 0, 0) };
    unsafe { step(ctx, 13, active, 0) };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_muzzle_shutdown(
    step: Option<MuzzleStep>,
    ctx: *mut c_void,
) {
    if let Some(step) = step {
        unsafe { step(ctx, 14, 0, 0) };
    }
}
