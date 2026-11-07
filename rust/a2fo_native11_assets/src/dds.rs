//! Borrowed, CPU-only DDS mip view for the native11 renderer.
use std::ffi::c_void;
use std::panic::{AssertUnwindSafe, catch_unwind};

const MAX_BYTES: usize = 256 * 1024 * 1024;
const MAX_DIM: u32 = 16384;
const MAX_MIPS: usize = 15;

#[repr(C)]
#[derive(Clone, Copy, Default, Debug)]
pub struct TextureMip {
    pub width: u32,
    pub height: u32,
    pub row_pitch: u32,
    pub slice_pitch: u32,
    pub offset: u64,
    pub byte_count: u64,
}

#[repr(C)]
pub struct Texture {
    pub format: u32,
    pub alpha_mode: u32,
    pub width: u32,
    pub height: u32,
    pub mip_count: u32,
    pub reserved: u32,
    pub payload_bytes: u64,
    pub payload: *const u8,
    pub mips: *const TextureMip,
}

pub type TextureSink = unsafe extern "C" fn(*mut c_void, *const Texture) -> i32;

fn word(data: &[u8], at: usize) -> Option<u32> {
    let bytes: [u8; 4] = data.get(at..at + 4)?.try_into().ok()?;
    Some(u32::from_le_bytes(bytes))
}

fn fourcc(bytes: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*bytes)
}

// Format protocol IDs are declared alongside the C struct in assets.h.
fn dxgi_format(value: u32) -> Option<u32> {
    Some(match value {
        28 => 2,
        29 => 4,
        87 => 1,
        91 => 3,
        71 => 5,
        72 => 6,
        74 => 7,
        75 => 8,
        77 => 9,
        78 => 10,
        80 => 11,
        81 => 12,
        83 => 13,
        84 => 14,
        98 => 15,
        99 => 16,
        _ => return None,
    })
}

fn legacy_format(value: u32) -> Option<(u32, u32)> {
    Some(match value {
        x if x == fourcc(b"DXT1") => (5, 0),
        x if x == fourcc(b"DXT2") => (7, 2),
        x if x == fourcc(b"DXT3") => (7, 0),
        x if x == fourcc(b"DXT4") => (9, 2),
        x if x == fourcc(b"DXT5") => (9, 0),
        x if x == fourcc(b"ATI1") || x == fourcc(b"BC4U") => (11, 0),
        x if x == fourcc(b"BC4S") => (12, 0),
        x if x == fourcc(b"ATI2") || x == fourcc(b"BC5U") => (13, 0),
        x if x == fourcc(b"BC5S") => (14, 0),
        _ => return None,
    })
}

fn raw_format(data: &[u8]) -> Option<(u32, u32)> {
    if word(data, 88)? != 32 || word(data, 96)? != 0x0000_ff00 {
        return None;
    }
    let alpha = match word(data, 104)? {
        0 => 3,
        0xff00_0000 => 0,
        _ => return None,
    };
    let format = match (word(data, 92)?, word(data, 100)?) {
        (0x00ff_0000, 0x0000_00ff) => 1,
        (0x0000_00ff, 0x00ff_0000) => 2,
        _ => return None,
    };
    Some((format, alpha))
}

fn mip_layout(format: u32, width: u32, height: u32) -> Option<(u32, u32)> {
    let (row, rows) = match format {
        1..=4 => (u64::from(width) * 4, u64::from(height)),
        5 | 6 | 11 | 12 => (
            u64::from(width.div_ceil(4)) * 8,
            u64::from(height.div_ceil(4)),
        ),
        7..=10 | 13..=16 => (
            u64::from(width.div_ceil(4)) * 16,
            u64::from(height.div_ceil(4)),
        ),
        _ => return None,
    };
    let slice = row.checked_mul(rows)?;
    Some((u32::try_from(row).ok()?, u32::try_from(slice).ok()?))
}

fn chain(mut width: u32, mut height: u32) -> u32 {
    let mut count = 1;
    while width > 1 || height > 1 {
        width = (width / 2).max(1);
        height = (height / 2).max(1);
        count += 1;
    }
    count
}

