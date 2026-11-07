//! A1 GUI scale arithmetic, with GNU i686 x87 evaluation and native CRT rounding.
use core::ffi::c_void;
type Round = unsafe extern "C" fn(u64) -> i32;
/// # Safety
/// Area is a live 16-byte native rectangle, the scale pointers cover initialized
/// f64 bit patterns and output covers four private i32 slots. Round is the native
/// synchronous CRT adapter and cannot access output. Inputs stay live through all
/// callbacks; later area fields are deliberately reread after earlier rounding.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_scale(
    area: *const c_void,
    sx: *const u64,
    sy: *const u64,
    output: *mut i32,
    round: Round,
) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        #[repr(C)]
        struct Context {
            area: *const c_void,
            sx: *const u64,
            sy: *const u64,
            scales: [[u32; 2]; 2],
            product: [u32; 2],
        }
        const _: () = assert!(core::mem::offset_of!(Context, scales) == 12);
        const _: () = assert!(core::mem::offset_of!(Context, product) == 28);
        let mut c = Context {
            area,
            sx,
            sy,
            scales: [[0; 2]; 2],
            product: [0; 2],
        };
        unsafe {
            core::arch::asm!(
            "mov eax,dword ptr [{p}+4]","fld qword ptr [eax]",
            "mov eax,dword ptr [{p}+8]","fld qword ptr [eax]","fstp qword ptr [{p}+20]",
            "mov eax,dword ptr [{p}]","fild dword ptr [eax]","fmul st(0),st(1)",
            "fxch st(1)","fstp qword ptr [{p}+12]","fstp qword ptr [{p}+28]",
            p=in(reg)&mut c,out("eax")_,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        unsafe {
            output.write(round(
                u64::from(c.product[0]) | (u64::from(c.product[1]) << 32),
            ));
        }
        for index in 1..4 {
            let value = unsafe { area.cast::<i32>().add(index) };
            let scale = &c.scales[index & 1];
            let mut product = 0u64;
            unsafe {
                core::arch::asm!("fild dword ptr [{v}]","fmul qword ptr [{s}]","fstp qword ptr [{o}]",
                v=in(reg)value,s=in(reg)scale,o=in(reg)&mut product,
                out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
                out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
                output.add(index).write(round(product));
            }
        }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let scales = [
            f64::from_bits(unsafe { sx.read() }),
            f64::from_bits(unsafe { sy.read() }),
        ];
        for index in 0..4 {
            let value = unsafe { area.cast::<i32>().add(index).read() };
            unsafe {
                output
                    .add(index)
                    .write(round(((value as f64) * scales[index & 1]).to_bits()));
            }
        }
    }
}
/// # Safety
/// Scale points to one initialized f64 bit pattern. Round is the live native
/// synchronous CRT rounding adapter and preserves the native exception boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_ui_scale_scalar(
    value: i32,
    scale: *const u64,
    round: Round,
) -> i32 {
    let product: u64;
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let mut bits = 0u64;
        unsafe {
            core::arch::asm!("fild dword ptr [{v}]","fmul qword ptr [{s}]","fstp qword ptr [{o}]",
        v=in(reg)&value,s=in(reg)scale,o=in(reg)&mut bits,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        product = bits;
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        product = ((value as f64) * f64::from_bits(unsafe { scale.read() })).to_bits();
    }
    unsafe { round(product) }
}
#[cfg(test)]
mod tests {
    use super::*;
    unsafe extern "C" fn round(bits: u64) -> i32 {
        f64::from_bits(bits).round() as i32
    }
    #[test]
    fn scaled_fields() {
        let area = [-3, 7, 5, -9];
        let x = 1.5f64.to_bits();
        let y = 0.5f64.to_bits();
        let mut out = [0; 4];
        unsafe {
            a2fo_rs_stage9_a1_ui_scale(area.as_ptr().cast(), &x, &y, out.as_mut_ptr(), round)
        };
        assert_eq!(out, [-5, 4, 8, -5]);
    }
}
