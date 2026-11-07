//! Shell display policy. All USER32 calls, foreign layouts, diagnostics and C++
//! exception objects stay in the synchronous native bridge. No HWND is operated
//! by Rust directly, and no reference is kept across a callback.
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
                if self.call(0)? == 0 || self.call(1)? == 0 {
                    return Some(0);
                }
                if self.call(2)? != self.call(3)? {
                    return Some(0);
                }
                if (self.call(4)? as i64) <= 0 {
                    return Some(0);
                }
                Some(u64::from(self.call(5)? == 0))
            }
            1 => {
                if self.call(0)? == 0 || self.call(1)? == 0 || self.call(2)? == 0 {
                    return Some(1);
                }
                let left = self.call(3)? as i32;
                let right = self.call(4)? as i32;
                let top = self.call(5)? as i32;
                let bottom = self.call(6)? as i32;
                let width = right.wrapping_sub(left);
                let height = bottom.wrapping_sub(top);
                if width <= 0 || height <= 0 {
                    return Some(1);
                }
                let mut score = (width as u64) * (height as u64);
                if self.call(7)? == 0 {
                    score += 1 << 48;
                }
                if self.call(8)? != 0 {
                    score += 1 << 52;
                }
                if self.call(9)? == 0 || score > self.call(10)? {
                    self.arg(11, score)?;
                }
                Some(1)
            }
            2 => {
                self.call(0)?;
                self.call(1)?;
                if self.call(2)? != 0 {
                    return self.call(3);
                }
                self.call(4)?;
                self.call(5)
            }
            3 => {
                if self.call(0)? == 0 {
                    return Some(0);
                }
                let old = self.call(1)?;
                if old == self.call(2)? {
                    self.call(3)?;
                    Some(0)
                } else {
                    Some(1)
                }
            }
            4 => {
                self.call(0)?;
                let monitor = self.call(1)?;
                if monitor != 0 {
                    Some(monitor)
                } else {
                    self.call(2)
                }
            }
            5 => {
                for op in 0..5 {
                    self.call(op)?;
                }
                Some(0)
            }
            6 | 7 => {
                self.call(0)?;
                if self.call(1)? != self.call(2)? && self.call(3)? == 0 {
                    return Some(0);
                }
                if self.call(4)? != self.call(5)? && self.call(6)? == 0 {
                    return Some(0);
                }
                if kind == 7 {
                    if self.call(7)? == 0 {
                        return Some(0);
                    }
                    self.call(8)?;
                }
                if self.call(9)? == 0 {
                    return Some(0);
                }
                if self.call(10)? == 0 && self.call(11)? == 0 {
                    return Some(0);
                }
                if self.call(12)? == 0 {
                    self.call(13)?;
                }
                Some(1)
            }
            8 => {
                if self.call(0)? == 0 {
                    return Some(0);
                }
                if self.call(1)? == 0 || self.call(2)? == 0 {
                    self.call(3)?;
                    self.call(4)?;
                }
                if self.call(5)? == 0 || self.call(6)? == 0 {
                    return Some(0);
                }
                if self.call(7)? == 0 || self.call(8)? == 0 {
                    self.call(9)?;
                    if (self.call(10)? as i32) < 4 {
                        self.call(11)?;
                    }
                }
                if self.call(12)? == 0 || self.call(13)? == 0 {
                    return Some(0);
                }
                let applied = if self.call(14)? != 0 {
                    self.call(15)?
                } else {
                    self.call(16)?
                };
                if applied != 0 {
                    self.call(17)?;
                } else if self.call(18)? == 0 {
                    self.call(19)?;
                    self.call(20)?;
                }
                Some(0)
            }
            9 => {
                self.call(0)?;
                loop {
                    if self.call(1)? == 0 {
                        self.call(2)?;
                    }
                    self.call(3)?;
                }
            }
            10 => {
                self.call(0)?;
                Some(u64::from(self.call(1)? != 0 || self.call(2)? == 0))
            }
            11 => {
                if self.call(0)? == self.call(1)? {
                    self.call(2)?;
                    self.call(3)?;
                }
                Some(1)
            }
            12 => {
                self.call(0)?;
                self.call(1)?;
                Some(0)
            }
            13 => {
                self.call(0)?;
                Some(1)
            }
            _ => Some(0),
        }
    }
}
/// # Safety
/// `failed` remains valid for the call; callback writes it on exception and
/// never unwinds. Native ownership and all pointed-to data remain native.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_shell_flow(
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
pub extern "C" fn a2fo_rs_stage9_shell_settings_valid(display: i32, width: i32, height: i32) -> u8 {
    u8::from(
        (0..=31).contains(&display)
            && (0..=16384).contains(&width)
            && (0..=16384).contains(&height),
    )
}
/// # Safety
/// Each pointer addresses four readable i32 values for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_shell_rect_equal(
    left: *const i32,
    right: *const i32,
) -> u8 {
    for i in 0..4 {
        if unsafe { left.add(i).read_unaligned() != right.add(i).read_unaligned() } {
            return 0;
        }
    }
    1
}
/// # Safety
/// `masks` addresses eight u32 Win32 style constants, in the header-defined order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_shell_styles(
    windowed: u8,
    mut style: u32,
    mut ex: u32,
    masks: *const u32,
) -> u64 {
    let mut m = [0; 8];
    for (i, value) in m.iter_mut().enumerate() {
        *value = unsafe { masks.add(i).read_unaligned() };
    }
    if windowed == 0 {
        style = (style & !(m[0] | m[1])) | m[2] | m[3];
        ex &= !m[7];
    } else if style & m[4] == 0 {
        style = (style & !(m[2] | m[1])) | m[0] | m[3];
    }
    u64::from(style) | (u64::from(ex) << 32)
}
/// # Safety
/// `outer` and `work` address four readable coordinates; `target` addresses six
/// writable i32s. All inputs are copied before output is written (alias-safe).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_shell_center(
    outer: *const i32,
    work: *const i32,
    target: *mut i32,
) {
    let mut o = [0i32; 4];
    let mut w = [0i32; 4];
    for i in 0..4 {
        o[i] = unsafe { outer.add(i).read_unaligned() };
        w[i] = unsafe { work.add(i).read_unaligned() };
    }
    let width = o[2].wrapping_sub(o[0]);
    let height = o[3].wrapping_sub(o[1]);
    let left = w[0].wrapping_add(w[2].wrapping_sub(w[0]).wrapping_sub(width) / 2);
    let top = w[1].wrapping_add(w[3].wrapping_sub(w[1]).wrapping_sub(height) / 2);
    for (i, value) in [
        left,
        top,
        left.wrapping_add(width),
        top.wrapping_add(height),
        width,
        height,
    ]
    .into_iter()
    .enumerate()
    {
        unsafe { target.add(i).write_unaligned(value) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_shell_client(width: i32, height: i32) -> u64 {
    let width = if width >= 640 { width } else { 1200 };
    let height = if height >= 480 { height } else { 900 };
    (width as u32 as u64) | ((height as u32 as u64) << 32)
}
