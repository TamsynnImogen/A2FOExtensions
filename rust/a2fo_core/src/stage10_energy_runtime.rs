//! Residual ammunition presentation and timer policy. Native object/storage
//! access stays in C++; Windows numeric kernels preserve GNU++17/O2 x87 order.

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_energy_whole(bits: u32) -> i32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let constants = [0x7f7fffffu32, 0x4f000000, 0x38d1b717];
        let mut scratch = [0u8; 8];
        let result: i32;
        // Balanced x87 stack; both temporary control words and the caller's
        // precision/rounding mode match the native finite/positive path.
        unsafe {
            core::arch::asm!(
                "fld dword ptr [{v}]", "fld st(0)", "fabs",
                "fld dword ptr [{c}]", "fucomip st(0),st(1)", "fstp st(0)",
                "jb 3f", "fldz", "fcomip st(0),st(1)", "jae 3f",
                "fnstcw word ptr [{t}+6]", "fld dword ptr [{c}+4]", "fxch st(1)",
                "movzx eax,word ptr [{t}+6]", "fcomi st(0),st(1)",
                "fcmovnbe st(0),st(1)", "fstp st(1)", "fadd dword ptr [{c}+8]",
                "or ah,12", "mov word ptr [{t}+4],ax", "fldcw word ptr [{t}+4]",
                "frndint", "fldcw word ptr [{t}+6]", "fldcw word ptr [{t}+4]",
                "fistp dword ptr [{t}]", "fldcw word ptr [{t}+6]",
                "mov eax,dword ptr [{t}]", "jmp 4f",
                "3:", "fstp st(0)", "xor eax,eax", "4:",
                v=in(reg)&bits,c=in(reg)constants.as_ptr(),t=in(reg)scratch.as_mut_ptr(),
                out("eax")result,
                out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
                out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,
                options(nostack)
            );
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let value = f32::from_bits(bits);
        if !value.is_finite() || value <= 0.0 {
            return 0;
        }
        let value = (value.min(2147483648.0) as f64) + (0.0001f32 as f64);
        if value >= 2147483648.0 {
            i32::MIN
        } else {
            value as i32
        }
    }
}

/// # Safety
/// Colour points to three initialized native float words for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_energy_colour_valid(colour: *const f32) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let maximum = 0x7f7fffffu32;
        let result: u8;
        unsafe {
            core::arch::asm!(
                "fld dword ptr [{v}]", "fabs", "fld dword ptr [{m}]",
                "fucomi st(0),st(1)", "fstp st(1)", "jb 3f",
                "fld dword ptr [{v}+4]", "fabs", "fxch st(1)",
                "fucomi st(0),st(1)", "fstp st(1)", "jb 3f",
                "fld dword ptr [{v}+8]", "fabs", "fxch st(1)",
                "fucomip st(0),st(1)", "fstp st(0)", "setae {r}", "jmp 4f",
                "3:", "fstp st(0)", "mov {r},0", "4:",
                v=in(reg)colour,m=in(reg)&maximum,r=out(reg_byte)result,
                out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
                out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack)
            );
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        for index in 0..3 {
            if !unsafe { colour.add(index).read() }.is_finite() {
                return 0;
            }
        }
        1
    }
}

/// # Safety
/// Rectangle points to four initialized i32 fields: left, top, right, bottom.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_energy_rectangle_usable(rectangle: *const i32) -> u8 {
    unsafe {
        u8::from(
            rectangle.add(2).read() > rectangle.read()
                && rectangle.add(3).read() > rectangle.add(1).read(),
        )
    }
}

/// # Safety
/// The three pointers each address four initialized i32 fields; rectangle is
/// writable. Anchor and target use x, y, width, height fields. Native callers
/// use disjoint objects, with rectangle already copied from the live anchor.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_energy_translate(
    rectangle: *mut i32,
    anchor: *const i32,
    target: *const i32,
) {
    unsafe {
        for index in 0..4 {
            let axis = index & 1;
            let mut delta = target
                .add(axis)
                .read()
                .wrapping_sub(anchor.add(axis).read());
            if index >= 2 {
                delta = delta
                    .wrapping_add(target.add(index).read())
                    .wrapping_sub(anchor.add(index).read());
            }
            rectangle
                .add(index)
                .write(rectangle.add(index).read().wrapping_add(delta));
        }
    }
}

/// # Safety
/// Rectangles addresses twelve writable i32 fields, anchor four initialized
/// disjoint fields. Each output rectangle receives the native fallback offset.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_energy_default_rectangles(
    rectangles: *mut i32,
    anchor: *const i32,
) {
    unsafe {
        for index in 0..3 {
            for field in 0..4 {
                let value = anchor.add(field).read();
                let delta = if field & 1 == 1 {
                    56 + index as i32 * 28
                } else {
                    0
                };
                rectangles
                    .add(index * 4 + field)
                    .write(value.wrapping_add(delta));
            }
        }
    }
}

