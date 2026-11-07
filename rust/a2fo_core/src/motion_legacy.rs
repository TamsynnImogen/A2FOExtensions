//! x87 expression kernels for MinGW's excess-precision float evaluation.
#[cfg(all(target_arch = "x86", target_os = "windows"))]
use core::arch::asm;
unsafe extern "C" {
    fn sqrtf(x: f32) -> f32;
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    fn atan2f(y: f32, x: f32) -> f32;
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    dot_affine(a, b, 1.0, 0.0, false)
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn dot_affine(a: [f32; 3], b: [f32; 3], scale: f32, offset: f32, _affine: bool) -> f32 {
    offset + scale * dot(a, b)
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn dot_affine(a: [f32; 3], b: [f32; 3], scale: f32, offset: f32, affine: bool) -> f32 {
    let mut v = [a[0], a[1], a[2], b[0], b[1], b[2], scale, offset, 0.0];
    let op = u32::from(affine);
    // SAFETY: all referenced slots are local. Three products/sums stay extended
    // until the original float store; optional scale/offset share the expression.
    unsafe {
        asm!("fld dword ptr [{p}]","fmul dword ptr [{p}+12]",
  "fld dword ptr [{p}+4]","fmul dword ptr [{p}+16]","faddp st(1),st(0)",
  "fld dword ptr [{p}+8]","fmul dword ptr [{p}+20]","faddp st(1),st(0)",
  "cmp {op:e},0","je 2f","fmul dword ptr [{p}+24]","fadd dword ptr [{p}+28]",
  "2:","fstp dword ptr [{p}+32]",p=in(reg) v.as_mut_ptr(),op=in(reg) op,
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[8]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn pair(a: f32, b: f32, c: f32, d: f32, subtract: bool) -> f32 {
    if subtract {
        a * b - c * d
    } else {
        a * b + c * d
    }
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn pair(a: f32, b: f32, c: f32, d: f32, subtract: bool) -> f32 {
    let mut v = [a, b, c, d, 0.0];
    let op = u32::from(subtract);
    // SAFETY: bounded locals, two extended products and one final float spill.
    unsafe {
        asm!("fld dword ptr [{p}]","fmul dword ptr [{p}+4]",
  "fld dword ptr [{p}+8]","fmul dword ptr [{p}+12]","cmp {op:e},0","je 2f",
  "fsubp st(1),st(0)","jmp 3f","2:","faddp st(1),st(0)","3:","fstp dword ptr [{p}+16]",
  p=in(reg) v.as_mut_ptr(),op=in(reg) op,
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[4]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a + (b - a) * t
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    let mut v = [a, b, t, 0.0];
    // SAFETY: bounded local expression storage; stack empty on exit, CW unchanged.
    unsafe {
        asm!("fld dword ptr [{p}+4]","fsub dword ptr [{p}]","fmul dword ptr [{p}+8]",
  "fadd dword ptr [{p}]","fstp dword ptr [{p}+12]",p=in(reg) v.as_mut_ptr(),
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[3]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn cube(a: f32) -> f32 {
    a * a * a
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn cube(a: f32) -> f32 {
    let mut v = [a, 0.0];
    // SAFETY: two valid slots, original extended product followed by float store.
    unsafe {
        asm!("fld dword ptr [{p}]","fmul st(0),st(0)","fmul dword ptr [{p}]",
  "fstp dword ptr [{p}+4]",p=in(reg) v.as_mut_ptr(),
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[1]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn sqrt_ratio(numerator: f32, arg: f32) -> f32 {
    numerator / unsafe { sqrtf(arg) }
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn sqrt_ratio(numerator: f32, arg: f32) -> f32 {
    if !arg.is_finite() || arg < 0.0 {
        return numerator / unsafe { sqrtf(arg) };
    }
    let mut v = [numerator, arg, 0.0];
    // SAFETY: nonnegative finite sqrt argument. MinGW sqrtf returns the direct
    // fsqrt value in ST0; retaining it through division avoids an extra spill.
    unsafe {
        asm!("fld dword ptr [{p}+4]","fsqrt","fld dword ptr [{p}]",
  "fdiv st(0),st(1)","fstp dword ptr [{p}+8]","fstp st(0)",p=in(reg) v.as_mut_ptr(),
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[2]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn sqrt_scale(arg: f32, scale: f32) -> f32 {
    (unsafe { sqrtf(arg) }) * scale
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn sqrt_scale(arg: f32, scale: f32) -> f32 {
    if !arg.is_finite() || arg < 0.0 {
        return unsafe { sqrtf(arg) } * scale;
    }
    let mut v = [arg, scale, 0.0];
    // SAFETY: bounded slots; emulate a positive CRT sqrtf return before multiply.
    unsafe {
        asm!("fld dword ptr [{p}]","fsqrt","fmul dword ptr [{p}+4]","fstp dword ptr [{p}+8]",
  p=in(reg) v.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[2]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn atan_scaled(y: f32, x: f32, scale: f32) -> f32 {
    (unsafe { atan2f(y, x) }) * scale
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn atan_scaled(y: f32, x: f32, scale: f32) -> f32 {
    let mut v = [y, x, scale, 0.0];
    // SAFETY: MinGW atan2f is fld y; fld x; fpatan, including zero/NaN handling.
    // Match its extended return through the scale multiply and final float store.
    unsafe {
        asm!("fld dword ptr [{p}]","fld dword ptr [{p}+4]","fpatan",
  "fmul dword ptr [{p}+8]","fstp dword ptr [{p}+12]",p=in(reg) v.as_mut_ptr(),
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[3]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn sqrt_entry(b: f32, disc: f32, den: f32) -> f32 {
    (-b - unsafe { sqrtf(disc) }) / den
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn sqrt_entry(b: f32, disc: f32, den: f32) -> f32 {
    let mut v = [b, disc, den, 0.0];
    // SAFETY: caller admitted a nonnegative discriminant; preserve the extended
    // sqrtf return through the difference/division before the final float spill.
    unsafe {
        asm!("fld dword ptr [{p}+4]","fsqrt","fld dword ptr [{p}]","fchs",
  "fsub st(0),st(1)","fdiv dword ptr [{p}+8]","fstp dword ptr [{p}+12]","fstp st(0)",
  p=in(reg) v.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[3]
}

#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn affine(offset: f32, scale: f32, value: f32) -> f32 {
    offset + scale * value
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn affine(offset: f32, scale: f32, value: f32) -> f32 {
    let mut v = [offset, scale, value, 0.0];
    // SAFETY: bounded slots; preserve multiply/add as a single x87 expression.
    unsafe {
        asm!("fld dword ptr [{p}+4]","fmul dword ptr [{p}+8]","fadd dword ptr [{p}]","fstp dword ptr [{p}+12]",
  p=in(reg) v.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[3]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn cone_component(centre: f32, ch: f32, ta: f32, ca: f32, tb: f32, sa: f32, sh: f32) -> f32 {
    centre * ch + (ta * ca + tb * sa) * sh
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn cone_component(centre: f32, ch: f32, ta: f32, ca: f32, tb: f32, sa: f32, sh: f32) -> f32 {
    let mut v = [centre, ch, ta, ca, tb, sa, sh, 0.0];
    // SAFETY: bounded slots; one final float store for the cone direction.
    unsafe {
        asm!("fld dword ptr [{p}]","fmul dword ptr [{p}+4]",
  "fld dword ptr [{p}+8]","fmul dword ptr [{p}+12]",
  "fld dword ptr [{p}+16]","fmul dword ptr [{p}+20]","faddp st(1),st(0)",
  "fmul dword ptr [{p}+24]","faddp st(1),st(0)","fstp dword ptr [{p}+28]",
  p=in(reg) v.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[7]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn sqrt_le(arg: f32, a: f32, b: f32) -> bool {
    (unsafe { sqrtf(arg) }) <= a + b
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn sqrt_le(arg: f32, a: f32, b: f32) -> bool {
    let v = [arg, a, b];
    let result: u8;
    // SAFETY: caller validated nonnegative squared distance. Compare unspilled
    // MinGW sqrtf return against the original extended sum, CW unchanged.
    unsafe {
        asm!("fld dword ptr [{p}]","fsqrt","fld dword ptr [{p}+4]","fadd dword ptr [{p}+8]",
  "fucomip st(0),st(1)","setae {r}","fstp st(0)",p=in(reg) v.as_ptr(),r=out(reg_byte) result,
  out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
  out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    result != 0
}
