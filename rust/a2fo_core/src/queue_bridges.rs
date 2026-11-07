//! Immutable callback registration for the native Refit/Hybrid queue bridges.
//! Native thunks own calling conventions; this module owns validation,
//! publication and fail-open dispatch. A published callback array never moves.
use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::sync::atomic::{AtomicU32, Ordering};
struct Registry {
    state: AtomicU32,
    functions: UnsafeCell<[usize; 12]>,
}
// SAFETY: exactly one writer wins state 0->1; it copies the complete table
// before release-publishing state 2. Readers acquire 2 and never write it.
unsafe impl Sync for Registry {}
impl Registry {
    const fn new() -> Self {
        Self {
            state: AtomicU32::new(0),
            functions: UnsafeCell::new([0; 12]),
        }
    }
    unsafe fn register(
        &self,
        version: u32,
        size: u32,
        p: *const usize,
        expected: u32,
        count: usize,
    ) -> bool {
        if p.is_null()
            || version != expected
            || (size as usize) < 8 + count * core::mem::size_of::<usize>()
        {
            return false;
        }
        // SAFETY: private adapter provides a live array with count entries.
        let values = unsafe { core::slice::from_raw_parts(p, count) };
        if values.contains(&0)
            || self
                .state
                .compare_exchange(0, 1, Ordering::AcqRel, Ordering::Acquire)
                .is_err()
        {
            return false;
        }
        // SAFETY: only winning registrar can write before publishing state 2.
        unsafe {
            (&mut *self.functions.get())[..count].copy_from_slice(values);
        }
        self.state.store(2, Ordering::Release);
        true
    }
    fn lookup(&self, operation: u32, count: usize) -> Option<usize> {
        if operation as usize >= count || self.state.load(Ordering::Acquire) != 2 {
            return None;
        }
        // SAFETY: immutable fully published table, bounded operation.
        Some(unsafe { (*self.functions.get())[operation as usize] })
    }
}
static REFIT: Registry = Registry::new();
static HYBRID: Registry = Registry::new();
fn registry(kind: u32) -> Option<(&'static Registry, u32, usize)> {
    match kind {
        0 => Some((&REFIT, 2, 5)),
        1 => Some((&HYBRID, 1, 12)),
        _ => None,
    }
}
type Invoke =
    unsafe extern "C" fn(usize, u32, *mut c_void, *mut c_void, *mut c_void, u32, u32) -> usize;
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_queue_bridge_valid_header(kind: u32, version: u32, size: u32) -> u8 {
    let Some((_, expected, count)) = registry(kind) else {
        return 0;
    };
    u8::from(version == expected && size as usize >= 8 + count * core::mem::size_of::<usize>())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_queue_bridge_register(
    kind: u32,
    version: u32,
    size: u32,
    functions: *const usize,
) -> u8 {
    let Some((r, v, n)) = registry(kind) else {
        return 0;
    };
    // SAFETY: forwarded private adapter contract, as documented in register.
    u8::from(unsafe { r.register(version, size, functions, v, n) })
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_queue_bridge_dispatch(
    kind: u32,
    operation: u32,
    invoke: Option<Invoke>,
    source: *mut c_void,
    target: *mut c_void,
    result: *mut c_void,
    queue: u32,
    value: u32,
) -> usize {
    let Some((r, _, n)) = registry(kind) else {
        return 0;
    };
    let (Some(function), Some(invoke)) = (r.lookup(operation, n), invoke) else {
        return 0;
    };
    // SAFETY: synchronous native thunk uses the registered callback's exact ABI.
    unsafe { invoke(function, operation, source, target, result, queue, value) }
}
