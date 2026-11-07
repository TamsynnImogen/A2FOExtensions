//! A1 crew-condition policy and observed GNU i686 x87 evaluation.
use core::ffi::c_void;
#[cfg(all(target_arch = "x86", target_os = "windows"))]
#[repr(C)]
struct Kernel {
    current: u32,
    maximum: u32,
    half: u32,
    quarter: u32,
    float_max: u32,
    maximum_readable: u32,
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
macro_rules! health_kernel {($prefix:literal,$k:expr)=>{{
    let result:u32;
    unsafe{core::arch::asm!(concat!($prefix,
        "\n4:\nfld dword ptr [{p}]\nfldz\nfcomi st(0),st(1)\njae 6f\n",
        "fld dword ptr [{p}+8]\nfmul st(0),st(3)\nfxch st(2)\nfcomi st(0),st(2)\nfstp st(2)\nja 5f\n",
        "fxch st(2)\nfmul dword ptr [{p}+12]\nfxch st(1)\nfcomip st(0),st(1)\nfstp st(0)\nja 7f\njmp 8f\n",
        "5:\nfstp st(0)\nfstp st(0)\nfstp st(0)\njmp 80f\n",
        "6:\nfstp st(0)\nfstp st(0)\nfstp st(0)\njmp 81f\n",
        "7:\nfstp st(0)\nmov {r},1\njmp 99f\n",
        "8:\nfstp st(0)\njmp 81f\n9:\nfstp st(0)\njmp 82f\n",
        "80:\nmov {r},0\njmp 99f\n81:\nmov {r},2\njmp 99f\n82:\nmov {r},3\n99:"),
        p=in(reg)&$k,r=out(reg)result,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));}
    result
}}}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn ordinary_health(current: f64, maximum: f64) -> u32 {
    if maximum <= 0.0 || current <= 0.0 {
        2
    } else if current > maximum * 0.5 {
        0
    } else if current > maximum * 0.25 {
        1
    } else {
        2
    }
}
fn primary(current: u32, maximum: u32, readable: u8) -> u32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let k = Kernel {
            current,
            maximum,
            half: 0x3f000000,
            quarter: 0x3e800000,
            float_max: 0x7f7fffff,
            maximum_readable: readable as u32,
        };
        health_kernel!(
            "fld dword ptr [{p}]\nfabs\ncmp dword ptr [{p}+20],0\nje 2f\n\
        fld dword ptr [{p}+16]\nfucomi st(0),st(1)\nfstp st(1)\njb 3f\n\
        fld dword ptr [{p}+4]\nfld st(0)\nfabs\nfxch st(2)\nfucomip st(0),st(2)\nfstp st(1)\njb 9f\n\
        fldz\nfcomip st(0),st(1)\njae 9f\njmp 4f\n\
        2:\nfld dword ptr [{p}+16]\nfucomip st(0),st(1)\nfstp st(0)\njb 80f\njmp 82f\n\
        3:\nfstp st(0)\njmp 80f",
            k
        )
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let c = f32::from_bits(current);
        let m = f32::from_bits(maximum);
        if !c.is_finite() {
            0
        } else if readable == 0 || !m.is_finite() || m <= 0.0 {
            3
        } else {
            ordinary_health(c as f64, m as f64)
        }
    }
}
fn fallback(current: u32, maximum: i32) -> u32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let k = Kernel {
            current,
            maximum: maximum.max(0) as u32,
            half: 0x3f000000,
            quarter: 0x3e800000,
            float_max: 0,
            maximum_readable: 0,
        };
        health_kernel!(
            "fild dword ptr [{p}+4]\nfldz\nfcomip st(0),st(1)\njae 8f\njmp 4f",
            k
        )
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        ordinary_health(f32::from_bits(current) as f64, maximum.max(0) as f64)
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_crew_health(current: u32, maximum: u32) -> u32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let k = Kernel {
            current,
            maximum,
            half: 0x3f000000,
            quarter: 0x3e800000,
            float_max: 0,
            maximum_readable: 0,
        };
        health_kernel!(
            "fld dword ptr [{p}+4]\nfldz\nfcomip st(0),st(1)\njae 8f\njmp 4f",
            k
        )
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        ordinary_health(
            f32::from_bits(current) as f64,
            f32::from_bits(maximum) as f64,
        )
    }
}
/// # Safety
/// Output covers three writable caller-owned u32 slots; no alias is retained.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_crew_colour_words(kind: u32, output: *mut u32) {
    let words = match kind {
        0 => [0, 0x3f800000, 0],
        1 => [0x3f800000, 0x3f800000, 0],
        _ => [0x3f800000, 0, 0],
    };
    unsafe { core::ptr::copy_nonoverlapping(words.as_ptr(), output, 3) };
}
type Fallback = unsafe extern "C" fn(*mut c_void) -> i32;
/// # Safety
/// Context/callback stay live synchronously. Native reads the class fallback
/// only when requested and cannot access output, which covers three writable
/// private u32 slots. The callback preserves the native noexcept boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_crew_colour(
    current: u32,
    maximum: u32,
    readable: u8,
    context: *mut c_void,
    read_fallback: Fallback,
    output: *mut u32,
) {
    let kind = primary(current, maximum, readable);
    let kind = if kind == 3 {
        fallback(current, unsafe { read_fallback(context) })
    } else {
        kind
    };
    unsafe { a2fo_rs_stage9_a1_crew_colour_words(kind, output) };
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn bands() {
        for (c, m, want) in [
            (60., 100., 0),
            (50., 100., 1),
            (25., 100., 2),
            (0., 100., 2),
            (1., 0., 2),
        ] {
            assert_eq!(
                a2fo_rs_stage9_a1_crew_health((c as f32).to_bits(), (m as f32).to_bits()),
                want
            )
        }
    }
    #[test]
    fn fallback_admission() {
        assert_eq!(primary(f32::NAN.to_bits(), 0, 0), 0);
        assert_eq!(primary(1f32.to_bits(), 0, 1), 3);
        assert_eq!(fallback(1f32.to_bits(), -1), 2);
    }
}
