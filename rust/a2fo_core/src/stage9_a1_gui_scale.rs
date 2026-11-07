//! A1 control/cinematic scale selection and native-width x87 ratios.
use core::ffi::{c_char, c_void};
unsafe fn coordinate(area: *const i32, index: usize) -> i32 {
    unsafe { area.add(index).read() }
}
unsafe fn ratio(numerator: i32, denominator: i32, output: *mut c_void) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    unsafe {
        core::arch::asm!("fild dword ptr [{n}]","fidiv dword ptr [{d}]","fstp qword ptr [{o}]",
        n=in(reg)&numerator,d=in(reg)&denominator,o=in(reg)output,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    unsafe {
        output
            .cast::<u64>()
            .write_unaligned(((numerator as f64) / (denominator as f64)).to_bits())
    }
}
unsafe fn positive(value: *const c_void) -> bool {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let result: u8;
        unsafe {
            core::arch::asm!("fld qword ptr [{p}]","fldz","fxch st(1)","fcomip st(0),st(1)","fstp st(0)","seta {r}",
        p=in(reg)value,r=out(reg_byte)result,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result != 0
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        f64::from_bits(unsafe { value.cast::<u64>().read_unaligned() }) > 0.0
    }
}
unsafe fn nonpositive(value: *const c_void) -> bool {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let result: u8;
        unsafe {
            core::arch::asm!("fld qword ptr [{p}]","fldz","fcomip st(0),st(1)","fstp st(0)","setae {r}",
        p=in(reg)value,r=out(reg_byte)result,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result != 0
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        f64::from_bits(unsafe { value.cast::<u64>().read_unaligned() }) <= 0.0
    }
}
/// # Safety
/// Raw and actual each cover four initialized native i32 fields. Raw dimensions
/// are positive as established by the shared area validator. Outputs cover two
/// disjoint writable native double slots, disjoint from the input areas.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_scale_control(
    raw: *const i32,
    actual: *const i32,
    sx: *mut c_void,
    sy: *mut c_void,
) {
    let raw_right = unsafe { coordinate(raw, 0) }.wrapping_add(unsafe { coordinate(raw, 2) });
    let raw_bottom = unsafe { coordinate(raw, 1) }.wrapping_add(unsafe { coordinate(raw, 3) });
    let (nx, dx) = if raw_right == 640 {
        (
            unsafe { coordinate(actual, 0) }.wrapping_add(unsafe { coordinate(actual, 2) }),
            640,
        )
    } else {
        (unsafe { coordinate(actual, 2) }, unsafe {
            coordinate(raw, 2)
        })
    };
    unsafe { ratio(nx, dx, sx) };
    let (ny, dy) = if raw_bottom == 480 {
        (
            unsafe { coordinate(actual, 1) }.wrapping_add(unsafe { coordinate(actual, 3) }),
            480,
        )
    } else {
        (unsafe { coordinate(actual, 3) }, unsafe {
            coordinate(raw, 3)
        })
    };
    unsafe { ratio(ny, dy, sy) };
}
/// # Safety
/// Area and scale spans follow the control function contract, including private
/// initialized double slots. Unusable raw dimensions are never divided.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_scale_cinematic(
    has_raw: u8,
    has_panel: u8,
    raw: *const i32,
    actual: *const i32,
    sx: *mut c_void,
    sy: *mut c_void,
) -> u8 {
    if has_raw == 0 || has_panel == 0 {
        return 0;
    }
    if !unsafe { nonpositive(sx) } && !unsafe { nonpositive(sy) } {
        return 0;
    }
    if unsafe { coordinate(raw, 2) } <= 0 || unsafe { coordinate(raw, 3) } <= 0 {
        return 0;
    }
    unsafe {
        ratio(coordinate(actual, 2), coordinate(raw, 2), sx);
        ratio(coordinate(actual, 3), coordinate(raw, 3), sy)
    };
    1
}
/// # Safety
/// Both pointers cover initialized private native double values, stable here.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_scale_use_raw(
    has_raw: u8,
    sx: *const c_void,
    sy: *const c_void,
) -> u8 {
    u8::from(has_raw != 0 && unsafe { positive(sx) } && unsafe { positive(sy) })
}
/// # Safety
/// Value covers one initialized private native double, stable for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_gui_scale_speed_valid(value: *const c_void) -> u8 {
    if !unsafe { positive(value) } {
        return 0;
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let maximum = 0x7fefffffffffffffu64;
        let result: u8;
        unsafe {
            core::arch::asm!("fld qword ptr [{p}]","fabs","fld qword ptr [{m}]","fucomip st(0),st(1)","fstp st(0)","setae {r}",
        p=in(reg)value,m=in(reg)&maximum,r=out(reg_byte)result,
        out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
        out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        u8::from(f64::from_bits(unsafe { value.cast::<u64>().read_unaligned() }).is_finite())
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_gui_default_energy_sprite(
    kind: u8,
    has_content: u8,
) -> *const c_char {
    if has_content != 0 {
        return core::ptr::null();
    }
    if kind == 0 {
        c"large_energy_bar".as_ptr()
    } else {
        c"small_energy_bar".as_ptr()
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn edge_anchored_and_width_ratios() {
        let raw = [524, 334, 116, 146];
        let actual = [1048, 668, 232, 292];
        let (mut x, mut y) = (0u64, 0u64);
        unsafe {
            a2fo_rs_stage9_a1_gui_scale_control(
                raw.as_ptr(),
                actual.as_ptr(),
                (&mut x as *mut u64).cast(),
                (&mut y as *mut u64).cast(),
            )
        };
        assert_eq!((f64::from_bits(x), f64::from_bits(y)), (2., 2.));
    }
    #[test]
    fn cinematic_guard_and_source() {
        let raw = [0, 0, 320, 240];
        let actual = [0, 0, 640, 480];
        let (mut x, mut y) = (0u64, 1.0f64.to_bits());
        let px = (&mut x as *mut u64).cast();
        let py = (&mut y as *mut u64).cast();
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_gui_scale_cinematic(1, 1, raw.as_ptr(), actual.as_ptr(), px, py)
            },
            1
        );
        assert_eq!(unsafe { a2fo_rs_stage9_a1_gui_scale_use_raw(1, px, py) }, 1);
        assert_eq!(unsafe { a2fo_rs_stage9_a1_gui_scale_speed_valid(px) }, 1);
    }
}
