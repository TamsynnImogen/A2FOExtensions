//! Private synchronous SOD group-material query; no game memory is read here.
use a2fo_native11_model::{GroupMaterialSink, MAX_INPUT};
use core::ffi::c_void;

/// # Safety
/// `data` must be a readable `length`-byte SOD and `sink` must not unwind.
/// The sink receives borrowed fields valid only until it returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_native11_inspect_sod_group_material(
    data: *const u8,
    length: u32,
    node_index: u32,
    group_index: u32,
    sink: Option<GroupMaterialSink>,
    context: *mut c_void,
) -> i32 {
    if data.is_null() || length == 0 || length as usize > MAX_INPUT {
        return 0;
    }
    let Some(sink) = sink else {
        return 0;
    };
    a2fo_native11_model::inspect_sod_group_material(
        unsafe { core::slice::from_raw_parts(data, length as usize) },
        node_index,
        group_index,
        sink,
        context,
    ) as i32
}
