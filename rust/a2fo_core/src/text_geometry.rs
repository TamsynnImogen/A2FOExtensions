//! Build-cost token policy and decal rotation; no native engine state is used.
#[cfg(all(target_arch = "x86", target_os = "windows"))]
use core::arch::asm;
use core::ffi::{CStr, c_char};

#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn product(a: f32, b: f32) -> f32 {
    a * b
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn product(a: f32, b: f32) -> f32 {
    let mut v = [a, b, 0.0];
    // SAFETY: three valid local slots; all x87 registers clobbered and emptied.
    unsafe {
        asm!("fld dword ptr [{p}]","fmul dword ptr [{p}+4]","fstp dword ptr [{p}+8]",
        p=in(reg) v.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
        out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[2]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn pair(a: f32, b: f32, c: f32, d: f32, subtract: bool) -> f32 {
    if subtract {
        a * b - c * d
    } else {
        a * b + c * d
    }
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn rotate_production(v: [f32; 3], sin: [f32; 3], cos: [f32; 3]) -> [f32; 3] {
    let data = [
        v[0], v[1], v[2], cos[0], cos[1], cos[2], sin[0], sin[1], sin[2],
    ];
    let mut result = [0.0; 3];
    // SAFETY: both arrays are live and disjoint. This is the production GCC
    // GNU++17/O2 x87 expression order: the Z/Y intermediate vectors stay in
    // registers until final XYZ stores. The caller's control word is unchanged
    // and the x87 stack is empty on exit, including unordered operands.
    unsafe {
        asm!(
            "fld dword ptr [{p}+20]", "fld dword ptr [{p}+32]",
            "fld dword ptr [{p}]", "fld st(0)", "fmul st(0), st(3)",
            "fld dword ptr [{p}+4]", "fld st(0)", "fmul st(0), st(4)",
            "fsubp st(2), st(0)", "fxch st(3)", "fmulp st(2), st(0)",
            "fxch st(2)", "fmulp st(3), st(0)", "faddp st(2), st(0)",
            "fld st(0)", "fld dword ptr [{p}+28]", "fmul st(1), st(0)",
            "fld dword ptr [{p}+8]", "fld st(0)", "fld dword ptr [{p}+16]",
            "fmul st(1), st(0)", "fxch st(4)", "fsubp st(1), st(0)",
            "fxch st(4)", "fmulp st(3), st(0)", "fmulp st(1), st(0)",
            "faddp st(1), st(0)", "fstp dword ptr [{out}]",
            "fld dword ptr [{p}+12]", "fld st(0)", "fmul st(0), st(3)",
            "fld dword ptr [{p}+24]", "fld st(0)", "fmul st(0), st(4)",
            "fsubp st(2), st(0)", "fxch st(1)", "fstp dword ptr [{out}+4]",
            "fmulp st(3), st(0)", "fmulp st(1), st(0)",
            "faddp st(1), st(0)", "fstp dword ptr [{out}+8]",
            p=in(reg) data.as_ptr(), out=in(reg) result.as_mut_ptr(),
            out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
            out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
            options(nostack)
        );
    }
    result
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn nearly_equal(a: f32, b: f32) -> bool {
    let scale = 1.0_f32.max(a.abs().max(b.abs()));
    (a - b).abs() <= scale * 0.001
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn nearly_equal(a: f32, b: f32) -> bool {
    let mut v = [a, b, 1.0_f32.max(a.abs().max(b.abs())), 0.0];
    // GCC's extended representation of the source literal .001f.
    let epsilon = [
        (-1924145349_i32) as u32,
        (-2095944041_i32) as u32,
        16373_u32,
    ];
    let result: u8;
    // SAFETY: bounded locals; subtraction spills to float, threshold remains
    // extended until comparison, exactly as in the original MinGW output.
    unsafe {
        asm!(
        "fld dword ptr [{p}]","fsub dword ptr [{p}+4]","fstp dword ptr [{p}+12]",
        "fld dword ptr [{p}+12]","fabs","fld tbyte ptr [{e}]",
        "fmul dword ptr [{p}+8]","fucomip st(0),st(1)","setae {r}","fstp st(0)",
        p=in(reg) v.as_mut_ptr(),e=in(reg) epsilon.as_ptr(),r=out(reg_byte) result,
        out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
        out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    result != 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_text_reconcile(raw: f32, adjusted: f32, modifier: f32) -> f32 {
    if !adjusted.is_finite()
        || adjusted < 0.0
        || !raw.is_finite()
        || raw < 0.0
        || !modifier.is_finite()
        || modifier < 0.0
    {
        return adjusted;
    }
    let explicit = product(raw, modifier);
    if !explicit.is_finite() || nearly_equal(adjusted, explicit) || !nearly_equal(adjusted, raw) {
        adjusted
    } else {
        explicit
    }
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn rounded_seconds(seconds: f32) -> f64 {
    unsafe extern "C" {
        fn floor(v: f64) -> f64;
    }
    // SAFETY: finite nonnegative input already validated.
    unsafe { floor(seconds as f64 + 0.5) }
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn rounded_seconds(seconds: f32) -> f64 {
    let mut v = [seconds.to_bits() as u64, 0.5_f32.to_bits() as u64, 0, 0, 0];
    // Slots: f32 seconds0/half8, CW16/18, double24/result32. The original
    // positive-only floor uses truncate RC; restore every caller CW bit.
    unsafe {
        asm!(
        "fnstcw word ptr [{p}+16]","mov ax,word ptr [{p}+16]","or ax,0xc00","mov word ptr [{p}+18],ax",
        "fld dword ptr [{p}]","fadd dword ptr [{p}+8]","fstp qword ptr [{p}+24]",
        "fld qword ptr [{p}+24]","fldcw word ptr [{p}+18]","frndint",
        "fldcw word ptr [{p}+16]","fstp qword ptr [{p}+32]",
        p=in(reg) v.as_mut_ptr(),out("eax") _,
        out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
        out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    f64::from_bits(v[4])
}
struct Writer {
    p: *mut u8,
    capacity: usize,
    written: usize,
}
impl Writer {
    fn bytes(&mut self, bytes: &[u8]) {
        for &b in bytes {
            if self.written < self.capacity - 1 {
                // SAFETY: caller owns capacity bytes and constructor checks >0.
                unsafe { self.p.add(self.written).write(b) };
            }
            self.written = self.written.saturating_add(1);
        }
    }
    fn number(&mut self, mut n: u32) {
        let mut digits = [0; 10];
        let mut at = 10;
        loop {
            at -= 1;
            digits[at] = b'0' + (n % 10) as u8;
            n /= 10;
            if n == 0 {
                break;
            }
        }
        self.bytes(&digits[at..]);
    }
    fn finish(self) -> u8 {
        // SAFETY: capacity>0 and min keeps the terminator inside the buffer.
        unsafe { self.p.add(self.written.min(self.capacity - 1)).write(0) };
        u8::from(self.written < self.capacity && self.written <= i32::MAX as usize)
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_text_build_token(
    separator: *const c_char,
    seconds: f32,
    compact: u8,
    output: *mut c_char,
    capacity: usize,
) -> u8 {
    if separator.is_null()
        || output.is_null()
        || capacity == 0
        || !seconds.is_finite()
        || seconds < 0.0
    {
        return 0;
    }
    let rounded = rounded_seconds(seconds);
    if rounded > u32::MAX as f64 {
        return 0;
    }
    let n = rounded as u32;
    let mut w = Writer {
        p: output.cast(),
        capacity,
        written: 0,
    };
    w.bytes(b" ");
    w.bytes(unsafe { CStr::from_ptr(separator) }.to_bytes());
    w.number(n);
    if compact != 0 {
        w.bytes(b" \x8a");
    } else {
        w.bytes(b" second");
        if n != 1 {
            w.bytes(b"s");
        }
    }
    w.finish()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_text_resource_token(
    separator: *const c_char,
    name: *const c_char,
    cost: i32,
    compact: u8,
    output: *mut c_char,
    capacity: usize,
) -> u8 {
    if separator.is_null() || name.is_null() || output.is_null() || capacity == 0 || cost <= 0 {
        return 0;
    }
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    if name.is_empty() {
        return 0;
    }
    let mut w = Writer {
        p: output.cast(),
        capacity,
        written: 0,
    };
    w.bytes(b" ");
    w.bytes(unsafe { CStr::from_ptr(separator) }.to_bytes());
    w.number(cost as u32);
    w.bytes(b" ");
    w.bytes(if compact != 0 { &name[..1] } else { name });
    w.finish()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_geometry_rotate_xyz(
    value: *const f32,
    degrees: *const f32,
    output: *mut f32,
) {
    if value.is_null() || degrees.is_null() || output.is_null() {
        return;
    }
    let v = unsafe { [value.read(), value.add(1).read(), value.add(2).read()] };
    let d = unsafe { [degrees.read(), degrees.add(1).read(), degrees.add(2).read()] };
    unsafe extern "C" {
        fn sincosf(v: f32, s: *mut f32, c: *mut f32);
    }
    let mut s = [0.0; 3];
    let mut c = [0.0; 3];
    for i in 0..3 {
        // SAFETY: CRT sincosf receives valid, disjoint float output slots.
        unsafe {
            sincosf(
                product(d[i], f32::from_bits(0x3c8e_fa35)),
                &mut s[i],
                &mut c[i],
            )
        };
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    let result = rotate_production(v, s, c);
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    let result = {
        let z = [
            pair(v[0], c[2], v[1], s[2], true),
            pair(v[0], s[2], v[1], c[2], false),
            v[2],
        ];
        let y = [
            pair(z[0], c[1], z[2], s[1], false),
            z[1],
            pair(-z[0], s[1], z[2], c[1], false),
        ];
        [
            y[0],
            pair(y[1], c[0], y[2], s[0], true),
            pair(y[1], s[0], y[2], c[0], false),
        ]
    };
    // SAFETY: output owns three floats; inputs were fully read before writes.
    unsafe { core::ptr::copy_nonoverlapping(result.as_ptr(), output, 3) };
}
