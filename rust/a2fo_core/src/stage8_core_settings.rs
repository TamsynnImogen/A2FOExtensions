//! Info defaults workflows; native SDK callbacks, strings, locks and immutable
//! Delphi string allocation stay opaque and exception-contained in C++.
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
unsafe fn query(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        checked(step, ctx, 0, 0)?;
        checked(step, ctx, 1, 0)?;
        if checked(step, ctx, 2, 0)? == 0 {
            return Ok(0);
        }
        if checked(step, ctx, 3, 0)? != 0 {
            checked(step, ctx, 4, 0)?;
        }
        if checked(step, ctx, 5, 0)? == 0 {
            return Ok(0);
        }
        if checked(step, ctx, 6, 0)? != 0 {
            checked(step, ctx, 7, 0)?;
        }
        let has = if checked(step, ctx, 8, 0)? != 0 {
            let speed = checked(step, ctx, 9, 0)? as u32 as i32;
            (1..=6).contains(&speed)
        } else {
            false
        };
        checked(step, ctx, 10, usize::from(has))?;
        checked(step, ctx, 11, usize::from(has))?;
        Ok(1)
    }
}
/// Safety: synchronous native callback owns SDK outputs and contains exceptions;
/// native caller rethrows inside its original outer try/catch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_query_defaults(step: Step, ctx: *mut c_void) -> u64 {
    (unsafe { query(step, ctx) }).unwrap_or(2)
}
unsafe fn ensure(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        checked(step, ctx, 0, 0)?;
        if checked(step, ctx, 1, 0)? == 0 {
            return Ok(0);
        }
        let prior = checked(step, ctx, 2, 0)? as u32 as i32;
        if prior == 0 {
            let loaded = checked(step, ctx, 3, 0)? != 0;
            if loaded {
                checked(step, ctx, 4, 0)?;
            }
            checked(step, ctx, 5, usize::from(loaded))?;
        } else {
            while checked(step, ctx, 6, 0)? == 1 {
                checked(step, ctx, 7, 0)?;
            }
        }
        Ok(0)
    }
}
/// Safety: native callback performs the original atomic protocol and native
/// provider call; no state reset is synthesized when a callback fails.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_ensure_defaults(step: Step, ctx: *mut c_void) -> u64 {
    (unsafe { ensure(step, ctx) }).unwrap_or(2)
}
unsafe fn resolve(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        checked(step, ctx, 0, 0)?;
        if checked(step, ctx, 1, 0)? == 0 {
            return Ok(0);
        }
        if checked(step, ctx, 2, 0)? != 0 {
            return Ok(1);
        }
        if checked(step, ctx, 3, 0)? == 0 || checked(step, ctx, 4, 0)? != 0 {
            return Ok(0);
        }
        checked(step, ctx, 5, 0)?;
        checked(step, ctx, 6, 0)?;
        let available = checked(step, ctx, 7, 0)? != 0;
        checked(step, ctx, if available { 8 } else { 9 }, 0)?;
        Ok(1)
    }
}
/// Safety: native context owns candidate/cached immortal pointers and publishes
/// them under the original native lock; Rust never interprets Delphi layouts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_resolve_directory(
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { resolve(step, ctx) }).unwrap_or(2)
}
