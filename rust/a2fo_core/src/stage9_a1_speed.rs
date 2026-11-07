//! A1 speed workflow and the observed GNU i686 x87 comparisons/max kernel.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u64;
fn positive(found: u64, bits: u32) -> bool {
    if found & 255 == 0 {
        return false;
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let max = 0x7f7f_ffffu32;
        let admitted: u8;
        unsafe {
            core::arch::asm!(
   "fld dword ptr [{v}]","fld st(0)","fabs","fld dword ptr [{m}]",
   "fucomip st(0),st(1)","fstp st(0)","jb 2f",
   "fldz","fxch st(1)","fcomip st(0),st(1)","fstp st(0)","seta {a}","jmp 3f",
   "2:","fstp st(0)","mov {a},0","3:",v=in(reg)&bits,m=in(reg)&max,a=out(reg_byte)admitted,
   out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        admitted != 0
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let v = f32::from_bits(bits);
        v.is_finite() && v > 0.0
    }
}
fn half_max(base: u32, warp: u32) -> u32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let mut values = [warp, base, 0x3f00_0000u32, 0];
        let mut result = base;
        unsafe {
            core::arch::asm!(
   "fld dword ptr [{p}+8]","fmul dword ptr [{p}]","fld dword ptr [{p}+4]","fxch st(1)",
   "fcomi st(0),st(1)","fstp st(1)","fstp dword ptr [{p}+12]","cmova {r},dword ptr [{p}+12]",
   p=in(reg)values.as_mut_ptr(),r=inout(reg)result,
   out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let b = f32::from_bits(base);
        let half = f32::from_bits(warp) * 0.5;
        if b < half { half.to_bits() } else { base }
    }
}
/// # Safety
/// Native context/callback remain live; callback preserves native call order,
/// validates pointer accesses and terminates at its original noexcept boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_speed(context: *mut c_void, step: Step) -> usize {
    let call = |op, arg| unsafe { step(context, op, arg) };
    let found = call(0, 0) as usize;
    if call(1, 0) == 0 {
        return found;
    }
    let combat = call(2, 0) != 0;
    let impulse = call(3, 0) != 0;
    let warp = call(4, 0) != 0;
    if !combat && !impulse && !warp {
        return found;
    }
    if !combat {
        if call(5, 0) & 255 != 0 {
            return found;
        }
    } else if found & 255 != 0 {
        return found;
    }
    let impulse_found = call(6, 0);
    let warp_found = call(7, 0);
    let impulse_bits = call(8, 0) as u32;
    let has_impulse = positive(impulse_found, impulse_bits);
    let warp_bits = call(9, 0) as u32;
    let has_warp = positive(warp_found, warp_bits);
    if call(10, 0) == 0 {
        return found;
    }
    let kind = if combat {
        0
    } else if impulse {
        1
    } else {
        2
    };
    let translated = match kind {
        0 => {
            if !has_impulse && !has_warp {
                return found;
            }
            let base = if has_impulse { impulse_bits } else { 0 };
            if has_warp {
                half_max(base, warp_bits)
            } else {
                base
            }
        }
        1 => {
            if !has_warp {
                return found;
            }
            warp_bits
        }
        _ => 0,
    };
    call(11, translated as usize);
    let pass = call(12, kind) as u32 as i32;
    if pass <= 64 {
        call(13, pass as u32 as usize);
    } else if pass == 65 {
        call(14, 0);
    }
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_speed_kind(kind: u32) -> *const core::ffi::c_char {
    match kind {
        0 => c"combat-speed default".as_ptr(),
        1 => c"cruise-speed translation".as_ptr(),
        _ => c"warp-speed translation".as_ptr(),
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_speed_source(kind: u32) -> *const core::ffi::c_char {
    match kind {
        0 => c"legacy impulse/warp speeds".as_ptr(),
        1 => c"legacy warpSpeed".as_ptr(),
        _ => c"legacy strategic-warp disable".as_ptr(),
    }
}
