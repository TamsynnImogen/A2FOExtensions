// D3D8 resource decisions. The C++ adapter owns COM objects and storage.
const OK: i32 = 3;
const INVALID: i32 = 0;
const NOT_FOUND: i32 = 1;
const MORE_DATA: i32 = 2;
const UNKNOWN: u32 = 1;
const READONLY: u32 = 0x10;
const DISCARD: u32 = 0x2000;
const NOOVERWRITE: u32 = 0x1000;
const NOSYSLOCK: u32 = 0x800;
const NO_DIRTY_UPDATE: u32 = 0x8000;

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_private_set(
    present: bool,
    size: u32,
    flags: u32,
    pointer_size: u32,
) -> i32 {
    if !present || flags & !UNKNOWN != 0 || (flags & UNKNOWN != 0 && size != pointer_size) {
        INVALID
    } else {
        OK
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_private_get(
    size_present: bool,
    found: bool,
    data_present: bool,
    capacity: u32,
    needed: u32,
) -> i32 {
    if !size_present {
        INVALID
    } else if !found {
        NOT_FOUND
    } else if data_present && capacity < needed {
        MORE_DATA
    } else {
        OK
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_resource_hash(bytes: *const u8, size: usize) -> u64 {
    let mut h = 14695981039346656037u64;
    if size != 0 && bytes.is_null() {
        return h;
    }
    for i in 0..size {
        h = (h ^ unsafe { *bytes.add(i) } as u64).wrapping_mul(1099511628211);
    }
    h
}
const DXT1: u32 = u32::from_le_bytes(*b"DXT1");
const DXT2: u32 = u32::from_le_bytes(*b"DXT2");
const DXT3: u32 = u32::from_le_bytes(*b"DXT3");
const DXT4: u32 = u32::from_le_bytes(*b"DXT4");
const DXT5: u32 = u32::from_le_bytes(*b"DXT5");
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_block_format(f: u32) -> bool {
    matches!(f, DXT1 | DXT2 | DXT3 | DXT4 | DXT5)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_pitch(f: u32, w: u32) -> u32 {
    if a2fo_resource_block_format(f) {
        return w
            .wrapping_add(3)
            .wrapping_div(4)
            .wrapping_mul(if f == DXT1 { 8 } else { 16 });
    }
    match f {
        21 | 22 => w.wrapping_mul(4),
        20 => w.wrapping_mul(3),
        23 | 24 | 25 | 26 | 51 => w.wrapping_mul(2),
        50 | 28 | 41 => w,
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_rows(f: u32, h: u32) -> u32 {
    if a2fo_resource_block_format(f) {
        h.wrapping_add(3) / 4
    } else {
        h
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_next_dimension(v: u32) -> u32 {
    (v / 2).max(1)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_levels(mut w: u32, mut h: u32) -> u32 {
    let mut levels = 1;
    while w > 1 || h > 1 {
        w = a2fo_resource_next_dimension(w);
        h = a2fo_resource_next_dimension(h);
        levels += 1;
    }
    levels
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_lod(requested: u32, levels: usize) -> u32 {
    requested.min((levels as u32).wrapping_sub(1))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_buffer_lock(
    out: bool,
    locked: bool,
    length: usize,
    offset: u32,
    size: u32,
    flags: u32,
) -> bool {
    out && !locked
        && (offset as usize) <= length
        && (size == 0 || (size as usize) <= length - offset as usize)
        && flags & !(READONLY | DISCARD | NOOVERWRITE | NOSYSLOCK) == 0
        && !(flags & READONLY != 0 && flags & (DISCARD | NOOVERWRITE) != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_resource_rect_lock(
    out: bool,
    locked: bool,
    render_target: bool,
    flags: u32,
    format: u32,
    width: u32,
    height: u32,
    pitch: u32,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
    offset: *mut usize,
) -> bool {
    if !out
        || locked
        || render_target
        || offset.is_null()
        || flags & !(READONLY | DISCARD | NOSYSLOCK | NO_DIRTY_UPDATE) != 0
        || flags & READONLY != 0 && flags & DISCARD != 0
        || left < 0
        || top < 0
        || right <= left
        || bottom <= top
        || right > width as i32
        || bottom > height as i32
    {
        return false;
    }
    let result = if a2fo_resource_block_format(format) {
        if left % 4 != 0
            || top % 4 != 0
            || right % 4 != 0 && right != width as i32
            || bottom % 4 != 0 && bottom != height as i32
        {
            return false;
        }
        ((top / 4) as u32)
            .wrapping_mul(pitch)
            .wrapping_add(((left / 4) as u32).wrapping_mul(if format == DXT1 { 8 } else { 16 }))
    } else {
        if width == 0 {
            return false;
        }
        (top as u32)
            .wrapping_mul(pitch)
            .wrapping_add((left as u32).wrapping_mul(pitch / width))
    };
    unsafe {
        *offset = result as usize;
    }
    true
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_surface_lock(
    out: bool,
    locked: bool,
    target: bool,
    region: bool,
    flags: u32,
) -> bool {
    out && !locked && (!target || flags & READONLY != 0 && !region)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_upload_immutable_level(level: usize, count: usize) -> bool {
    level < count
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_resource_upload_immutable(
    level: usize,
    count: usize,
    actual: u64,
    expected: *const u64,
) -> bool {
    level < count && !expected.is_null() && actual == unsafe { *expected.add(level) }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_upload_plan(
    compressed: bool,
    dxt4: bool,
    level: usize,
    has_gpu: bool,
) -> u32 {
    if !compressed && dxt4 && level == 0 {
        1
    } else if compressed {
        2
    } else if !has_gpu {
        3
    } else {
        4
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_target_plan(has_texture: bool, has_target: bool) -> bool {
    has_texture && !has_target
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_has_level(
    output_present: bool,
    level: usize,
    count: usize,
) -> bool {
    output_present && level < count
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_can_unlock(locked: bool) -> bool {
    locked
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_should_upload(readonly: bool) -> bool {
    !readonly
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_can_dirty(compressed: bool) -> bool {
    !compressed
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_texture_owner(kind: u32, has_texture: bool) -> bool {
    kind == 3 && has_texture
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_can_initialize(pitch: u32) -> bool {
    pitch != 0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_resource_swizzle_bgra(dst: *mut u8, src: *const u8, size: usize) {
    if dst.is_null() || src.is_null() {
        return;
    }
    for i in (0..size / 4 * 4).step_by(4) {
        unsafe {
            *dst.add(i) = *src.add(i + 2);
        }
        unsafe {
            *dst.add(i + 1) = *src.add(i + 1);
        }
        unsafe {
            *dst.add(i + 2) = *src.add(i);
        }
        unsafe {
            *dst.add(i + 3) = *src.add(i + 3);
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_accumulate_size(total: u64, size: u32) -> u64 {
    total.wrapping_add(size as u64)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_is_object(flags: u32) -> bool {
    flags & UNKNOWN != 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_is_readonly(flags: u32) -> bool {
    flags & READONLY != 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_is_render_target(usage: u32) -> bool {
    usage & 1 != 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_resource_bc_miss_plan(error_present: bool, bc_assets: bool) -> u32 {
    if error_present {
        1
    } else if bc_assets {
        2
    } else {
        0
    }
}
