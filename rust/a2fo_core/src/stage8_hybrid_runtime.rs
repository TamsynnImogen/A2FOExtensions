//! Hybrid production runtime policies; native engine/Win32 ABI and storage stay
//! behind synchronous exception-containing C++ callbacks.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;
unsafe fn checked(step: Step, ctx: *mut c_void, op: u32, index: usize) -> Result<u64, ()> {
    let value = unsafe { step(ctx, op, index, 0) };
    if value == u64::MAX {
        Err(())
    } else {
        Ok(value)
    }
}
unsafe fn resolve(kind: u32, step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        match kind {
            0 => {
                if checked(step, ctx, 0, 0)? != 0 {
                    checked(step, ctx, 1, 0)?;
                }
                if checked(step, ctx, 2, 0)? == 0 && checked(step, ctx, 3, 0)? != 0 {
                    checked(step, ctx, 4, 0)?;
                }
                checked(step, ctx, 5, 0)?;
                if checked(step, ctx, 6, 0)? != 0 {
                    checked(step, ctx, 7, 0)?;
                }
            }
            1 => {
                if checked(step, ctx, 0, 0)? != 0 {
                    checked(step, ctx, 1, 0)?;
                    checked(step, ctx, 2, 0)?;
                }
                checked(step, ctx, 3, 0)?;
                if checked(step, ctx, 4, 0)? != 0 && checked(step, ctx, 5, 0)? != 0 {
                    checked(step, ctx, 6, 0)?;
                } else {
                    checked(step, ctx, 7, 0)?;
                }
            }
            2 => {
                let provider = checked(step, ctx, 0, 0)? != 0;
                if provider {
                    for i in 0..4 {
                        checked(step, ctx, 1, i)?;
                    }
                }
                let ready = checked(step, ctx, 2, 0)? != 0 && checked(step, ctx, 3, 0)? != 0;
                if !ready {
                    checked(step, ctx, 4, 0)?;
                }
                checked(step, ctx, 5, 0)?;
                if checked(step, ctx, 6, 0)? != 0
                    && checked(step, ctx, 7, 0)? != 0
                    && checked(step, ctx, 8, 0)? != 0
                {
                    checked(step, ctx, 9, 0)?;
                } else if checked(step, ctx, 6, 0)? != 0 {
                    checked(step, ctx, 10, 0)?;
                } else {
                    checked(step, ctx, 11, 0)?;
                }
            }
            3 => {
                if checked(step, ctx, 0, 0)? == 0 {
                    return Ok(0);
                }
                if checked(step, ctx, 1, 0)? != 0 {
                    return checked(step, ctx, 2, 0);
                }
                if checked(step, ctx, 3, 0)? == 0 {
                    return Ok(0);
                }
                return Ok(u64::from(
                    checked(step, ctx, 4, 0)? >= checked(step, ctx, 5, 0)?,
                ));
            }
            4 => {
                if checked(step, ctx, 0, 0)? == 0 || checked(step, ctx, 1, 0)? == 0 {
                    return Ok(0);
                }
                return checked(
                    step,
                    ctx,
                    if checked(step, ctx, 2, 0)? != 0 { 3 } else { 4 },
                    0,
                );
            }
            5 => {
                if checked(step, ctx, 0, 0)? == 0
                    || checked(step, ctx, 1, 0)? == 0
                    || checked(step, ctx, 2, 0)? == 0
                    || checked(step, ctx, 3, 0)? == 0
                    || checked(step, ctx, 4, 0)? < checked(step, ctx, 25, 0)?
                {
                    return Ok(0);
                }
                checked(step, ctx, 5, 0)?;
                if checked(step, ctx, 6, 0)? == 0 {
                    return Ok(0);
                }
                let count = checked(step, ctx, 7, 0)? as usize;
                let mut masked = false;
                for index in 0..count {
                    checked(step, ctx, 8, index)?;
                    if checked(step, ctx, 9, 0)? == 0
                        || checked(step, ctx, 10, 0)? == 0
                        || checked(step, ctx, 11, 0)? == 0
                    {
                        continue;
                    }
                    checked(step, ctx, 12, 0)?;
                    if checked(step, ctx, 13, 0)? == 0
                        || checked(step, ctx, 14, 0)? == 0
                        || checked(step, ctx, 15, 0)? == 0
                    {
                        continue;
                    }
                    checked(step, ctx, 16, 0)?;
                    checked(step, ctx, 17, 0)?;
                    if checked(step, ctx, 18, 0)? == 0 || checked(step, ctx, 19, 0)? == 0 {
                        continue;
                    }
                    if checked(step, ctx, 20, 0)? == 0 {
                        checked(step, ctx, 21, 0)?;
                        masked = true;
                    }
                }
                if masked && checked(step, ctx, 22, 0)? == 0 {
                    checked(step, ctx, 23, 0)?;
                    checked(step, ctx, 24, 0)?;
                }
            }
            6 => {
                if checked(step, ctx, 0, 0)? == 0
                    || checked(step, ctx, 1, 0)? == 0
                    || checked(step, ctx, 2, 0)? == 0
                    || checked(step, ctx, 3, 0)? == 0
                {
                    return Ok(1);
                }
                return checked(step, ctx, 4, 0);
            }
            _ => return Err(()),
        }
        Ok(0)
    }
}
/// Safety: native callbacks own provider handles, function-pointer memcpy and
/// native exception policy; Rust never calls or interprets native function ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_hybrid_resolve(
    kind: u32,
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { resolve(kind, step, ctx) }).unwrap_or(2)
}
