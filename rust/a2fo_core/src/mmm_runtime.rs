//! Mission handoff state and opaque Worker ownership. Native callbacks are an
//! exception firewall; no engine calls or destruction occur in mark_unloaded.
use core::ffi::c_void;
use core::ptr;
use core::sync::atomic::{AtomicBool, AtomicPtr, Ordering};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CallResult {
    pub value: usize,
    pub failed: u32,
}
type Invoke = unsafe extern "C" fn(*mut c_void, u32, *mut c_void) -> CallResult;
struct Runtime {
    first: AtomicBool,
    new_game: AtomicBool,
    unloaded: AtomicBool,
    worker: AtomicPtr<c_void>,
}
impl Runtime {
    const fn new() -> Self {
        Self {
            first: AtomicBool::new(true),
            new_game: AtomicBool::new(false),
            unloaded: AtomicBool::new(false),
            worker: AtomicPtr::new(ptr::null_mut()),
        }
    }
}
static RUNTIME: Runtime = Runtime::new();
struct Calls {
    invoke: Invoke,
    context: *mut c_void,
}
impl Calls {
    fn call(&self, operation: u32, argument: *mut c_void) -> Result<usize, ()> {
        // SAFETY: caller supplies a synchronous, non-unwinding callback and context.
        let result = unsafe { (self.invoke)(self.context, operation, argument) };
        if result.failed == 0 {
            Ok(result.value)
        } else {
            Err(())
        }
    }
    fn op(&self, operation: u32) -> Result<usize, ()> {
        self.call(operation, ptr::null_mut())
    }
    fn log(&self, message: usize) -> Result<(), ()> {
        self.call(1, message as *mut c_void).map(|_| ())
    }
}
fn result(value: usize, failed: bool) -> CallResult {
    CallResult {
        value,
        failed: u32::from(failed),
    }
}
fn extend(state: &Runtime, calls: &Calls, interface: *mut c_void) -> Result<(), ()> {
    calls.log(1)?;
    if interface.is_null() {
        calls.log(2)?;
        return Ok(());
    }
    if state.unloaded.swap(false, Ordering::AcqRel) {
        calls.op(3)?;
        calls.call(5, interface)?;
        // unique_ptr::reset published null before running the old destructor.
        let worker = state.worker.swap(ptr::null_mut(), Ordering::AcqRel);
        if !worker.is_null() {
            calls.call(9, worker)?;
        }
        state.first.store(true, Ordering::Release);
        state.new_game.store(false, Ordering::Release);
    }
    calls.log(3)?;
    if calls.op(2)? != 0 {
        calls.log(4)?;
        return Ok(());
    }
    calls.log(5)?;
    calls.log(6)?;
    calls.call(5, interface)?;
    calls.log(7)?;
    state.first.store(true, Ordering::Release);
    state.new_game.store(false, Ordering::Release);
    calls.log(8)?;
    Ok(())
}
fn input(state: &Runtime, calls: &Calls) -> Result<(), ()> {
    if calls.op(4)? != 0 {
        calls.log(14)?;
        return Ok(());
    }
    if state.first.load(Ordering::Acquire) {
        calls.log(15)?;
        let mut worker = state.worker.load(Ordering::Acquire);
        if worker.is_null() {
            calls.log(16)?;
            worker = calls.op(8)? as *mut c_void;
            state.worker.store(worker, Ordering::Release);
            calls.log(17)?;
        }
        if state.new_game.load(Ordering::Acquire) {
            calls.log(18)?;
            calls.call(10, worker)?;
            calls.log(19)?;
        } else {
            calls.log(20)?;
            calls.call(11, worker)?;
            calls.log(21)?;
        }
        state.first.store(false, Ordering::Release);
    }
    Ok(())
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_mmm_runtime() -> *mut c_void {
    ptr::from_ref(&RUNTIME).cast_mut().cast()
}
/// # Safety
/// State is returned by runtime(); callback/context stay live for this call.
/// Callbacks must never unwind. Engine lifecycle calls use its mission thread.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_mmm_extend(
    state: *mut c_void,
    invoke: Invoke,
    context: *mut c_void,
    interface: *mut c_void,
) -> CallResult {
    let state = unsafe { &*state.cast::<Runtime>() };
    result(
        51,
        extend(state, &Calls { invoke, context }, interface).is_err(),
    )
}
/// # Safety
/// Same contract as extend. Returns the original edit-mode result 51.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_mmm_init(
    state: *mut c_void,
    invoke: Invoke,
    context: *mut c_void,
) -> CallResult {
    let state = unsafe { &*state.cast::<Runtime>() };
    // The edit probe belongs inside init's exact callback sequence, so capture
    // its result there rather than probing twice.
    let calls = Calls { invoke, context };
    let run = || -> Result<usize, ()> {
        calls.log(9)?;
        calls.op(3)?;
        if calls.op(2)? != 0 {
            calls.log(10)?;
            return Ok(51);
        }
        if calls.op(6)? == 0 {
            calls.log(11)?;
            return Ok(0);
        }
        calls.log(12)?;
        calls.op(7)?;
        calls.log(13)?;
        state.new_game.store(true, Ordering::Release);
        Ok(0)
    };
    match run() {
        Ok(value) => result(value, false),
        Err(()) => result(0, true),
    }
}
/// # Safety
/// Same contract as extend; native exceptions return failed and preserve retry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_mmm_input(
    state: *mut c_void,
    invoke: Invoke,
    context: *mut c_void,
) -> CallResult {
    let state = unsafe { &*state.cast::<Runtime>() };
    result(1, input(state, &Calls { invoke, context }).is_err())
}
/// # Safety
/// State remains live. This is the only entry permitted from loader detach.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_mmm_mark_unloaded(state: *mut c_void) {
    unsafe { &*state.cast::<Runtime>() }
        .unloaded
        .store(true, Ordering::Release);
}
/// # Safety
/// State remains live. Preserve process-teardown abandonment without destruction.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_mmm_abandon_worker(state: *mut c_void) {
    unsafe { &*state.cast::<Runtime>() }
        .worker
        .store(ptr::null_mut(), Ordering::Release);
}

/// # Safety
/// Same callback contract as extend. Used only by the standalone native CRT
/// finalizer, preserving its prior unique_ptr destructor when shutdown was not
/// called. Normal module shutdown abandons the worker before this fallback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_mmm_drop_worker(
    state: *mut c_void,
    invoke: Invoke,
    context: *mut c_void,
) -> CallResult {
    let state = unsafe { &*state.cast::<Runtime>() };
    let worker = state.worker.load(Ordering::Acquire);
    if !worker.is_null() {
        if (Calls { invoke, context }).call(9, worker).is_err() {
            return result(0, true);
        }
        // Match unique_ptr's destructor: clear only after the deleter returns.
        state.worker.store(ptr::null_mut(), Ordering::Release);
    }
    result(0, false)
}
