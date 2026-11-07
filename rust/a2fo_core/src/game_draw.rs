// Portable admission and state planning for the native D3D11 draw pipeline.
use core::ptr;

const MAX_BYTES: usize = 64 * 1024 * 1024;
const VERTEX_FLOATS: usize = 16 * 4;

#[repr(C)]
pub struct A2foDrawAdmission {
    pub code: u32,
    pub rhw: f32,
}
#[repr(C)]
pub struct A2foDrawBlend {
    pub source: u32,
    pub dest: u32,
    pub source_alpha: u32,
    pub dest_alpha: u32,
}
#[repr(C)]
pub struct A2foDrawStage {
    pub code: u32,
    pub uses_texture: bool,
    pub row: [u32; 16],
}
#[repr(C)]
pub struct A2foDrawSampler {
    pub filter: u32,
    pub address_u: u32,
    pub address_v: u32,
    pub address_w: u32,
    pub border: u32,
    pub bias: u32,
    pub max_anisotropy: u32,
    pub min_lod: f32,
    pub max_lod: f32,
}
#[repr(C)]
pub struct A2foDrawStates {
    pub blend_enabled: u32,
    pub blend_op: u32,
    pub write_mask: u32,
    pub depth_enabled: u32,
    pub depth_write: u32,
    pub depth_func: u32,
    pub stencil_enabled: u32,
    pub stencil_read: u32,
    pub stencil_write: u32,
    pub stencil_fail: u32,
    pub stencil_depth_fail: u32,
    pub stencil_pass: u32,
    pub stencil_func: u32,
    pub fill: u32,
    pub cull: u32,
    pub depth_clip: u32,
    pub topology: u32,
}
#[repr(C)]
pub struct A2foDrawFixedInput {
    pub world: *const f32,
    pub view: *const f32,
    pub projection: *const f32,
    pub textures: *const f32,
    pub material_diffuse: *const f32,
    pub material_ambient: *const f32,
    pub material_specular: *const f32,
    pub material_emissive: *const f32,
    pub material_power: f32,
    pub render: *const u32,
    pub stages: *const u32,
    pub tex_components: *const u32,
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub height: u32,
    pub transformed: bool,
    pub has_normal: bool,
    pub has_diffuse: bool,
    pub has_specular: bool,
    pub active: u32,
}
#[repr(C, align(16))]
pub struct A2foDrawFixed {
    pub world: [f32; 16],
    pub view: [f32; 16],
    pub projection: [f32; 16],
    pub normal: [f32; 16],
    pub texture: [[f32; 16]; 8],
    pub material_diffuse: [f32; 4],
    pub material_ambient: [f32; 4],
    pub material_specular: [f32; 4],
    pub material_emissive: [f32; 4],
    pub global_ambient: [f32; 4],
    pub camera: [f32; 4],
    pub viewport: [f32; 4],
    pub fog: [f32; 4],
    pub light_diffuse: [[f32; 4]; 8],
    pub light_ambient: [[f32; 4]; 8],
    pub light_specular: [[f32; 4]; 8],
    pub light_position: [[f32; 4]; 8],
    pub light_direction: [[f32; 4]; 8],
    pub light_attenuation: [[f32; 4]; 8],
    pub flags: [u32; 4],
    pub material_source: [u32; 4],
    pub light_type: [[u32; 4]; 8],
    pub texcoord: [[u32; 4]; 8],
}
#[repr(C)]
pub struct A2foDrawLight {
    pub enabled: bool,
    pub kind: u32,
    pub diffuse: [f32; 4],
    pub ambient: [f32; 4],
    pub specular: [f32; 4],
    pub position: [f32; 3],
    pub direction: [f32; 3],
    pub attenuation0: f32,
    pub attenuation1: f32,
    pub attenuation2: f32,
    pub range: f32,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_admit(
    width: u32,
    height: u32,
    min_z: f32,
    max_z: f32,
    primitive: u32,
    vertex_count: usize,
    index_count: usize,
    transformed: bool,
    vertices: *const f32,
) -> A2foDrawAdmission {
    let result = |code, rhw| A2foDrawAdmission { code, rhw };
    if width == 0 || height == 0 || min_z < 0.0 || max_z > 1.0 || min_z > max_z {
        return result(1, 0.0);
    }
    if !(1..=6).contains(&primitive) {
        return result(2, 0.0);
    }
    if vertex_count == 0 && index_count == 0 {
        return result(0, 0.0);
    }
    if vertex_count == 0 || vertex_count > MAX_BYTES / (VERTEX_FLOATS * 4) {
        return result(3, 0.0);
    }
    if index_count > MAX_BYTES / 4 {
        return result(4, 0.0);
    }
    if vertices.is_null() {
        return result(3, 0.0);
    }
    for i in 0..vertex_count {
        let v = unsafe { vertices.add(i * VERTEX_FLOATS) };
        for axis in 0..3 {
            if !unsafe { *v.add(axis) }.is_finite() {
                return result(5, 0.0);
            }
        }
        let rhw = unsafe { *v.add(3) };
        if transformed && (!rhw.is_finite() || rhw <= 0.0) {
            return result(6, rhw);
        }
    }
    result(7, 0.0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_indices(
    indices: *const u32,
    count: usize,
    vertex_count: usize,
    primitive: u32,
    fan: *mut u32,
    fan_capacity: usize,
    fan_count: *mut usize,
) -> u32 {
    if fan_count.is_null() {
        return 1;
    }
    let index = |i: usize| -> u32 {
        if count == 0 {
            i as u32
        } else {
            unsafe { *indices.add(i) }
        }
    };
    let len = if count == 0 { vertex_count } else { count };
    if count != 0 && indices.is_null() {
        return 1;
    }
    for i in 0..len {
        if index(i) as usize >= vertex_count {
            return 1;
        }
    }
    if primitive == 6 && !(3..=MAX_BYTES / 12 + 2).contains(&len) {
        return 2;
    }
    let total = if primitive == 6 { (len - 2) * 3 } else { len };
    if total == 0 || total > MAX_BYTES / 4 {
        return 3;
    }
    if (primitive == 2 && total % 2 != 0)
        || (primitive == 4 && total % 3 != 0)
        || (primitive == 3 && total < 2)
        || (primitive >= 5 && total < 3)
    {
        return 4;
    }
    unsafe { *fan_count = total };
    if primitive == 6 && !fan.is_null() {
        if fan_capacity < total {
            return 5;
        }
        for i in 1..len - 1 {
            unsafe {
                *fan.add((i - 1) * 3) = index(0);
                *fan.add((i - 1) * 3 + 1) = index(i);
                *fan.add((i - 1) * 3 + 2) = index(i + 1);
            }
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_sequence(output: *mut u32, count: usize) {
    if output.is_null() {
        return;
    }
    for i in 0..count {
        unsafe {
            *output.add(i) = i as u32;
        }
    }
}

fn compare(v: u32) -> bool {
    (1..=8).contains(&v)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_render(
    render: *const u32,
    ff: bool,
    transformed: bool,
    primitive: u32,
) -> u32 {
    if render.is_null() {
        return 1;
    }
    let r = unsafe { core::slice::from_raw_parts(render, 256) };
    if r[7] > 1
        || r[14] > 1
        || !(1..=3).contains(&r[22])
        || !compare(r[23])
        || !compare(r[25])
        || r[24] > 255
        || !(1..=5).contains(&r[171])
        || r[168] > 15
    {
        return 1;
    }
    if (r[52] != 0
        && (!compare(r[56])
            || !(1..=8).contains(&r[53])
            || !(1..=8).contains(&r[54])
            || !(1..=8).contains(&r[55])))
        || (r[28] != 0 && (r[35] > 3 || r[140] > 3 || r[48] != 0))
    {
        return 2;
    }
    if (r[8] != 3 && r[8] != 2)
        || r[9] != 2
        || r[47] != 0
        || r[145] > 2
        || r[146] > 2
        || r[147] > 2
        || r[148] > 2
    {
        return 3;
    }
    if ff && !transformed && (r[151] != 0 || r[167] != 0) {
        return 4;
    }
    if r[152] != 0 {
        return 5;
    }
    if primitive == 1 && (r[156] != 0 || r[157] != 0) {
        return 6;
    }
    if ff && !transformed && r[137] != 0 && r[29] != 0 && r[142] == 0 {
        return 7;
    }
    for &v in &r[128..136] {
        if v != 0 {
            return 8;
        }
    }
    if !ff && r[28] != 0 && r[140] != 0 {
        return 9;
    }
    0
}

fn blend(v: u32, alpha: bool) -> Option<u32> {
    Some(match v {
        1 => 1,
        2 => 2,
        3 => {
            if alpha {
                5
            } else {
                3
            }
        }
        4 => {
            if alpha {
                6
            } else {
                4
            }
        }
        5 => 5,
        6 => 6,
        7 => 7,
        8 => 8,
        9 => {
            if alpha {
                7
            } else {
                9
            }
        }
        10 => {
            if alpha {
                8
            } else {
                10
            }
        }
        11 if !alpha => 11,
        _ => return None,
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_blend(source: u32, dest: u32, out: *mut A2foDrawBlend) -> bool {
    if out.is_null() {
        return false;
    }
    let (Some(source), Some(dest), Some(source_alpha), Some(dest_alpha)) = (
        blend(source, false),
        blend(dest, false),
        blend(source, true),
        blend(dest, true),
    ) else {
        return false;
    };
    unsafe {
        ptr::write(
            out,
            A2foDrawBlend {
                source,
                dest,
                source_alpha,
                dest_alpha,
            },
        )
    };
    true
}
fn arg(a: u32) -> bool {
    a & !0x3f == 0 && (a & 15) <= 5
}
fn op(a: u32) -> bool {
    (1..=26).contains(&a) && a != 17 && a != 22 && a != 23
}
fn stage_uses_texture(operation: u32, a0: u32, a1: u32, a2: u32) -> bool {
    if operation == 1 {
        return false;
    }
    if operation == 13 || operation == 15 {
        return true;
    }
    if (operation == 25 || operation == 26) && (a0 & 15) == 2 {
        return true;
    }
    (operation != 3 && (a1 & 15) == 2) || (operation != 2 && (a2 & 15) == 2)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_stage_pre(stage: *const u32) -> u32 {
    if stage.is_null() {
        return 1;
    }
    let s = unsafe { core::slice::from_raw_parts(stage, 33) };
    if !op(s[1])
        || !op(s[4])
        || !arg(s[2])
        || !arg(s[3])
        || !arg(s[5])
        || !arg(s[6])
        || !arg(s[26])
        || !arg(s[27])
        || (s[28] != 1 && s[28] != 5)
        || s[11] > 0x30007
        || !matches!(s[11] & 0xffff0000, 0 | 0x10000 | 0x20000 | 0x30000)
        || s[24] & !0x10f != 0
        || (s[24] & 255) > 4
        || (s[24] & 256 != 0 && s[24] & 255 == 0)
    {
        return 1;
    }
    if (18..=21).contains(&s[4]) {
        return 2;
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_stage_post(
    stage: *const u32,
    ff: bool,
    transformed: bool,
    has_normal: bool,
    tex_components: *const u32,
) -> A2foDrawStage {
    let result = |code| A2foDrawStage {
        code,
        uses_texture: false,
        row: [0; 16],
    };
    if stage.is_null() || tex_components.is_null() {
        return result(1);
    }
    let s = unsafe { core::slice::from_raw_parts(stage, 33) };
    let tc = unsafe { core::slice::from_raw_parts(tex_components, 8) };
    if !ff && (s[11] & 0xffff0000 != 0 || s[24] != 0) {
        return result(3);
    }
    if tc[(s[11] & 7) as usize] > 4 {
        return result(4);
    }
    if ff
        && s[11] & 0xffff0000 != 0
        && (transformed || (s[11] & 0xffff0000 != 0x20000 && !has_normal))
    {
        return result(5);
    }
    let uses =
        stage_uses_texture(s[1], s[26], s[2], s[3]) || stage_uses_texture(s[4], s[27], s[5], s[6]);
    let mut row = [0; 16];
    row[0] = s[1];
    row[1] = s[2];
    row[2] = s[3];
    row[4] = s[4];
    row[5] = s[5];
    row[6] = s[6];
    row[7] = s[26];
    A2foDrawStage {
        code: 0,
        uses_texture: uses,
        row,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_stage_finish(stage: *const u32, row: *mut u32) {
    if stage.is_null() || row.is_null() {
        return;
    }
    let s = unsafe { core::slice::from_raw_parts(stage, 33) };
    unsafe {
        *row.add(10) = s[27];
        *row.add(11) = (s[28] == 5) as u32;
        *row.add(12) = s[24];
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_sampler_validate(stage: *const u32) -> bool {
    if stage.is_null() {
        return false;
    }
    let s = unsafe { core::slice::from_raw_parts(stage, 33) };
    (1..=5).contains(&s[13])
        && (1..=5).contains(&s[14])
        && (1..=5).contains(&s[25])
        && (1..=3).contains(&s[16])
        && (1..=3).contains(&s[17])
        && s[18] <= 3
        && (1..=16).contains(&s[21])
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_sampler_plan(
    stage: *const u32,
    minimum_lod: u32,
    out: *mut A2foDrawSampler,
) -> bool {
    if stage.is_null() || out.is_null() {
        return false;
    }
    let s = unsafe { core::slice::from_raw_parts(stage, 33) };
    let min = if s[17] == 1 { 0 } else { 1 };
    let mag = if s[16] == 1 { 0 } else { 1 };
    let mip = if s[18] >= 2 { 1 } else { 0 };
    let filter = if s[17] == 3 || s[16] == 3 {
        0x55
    } else {
        (min << 4) | (mag << 2) | mip
    };
    let low = s[20].max(minimum_lod);
    unsafe {
        ptr::write(
            out,
            A2foDrawSampler {
                filter,
                address_u: s[13],
                address_v: s[14],
                address_w: s[25],
                border: s[15],
                bias: s[19],
                max_anisotropy: s[21],
                min_lod: low as f32,
                max_lod: if s[18] == 0 { low as f32 } else { f32::MAX },
            },
        )
    };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_buffer_capacity(current: u32, bytes: u32) -> u32 {
    if bytes == 0 || bytes as usize > MAX_BYTES {
        return 0;
    }
    if current >= bytes {
        return current;
    }
    let mut next = 256u32;
    while next < bytes && next < (MAX_BYTES / 2) as u32 {
        next *= 2
    }
    if next < bytes { bytes } else { next }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_states(
    render: *const u32,
    primitive: u32,
    out: *mut A2foDrawStates,
) {
    if render.is_null() || out.is_null() {
        return;
    }
    let r = unsafe { core::slice::from_raw_parts(render, 256) };
    let stencil_enabled = (r[52] != 0) as u32;
    let state = A2foDrawStates {
        blend_enabled: (r[27] != 0) as u32,
        blend_op: r[171],
        write_mask: r[168],
        depth_enabled: (r[7] != 0) as u32,
        depth_write: if r[14] != 0 { 1 } else { 0 },
        depth_func: r[23],
        stencil_enabled,
        stencil_read: r[58],
        stencil_write: r[59],
        stencil_fail: if stencil_enabled != 0 { r[53] } else { 1 },
        stencil_depth_fail: if stencil_enabled != 0 { r[54] } else { 1 },
        stencil_pass: if stencil_enabled != 0 { r[55] } else { 1 },
        stencil_func: if stencil_enabled != 0 { r[56] } else { 8 },
        fill: if r[8] == 2 { 2 } else { 3 },
        cull: if r[22] == 1 {
            1
        } else if r[22] == 2 {
            2
        } else {
            3
        },
        depth_clip: (r[136] != 0) as u32,
        topology: match primitive {
            1 => 1,
            2 => 2,
            3 => 3,
            5 => 5,
            _ => 4,
        },
    };
    unsafe { ptr::write(out, state) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_rgba(value: u32, out: *mut f32) {
    if out.is_null() {
        return;
    }
    for (i, shift) in [16, 8, 0, 24].into_iter().enumerate() {
        unsafe { *out.add(i) = ((value >> shift) & 255) as f32 / 255.0 }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_view_dimension(dimension: u32) -> bool {
    dimension == 4
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_view_owner(same: bool) -> bool {
    same
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_normal_mode(normal_xy: bool, format: u32) -> u32 {
    if !normal_xy {
        0
    } else if format == 0x53 {
        1
    } else if format == 0x54 {
        2
    } else {
        3
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_finish_stages(stages: *mut u32, active: u32) {
    if stages.is_null() {
        return;
    }
    for n in active.min(8) as usize..8 {
        unsafe { *stages.add(n * 16) = 1 }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_pixel_constants(
    render: *const u32,
    control: *mut u32,
    fog: *mut f32,
    fog_color: *mut f32,
    factor: *mut f32,
) {
    if render.is_null()
        || control.is_null()
        || fog.is_null()
        || fog_color.is_null()
        || factor.is_null()
    {
        return;
    }
    let r = unsafe { core::slice::from_raw_parts(render, 256) };
    unsafe {
        *control = (r[15] != 0) as u32;
        *control.add(1) = r[24];
        *control.add(2) = r[25];
        *control.add(3) = (r[29] != 0) as u32;
        a2fo_draw_rgba(r[34], fog_color);
        *fog_color.add(3) = if r[28] != 0 { 1.0 } else { 0.0 };
        *fog = f32::from_bits(r[36]);
        *fog.add(1) = f32::from_bits(r[37]);
        *fog.add(2) = f32::from_bits(r[38]);
        *fog.add(3) = r[35] as f32;
        a2fo_draw_rgba(r[60], factor);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_fixed_base(
    input: *const A2foDrawFixedInput,
    out: *mut A2foDrawFixed,
) -> bool {
    if input.is_null() || out.is_null() {
        return false;
    }
    let input = unsafe { &*input };
    for ptr in [
        input.world,
        input.view,
        input.projection,
        input.textures,
        input.material_diffuse,
        input.material_ambient,
        input.material_specular,
        input.material_emissive,
    ] {
        if ptr.is_null() {
            return false;
        }
    }
    if input.render.is_null() || input.stages.is_null() || input.tex_components.is_null() {
        return false;
    }
    unsafe { ptr::write_bytes(out, 0, 1) };
    let c = unsafe { &mut *out };
    let r = unsafe { core::slice::from_raw_parts(input.render, 256) };
    let stages = unsafe { core::slice::from_raw_parts(input.stages, 8 * 33) };
    let components = unsafe { core::slice::from_raw_parts(input.tex_components, 8) };
    unsafe {
        ptr::copy_nonoverlapping(input.world, c.world.as_mut_ptr(), 16);
        ptr::copy_nonoverlapping(input.view, c.view.as_mut_ptr(), 16);
        ptr::copy_nonoverlapping(input.projection, c.projection.as_mut_ptr(), 16);
        ptr::copy_nonoverlapping(input.material_diffuse, c.material_diffuse.as_mut_ptr(), 4);
        ptr::copy_nonoverlapping(input.material_ambient, c.material_ambient.as_mut_ptr(), 4);
        ptr::copy_nonoverlapping(input.material_specular, c.material_specular.as_mut_ptr(), 4);
        ptr::copy_nonoverlapping(input.material_emissive, c.material_emissive.as_mut_ptr(), 4);
    }
    c.normal[15] = 1.0;
    c.material_specular[3] = input.material_power;
    unsafe { a2fo_draw_rgba(r[139], c.global_ambient.as_mut_ptr()) };
    c.camera[3] = if r[143] != 0 { 1.0 } else { 0.0 };
    c.viewport = [
        input.x as f32,
        input.y as f32,
        input.width as f32,
        input.height as f32,
    ];
    c.flags = [
        input.transformed as u32,
        (r[137] != 0 && !input.transformed) as u32,
        input.has_diffuse as u32,
        input.has_specular as u32,
    ];
    c.material_source = if r[141] != 0 {
        [r[145], r[147], r[146], r[148]]
    } else {
        [0; 4]
    };
    let mut need_normal = r[137] != 0 && input.has_normal;
    for n in 0..8 {
        unsafe {
            ptr::copy_nonoverlapping(input.textures.add(n * 16), c.texture[n].as_mut_ptr(), 16)
        };
        let base = n * 33;
        let selector = stages[base + 11];
        c.texcoord[n][0] = selector;
        c.texcoord[n][1] = stages[base + 24];
        c.texcoord[n][2] = components[(selector & 7) as usize];
        if n < (input.active as usize).min(8) && matches!(selector & 0xffff0000, 0x10000 | 0x30000)
        {
            need_normal = true;
        }
    }
    c.fog = [
        f32::from_bits(r[36]),
        f32::from_bits(r[37]),
        f32::from_bits(r[38]),
        r[140] as f32,
    ];
    need_normal
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_fixed_lights(
    lights: *const A2foDrawLight,
    out: *mut A2foDrawFixed,
) -> bool {
    if lights.is_null() || out.is_null() {
        return false;
    }
    let lights = unsafe { core::slice::from_raw_parts(lights, 8) };
    let c = unsafe { &mut *out };
    for (n, l) in lights.iter().enumerate() {
        if !l.enabled {
            continue;
        }
        if l.kind != 1 && l.kind != 3 {
            return false;
        }
        c.light_type[n][0] = l.kind;
        c.light_diffuse[n] = l.diffuse;
        c.light_ambient[n] = l.ambient;
        c.light_specular[n] = l.specular;
        c.light_position[n][0..3].copy_from_slice(&l.position);
        c.light_direction[n][0..3].copy_from_slice(&l.direction);
        c.light_attenuation[n] = [l.attenuation0, l.attenuation1, l.attenuation2, l.range];
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_alpha_note(
    color_op: u32,
    alpha_op: u32,
    has_log: bool,
    note_count: usize,
) -> bool {
    color_op != 1 && alpha_op == 1 && has_log && note_count < 16
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_alias_step(
    prior: bool,
    source_present: bool,
    same_color: bool,
    same_depth: bool,
) -> bool {
    prior || (source_present && (same_color || same_depth))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_target_admit(
    has_color: bool,
    has_depth: bool,
    need_depth: bool,
    aliases: bool,
) -> u32 {
    if !has_color {
        1
    } else if need_depth && !has_depth {
        2
    } else if aliases {
        3
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_log_admit(has_log: bool, warning_count: usize) -> bool {
    has_log && warning_count < 64
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_log_limit(warning_count: usize) -> bool {
    warning_count == 64
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_draw_cache_evict(count: usize, limit: usize) -> bool {
    count >= limit
}

#[cfg(all(target_os = "windows", target_arch = "x86"))]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_inverse3(matrix: *const f32, out: *mut f32) -> bool {
    use core::arch::asm;
    if matrix.is_null() || out.is_null() {
        return false;
    }
    // Exact GNU++17/O2 x87 operation and float spill sequence from the frozen
    // draw_pipeline.cpp. Slots 0..7 match the native stack temporaries.
    let mut scratch = [0u32; 11];
    scratch[8] = f32::MAX.to_bits();
    scratch[9] = 1e-12f32.to_bits();
    scratch[10] = 1f32.to_bits();
    let result: usize;
    unsafe {
        asm!(
      "fld dword ptr [eax]", "fstp dword ptr [ecx+16]",
      "fld dword ptr [eax+4]", "fstp dword ptr [ecx+4]",
      "fld dword ptr [eax+8]", "fld dword ptr [eax+16]", "fst dword ptr [ecx+28]",
      "fld dword ptr [eax+20]", "fst dword ptr [ecx+20]",
      "fld dword ptr [eax+24]", "fld dword ptr [eax+32]", "fstp dword ptr [ecx]",
      "fld dword ptr [eax+36]", "fld dword ptr [eax+40]",
      "fld st(3)", "fmul st(0),st(1)",
      "fld st(3)", "fmul st(0),st(3)", "fsubp st(1),st(0)",
      "fld st(5)", "fmul st(0),st(2)", "fstp dword ptr [ecx+8]",
      "fld dword ptr [ecx]", "fmul st(0),st(4)", "fstp dword ptr [ecx+12]",
      "fxch st(5)", "fmul st(0),st(2)",
      "fld dword ptr [ecx]", "fmulp st(5),st(0)", "fsubrp st(4),st(0)",
      "fld st(3)", "fstp dword ptr [ecx+24]",
      "fld dword ptr [ecx+16]", "fmul st(0),st(5)",
      "fld dword ptr [ecx+8]", "fsub dword ptr [ecx+12]", "fmul dword ptr [ecx+4]",
      "fsubp st(1),st(0)", "fxch st(4)", "fmul st(0),st(6)", "faddp st(4),st(0)",
      "fld st(3)", "fabs", "fld dword ptr [ecx+32]", "fucomip st(0),st(1)",
      "jb 2f", "fld dword ptr [ecx+36]", "fcomip st(0),st(1)",
      "fstp st(0)", "ja 3f", "fxch st(3)", "fdivr dword ptr [ecx+40]",
      "mov eax,1", "fmul st(4),st(0)", "fxch st(4)", "fstp dword ptr [edx]",
      "fld st(4)", "fmul st(0),st(1)",
      "fld dword ptr [ecx+4]", "fld st(0)", "fmul st(0),st(5)",
      "fsubp st(2),st(0)", "fxch st(1)", "fmul st(0),st(5)",
      "fstp dword ptr [edx+4]", "fmul st(0),st(2)",
      "fld dword ptr [ecx+20]", "fmul st(0),st(6)", "fsubp st(1),st(0)",
      "fmul st(0),st(4)", "fstp dword ptr [edx+8]",
      "fld dword ptr [ecx+12]", "fsub dword ptr [ecx+8]", "fmul st(0),st(4)",
      "fstp dword ptr [edx+12]",
      "fld dword ptr [ecx+16]", "fmul st(3),st(0)",
      "fld dword ptr [ecx]", "fmul st(0),st(6)", "fsubp st(4),st(0)",
      "fxch st(3)", "fmul st(0),st(4)", "fstp dword ptr [edx+16]",
      "fld dword ptr [ecx+28]", "fmul st(5),st(0)", "fxch st(2)",
      "fmul st(0),st(3)", "fsubp st(5),st(0)", "fxch st(4)",
      "fmul st(0),st(3)", "fstp dword ptr [edx+20]",
      "fld dword ptr [ecx+24]", "fmul st(0),st(3)", "fstp dword ptr [edx+24]",
      "fld dword ptr [ecx+4]", "fld dword ptr [ecx]", "fmul st(0),st(1)",
      "fxch st(5)", "fmul st(0),st(3)", "fsubp st(5),st(0)",
      "fxch st(4)", "fmul st(0),st(3)", "fstp dword ptr [edx+28]",
      "fxch st(1)", "fmul dword ptr [ecx+20]", "fxch st(3)",
      "fmulp st(1),st(0)", "fsubp st(2),st(0)", "fmulp st(1),st(0)",
      "fstp dword ptr [edx+32]", "jmp 4f",
      "2:", "fstp st(0)", "fstp st(0)", "fstp st(0)", "fstp st(0)",
      "fstp st(0)", "fstp st(0)", "fstp st(0)", "xor eax,eax", "jmp 4f",
      "3:", "fstp st(0)", "fstp st(0)", "fstp st(0)",
      "fstp st(0)", "fstp st(0)", "fstp st(0)", "xor eax,eax",
      "4:",
      inlateout("eax")matrix as usize=>result,
      in("edx")out,in("ecx")scratch.as_mut_ptr(),
      out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
      out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,
      options(nostack));
    }
    result != 0
}

#[cfg(all(target_os = "windows", target_arch = "x86"))]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_camera(view: *const f32, inverse: *const f32, out: *mut f32) {
    use core::arch::asm;
    if view.is_null() || inverse.is_null() || out.is_null() {
        return;
    }
    for col in 0..3 {
        let inv = unsafe { inverse.add(col) };
        let dst = unsafe { out.add(col) };
        unsafe {
            asm!(
            "fld dword ptr [{inv}]", "fmul dword ptr [{view}+48]",
            "fld dword ptr [{inv}+12]", "fmul dword ptr [{view}+52]",
            "faddp st(1),st(0)",
            "fld dword ptr [{inv}+24]", "fmul dword ptr [{view}+56]",
            "faddp st(1),st(0)", "fchs", "fstp dword ptr [{dst}]",
            inv=in(reg)inv,view=in(reg)view,dst=in(reg)dst,
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,
            options(nostack));
        }
    }
}

pub type A2foDrawOperation = unsafe extern "C" fn(*mut core::ffi::c_void, u32) -> i32;
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_prepare_workflow(
    context: *mut core::ffi::c_void,
    fixed_function: bool,
    operation: Option<A2foDrawOperation>,
) -> i32 {
    let Some(operation) = operation else {
        return 0x8876086cu32 as i32;
    };
    for step in 0..=if fixed_function { 5 } else { 6 } {
        let hr = unsafe { operation(context, step) };
        if hr < 0 {
            return hr;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_buffer_workflow(
    context: *mut core::ffi::c_void,
    needs_create: bool,
    operation: Option<A2foDrawOperation>,
) -> i32 {
    let Some(operation) = operation else {
        return 0x8876086cu32 as i32;
    };
    for step in 0..=2 {
        if step == 0 && !needs_create {
            continue;
        }
        let hr = unsafe { operation(context, step) };
        if hr < 0 {
            return hr;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_program_workflow(
    context: *mut core::ffi::c_void,
    operation: Option<A2foDrawOperation>,
) -> i32 {
    let Some(operation) = operation else {
        return 0x8876086cu32 as i32;
    };
    for step in 0..=4 {
        let hr = unsafe { operation(context, step) };
        if step == 0 && hr == 1 {
            return 0;
        }
        if hr < 0 {
            return hr;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_pixel_workflow(
    context: *mut core::ffi::c_void,
    operation: Option<A2foDrawOperation>,
) -> i32 {
    let Some(operation) = operation else {
        return 0x8876086cu32 as i32;
    };
    for step in 0..=2 {
        let hr = unsafe { operation(context, step) };
        if step == 0 && hr == 1 {
            return 0;
        }
        if hr < 0 {
            return hr;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_state_workflow(
    context: *mut core::ffi::c_void,
    operation: Option<A2foDrawOperation>,
) -> i32 {
    let Some(operation) = operation else {
        return 0x8876086cu32 as i32;
    };
    for step in 0..=3 {
        let hr = unsafe { operation(context, step) };
        if step == 0 && hr == 1 {
            return 0;
        }
        if hr < 0 {
            return hr;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_raster_inverse(width: u32, height: u32, out: *mut f32) {
    if out.is_null() || width == 0 || height == 0 {
        return;
    }
    unsafe {
        *out = 1.0 / (width as f32);
        *out.add(1) = 1.0 / (height as f32);
        *out.add(2) = 0.0;
        *out.add(3) = 0.0;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_initialize_workflow(
    context: *mut core::ffi::c_void,
    has_device: bool,
    has_context: bool,
    operation: Option<A2foDrawOperation>,
) -> i32 {
    let Some(operation) = operation else {
        return 0x8876086cu32 as i32;
    };
    if !has_device || !has_context {
        return 0x8876086cu32 as i32;
    }
    let feature = unsafe { operation(context, 0) };
    if (feature as u32) < 0xb000 {
        return 0x8876086cu32 as i32;
    }
    let kind = unsafe { operation(context, 1) };
    if kind != 0 {
        return 0x8876086cu32 as i32;
    }
    let same = unsafe { operation(context, 2) };
    if same != 1 {
        return 0x8876086cu32 as i32;
    }
    unsafe { operation(context, 3) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_dispatch_workflow(
    context: *mut core::ffi::c_void,
    operation: Option<A2foDrawOperation>,
) -> i32 {
    let Some(operation) = operation else {
        return 0x8876086cu32 as i32;
    };
    for step in 0..=17 {
        let hr = unsafe { operation(context, step) };
        if hr < 0 {
            return hr;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_viewport(
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    out: *mut f32,
) {
    if out.is_null() {
        return;
    }
    unsafe {
        *out = x as f32;
        *out.add(1) = y as f32;
        *out.add(2) = width as f32;
        *out.add(3) = height as f32;
    }
}

#[cfg(not(all(target_os = "windows", target_arch = "x86")))]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_camera(view: *const f32, inverse: *const f32, out: *mut f32) {
    if view.is_null() || inverse.is_null() || out.is_null() {
        return;
    }
    let view = unsafe { core::slice::from_raw_parts(view, 16) };
    let inverse = unsafe { core::slice::from_raw_parts(inverse, 9) };
    for col in 0..3 {
        unsafe {
            *out.add(col) = -(view[12] * inverse[col]
                + view[13] * inverse[3 + col]
                + view[14] * inverse[6 + col])
        };
    }
}

#[cfg(not(all(target_os = "windows", target_arch = "x86")))]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_draw_inverse3(matrix: *const f32, out: *mut f32) -> bool {
    if matrix.is_null() || out.is_null() {
        return false;
    }
    let m = unsafe { core::slice::from_raw_parts(matrix, 16) };
    let (a, b, c, d, e, f, g, h, i) = (m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]);
    let det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if !det.is_finite() || det.abs() < 1e-12 {
        return false;
    }
    let k = 1.0 / det;
    let values = [
        (e * i - f * h) * k,
        (c * h - b * i) * k,
        (b * f - c * e) * k,
        (f * g - d * i) * k,
        (a * i - c * g) * k,
        (c * d - a * f) * k,
        (d * h - e * g) * k,
        (b * g - a * h) * k,
        (a * e - b * d) * k,
    ];
    unsafe { ptr::copy_nonoverlapping(values.as_ptr(), out, 9) };
    true
}