/// # Safety
/// Amount is an initialized writable native float field.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_energy_add(amount: *mut f32, current: u32) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!("fld dword ptr [{v}]", "fadd dword ptr [{c}]", "fstp dword ptr [{v}]",
            v=in(reg)amount,c=in(reg)&current,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        amount.write(f32::from_bits(current) + amount.read());
    }
}

/// # Safety
/// Remaining is a writable initialized native float, elapsed a disjoint float.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_energy_age(remaining: *mut f32, elapsed: *const f32) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!("fld dword ptr [{v}]", "fsub dword ptr [{e}]", "fstp dword ptr [{v}]",
            v=in(reg)remaining,e=in(reg)elapsed,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        remaining.write(remaining.read() - elapsed.read());
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_energy_active(bits: u32) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let result: u8;
        unsafe {
            core::arch::asm!("fld dword ptr [{v}]", "fldz", "fcomip st(0),st(1)", "fstp st(0)", "setb {r}",
                v=in(reg)&bits,r=out(reg_byte)result,
                out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
                out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        u8::from(f32::from_bits(bits).is_nan() || f32::from_bits(bits) > 0.0)
    }
}

/// # Safety
/// Data is a writable initialized string byte range of size bytes. Native
/// std::string retains its allocation and performs the final erase afterward.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_energy_unquote(data: *mut u8, size: usize) -> usize {
    let mut first = 0;
    while first < size && unsafe { data.add(first).read() } != b'"' {
        first += 1;
    }
    if first == size {
        return size;
    }
    for next in first + 1..size {
        let value = unsafe { data.add(next).read() };
        if value != b'"' {
            unsafe {
                data.add(first).write(value);
            }
            first += 1;
        }
    }
    first
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn whole_amount_edges() {
        for value in [f32::NAN, f32::INFINITY, -1.0, -0.0, 0.0] {
            assert_eq!(a2fo_rs_stage10_energy_whole(value.to_bits()), 0);
        }
        assert_eq!(a2fo_rs_stage10_energy_whole(1.99999f32.to_bits()), 2);
        assert_eq!(a2fo_rs_stage10_energy_whole(1.5f32.to_bits()), 1);
        assert_eq!(
            a2fo_rs_stage10_energy_whole(2147483648f32.to_bits()),
            i32::MIN
        );
    }
    #[test]
    fn rectangle_layout_and_translation() {
        let a = [10, 20, 110, 40];
        let c = [2, 3, 100, 20];
        let t = [4, 7, 80, 10];
        let mut r = a;
        unsafe {
            a2fo_rs_stage10_energy_translate(r.as_mut_ptr(), c.as_ptr(), t.as_ptr());
        }
        assert_eq!(r, [12, 24, 92, 34]);
        assert_eq!(
            unsafe { a2fo_rs_stage10_energy_rectangle_usable(r.as_ptr()) },
            1
        );
        let mut defaults = [0; 12];
        unsafe {
            a2fo_rs_stage10_energy_default_rectangles(defaults.as_mut_ptr(), a.as_ptr());
        }
        assert_eq!(
            defaults,
            [10, 76, 110, 96, 10, 104, 110, 124, 10, 132, 110, 152]
        );
    }
    #[test]
    fn quote_compaction_preserves_embedded_nul_and_tail() {
        let mut s = *b"\"a\0\"b\"";
        let n = unsafe { a2fo_rs_stage10_energy_unquote(s.as_mut_ptr(), s.len()) };
        assert_eq!(n, 3);
        assert_eq!(&s[..n], b"a\0b");
        assert_eq!(
            unsafe { a2fo_rs_stage10_energy_unquote(core::ptr::null_mut(), 0) },
            0
        );
    }
    #[test]
    fn timer_and_colour_policy() {
        let mut remaining = 0.5;
        let elapsed = 0.25;
        unsafe {
            a2fo_rs_stage10_energy_age(&mut remaining, &elapsed);
        }
        assert_eq!(remaining, 0.25);
        assert_eq!(a2fo_rs_stage10_energy_active(f32::NAN.to_bits()), 1);
        assert_eq!(a2fo_rs_stage10_energy_active((-0f32).to_bits()), 0);
        unsafe {
            a2fo_rs_stage10_energy_add(&mut remaining, 0.5f32.to_bits());
        }
        assert_eq!(remaining, 0.75);
        assert_eq!(
            unsafe { a2fo_rs_stage10_energy_colour_valid([1f32, 2., 3.].as_ptr()) },
            1
        );
        assert_eq!(
            unsafe { a2fo_rs_stage10_energy_colour_valid([1f32, f32::NAN, 3.].as_ptr()) },
            0
        );
    }
}
