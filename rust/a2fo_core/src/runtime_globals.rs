//! Process-lifetime lifecycle storage owned by Rust. The native ABI uses
//! Win32 interlocked operations on the aligned 32-bit cells; no Rust reference
//! is retained while native code accesses the storage.
use core::ffi::c_void;

#[repr(C)]
pub struct RuntimeGlobals {
    pub initialize_state: i32,
    pub deferred_finished: i32,
    pub initialize_thread_id: u32,
    pub log: *mut c_void,
}

static mut GLOBALS: RuntimeGlobals = RuntimeGlobals {
    initialize_state: 0,
    deferred_finished: 0,
    initialize_thread_id: 0,
    log: usize::MAX as *mut c_void,
};

/// Borrows process-lifetime storage. Native callers must preserve the existing
/// initialization serialization and use interlocked operations on state cells.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_globals() -> *mut RuntimeGlobals {
    &raw mut GLOBALS
}

#[cfg(all(target_os = "windows", target_arch = "x86"))]
const _: () = {
    assert!(core::mem::size_of::<RuntimeGlobals>() == 16);
    assert!(core::mem::align_of::<RuntimeGlobals>() == 4);
    assert!(core::mem::offset_of!(RuntimeGlobals, log) == 12);
};

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn lifecycle_cells_are_c_layout_and_aligned() {
        assert_eq!(core::mem::offset_of!(RuntimeGlobals, initialize_state), 0);
        assert_eq!(core::mem::offset_of!(RuntimeGlobals, deferred_finished), 4);
        assert_eq!(
            core::mem::offset_of!(RuntimeGlobals, initialize_thread_id),
            8
        );
        let pointer = a2fo_rs_runtime_globals();
        assert_eq!(
            (pointer as usize) % core::mem::align_of::<RuntimeGlobals>(),
            0
        );
        assert_eq!(pointer, a2fo_rs_runtime_globals());
    }
}
