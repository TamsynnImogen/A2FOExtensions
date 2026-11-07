//! Private CPU parser adapter; the existing core CRT allocator/panic policy owns it.
use a2fo_native11_model::{DetailedPartSink, MAX_INPUT, PartSink, PoseSink};
use core::ffi::c_void;
/// # Safety
/// Readable complete model bytes and a synchronous non-unwinding C sink are required.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_native11_parse_sod(
    data: *const u8,
    length: u32,
    sink: Option<PartSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    let Some(sink) = sink else {
        return 0;
    };
    a2fo_native11_model::parse_sod(
        unsafe { core::slice::from_raw_parts(data, length as usize) },
        sink,
        context,
    ) as i32
}

/// # Safety
/// Readable complete model bytes and a synchronous non-unwinding C sink are required.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_native11_inspect_sod(
    data: *const u8,
    length: u32,
    sink: Option<DetailedPartSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    let Some(sink) = sink else {
        return 0;
    };
    a2fo_native11_model::parse_sod_detailed(
        unsafe { core::slice::from_raw_parts(data, length as usize) },
        sink,
        context,
    ) as i32
}

/// # Safety
/// Readable complete model bytes and a synchronous non-unwinding C sink are required.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_native11_inspect_sod_all(
    data: *const u8,
    length: u32,
    sink: Option<DetailedPartSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    let Some(sink) = sink else {
        return 0;
    };
    a2fo_native11_model::parse_sod_detailed_all(
        unsafe { core::slice::from_raw_parts(data, length as usize) },
        sink,
        context,
    ) as i32
}

/// # Safety
/// Readable complete model bytes and a synchronous non-unwinding C sink are required.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_native11_inspect_sod_pose(
    data: *const u8,
    length: u32,
    sink: Option<PoseSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || length == 0 || length as usize > MAX_INPUT { return 0; }
    let Some(sink) = sink else { return 0; };
    a2fo_native11_model::inspect_sod_pose(
        unsafe { core::slice::from_raw_parts(data, length as usize) }, sink, context,
    ) as i32
}
