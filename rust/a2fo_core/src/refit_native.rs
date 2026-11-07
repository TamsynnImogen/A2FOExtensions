//! Private pixel and policy core for Refit's Win32 and D3D8 adapters.
// Keep the legacy field order, bounded indices and ABI-shaped arguments auditable.
#![allow(clippy::too_many_arguments)]
#[cfg(target_arch = "x86")]
use core::arch::x86::*;
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::*;
use core::{ffi::c_void, slice};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct NativeRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

#[repr(C)]
pub struct SkinDirect {
    registered: i32,
    dc_kind: i32,
    map_mode: i32,
    graphics_mode: i32,
    layout: i32,
    viewport_x: i32,
    viewport_y: i32,
    window_x: i32,
    window_y: i32,
    clip_kind: i32,
    clip_left: i32,
    clip_top: i32,
    clip_right: i32,
    clip_bottom: i32,
    object_valid: i32,
    bits_match: i32,
    section_null: i32,
    section_offset: i32,
    header_size: i32,
    planes: i32,
    bit_count: i32,
    compression: i32,
    header_width: i32,
    header_height: i32,
    width: i32,
    height: i32,
    stride: i32,
    bitmap_planes: i32,
    bitmap_bit_count: i32,
    bits: *mut u8,
}

const ATLAS_WIDTH: usize = 128;
const ATLAS_HEIGHT: usize = 256;
const HEADER: [u32; 8] = [0x31534955, 128, 256, 64, 64, 8, 0, 0];

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_atlas_size_valid(size: usize) -> i32 {
    (size == 32 + ATLAS_WIDTH * ATLAS_HEIGHT * 4) as i32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_tile_valid(width: i32, height: i32) -> i32 {
    (width > 0 && height > 0 && width <= i32::MAX / 4) as i32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_direct_precheck(
    target_valid: i32,
    registered: i32,
    registered_width: i32,
    registered_height: i32,
    rect: NativeRect,
) -> i32 {
    (target_valid != 0
        && rect.width > 0
        && rect.height > 0
        && registered != 0
        && registered_width > 0
        && registered_height > 0) as i32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_direct_property(step: i32, value: i32) -> i32 {
    match step {
        0 => (value == 10) as i32,
        1 | 2 => (value == 1) as i32,
        3 => (value == 0) as i32,
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_direct_origins(
    viewport_x: i32,
    viewport_y: i32,
    window_x: i32,
    window_y: i32,
) -> i32 {
    (viewport_x == 0 && viewport_y == 0 && window_x == 0 && window_y == 0) as i32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_direct_clip(
    rect: NativeRect,
    kind: i32,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
) -> i32 {
    (kind == 2
        && rect.x >= left
        && rect.y >= top
        && i64::from(rect.x) + i64::from(rect.width) <= i64::from(right)
        && i64::from(rect.y) + i64::from(rect.height) <= i64::from(bottom)) as i32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_tile_matches(
    rect: NativeRect,
    bits_valid: i32,
    width: i32,
    height: i32,
) -> i32 {
    (bits_valid != 0 && width == rect.width && height == rect.height) as i32
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_atlas_valid(data: *const u8, size: usize) -> i32 {
    if data.is_null() || size != 32 + ATLAS_WIDTH * ATLAS_HEIGHT * 4 {
        return 0;
    }
    let bytes = unsafe { slice::from_raw_parts(data, size) };
    for (i, value) in HEADER.iter().enumerate() {
        let offset = i * 4;
        if u32::from_le_bytes(bytes[offset..offset + 4].try_into().unwrap()) != *value {
            return 0;
        }
    }
    for pixel in bytes[32..].chunks_exact(4) {
        if pixel[0] > pixel[3] || pixel[1] > pixel[3] || pixel[2] > pixel[3] {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_source_axis(position: i32, length: i32, corner: i32) -> i32 {
    if corner < 1 || length <= 2 * corner {
        return 0;
    }
    if position < corner {
        if corner == 1 {
            0
        } else {
            position * 7 / (corner - 1)
        }
    } else if position >= length - corner {
        if corner == 1 {
            63
        } else {
            56 + (position - (length - corner)) * 7 / (corner - 1)
        }
    } else {
        8 + (position - corner) * 48 / (length - 2 * corner)
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_fill_tile(
    atlas: *const u8,
    output: *mut u8,
    index: u32,
    width: i32,
    height: i32,
    corner: i32,
) -> i32 {
    if atlas.is_null()
        || output.is_null()
        || index >= 8
        || width <= 0
        || height <= 0
        || width > i32::MAX / 4
        || !(1..=32).contains(&corner)
        || width <= corner * 2
        || height <= corner * 2
    {
        return 0;
    }
    let source = unsafe { slice::from_raw_parts(atlas, ATLAS_WIDTH * ATLAS_HEIGHT * 4) };
    let dest = unsafe { slice::from_raw_parts_mut(output, width as usize * height as usize * 4) };
    let origin_x = (index as usize % 2) * 64;
    let origin_y = (index as usize / 2) * 64;
    for y in 0..height as usize {
        let sy = origin_y + a2fo_refit_skin_source_axis(y as i32, height, corner) as usize;
        for x in 0..width as usize {
            let sx = origin_x + a2fo_refit_skin_source_axis(x as i32, width, corner) as usize;
            let src = (sy * ATLAS_WIDTH + sx) * 4;
            let dst = (y * width as usize + x) * 4;
            dest[dst..dst + 4].copy_from_slice(&source[src..src + 4]);
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_direct_valid(
    r: NativeRect,
    info: *const SkinDirect,
) -> i32 {
    let Some(i) = (unsafe { info.as_ref() }) else {
        return 0;
    };
    if r.width <= 0
        || r.height <= 0
        || i.registered == 0
        || i.dc_kind != 10
        || i.map_mode != 1
        || i.graphics_mode != 1
        || i.layout != 0
        || i.viewport_x != 0
        || i.viewport_y != 0
        || i.window_x != 0
        || i.window_y != 0
        || i.clip_kind != 2
        || r.x < i.clip_left
        || r.y < i.clip_top
        || i64::from(r.x) + i64::from(r.width) > i64::from(i.clip_right)
        || i64::from(r.y) + i64::from(r.height) > i64::from(i.clip_bottom)
        || i.object_valid == 0
        || i.bits_match == 0
        || i.section_null == 0
        || i.section_offset != 0
        || i.header_size != 40
        || i.planes != 1
        || i.bit_count != 32
        || i.compression != 0
        || i.header_height == 0
        || i.width <= 0
        || i.height <= 0
        || i.bitmap_bit_count != 32
        || i.bitmap_planes != 1
        || i.stride <= 0
        || i64::from(i.width) * 4 != i64::from(i.stride)
        || i.width > i32::MAX / 4
        || i64::from(i.header_height).abs() != i64::from(i.height)
        || i.header_width != i.width
        || i.bits.is_null()
        || r.x < 0
        || r.y < 0
        || r.x > i.width
        || r.y > i.height
        || r.width > i.width - r.x
        || r.height > i.height - r.y
    {
        return 0;
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_blend(output: *mut u8, source: *const u8, width: i32) {
    if output.is_null() || source.is_null() || width <= 0 {
        return;
    }
    let dst = unsafe { slice::from_raw_parts_mut(output, width as usize * 4) };
    let src = unsafe { slice::from_raw_parts(source, width as usize * 4) };
    for (out, pixel) in dst.chunks_exact_mut(4).zip(src.chunks_exact(4)) {
        let inverse = 255u32 - pixel[3] as u32;
        for channel in 0..4 {
            out[channel] = (pixel[channel] as u32 + (out[channel] as u32 * inverse + 127) / 255)
                .min(255) as u8;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_tint(output: *mut u8, rgb: u32, alpha: u8, width: i32) {
    if output.is_null() || width <= 0 {
        return;
    }
    let dst = unsafe { slice::from_raw_parts_mut(output, width as usize * 4) };
    let src = [rgb as u8, (rgb >> 8) as u8, (rgb >> 16) as u8, 0];
    for pixel in dst.chunks_exact_mut(4) {
        for channel in 0..4 {
            pixel[channel] = ((src[channel] as u32 * alpha as u32
                + pixel[channel] as u32 * (255 - alpha as u32)
                + 127)
                / 255) as u8;
        }
    }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "sse2")]
unsafe fn blend_sse2(mut output: *mut u8, mut source: *const u8, width: i32) {
    unsafe {
        let zero = _mm_setzero_si128();
        let full = _mm_set1_epi16(255);
        let bias = _mm_set1_epi16(128);
        let mut x = 0;
        while x <= width - 4 {
            let src = _mm_loadu_si128(source as *const __m128i);
            let dst = _mm_loadu_si128(output as *const __m128i);
            let lo = _mm_unpacklo_epi8(src, zero);
            let hi = _mm_unpackhi_epi8(src, zero);
            let dlo = _mm_unpacklo_epi8(dst, zero);
            let dhi = _mm_unpackhi_epi8(dst, zero);
            let alo = _mm_shufflehi_epi16(_mm_shufflelo_epi16(lo, 0xff), 0xff);
            let ahi = _mm_shufflehi_epi16(_mm_shufflelo_epi16(hi, 0xff), 0xff);
            let plo = _mm_mullo_epi16(dlo, _mm_sub_epi16(full, alo));
            let phi = _mm_mullo_epi16(dhi, _mm_sub_epi16(full, ahi));
            let rlo = _mm_add_epi16(plo, bias);
            let rhi = _mm_add_epi16(phi, bias);
            let qlo = _mm_srli_epi16(_mm_add_epi16(rlo, _mm_srli_epi16(rlo, 8)), 8);
            let qhi = _mm_srli_epi16(_mm_add_epi16(rhi, _mm_srli_epi16(rhi, 8)), 8);
            let result = _mm_packus_epi16(_mm_add_epi16(lo, qlo), _mm_add_epi16(hi, qhi));
            _mm_storeu_si128(output as *mut __m128i, result);
            output = output.add(16);
            source = source.add(16);
            x += 4;
        }
        a2fo_refit_skin_blend(output, source, width - x);
    }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "sse2")]
unsafe fn tint_sse2(mut output: *mut u8, rgb: u32, alpha: u8, width: i32) {
    unsafe {
        let zero = _mm_setzero_si128();
        let bias = _mm_set1_epi16(128);
        let source = [
            (rgb & 255) as i16,
            ((rgb >> 8) & 255) as i16,
            ((rgb >> 16) & 255) as i16,
            0,
        ];
        let raw = _mm_set_epi16(
            source[3], source[2], source[1], source[0], source[3], source[2], source[1], source[0],
        );
        let factor = _mm_set1_epi16(alpha as i16);
        let inverse = _mm_set1_epi16((255 - alpha as i32) as i16);
        let scaled = _mm_mullo_epi16(raw, factor);
        let mut x = 0;
        while x <= width - 4 {
            let dst = _mm_loadu_si128(output as *const __m128i);
            let dlo = _mm_unpacklo_epi8(dst, zero);
            let dhi = _mm_unpackhi_epi8(dst, zero);
            let rlo = _mm_add_epi16(_mm_add_epi16(_mm_mullo_epi16(dlo, inverse), scaled), bias);
            let rhi = _mm_add_epi16(_mm_add_epi16(_mm_mullo_epi16(dhi, inverse), scaled), bias);
            let qlo = _mm_srli_epi16(_mm_add_epi16(rlo, _mm_srli_epi16(rlo, 8)), 8);
            let qhi = _mm_srli_epi16(_mm_add_epi16(rhi, _mm_srli_epi16(rhi, 8)), 8);
            _mm_storeu_si128(output as *mut __m128i, _mm_packus_epi16(qlo, qhi));
            output = output.add(16);
            x += 4;
        }
        a2fo_refit_skin_tint(output, rgb, alpha, width - x);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_blend_fast(
    output: *mut u8,
    source: *const u8,
    width: i32,
    use_sse2: i32,
) {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    if use_sse2 != 0 && width > 0 && !output.is_null() && !source.is_null() {
        unsafe { blend_sse2(output, source, width) };
        return;
    }
    let _ = use_sse2;
    unsafe { a2fo_refit_skin_blend(output, source, width) };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_tint_fast(
    output: *mut u8,
    rgb: u32,
    alpha: u8,
    width: i32,
    use_sse2: i32,
) {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    if use_sse2 != 0 && width > 0 && !output.is_null() {
        unsafe { tint_sse2(output, rgb, alpha, width) };
        return;
    }
    let _ = use_sse2;
    unsafe { a2fo_refit_skin_tint(output, rgb, alpha, width) };
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_paint_valid(
    target_valid: i32,
    r: NativeRect,
    part: u32,
    corner: i32,
) -> i32 {
    (target_valid != 0
        && part < 8
        && (1..=32).contains(&corner)
        && r.width > corner * 2
        && r.height > corner * 2) as i32
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_skin_cache_decision(
    width: i32,
    height: i32,
    tile_count: usize,
    pixel_count: usize,
) -> i32 {
    if width <= 0 || height <= 0 {
        return 0;
    }
    let count = width as u64 * height as u64;
    if count > 8_000_000 {
        return 0;
    }
    if tile_count >= 96 || pixel_count > 8_000_000usize - count as usize {
        2
    } else {
        1
    }
}

type SkinOp = unsafe extern "C" fn(*mut c_void, i32) -> i32;
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_skin_paint(
    target_valid: i32,
    r: NativeRect,
    part: u32,
    corner: i32,
    operation: SkinOp,
    context: *mut c_void,
) -> i32 {
    if a2fo_refit_skin_paint_valid(target_valid, r, part, corner) == 0 {
        return 0;
    }
    if unsafe { operation(context, 0) } == 0 {
        return 0;
    }
    if unsafe { operation(context, 1) } == 0 {
        return 0;
    }
    if unsafe { operation(context, 2) } != 0 {
        return 1;
    }
    unsafe { operation(context, 3) }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct GpuImage {
    width: i32,
    height: i32,
    pixels: *const u32,
    pixel_count: usize,
}
#[repr(C)]
#[derive(Default)]
pub struct GpuCaps {
    texture_caps: u32,
    max_width: u32,
    max_height: u32,
}
#[repr(C)]
#[derive(Default)]
pub struct GpuCacheState {
    present: i32,
    device_match: i32,
    uploaded: i32,
    alpha_preserved: i32,
    width: u32,
    height: u32,
    texture_width: u32,
    texture_height: u32,
    revision: u64,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct GpuVertex {
    x: f32,
    y: f32,
    z: f32,
    rhw: f32,
    colour: u32,
    u: f32,
    v: f32,
}

#[repr(C)]
pub struct GpuOps {
    context: *mut c_void,
    get_caps: unsafe extern "C" fn(*mut c_void, *mut GpuCaps) -> i32,
    cache_state: unsafe extern "C" fn(*mut c_void, i32, *mut GpuCacheState) -> i32,
    reset: unsafe extern "C" fn(*mut c_void, i32),
    create: unsafe extern "C" fn(*mut c_void, i32, u32, u32, u32, u32) -> i32,
    lock: unsafe extern "C" fn(*mut c_void, i32, *mut *mut u8, *mut i32) -> i32,
    unlock: unsafe extern "C" fn(*mut c_void, i32) -> i32,
    mark_upload: unsafe extern "C" fn(*mut c_void, i32, u64, i32, i32),
    capture: unsafe extern "C" fn(*mut c_void) -> i32,
    restore: unsafe extern "C" fn(*mut c_void),
    viewport: unsafe extern "C" fn(*mut c_void, u32, u32) -> i32,
    render_state: unsafe extern "C" fn(*mut c_void, i32, i32) -> i32,
    pixel_shader: unsafe extern "C" fn(*mut c_void) -> i32,
    vertex_shader: unsafe extern "C" fn(*mut c_void) -> i32,
    bind: unsafe extern "C" fn(*mut c_void, i32) -> i32,
    stage_state: unsafe extern "C" fn(*mut c_void, i32, i32) -> i32,
    draw: unsafe extern "C" fn(*mut c_void, *const GpuVertex) -> i32,
}

fn valid_image(image: GpuImage) -> bool {
    image.width >= 1
        && image.height >= 1
        && image.width <= 3840
        && image.height <= 2160
        && (image.width as u64) * (image.height as u64) <= 8_000_000
        && image.pixel_count == (image.width as usize) * (image.height as usize)
        && !image.pixels.is_null()
}
unsafe fn texture_size(ops: &GpuOps, image: GpuImage) -> Option<(u32, u32)> {
    let mut caps = GpuCaps::default();
    if unsafe { (ops.get_caps)(ops.context, &mut caps) } == 0 {
        return None;
    }
    let mut tw = image.width as u32;
    let mut th = image.height as u32;
    if caps.texture_caps & 2 != 0 && caps.texture_caps & 0x100 == 0 {
        tw = 1;
        th = 1;
        while tw < image.width as u32 {
            tw *= 2
        }
        while th < image.height as u32 {
            th *= 2
        }
    }
    if caps.texture_caps & 0x20 != 0 {
        tw = tw.max(th);
        th = tw
    }
    if tw > caps.max_width || th > caps.max_height {
        None
    } else {
        Some((tw, th))
    }
}
unsafe fn upload(ops: &GpuOps, slot: i32, image: GpuImage, preserve: bool) -> bool {
    let mut bits = core::ptr::null_mut();
    let mut pitch = 0;
    if unsafe { (ops.lock)(ops.context, slot, &mut bits, &mut pitch) } == 0 {
        return false;
    }
    if bits.is_null() || pitch < image.width * 4 {
        unsafe { (ops.unlock)(ops.context, slot) };
        return false;
    }
    for y in 0..image.height as usize {
        for x in 0..image.width as usize {
            let src = unsafe { *image.pixels.add(y * image.width as usize + x) };
            let pixel = if preserve { src } else { src | 0xff00_0000 };
            unsafe {
                core::ptr::write_unaligned(bits.add(y * pitch as usize + x * 4) as *mut u32, pixel)
            };
        }
    }
    (unsafe { (ops.unlock)(ops.context, slot) }) != 0
}
unsafe fn prepare(
    ops: &GpuOps,
    slot: i32,
    image: GpuImage,
    tw: u32,
    th: u32,
    cached: bool,
    revision: u64,
    preserve: bool,
) -> bool {
    if !cached {
        return unsafe {
            (ops.create)(
                ops.context,
                slot,
                tw,
                th,
                image.width as u32,
                image.height as u32,
            )
        } != 0
            && unsafe { upload(ops, slot, image, preserve) };
    }
    let mut state = GpuCacheState::default();
    if unsafe { (ops.cache_state)(ops.context, slot, &mut state) } == 0 {
        return false;
    }
    if state.present != 0
        && (state.device_match == 0
            || state.width != image.width as u32
            || state.height != image.height as u32
            || state.texture_width != tw
            || state.texture_height != th)
    {
        unsafe { (ops.reset)(ops.context, slot) };
        state.present = 0;
        state.uploaded = 0;
    }
    if state.present == 0 {
        if unsafe {
            (ops.create)(
                ops.context,
                slot,
                tw,
                th,
                image.width as u32,
                image.height as u32,
            )
        } == 0
        {
            return false;
        }
        state.uploaded = 0;
    }
    if state.uploaded == 0
        || state.revision != revision
        || state.alpha_preserved != (preserve as i32)
    {
        unsafe { (ops.mark_upload)(ops.context, slot, revision, preserve as i32, 0) };
        if !unsafe { upload(ops, slot, image, preserve) } {
            return false;
        }
        unsafe { (ops.mark_upload)(ops.context, slot, revision, preserve as i32, 1) };
    }
    true
}
unsafe fn setup_state(ops: &GpuOps, width: u32, height: u32, layered: bool) -> bool {
    let mode = layered as i32;
    let mut ok = unsafe { (ops.viewport)(ops.context, width, height) } != 0;
    for index in 0..if layered { 14 } else { 12 } {
        let success = unsafe { (ops.render_state)(ops.context, index, mode) } != 0;
        ok = success && ok;
    }
    let success = unsafe { (ops.pixel_shader)(ops.context) } != 0;
    ok = success && ok;
    let success = unsafe { (ops.vertex_shader)(ops.context) } != 0;
    ok = success && ok;
    if !layered {
        let success = unsafe { (ops.bind)(ops.context, 0) } != 0;
        ok = success && ok;
    }
    for index in 0..if layered { 14 } else { 13 } {
        let success = unsafe { (ops.stage_state)(ops.context, index, mode) } != 0;
        ok = success && ok;
    }
    ok
}
unsafe fn draw_quad(
    ops: &GpuOps,
    slot: i32,
    out_width: i32,
    out_height: i32,
    image: GpuImage,
    tw: u32,
    th: u32,
    colours: [u32; 4],
    prior_ok: bool,
) -> bool {
    let bound = unsafe { (ops.bind)(ops.context, slot) } != 0;
    let ok = bound && prior_ok;
    let w = out_width as f32;
    let h = out_height as f32;
    let u = image.width as f32 / tw as f32;
    let v = image.height as f32 / th as f32;
    let vertices = [
        GpuVertex {
            x: -0.5,
            y: -0.5,
            z: 0.0,
            rhw: 1.0,
            colour: colours[0],
            u: 0.0,
            v: 0.0,
        },
        GpuVertex {
            x: w - 0.5,
            y: -0.5,
            z: 0.0,
            rhw: 1.0,
            colour: colours[1],
            u,
            v: 0.0,
        },
        GpuVertex {
            x: -0.5,
            y: h - 0.5,
            z: 0.0,
            rhw: 1.0,
            colour: colours[2],
            u: 0.0,
            v,
        },
        GpuVertex {
            x: w - 0.5,
            y: h - 0.5,
            z: 0.0,
            rhw: 1.0,
            colour: colours[3],
            u,
            v,
        },
    ];
    ok && unsafe { (ops.draw)(ops.context, vertices.as_ptr()) } != 0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_gpu_draw(
    image: GpuImage,
    revision: u64,
    cached: i32,
    ops: *const GpuOps,
) -> i32 {
    let Some(ops) = (unsafe { ops.as_ref() }) else {
        return 0;
    };
    if !valid_image(image) {
        return 0;
    }
    let Some((tw, th)) = (unsafe { texture_size(ops, image) }) else {
        return 0;
    };
    if !unsafe { prepare(ops, 0, image, tw, th, cached != 0, revision, false) } {
        return 0;
    }
    if unsafe { (ops.capture)(ops.context) } == 0 {
        return 0;
    }
    let result = (|| {
        let ok = unsafe { setup_state(ops, image.width as u32, image.height as u32, false) };
        if !ok {
            return false;
        }
        let w = image.width as f32;
        let h = image.height as f32;
        let u = w / tw as f32;
        let v = h / th as f32;
        let vertices = [
            GpuVertex {
                x: -0.5,
                y: -0.5,
                z: 0.0,
                rhw: 1.0,
                colour: 0xffff_ffff,
                u: 0.0,
                v: 0.0,
            },
            GpuVertex {
                x: w - 0.5,
                y: -0.5,
                z: 0.0,
                rhw: 1.0,
                colour: 0xffff_ffff,
                u,
                v: 0.0,
            },
            GpuVertex {
                x: -0.5,
                y: h - 0.5,
                z: 0.0,
                rhw: 1.0,
                colour: 0xffff_ffff,
                u: 0.0,
                v,
            },
            GpuVertex {
                x: w - 0.5,
                y: h - 0.5,
                z: 0.0,
                rhw: 1.0,
                colour: 0xffff_ffff,
                u,
                v,
            },
        ];
        (unsafe { (ops.draw)(ops.context, vertices.as_ptr()) }) != 0
    })();
    unsafe { (ops.restore)(ops.context) };
    result as i32
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_refit_gpu_layers(
    background: GpuImage,
    overlay: GpuImage,
    out_width: i32,
    out_height: i32,
    background_revision: u64,
    overlay_revision: u64,
    background_cached: i32,
    overlay_cached: i32,
    same_cache: i32,
    ops: *const GpuOps,
) -> i32 {
    let Some(ops) = (unsafe { ops.as_ref() }) else {
        return 0;
    };
    if !valid_image(background)
        || !valid_image(overlay)
        || out_width < 1
        || out_height < 1
        || out_width > 3840
        || out_height > 2160
        || (out_width as u64) * (out_height as u64) > 8_000_000
        || overlay.width != out_width
        || overlay.height != out_height
        || same_cache != 0
    {
        return 0;
    }
    let Some((btw, bth)) = (unsafe { texture_size(ops, background) }) else {
        return 0;
    };
    let Some((otw, oth)) = (unsafe { texture_size(ops, overlay) }) else {
        return 0;
    };
    if unsafe { (ops.capture)(ops.context) } == 0 {
        return 0;
    }
    let result = (|| {
        let ok = unsafe { setup_state(ops, out_width as u32, out_height as u32, true) };
        if !unsafe {
            prepare(
                ops,
                0,
                background,
                btw,
                bth,
                background_cached != 0,
                background_revision,
                false,
            )
        } {
            return false;
        }
        if !unsafe {
            prepare(
                ops,
                1,
                overlay,
                otw,
                oth,
                overlay_cached != 0,
                overlay_revision,
                true,
            )
        } {
            return false;
        }
        if !ok
            || unsafe { (ops.bind)(ops.context, 1) } == 0
            || unsafe { (ops.bind)(ops.context, 0) } == 0
        {
            return false;
        }
        if !unsafe {
            draw_quad(
                ops,
                0,
                out_width,
                out_height,
                background,
                btw,
                bth,
                [0xff707070, 0xffff_ffff, 0xff5e5e5e, 0xff5e5e5e],
                ok,
            )
        } {
            return false;
        }
        unsafe {
            draw_quad(
                ops,
                1,
                out_width,
                out_height,
                overlay,
                otw,
                oth,
                [0xffff_ffff; 4],
                ok,
            )
        }
    })();
    unsafe { (ops.restore)(ops.context) };
    result as i32
}
