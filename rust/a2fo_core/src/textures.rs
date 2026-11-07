//! DDS and D3D8 texture byte operations. Native containers stay with the caller.
use core::slice;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Mip {
    width: u32,
    height: u32,
    row_pitch: u32,
    slice_pitch: u32,
    offset: usize,
}
#[repr(C)]
pub struct DdsPlan {
    format: u32,
    width: u32,
    height: u32,
    alpha: u32,
    mip_count: u32,
    error: u32,
    payload_offset: usize,
    payload_size: usize,
    mips: [Mip; 15],
}
#[repr(C)]
#[derive(Default)]
pub struct UploadPlan {
    format: u32,
    width: u32,
    height: u32,
    row_pitch: u32,
    output_size: usize,
    required_source: usize,
}

const MAX_DIM: u32 = 16384;
const fn cc(bytes: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*bytes)
}
fn read32(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes([b[at], b[at + 1], b[at + 2], b[at + 3]])
}
fn block_bytes(format: u32) -> u32 {
    match format {
        0x47 | 0x48 | 0x50 | 0x51 => 8,
        0x4a | 0x4b | 0x4d | 0x4e | 0x53 | 0x54 | 0x62 | 0x63 => 16,
        _ => 0,
    }
}
fn layout(format: u32, width: u32, height: u32) -> Option<(u32, u32)> {
    let block = block_bytes(format);
    if block == 0 || width == 0 || height == 0 || width > MAX_DIM || height > MAX_DIM {
        return None;
    }
    let row = (width as u64).div_ceil(4) * block as u64;
    let slice = row * (height as u64).div_ceil(4);
    (row <= u32::MAX as u64 && slice <= u32::MAX as u64).then_some((row as u32, slice as u32))
}
fn chain(mut w: u32, mut h: u32) -> u32 {
    let mut count = 1;
    while w > 1 || h > 1 {
        w = (w / 2).max(1);
        h = (h / 2).max(1);
        count += 1;
    }
    count
}
fn legacy(code: u32) -> (u32, u32) {
    match code {
        x if x == cc(b"DXT1") => (0x47, 0),
        x if x == cc(b"DXT2") => (0x4a, 2),
        x if x == cc(b"DXT3") => (0x4a, 0),
        x if x == cc(b"DXT4") => (0x4d, 2),
        x if x == cc(b"DXT5") => (0x4d, 0),
        x if x == cc(b"ATI1") || x == cc(b"BC4U") => (0x50, 0),
        x if x == cc(b"BC4S") => (0x51, 0),
        x if x == cc(b"ATI2") || x == cc(b"BC5U") => (0x53, 0),
        x if x == cc(b"BC5S") => (0x54, 0),
        _ => (0, 0),
    }
}
// Error IDs preserve the public C++ diagnostics, including their priority.
fn parse(b: &[u8], p: &mut DdsPlan) -> u32 {
    macro_rules! fail {
        ($status:expr, $error:expr) => {{
            p.error = $error;
            return $status;
        }};
    }
    if b.len() < 128 {
        fail!(1, 1);
    }
    if read32(b, 0) != cc(b"DDS ") {
        fail!(1, 2);
    }
    if read32(b, 4) != 124 || read32(b, 76) != 32 {
        fail!(1, 3);
    }
    let flags = read32(b, 8);
    let h = read32(b, 12);
    let w = read32(b, 16);
    let depth = read32(b, 24);
    let declared = read32(b, 28);
    let pixel_flags = read32(b, 80);
    let code = read32(b, 84);
    if flags & 2 == 0 || flags & 4 == 0 || w == 0 || h == 0 || w > MAX_DIM || h > MAX_DIM {
        fail!(1, 4);
    }
    if pixel_flags & 4 == 0 {
        fail!(2, 5);
    }
    if pixel_flags & (2 | 0x40 | 0x200 | 0x20000) != 0 {
        fail!(1, 6);
    }
    if flags & 0x800000 != 0 || depth > 1 || read32(b, 112) & (0xfe00 | 0x200000) != 0 {
        fail!(2, 7);
    }
    let count = declared.max(1);
    if count > chain(w, h) {
        fail!(1, 8);
    }
    let (format, alpha, payload_at) = if code == cc(b"DX10") {
        if b.len() < 148 {
            fail!(1, 9);
        }
        let format = read32(b, 128);
        let dimension = read32(b, 132);
        let misc = read32(b, 136);
        let array = read32(b, 140);
        let alpha = read32(b, 144);
        if dimension != 3 || array != 1 || misc & 4 != 0 {
            fail!(2, 10);
        }
        if misc != 0 || alpha & !7 != 0 || alpha & 7 > 4 {
            fail!(1, 11);
        }
        (format, alpha, 148)
    } else {
        let (format, alpha) = legacy(code);
        (format, alpha, 128)
    };
    if block_bytes(format) == 0 {
        fail!(2, 12);
    }
    let mut mw = w;
    let mut mh = h;
    let mut used = 0usize;
    for i in 0..count as usize {
        let Some((row, bytes)) = layout(format, mw, mh) else {
            fail!(1, 13);
        };
        let Some(next) = used.checked_add(bytes as usize) else {
            fail!(1, 14);
        };
        if next > b.len() - payload_at {
            fail!(1, 14);
        }
        p.mips[i] = Mip {
            width: mw,
            height: mh,
            row_pitch: row,
            slice_pitch: bytes,
            offset: used,
        };
        used = next;
        mw = (mw / 2).max(1);
        mh = (mh / 2).max(1);
    }
    p.format = format;
    p.width = w;
    p.height = h;
    p.alpha = alpha;
    p.mip_count = count;
    p.payload_offset = payload_at;
    p.payload_size = used;
    p.error = 0;
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_textures_layout(
    format: u32,
    width: u32,
    height: u32,
    row: *mut u32,
    slice: *mut u32,
) -> u8 {
    let Some((r, s)) = layout(format, width, height) else {
        return 0;
    };
    if row.is_null() || slice.is_null() {
        return 0;
    }
    unsafe {
        *row = r;
        *slice = s;
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_textures_parse(
    data: *const u8,
    size: usize,
    plan: *mut DdsPlan,
) -> u32 {
    if plan.is_null() {
        return 1;
    }
    let p = unsafe { &mut *plan };
    if data.is_null() {
        p.error = 1;
        return 1;
    }
    if size > isize::MAX as usize {
        p.error = 1;
        return 1;
    }
    parse(unsafe { slice::from_raw_parts(data, size) }, p)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_textures_validate(
    format: u32,
    width: u32,
    height: u32,
    alpha: u32,
    mips: *const Mip,
    count: usize,
    payload: usize,
    error: *mut u32,
) -> u32 {
    if error.is_null() {
        return 1;
    }
    macro_rules! fail {
        ($status:expr, $id:expr) => {{
            unsafe {
                *error = $id;
            }
            return $status;
        }};
    }
    if block_bytes(format) == 0 {
        fail!(2, 15);
    }
    if width == 0 || height == 0 || width > MAX_DIM || height > MAX_DIM {
        fail!(1, 16);
    }
    if alpha > 4 {
        fail!(1, 17);
    }
    if count == 0
        || count > chain(width, height) as usize
        || mips.is_null()
        || count > isize::MAX as usize / core::mem::size_of::<Mip>()
    {
        fail!(1, 18);
    }
    let ms = unsafe { slice::from_raw_parts(mips, count) };
    let mut w = width;
    let mut h = height;
    let mut next = 0usize;
    for m in ms {
        let Some((row, size)) = layout(format, w, h) else {
            fail!(1, 19);
        };
        if m.width != w
            || m.height != h
            || m.row_pitch != row
            || m.slice_pitch != size
            || m.offset != next
        {
            fail!(1, 19);
        }
        if size as usize > payload.saturating_sub(next) {
            fail!(1, 20);
        }
        next += size as usize;
        w = (w / 2).max(1);
        h = (h / 2).max(1);
    }
    if next != payload {
        fail!(1, 21);
    }
    unsafe {
        *error = 0;
    }
    0
}

fn upload_plan(
    format: u32,
    width: u32,
    height: u32,
    source_size: usize,
    pitch: u32,
) -> Option<UploadPlan> {
    if width == 0 || height == 0 || width > MAX_DIM || height > MAX_DIM {
        return None;
    }
    let (target, rows, input_row, output_row) = match format {
        21 | 22 => (28, height as usize, width as usize * 4, width as usize * 4),
        20 => (28, height as usize, width as usize * 3, width as usize * 4),
        23 | 24 | 25 | 26 | 51 => (28, height as usize, width as usize * 2, width as usize * 4),
        28 | 50 => (28, height as usize, width as usize, width as usize * 4),
        f if f == cc(b"DXT1")
            || f == cc(b"DXT2")
            || f == cc(b"DXT3")
            || f == cc(b"DXT4")
            || f == cc(b"DXT5") =>
        {
            let block = if f == cc(b"DXT1") { 8 } else { 16 };
            let target = if block == 8 {
                0x47
            } else if f == cc(b"DXT2") || f == cc(b"DXT3") {
                0x4a
            } else {
                0x4d
            };
            let row = (width as usize).div_ceil(4) * block;
            (target, (height as usize).div_ceil(4), row, row)
        }
        _ => return None,
    };
    if (pitch as usize) < input_row || output_row > u32::MAX as usize {
        return None;
    }
    let required = (rows - 1)
        .checked_mul(pitch as usize)?
        .checked_add(input_row)?;
    let total = rows.checked_mul(output_row)?;
    if source_size < required {
        return None;
    }
    Some(UploadPlan {
        format: target,
        width,
        height,
        row_pitch: output_row as u32,
        output_size: total,
        required_source: required,
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_textures_upload_plan(
    format: u32,
    width: u32,
    height: u32,
    source_size: usize,
    pitch: u32,
    plan: *mut UploadPlan,
) -> u8 {
    if plan.is_null() {
        return 0;
    }
    let Some(p) = upload_plan(format, width, height, source_size, pitch) else {
        return 0;
    };
    unsafe {
        *plan = p;
    }
    1
}
fn normalize(value: u32, max: u32) -> u8 {
    ((value * 255 + max / 2) / max) as u8
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_textures_upload_copy(
    format: u32,
    width: u32,
    height: u32,
    source: *const u8,
    source_size: usize,
    pitch: u32,
    output: *mut u8,
    output_size: usize,
) -> u8 {
    if source.is_null()
        || output.is_null()
        || source_size > isize::MAX as usize
        || output_size > isize::MAX as usize
    {
        return 0;
    }
    let Some(p) = upload_plan(format, width, height, source_size, pitch) else {
        return 0;
    };
    if p.output_size != output_size {
        return 0;
    }
    let src = unsafe { slice::from_raw_parts(source, source_size) };
    let dst = unsafe { slice::from_raw_parts_mut(output, output_size) };
    let rows = if p.format == 28 {
        height as usize
    } else {
        (height as usize).div_ceil(4)
    };
    let output_pitch = p.row_pitch as usize;
    for row in 0..rows {
        let from = &src[row * pitch as usize..];
        let to = &mut dst[row * output_pitch..(row + 1) * output_pitch];
        if p.format != 28 {
            to.copy_from_slice(&from[..output_pitch]);
            continue;
        }
        for i in 0..width as usize {
            let rgba = match format {
                21 | 22 => [
                    from[i * 4 + 2],
                    from[i * 4 + 1],
                    from[i * 4],
                    if format == 21 { from[i * 4 + 3] } else { 255 },
                ],
                20 => [from[i * 3 + 2], from[i * 3 + 1], from[i * 3], 255],
                28 => [255, 255, 255, from[i]],
                51 => [from[i * 2], from[i * 2], from[i * 2], from[i * 2 + 1]],
                23..=26 => {
                    let v = u16::from_le_bytes([from[i * 2], from[i * 2 + 1]]) as u32;
                    if format == 26 {
                        [
                            normalize((v >> 8) & 15, 15),
                            normalize((v >> 4) & 15, 15),
                            normalize(v & 15, 15),
                            normalize(v >> 12, 15),
                        ]
                    } else if format == 23 {
                        [
                            normalize((v >> 11) & 31, 31),
                            normalize((v >> 5) & 63, 63),
                            normalize(v & 31, 31),
                            255,
                        ]
                    } else {
                        [
                            normalize((v >> 10) & 31, 31),
                            normalize((v >> 5) & 31, 31),
                            normalize(v & 31, 31),
                            if format == 24 || v & 0x8000 != 0 {
                                255
                            } else {
                                0
                            },
                        ]
                    }
                }
                _ => [from[i], from[i], from[i], 255],
            };
            to[i * 4..i * 4 + 4].copy_from_slice(&rgba);
        }
    }
    1
}
