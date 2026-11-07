//! Startup compatibility DLL: the stock .original DLL is no longer a consumer.
//! This preserves the existing early nested-load contract; it does not redesign
//! loader-lock initialization or remove the Win2k compatibility filename.
#![cfg_attr(not(test), no_std)]
#![deny(unsafe_op_in_unsafe_fn)]

use core::sync::atomic::{AtomicUsize, Ordering};

const ATTACHED: usize = 1;
const INITIALIZING: usize = 2;
const READY: usize = 3;
const FAILED: usize = 4;
const CAPACITY: usize = 32_768;

trait Host {
    fn load_relative(&mut self, instance: usize, parent: bool, name: &str) -> usize;
    fn compatible(&mut self, module: usize) -> bool;
    fn early_attach(&mut self, _module: usize) -> bool {
        true
    }
    fn initialize(&mut self, module: usize) -> bool;
    fn sleep(&mut self);
    fn log(&mut self, message: &'static [u8]);
}

struct State {
    instance: AtomicUsize,
    core: AtomicUsize,
    phase: AtomicUsize,
}
impl State {
    const fn new() -> Self {
        Self {
            instance: AtomicUsize::new(0),
            core: AtomicUsize::new(0),
            phase: AtomicUsize::new(0),
        }
    }
    fn attach(&self, instance: usize, host: &mut impl Host) -> bool {
        self.instance.store(instance, Ordering::Release);
        host.log(b"Rust startup: loading FleetOpsHook directly\r\n");
        if host.load_relative(instance, true, "FleetOpsHook.dll") == 0 {
            self.phase.store(FAILED, Ordering::Release);
            host.log(b"Rust startup: FleetOpsHook load failed\r\n");
            return false;
        }
        host.log(b"Rust startup: FleetOpsHook attached; loading A2FOExtensions\r\n");
        let core = host.load_relative(instance, false, "A2FOExtensions.dll");
        if core == 0 {
            self.phase.store(FAILED, Ordering::Release);
            host.log(b"Rust startup: A2FOExtensions load failed\r\n");
            return false;
        }
        if !host.compatible(core) {
            self.phase.store(FAILED, Ordering::Release);
            host.log(b"Rust startup: matching core with no-unwind initialization is required\r\n");
            return false;
        }
        if !host.early_attach(core) {
            self.phase.store(FAILED, Ordering::Release);
            host.log(b"Rust startup: Bink early attachment failed\r\n");
            return false;
        }
        self.core.store(core, Ordering::Release);
        self.phase.store(ATTACHED, Ordering::Release);
        host.log(b"Rust startup: A2FOExtensions attached; initialization remains deferred\r\n");
        true
    }
    fn ensure_ready(&self, host: &mut impl Host) -> bool {
        for _ in 0..5_000 {
            match self.phase.load(Ordering::Acquire) {
                READY => return true,
                FAILED | 0 => return false,
                ATTACHED => {
                    if self
                        .phase
                        .compare_exchange(
                            ATTACHED,
                            INITIALIZING,
                            Ordering::AcqRel,
                            Ordering::Acquire,
                        )
                        .is_err()
                    {
                        continue;
                    }
                    // Export calls occur after DllMain. The core's own worker may
                    // already have initialized it; its public entry is idempotent.
                    let good = host.initialize(self.core.load(Ordering::Acquire));
                    self.phase
                        .store(if good { READY } else { FAILED }, Ordering::Release);
                    host.log(if good {
                        b"Rust startup: deferred initialization ready\r\n"
                    } else {
                        b"Rust startup: deferred initialization failed\r\n"
                    });
                    return good;
                }
                INITIALIZING => host.sleep(),
                _ => return false,
            }
        }
        host.log(b"Rust startup: deferred initialization wait timed out\r\n");
        false
    }
}

/// Rewrite a NUL-free absolute module filename into a sibling/parent filename.
/// No current-working-directory or PATH lookup supplies either product DLL.
fn relative_name(path: &mut [u16], length: usize, parent: bool, name: &str) -> bool {
    if length == 0
        || length >= path.len()
        || length >= CAPACITY
        || name.is_empty()
        || !name.is_ascii()
        || name.bytes().any(|c| matches!(c, 0 | b'/' | b'\\' | b':'))
    {
        return false;
    }
    let Some(mut end) = path[..length]
        .iter()
        .rposition(|&c| c == b'\\' as u16 || c == b'/' as u16)
    else {
        return false;
    };
    if parent {
        let Some(previous) = path[..end]
            .iter()
            .rposition(|&c| c == b'\\' as u16 || c == b'/' as u16)
        else {
            return false;
        };
        end = previous;
    }
    // A rooted drive path must retain its slash; UNC/server roots also retain
    // their existing prefix. The supported installation has a Data directory.
    if end == 0 {
        return false;
    }
    let start = end + 1;
    if name.len() >= path.len().saturating_sub(start) {
        return false;
    }
    for (i, byte) in name.bytes().enumerate() {
        path[start + i] = u16::from(byte);
    }
    path[start + name.len()] = 0;
    true
}

#[cfg(not(test))]
mod platform {
    use super::*;
    use core::ffi::c_void;
    static STATE: State = State::new();
    #[link(
        name = "kernel32",
        kind = "raw-dylib",
        import_name_type = "undecorated"
    )]
    unsafe extern "system" {
        fn DisableThreadLibraryCalls(module: usize) -> i32;
        fn GetModuleFileNameW(module: usize, path: *mut u16, count: u32) -> u32;
        fn LoadLibraryW(path: *const u16) -> usize;
        fn GetProcAddress(module: usize, name: *const u8) -> *mut c_void;
        fn GetProcessHeap() -> usize;
        fn HeapAlloc(heap: usize, flags: u32, size: usize) -> *mut c_void;
        fn HeapFree(heap: usize, flags: u32, pointer: *mut c_void) -> i32;
        fn Sleep(milliseconds: u32);
        fn CreateFileW(
            path: *const u16,
            access: u32,
            share: u32,
            security: *const c_void,
            disposition: u32,
            attributes: u32,
            template: usize,
        ) -> usize;
        fn WriteFile(
            file: usize,
            buffer: *const u8,
            size: u32,
            written: *mut u32,
            overlapped: *mut c_void,
        ) -> i32;
        fn CloseHandle(handle: usize) -> i32;
        fn ExitProcess(code: u32) -> !;
    }
    struct PathBuffer {
        pointer: *mut u16,
        heap: usize,
    }
    impl PathBuffer {
        fn new(instance: usize, parent: bool, name: &str) -> Option<Self> {
            let heap = unsafe { GetProcessHeap() };
            let pointer = unsafe { HeapAlloc(heap, 8, CAPACITY * 2) }.cast::<u16>();
            if pointer.is_null() {
                return None;
            }
            let result = Self { pointer, heap };
            let length = unsafe { GetModuleFileNameW(instance, pointer, CAPACITY as u32) } as usize;
            let path = unsafe { core::slice::from_raw_parts_mut(pointer, CAPACITY) };
            if relative_name(path, length, parent, name) {
                Some(result)
            } else {
                None
            }
        }
    }
    impl Drop for PathBuffer {
        fn drop(&mut self) {
            unsafe {
                HeapFree(self.heap, 0, self.pointer.cast());
            }
        }
    }
    struct Windows;
    impl Host for Windows {
        fn load_relative(&mut self, instance: usize, parent: bool, name: &str) -> usize {
            PathBuffer::new(instance, parent, name)
                .map_or(0, |path| unsafe { LoadLibraryW(path.pointer) })
        }
        fn initialize(&mut self, module: usize) -> bool {
            let address =
                unsafe { GetProcAddress(module, c"A2FO_InitializeNoUnwind".as_ptr().cast()) };
            if address.is_null() {
                return false;
            }
            let function: unsafe extern "C" fn() -> bool = unsafe { core::mem::transmute(address) };
            unsafe { function() }
        }
        fn compatible(&mut self, module: usize) -> bool {
            !unsafe { GetProcAddress(module, c"A2FO_InitializeNoUnwind".as_ptr().cast()) }.is_null()
        }
        fn early_attach(&mut self, module: usize) -> bool {
            let address = unsafe {
                GetProcAddress(module, c"A2FO_BinkEarlyAttachNoUnwind".as_ptr().cast())
            };
            if address.is_null() {
                return true;
            }
            // A2FO_CALL is __cdecl on the 32-bit target, as is the existing
            // A2FO_InitializeNoUnwind entry above.
            let function: unsafe extern "C" fn() -> bool = unsafe { core::mem::transmute(address) };
            unsafe { function() }
        }
        fn sleep(&mut self) {
            unsafe {
                Sleep(1);
            }
        }
        fn log(&mut self, message: &'static [u8]) {
            let Some(path) = PathBuffer::new(
                STATE.instance.load(Ordering::Acquire),
                false,
                "A2FOStartupProxy.log",
            ) else {
                return;
            };
            let file = unsafe { CreateFileW(path.pointer, 4, 3, core::ptr::null(), 4, 0x80, 0) };
            if file == usize::MAX {
                return;
            }
            let mut written = 0;
            unsafe {
                WriteFile(
                    file,
                    message.as_ptr(),
                    message.len() as u32,
                    &mut written,
                    core::ptr::null_mut(),
                );
                CloseHandle(file);
            }
        }
    }
    #[unsafe(no_mangle)]
    pub extern "C" fn a2fo_proxy_set_hook_id(_hook: usize) {
        STATE.ensure_ready(&mut Windows);
        // The pinned stock SetHookID body is empty. No input hook is installed.
    }
    #[unsafe(no_mangle)]
    pub extern "system" fn a2fo_proxy_low_level_keyboard_proc(
        _code: i32,
        _wparam: usize,
        _lparam: isize,
    ) -> isize {
        STATE.ensure_ready(&mut Windows);
        // Preserve its observed zero result without calling an input API.
        0
    }
    #[unsafe(no_mangle)]
    pub extern "system" fn DllMain(instance: usize, reason: u32, _reserved: usize) -> i32 {
        if reason != 1 {
            return 1;
        }
        unsafe {
            DisableThreadLibraryCalls(instance);
        }
        i32::from(STATE.attach(instance, &mut Windows))
    }
    #[panic_handler]
    fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
        unsafe { ExitProcess(0xe000_a2f0) }
    }
}

#[cfg(test)]
mod tests;
