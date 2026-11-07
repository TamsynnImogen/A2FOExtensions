//! Exact world-bound publication after the native adapter validates both spans.

/// Safety: input vectors each hold three readable f32 fields in C layout;
/// both output spans hold three writable f32 fields for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_sol_publish_bounds(
    minimum: *const f32,
    size: *const f32,
    native_minimum: *mut f32,
    native_maximum: *mut f32,
) {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        #[repr(C)]
        struct Context {
            minimum: *const f32,
            size: *const f32,
            native_minimum: *mut f32,
            native_maximum: *mut f32,
            temporary: [u32; 6],
        }
        let mut context = Context {
            minimum,
            size,
            native_minimum,
            native_maximum,
            temporary: [0; 6],
        };
        // SAFETY: private i686 repr(C) context (four 32-bit pointers followed
        // by six dwords), native spans checked by the caller. This mirrors the
        // GNU++17/O2 load/FST/add/spill/publication order, including publishing
        // all minima before the last maximum spill. All x87 registers are
        // declared clobbered, the stack balances, and CW is unchanged.
        unsafe {
            core::arch::asm!(
                "mov eax,dword ptr [{p}]", "mov edx,dword ptr [{p}+4]",
                "mov ecx,dword ptr [{p}+8]",
                "fld dword ptr [eax]", "fst dword ptr [{p}+16]",
                "fld dword ptr [eax+4]", "fst dword ptr [{p}+20]",
                "fld dword ptr [eax+8]", "fst dword ptr [{p}+24]",
                "fxch st(2)", "fadd dword ptr [edx]", "fstp dword ptr [{p}+28]",
                "fadd dword ptr [edx+4]", "fstp dword ptr [{p}+32]",
                "fadd dword ptr [edx+8]", "mov edx,dword ptr [{p}+12]",
                "mov eax,dword ptr [{p}+16]", "mov dword ptr [ecx],eax",
                "mov eax,dword ptr [{p}+20]", "mov dword ptr [ecx+4],eax",
                "mov eax,dword ptr [{p}+24]", "mov dword ptr [ecx+8],eax",
                "mov eax,dword ptr [{p}+28]", "fstp dword ptr [{p}+36]",
                "mov dword ptr [edx],eax",
                "mov eax,dword ptr [{p}+32]", "mov dword ptr [edx+4],eax",
                "mov eax,dword ptr [{p}+36]", "mov dword ptr [edx+8],eax",
                p = in(reg) &mut context,
                out("eax") _, out("edx") _, out("ecx") _,
                out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
                out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
                options(nostack)
            );
        }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let mut minima = [0.0; 3];
        let mut maxima = [0.0; 3];
        for (index, target) in minima.iter_mut().enumerate() {
            // SAFETY: three readable native fields supplied by the caller.
            *target = unsafe { minimum.add(index).read() };
        }
        for (index, target) in maxima.iter_mut().enumerate() {
            // SAFETY: three readable native size fields supplied by caller.
            *target = minima[index] + unsafe { size.add(index).read() };
        }
        // SAFETY: local input arrays do not alias the writable native spans.
        unsafe {
            core::ptr::copy_nonoverlapping(minima.as_ptr(), native_minimum, 3);
            core::ptr::copy_nonoverlapping(maxima.as_ptr(), native_maximum, 3);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn publishes_minimum_then_derived_maximum() {
        let minimum = [-10.0, -20.0, -30.0];
        let size = [100.0, 200.0, 300.0];
        let mut native_minimum = [77.0; 3];
        let mut native_maximum = [77.0; 3];
        unsafe {
            a2fo_rs_stage9_sol_publish_bounds(
                minimum.as_ptr(),
                size.as_ptr(),
                native_minimum.as_mut_ptr(),
                native_maximum.as_mut_ptr(),
            );
        }
        assert_eq!(native_minimum, minimum);
        assert_eq!(native_maximum, [90.0, 180.0, 270.0]);
    }
}