fn parse(data: &[u8], sink: TextureSink, context: *mut c_void) -> bool {
    if data.len() < 128
        || data.get(..4) != Some(b"DDS ")
        || word(data, 4) != Some(124)
        || word(data, 76) != Some(32)
    {
        return false;
    }
    let flags = word(data, 8).unwrap();
    let height = word(data, 12).unwrap();
    let width = word(data, 16).unwrap();
    let depth = word(data, 24).unwrap();
    let count = word(data, 28).unwrap().max(1);
    let pixel_flags = word(data, 80).unwrap();
    let caps2 = word(data, 112).unwrap();
    if flags & (0x2 | 0x4) != (0x2 | 0x4)
        || width == 0
        || height == 0
        || width > MAX_DIM
        || height > MAX_DIM
        || count > chain(width, height)
        || count as usize > MAX_MIPS
        || flags & 0x800000 != 0
        || depth > 1
        || caps2 & (0xfe00 | 0x200000) != 0
    {
        return false;
    }
    let code = word(data, 84).unwrap();
    let (format, alpha_mode, payload_at) = if pixel_flags & 4 != 0 {
        if pixel_flags & (0x2 | 0x40 | 0x200 | 0x20000) != 0 {
            return false;
        }
        if code == fourcc(b"DX10") {
            if data.len() < 148 {
                return false;
            }
            let dxgi = word(data, 128).unwrap();
            let dimension = word(data, 132).unwrap();
            let misc = word(data, 136).unwrap();
            let array = word(data, 140).unwrap();
            let alpha = word(data, 144).unwrap();
            if dimension != 3 || misc != 0 || array != 1 || alpha > 4 {
                return false;
            }
            let Some(format) = dxgi_format(dxgi) else {
                return false;
            };
            (format, alpha, 148)
        } else {
            let Some((format, alpha)) = legacy_format(code) else {
                return false;
            };
            (format, alpha, 128)
        }
    } else {
        if pixel_flags & 0x40 == 0
            || pixel_flags & !(0x40 | 0x1) != 0
            || code == fourcc(b"DX10")
            || legacy_format(code).is_some()
        {
            return false;
        }
        // The callback describes tightly packed rows. An authored pitch hint
        // that says otherwise cannot be represented by this canonical view.
        if flags & 0x8 != 0 && word(data, 20) != width.checked_mul(4) {
            return false;
        }
        let Some((format, alpha)) = raw_format(data) else {
            return false;
        };
        (format, alpha, 128)
    };
    let mut mips = [TextureMip::default(); MAX_MIPS];
    let mut offset = 0usize;
    let mut mip_width = width;
    let mut mip_height = height;
    for mip in &mut mips[..count as usize] {
        let Some((row_pitch, slice_pitch)) = mip_layout(format, mip_width, mip_height) else {
            return false;
        };
        let Some(end) = offset.checked_add(slice_pitch as usize) else {
            return false;
        };
        if end > MAX_BYTES || end > data.len() - payload_at {
            return false;
        }
        *mip = TextureMip {
            width: mip_width,
            height: mip_height,
            row_pitch,
            slice_pitch,
            offset: offset as u64,
            byte_count: u64::from(slice_pitch),
        };
        offset = end;
        mip_width = (mip_width / 2).max(1);
        mip_height = (mip_height / 2).max(1);
    }
    let view = Texture {
        format,
        alpha_mode,
        width,
        height,
        mip_count: count,
        reserved: 0,
        payload_bytes: offset as u64,
        payload: data[payload_at..].as_ptr(),
        mips: mips.as_ptr(),
    };
    unsafe { sink(context, &view) != 0 }
}

struct TraceSink {
    sink: TextureSink,
    context: *mut c_void,
    visited: bool,
}

unsafe extern "C" fn trace_sink(context: *mut c_void, texture: *const Texture) -> i32 {
    let trace = unsafe { &mut *(context as *mut TraceSink) };
    trace.visited = true;
    unsafe { (trace.sink)(trace.context, texture) }
}

unsafe extern "C" fn image_as_texture(
    context: *mut c_void,
    pixels: *const u8,
    width: u32,
    height: u32,
    pitch: u32,
    byte_count: u64,
) -> i32 {
    let trace = unsafe { &mut *(context as *mut TraceSink) };
    let mip = TextureMip {
        width,
        height,
        row_pitch: pitch,
        slice_pitch: byte_count as u32,
        offset: 0,
        byte_count,
    };
    let texture = Texture {
        format: 1,
        alpha_mode: 0,
        width,
        height,
        mip_count: 1,
        reserved: 0,
        payload_bytes: byte_count,
        payload: pixels,
        mips: &mip,
    };
    trace.visited = true;
    unsafe { (trace.sink)(trace.context, &texture) }
}

fn legacy_raw_single_mip(data: &[u8]) -> bool {
    if data.len() < 128
        || data.get(..4) != Some(b"DDS ")
        || word(data, 4) != Some(124)
        || word(data, 76) != Some(32)
    {
        return false;
    }
    let flags = word(data, 8).unwrap();
    let pixel_flags = word(data, 80).unwrap();
    let code = word(data, 84).unwrap();
    flags & 0x1007 == 0x1007
        && flags & 0x800000 == 0
        && word(data, 24).unwrap() <= 1
        && word(data, 112) == Some(0)
        && pixel_flags & 0x40 != 0
        && pixel_flags & 0x4 == 0
        && code != fourcc(b"DX10")
        && legacy_format(code).is_none()
        && word(data, 28).unwrap() <= 1
        && matches!(word(data, 88), Some(16 | 24 | 32))
}

/// # Safety
/// `data` points to `length` readable bytes. `sink` obeys its C ABI and may
/// borrow payload and mip descriptors only during the synchronous callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_native11_parse_texture(
    data: *const u8,
    length: u32,
    sink: Option<TextureSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || sink.is_none() || length == 0 || length as usize > MAX_BYTES {
        return 0;
    }
    catch_unwind(AssertUnwindSafe(|| {
        let bytes = unsafe { std::slice::from_raw_parts(data, length as usize) };
        let mut trace = TraceSink {
            sink: sink.unwrap(),
            context,
            visited: false,
        };
        let trace_ptr = &mut trace as *mut TraceSink as *mut c_void;
        if bytes.starts_with(b"DDS ") {
            if parse(bytes, trace_sink, trace_ptr) {
                return true;
            }
            if trace.visited || bytes.len() > super::MAX_INPUT || !legacy_raw_single_mip(bytes) {
                return false;
            }
        } else if bytes.len() > super::MAX_INPUT {
            return false;
        }
        super::decode_image(bytes, image_as_texture, trace_ptr)
    }))
    .unwrap_or(false) as i32
}

/// # Safety
/// `data` points to `length` readable bytes. `sink` obeys its C ABI and may
/// borrow the payload and mip descriptors only during this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_native11_parse_dds(
    data: *const u8,
    length: u32,
    sink: Option<TextureSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || sink.is_none() || length == 0 || length as usize > MAX_BYTES {
        return 0;
    }
    catch_unwind(AssertUnwindSafe(|| {
        parse(
            unsafe { std::slice::from_raw_parts(data, length as usize) },
            sink.unwrap(),
            context,
        )
    }))
    .unwrap_or(false) as i32
}
