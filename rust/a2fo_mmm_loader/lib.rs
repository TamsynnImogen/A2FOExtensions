//! MMM's two-export compatibility DLL. No allocation, VM or teardown under loader lock.
#![cfg_attr(not(test), no_std)]
#![deny(unsafe_op_in_unsafe_fn)]

use core::sync::atomic::{AtomicUsize, Ordering};
const CAPACITY: usize = 261;
const CORE: &[u8] = b"A2FOExtensions.dll\0";
const MODULE: &[u8] = b"A2FOMissions.dll\0";
const EXTEND: &[u8] = b"A2FOMissions_ScriptExtendInterface\0";
const INIT: &[u8] = b"A2FOMissions_ScriptInit\0";
const UNLOAD: &[u8] = b"A2FOMissions_MarkMissionUnloaded\0";

trait Host {
    fn module(&mut self, name: &[u8]) -> usize;
    fn symbol(&mut self, module: usize, name: &[u8]) -> usize;
    fn filename(&mut self, module: usize, path: &mut [u8; CAPACITY]) -> u32;
    fn load(&mut self, path: &[u8; CAPACITY]) -> usize;
    fn ready(&mut self, function: usize) -> bool;
    fn debug(&mut self, text: &[u8]);
}

struct State {
    instance: AtomicUsize,
    extend: AtomicUsize,
    init: AtomicUsize,
    unloaded: AtomicUsize,
}
impl State {
    const fn new() -> Self {
        Self {
            instance: AtomicUsize::new(0),
            extend: AtomicUsize::new(0),
            init: AtomicUsize::new(0),
            unloaded: AtomicUsize::new(0),
        }
    }
    fn cached(&self) -> bool {
        self.extend.load(Ordering::Acquire) != 0 && self.init.load(Ordering::Acquire) != 0
    }
    fn resolve(&self, host: &mut impl Host) -> bool {
        if self.cached() {
            return true;
        }
        let core = host.module(CORE);
        if core != 0 {
            let ready = host.symbol(core, b"A2FOBuiltinModule_StartupReady\0");
            let owner = host.symbol(core, b"A2FOBuiltinMissions_Init\0");
            if ready != 0 && owner != 0 {
                let extend = host.symbol(core, EXTEND);
                let init = host.symbol(core, INIT);
                let unloaded = host.symbol(core, UNLOAD);
                if extend == 0 || init == 0 || unloaded == 0 || !host.ready(ready) {
                    host.debug(b"built-in Missions unavailable\0");
                    return false;
                }
                self.publish(extend, init, unloaded);
                return true;
            }
        }
        let mut module = host.module(MODULE);
        if module == 0 {
            // GetModuleHandle is intentionally repeated, as in the original shim.
            let root = host.module(CORE);
            module = load_from(host, root, false);
        }
        if module == 0 {
            module = load_from(host, self.instance.load(Ordering::Acquire), true);
        }
        if module == 0 {
            host.debug(b"unable to locate modules/A2FOMissions.dll\0");
            return false;
        }
        let extend = host.symbol(module, EXTEND);
        let init = host.symbol(module, INIT);
        let unloaded = host.symbol(module, UNLOAD);
        // Preserve partial standalone publication, including its optional unload callback.
        self.publish(extend, init, unloaded);
        if extend == 0 || init == 0 {
            host.debug(b"A2FOMissions.dll is missing its MMM compatibility exports\0");
            return false;
        }
        true
    }
    fn publish(&self, extend: usize, init: usize, unloaded: usize) {
        self.unloaded.store(unloaded, Ordering::Release);
        self.init.store(init, Ordering::Release);
        self.extend.store(extend, Ordering::Release);
    }
}

fn separator(path: &[u8]) -> Option<usize> {
    // Legacy priority is the last backslash, falling back to the last slash.
    path.iter()
        .rposition(|&c| c == b'\\')
        .or_else(|| path.iter().rposition(|&c| c == b'/'))
}
fn load_from(host: &mut impl Host, module: usize, relative: bool) -> usize {
    if module == 0 {
        return 0;
    }
    let mut path = [0; CAPACITY];
    let count = host.filename(module, &mut path) as usize;
    if count == 0 || count >= 260 {
        return 0;
    }
    let used = path.iter().position(|&c| c == 0).unwrap_or(CAPACITY);
    let Some(mut slash) = separator(&path[..used]) else {
        return 0;
    };
    if relative {
        let Some(parent) = separator(&path[..slash]) else {
            return 0;
        };
        slash = parent;
    }
    let start = slash + 1;
    let suffix = b"modules\\A2FOMissions.dll\0";
    if start + suffix.len() > path.len() {
        return 0;
    }
    path[start..start + suffix.len()].copy_from_slice(suffix);
    host.load(&path)
}

