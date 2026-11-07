//! First-run configuration and settings-directory control. Foreign heaps,
//! Delphi/fastcall calls, atomics, fixed offsets and original exception scopes
//! are native primitives; callbacks cannot unwind across this boundary.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, u64) -> u64;
struct Flow {
    step: Step,
    context: *mut c_void,
    failed: *const u8,
}
impl Flow {
    fn call(&self, op: u32) -> Option<u64> {
        self.arg(op, 0)
    }
    fn arg(&self, op: u32, arg: u64) -> Option<u64> {
        let value = unsafe { (self.step)(self.context, op, arg) };
        (unsafe { self.failed.read() } == 0).then_some(value)
    }
    fn run(&self, kind: u32) -> Option<u64> {
        match kind {
            0 => {
                if self.call(0)? != 0 || self.call(1)? == self.call(2)? {
                    return Some(0);
                }
                for _ in 0..10000 {
                    if self.call(0)? != 0 {
                        return Some(0);
                    }
                    self.call(3)?;
                }
                Some(0)
            }
            1 => {
                if self.call(0)? == 0 || self.call(1)? == 0 {
                    return Some(0);
                }
                self.call(2)?;
                if self.call(3)? == 0 {
                    return Some(0);
                }
                self.call(4)?;
                if self.call(5)? == 0 {
                    return Some(0);
                }
                let len = self.call(6)? as u32;
                if u64::from(len.wrapping_add(1)) > self.call(1)? {
                    return Some(0);
                }
                self.arg(7, u64::from(len.wrapping_add(1)))?;
                Some(1)
            }
            2 => {
                if self.call(0)? != 0 {
                    return Some(0);
                }
                let len = self.call(1)? as u32;
                if u64::from(len) > self.call(2)? {
                    return Some(0);
                }
                if self.arg(3, u64::from(len.wrapping_add(9)))? == 0 {
                    return Some(0);
                }
                self.call(4)?;
                self.call(5)?;
                self.call(6)
            }
            3 => {
                if self.call(0)? == 0 {
                    return Some(0);
                }
                self.call(1)?;
                if self.call(2)? != 0 {
                    self.call(3)?;
                }
                Some(0)
            }
            4 => {
                if self.call(0)? != 0 {
                    self.call(1)?;
                }
                Some(0)
            }
            5 => {
                self.call(0)?;
                if self.call(1)? == 0 || self.call(2)? == 0 || self.call(3)? == 0 {
                    return Some(0);
                }
                let first = self.call(4)?;
                self.arg(5, u64::from(first != 0))?;
                if first == 0 || self.call(6)? != 0 {
                    return Some(0);
                }
                self.call(7)?;
                self.call(8)?;
                self.call(9)?;
                Some(0)
            }
            6 => {
                self.call(0)?;
                if self.call(1)? == 0 || self.call(2)? == 0 || self.call(3)? == 0 {
                    return Some(0);
                }
                self.call(4)?;
                if self.call(5)? == 0 {
                    self.call(6)?;
                }
                Some(0)
            }
            7 => {
                self.call(0)?;
                if self.call(1)? == 0
                    || self.call(2)? == 0
                    || self.call(3)? == 0
                    || self.call(4)? == 0
                {
                    return Some(0);
                }
                self.call(5)?;
                if self.call(6)? == 0 {
                    self.call(7)?;
                }
                Some(0)
            }
            8 => {
                self.call(0)?;
                if self.call(1)? != 0 && self.call(2)? != 0 {
                    if self.call(3)? == 0 {
                        self.call(4)?;
                    }
                    self.call(5)
                } else {
                    Some(5)
                }
            }
            _ => Some(0),
        }
    }
}
/// # Safety
/// The synchronous callback contains native exceptions; `failed` is valid and
/// updated after failure. Neither it nor the native context is retained.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_config_flow(
    kind: u32,
    step: Step,
    context: *mut c_void,
    failed: *const u8,
) -> u64 {
    if failed.is_null() {
        return 0;
    }
    Flow {
        step,
        context,
        failed,
    }
    .run(kind)
    .unwrap_or(0)
}
