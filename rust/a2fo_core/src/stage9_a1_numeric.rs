//! Exact i686 GNU++17/O2 A1 bounds addition and publication order.
/// # Safety
/// Inputs and outputs each cover three f32 values, live for this call. The
/// caller validates native spans; input/output spans do not alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_publish_bounds(
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
            temp: [u32; 3],
        }
        let mut c = Context {
            minimum,
            size,
            native_minimum,
            native_maximum,
            temp: [0; 3],
        };
        // Mirrors observed size.x+min.x and size.y+min.y stores, then min.z+size.z;
        // publishes raw minima before spilling z and publishing the three maxima.
        unsafe {
            core::arch::asm!(
   "mov eax,dword ptr [{p}]","mov edx,dword ptr [{p}+4]","mov ecx,dword ptr [{p}+8]",
   "fld dword ptr [edx]","fadd dword ptr [eax]","fstp dword ptr [{p}+16]",
   "fld dword ptr [edx+4]","fadd dword ptr [eax+4]","fstp dword ptr [{p}+20]",
   "fld dword ptr [eax+8]","fadd dword ptr [edx+8]","mov edx,dword ptr [{p}+12]",
   "mov eax,dword ptr [eax]","mov dword ptr [ecx],eax",
   "mov eax,dword ptr [{p}]","mov eax,dword ptr [eax+4]","mov dword ptr [ecx+4],eax",
   "mov eax,dword ptr [{p}]","mov eax,dword ptr [eax+8]","mov dword ptr [ecx+8],eax",
   "mov eax,dword ptr [{p}+16]","fstp dword ptr [{p}+24]","mov dword ptr [edx],eax",
   "mov eax,dword ptr [{p}+20]","mov dword ptr [edx+4],eax",
   "mov eax,dword ptr [{p}+24]","mov dword ptr [edx+8],eax",
   p=in(reg)&mut c,out("eax")_,out("edx")_,out("ecx")_,
   out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let mut m = [0.0f32; 3];
        let mut x = [0.0f32; 3];
        for i in 0..3 {
            m[i] = unsafe { minimum.add(i).read() };
            let component = unsafe { size.add(i).read() };
            let (left, right) = if i < 2 {
                (component, m[i])
            } else {
                (m[i], component)
            };
            x[i] = left + right;
        }
        unsafe {
            core::ptr::copy_nonoverlapping(m.as_ptr(), native_minimum, 3);
            core::ptr::copy_nonoverlapping(x.as_ptr(), native_maximum, 3);
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_bounds_report(pass: i32) -> u8 {
    if pass <= 8 {
        1
    } else if pass == 9 {
        2
    } else {
        0
    }
}
