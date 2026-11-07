//! Windows numeric kernels reproduce the original GNU++17/O2 x87 instruction
//! order, spills and status effects. Native objects are borrowed as POD fields.

/// # Safety
/// Total and values point to two initialized disjoint native float words.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_values(total: *mut f32, values: *const f32) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        let max = 0x7f7fffffu32;
        core::arch::asm!(
            "fld dword ptr [{v}]","fld st(0)","fabs","fld dword ptr [{m}]",
            "fucomi st(0),st(1)","fstp st(1)","jb 3f",
            "fld dword ptr [{v}+4]","fld st(0)","fabs","fxch st(2)",
            "fucomip st(0),st(2)","fstp st(1)","jb 3f",
            "fldz","fxch st(1)","fcomi st(0),st(1)","jbe 4f",
            "fxch st(1)","fcomi st(0),st(2)","ja 5f",
            "fstp st(0)","fxch st(1)","fcomi st(0),st(1)",
            "fcmovnbe st(0),st(1)","fstp st(1)","jmp 6f",
            "5:","fstp st(1)","fstp st(1)",
            "6:","fadd dword ptr [{t}]","fstp dword ptr [{t}]",
            "fld dword ptr [{t}+4]","fadd dword ptr [{v}+4]","fstp dword ptr [{t}+4]","jmp 7f",
            "4:","fstp st(0)","3:","fstp st(0)","fstp st(0)","7:",
            t=in(reg)total,v=in(reg)values,m=in(reg)&max,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        let a = values.read();
        let b = values.add(1).read();
        if a.is_finite() && b.is_finite() && b > 0.0 {
            total.write(total.read() + a.clamp(0.0, b));
            total.add(1).write(total.add(1).read() + b);
        }
    }
}
/// Preserve the original hook's terminal x87 reload/spill, including C1.
/// # Safety
/// Output and total each point to two initialized, disjoint float words;
/// output is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_values_copy(
    output: *mut f32,
    total: *const f32,
) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!(
            "fld dword ptr [{t}+4]", "fld dword ptr [{t}]",
            "fstp dword ptr [{o}]", "fstp dword ptr [{o}+4]",
            t=in(reg)total, o=in(reg)output,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        core::ptr::copy_nonoverlapping(total, output, 2);
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_squadrons_weight(bits: u32) -> u32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let max = 0x7f7fffffu32;
        let mut result = 0u32;
        unsafe {
            core::arch::asm!(
            "fld dword ptr [{v}]","fld st(0)","fabs","fld dword ptr [{m}]",
            "fucomip st(0),st(1)","fstp st(0)","fld1","jb 3f",
            "fldz","fucomip st(0),st(2)","fcmovb st(0),st(1)","fstp st(1)","jmp 4f",
            "3:","fstp st(1)","4:","fstp dword ptr [{r}]",
            v=in(reg)&bits,m=in(reg)&max,r=in(reg)&mut result,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let weight = f32::from_bits(bits);
        if !weight.is_finite() || weight <= 0.0 {
            1f32.to_bits()
        } else {
            bits
        }
    }
}
/// # Safety
/// Total and colour each have three initialized native floats; sum has one.
/// All are disjoint; total and sum are writable, weight is a sanitized word.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_colour(
    total: *mut f32,
    sum: *mut f32,
    colour: *const f32,
    weight: u32,
) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        let max = 0x7f7fffffu32;
        core::arch::asm!(
            "fld dword ptr [{w}]","fld dword ptr [{v}]","fabs","fld dword ptr [{m}]",
            "fucomi st(0),st(1)","fstp st(1)","jb 3f",
            "fld dword ptr [{v}+4]","fabs","fxch st(1)",
            "fucomi st(0),st(1)","fstp st(1)","jb 3f",
            "fld dword ptr [{v}+8]","fabs","fxch st(1)",
            "fucomip st(0),st(1)","fstp st(0)","jb 4f",
            "xor eax,eax","2:","fld dword ptr [{v}+eax*4]","fmul st(0),st(1)",
            "fadd dword ptr [{t}+eax*4]","fstp dword ptr [{t}+eax*4]",
            "add eax,1","cmp eax,3","jne 2b",
            "fadd dword ptr [{s}]","fstp dword ptr [{s}]","jmp 5f",
            "3:","fstp st(0)","4:","fstp st(0)","5:",
            t=in(reg)total,s=in(reg)sum,v=in(reg)colour,w=in(reg)&weight,m=in(reg)&max,
            out("eax")_,out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        if (0..3).any(|i| !colour.add(i).read().is_finite()) {
            return;
        }
        let weight = f32::from_bits(weight);
        for i in 0..3 {
            total
                .add(i)
                .write(total.add(i).read() + colour.add(i).read() * weight);
        }
        sum.write(sum.read() + weight);
    }
}
/// # Safety
/// Total holds three writable initialized floats; weight is a disjoint float.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_divide(total: *mut f32, weight: *const f32) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!(
        "fldz","fld dword ptr [{w}]","fcomi st(0),st(1)","fstp st(1)","jbe 3f",
        "fld dword ptr [{t}]","fdiv st(0),st(1)","fstp dword ptr [{t}]",
        "fld dword ptr [{t}+4]","fdiv st(0),st(1)","fstp dword ptr [{t}+4]",
        "fdivr dword ptr [{t}+8]","fstp dword ptr [{t}+8]","jmp 4f",
        "3:","fstp st(0)","4:",
        t=in(reg)total,w=in(reg)weight,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        let weight = weight.read();
        if weight > 0.0 {
            for i in 0..3 {
                total.add(i).write(total.add(i).read() / weight);
            }
        }
    }
}
/// # Safety
/// Distance is one writable initialized native float. Inputs are native words.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_axis(distance: *mut f32, craft: u32, yard: u32) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!(
        "fld dword ptr [{y}]","fsubr dword ptr [{c}]","fmul st(0),st(0)",
        "fadd dword ptr [{d}]","fstp dword ptr [{d}]",
        d=in(reg)distance,c=in(reg)&craft,y=in(reg)&yard,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        let delta = f32::from_bits(craft) - f32::from_bits(yard);
        distance.write(distance.read() + delta * delta);
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_squadrons_near(distance: u32, radius: u32) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let constants = [0x7f7fffffu32, 512f32.to_bits()];
        let result: u8;
        unsafe {
            core::arch::asm!(
            "fld dword ptr [{d}]","fld dword ptr [{r}]","fld st(0)","fabs",
            "fld dword ptr [{c}]","fucomip st(0),st(1)","fstp st(0)",
            "fld dword ptr [{c}+4]","jb 3f",
            "fldz","fcomi st(0),st(2)","fcmovbe st(0),st(2)","fstp st(2)","faddp st(1),st(0)","jmp 4f",
            "3:","fstp st(1)","4:","fld st(1)","fabs","fld dword ptr [{c}]",
            "fucomip st(0),st(1)","fstp st(0)","jb 5f",
            "fmul st(0),st(0)","fcomip st(0),st(1)","fstp st(0)","setae {v}","jmp 6f",
            "5:","fstp st(0)","fstp st(0)","mov {v},0","6:",
            d=in(reg)&distance,r=in(reg)&radius,c=in(reg)constants.as_ptr(),v=out(reg_byte)result,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let d = f32::from_bits(distance);
        let r = f32::from_bits(radius);
        let limit = 512.0 + if r.is_finite() { r.max(0.0) } else { 0.0 };
        u8::from(d.is_finite() && d <= limit * limit)
    }
}
/// # Safety
/// Remaining and seconds are disjoint initialized native floats, first writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_squadrons_retry(remaining: *mut f32, seconds: *const f32) {
    unsafe { crate::stage10_energy_runtime::a2fo_rs_stage10_energy_age(remaining, seconds) };
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_squadrons_retry_pending(bits: u32) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let result: u8;
        unsafe {
            core::arch::asm!("fldz","fld dword ptr [{v}]","fcomip st(0),st(1)","fstp st(0)","seta {r}",
            v=in(reg)&bits,r=out(reg_byte)result,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        u8::from(f32::from_bits(bits) > 0.0)
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn values_clamp_and_invalid_samples() {
        let mut total = [0f32, 0.0];
        unsafe { a2fo_rs_stage10_squadrons_values(total.as_mut_ptr(), [12f32, 10.0].as_ptr()) };
        assert_eq!(total, [10., 10.]);
        unsafe { a2fo_rs_stage10_squadrons_values(total.as_mut_ptr(), [f32::NAN, 10.0].as_ptr()) };
        assert_eq!(total, [10., 10.]);
    }
    #[test]
    fn weighted_colours_and_zero_weight() {
        let mut total = [0f32; 3];
        let mut weight = 0f32;
        unsafe {
            a2fo_rs_stage10_squadrons_colour(
                total.as_mut_ptr(),
                &mut weight,
                [0.2f32, 0.4, 0.6].as_ptr(),
                2f32.to_bits(),
            );
            a2fo_rs_stage10_squadrons_divide(total.as_mut_ptr(), &weight)
        };
        assert_eq!(total, [0.2, 0.4, 0.6]);
        assert_eq!(weight, 2.0);
        assert_eq!(
            a2fo_rs_stage10_squadrons_weight(f32::NAN.to_bits()),
            1f32.to_bits()
        );
    }
    #[test]
    fn distance_boundary_and_retry() {
        let mut distance = 0f32;
        unsafe { a2fo_rs_stage10_squadrons_axis(&mut distance, 512f32.to_bits(), 0) };
        assert_eq!(a2fo_rs_stage10_squadrons_near(distance.to_bits(), 0), 1);
        assert_eq!(a2fo_rs_stage10_squadrons_near(f32::NAN.to_bits(), 0), 0);
        let mut left = 0.5;
        let dt = 0.25;
        unsafe { a2fo_rs_stage10_squadrons_retry(&mut left, &dt) };
        assert_eq!(left, 0.25);
        assert_eq!(a2fo_rs_stage10_squadrons_retry_pending(left.to_bits()), 1);
    }
}
