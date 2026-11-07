//! D3D8 compatibility policy. Native COM objects and backend calls stay in C++.
use core::ffi::c_void;
use core::slice;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Hr {
    ok: i32,
    e_pointer: i32,
    e_nointerface: i32,
    invalid_call: i32,
    notfound: i32,
    moredata: i32,
    outmemory: i32,
}
#[repr(C)]
pub struct InterfacePlan {
    hr: i32,
    kind: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct FormatPair {
    dxgi: u32,
    legacy: u32,
}
#[repr(C)]
pub struct PrivateGet {
    hr: i32,
    needed: u32,
    copy: u8,
}
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
pub struct Mip {
    width: u32,
    height: u32,
    row_pitch: u32,
    offset: usize,
}
#[repr(C)]
pub struct LockPlan {
    hr: i32,
    pitch: i32,
    offset: usize,
}
#[repr(C)]
pub struct CreatePlan {
    hr: i32,
    error_kind: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Refs {
    context: *mut c_void,
    increment_public: extern "C" fn(*mut c_void) -> i32,
    decrement_public: extern "C" fn(*mut c_void) -> i32,
    increment_implicit: extern "C" fn(*mut c_void) -> i32,
    decrement_implicit: extern "C" fn(*mut c_void) -> i32,
    load_public: extern "C" fn(*mut c_void) -> i32,
    parent_addref: extern "C" fn(*mut c_void),
    parent_release: extern "C" fn(*mut c_void),
    delete_self: extern "C" fn(*mut c_void),
    texture_addref: extern "C" fn(*mut c_void),
    texture_release: extern "C" fn(*mut c_void),
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_texture_query(
    iid: *const u8,
    known: *const u8,
    surface: u8,
    has_output: u8,
    hr: Hr,
) -> InterfacePlan {
    if has_output == 0 {
        return InterfacePlan {
            hr: hr.e_pointer,
            kind: 0,
        };
    }
    let iid = unsafe { slice::from_raw_parts(iid, 16) };
    let known = unsafe { slice::from_raw_parts(known, 6 * 16) };
    let eq = |index: usize| iid == &known[index * 16..index * 16 + 16];
    if surface != 0 {
        if eq(0) || eq(5) {
            InterfacePlan { hr: hr.ok, kind: 1 }
        } else {
            InterfacePlan {
                hr: hr.e_nointerface,
                kind: 0,
            }
        }
    } else if eq(0) || eq(1) || eq(2) || eq(3) {
        InterfacePlan { hr: hr.ok, kind: 2 }
    } else if eq(4) {
        InterfacePlan { hr: hr.ok, kind: 3 }
    } else {
        InterfacePlan {
            hr: hr.e_nointerface,
            kind: 0,
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_private_set(
    has_data: u8,
    size: u32,
    flags: u32,
    unknown_flag: u32,
    pointer_size: u32,
    hr: Hr,
) -> i32 {
    if has_data == 0
        || flags & !unknown_flag != 0
        || (flags & unknown_flag != 0 && size != pointer_size)
    {
        hr.invalid_call
    } else {
        hr.ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_private_get(
    has_size: u8,
    found: u8,
    object: u8,
    byte_count: u32,
    supplied: u32,
    has_data: u8,
    pointer_size: u32,
    hr: Hr,
) -> PrivateGet {
    if has_size == 0 {
        return PrivateGet {
            hr: hr.invalid_call,
            needed: 0,
            copy: 0,
        };
    }
    if found == 0 {
        return PrivateGet {
            hr: hr.notfound,
            needed: 0,
            copy: 0,
        };
    }
    let needed = if object != 0 {
        pointer_size
    } else {
        byte_count
    };
    if has_data == 0 {
        return PrivateGet {
            hr: hr.ok,
            needed,
            copy: 0,
        };
    }
    if supplied < needed {
        return PrivateGet {
            hr: hr.moredata,
            needed,
            copy: 0,
        };
    }
    PrivateGet {
        hr: hr.ok,
        needed,
        copy: 1,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_private_erase(found: u8, hr: Hr) -> i32 {
    if found == 0 { hr.notfound } else { hr.ok }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_texture_legacy_format(
    dxgi: u32,
    formats: *const FormatPair,
    count: usize,
    unknown: u32,
) -> u32 {
    for pair in unsafe { slice::from_raw_parts(formats, count) } {
        if pair.dxgi == dxgi {
            return pair.legacy;
        }
    }
    unknown
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_texture_exchange_u32(old: u32, value: u32, next: *mut u32) -> u32 {
    unsafe { *next = value };
    old
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_output_valid(has_output: u8, invalid: i32, hr: Hr) -> i32 {
    if has_output == 0 { invalid } else { hr.ok }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_binding_valid(has_output: u8, has_binding: u8, hr: Hr) -> i32 {
    if has_output == 0 || has_binding == 0 {
        hr.e_pointer
    } else {
        hr.ok
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_texture_set_lod(
    old: u32,
    value: u32,
    levels: u32,
    next: *mut u32,
) -> u32 {
    unsafe { *next = value.min(levels.wrapping_sub(1)) };
    old
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_level_valid(
    has_output: u8,
    level: u32,
    count: usize,
    hr: Hr,
) -> i32 {
    if has_output == 0 || level as usize >= count {
        hr.invalid_call
    } else {
        hr.ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_lock_pre(
    has_output: u8,
    level: u32,
    count: usize,
    locked: u8,
    flags: u32,
    read_flag: u32,
    allowed_flags: u32,
    hr: Hr,
) -> i32 {
    if has_output == 0
        || level as usize >= count
        || locked != 0
        || flags & read_flag == 0
        || flags & !allowed_flags != 0
    {
        hr.invalid_call
    } else {
        hr.ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_lock(
    has_output: u8,
    level: u32,
    count: usize,
    locked: u8,
    flags: u32,
    read_flag: u32,
    allowed_flags: u32,
    has_region: u8,
    region: Rect,
    mip: Mip,
    format: u32,
    bc1: u32,
    bc1_srgb: u32,
    bc4: u32,
    bc4_snorm: u32,
    hr: Hr,
) -> LockPlan {
    let invalid = LockPlan {
        hr: hr.invalid_call,
        pitch: 0,
        offset: 0,
    };
    if has_output == 0
        || level as usize >= count
        || locked != 0
        || flags & read_flag == 0
        || flags & !allowed_flags != 0
    {
        return invalid;
    }
    let full = Rect {
        left: 0,
        top: 0,
        right: mip.width as i32,
        bottom: mip.height as i32,
    };
    let r = if has_region != 0 { region } else { full };
    if r.left < 0
        || r.top < 0
        || r.right <= r.left
        || r.bottom <= r.top
        || r.right > full.right
        || r.bottom > full.bottom
        || r.left % 4 != 0
        || r.top % 4 != 0
        || (r.right % 4 != 0 && r.right != full.right)
        || (r.bottom % 4 != 0 && r.bottom != full.bottom)
    {
        return invalid;
    }
    let bytes_per_block =
        if format == bc1 || format == bc1_srgb || format == bc4 || format == bc4_snorm {
            8usize
        } else {
            16usize
        };
    let offset = mip
        .offset
        .wrapping_add((r.top as usize / 4).wrapping_mul(mip.row_pitch as usize))
        .wrapping_add((r.left as usize / 4).wrapping_mul(bytes_per_block));
    LockPlan {
        hr: hr.ok,
        pitch: mip.row_pitch as i32,
        offset,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_unlock(level: u32, count: usize, locked: u8, hr: Hr) -> i32 {
    if level as usize >= count || locked == 0 {
        hr.invalid_call
    } else {
        hr.ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_create(
    has_output: u8,
    has_owner: u8,
    has_device: u8,
    interpretation: u32,
    raw: u32,
    normal_xy: u32,
    format: u32,
    bc5_unorm: u32,
    bc5_snorm: u32,
    hr: Hr,
) -> CreatePlan {
    if has_output == 0 || has_owner == 0 || has_device == 0 {
        return CreatePlan {
            hr: hr.invalid_call,
            error_kind: 1,
        };
    }
    if interpretation != raw && interpretation != normal_xy {
        return CreatePlan {
            hr: hr.invalid_call,
            error_kind: 0,
        };
    }
    if interpretation == normal_xy && format != bc5_unorm && format != bc5_snorm {
        return CreatePlan {
            hr: hr.invalid_call,
            error_kind: 2,
        };
    }
    CreatePlan {
        hr: hr.ok,
        error_kind: 0,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_binding_after(result: i32) -> u8 {
    (result >= 0) as u8
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_addref(ops: Refs) -> u32 {
    let count = (ops.increment_public)(ops.context);
    if count == 1 {
        (ops.parent_addref)(ops.context);
    }
    count as u32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_retain_binding(ops: Refs) -> u32 {
    (ops.increment_implicit)(ops.context) as u32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_release_binding(ops: Refs) -> u32 {
    let left = (ops.decrement_implicit)(ops.context);
    if left == 0 && (ops.load_public)(ops.context) == 0 {
        (ops.delete_self)(ops.context);
    }
    left as u32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_texture_release(ops: Refs) -> u32 {
    (ops.increment_implicit)(ops.context);
    let left = (ops.decrement_public)(ops.context);
    if left == 0 {
        (ops.parent_release)(ops.context);
    }
    let implicit = (ops.decrement_implicit)(ops.context);
    if implicit == 0 && (ops.load_public)(ops.context) == 0 {
        (ops.delete_self)(ops.context);
    }
    left as u32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_surface_addref(ops: Refs) -> u32 {
    let value = (ops.increment_public)(ops.context);
    if value == 1 {
        (ops.texture_addref)(ops.context);
    }
    value as u32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_surface_release(ops: Refs) -> u32 {
    let value = (ops.decrement_public)(ops.context);
    if value == 0 {
        (ops.texture_release)(ops.context);
    }
    value as u32
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DrawSource {
    data: *const u8,
    length: usize,
    stride: u32,
    present: u8,
    locked: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DrawElement {
    stream: u32,
    offset: u32,
    reg: u32,
    kind: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DrawLayout {
    stride: u32,
    position_components: u32,
    texture_count: u32,
    normal_offset: u32,
    diffuse_offset: u32,
    specular_offset: u32,
    texture_offsets: [u32; 8],
    texture_components: [u32; 8],
    transformed: u8,
    normal: u8,
    diffuse: u8,
    specular: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DrawStage {
    present: u8,
    compressed: u8,
    normal_xy: u8,
    needs_upload: u8,
    lod: u32,
}
#[repr(C)]
pub struct MemoryRegion {
    base: usize,
    size: usize,
    state: u32,
    protect: u32,
    valid: u8,
}
#[repr(C)]
pub struct DrawInput {
    kind: u32,
    count: u32,
    first: u32,
    user_stride: u32,
    index_format: u32,
    min_index: u32,
    vertex_count: u32,
    base: u32,
    user_vertices: *const u8,
    user_indices: *const u8,
    in_scene: u8,
    recording: u8,
    pixel_shader: u8,
    format16: u32,
    format32: u32,
    vertex_bytes: u32,
    mem_commit: u32,
    page_noaccess: u32,
    page_guard: u32,
    invalid_call: i32,
    outmemory: i32,
    ok: i32,
}
#[repr(C)]
pub struct DrawOps {
    context: *mut c_void,
    primitive_count: extern "C" fn(*mut c_void, u32, u32, *mut u32) -> i32,
    memory_region: extern "C" fn(*mut c_void, usize) -> MemoryRegion,
    resize_indices: extern "C" fn(*mut c_void, u32) -> u8,
    resize_vertices: extern "C" fn(*mut c_void, u32) -> u8,
    set_index: extern "C" fn(*mut c_void, u32, u32),
    vertex: extern "C" fn(*mut c_void, u32) -> *mut f32,
    has_shader: extern "C" fn(*mut c_void) -> u8,
    shader_hlsl_empty: extern "C" fn(*mut c_void) -> u8,
    shader_element_count: extern "C" fn(*mut c_void) -> u32,
    shader_element: extern "C" fn(*mut c_void, u32) -> DrawElement,
    decode_layout: extern "C" fn(*mut c_void, *mut DrawLayout) -> i32,
    set_state: extern "C" fn(*mut c_void, u8, u8, u8, u8, u8, *const u32) -> u8,
    stream: extern "C" fn(*mut c_void, u32) -> DrawSource,
    stage: extern "C" fn(*mut c_void, u32) -> DrawStage,
    compressed_view: extern "C" fn(*mut c_void, u32) -> i32,
    upload: extern "C" fn(*mut c_void, u32) -> i32,
    texture_view: extern "C" fn(*mut c_void, u32) -> i32,
    stage_finish: extern "C" fn(*mut c_void, u32, u8, u32),
    stage_view_present: extern "C" fn(*mut c_void, u32) -> u8,
    backend_draw: extern "C" fn(*mut c_void) -> i32,
    success: extern "C" fn(*mut c_void, u8, u8, u8),
    fail: extern "C" fn(*mut c_void, u32, i32) -> i32,
}
fn element_size(kind: u32) -> u32 {
    [4, 8, 12, 16, 4, 4, 4, 8]
        .get(kind as usize)
        .copied()
        .unwrap_or(0)
}
fn fail_draw(ops: &DrawOps, code: u32, hr: i32) -> i32 {
    (ops.fail)(ops.context, code, hr)
}
fn draw_readable(input: &DrawInput, ops: &DrawOps, pointer: *const u8, bytes: usize) -> bool {
    let mut begin = pointer as usize;
    if pointer.is_null() || bytes > usize::MAX - begin {
        return false;
    }
    let end = begin + bytes;
    while begin < end {
        let region = (ops.memory_region)(ops.context, begin);
        if region.valid == 0
            || region.state != input.mem_commit
            || region.protect & (input.page_noaccess | input.page_guard) != 0
        {
            return false;
        }
        let next = region.base.wrapping_add(region.size);
        if next <= begin {
            return false;
        }
        begin = end.min(next);
    }
    true
}
fn draw_source(
    input: &DrawInput,
    ops: &DrawOps,
    stream: u32,
    vertex: u32,
    offset: u32,
    bytes: u32,
    end: u32,
) -> *const u8 {
    let src = if !input.user_vertices.is_null() {
        if stream != 0 {
            return core::ptr::null();
        }
        let stride = input.user_stride;
        let length = (end as usize).wrapping_mul(stride as usize);
        if stride == 0 || !draw_readable(input, ops, input.user_vertices, length) {
            return core::ptr::null();
        }
        DrawSource {
            data: input.user_vertices,
            length,
            stride,
            present: 1,
            locked: 0,
        }
    } else {
        (ops.stream)(ops.context, stream)
    };
    if src.present == 0
        || src.data.is_null()
        || src.locked != 0
        || src.stride == 0
        || offset > src.stride
        || bytes > src.stride - offset
    {
        return core::ptr::null();
    }
    let at = (vertex as usize)
        .wrapping_mul(src.stride as usize)
        .wrapping_add(offset as usize);
    if at > src.length || bytes as usize > src.length - at {
        return core::ptr::null();
    }
    // The C++ source callback supplies a pointer into a live vector or a checked user span.
    unsafe { src.data.add(at) }
}
fn write_color(out: &mut [f32], p: *const u8) {
    unsafe {
        out[0] = (*p.add(2) as f32) / 255.0;
        out[1] = (*p.add(1) as f32) / 255.0;
        out[2] = (*p as f32) / 255.0;
        out[3] = (*p.add(3) as f32) / 255.0;
    }
}
fn write_element(out: &mut [f32], p: *const u8, kind: u32) {
    out[..4].copy_from_slice(&[0.0, 0.0, 0.0, 1.0]);
    unsafe {
        if kind <= 3 {
            core::ptr::copy_nonoverlapping(p, out.as_mut_ptr().cast(), ((kind + 1) * 4) as usize);
        } else if kind == 4 {
            write_color(out, p);
        } else if kind == 5 {
            for (i, v) in out.iter_mut().take(4).enumerate() {
                *v = *p.add(i) as f32;
            }
        } else {
            for (i, v) in out
                .iter_mut()
                .take(if kind == 6 { 2 } else { 4 })
                .enumerate()
            {
                *v = i16::from_ne_bytes([*p.add(2 * i), *p.add(2 * i + 1)]) as f32;
            }
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_draw_run(input: DrawInput, ops: DrawOps) -> i32 {
    if input.in_scene == 0 || input.recording != 0 {
        return fail_draw(&ops, 1, input.invalid_call);
    }
    if input.pixel_shader != 0 {
        return fail_draw(&ops, 2, input.invalid_call);
    }
    let mut elements = 0u32;
    if (ops.primitive_count)(ops.context, input.kind, input.count, &mut elements) < 0 {
        return input.invalid_call;
    }
    if elements == 0 {
        return input.ok;
    }
    if elements > 1024 * 1024 {
        return fail_draw(&ops, 3, input.invalid_call);
    }
    let mut start = input.first;
    let mut vertex_count = input.vertex_count;
    if !input.user_indices.is_null() {
        if input.index_format != input.format16 && input.index_format != input.format32 {
            return input.invalid_call;
        }
        let bytes = if input.index_format == input.format16 {
            2usize
        } else {
            4
        };
        if !draw_readable(&input, &ops, input.user_indices, elements as usize * bytes)
            || vertex_count == 0
            || input.min_index > u32::MAX - vertex_count
            || input.base > u32::MAX - input.min_index
        {
            return input.invalid_call;
        }
        if (ops.resize_indices)(ops.context, elements) == 0 {
            return input.outmemory;
        }
        for i in 0..elements {
            let p = unsafe { input.user_indices.add(i as usize * bytes) };
            let value = unsafe {
                if bytes == 2 {
                    u16::from_ne_bytes([*p, *p.add(1)]) as u32
                } else {
                    u32::from_ne_bytes([*p, *p.add(1), *p.add(2), *p.add(3)])
                }
            };
            if value < input.min_index || value - input.min_index >= vertex_count {
                return fail_draw(&ops, 4, input.invalid_call);
            }
            (ops.set_index)(ops.context, i, value - input.min_index);
        }
        start = input.base + input.min_index;
    } else {
        vertex_count = elements;
    }
    if input.vertex_bytes == 0
        || vertex_count > 64 * 1024 * 1024 / input.vertex_bytes
        || start > u32::MAX - vertex_count
    {
        return input.invalid_call;
    }
    if (ops.resize_vertices)(ops.context, vertex_count) == 0 {
        return input.outmemory;
    }
    let shader = (ops.has_shader)(ops.context) != 0;
    let mut layout = DrawLayout::default();
    let mut components = [0u32; 8];
    if shader {
        if (ops.set_state)(ops.context, 1, 0, 0, 0, 0, components.as_ptr()) == 0 {
            return input.outmemory;
        }
    } else {
        let hr = (ops.decode_layout)(ops.context, &mut layout);
        if hr < 0 {
            return fail_draw(&ops, 5, hr);
        }
        components = layout.texture_components;
        if (ops.set_state)(
            ops.context,
            0,
            layout.transformed,
            layout.normal,
            layout.diffuse,
            layout.specular,
            components.as_ptr(),
        ) == 0
        {
            return input.outmemory;
        }
    }
    let end = start + vertex_count;
    for i in 0..vertex_count {
        let vertex = (ops.vertex)(ops.context, i);
        if vertex.is_null() {
            return input.outmemory;
        }
        let v = unsafe { slice::from_raw_parts_mut(vertex, 64) };
        for r in 0..16 {
            v[r * 4 + 3] = 1.0;
        }
        if shader {
            for e in 0..(ops.shader_element_count)(ops.context) {
                let element = (ops.shader_element)(ops.context, e);
                let p = draw_source(
                    &input,
                    &ops,
                    element.stream,
                    start + i,
                    element.offset,
                    element_size(element.kind),
                    end,
                );
                if p.is_null() {
                    return fail_draw(&ops, 6, input.invalid_call);
                }
                let mut reg = element.reg;
                if (ops.shader_hlsl_empty)(ops.context) != 0 {
                    match reg {
                        0 => {}
                        3 => {
                            reg = 1;
                            if (ops.set_state)(ops.context, 2, 0, 1, 0, 0, components.as_ptr()) == 0
                            {
                                return input.outmemory;
                            }
                        }
                        5 => {
                            reg = 2;
                            if (ops.set_state)(ops.context, 2, 0, 0, 1, 0, components.as_ptr()) == 0
                            {
                                return input.outmemory;
                            }
                        }
                        6 => {
                            reg = 3;
                            if (ops.set_state)(ops.context, 2, 0, 0, 0, 1, components.as_ptr()) == 0
                            {
                                return input.outmemory;
                            }
                        }
                        7..=14 => {
                            reg = reg - 7 + 4;
                            components[(reg - 4) as usize] = if element.kind <= 3 {
                                element.kind + 1
                            } else {
                                4
                            };
                            if (ops.set_state)(ops.context, 3, 0, 0, 0, 0, components.as_ptr()) == 0
                            {
                                return input.outmemory;
                            }
                        }
                        _ => return fail_draw(&ops, 7, input.invalid_call),
                    }
                }
                if reg >= 16 {
                    return input.invalid_call;
                }
                write_element(
                    &mut v[reg as usize * 4..reg as usize * 4 + 4],
                    p,
                    element.kind,
                );
            }
        } else {
            let p = draw_source(&input, &ops, 0, start + i, 0, layout.stride, end);
            if p.is_null() {
                return fail_draw(&ops, 8, input.invalid_call);
            }
            unsafe {
                core::ptr::copy_nonoverlapping(
                    p,
                    v.as_mut_ptr().cast(),
                    (layout.position_components * 4) as usize,
                );
                v[7] = 0.0;
                v[8..12].fill(1.0);
                v[12..16].fill(0.0);
                if layout.normal != 0 {
                    core::ptr::copy_nonoverlapping(
                        p.add(layout.normal_offset as usize),
                        v[4..].as_mut_ptr().cast(),
                        12,
                    );
                }
                if layout.diffuse != 0 {
                    write_color(&mut v[8..12], p.add(layout.diffuse_offset as usize));
                }
                if layout.specular != 0 {
                    write_color(&mut v[12..16], p.add(layout.specular_offset as usize));
                }
                for t in 0..layout.texture_count.min(8) as usize {
                    core::ptr::copy_nonoverlapping(
                        p.add(layout.texture_offsets[t] as usize),
                        v[(t + 4) * 4..].as_mut_ptr().cast(),
                        (layout.texture_components[t] * 4) as usize,
                    );
                }
            }
        }
    }
    let sampled = draw_samples(&ops, input.ok, input.outmemory);
    if sampled < 0 {
        return sampled;
    }
    let hr = (ops.backend_draw)(ops.context);
    if hr == i32::MIN {
        return input.outmemory;
    }
    if hr < 0 {
        return fail_draw(&ops, 10, hr);
    }
    let mut any_normal = false;
    let mut compressed = false;
    for stage in 0..8 {
        let info = (ops.stage)(ops.context, stage);
        if info.present != 0 && info.normal_xy != 0 {
            any_normal = true;
            if (ops.stage_view_present)(ops.context, stage) != 0 {
                compressed = true;
                break;
            }
        }
    }
    if !any_normal {
        for stage in 0..8 {
            let info = (ops.stage)(ops.context, stage);
            if info.present != 0 && info.compressed != 0 {
                compressed = true;
                break;
            }
        }
    }
    (ops.success)(
        ops.context,
        compressed as u8,
        (!input.user_vertices.is_null()) as u8,
        (!input.user_indices.is_null()) as u8,
    );
    hr
}
fn draw_samples(ops: &DrawOps, ok: i32, outmemory: i32) -> i32 {
    for stage in 0..8 {
        let info = (ops.stage)(ops.context, stage);
        if info.present == 0 {
            continue;
        }
        let hr = if info.compressed != 0 {
            (ops.compressed_view)(ops.context, stage)
        } else {
            if info.needs_upload != 0 {
                let upload = (ops.upload)(ops.context, stage);
                if upload == i32::MIN {
                    return outmemory;
                }
                if upload < 0 {
                    return upload;
                }
            }
            (ops.texture_view)(ops.context, stage)
        };
        if hr == i32::MIN {
            return outmemory;
        }
        if hr < 0 {
            return fail_draw(ops, 9, hr);
        }
        (ops.stage_finish)(ops.context, stage, info.normal_xy, info.lod);
    }
    ok
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_draw_samples(ops: DrawOps, ok: i32, outmemory: i32) -> i32 {
    draw_samples(&ops, ok, outmemory)
}
#[repr(C)]
pub struct ShaderWordsOps {
    context: *mut c_void,
    memory_region: extern "C" fn(*mut c_void, usize) -> MemoryRegion,
    append_word: extern "C" fn(*mut c_void, u32) -> u8,
    parse_words: extern "C" fn(*mut c_void, u8) -> u32,
}
fn shader_readable(
    pointer: *const u8,
    bytes: usize,
    commit: u32,
    noaccess: u32,
    guard: u32,
    ops: &ShaderWordsOps,
) -> bool {
    let mut begin = pointer as usize;
    if pointer.is_null() || bytes > usize::MAX - begin {
        return false;
    }
    let end = begin + bytes;
    while begin < end {
        let m = (ops.memory_region)(ops.context, begin);
        if m.valid == 0 || m.state != commit || m.protect & (noaccess | guard) != 0 {
            return false;
        }
        let next = m.base.wrapping_add(m.size);
        if next <= begin {
            return false;
        }
        begin = end.min(next);
    }
    true
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_words(
    input: *const u32,
    declaration: u8,
    commit: u32,
    noaccess: u32,
    guard: u32,
    ops: ShaderWordsOps,
) -> u8 {
    if input.is_null() {
        return 0;
    }
    for i in 0..4096 {
        let word = input.wrapping_add(i);
        if !shader_readable(word.cast(), 4, commit, noaccess, guard, &ops) {
            return 0;
        }
        let value = unsafe { core::ptr::read_unaligned(word) };
        if (ops.append_word)(ops.context, value) == 0 {
            return 2;
        }
        match (ops.parse_words)(ops.context, declaration) {
            0 => return 1,
            1 => {}
            _ => return 0,
        }
    }
    0
}
#[repr(C)]
pub struct DeclarationOps {
    context: *mut c_void,
    append_element: extern "C" fn(*mut c_void, DrawElement) -> u8,
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_declaration(
    tokens: *const u32,
    count: usize,
    ops: DeclarationOps,
    register_mask: *mut u32,
) -> u8 {
    if tokens.is_null() || register_mask.is_null() {
        return 3;
    }
    let input = unsafe { slice::from_raw_parts(tokens, count) };
    let mut stream = 0usize;
    let mut offsets = [0u32; 16];
    let mut registers = 0u32;
    for &token in input {
        if token == u32::MAX {
            break;
        }
        let kind = token >> 29;
        if kind == 0 {
            continue;
        }
        if kind == 1 {
            if token & 0x10000000 != 0 {
                return 1;
            }
            stream = (token & 15) as usize;
            continue;
        }
        if kind != 2 {
            return 2;
        }
        if token & 0x10000000 != 0 {
            offsets[stream] = offsets[stream].wrapping_add(((token >> 16) & 15) * 4);
            continue;
        }
        let kind = (token >> 16) & 15;
        let reg = token & 31;
        let bytes = element_size(kind);
        if reg >= 16 || bytes == 0 || registers & (1 << reg) != 0 {
            return 3;
        }
        registers |= 1 << reg;
        if (ops.append_element)(
            ops.context,
            DrawElement {
                stream: stream as u32,
                offset: offsets[stream],
                reg,
                kind,
            },
        ) == 0
        {
            return 4;
        }
        offsets[stream] = offsets[stream].wrapping_add(bytes);
    }
    unsafe {
        *register_mask = registers;
    }
    0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_mask_valid(input_mask: u32, declared: u32) -> u8 {
    (input_mask & !declared == 0) as u8
}
#[repr(C)]
pub struct ShaderBlobPlan {
    hr: i32,
    needed: u32,
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_blob(
    bytes: *const u8,
    count: usize,
    output: *mut u8,
    has_size: u8,
    capacity: u32,
    invalid: i32,
    ok: i32,
    moredata: i32,
) -> ShaderBlobPlan {
    if has_size == 0 {
        return ShaderBlobPlan {
            hr: invalid,
            needed: 0,
        };
    }
    let needed = (count as u32).wrapping_mul(4);
    if output.is_null() {
        return ShaderBlobPlan { hr: ok, needed };
    }
    if capacity < needed {
        return ShaderBlobPlan {
            hr: moredata,
            needed,
        };
    }
    if needed != 0 && !bytes.is_null() {
        unsafe {
            core::ptr::copy_nonoverlapping(bytes, output, needed as usize);
        }
    }
    ShaderBlobPlan { hr: ok, needed }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_indexed_valid(
    has_indices: u8,
    locked: u8,
    count_hr: i32,
    start: u32,
    count: u32,
    index_size: u32,
    buffer_size: usize,
    invalid: i32,
    ok: i32,
) -> i32 {
    if has_indices == 0 || locked != 0 || count_hr < 0 {
        return invalid;
    }
    let offset = start as usize * index_size as usize;
    let length = count as usize * index_size as usize;
    if offset > buffer_size || length > buffer_size - offset {
        invalid
    } else {
        ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_index_size(format: u32, format16: u32) -> u32 {
    if format == format16 { 2 } else { 4 }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_up_valid(
    has_indices: u8,
    has_data: u8,
    invalid: i32,
    ok: i32,
) -> i32 {
    if has_indices == 0 || has_data == 0 {
        invalid
    } else {
        ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_constant(
    first: u32,
    data: *const u8,
    count: u32,
    target: *mut u8,
    mask_context: *mut c_void,
    mark: Option<extern "C" fn(*mut c_void, u32)>,
    invalid: i32,
    ok: i32,
) -> i32 {
    if data.is_null() || first > 96 || count > 96 - first {
        return invalid;
    }
    if count == 0 {
        return ok;
    }
    if target.is_null() {
        return invalid;
    }
    unsafe {
        core::ptr::copy_nonoverlapping(data, target, count as usize * 16);
    }
    if let Some(callback) = mark {
        for i in 0..count {
            callback(mask_context, first + i);
        }
    }
    ok
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_constant_valid(
    first: u32,
    has_data: u8,
    count: u32,
    invalid: i32,
    ok: i32,
) -> i32 {
    if has_data == 0 || first > 96 || count > 96 - first {
        invalid
    } else {
        ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_set_pixel(handle: u32, invalid: i32, ok: i32) -> i32 {
    if handle != 0 { invalid } else { ok }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_handle_valid(
    known: u8,
    fvf_hr: i32,
    invalid: i32,
    ok: i32,
) -> i32 {
    if known == 0 && fvf_hr < 0 {
        invalid
    } else {
        ok
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_erase_result(erased: u8, invalid: i32, ok: i32) -> i32 {
    if erased != 0 { ok } else { invalid }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_next_handle(current: u32, next: *mut u32) -> u32 {
    if !next.is_null() {
        unsafe {
            *next = current.wrapping_add(1);
        }
    }
    current
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shader_diagnostic(has_log: u8, current: u32, next: *mut u32) -> u8 {
    if has_log == 0 {
        return 0;
    }
    if !next.is_null() {
        unsafe {
            *next = current.wrapping_add(1);
        }
    }
    (current < 8) as u8
}
