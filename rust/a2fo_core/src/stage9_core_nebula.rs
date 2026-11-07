//! Core renderer control with synchronous native storage/COM/file primitives.
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
                if self.call(0)? == 0 {
                    return Some(0);
                }
                if self.call(1)? == 0 {
                    self.call(10)?;
                    return Some(0);
                }
                let mut equal =
                    self.call(2)? != 0 && self.call(3)? != 0 && self.call(4)? == self.call(5)?;
                while equal {
                    if self.call(6)? == 0
                        || self.call(7)? == 0
                        || self.call(8)? != self.call(9)?
                        || self.call(12)? != 0
                    {
                        equal = false;
                        break;
                    }
                    if self.call(8)? == 0 {
                        break;
                    }
                }
                self.call(11)?;
                self.call(10)?;
                Some(u64::from(equal))
            }
            2 => Some(u64::from(self.call(0)? != 0 && self.call(1)? != 0)),
            5 => {
                if self.call(0)? != 0 || self.call(1)? != 0 || self.call(2)? != 0 {
                    return Some(1);
                }
                if self.call(3)? == 0
                    || self.call(4)? == 0
                    || self.call(5)? != 0
                    || self.call(6)? == 0
                {
                    return Some(0);
                }
                if self.call(7)? == 0 {
                    return Some(0);
                }
                // The native adapter packs the original helper's argument
                // expressions together, preserving compiler evaluation order.
                let args = self.call(8)?;
                Some(u64::from(
                    args >> 32 != 0 && (1..=3).contains(&(args as u32)),
                ))
            }
            6 => {
                if self.call(0)? != 0 || self.call(1)? != 0 {
                    return Some(0);
                }
                let mut best = 0;
                self.call(2)?;
                while self.call(3)? != 0 {
                    let len = self.call(4)?;
                    if len != 0 && len < self.call(5)? && len > best && self.arg(6, len)? != 0 {
                        best = len;
                    }
                    self.call(7)?;
                }
                if best == 0 {
                    return Some(0);
                }
                self.arg(8, best)?;
                Some(1)
            }
            8 => {
                if self.call(0)? != 0 {
                    self.call(1)?;
                }
                let estimate = self.call(2)?;
                let cache = self.call(3)?;
                self.arg(4, cache.saturating_sub(estimate))?;
                self.call(5)?;
                Some(0)
            }
            9 => {
                self.call(0)?;
                while self.call(1)? != 0 {
                    self.call(2)?;
                    self.call(3)?;
                }
                self.call(4)?;
                Some(0)
            }
            10 | 11 => {
                if kind == 10 && self.call(0)? != 0 {
                    self.call(1)?;
                    self.call(2)?;
                }
                self.call(3)?;
                while self.call(4)? != 0 {
                    if self.call(5)? != 0 {
                        self.call(6)?;
                        while self.call(7)? != 0 {
                            if self.call(8)? != 0 {
                                if kind == 10 {
                                    self.call(9)?;
                                } else {
                                    if self.call(14)? != 0 {
                                        self.call(15)?;
                                    }
                                    self.call(16)?;
                                }
                            }
                            self.call(10)?;
                        }
                    }
                    self.call(11)?;
                }
                if kind == 10 {
                    self.call(12)?;
                    self.call(13)?;
                }
                Some(0)
            }
            12 => {
                if self.call(0)? != 0 {
                    return Some(0);
                }
                self.call(1)?;
                while self.call(2)? != 0 {
                    if self.call(3)? < self.call(4)? {
                        self.call(5)?;
                    }
                    self.call(6)?;
                }
                self.call(7)?;
                self.call(8)?;
                if self.call(9)? == 0 {
                    self.call(10)?;
                }
                Some(1)
            }
            13 => {
                while self.call(0)? > self.call(1)? {
                    let mut oldest = u64::MAX;
                    self.call(2)?;
                    self.call(3)?;
                    while self.call(4)? != 0 {
                        if self.call(5)? != 0 {
                            self.call(6)?;
                            while self.call(7)? != 0 {
                                if self.call(8)? != 0 && self.call(9)? > 1 {
                                    self.call(10)?;
                                    while self.call(11)? != 0 {
                                        let used = self.call(12)?;
                                        if used < oldest {
                                            oldest = used;
                                            self.call(13)?;
                                        }
                                        self.call(14)?;
                                    }
                                }
                                self.call(15)?;
                            }
                        }
                        self.call(16)?;
                    }
                    if self.call(17)? == 0 || self.call(18)? == 0 {
                        if self.call(19)? == 0 {
                            self.call(20)?;
                        }
                        break;
                    }
                }
                Some(0)
            }
            14 => {
                let cached = self.call(0)?;
                if cached != 0 {
                    return Some(cached);
                }
                if self.call(1)? == 0 {
                    return Some(0);
                }
                if self.call(2)? == 0 || self.call(3)? == 0 {
                    return Some(0);
                }
                if self.call(4)? == 0 || self.call(5)? == 0 {
                    self.call(6)?;
                    return Some(0);
                }
                self.call(7)?;
                self.call(8)?;
                self.call(9)?;
                self.call(10)
            }
            _ => Some(0),
        }
    }
}
/// # Safety
/// Native callbacks contain exceptions; failure storage is valid for the call.
/// Context/resources remain native and are not retained by Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_nebula_flow(
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
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_nebula_texture_bytes(mut width: u32, mut height: u32) -> u32 {
    let mut total = 0u32;
    while width != 0 && height != 0 {
        total = total.wrapping_add(width.wrapping_mul(height).wrapping_mul(4));
        if width == 1 && height == 1 {
            break;
        }
        width = (width / 2).max(1);
        height = (height / 2).max(1);
    }
    total
}
