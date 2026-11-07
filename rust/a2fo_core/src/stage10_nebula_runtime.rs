//! Nebula descriptor arithmetic. Windows kernels preserve the original x87
//! instruction order, precision/rounding controls, and status effects.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_nebula_suffix_before(left: usize, right: usize) -> u8 {
    u8::from(left > right)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_nebula_float_valid(found: usize, bits: u32) -> u8 {
    if found & 0xff == 0 {
        return 0;
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let maximum = 0x7f7fffffu32;
        let result: u8;
        unsafe {
            core::arch::asm!(
            "fld dword ptr [{v}]", "fabs", "fld dword ptr [{m}]",
            "fucomip st(0),st(1)", "fstp st(0)", "setae {r}",
            v=in(reg)&bits,m=in(reg)&maximum,r=out(reg_byte)result,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        u8::from(bits & 0x7f800000 != 0x7f800000)
    }
}

/// # Safety
/// Offset and size point to disjoint caller-owned arrays of three and two
/// writable f32 words respectively, valid for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_scale(
    offset: *mut f32,
    size: *mut f32,
    scale: u32,
) {
    let offset = offset.cast::<u32>();
    let size = size.cast::<u32>();
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!(
            "fld dword ptr [{v}]", "2:", "fld dword ptr [{p}]", "add {p},4",
            "fmul st(0),st(1)", "fstp dword ptr [{p}-4]", "cmp {p},{e}", "jne 2b",
            "fld dword ptr [{s}]", "fmul st(0),st(1)", "fstp dword ptr [{s}]",
            "fmul dword ptr [{s}+4]", "fstp dword ptr [{s}+4]",
            v=in(reg)&scale,p=inout(reg)offset=>_,e=in(reg)offset.add(3),s=in(reg)size,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let scale = f32::from_bits(scale);
        for (p, count) in [(offset, 3), (size, 2)] {
            for index in 0..count {
                let old = f32::from_bits(unsafe { p.add(index).read() });
                unsafe { p.add(index).write((old * scale).to_bits()) };
            }
        }
    }
}

/// # Safety
/// Words points to three initialized finite private colour components. The
/// native parser rejects non-finite values before this packing operation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_nebula_colour_key(words: *const u32) -> u32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let constants = [255.0f32.to_bits(), 0.5f32.to_bits()];
        let mut scratch = [0u8; 12];
        let result: u32;
        unsafe {
            core::arch::asm!(
            "fld dword ptr [{v}]", "fldz", "fcomi st(0),st(1)", "fxch st(1)",
            "fcmovnbe st(0),st(1)", "fld dword ptr [{v}+4]", "fxch st(2)",
            "fcomi st(0),st(2)", "fxch st(2)", "fcmovnbe st(0),st(2)",
            "fld dword ptr [{v}+8]", "fxch st(3)", "fnstcw word ptr [{t}+10]",
            "fcomi st(0),st(3)", "movzx eax,word ptr [{t}+10]", "fcmovbe st(0),st(3)",
            "fstp st(3)", "fld dword ptr [{c}]", "fxch st(2)",
            "fcomi st(0),st(2)", "fcmovnbe st(0),st(2)", "fld dword ptr [{c}+4]",
            "or ah,12", "mov word ptr [{t}+8],ax", "fadd st(1),st(0)", "fxch st(1)",
            "fldcw word ptr [{t}+8]", "fistp qword ptr [{t}]", "fldcw word ptr [{t}+10]",
            "fxch st(1)", "mov eax,dword ptr [{t}]", "shl eax,16",
            "fcomi st(0),st(2)", "fcmovnbe st(0),st(2)", "fadd st(0),st(1)",
            "fldcw word ptr [{t}+8]", "fistp qword ptr [{t}]", "fldcw word ptr [{t}+10]",
            "fxch st(2)", "mov edx,dword ptr [{t}]", "shl edx,8", "or eax,edx",
            "fcomi st(0),st(1)", "fcmovnbe st(0),st(1)", "fstp st(1)",
            "faddp st(1),st(0)", "fldcw word ptr [{t}+8]", "fistp qword ptr [{t}]",
            "fldcw word ptr [{t}+10]", "or eax,dword ptr [{t}]",
            v=in(reg)words,c=in(reg)constants.as_ptr(),t=in(reg)scratch.as_mut_ptr(),
            out("eax")result,out("edx")_,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let channel = |index| {
            let value = f32::from_bits(unsafe { words.add(index).read() });
            ((value.clamp(0.0, 255.0) as f64) + 0.5) as u32
        };
        (channel(0) << 16) | (channel(1) << 8) | channel(2)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_nebula_threshold(
    preview: u8,
    operation: u32,
    index: usize,
) -> u32 {
    if preview != 0 {
        0
    } else if operation == 8 {
        index as u32
    } else {
        index.wrapping_add(1) as u32
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn native_found_byte_controls_float_admission() {
        assert_eq!(a2fo_rs_stage10_nebula_float_valid(0x100, 1f32.to_bits()), 0);
        assert_eq!(a2fo_rs_stage10_nebula_float_valid(0x101, 1f32.to_bits()), 1);
        assert_eq!(
            a2fo_rs_stage10_nebula_float_valid(1, f32::INFINITY.to_bits()),
            0
        );
    }
    #[test]
    fn scale_fields_and_pack_channels() {
        let mut offset = [1f32.to_bits(), (-2f32).to_bits(), (-0.0f32).to_bits()];
        let mut size = [6f32.to_bits(), 3f32.to_bits()];
        unsafe {
            a2fo_rs_stage10_nebula_scale(
                offset.as_mut_ptr().cast(),
                size.as_mut_ptr().cast(),
                2f32.to_bits(),
            )
        };
        assert_eq!(
            offset,
            [2f32.to_bits(), (-4f32).to_bits(), (-0.0f32).to_bits()]
        );
        assert_eq!(size, [12f32.to_bits(), 6f32.to_bits()]);
        let c = [255f32.to_bits(), 127.5f32.to_bits(), (-1f32).to_bits()];
        assert_eq!(
            unsafe { a2fo_rs_stage10_nebula_colour_key(c.as_ptr()) },
            0xff8000
        );
    }
    #[test]
    fn threshold_selection_keeps_original_cast_and_wrap() {
        assert_eq!(a2fo_rs_stage10_nebula_threshold(1, 9, 99), 0);
        assert_eq!(a2fo_rs_stage10_nebula_threshold(0, 8, 99), 99);
        assert_eq!(a2fo_rs_stage10_nebula_threshold(0, 9, 99), 100);
        assert_eq!(a2fo_rs_stage10_nebula_threshold(0, 9, usize::MAX), 0);
    }
}
