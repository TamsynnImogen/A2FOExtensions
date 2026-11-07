//! Arithmetic compatibility for the original MinGW x87 expressions.
//! The caller's precision/rounding control is retained. Explicit spills match
//! the old compiler's float/double stores; extended constants match its output.

#[cfg(all(target_arch = "x86", target_os = "windows"))]
use core::arch::asm;

#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn ratio(current: f64, maximum: i32) -> f64 {
    let mut data = [current.to_bits(), maximum as u32 as u64, 0];
    // SAFETY: bounded local slots, positive maximum checked by the caller;
    // all x87 registers are declared clobbered and the stack ends empty.
    unsafe {
        asm!(
            "fld qword ptr [{p}]", "fidiv dword ptr [{p}+8]", "fstp qword ptr [{p}+16]",
            p=in(reg) data.as_mut_ptr(),
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack)
        );
    }
    f64::from_bits(data[2])
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn ratio(current: f64, maximum: i32) -> f64 {
    current / maximum as f64
}

#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn below_maximum(current: f64, maximum: i32) -> bool {
    // GCC's original extended representation of the source literal 0.0001.
    let epsilon = [(-501652180_i32) as u32, (-776530088_i32) as u32, 16369_u32];
    let data = [current.to_bits(), maximum as u32 as u64];
    let result: u8;
    // SAFETY: both memory inputs are live and bounded, stack balanced.
    unsafe {
        asm!(
            "fld qword ptr [{p}]", "fld tbyte ptr [{e}]", "faddp st(1), st(0)",
            "fild dword ptr [{p}+8]", "fucomip st(0), st(1)", "seta {r}", "fstp st(0)",
            p=in(reg) data.as_ptr(),e=in(reg) epsilon.as_ptr(),r=out(reg_byte) result,
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack)
        );
    }
    result != 0
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn below_maximum(current: f64, maximum: i32) -> bool {
    current + 0.0001 < maximum as f64
}

#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn centered_axis(start: f32, extent: f32, ratio: f32) -> (f32, f32) {
    let mut data = [start, extent, ratio, 0.5, 0.0, 0.0];
    // SAFETY: retain the product extended through position arithmetic, while
    // storing the size as float at its original output boundary. CW unchanged.
    unsafe {
        asm!(
            "fld dword ptr [{p}+4]", "fmul dword ptr [{p}+8]", "fst dword ptr [{p}+20]",
            "fld dword ptr [{p}+4]", "fsub st(0),st(1)", "fmul dword ptr [{p}+12]",
            "fadd dword ptr [{p}]", "fstp dword ptr [{p}+16]", "fstp st(0)",
            p=in(reg) data.as_mut_ptr(),
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack)
        );
    }
    (data[4], data[5])
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn centered_axis(start: f32, extent: f32, ratio: f32) -> (f32, f32) {
    let retained = extent * ratio;
    (start + (extent - retained) * 0.5, retained)
}

#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn within_end(cursor: f32, start: f32, extent: f32) -> bool {
    let data = [cursor, start, extent];
    let result: u8;
    // SAFETY: bounded inputs; compare against the unspilled extended sum.
    unsafe {
        asm!(
            "fld dword ptr [{p}+4]", "fadd dword ptr [{p}+8]", "fld dword ptr [{p}]",
            "fucomip st(0), st(1)", "setbe {r}", "fstp st(0)",
            p=in(reg) data.as_ptr(),r=out(reg_byte) result,
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack)
        );
    }
    result != 0
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn within_end(cursor: f32, start: f32, extent: f32) -> bool {
    cursor <= start + extent
}

#[cfg(all(target_arch = "x86", target_os = "windows"))]
pub fn rebuild(current: f64, maximum: i32) -> f32 {
    let mut data = [
        current.to_bits(),
        maximum as u32 as u64,
        3f32.to_bits() as u64,
        0.92f64.to_bits(),
        0.08f64.to_bits(),
        0,
    ];
    // SAFETY: input admitted by caller. Match GNU++17's unspilled quotient and
    // smoothstep, its binary64 constants and one final binary32 output store.
    unsafe {
        asm!("fld qword ptr [{p}]", "fidiv dword ptr [{p}+8]",
        "fld1", "fxch st(1)", "fcomi st(0),st(1)", "fcmovnbe st(0),st(1)", "fstp st(1)",
        "fld st(0)", "fadd st(0),st(1)", "fsubr dword ptr [{p}+16]", "fxch st(1)",
        "fmul st(0),st(0)", "fmulp st(1),st(0)", "fmul qword ptr [{p}+24]",
        "fadd qword ptr [{p}+32]", "fstp dword ptr [{p}+40]",
        p=in(reg)data.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
        out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    f32::from_bits(data[5] as u32)
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
pub fn rebuild(current: f64, maximum: i32) -> f32 {
    let p = ratio(current, maximum).clamp(0.0, 1.0);
    (0.08 + p * p * (3.0 - 2.0 * p) * 0.92) as f32
}

/// GNU++17 stores the previous total as double before each resolver call,
/// then keeps the current row's sum extended through its final float result.
pub struct Seconds {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    value: [u8; 16],
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    value: f64,
}
impl Seconds {
    pub fn new() -> Self {
        Self {
            #[cfg(all(target_arch = "x86", target_os = "windows"))]
            value: [0; 16],
            #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
            value: 0.0,
        }
    }
    pub fn before_row(&mut self) {
        #[cfg(all(target_arch = "x86", target_os = "windows"))]
        {
            let mut spill = 0f64;
            // SAFETY: bounded initialized accumulator and double spill. This
            // store occurs before the original native resolver callback, not
            // after the last row. The stack is balanced and CW unchanged.
            unsafe {
                asm!("fld tbyte ptr [{p}]", "fstp qword ptr [{q}]",
                "fld qword ptr [{q}]", "fstp tbyte ptr [{p}]",
                p=in(reg)self.value.as_mut_ptr(),q=in(reg)&mut spill,
                out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
                out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
            }
        }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    pub fn add(&mut self, seconds: f32, count: u64) -> bool {
        self.value += seconds as f64 * count as f64;
        self.value > f32::MAX as f64
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    pub fn add(&mut self, seconds: f32, count: u64) -> bool {
        let data = [count, seconds.to_bits() as u64, f32::MAX.to_bits() as u64];
        let overflow: u8;
        // SAFETY: original fild/multiply/add stays extended until either the
        // next native resolver call or final result. No Rust reference crosses
        // callbacks; all eight x87 registers are clobbered, empty on return.
        unsafe {
            asm!("fild qword ptr [{p}]", "fmul dword ptr [{p}+8]",
            "fld tbyte ptr [{a}]", "faddp st(1),st(0)",
            "fld dword ptr [{p}+16]", "fucomip st(0),st(1)", "setb {o}",
            "fstp tbyte ptr [{a}]",
            p=in(reg)data.as_ptr(),a=in(reg)self.value.as_mut_ptr(),o=out(reg_byte)overflow,
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        overflow != 0
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    pub fn value(&self) -> f32 {
        self.value as f32
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    pub fn value(&self) -> f32 {
        let mut result = 0f32;
        // SAFETY: bounded extended-to-float final spill; caller CW retained.
        unsafe {
            asm!("fld tbyte ptr [{p}]", "fstp dword ptr [{r}]",
            p=in(reg)self.value.as_ptr(),r=in(reg)&mut result,
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        result
    }
}
