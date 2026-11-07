//! Checked patch primitives. Callers retain instruction-boundary/site validation;
//! The Windows backend below owns protection, allocation and cache operations.
use core::{ffi::c_void, ptr, slice};
#[repr(C)]
pub struct Memory {
    protect: unsafe extern "C" fn(*mut c_void, usize, u32, *mut u32) -> i32,
    allocate: unsafe extern "C" fn(usize) -> *mut c_void,
    free_memory: unsafe extern "C" fn(*mut c_void),
    flush: unsafe extern "C" fn(*mut c_void, usize),
}
#[repr(C)]
pub struct Hook {
    target: *mut c_void,
    gateway: *mut c_void,
    length: usize,
}
unsafe fn branch(
    source: *mut c_void,
    destination: *const c_void,
    n: usize,
    opcode: u8,
    api: &Memory,
) -> bool {
    if n < 5 {
        return false;
    }
    let from = source as isize;
    let to = destination as isize;
    // The original target-width intptr_t subtraction wraps on i686. On a
    // 64-bit host the explicit range check still rejects non-near branches.
    let relative = to.wrapping_sub(from.wrapping_add(5));
    if relative < i32::MIN as isize || relative > i32::MAX as isize {
        return false;
    }
    let mut old = 0;
    if unsafe { (api.protect)(source, n, 0x40, &mut old) } == 0 {
        return false;
    }
    // SAFETY: caller validated writable target extent; protection succeeded.
    // Branch sites contain whole instructions and no relocatable relative operands.
    unsafe {
        let p = source.cast::<u8>();
        p.write(opcode);
        ptr::copy_nonoverlapping((relative as i32).to_le_bytes().as_ptr(), p.add(1), 4);
        ptr::write_bytes(p.add(5), 0x90, n - 5);
        (api.flush)(source, n);
        let mut ignored = 0;
        (api.protect)(source, n, old, &mut ignored);
    }
    true
}
unsafe fn expected_matches(target: *const c_void, expected: *const u8, n: usize) -> bool {
    if target.is_null() || expected.is_null() || n > isize::MAX as usize {
        return false;
    }
    // SAFETY: the internal host contract supplies two live readable n-byte spans.
    unsafe { slice::from_raw_parts(target.cast::<u8>(), n) == slice::from_raw_parts(expected, n) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hook_patch(
    target: *mut c_void,
    replacement: *const c_void,
    expected: *const u8,
    n: usize,
    kind: u32,
    api: *const Memory,
) -> u8 {
    if replacement.is_null()
        || api.is_null()
        || (kind == 1 && n < 5)
        || (kind == 2 && n == 0)
        || !unsafe { expected_matches(target, expected, n) }
    {
        return 0;
    }
    let api = unsafe { &*api };
    if kind <= 1 {
        return u8::from(unsafe {
            branch(
                target,
                replacement,
                n,
                if kind == 0 { 0xe9 } else { 0xe8 },
                api,
            )
        });
    }
    if kind != 2 {
        return 0;
    }
    let mut old = 0;
    if unsafe { (api.protect)(target, n, 0x40, &mut old) } == 0 {
        return 0;
    }
    // SAFETY: native caller provides nonoverlapping buffers, as required by the
    // original memcpy; first protection succeeded. Restore/flush results ignored
    // deliberately, preserving the original contract rather than adding rollback.
    unsafe {
        ptr::copy_nonoverlapping(replacement.cast::<u8>(), target.cast::<u8>(), n);
        (api.flush)(target, n);
        let mut ignored = 0;
        (api.protect)(target, n, old, &mut ignored);
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hook_install(
    target: *mut c_void,
    replacement: *mut c_void,
    n: usize,
    expected: *const u8,
    out: *mut Hook,
    api: *const Memory,
) -> u8 {
    if out.is_null()
        || api.is_null()
        || replacement.is_null()
        || n < 5
        || !unsafe { expected_matches(target, expected, n) }
    {
        return 0;
    }
    let Some(allocation) = n.checked_add(5) else {
        return 0;
    };
    let api = unsafe { &*api };
    let gateway = unsafe { (api.allocate)(allocation) };
    if gateway.is_null() {
        return 0;
    }
    // SAFETY: callback allocated allocation bytes, source extent was validated.
    unsafe {
        ptr::copy_nonoverlapping(target.cast::<u8>(), gateway.cast::<u8>(), n);
    }
    let back = unsafe {
        branch(
            gateway.cast::<u8>().wrapping_add(n).cast(),
            target.cast::<u8>().wrapping_add(n).cast(),
            5,
            0xe9,
            api,
        )
    };
    if !back || !unsafe { branch(target, replacement, n, 0xe9, api) } {
        unsafe { (api.free_memory)(gateway) };
        return 0;
    }
    // Publication follows both successful writes, exactly as in the original.
    unsafe {
        out.write(Hook {
            target,
            gateway,
            length: n,
        })
    };
    1
}

// Only the supported Win32 target exposes the platform backend. The portable
// callback entry points above remain available for host policy tests.
#[cfg(all(target_os = "windows", target_arch = "x86", target_env = "gnu"))]
mod windows {
    use super::*;
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn GetLastError() -> u32;
        fn VirtualProtect(p: *mut c_void, n: usize, flags: u32, old: *mut u32) -> i32;
        fn VirtualAlloc(p: *mut c_void, n: usize, kind: u32, flags: u32) -> *mut c_void;
        fn VirtualFree(p: *mut c_void, n: usize, kind: u32) -> i32;
        fn GetCurrentProcess() -> *mut c_void;
        fn FlushInstructionCache(process: *mut c_void, p: *const c_void, n: usize) -> i32;
    }
    unsafe extern "C" fn protect(p: *mut c_void, n: usize, flags: u32, old: *mut u32) -> i32 {
        let mut value = 0;
        // Match the native callback's initialized out value, even on failure.
        let ok = unsafe { VirtualProtect(p, n, flags, &mut value) };
        unsafe { old.write(value) };
        ok
    }
    unsafe extern "C" fn allocate(n: usize) -> *mut c_void {
        unsafe { VirtualAlloc(ptr::null_mut(), n, 0x1000 | 0x2000, 0x40) }
    }
    unsafe extern "C" fn free_memory(p: *mut c_void) {
        let _ = unsafe { VirtualFree(p, 0, 0x8000) };
    }
    unsafe extern "C" fn flush(p: *mut c_void, n: usize) {
        let _ = unsafe { FlushInstructionCache(GetCurrentProcess(), p, n) };
    }
    pub(super) const MEMORY: Memory = Memory {
        protect,
        allocate,
        free_memory,
        flush,
    };

    pub(super) unsafe extern "C" fn last_error() -> u32 {
        unsafe { GetLastError() }
    }

    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_hook_patch(
        target: *mut c_void,
        replacement: *const c_void,
        expected: *const u8,
        n: usize,
        kind: u32,
    ) -> u8 {
        unsafe { a2fo_rs_hook_patch(target, replacement, expected, n, kind, &MEMORY) }
    }
    // Direct SDK-compatible C entry point; no C++ policy/publication adapter.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_patch_jump(
        target: *mut c_void,
        replacement: *mut c_void,
        expected: *const u8,
        n: usize,
    ) -> bool {
        unsafe { a2fo_rs_hook_patch(target, replacement.cast(), expected, n, 0, &MEMORY) != 0 }
    }
    // Direct SDK-compatible C entry point; no C++ policy/publication adapter.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_patch_call(
        target: *mut c_void,
        replacement: *mut c_void,
        expected: *const u8,
        n: usize,
    ) -> bool {
        unsafe { a2fo_rs_hook_patch(target, replacement.cast(), expected, n, 1, &MEMORY) != 0 }
    }
    // Direct SDK-compatible C entry point; no C++ policy/publication adapter.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_patch_bytes(
        target: *mut c_void,
        replacement: *const u8,
        expected: *const u8,
        n: usize,
    ) -> bool {
        unsafe { a2fo_rs_hook_patch(target, replacement.cast(), expected, n, 2, &MEMORY) != 0 }
    }
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_hook_install(
        target: *mut c_void,
        replacement: *mut c_void,
        n: usize,
        expected: *const u8,
        out: *mut Hook,
    ) -> u8 {
        unsafe { a2fo_rs_hook_install(target, replacement, n, expected, out, &MEMORY) }
    }
}

#[path = "native_patch_profiles.rs"]
mod profiles;
