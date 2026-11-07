//! Historical patch profiles whose ordering differs from the shared checked
//! hook API. Keep their double checks, publication and restore-error behavior.
use super::{Memory, branch, expected_matches};
use core::{ffi::c_void, ptr};

#[repr(C)]
#[derive(Default)]
pub struct Report {
    written: u32,
    restored: u32,
    restore_error: u32,
}
type LastError = unsafe extern "C" fn() -> u32;
type Publish = unsafe extern "C" fn(*mut c_void, *mut c_void);

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hook_write_report(
    target: *mut c_void,
    source: *const c_void,
    n: usize,
    out: *mut Report,
    api: *const Memory,
    last_error: LastError,
) -> u8 {
    if out.is_null() || api.is_null() {
        return 0;
    }
    unsafe { out.write(Report::default()) };
    let api = unsafe { &*api };
    let mut old = 0;
    if unsafe { (api.protect)(target, n, 0x40, &mut old) } == 0 {
        return 0;
    }
    // Caller supplies live nonoverlapping n-byte spans, as for native memcpy.
    unsafe {
        ptr::copy_nonoverlapping(source.cast::<u8>(), target.cast::<u8>(), n);
        (api.flush)(target, n);
    }
    let mut ignored = 0;
    let restored = unsafe { (api.protect)(target, n, old, &mut ignored) } != 0;
    let restore_error = if restored { 0 } else { unsafe { last_error() } };
    unsafe {
        out.write(Report {
            written: 1,
            restored: u32::from(restored),
            restore_error,
        });
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hook_patch_double(
    target: *mut c_void,
    replacement: *const u8,
    expected: *const u8,
    n: usize,
    api: *const Memory,
) -> u8 {
    if api.is_null() || replacement.is_null() || !unsafe { expected_matches(target, expected, n) } {
        return 0;
    }
    let api = unsafe { &*api };
    let mut old = 0;
    if unsafe { (api.protect)(target, n, 0x40, &mut old) } == 0 {
        return 0;
    }
    // CraftIdentity rereads after VirtualProtect. A mutation there prevents
    // the write but still restores protection; restoration failure is ignored.
    let changed = unsafe { expected_matches(target, expected, n) };
    if changed {
        unsafe {
            ptr::copy_nonoverlapping(replacement, target.cast::<u8>(), n);
            (api.flush)(target, n);
        }
    }
    let mut ignored = 0;
    let _ = unsafe { (api.protect)(target, n, old, &mut ignored) };
    u8::from(changed)
}

#[repr(C)]
#[derive(Default)]
pub struct VideoInstall {
    gateway: *mut c_void,
    // 0 installed; 1 signature/argument failure; 2 allocation; 3 target patch.
    status: u32,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hook_video_install(
    target: *mut c_void,
    replacement: *mut c_void,
    n: usize,
    expected: *const u8,
    out: *mut VideoInstall,
    api: *const Memory,
    context: *mut c_void,
    publish: Publish,
) -> u8 {
    if out.is_null() {
        return 0;
    }
    unsafe {
        out.write(VideoInstall {
            gateway: ptr::null_mut(),
            status: 1,
        })
    };
    if api.is_null()
        || replacement.is_null()
        || n < 5
        || !unsafe { expected_matches(target, expected, n) }
    {
        return 0;
    }
    let Some(size) = n.checked_add(5) else {
        return 0;
    };
    let api = unsafe { &*api };
    let gateway = unsafe { (api.allocate)(size) };
    if gateway.is_null() {
        unsafe { (*out).status = 2 };
        return 0;
    }
    // The original Video code copies the current prologue after allocation,
    // appends its return jump without another VirtualProtect, then flushes the
    // entire gateway once. Offsets wrap at x86 width even in host fixtures.
    unsafe {
        let bytes = gateway.cast::<u8>();
        ptr::copy_nonoverlapping(target.cast::<u8>(), bytes, n);
        bytes.add(n).write(0xe9);
        let displacement = (target as usize).wrapping_add(n) as u32;
        let displacement = displacement.wrapping_sub((gateway as usize).wrapping_add(size) as u32);
        ptr::copy_nonoverlapping(displacement.to_le_bytes().as_ptr(), bytes.add(n + 1), 4);
        (api.flush)(gateway, size);
        publish(context, gateway);
    }
    if !unsafe { branch(target, replacement, n, 0xe9, api) } {
        unsafe {
            (api.free_memory)(gateway);
            publish(context, ptr::null_mut());
            (*out).status = 3;
        }
        return 0;
    }
    unsafe { out.write(VideoInstall { gateway, status: 0 }) };
    1
}

#[cfg(all(target_os = "windows", target_arch = "x86", target_env = "gnu"))]
mod windows {
    use super::super::windows::{MEMORY, last_error};
    use super::*;
    // The renderer reports only the restore outcome; unlike Radar it never
    // queried GetLastError. Keep that Win32 call sequence unchanged.
    unsafe extern "C" fn no_error() -> u32 {
        0
    }

    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_write_report(
        target: *mut c_void,
        source: *const c_void,
        n: usize,
        out: *mut Report,
    ) -> u8 {
        unsafe { a2fo_rs_hook_write_report(target, source, n, out, &MEMORY, last_error) }
    }
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_jump_report(
        target: *mut c_void,
        replacement: *const c_void,
        out: *mut Report,
    ) -> u8 {
        let relative =
            (replacement as usize as u32).wrapping_sub((target as usize as u32).wrapping_add(5));
        let mut bytes = [0xe9, 0, 0, 0, 0];
        bytes[1..].copy_from_slice(&relative.to_le_bytes());
        unsafe {
            a2fo_rs_hook_write_report(target, bytes.as_ptr().cast(), 5, out, &MEMORY, no_error)
        }
    }
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_patch_double(
        target: *mut c_void,
        replacement: *const u8,
        expected: *const u8,
        n: usize,
    ) -> u8 {
        unsafe { a2fo_rs_hook_patch_double(target, replacement, expected, n, &MEMORY) }
    }
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_jump_unchecked(
        target: *mut c_void,
        replacement: *const c_void,
        n: usize,
    ) -> u8 {
        if target.is_null() || replacement.is_null() || n < 5 {
            return 0;
        }
        u8::from(unsafe { branch(target, replacement, n, 0xe9, &MEMORY) })
    }
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn a2fo_rs_windows_video_install(
        target: *mut c_void,
        replacement: *mut c_void,
        n: usize,
        expected: *const u8,
        out: *mut VideoInstall,
        context: *mut c_void,
        publish: Publish,
    ) -> u8 {
        unsafe {
            a2fo_rs_hook_video_install(
                target,
                replacement,
                n,
                expected,
                out,
                &MEMORY,
                context,
                publish,
            )
        }
    }
}
