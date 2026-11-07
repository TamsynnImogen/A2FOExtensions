//! Deferred core startup policies. Loader-lock entry, native atomics, locks,
//! thread IDs, module handles and checked engine addresses remain native.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;
unsafe fn checked(step: Step, ctx: *mut c_void, op: u32, value: usize) -> Result<u64, ()> {
    let result = unsafe { step(ctx, op, value, 0) };
    if result == u64::MAX {
        Err(())
    } else {
        Ok(result)
    }
}
unsafe fn initialize(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        let prior = checked(step, ctx, 0, 0)? as u32 as i32;
        if prior == 2 {
            return Ok(1);
        }
        if prior == 1 {
            for _ in 0..5000 {
                let state = checked(step, ctx, 1, 0)? as u32 as i32;
                if state == 2 {
                    return Ok(1);
                }
                if state == 0 {
                    break;
                }
                checked(step, ctx, 2, 0)?;
            }
            return Ok(u64::from(checked(step, ctx, 1, 0)? == 2));
        }
        if prior != 0 {
            return Ok(0);
        }
        if checked(step, ctx, 3, 0)? != 0 {
            checked(step, ctx, 4, 0)?;
        }
        checked(step, ctx, 5, 0)?;
        if checked(step, ctx, 6, 0)? != 0 {
            checked(step, ctx, 7, 0)?;
        }
        checked(step, ctx, 8, 0)?;
        if checked(step, ctx, 9, 0)? != 0 {
            checked(step, ctx, 10, 0)?;
        }
        if checked(step, ctx, 11, 0)? != 0 {
            checked(step, ctx, 12, 0)?;
            checked(step, ctx, 13, 0)?;
            return Ok(0);
        }
        checked(step, ctx, 14, 0)?;
        if checked(step, ctx, 15, 0)? != 0 {
            checked(step, ctx, 16, 0)?;
        }
        let ok = checked(step, ctx, 17, 0)? != 0;
        checked(step, ctx, 18, usize::from(ok))?;
        Ok(u64::from(ok))
    }
}
/// Safety: callback owns all native state and contains native exceptions;
/// failures are rethrown after returning to the original native export frame.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_initialize(step: Step, ctx: *mut c_void) -> u64 {
    (unsafe { initialize(step, ctx) }).unwrap_or(2)
}
unsafe fn worker(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        checked(step, ctx, 0, 0)?;
        let initialized = checked(step, ctx, 1, 0)? != 0;
        checked(step, ctx, 2, 0)?;
        checked(step, ctx, 3, 0)?;
        if initialized {
            let monitor = checked(step, ctx, 4, 0)? != 0;
            checked(step, ctx, if monitor { 5 } else { 6 }, 0)?;
        }
        Ok(0)
    }
}
/// Safety: callback owns the native worker state; no native exception or
/// native container layout crosses the Rust program.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_worker(step: Step, ctx: *mut c_void) -> u64 {
    (unsafe { worker(step, ctx) }).unwrap_or(2)
}
