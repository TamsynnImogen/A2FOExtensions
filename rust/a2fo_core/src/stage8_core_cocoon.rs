//! Cocoon geometry selection policy. Native class/cache storage, manual locks,
//! engine ABI, geometry loading and process-global slots stay native.
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
unsafe fn select(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        if checked(step, ctx, 0, 0)? == 0 || checked(step, ctx, 1, 0)? == 0 {
            return Ok(0);
        }
        checked(step, ctx, 2, 0)?;
        checked(step, ctx, 3, 0)?;
        if checked(step, ctx, 4, 0)? != 0 {
            checked(step, ctx, 5, 0)?;
        }
        let cached = if checked(step, ctx, 6, 0)? == 0 {
            checked(step, ctx, 7, 0)? != 0
        } else {
            false
        };
        let first = checked(step, ctx, 9, 0)? != 0;
        checked(step, ctx, 10, 0)?;
        if first {
            checked(step, ctx, 11, 0)?;
        }
        if cached {
            if checked(step, ctx, 8, 0)? != 0 {
                checked(step, ctx, 12, 0)?;
                return Ok(1);
            }
            checked(step, ctx, 13, 0)?;
        }
        if checked(step, ctx, 6, 0)? == 0 {
            if checked(step, ctx, 14, 0)? != 0 {
                checked(step, ctx, 15, 0)?;
            }
            checked(step, ctx, 16, 0)?;
            checked(step, ctx, 17, 0)?;
            checked(step, ctx, 18, 0)?;
            if checked(step, ctx, 19, 0)? != 0 {
                checked(step, ctx, 20, 0)?;
                return Ok(1);
            }
            checked(step, ctx, 21, 0)?;
        }
        let alternative = checked(step, ctx, 22, 0)? != 0;
        if checked(step, ctx, 23, usize::from(alternative))? != 0 {
            checked(step, ctx, 24, 0)?;
            return Ok(1);
        }
        checked(step, ctx, 25, 0)?;
        if checked(step, ctx, 26, 0)? != 0 {
            checked(step, ctx, 27, 0)?;
        }
        if checked(step, ctx, 19, 0)? != 0 {
            checked(step, ctx, 29, 0)?;
            checked(step, ctx, 30, 0)?;
        } else {
            checked(step, ctx, 31, 0)?;
        }
        checked(step, ctx, 20, 0)?;
        Ok(1)
    }
}
/// Safety: native context contains all pointers and exceptions. Native manual
/// lock behavior, including original exceptional paths, is not synthesized or
/// repaired by Rust. No SDK layout or native mutable reference crosses FFI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_select_cocoon(step: Step, ctx: *mut c_void) -> u64 {
    (unsafe { select(step, ctx) }).unwrap_or(2)
}