#[cfg(all(
    not(test),
    target_os = "windows",
    target_arch = "x86",
    target_env = "gnu"
))]
mod windows {
    use super::*;
    use core::ffi::{c_char, c_void};
    static STATE: State = State::new();
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn GetModuleHandleA(name: *const c_char) -> usize;
        fn GetProcAddress(module: usize, name: *const c_char) -> usize;
        fn GetModuleFileNameA(module: usize, path: *mut u8, size: u32) -> u32;
        fn LoadLibraryA(name: *const u8) -> usize;
        fn OutputDebugStringA(text: *const u8);
        fn DisableThreadLibraryCalls(module: usize) -> i32;
        fn TerminateProcess(process: usize, code: u32) -> i32;
        fn GetCurrentProcess() -> usize;
    }
    struct Win32;
    impl Host for Win32 {
        fn module(&mut self, name: &[u8]) -> usize {
            unsafe { GetModuleHandleA(name.as_ptr().cast()) }
        }
        fn symbol(&mut self, module: usize, name: &[u8]) -> usize {
            unsafe { GetProcAddress(module, name.as_ptr().cast()) }
        }
        fn filename(&mut self, module: usize, path: &mut [u8; CAPACITY]) -> u32 {
            unsafe { GetModuleFileNameA(module, path.as_mut_ptr(), 260) }
        }
        fn load(&mut self, path: &[u8; CAPACITY]) -> usize {
            unsafe { LoadLibraryA(path.as_ptr()) }
        }
        fn ready(&mut self, function: usize) -> bool {
            let ready: unsafe extern "C" fn(*const u8) -> bool =
                unsafe { core::mem::transmute(function) };
            unsafe { ready(c"A2FOMissions".as_ptr().cast()) }
        }
        fn debug(&mut self, text: &[u8]) {
            unsafe {
                OutputDebugStringA(c"mmm_loader shim: ".as_ptr().cast());
                OutputDebugStringA(text.as_ptr());
                OutputDebugStringA(c"\n".as_ptr().cast());
            }
        }
    }
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ScriptExtendInterface(param: *mut c_void) -> i32 {
        if !STATE.resolve(&mut Win32) {
            return 51;
        }
        let function: unsafe extern "C" fn(*mut c_void) -> i32 =
            unsafe { core::mem::transmute(STATE.extend.load(Ordering::Acquire)) };
        unsafe { function(param) }
    }
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ScriptInit(param: *mut c_void) -> i32 {
        if !STATE.resolve(&mut Win32) {
            return 0;
        }
        let function: unsafe extern "C" fn(*mut c_void) -> i32 =
            unsafe { core::mem::transmute(STATE.init.load(Ordering::Acquire)) };
        unsafe { function(param) }
    }
    unsafe extern "system" fn dll_main(instance: usize, reason: u32, reserved: *mut c_void) -> i32 {
        if reason == 1 {
            STATE.instance.store(instance, Ordering::Release);
            unsafe { DisableThreadLibraryCalls(instance) };
        } else if reason == 0 && reserved.is_null() {
            let unloaded = STATE.unloaded.load(Ordering::Acquire);
            if unloaded != 0 {
                let function: unsafe extern "C" fn() = unsafe { core::mem::transmute(unloaded) };
                unsafe { function() };
            }
        }
        1
    }
    core::arch::global_asm!(".globl _DllMain@12", "_DllMain@12:", "jmp {}", sym dll_main);
    #[panic_handler]
    fn panic(_: &core::panic::PanicInfo<'_>) -> ! {
        unsafe { TerminateProcess(GetCurrentProcess(), 3) };
        loop {
            core::hint::spin_loop();
        }
    }
}
#[cfg(all(
    not(test),
    not(all(target_os = "windows", target_arch = "x86", target_env = "gnu"))
))]
compile_error!("MMM loader requires the verified x86 Windows GNU ABI");

#[cfg(test)]
mod tests;
