//! Rust-owned PE entry and attachment sequencing. Native operations never unwind.
#![no_std]
#[cfg(feature = "bink-runtime")]
extern crate std;
use core::ffi::c_void;
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("The DLL requires the verified 32-bit Windows GNU ABI");
unsafe extern "C" {
    fn a2fo_native_process_attach_step(operation: u32, instance: *mut c_void) -> u32;
    fn a2fo_native_initialize_worker() -> u32;
    #[cfg(feature = "bink-runtime")]
    fn a2fo_native_bink_early_ready() -> u32;
}
#[link(name = "kernel32")]
unsafe extern "system" {
    fn CreateThread(
        attributes: *mut c_void,
        stack_size: usize,
        start: Option<unsafe extern "system" fn(*mut c_void) -> u32>,
        argument: *mut c_void,
        flags: u32,
        thread_id: *mut u32,
    ) -> *mut c_void;
    fn CloseHandle(handle: *mut c_void) -> i32;
}
unsafe extern "system" fn initialize_worker(_argument: *mut c_void) -> u32 {
    #[cfg(feature = "bink-runtime")]
    if unsafe { a2fo_native_bink_early_ready() } == 0 {
        return 1;
    }
    unsafe { a2fo_native_initialize_worker() }
}
unsafe extern "system" fn dll_main(
    instance: *mut c_void,
    reason: u32,
    _reserved: *mut c_void,
) -> i32 {
    let _ = a2fo_core::runtime_link_anchor();
    #[cfg(feature = "bink-runtime")]
    let _ = a2fo_bink_runtime::ffi::link_anchor();
    if reason != 1 {
        // Preserve process-lifetime teardown; never unload modules under loader lock.
        return 1;
    }
    let step = |operation| unsafe { a2fo_native_process_attach_step(operation, instance) };
    for operation in 0..=5 {
        step(operation);
    }
    if step(6) != 0 {
        step(7);
        step(8);
    }
    step(9);
    // Bink import redirection maps this DLL before FleetOpsHook attaches.
    // The paired startup proxy invokes the native early-attach bridge after
    // FleetOpsHook's LoadLibrary completes, preserving the old hook order.
    #[cfg(not(feature = "bink-runtime"))]
    if step(10) != 0 {
        step(11);
        step(12);
        step(13);
        if step(14) != 0 {
            for operation in 15..=20 {
                step(operation);
            }
        }
    }
    let worker = unsafe {
        CreateThread(
            core::ptr::null_mut(),
            0,
            Some(initialize_worker),
            core::ptr::null_mut(),
            0,
            core::ptr::null_mut(),
        )
    };
    if worker.is_null() {
        step(21);
    } else {
        unsafe { CloseHandle(worker) };
    }
    1
}
// CRT-visible ABI label, separate from the public DLL export table.
core::arch::global_asm!(".globl _DllMain@12", "_DllMain@12:", "jmp {}", sym dll_main);
