//! CPU-only, synchronous adapter for renderer/native11/assets.h.
//! This first slice uses authored base transforms and the highest-detail LOD.
//! Animation, sprite, emitter, and light playback is omitted by static preview
//! policy. Bump/height maps are omitted in this diffuse preview. SOD materials
//! contain RGB, not alpha: emitted alpha is 1.0. The pristine static preview
//! excludes the standard null-node `damage` subtree (including Borg overlays).

use std::ffi::c_void;
use std::panic::{AssertUnwindSafe, catch_unwind};

mod dds;
pub use dds::{
    Texture, TextureMip, TextureSink, a2fo_native11_parse_dds, a2fo_native11_parse_texture,
};

const MAX_INPUT: usize = 64 * 1024 * 1024;
const MAX_IMAGE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_DIMENSION: u32 = 8192;
pub use a2fo_native11_model::{Vertex,Part,PartSink,PartSource,DetailedPartSink,TransformPositions};
use a2fo_native11_model::{parse_sod, parse_sod_all, parse_sod_all_with_positions,parse_sod_detailed_all};
pub type ImageSink = unsafe extern "C" fn(*mut c_void, *const u8, u32, u32, u32, u64) -> i32;

fn decode_image(data: &[u8], sink: ImageSink, context: *mut c_void) -> bool {
    let (width, height) = if data.starts_with(b"DDS ") {
        let Ok(dim) = a2fmt::dds::dimensions(data) else {
            return false;
        };
        dim
    } else {
        let Ok(header) = a2fmt::tga::Header::parse(data) else {
            return false;
        };
        (u32::from(header.width), u32::from(header.height))
    };
    let Some(byte_count) = u64::from(width)
        .checked_mul(u64::from(height))
        .and_then(|v| v.checked_mul(4))
    else {
        return false;
    };
    if width == 0
        || height == 0
        || width > MAX_DIMENSION
        || height > MAX_DIMENSION
        || byte_count > MAX_IMAGE_BYTES
    {
        return false;
    }
    let decoded = if data.starts_with(b"DDS ") {
        a2fmt::dds::decode(data)
    } else {
        a2fmt::tga::decode(data).map_err(|_| a2fmt::dds::Error::InvalidHeader)
    };
    let Ok(image) = decoded else {
        return false;
    };
    let Some(pitch) = width.checked_mul(4) else {
        return false;
    };
    if image.width != width || image.height != height || image.pixels.len() as u64 != byte_count {
        return false;
    }
    unsafe {
        sink(
            context,
            image.pixels.as_ptr(),
            width,
            height,
            pitch,
            byte_count,
        ) != 0
    }
}

/// # Safety
/// `data` must point to `length` readable bytes and `sink` must obey its C ABI.
/// The input is bounded and Rust panics are caught; process OOM and invalid
/// caller pointers are outside this guarantee.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_native11_parse_sod(
    data: *const u8,
    length: u32,
    sink: Option<PartSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || sink.is_none() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    catch_unwind(AssertUnwindSafe(|| {
        parse_sod(
            unsafe { std::slice::from_raw_parts(data, length as usize) },
            sink.unwrap(),
            context,
        )
    }))
    .unwrap_or(false) as i32
}

/// # Safety
/// `data` must point to `length` readable bytes and `sink` must obey its C ABI.
/// The callback receives borrowed data only for its synchronous invocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_native11_parse_sod_all(
    data: *const u8,
    length: u32,
    sink: Option<PartSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || sink.is_none() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    catch_unwind(AssertUnwindSafe(|| {
        parse_sod_all(
            unsafe { std::slice::from_raw_parts(data, length as usize) },
            sink.unwrap(),
            context,
        )
    }))
    .unwrap_or(false) as i32
}

/// # Safety
/// `data` must point to `length` readable bytes and `sink` must obey its C ABI.
/// The detailed source and part storage are borrowed only during the callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_native11_inspect_sod_all(
    data: *const u8,
    length: u32,
    sink: Option<DetailedPartSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || sink.is_none() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    catch_unwind(AssertUnwindSafe(|| {
        parse_sod_detailed_all(
            unsafe { std::slice::from_raw_parts(data, length as usize) },
            sink.unwrap(),
            context,
        )
    }))
    .unwrap_or(false) as i32
}

/// # Safety
/// `data` must point to `length` readable bytes. Both callbacks receive only
/// synchronous borrowed storage; the position count is contiguous XYZ triplets.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_native11_parse_sod_native_geometry(
    data: *const u8,
    length: u32,
    transform: Option<TransformPositions>,
    transform_context: *mut c_void,
    sink: Option<PartSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null()
        || transform.is_none()
        || sink.is_none()
        || length == 0
        || length as usize > MAX_INPUT
    {
        return 0;
    }
    catch_unwind(AssertUnwindSafe(|| {
        let bytes = unsafe { std::slice::from_raw_parts(data, length as usize) };
        parse_sod_all_with_positions(
            bytes,
            transform.unwrap(),
            transform_context,
            sink.unwrap(),
            context,
        )
    }))
    .unwrap_or(false) as i32
}

/// # Safety
/// `data` must point to `length` readable bytes and `sink` must obey its C ABI.
/// The input is bounded and Rust panics are caught; process OOM and invalid
/// caller pointers are outside this guarantee.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_native11_decode_image(
    data: *const u8,
    length: u32,
    sink: Option<ImageSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || sink.is_none() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    catch_unwind(AssertUnwindSafe(|| {
        decode_image(
            unsafe { std::slice::from_raw_parts(data, length as usize) },
            sink.unwrap(),
            context,
        )
    }))
    .unwrap_or(false) as i32
}

#[cfg(test)]
mod native_geometry_tests {
    use super::*;

    unsafe extern "C" fn sink(_: *mut c_void, _: *const Part) -> i32 {
        1
    }

    #[test]
    fn native_geometry_requires_transform_policy() {
        let bytes = b"Storm3D_SW";
        assert_eq!(
            unsafe {
                a2fo_native11_parse_sod_native_geometry(
                    bytes.as_ptr(),
                    bytes.len() as u32,
                    None,
                    core::ptr::null_mut(),
                    Some(sink),
                    core::ptr::null_mut(),
                )
            },
            0
        );
    }
}
