//! A1 shutdown phase order. Native callbacks retain Win32 vtable restoration,
//! background ownership, storage clears and ABI-specific cleanup primitives.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_shutdown(ctx: *mut c_void, op: Op) {
    // Each operation is synchronous. A native exception sentinel stops before
    // the next phase; the C++ entrypoint rethrows after this call returns.
    for id in 0..=13 {
        if unsafe { op(ctx, id) } == u64::MAX {
            return;
        }
    }
}
