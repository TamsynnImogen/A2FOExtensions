//! Portable decisions for the D3D11 resource and diagnostic probe path.
//! Native handles and API calls remain in synchronous C++ callbacks.
use core::{ffi::c_void, ptr, slice};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Mip {
    pub(crate) width: u32,
    pub(crate) height: u32,
    pub(crate) row_pitch: u32,
    pub(crate) slice_pitch: u32,
    pub(crate) offset: usize,
}
#[repr(C)]
pub struct Image {
    pub(crate) format: u32,
    pub(crate) width: u32,
    pub(crate) height: u32,
    pub(crate) alpha: u32,
    pub(crate) mips: *const Mip,
    pub(crate) mip_count: usize,
    pub(crate) bytes: *const u8,
    pub(crate) byte_count: usize,
}
#[repr(C)]
pub struct Constants {
    pub(crate) pointer_error: i32,
    pub(crate) invalid_arg: i32,
    pub(crate) unexpected: i32,
    pub(crate) unsupported: i32,
    pub(crate) sdk_missing: i32,
    pub(crate) bc4_unorm: u32,
    pub(crate) bc4_snorm: u32,
    pub(crate) bc5_unorm: u32,
    pub(crate) bc5_snorm: u32,
    pub(crate) bc7_unorm: u32,
    pub(crate) bc7_srgb: u32,
    pub(crate) level10: u32,
    pub(crate) level11: u32,
    pub(crate) support_texture2d: u32,
    pub(crate) support_sample: u32,
    pub(crate) support_mip: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct TexturePlan {
    pub(crate) format: u32,
    pub(crate) width: u32,
    pub(crate) height: u32,
    pub(crate) mip_levels: u32,
    pub(crate) alpha: u32,
    pub(crate) block_bytes: u64,
    pub(crate) rgba8_bytes: u64,
    pub(crate) mips: [Mip; 15],
}
#[repr(C)]
#[derive(Default)]
pub struct ResultState {
    pub(crate) hr: i32,
    pub(crate) operation: u32,
    pub(crate) plan: TexturePlan,
}
#[repr(C)]
#[derive(Default)]
pub struct Reply {
    pub(crate) hr: i32,
    pub(crate) value: u32,
    pub(crate) handle: usize,
    pub(crate) value64: u64,
}
type Call = unsafe extern "C" fn(*mut c_void, u32, *const c_void, *mut Reply) -> i32;
fn failed(hr: i32) -> bool {
    hr < 0
}
fn call(ctx: *mut c_void, cb: Call, op: u32, input: *const c_void) -> Option<Reply> {
    let mut reply = Reply::default();
    (unsafe { cb(ctx, op, input, &mut reply) } != 0).then_some(reply)
}
fn fail(out: &mut ResultState, hr: i32, operation: u32) {
    out.hr = hr;
    out.operation = operation;
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_compressed_upload(
    image: *const Image,
    constants: *const Constants,
    has_device: i32,
    has_output: i32,
    context: *mut c_void,
    callback: Call,
    output: *mut ResultState,
) -> i32 {
    if image.is_null() || constants.is_null() || output.is_null() {
        return 0;
    }
    let image = unsafe { &*image };
    let constants = unsafe { &*constants };
    let out = unsafe { &mut *output };
    *out = ResultState::default();
    if has_output == 0 {
        fail(out, constants.pointer_error, 1);
        return 1;
    }
    if has_device == 0 {
        fail(out, constants.invalid_arg, 2);
        return 1;
    }
    let Some(validated) = call(context, callback, 1, ptr::null()) else {
        return 0;
    };
    if failed(validated.hr) {
        fail(out, validated.hr, 0);
        return 1;
    }
    let bc7 = image.format == constants.bc7_unorm || image.format == constants.bc7_srgb;
    let bc45 = image.format == constants.bc4_unorm
        || image.format == constants.bc4_snorm
        || image.format == constants.bc5_unorm
        || image.format == constants.bc5_snorm;
    if bc7 || bc45 {
        let Some(level) = call(context, callback, 2, ptr::null()) else {
            return 0;
        };
        if (bc7 && level.value < constants.level11) || (bc45 && level.value < constants.level10) {
            fail(out, constants.unsupported, 3);
            return 1;
        }
    }
    let Some(support) = call(context, callback, 3, ptr::null()) else {
        return 0;
    };
    if failed(support.hr) {
        fail(out, support.hr, 4);
        return 1;
    }
    let mut required = constants.support_texture2d | constants.support_sample;
    if image.mip_count > 1 {
        required |= constants.support_mip;
    }
    if support.value & required != required {
        fail(out, constants.unsupported, 5);
        return 1;
    }
    // The shared DDS validator accepts no more than fifteen mips. Its success
    // is the precondition for reading the borrowed span and filling the plan.
    if image.mip_count > 15 || (image.mip_count != 0 && image.mips.is_null()) {
        return 0;
    }
    let mips = if image.mip_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(image.mips, image.mip_count) }
    };
    out.plan.format = image.format;
    out.plan.width = image.width;
    out.plan.height = image.height;
    out.plan.mip_levels = image.mip_count as u32;
    out.plan.alpha = image.alpha;
    out.plan.block_bytes = image.byte_count as u64;
    for (index, mip) in mips.iter().enumerate() {
        out.plan.mips[index] = *mip;
        out.plan.rgba8_bytes += mip.width as u64 * mip.height as u64 * 4;
    }
    let Some(created) = call(
        context,
        callback,
        4,
        (&out.plan as *const TexturePlan).cast(),
    ) else {
        return 0;
    };
    if failed(created.hr) {
        fail(out, created.hr, 6);
        let _ = call(context, callback, 6, ptr::null());
        return 1;
    }
    let Some(view) = call(
        context,
        callback,
        5,
        (&out.plan as *const TexturePlan).cast(),
    ) else {
        let _ = call(context, callback, 6, ptr::null());
        return 0;
    };
    if failed(view.hr) {
        fail(out, view.hr, 7);
        let _ = call(context, callback, 7, ptr::null());
        let _ = call(context, callback, 6, ptr::null());
        return 1;
    }
    let _ = call(context, callback, 6, ptr::null());
    out.hr = 0;
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_handle_initialize(
    context: *mut c_void,
    callback: Call,
    result: *mut i32,
) -> i32 {
    if result.is_null() {
        return 0;
    }
    let Some(created) = call(context, callback, 8, ptr::null()) else {
        return 0;
    };
    unsafe { *result = created.hr };
    if failed(created.hr) {
        return 1;
    }
    if call(context, callback, 9, ptr::null()).is_none() {
        return 0;
    }
    unsafe { *result = 0 };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_handle_get_view(
    has_output: i32,
    has_view: i32,
    pointer_error: i32,
    unexpected: i32,
    callback_failure: i32,
    context: *mut c_void,
    callback: Call,
) -> i32 {
    if has_output == 0 {
        return pointer_error;
    }
    if has_view == 0 {
        return unexpected;
    }
    if call(context, callback, 10, ptr::null()).is_none() {
        return callback_failure;
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_handle_drop(
    has_view: i32,
    context: *mut c_void,
    callback: Call,
) {
    if has_view != 0 {
        let _ = call(context, callback, 11, ptr::null());
    }
}

#[repr(C)]
struct Path {
    bytes: *const u8,
    length: usize,
}
#[repr(C)]
struct PathBuffer {
    bytes: *mut u8,
    capacity: usize,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_probe_bridge(
    already_exists: u32,
    context: *mut c_void,
    callback: Call,
    result: *mut i32,
) -> i32 {
    if result.is_null() {
        return 0;
    }
    let Some(initialized) = call(context, callback, 12, ptr::null()) else {
        return 0;
    };
    unsafe { *result = initialized.hr };
    if failed(initialized.hr) {
        return 1;
    }
    let mut path = [0u8; 32768 + 64];
    let mut buffer = PathBuffer {
        bytes: path.as_mut_ptr(),
        capacity: 32768,
    };
    let Some(mut found) = call(
        context,
        callback,
        13,
        (&mut buffer as *mut PathBuffer).cast(),
    ) else {
        return 0;
    };
    if found.value == 0 || found.value as usize >= 32768 {
        found = match call(
            context,
            callback,
            14,
            (&mut buffer as *mut PathBuffer).cast(),
        ) {
            Some(reply) => reply,
            None => return 0,
        };
    }
    let mut root_len = 0;
    if found.value != 0 && (found.value as usize) < 32768 {
        let length = found.value as usize;
        root_len = path[..length]
            .iter()
            .position(|&c| c == 0)
            .unwrap_or(length);
        let first = b"\\A2FOExtensions";
        path[root_len..root_len + first.len()].copy_from_slice(first);
        root_len += first.len();
        path[root_len] = 0;
        let directory = Path {
            bytes: path.as_ptr(),
            length: root_len,
        };
        let Some(created) = call(context, callback, 15, (&directory as *const Path).cast()) else {
            return 0;
        };
        if created.value == 0 && created.hr as u32 != already_exists {
            root_len = 0;
        } else {
            let second = b"\\ShaderCache";
            path[root_len..root_len + second.len()].copy_from_slice(second);
            root_len += second.len();
            path[root_len] = 0;
            let directory = Path {
                bytes: path.as_ptr(),
                length: root_len,
            };
            let Some(created) = call(context, callback, 15, (&directory as *const Path).cast())
            else {
                return 0;
            };
            if created.value == 0 && created.hr as u32 != already_exists {
                root_len = 0;
            }
        }
    }
    let cache = Path {
        bytes: path.as_ptr(),
        length: root_len,
    };
    let Some(presented) = call(context, callback, 16, (&cache as *const Path).cast()) else {
        return 0;
    };
    unsafe { *result = presented.hr };
    1
}

#[repr(C)]
pub struct ProbeParams {
    width: u32,
    height: u32,
    debug_layer: u32,
    invalid_arg: i32,
    fail: i32,
    unexpected: i32,
    sdk_missing: i32,
    removed: i32,
    reset: i32,
    max_dimension: u32,
    level11: u32,
    rgba_format: u32,
    depth_format: u32,
    render_target_usage: u32,
    discard_swap: u32,
    depth_usage: u32,
    depth_bind: u32,
    debug_flag: u32,
    no_window_changes: u32,
    no_alt_enter: u32,
}
#[repr(C)]
#[derive(Default)]
pub struct ProbeRequest {
    id: u32,
    width: u32,
    height: u32,
    flags: u32,
    format: u32,
    usage: u32,
    bind: u32,
    mip_levels: u32,
    array_size: u32,
    sample_count: u32,
    buffer_count: u32,
    windowed: u32,
    swap_effect: u32,
    hr: i32,
    elapsed_ms: u64,
    viewport_width: f32,
    viewport_height: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Vertex {
    x: f32,
    y: f32,
    z: f32,
    rhw: f32,
    color: u32,
}
#[repr(C)]
pub struct Triangle {
    vertices: [Vertex; 3],
    primitive: u32,
    primitive_count: u32,
    fvf: u32,
    stride: u32,
    color_op: u32,
    alpha_op: u32,
    clear_color: u32,
}
fn probe_call(ctx: *mut c_void, cb: Call, op: u32) -> Option<Reply> {
    call(ctx, cb, op, ptr::null())
}
fn probe_request(ctx: *mut c_void, cb: Call, op: u32, request: &ProbeRequest) -> Option<Reply> {
    call(ctx, cb, op, (request as *const ProbeRequest).cast())
}
fn report(ctx: *mut c_void, cb: Call, id: u32, hr: i32) -> bool {
    probe_request(
        ctx,
        cb,
        23,
        &ProbeRequest {
            id,
            hr,
            ..ProbeRequest::default()
        },
    )
    .is_some()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_probe_initialize(
    params: *const ProbeParams,
    context: *mut c_void,
    callback: Call,
) -> i32 {
    if params.is_null() {
        return 0;
    }
    let p = unsafe { &*params };
    if probe_call(context, callback, 20).is_none() || probe_call(context, callback, 21).is_none() {
        return p.fail;
    }
    let Some(owner) = probe_call(context, callback, 22) else {
        return p.fail;
    };
    let process_id = owner.handle as u32;
    let current_process_id = (owner.value64 >> 32) as u32;
    let current_thread_id = owner.value64 as u32;
    if owner.value == 0
        || process_id != current_process_id
        || owner.value != current_thread_id
        || p.width == 0
        || p.height == 0
        || p.width > p.max_dimension
        || p.height > p.max_dimension
    {
        if !report(context, callback, 1, p.invalid_arg) {
            return p.fail;
        }
        let _ = probe_call(context, callback, 20);
        return p.invalid_arg;
    }
    let Some(loaded) = probe_call(context, callback, 24) else {
        return p.fail;
    };
    if failed(loaded.hr) {
        if !report(context, callback, 2, loaded.hr) {
            return p.fail;
        }
        let _ = probe_call(context, callback, 20);
        return loaded.hr;
    }
    if !report(context, callback, 2, 0) {
        return p.fail;
    }
    let Some(proc) = probe_call(context, callback, 25) else {
        return p.fail;
    };
    if failed(proc.hr) {
        if !report(context, callback, 3, proc.hr) {
            return p.fail;
        }
        let _ = probe_call(context, callback, 20);
        return proc.hr;
    }
    if !report(context, callback, 3, 0) {
        return p.fail;
    }
    let Some(start) = probe_call(context, callback, 26) else {
        return p.fail;
    };
    let mut create_plan = ProbeRequest {
        width: p.width,
        height: p.height,
        flags: if p.debug_layer != 0 { p.debug_flag } else { 0 },
        format: p.rgba_format,
        usage: p.render_target_usage,
        sample_count: 1,
        buffer_count: 1,
        windowed: 1,
        swap_effect: p.discard_swap,
        ..ProbeRequest::default()
    };
    let Some(mut created) = probe_request(context, callback, 27, &create_plan) else {
        return p.fail;
    };
    if !report(
        context,
        callback,
        if p.debug_layer != 0 { 4 } else { 5 },
        created.hr,
    ) {
        return p.fail;
    }
    if p.debug_layer != 0 && created.hr == p.sdk_missing {
        if probe_call(context, callback, 32).is_none()
            || probe_call(context, callback, 29).is_none()
            || probe_call(context, callback, 30).is_none()
            || probe_call(context, callback, 31).is_none()
        {
            return p.fail;
        }
        create_plan.flags = 0;
        created = match probe_request(context, callback, 27, &create_plan) {
            Some(result) => result,
            None => return p.fail,
        };
        if !report(context, callback, 6, created.hr) {
            return p.fail;
        }
    }
    let Some(end) = probe_call(context, callback, 26) else {
        return p.fail;
    };
    let elapsed = ProbeRequest {
        hr: created.hr,
        elapsed_ms: end.value64.wrapping_sub(start.value64),
        ..ProbeRequest::default()
    };
    if probe_request(context, callback, 33, &elapsed).is_none() {
        return p.fail;
    }
    let Some(verification) = probe_call(context, callback, 28) else {
        return p.fail;
    };
    if failed(created.hr) || verification.value & 7 != 7 || created.value < p.level11 {
        let hr = if failed(created.hr) {
            created.hr
        } else {
            p.fail
        };
        if !report(context, callback, 7, hr) {
            return p.fail;
        }
        let _ = probe_call(context, callback, 20);
        return hr;
    }
    let Some(mut hr) = probe_call(context, callback, 34) else {
        return p.fail;
    };
    if !report(context, callback, 8, hr.hr) {
        return p.fail;
    }
    if !failed(hr.hr) {
        hr = match probe_call(context, callback, 35) {
            Some(result) => result,
            None => return p.fail,
        };
        if !report(context, callback, 9, hr.hr) {
            return p.fail;
        }
    }
    if !failed(hr.hr) {
        hr = match probe_call(context, callback, 36) {
            Some(result) => result,
            None => return p.fail,
        };
        if !report(context, callback, 10, hr.hr) {
            return p.fail;
        }
    }
    if !failed(hr.hr) {
        let association = ProbeRequest {
            flags: p.no_window_changes | p.no_alt_enter,
            ..ProbeRequest::default()
        };
        hr = match probe_request(context, callback, 37, &association) {
            Some(result) => result,
            None => return p.fail,
        };
        if !report(context, callback, 11, hr.hr) {
            return p.fail;
        }
    }
    if probe_call(context, callback, 38).is_none()
        || probe_call(context, callback, 39).is_none()
        || probe_call(context, callback, 40).is_none()
    {
        return p.fail;
    }
    if failed(hr.hr) {
        let _ = probe_call(context, callback, 20);
        return hr.hr;
    }
    hr = match probe_call(context, callback, 41) {
        Some(result) => result,
        None => return p.fail,
    };
    if !report(context, callback, 12, hr.hr) {
        return p.fail;
    }
    if !failed(hr.hr) {
        hr = match probe_call(context, callback, 42) {
            Some(result) => result,
            None => return p.fail,
        };
        if !report(context, callback, 13, hr.hr) {
            return p.fail;
        }
    }
    if probe_call(context, callback, 43).is_none() {
        return p.fail;
    }
    if failed(hr.hr) {
        let _ = probe_call(context, callback, 20);
        return hr.hr;
    }
    let depth = ProbeRequest {
        width: p.width,
        height: p.height,
        format: p.depth_format,
        usage: p.depth_usage,
        bind: p.depth_bind,
        mip_levels: 1,
        array_size: 1,
        sample_count: 1,
        ..ProbeRequest::default()
    };
    hr = match probe_request(context, callback, 44, &depth) {
        Some(result) => result,
        None => return p.fail,
    };
    if !report(context, callback, 14, hr.hr) {
        return p.fail;
    }
    if !failed(hr.hr) {
        hr = match probe_call(context, callback, 45) {
            Some(result) => result,
            None => return p.fail,
        };
        if !report(context, callback, 15, hr.hr) {
            return p.fail;
        }
    }
    if failed(hr.hr) {
        let _ = probe_call(context, callback, 20);
        return hr.hr;
    }
    let viewport = ProbeRequest {
        viewport_width: p.width as f32,
        viewport_height: p.height as f32,
        ..ProbeRequest::default()
    };
    if probe_request(context, callback, 46, &viewport).is_none()
        || !report(context, callback, 16, 0)
    {
        return p.fail;
    }
    0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_d3d_probe_ready(ready_bits: u32) -> i32 {
    (ready_bits & 31 == 31) as i32
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_probe_present_colour(
    params: *const ProbeParams,
    ready_bits: u32,
    context: *mut c_void,
    callback: Call,
) -> i32 {
    if params.is_null() {
        return 0;
    }
    let p = unsafe { &*params };
    if a2fo_rs_d3d_probe_ready(ready_bits) == 0 {
        let _ = report(context, callback, 17, p.unexpected);
        return p.unexpected;
    }
    if probe_call(context, callback, 47).is_none() || probe_call(context, callback, 48).is_none() {
        return p.fail;
    }
    let Some(presented) = probe_call(context, callback, 49) else {
        return p.fail;
    };
    if !report(context, callback, 18, presented.hr) {
        return p.fail;
    }
    if presented.hr == p.removed || presented.hr == p.reset {
        if let Some(reason) = probe_call(context, callback, 50) {
            let _ = report(context, callback, 19, reason.hr);
        }
        let _ = probe_call(context, callback, 20);
    }
    presented.hr
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_probe_present_triangle(
    params: *const ProbeParams,
    ready_bits: u32,
    context: *mut c_void,
    callback: Call,
) -> i32 {
    if params.is_null() {
        return 0;
    }
    let p = unsafe { &*params };
    if a2fo_rs_d3d_probe_ready(ready_bits) == 0 {
        return p.unexpected;
    }
    let Some(mut step) = probe_call(context, callback, 51) else {
        return p.fail;
    };
    if !failed(step.hr) {
        step = match probe_call(context, callback, 52) {
            Some(result) => result,
            None => return p.fail,
        };
    }
    if !failed(step.hr) {
        let clear = ProbeRequest {
            flags: 0xff145ed1,
            ..ProbeRequest::default()
        };
        step = match probe_request(context, callback, 53, &clear) {
            Some(result) => result,
            None => return p.fail,
        };
    }
    if !failed(step.hr) {
        let w = p.width as f32;
        let h = p.height as f32;
        let triangle = Triangle {
            vertices: [
                Vertex {
                    x: w * 0.5,
                    y: h * 0.2,
                    z: 0.5,
                    rhw: 1.0,
                    color: 0xffff6040,
                },
                Vertex {
                    x: w * 0.8,
                    y: h * 0.8,
                    z: 0.5,
                    rhw: 1.0,
                    color: 0xff40ff80,
                },
                Vertex {
                    x: w * 0.2,
                    y: h * 0.8,
                    z: 0.5,
                    rhw: 1.0,
                    color: 0xff80a0ff,
                },
            ],
            primitive: 4,
            primitive_count: 1,
            fvf: 0x44,
            stride: 0,
            color_op: 1,
            alpha_op: 1,
            clear_color: 0xff145ed1,
        };
        step = match call(context, callback, 54, (&triangle as *const Triangle).cast()) {
            Some(result) => result,
            None => return p.fail,
        };
    }
    if !report(context, callback, 20, step.hr) {
        return p.fail;
    }
    if failed(step.hr) {
        return step.hr;
    }
    let Some(presented) = probe_call(context, callback, 49) else {
        return p.fail;
    };
    if !report(context, callback, 21, presented.hr) {
        return p.fail;
    }
    if presented.hr == p.removed || presented.hr == p.reset {
        if let Some(reason) = probe_call(context, callback, 50) {
            let _ = report(context, callback, 19, reason.hr);
        }
        let _ = probe_call(context, callback, 55);
        let _ = probe_call(context, callback, 20);
    }
    presented.hr
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_probe_shutdown(context: *mut c_void, callback: Call) {
    for op in [56, 57, 58, 59, 31, 29, 30, 60, 61] {
        let _ = probe_call(context, callback, op);
    }
}
