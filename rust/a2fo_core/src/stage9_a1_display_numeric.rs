//! A1 display validation/clamping, preserving x87 loads and CRT rounding.
fn finite(bits: u32) -> bool {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let maximum = 0x7f7fffffu32;
        let valid: u8;
        unsafe {
            core::arch::asm!("fld dword ptr [{v}]","fabs","fld dword ptr [{m}]",
        "fucomip st(0),st(1)","fstp st(0)","setae {r}",
        v=in(reg)&bits,m=in(reg)&maximum,r=out(reg_byte)valid,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        valid != 0
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        bits & 0x7f800000 != 0x7f800000
    }
}
fn positive(bits: u32) -> bool {
    bits & 0x80000000 == 0 && bits & 0x7fffffff != 0
}
/// # Safety
/// Output is one private writable u32 slot. No callback or foreign borrow occurs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_display_energy(
    current: u32,
    maximum: u32,
    output: *mut u32,
) -> u8 {
    // The finite tests load through x87, including signalling-NaN and denormal
    // events. Integer ordering then selects existing f32 words without rounding.
    if !finite(maximum) || !positive(maximum) || !finite(current) {
        return 0;
    }
    let selected = if current & 0x80000000 != 0 && current & 0x7fffffff != 0 {
        0
    } else if current & 0x80000000 == 0 && current > maximum {
        maximum
    } else {
        current
    };
    unsafe { output.write(selected) };
    1
}
/// # Safety
/// Words covers three initialized private u32 slots, stable for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_display_colour_finite(words: *const u32) -> u8 {
    // GNU x87 struct copying loads all three source components before the
    // short-circuit validation. Preserve the resulting denormal/signalling-NaN
    // events even when an earlier component is already non-finite. Disassembly:
    // fixture-v2-colour-disassembly.txt, 570c98..570cb3.
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!(
        "fld dword ptr [{p}]", "fld dword ptr [{p}+4]", "fstp st(0)",
        "fld dword ptr [{p}+8]", "fstp st(0)", "fstp st(0)",
        p=in(reg)words,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    for index in 0..3 {
        if !finite(unsafe { words.add(index).read() }) {
            return 0;
        }
    }
    1
}
fn to_double(bits: u32) -> u64 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let mut output = 0u64;
        unsafe {
            core::arch::asm!("fld dword ptr [{v}]","fstp qword ptr [{o}]",
        v=in(reg)&bits,o=in(reg)&mut output,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        output
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        (f32::from_bits(bits) as f64).to_bits()
    }
}
type Round = unsafe extern "C" fn(u64) -> i32;
/// # Safety
/// Output is one private writable i32 slot, disjoint from callback storage. The
/// synchronous callback uses the native CRT lround implementation and preserves
/// the native exception boundary. Mode0 is current crew, mode1 positive maximum.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_display_number(
    mode: u8,
    bits: u32,
    round: Round,
    output: *mut i32,
) -> u8 {
    if !finite(bits) || (mode != 0 && !positive(bits)) {
        return 0;
    }
    let prepared = if bits & 0x80000000 != 0 && bits & 0x7fffffff != 0 {
        0
    } else if bits & 0x80000000 == 0 && bits >= 0x4f000000 {
        0x41dfffffffc00000
    } else {
        to_double(bits)
    };
    let result = unsafe { round(prepared) };
    unsafe { output.write(result) };
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_display_nonnegative(value: i32) -> i32 {
    value.max(0)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_display_has_maximum(value: i32) -> u8 {
    u8::from(value > 0)
}
#[cfg(test)]
mod tests {
    use super::*;
    unsafe extern "C" fn round(bits: u64) -> i32 {
        f64::from_bits(bits).round() as i32
    }
    #[test]
    fn energy_word_selection() {
        let mut out = 99;
        for (c, m, want) in [
            (0x80000000, 1, 0x80000000),
            (0xbf800000, 1, 0),
            (0x40000000, 0x3f800000, 0x3f800000),
        ] {
            assert_eq!(
                unsafe { a2fo_rs_stage9_a1_display_energy(c, m, &mut out) },
                1
            );
            assert_eq!(out, want)
        }
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_display_energy(0, 0, &mut out) },
            0
        )
    }
    #[test]
    fn number_clamps_and_missing_max() {
        let mut out = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_display_number(0, 0x4f000000, round, &mut out) },
            1
        );
        assert_eq!(out, i32::MAX);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_display_number(1, 0, round, &mut out) },
            0
        );
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_display_number(0, 0x7fc00001, round, &mut out) },
            0
        );
    }
}
