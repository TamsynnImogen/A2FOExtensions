//! Sprite cache and draw-scope policy. Native adapters retain live reference
//! versus copied-pointer semantics, foreign texture writes and x87 kernels.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
struct Flow {
    step: Step,
    context: *mut c_void,
    failed: *const u8,
}
impl Flow {
    fn call(&self, op: u32) -> Option<usize> {
        let value = unsafe { (self.step)(self.context, op, 0) };
        (unsafe { self.failed.read() } == 0).then_some(value)
    }
    fn run(&self, kind: u32) -> Option<bool> {
        match kind {
            0 => {
                if self.call(0)? == 0 || self.call(1)? == 0 {
                    return Some(false);
                }
                if self.call(2)? == 0 {
                    self.call(3)?;
                }
                if self.call(4)? == 0 {
                    self.call(5)?;
                }
                if self.call(6)? != 0 && self.call(7)? == 0 {
                    self.call(8)?;
                    return Some(false);
                }
                if self.call(9)? == 0 && self.call(10)? == 0 {
                    self.call(11)?;
                    self.call(12)?;
                }
                if self.call(13)? == 0 {
                    if self.call(14)? == 0 {
                        self.call(15)?;
                    }
                    return Some(false);
                }
                self.call(16)?;
                for op in 17..=20 {
                    if self.call(op)? == 0 {
                        return Some(false);
                    }
                }
                if self.call(21)? == 0 || self.call(22)? == 0 {
                    if self.call(23)? == 0 {
                        self.call(24)?;
                    }
                    return Some(false);
                }
                self.call(25)?;
                self.call(26)?;
                if self.call(27)? == 0 {
                    return Some(false);
                }
                // Icon restore order is colour first, then the four texture-window values.
                for op in 28..=32 {
                    self.call(op)?;
                }
                Some(true)
            }
            1 => {
                self.call(0)?;
                if self.call(1)? == 0 || self.call(2)? == 0 {
                    self.call(3)?;
                }
                if self.call(4)? != 0 && self.call(5)? == 0 {
                    self.call(6)?;
                    return Some(false);
                }
                if self.call(7)? == 0 {
                    self.call(8)?;
                    self.call(9)?;
                    if self.call(10)? == 0 && self.call(11)? == 0 {
                        self.call(12)?;
                        self.call(13)?;
                    }
                }
                self.call(14)?;
                if self.call(15)? == 0 || self.call(16)? == 0 {
                    return Some(false);
                }
                self.call(17)?;
                if self.call(18)? == 0 {
                    return Some(false);
                }
                // Bar restore order is texture width first, then colour.
                for op in 19..=24 {
                    self.call(op)?;
                }
                Some(true)
            }
            2 => {
                if self.call(0)? == 0 || self.call(1)? == 0 {
                    return Some(false);
                }
                self.call(2)?;
                self.call(3)?;
                let track = self.call(4)? != 0;
                self.call(5)?;
                let foreground = self.call(6)? != 0 && self.call(7)? != 0;
                Some(track || foreground)
            }
            3 => {
                if self.call(0)? != 0 {
                    return Some(true);
                }
                self.call(1)?;
                if self.call(2)? == 0 || self.call(3)? == 0 {
                    return Some(false);
                }
                self.call(4)?;
                self.call(5)?;
                Some(true)
            }
            _ => Some(false),
        }
    }
}
/// # Safety
/// The synchronous native callback contains all exceptions, updates `failed`,
/// and owns every referenced cache, sprite, rectangle and restore value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_sprite_flow(
    kind: u32,
    step: Step,
    context: *mut c_void,
    failed: *const u8,
) -> u8 {
    if failed.is_null() {
        return 0;
    }
    u8::from(
        Flow {
            step,
            context,
            failed,
        }
        .run(kind)
        .unwrap_or(false),
    )
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct AnchorCandidate {
    component: usize,
    rectangle: [i32; 4],
    configured: *const i32,
}
fn usable(rect: [i32; 4]) -> bool {
    rect[2] > rect[0] && rect[3] > rect[1]
}
unsafe fn raw_rect(source: *const i32) -> [i32; 4] {
    let mut out = [0; 4];
    for (i, value) in out.iter_mut().enumerate() {
        *value = unsafe { source.add(i).read_unaligned() };
    }
    out
}
unsafe fn write_rect(target: *mut i32, rect: [i32; 4]) {
    for (i, value) in rect.into_iter().enumerate() {
        unsafe { target.add(i).write_unaligned(value) };
    }
}
/// # Safety
/// Candidate pointers address initialized C POD records and two fallbacks.
/// Configured rectangles have four readable i32 values when nonnull; output
/// has four writable values. This call has no foreign callbacks or allocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_anchor(
    captain: *const AnchorCandidate,
    configured: *const i32,
    fallbacks: *const AnchorCandidate,
    output: *mut i32,
) -> usize {
    let captain = unsafe { captain.read_unaligned() };
    if captain.component != 0 && usable(captain.rectangle) {
        unsafe { write_rect(output, captain.rectangle) };
        return captain.component;
    }
    for index in 0..2 {
        let candidate = unsafe { fallbacks.add(index).read_unaligned() };
        if candidate.component == 0 || !usable(candidate.rectangle) {
            continue;
        }
        if configured.is_null() {
            unsafe { write_rect(output, candidate.rectangle) };
            return candidate.component;
        }
        if candidate.configured.is_null() {
            continue;
        }
        let from = unsafe { raw_rect(candidate.configured) };
        let to = unsafe { raw_rect(configured) };
        let mut rect = candidate.rectangle;
        let dx = to[0].wrapping_sub(from[0]);
        let dy = to[1].wrapping_sub(from[1]);
        rect[0] = rect[0].wrapping_add(dx);
        rect[1] = rect[1].wrapping_add(dy);
        rect[2] = rect[2].wrapping_add(dx.wrapping_add(to[2]).wrapping_sub(from[2]));
        rect[3] = rect[3].wrapping_add(dy.wrapping_add(to[3]).wrapping_sub(from[3]));
        if usable(rect) {
            unsafe { write_rect(output, rect) };
            return candidate.component;
        }
    }
    0
}
/// # Safety
/// `source` and `output` each address four native i32 fields. Inputs are copied
/// before any output write; aliases are supported.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_craft_anchor_fallback(
    source: *const i32,
    offset: i32,
    output: *mut i32,
) {
    let raw = unsafe { raw_rect(source) };
    let top = raw[1].wrapping_sub(offset);
    unsafe {
        write_rect(
            output,
            [
                raw[0],
                top,
                raw[0].wrapping_add(raw[2].max(2)).wrapping_sub(1),
                top.wrapping_add(raw[3].max(2)).wrapping_sub(1),
            ],
        )
    };
}
