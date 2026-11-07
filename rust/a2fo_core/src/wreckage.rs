//! Seeded replacement selection. Preserve the current CRT strtof spelling,
//! locale and embedded-NUL behavior; replacing CRT parsing needs its own proof.
unsafe extern "C" {
    fn strtof(text: *const core::ffi::c_char, end: *mut *mut core::ffi::c_char) -> f32;
}
fn trim(value: &[u8]) -> (usize, &[u8]) {
    let is_space = |c: &u8| matches!(c, b' ' | b'\t' | b'\r' | b'\n');
    let begin = value
        .iter()
        .position(|c| !is_space(c))
        .unwrap_or(value.len());
    let end = value
        .iter()
        .rposition(|c| !is_space(c))
        .map_or(begin, |i| i + 1);
    (begin, &value[begin..end])
}
fn chance(text: &[u8]) -> Option<f32> {
    let (_, text) = trim(text);
    if text.is_empty() || text.len() >= 64 {
        return None;
    }
    let mut buffer = [0_u8; 64];
    buffer[..text.len()].copy_from_slice(text);
    let mut end = core::ptr::null_mut();
    // SAFETY: buffer is NUL terminated; the CRT borrows it synchronously and
    // writes its end pointer into a live local pointer slot.
    let parsed = unsafe { strtof(buffer.as_ptr().cast(), &mut end) };
    if end.is_null() || core::ptr::eq(end, buffer.as_ptr().cast()) {
        return None;
    }
    // SAFETY: strtof returns a pointer into this terminated buffer.
    if unsafe { *end } != 0 || !parsed.is_finite() || !(0.0..=100.0).contains(&parsed) {
        None
    } else {
        Some(parsed)
    }
}
fn mix(mut value: u32) -> u32 {
    value ^= value >> 16;
    value = value.wrapping_mul(0x7feb352d);
    value ^= value >> 15;
    value = value.wrapping_mul(0x846ca68b);
    value ^ (value >> 16)
}
fn decide(wreckage: &[u8], present: bool, text: &[u8], seed: u32) -> (u32, usize, usize) {
    let (offset, name) = trim(wreckage);
    if name.is_empty() {
        return (0, 0, 0);
    }
    let probability = if present {
        match chance(text) {
            Some(p) => p,
            None => return (2, 0, 0),
        }
    } else {
        100.0
    };
    if roll_below(mix(seed), probability) {
        (1, offset, name.len())
    } else {
        (0, 0, 0)
    }
}
unsafe fn bytes<'a>(p: *const u8, len: usize) -> Option<&'a [u8]> {
    if len == 0 {
        Some(&[])
    } else if p.is_null() || len > isize::MAX as usize {
        None
    } else {
        Some(unsafe { core::slice::from_raw_parts(p, len) })
    }
}
// Safety: input spans must be readable; out slots, when non-null, writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_wreckage_decide(
    w: *const u8,
    wl: usize,
    present: u8,
    c: *const u8,
    cl: usize,
    seed: u32,
    offset: *mut usize,
    length: *mut usize,
) -> u32 {
    let result = match (unsafe { bytes(w, wl) }, unsafe { bytes(c, cl) }) {
        (Some(w), Some(c)) => decide(w, present != 0, c, seed),
        _ => (0, 0, 0),
    };
    if !offset.is_null() {
        unsafe { offset.write(result.1) }
    }
    if !length.is_null() {
        unsafe { length.write(result.2) }
    }
    result.0
}

#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn roll_below(mixed: u32, chance: f32) -> bool {
    mixed as f64 * (100.0 / 4294967296.0) < chance as f64
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn roll_below(mixed: u32, chance: f32) -> bool {
    let integer = mixed as u64;
    let scale = 100.0_f64 / 4294967296.0_f64;
    let allowed: u8;
    // SAFETY: local slots remain live; FILD/FMULT then FLD create two stack
    // entries, FUCOMIP/FSTP remove both. All x87 registers are clobbered.
    // This preserves the game's current precision, including 24-bit mode.
    unsafe {
        core::arch::asm!(
            "fild qword ptr [{integer}]", "fmul qword ptr [{scale}]",
            "fld dword ptr [{chance}]", "fucomip st(0), st(1)",
            "fstp st(0)", "seta {allowed}",
            integer=in(reg) &integer, scale=in(reg) &scale,
            chance=in(reg) &chance, allowed=out(reg_byte) allowed,
            out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
            out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
            options(nostack)
        );
    }
    allowed != 0
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn defaults_invalid_and_borrowed_trim() {
        assert_eq!(decide(b" \tfoo.odf\r\n", false, b"", u32::MAX), (1, 2, 7));
        assert_eq!(decide(b"foo", true, b"0", 42), (0, 0, 0));
        assert_eq!(decide(b"foo", true, b"NaN", 42), (2, 0, 0));
        assert_eq!(decide(b" ", true, b"NaN", 42), (0, 0, 0));
        assert_eq!(chance(b"100\0junk"), Some(100.0));
        assert_eq!(chance(&[b'1'; 64]), None);
        assert_eq!(chance(b"0x1.9p6"), Some(100.0));
    }
    #[test]
    fn deterministic_seed_boundaries() {
        assert_eq!(mix(0), 0);
        assert_eq!(mix(1), 0x688990c0);
        assert_eq!(decide(b"x", true, b"0", 0), (0, 0, 0));
        assert_eq!(decide(b"x", true, b"0.000001", 0), (1, 0, 1));
        for seed in 0..1000 {
            assert_eq!(decide(b"x", true, b"100", seed), (1, 0, 1));
        }
    }
}
