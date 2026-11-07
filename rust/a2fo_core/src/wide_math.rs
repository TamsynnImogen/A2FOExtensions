//! Exact x86 long-double accumulator used by collective XP thresholds.
//! Byte storage carries the original 80-bit value across native callbacks.
use core::arch::asm;
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_xp_wide_reset(p: *mut u8) {
    if !p.is_null() {
        unsafe { core::ptr::write_bytes(p, 0, 16) }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_xp_wide_add(p: *mut u8, value: f64, multiplier: f64) -> u8 {
    if p.is_null() {
        return 0;
    }
    let data = [value, multiplier, f64::MAX];
    let within: u8;
    // SAFETY: private host supplies live 16-byte storage. Extended product and
    // sum stay unrounded until tbyte spill, matching C++ long double under the
    // caller's precision and rounding modes. All eight x87 slots are clobbered;
    // this block empties its stack and does not alter the control word.
    unsafe {
        asm!("fld tbyte ptr [{p}]","fld qword ptr [{d}]","fmul qword ptr [{d}+8]",
  "faddp st(1),st(0)","fstp tbyte ptr [{p}]","fld tbyte ptr [{p}]",
  "fld qword ptr [{d}+16]","fucomip st(0),st(1)","setae {r}","fstp st(0)",
  p=in(reg) p,d=in(reg) data.as_ptr(),r=out(reg_byte) within,
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    // IEEE extended exponent 0x7fff denotes infinity/NaN, independently of sign.
    let exponent = unsafe { p.add(8).cast::<u16>().read_unaligned() } & 0x7fff;
    u8::from(within != 0 && exponent != 0x7fff)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_xp_wide_result(p: *const u8, out: *mut f64) -> u8 {
    if p.is_null() || out.is_null() {
        return 0;
    }
    let positive: u8;
    // SAFETY: bounded live accumulator/output. The explicit qword spill is the
    // original long-double-to-double conversion; underflow is checked afterwards.
    unsafe {
        asm!("fld tbyte ptr [{p}]","fstp qword ptr [{o}]","fld tbyte ptr [{p}]",
  "fldz","fucomip st(0),st(1)","setb {r}","fstp st(0)",
  p=in(reg) p,o=in(reg) out,r=out(reg_byte) positive,
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    u8::from(positive == 0 || unsafe { *out } != 0.0)
}

// C++ double assignments use x87 arithmetic under the caller's control word.
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn assignment(a: f64, b: f64, subtract: bool) -> f64 {
    if subtract { a - b } else { a + b }
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn assignment(a: f64, b: f64, subtract: bool) -> f64 {
    let mut v = [a, b, 0.0];
    let op = u32::from(subtract);
    // SAFETY: local qword inputs/output; one balanced x87 expression, CW unchanged.
    unsafe {
        asm!("fld qword ptr [{p}]","cmp {op:e},0","je 2f","fsub qword ptr [{p}+8]","jmp 3f","2:","fadd qword ptr [{p}+8]","3:","fstp qword ptr [{p}+16]",p=in(reg)v.as_mut_ptr(),op=in(reg)op,
 out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[2]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn above_difference(x: f64, a: f64, b: f64) -> bool {
    x > a - b
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn above_difference(x: f64, a: f64, b: f64) -> bool {
    let v = [x, a, b];
    let result: u8;
    // SAFETY: compare against the unspilled legacy subtraction; stack balanced.
    unsafe {
        asm!("fld qword ptr [{p}+8]","fsub qword ptr [{p}+16]","fld qword ptr [{p}]","fucomip st(0),st(1)","seta {r}","fstp st(0)",p=in(reg)v.as_ptr(),r=out(reg_byte)result,
 out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    result != 0
}
