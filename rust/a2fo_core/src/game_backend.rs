// Portable D3D8 observer and D3D11 backend policy.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Rect {
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Point {
    x: i32,
    y: i32,
}
#[repr(C)]
pub struct Region {
    left: u32,
    top: u32,
    right: u32,
    bottom: u32,
    x: u32,
    y: u32,
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_dimensions(w: u32, h: u32, max: u32) -> bool {
    w != 0 && h != 0 && w <= max && h <= max
}
// 0 unknown, 1 RGBA8, 2 D16, 3 D24S8, 4 D32.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_format(legacy: u32, depth: bool) -> u32 {
    if !depth && matches!(legacy, 21 | 22) {
        1
    } else if depth && matches!(legacy, 70 | 80) {
        2
    } else if depth && matches!(legacy, 75 | 77) {
        3
    } else if depth && legacy == 71 {
        4
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_lost(status: i32, removed: i32, reset: i32) -> bool {
    status == removed || status == reset
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_remaining(budget: u64, used: u64) -> u64 {
    budget.saturating_sub(used)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_available(
    local_ok: bool,
    local_budget: u64,
    local_used: u64,
    nonlocal_ok: bool,
    nonlocal_budget: u64,
    nonlocal_used: u64,
) -> u32 {
    let maximum = u32::MAX as u64;
    let available = if local_ok {
        a2fo_backend_remaining(local_budget, local_used)
    } else {
        0
    };
    if available >= maximum {
        return u32::MAX;
    }
    let extra = if nonlocal_ok {
        a2fo_backend_remaining(nonlocal_budget, nonlocal_used)
    } else {
        0
    };
    (available + extra.min(maximum - available)) as u32
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_copy_region(
    r: Rect,
    p: Point,
    sw: u32,
    sh: u32,
    dw: u32,
    dh: u32,
    same: bool,
    out: *mut Region,
) -> bool {
    if out.is_null() {
        return false;
    }
    let w = r.right as i64 - r.left as i64;
    let h = r.bottom as i64 - r.top as i64;
    if r.left < 0
        || r.top < 0
        || w <= 0
        || h <= 0
        || r.right > sw as i32
        || r.bottom > sh as i32
        || p.x < 0
        || p.y < 0
        || p.x as i64 + w > dw as i64
        || p.y as i64 + h > dh as i64
    {
        return false;
    }
    if same
        && (r.left as i64) < p.x as i64 + w
        && p.x < r.right
        && (r.top as i64) < p.y as i64 + h
        && p.y < r.bottom
    {
        return false;
    }
    unsafe {
        *out = Region {
            left: r.left as u32,
            top: r.top as u32,
            right: r.right as u32,
            bottom: r.bottom as u32,
            x: p.x as u32,
            y: p.y as u32,
        };
    }
    true
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_clear_bounds(
    x: u32,
    y: u32,
    w: u32,
    h: u32,
    tw: u32,
    th: u32,
) -> Rect {
    Rect {
        left: x.min(tw) as i32,
        top: y.min(th) as i32,
        right: ((x as u64 + w as u64).min(tw as u64)) as i32,
        bottom: ((y as u64 + h as u64).min(th as u64)) as i32,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_intersect(a: Rect, b: Rect) -> Rect {
    Rect {
        left: a.left.max(b.left),
        top: a.top.max(b.top),
        right: a.right.min(b.right),
        bottom: a.bottom.min(b.bottom),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_whole_clear(
    areas: *const Rect,
    count: usize,
    w: u32,
    h: u32,
) -> bool {
    if count != 1 || areas.is_null() {
        return false;
    }
    let a = unsafe { *areas };
    a.left == 0 && a.top == 0 && a.right == w as i32 && a.bottom == h as i32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_mip_compatible(
    format: u32,
    first: u32,
    w: u32,
    h: u32,
    prev_w: u32,
    prev_h: u32,
) -> bool {
    format == first && w == (prev_w / 2).max(1) && h == (prev_h / 2).max(1)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_draw_viewport(
    x: u32,
    y: u32,
    w: u32,
    h: u32,
    tw: u32,
    th: u32,
) -> bool {
    w != 0
        && h != 0
        && x < tw
        && y < th
        && (x as u64 + w as u64) <= tw as u64
        && (y as u64 + h as u64) <= th as u64
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_copy_rows(
    dst: *mut u8,
    src: *const u8,
    row_bytes: usize,
    row_pitch: usize,
    height: u32,
) {
    if dst.is_null() || src.is_null() {
        return;
    }
    for y in 0..height as usize {
        unsafe {
            core::ptr::copy_nonoverlapping(
                src.add(y * row_pitch),
                dst.add(y * row_bytes),
                row_bytes,
            );
        }
    }
}
pub type CopyInput = extern "C" fn(*mut core::ffi::c_void, u32, *mut Rect, *mut Point);
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_copy_plan(
    ctx: *mut core::ffi::c_void,
    count: u32,
    sw: u32,
    sh: u32,
    dw: u32,
    dh: u32,
    same: bool,
    input: CopyInput,
    regions: *mut Region,
) -> bool {
    if regions.is_null() {
        return false;
    }
    let iterations = count.max(1);
    for i in 0..iterations {
        let mut rect = Rect {
            left: 0,
            top: 0,
            right: 0,
            bottom: 0,
        };
        let mut point = Point { x: 0, y: 0 };
        input(ctx, i, &mut rect, &mut point);
        let mut region = Region {
            left: 0,
            top: 0,
            right: 0,
            bottom: 0,
            x: 0,
            y: 0,
        };
        if !unsafe { a2fo_backend_copy_region(rect, point, sw, sh, dw, dh, same, &mut region) } {
            return false;
        }
        unsafe {
            *regions.add(i as usize) = region;
        }
    }
    true
}
pub type ClearInput = extern "C" fn(*mut core::ffi::c_void, u32, *mut Rect);
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_clear_plan(
    ctx: *mut core::ffi::c_void,
    bounds: Rect,
    count: u32,
    input: ClearInput,
    areas: *mut Rect,
    valid: *mut bool,
) -> u32 {
    if areas.is_null() || valid.is_null() {
        return 0;
    }
    let mut used = 0usize;
    if count == 0 {
        unsafe {
            *areas = bounds;
            *valid = bounds.right > bounds.left && bounds.bottom > bounds.top;
        }
        return 1;
    }
    for i in 0..count {
        let mut rect = Rect {
            left: 0,
            top: 0,
            right: 0,
            bottom: 0,
        };
        input(ctx, i, &mut rect);
        let intersection = a2fo_backend_intersect(bounds, rect);
        if intersection.right > intersection.left && intersection.bottom > intersection.top {
            unsafe {
                *areas.add(used) = intersection;
            }
            used += 1;
        }
    }
    unsafe {
        *valid = used != 0;
    }
    used as u32
}
pub type PrepareMip =
    extern "C" fn(*mut core::ffi::c_void, u32, *mut i32, *mut u32, *mut u32, *mut u32, *mut bool);
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_prepare_mips(
    ctx: *mut core::ffi::c_void,
    count: u32,
    maximum: u32,
    prepare: PrepareMip,
    invalid: i32,
    exception_result: i32,
    threw: *mut bool,
) -> i32 {
    if count == 0 || count > maximum {
        return invalid;
    }
    let mut first = 0u32;
    let mut previous_w = 0u32;
    let mut previous_h = 0u32;
    for i in 0..count {
        let mut hr = 0i32;
        let mut format = 0u32;
        let mut width = 0u32;
        let mut height = 0u32;
        let mut exception = false;
        prepare(
            ctx,
            i,
            &mut hr,
            &mut format,
            &mut width,
            &mut height,
            &mut exception,
        );
        if exception {
            if !threw.is_null() {
                unsafe {
                    *threw = true;
                }
            }
            return exception_result;
        }
        if hr < 0 {
            return hr;
        }
        if i != 0
            && !a2fo_backend_mip_compatible(format, first, width, height, previous_w, previous_h)
        {
            return invalid;
        }
        if i == 0 {
            first = format;
        }
        previous_w = width;
        previous_h = height;
    }
    0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_mip_count_valid(count: u32, maximum: u32) -> bool {
    count != 0 && count <= maximum
}
#[repr(C)]
pub struct ObserverPeLayout {
    dos_size: u32,
    nt_size: u32,
    import_size: u32,
    thunk_size: u32,
    dos_magic_at: u32,
    dos_lfanew_at: u32,
    nt_signature_at: u32,
    nt_magic_at: u32,
    nt_machine_at: u32,
    nt_image_size_at: u32,
    nt_import_rva_at: u32,
    nt_import_size_at: u32,
    import_name_at: u32,
    import_original_at: u32,
    import_first_at: u32,
    dos_magic: u32,
    nt_signature: u32,
    nt_magic: u32,
    machine: u32,
    ordinal_mask: u32,
}
#[repr(C)]
pub struct ObserverPatchPlan {
    slot: usize,
    original: usize,
    duplicate: u32,
}
pub type ObserverSpan = extern "C" fn(*mut core::ffi::c_void, usize, usize) -> bool;
pub type ObserverRead = extern "C" fn(*mut core::ffi::c_void, usize, *mut u8, usize);
fn pe_u16(data: &[u8], at: u32) -> u16 {
    let i = at as usize;
    u16::from_le_bytes([data[i], data[i + 1]])
}
fn pe_u32(data: &[u8], at: u32) -> u32 {
    let i = at as usize;
    u32::from_le_bytes([data[i], data[i + 1], data[i + 2], data[i + 3]])
}
fn pe_field(size: u32, at: u32, len: u32) -> bool {
    at <= size && len <= size - at
}
fn pe_valid(
    base: usize,
    size: u32,
    rva: u32,
    len: u32,
    span: ObserverSpan,
    ctx: *mut core::ffi::c_void,
) -> bool {
    rva <= size && len <= size - rva && span(ctx, base.wrapping_add(rva as usize), len as usize)
}
fn pe_text(
    base: usize,
    size: u32,
    rva: u32,
    want: &[u8],
    span: ObserverSpan,
    read: ObserverRead,
    ctx: *mut core::ffi::c_void,
) -> bool {
    let len = want.len() as u32;
    if !pe_valid(base, size, rva, len + 1, span, ctx) {
        return false;
    }
    let mut end = 0u8;
    read(
        ctx,
        base.wrapping_add(rva as usize).wrapping_add(len as usize),
        &mut end,
        1,
    );
    if end != 0 {
        return false;
    }
    let mut bytes = [0u8; 64];
    if want.len() > bytes.len() {
        return false;
    }
    read(
        ctx,
        base.wrapping_add(rva as usize),
        bytes.as_mut_ptr(),
        want.len(),
    );
    for (actual, expected) in bytes[..want.len()].iter().zip(want) {
        if !actual.eq_ignore_ascii_case(expected) {
            return false;
        }
    }
    true
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_observer_patch_walk(
    base: usize,
    layout: *const ObserverPeLayout,
    span: ObserverSpan,
    read: ObserverRead,
    ctx: *mut core::ffi::c_void,
    callback: usize,
    previous_factory: usize,
    plan: *mut ObserverPatchPlan,
) -> bool {
    if base == 0 || layout.is_null() || plan.is_null() {
        return false;
    }
    let l = unsafe { &*layout };
    if l.dos_size > 512
        || l.nt_size > 512
        || l.import_size > 512
        || l.thunk_size > 512
        || !pe_field(l.dos_size, l.dos_magic_at, 2)
        || !pe_field(l.dos_size, l.dos_lfanew_at, 4)
        || !pe_field(l.nt_size, l.nt_signature_at, 4)
        || !pe_field(l.nt_size, l.nt_magic_at, 2)
        || !pe_field(l.nt_size, l.nt_machine_at, 2)
        || !pe_field(l.nt_size, l.nt_image_size_at, 4)
        || !pe_field(l.nt_size, l.nt_import_rva_at, 4)
        || !pe_field(l.nt_size, l.nt_import_size_at, 4)
        || !pe_field(l.import_size, l.import_name_at, 4)
        || !pe_field(l.import_size, l.import_original_at, 4)
        || !pe_field(l.import_size, l.import_first_at, 4)
        || l.thunk_size != 4
    {
        return false;
    }
    if !span(ctx, base, l.dos_size as usize) {
        return false;
    }
    let mut dos = [0u8; 512];
    read(ctx, base, dos.as_mut_ptr(), l.dos_size as usize);
    let offset = pe_u32(&dos, l.dos_lfanew_at) as i32;
    if pe_u16(&dos, l.dos_magic_at) as u32 != l.dos_magic || !(0..=0x100000).contains(&offset) {
        return false;
    }
    let nt_address = base.wrapping_add(offset as usize);
    if !span(ctx, nt_address, l.nt_size as usize) {
        return false;
    }
    let mut nt = [0u8; 512];
    read(ctx, nt_address, nt.as_mut_ptr(), l.nt_size as usize);
    if pe_u32(&nt, l.nt_signature_at) != l.nt_signature
        || pe_u16(&nt, l.nt_magic_at) as u32 != l.nt_magic
        || pe_u16(&nt, l.nt_machine_at) as u32 != l.machine
    {
        return false;
    }
    let size = pe_u32(&nt, l.nt_image_size_at);
    let directory = pe_u32(&nt, l.nt_import_rva_at);
    let directory_size = pe_u32(&nt, l.nt_import_size_at);
    if directory == 0 || !pe_valid(base, size, directory, directory_size, span, ctx) {
        return false;
    }
    let mut offset = 0u32;
    while offset <= directory_size && l.import_size <= directory_size - offset {
        let entry_address = base
            .wrapping_add(directory as usize)
            .wrapping_add(offset as usize);
        let mut entry = [0u8; 512];
        read(
            ctx,
            entry_address,
            entry.as_mut_ptr(),
            l.import_size as usize,
        );
        let name = pe_u32(&entry, l.import_name_at);
        if name == 0 {
            break;
        }
        let original = pe_u32(&entry, l.import_original_at);
        let first = pe_u32(&entry, l.import_first_at);
        if pe_text(base, size, name, b"d3d8.dll", span, read, ctx) && original != 0 && first != 0 {
            let mut index = 0u32;
            loop {
                let name_rva = original.wrapping_add(index.wrapping_mul(l.thunk_size));
                let slot_rva = first.wrapping_add(index.wrapping_mul(l.thunk_size));
                if !pe_valid(base, size, name_rva, l.thunk_size, span, ctx)
                    || !pe_valid(base, size, slot_rva, 4, span, ctx)
                {
                    break;
                }
                let mut thunk = [0u8; 4];
                read(
                    ctx,
                    base.wrapping_add(name_rva as usize),
                    thunk.as_mut_ptr(),
                    4,
                );
                let imported = u32::from_le_bytes(thunk);
                if imported == 0 {
                    break;
                }
                if imported & l.ordinal_mask == 0
                    && pe_valid(base, size, imported, 2, span, ctx)
                    && pe_text(
                        base,
                        size,
                        imported.wrapping_add(2),
                        b"Direct3DCreate8",
                        span,
                        read,
                        ctx,
                    )
                {
                    let slot = base.wrapping_add(slot_rva as usize);
                    let mut prior = [0u8; 4];
                    read(ctx, slot, prior.as_mut_ptr(), 4);
                    let previous = u32::from_le_bytes(prior) as usize;
                    if previous == callback {
                        unsafe {
                            *plan = ObserverPatchPlan {
                                slot,
                                original: previous,
                                duplicate: 1,
                            };
                        }
                        return true;
                    }
                    if previous == 0 || (previous_factory != 0 && previous != previous_factory) {
                        return false;
                    }
                    unsafe {
                        *plan = ObserverPatchPlan {
                            slot,
                            original: previous,
                            duplicate: 0,
                        };
                    }
                    return true;
                }
                if index == u32::MAX {
                    break;
                }
                index += 1;
            }
        }
        offset = offset.wrapping_add(l.import_size);
    }
    false
}
pub type ObserverStep = extern "C" fn(*mut core::ffi::c_void) -> bool;
pub type ObserverStage = extern "C" fn(*mut core::ffi::c_void) -> u32;
pub type ObserverVoid = extern "C" fn(*mut core::ffi::c_void);
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_observe_workflow(
    ctx: *mut core::ffi::c_void,
    prepare: ObserverStep,
    prior: ObserverStage,
    erase_prior: ObserverVoid,
    count: ObserverStage,
    dropped: ObserverVoid,
    cloned: ObserverStage,
    readable_table: ObserverStep,
    cache_allowed: ObserverStep,
    ensure_dispatch: ObserverStep,
    publish: ObserverStep,
) {
    if !prepare(ctx) {
        return;
    }
    match prior(ctx) {
        2 => return,
        1 => erase_prior(ctx),
        _ => {}
    }
    if count(ctx) >= 65536 {
        dropped(ctx);
        return;
    }
    match cloned(ctx) {
        2 => return,
        1 => {}
        _ => {
            if !readable_table(ctx) || !cache_allowed(ctx) || !ensure_dispatch(ctx) {
                return;
            }
        }
    }
    let _ = publish(ctx);
}
pub type BackendNativeStep = extern "C" fn(*mut core::ffi::c_void, u32, *mut i32, *mut bool);
fn backend_native(
    ctx: *mut core::ffi::c_void,
    step: u32,
    callback: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let mut result = 0;
    let mut exception = false;
    callback(ctx, step, &mut result, &mut exception);
    if exception && !threw.is_null() {
        unsafe {
            *threw = true;
        }
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_resize_workflow(
    ctx: *mut core::ffi::c_void,
    width: u32,
    height: u32,
    _windowed: bool,
    depth_format: u32,
    maximum: u32,
    invalid: i32,
    invalid_arg: i32,
    removed: i32,
    reset: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    let owner = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return owner;
    }
    if owner == 0 || !a2fo_backend_dimensions(width, height, maximum) {
        return invalid_arg;
    }
    if depth_format != 0 && a2fo_backend_format(depth_format, true) == 0 {
        let _ = backend_native(ctx, 11, native, threw);
        return invalid;
    }
    let cleared = backend_native(ctx, 2, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return cleared;
    }
    let fullscreen = backend_native(ctx, 3, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return fullscreen;
    }
    if fullscreen < 0 {
        let _ = backend_native(ctx, 4, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return fullscreen;
        }
        if a2fo_backend_lost(fullscreen, removed, reset) {
            let _ = backend_native(ctx, 10, native, threw);
        }
        return fullscreen;
    }
    let resized = backend_native(ctx, 5, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return resized;
    }
    if resized < 0 {
        if a2fo_backend_lost(resized, removed, reset) {
            let _ = backend_native(ctx, 10, native, threw);
        } else {
            let _ = backend_native(ctx, 6, native, threw);
        }
        return resized;
    }
    let committed = backend_native(ctx, 7, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return committed;
    }
    let made = backend_native(ctx, 8, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return made;
    }
    if made < 0 {
        let _ = backend_native(ctx, 9, native, threw);
    }
    made
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_observer_iid_kind(iid: *const u8, known: *const u8) -> i32 {
    if iid.is_null() || known.is_null() {
        return -1;
    }
    for kind in 0..10usize {
        let mut same = true;
        for i in 0..16usize {
            if unsafe { *iid.add(i) != *known.add(kind * 16 + i) } {
                same = false;
                break;
            }
        }
        if same {
            return kind as i32;
        }
    }
    -1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_texture_kind(resource: u32) -> i32 {
    match resource {
        3 => 2,
        5 => 4,
        4 => 3,
        _ => -1,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_trace_file_limit(
    used: usize,
    length: usize,
    maximum: usize,
    margin: usize,
) -> bool {
    length > maximum.wrapping_sub(margin).wrapping_sub(used)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_event_sample(
    sampled: bool,
    failed: bool,
    events: u32,
    maximum: u32,
) -> bool {
    (sampled || failed) && events < maximum
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_present_summary(frames: u32) -> bool {
    frames == 1 || frames.is_multiple_of(600)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_observer_backend_mode(
    name: *const u8,
    length: usize,
    child: bool,
) -> u32 {
    if name.is_null() {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(name, length) };
    if bytes.eq_ignore_ascii_case(b"d3d8") {
        return 1;
    }
    if bytes.eq_ignore_ascii_case(b"d3d11-probe") {
        return 2;
    }
    if child && bytes.eq_ignore_ascii_case(b"d3d11-test") {
        return 3;
    }
    0
}
pub type ObserverDiagnosticStep = extern "C" fn(*mut core::ffi::c_void, u32) -> bool;
pub type ObserverDiagnosticState = extern "C" fn(*mut core::ffi::c_void) -> u32;
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_diagnostic_workflow(
    ctx: *mut core::ffi::c_void,
    failed: bool,
    events: u32,
    maximum: u32,
    step: ObserverDiagnosticStep,
    state: ObserverDiagnosticState,
) {
    if !step(ctx, 0) {
        return;
    }
    let choice = state(ctx);
    let sampled = choice == 1;
    if !step(
        ctx,
        match choice {
            0 => 1,
            1 => 2,
            _ => 3,
        },
    ) {
        return;
    }
    if !step(ctx, 4) {
        return;
    }
    if a2fo_observer_event_sample(sampled, failed, events, maximum) && step(ctx, 5) {
        let _ = step(ctx, 6);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_initialize_workflow(
    ctx: *mut core::ffi::c_void,
    width: u32,
    height: u32,
    depth_format: u32,
    maximum: u32,
    debug: bool,
    invalid: i32,
    invalid_arg: i32,
    sdk_missing: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let owner = backend_native(ctx, 0, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return owner;
    }
    if owner == 0 || !a2fo_backend_dimensions(width, height, maximum) {
        let result = backend_native(ctx, 11, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return result;
        }
        return invalid_arg;
    }
    if depth_format != 0 && a2fo_backend_format(depth_format, true) == 0 {
        let result = backend_native(ctx, 12, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return result;
        }
        return invalid;
    }
    for step in [1, 2, 3, 4] {
        let result = backend_native(ctx, step, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return result;
        }
        if step != 4 && result < 0 {
            return result;
        }
        if step == 2 || step == 3 {
            let present = backend_native(ctx, step + 11, native, threw);
            if !threw.is_null() && unsafe { *threw } {
                return present;
            }
            if present == 0 {
                return result;
            }
        }
        if step == 4 {
            let mut hr = result;
            if debug && hr == sdk_missing {
                hr = backend_native(ctx, 5, native, threw);
                if !threw.is_null() && unsafe { *threw } {
                    return hr;
                }
            }
            if hr < 0 {
                return hr;
            }
        }
    }
    for step in 6..=10 {
        let result = backend_native(ctx, step, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return result;
        }
        if result < 0 {
            return result;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_present_workflow(
    ctx: *mut core::ffi::c_void,
    source: bool,
    destination: bool,
    override_mismatch: bool,
    dirty: bool,
    invalid: i32,
    removed: i32,
    reset: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    if source || destination || override_mismatch || dirty {
        return invalid;
    }
    let hr = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return hr;
    }
    if hr < 0 {
        let logged = backend_native(ctx, 2, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return logged;
        }
        if a2fo_backend_lost(hr, removed, reset) {
            let reason = backend_native(ctx, 3, native, threw);
            if !threw.is_null() && unsafe { *threw } {
                return reason;
            }
        }
    }
    hr
}
#[repr(C)]
pub struct MemoryPlan {
    available: u32,
    report_flags: u32,
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_memory_workflow(
    result: i32,
    local_status: i32,
    nonlocal_status: i32,
    local_budget: u64,
    local_used: u64,
    nonlocal_budget: u64,
    nonlocal_used: u64,
    unknown_logged: bool,
    log_only: bool,
) -> MemoryPlan {
    if result < 0 || (!log_only && local_status < 0 && nonlocal_status < 0) {
        return MemoryPlan {
            available: 0,
            report_flags: if unknown_logged { 0 } else { 1 },
        };
    }
    if log_only {
        let mut flags = 0;
        if local_status >= 0 {
            flags |= 4;
        }
        if nonlocal_status >= 0 {
            flags |= 8;
        }
        if !unknown_logged && (local_status < 0 || nonlocal_status < 0) {
            if local_status < 0 {
                flags |= 16;
            }
            if nonlocal_status < 0 {
                flags |= 32;
            }
            flags |= 2;
        }
        return MemoryPlan {
            available: 0,
            report_flags: flags,
        };
    }
    let mut flags = 0;
    if !unknown_logged && (local_status < 0 || nonlocal_status < 0) {
        flags |= 2;
    }
    MemoryPlan {
        available: a2fo_backend_available(
            local_status >= 0,
            local_budget,
            local_used,
            nonlocal_status >= 0,
            nonlocal_budget,
            nonlocal_used,
        ),
        report_flags: flags,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_probe_workflow(
    ctx: *mut core::ffi::c_void,
    enabled: bool,
    attempted: bool,
    find: ObserverStep,
    windowed: ObserverStep,
    claim: ObserverStep,
    client: ObserverStep,
    skipped: ObserverVoid,
    run: ObserverVoid,
) {
    if !enabled || attempted || !find(ctx) || !windowed(ctx) || !claim(ctx) {
        return;
    }
    if !client(ctx) {
        skipped(ctx);
        return;
    }
    run(ctx);
}
pub type ObserverBridge = extern "C" fn(*mut core::ffi::c_void, u32) -> bool;
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_bridge_workflow(
    ctx: *mut core::ffi::c_void,
    installed: bool,
    bc: bool,
    fleet: bool,
    bridge: ObserverBridge,
) -> bool {
    if !installed {
        return false;
    }
    if !bc {
        return true;
    }
    if !bridge(ctx, 0) {
        return false;
    }
    if fleet && (!bridge(ctx, 1) || !bridge(ctx, 2)) {
        return false;
    }
    true
}
pub type BackendClearSetupStep =
    extern "C" fn(*mut core::ffi::c_void, u32, u32, *mut i32, *mut bool);
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_clear_setup_workflow(
    ctx: *mut core::ffi::c_void,
    already_ready: bool,
    step: BackendClearSetupStep,
    threw: *mut bool,
) -> i32 {
    if already_ready {
        return 0;
    }
    for action in 0..12u32 {
        let index = if action == 7 {
            1
        } else {
            action.saturating_sub(8)
        };
        let operation = if action == 6 || action == 7 {
            6
        } else if action >= 8 {
            7
        } else {
            action
        };
        let mut hr = 0;
        let mut exception = false;
        step(ctx, operation, index, &mut hr, &mut exception);
        if exception {
            if !threw.is_null() {
                unsafe {
                    *threw = true;
                }
            }
            return hr;
        }
        if hr < 0 {
            let failed = hr;
            let mut cleanup = 0;
            let mut cleanup_exception = false;
            step(ctx, 8, 0, &mut cleanup, &mut cleanup_exception);
            if cleanup_exception {
                if !threw.is_null() {
                    unsafe {
                        *threw = true;
                    }
                }
                return cleanup;
            }
            return failed;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_make_back_workflow(
    ctx: *mut core::ffi::c_void,
    depth: bool,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    for step in [0, 1] {
        let hr = backend_native(ctx, step, native, threw);
        if hr < 0 || (!threw.is_null() && unsafe { *threw }) {
            return hr;
        }
    }
    if depth {
        for step in [2, 3] {
            let hr = backend_native(ctx, step, native, threw);
            if hr < 0 || (!threw.is_null() && unsafe { *threw }) {
                return hr;
            }
        }
    }
    backend_native(ctx, 4, native, threw)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_create_target_workflow(
    ctx: *mut core::ffi::c_void,
    width: u32,
    height: u32,
    legacy: u32,
    depth: bool,
    maximum: u32,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    if !a2fo_backend_dimensions(width, height, maximum) || a2fo_backend_format(legacy, depth) == 0 {
        return invalid;
    }
    for step in 1..=3 {
        let hr = backend_native(ctx, step, native, threw);
        if hr < 0 || (!threw.is_null() && unsafe { *threw }) {
            return hr;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_bind_target_workflow(
    ctx: *mut core::ffi::c_void,
    has_color: bool,
    has_depth: bool,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    if !has_color {
        return invalid;
    }
    let color = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return color;
    }
    if color == 0 {
        return invalid;
    }
    if has_depth {
        let depth = backend_native(ctx, 2, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return depth;
        }
        if depth == 0 {
            return invalid;
        }
    }
    backend_native(ctx, 3, native, threw)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_create_texture_workflow(
    ctx: *mut core::ffi::c_void,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let create = backend_native(ctx, 0, native, threw);
    if create < 0 || (!threw.is_null() && unsafe { *threw }) {
        return create;
    }
    let view = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return view;
    }
    if view >= 0 {
        let published = backend_native(ctx, 2, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return published;
        }
    }
    view
}
#[repr(C)]
pub struct UpdateMeta {
    desc_width: u32,
    desc_height: u32,
    expected_format: u32,
    prepared_format: u32,
    upload_width: u32,
    upload_height: u32,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_update_texture_workflow(
    ctx: *mut core::ffi::c_void,
    level: u32,
    mip_width: u32,
    mip_height: u32,
    meta: *mut UpdateMeta,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    if meta.is_null() {
        return invalid;
    }
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    let valid = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return valid;
    }
    if valid == 0 {
        return invalid;
    }
    let desc = backend_native(ctx, 2, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return desc;
    }
    let width = (unsafe { (*meta).desc_width } >> level).max(1);
    let height = (unsafe { (*meta).desc_height } >> level).max(1);
    unsafe {
        (*meta).upload_width = width;
        (*meta).upload_height = height;
    }
    if mip_width != width || mip_height != height {
        return invalid;
    }
    let prepared = backend_native(ctx, 3, native, threw);
    if prepared < 0 || (!threw.is_null() && unsafe { *threw }) {
        return prepared;
    }
    if unsafe { (*meta).prepared_format } != unsafe { (*meta).expected_format } {
        return invalid;
    }
    backend_native(ctx, 4, native, threw)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_texture_view_workflow(
    ctx: *mut core::ffi::c_void,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    let valid = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return valid;
    }
    if valid == 0 {
        return invalid;
    }
    backend_native(ctx, 2, native, threw)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_texture_target_workflow(
    ctx: *mut core::ffi::c_void,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    let valid = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return valid;
    }
    if valid == 0 {
        return invalid;
    }
    let cached = backend_native(ctx, 2, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return cached;
    }
    if cached == 0 {
        let created = backend_native(ctx, 3, native, threw);
        if created < 0 || (!threw.is_null() && unsafe { *threw }) {
            return created;
        }
        let stored = backend_native(ctx, 4, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return stored;
        }
    }
    backend_native(ctx, 5, native, threw)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_draw_workflow(
    ctx: *mut core::ffi::c_void,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    target_width: *const u32,
    target_height: *const u32,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    let valid = backend_native(ctx, 1, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return valid;
    }
    if valid == 0 {
        return invalid;
    }
    let bound = backend_native(ctx, 2, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return bound;
    }
    if target_width.is_null()
        || target_height.is_null()
        || !a2fo_backend_draw_viewport(x, y, width, height, unsafe { *target_width }, unsafe {
            *target_height
        })
    {
        return invalid;
    }
    let viewport = backend_native(ctx, 3, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return viewport;
    }
    let hr = backend_native(ctx, 4, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return hr;
    }
    if hr < 0 {
        let logged = backend_native(ctx, 5, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return logged;
        }
    }
    hr
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_read_target_workflow(
    ctx: *mut core::ffi::c_void,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    for step in [1, 2] {
        let valid = backend_native(ctx, step, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return valid;
        }
        if valid == 0 {
            return invalid;
        }
    }
    let created = backend_native(ctx, 3, native, threw);
    if created < 0 || (!threw.is_null() && unsafe { *threw }) {
        return created;
    }
    let copied = backend_native(ctx, 4, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return copied;
    }
    let mapped = backend_native(ctx, 5, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return mapped;
    }
    if mapped >= 0 {
        let output = backend_native(ctx, 6, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return output;
        }
    }
    let released = backend_native(ctx, 7, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return released;
    }
    mapped
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_status_plan(
    device: bool,
    context: bool,
    swap: bool,
    same_thread: bool,
    lost: bool,
) -> u32 {
    if !device || !context || !swap {
        1
    } else if !same_thread {
        2
    } else if lost {
        3
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_clear_admission(
    flags: u32,
    count: u32,
    rects: bool,
    color: bool,
    depth: bool,
    stencil: bool,
) -> bool {
    flags & !7 == 0
        && (count == 0 || rects)
        && color
        && (flags & 6 == 0 || depth)
        && (flags & 4 == 0 || stencil)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_clear_dispatch_workflow(
    ctx: *mut core::ffi::c_void,
    whole: bool,
    flags: u32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    if !whole {
        return backend_native(ctx, 0, native, threw);
    }
    if flags & 1 != 0 {
        let color = backend_native(ctx, 1, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return color;
        }
    }
    if flags & 6 != 0 {
        let depth = backend_native(ctx, 2, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return depth;
        }
    }
    0
}
pub type BackendEach = extern "C" fn(*mut core::ffi::c_void, u32) -> bool;
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_each(
    ctx: *mut core::ffi::c_void,
    count: u32,
    callback: BackendEach,
) -> bool {
    for index in 0..count {
        if !callback(ctx, index) {
            return false;
        }
    }
    true
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_copy_admission(
    ctx: *mut core::ffi::c_void,
    source_format: *const u32,
    dest_format: *const u32,
    count: u32,
    rects: bool,
    invalid: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    let status = backend_native(ctx, 0, native, threw);
    if status < 0 || (!threw.is_null() && unsafe { *threw }) {
        return status;
    }
    for step in 1..=3 {
        let valid = backend_native(ctx, step, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return valid;
        }
        if valid == 0 {
            return invalid;
        }
    }
    if source_format.is_null()
        || dest_format.is_null()
        || unsafe { *source_format } != unsafe { *dest_format }
        || unsafe { *source_format } != 28
        || (count != 0) != rects
    {
        return invalid;
    }
    0
}
pub type BackendCopyStep = extern "C" fn(*mut core::ffi::c_void, u32, u32, *mut i32, *mut bool);
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_copy_dispatch(
    ctx: *mut core::ffi::c_void,
    count: u32,
    step: BackendCopyStep,
    threw: *mut bool,
) -> i32 {
    for (action, index) in [(0, 0), (1, 0)] {
        let mut result = 0;
        let mut exception = false;
        step(ctx, action, index, &mut result, &mut exception);
        if exception {
            if !threw.is_null() {
                unsafe {
                    *threw = true;
                }
            }
            return result;
        }
    }
    for index in 0..count {
        let mut result = 0;
        let mut exception = false;
        step(ctx, 2, index, &mut result, &mut exception);
        if exception {
            if !threw.is_null() {
                unsafe {
                    *threw = true;
                }
            }
            return result;
        }
    }
    let mut result = 0;
    let mut exception = false;
    step(ctx, 3, 0, &mut result, &mut exception);
    if exception {
        if !threw.is_null() {
            unsafe {
                *threw = true;
            }
        }
        return result;
    }
    0
}
pub type ObserverFactoryStep = extern "C" fn(*mut core::ffi::c_void, u32, *mut usize, *mut bool);
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_observer_factory_workflow(
    ctx: *mut core::ffi::c_void,
    game_test: bool,
    game_ready: bool,
    step: ObserverFactoryStep,
    threw: *mut bool,
) -> usize {
    if game_test && !game_ready {
        let mut result = 0;
        let mut exception = false;
        step(ctx, 0, &mut result, &mut exception);
        if exception && !threw.is_null() {
            unsafe {
                *threw = true;
            }
        }
        return 0;
    }
    let mut native = 0;
    let mut exception = false;
    step(ctx, 1, &mut native, &mut exception);
    if exception {
        if !threw.is_null() {
            unsafe {
                *threw = true;
            }
        }
        return 0;
    }
    let mut result = native;
    step(
        ctx,
        if game_test { 2 } else { 3 },
        &mut result,
        &mut exception,
    );
    if exception {
        if !threw.is_null() {
            unsafe {
                *threw = true;
            }
        }
        return 0;
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_backend_query_memory_workflow(
    ctx: *mut core::ffi::c_void,
    device: bool,
    unexpected: i32,
    native: BackendNativeStep,
    threw: *mut bool,
) -> i32 {
    if !device {
        return unexpected;
    }
    let mut hr = 0;
    for step in 0..3u32 {
        hr = backend_native(ctx, step, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return hr;
        }
        if hr < 0 {
            break;
        }
    }
    if hr >= 0 {
        let local = backend_native(ctx, 3, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return local;
        }
        let nonlocal = backend_native(ctx, 4, native, threw);
        if !threw.is_null() && unsafe { *threw } {
            return nonlocal;
        }
        if local < 0 && nonlocal < 0 {
            hr = local;
        }
    }
    let cleanup = backend_native(ctx, 5, native, threw);
    if !threw.is_null() && unsafe { *threw } {
        return cleanup;
    }
    hr
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_clear_active(bounds: Rect, flags: u32) -> bool {
    bounds.right > bounds.left && bounds.bottom > bounds.top && flags != 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_backend_texture_bind_flags(
    format: u32,
    shader_resource: u32,
    render_target: u32,
) -> u32 {
    if format == 28 {
        shader_resource | render_target
    } else {
        shader_resource
    }
}
#[repr(C)]
pub struct ObserverActivation {
    installed: u32,
    fo_installed: u32,
    files: u32,
    ready: u32,
    result: u32,
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_activation_workflow(
    ctx: *mut core::ffi::c_void,
    game_test: bool,
    bc: bool,
    fleet: bool,
    patch: ObserverBridge,
    bridge: ObserverBridge,
) -> ObserverActivation {
    let installed = patch(ctx, 0);
    let fo_installed = patch(ctx, 1);
    let files = if game_test {
        a2fo_observer_bridge_workflow(ctx, installed, bc, fleet, bridge)
    } else {
        false
    };
    let ready = game_test && installed && files;
    ObserverActivation {
        installed: installed as u32,
        fo_installed: fo_installed as u32,
        files: files as u32,
        ready: ready as u32,
        result: (installed && (!game_test || ready)) as u32,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_max_events(configured: u32) -> u32 {
    configured.clamp(1, 100000)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_observer_should_install(trace: bool, probe: bool, game_test: bool) -> bool {
    trace || probe || game_test
}
