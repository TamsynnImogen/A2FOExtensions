//! Extended weapon-icon workflows; the host owns native controls and sidecars.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, u32) -> usize;
unsafe fn cleanup(ctx: *mut c_void, s: Step) {
    for i in 0..96 {
        unsafe {
            s(ctx, 7, i);
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_icon_create(ctx: *mut c_void, step: Option<Step>) {
    let Some(s) = step else {
        return;
    };
    // SAFETY: synchronous native context owns all newly allocated controls. Every
    // failure frees only the uncommitted controls, in the original order.
    unsafe {
        if s(ctx, 0, 0) == 0 {
            return;
        }
        if s(ctx, 1, 0) == 0 {
            s(ctx, 11, 0);
            return;
        }
        if s(ctx, 2, 0) == 0 {
            return;
        }
        for i in 0..96 {
            if s(ctx, 3, i) == 0 {
                cleanup(ctx, s);
                s(ctx, 11, 1);
                return;
            }
            s(ctx, 4, i);
            if s(ctx, 5, i) != i as usize + 32 {
                s(ctx, 6, 0);
                cleanup(ctx, s);
                s(ctx, 11, 2);
                return;
            }
            s(ctx, 8, i);
        }
        match s(ctx, 9, 0) {
            0 => {
                cleanup(ctx, s);
                s(ctx, 11, 3);
                return;
            }
            1 => {
                cleanup(ctx, s);
                return;
            }
            _ => {}
        }
        if s(ctx, 10, 0) == 0 {
            s(ctx, 11, 4);
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_icon_destroy(ctx: *mut c_void, step: Option<Step>) {
    let Some(s) = step else {
        return;
    };
    // SAFETY: native context owns the found entry and all native destruction calls.
    unsafe {
        if s(ctx, 0, 0) == 0 {
            return;
        }
        for i in 0..96 {
            s(ctx, 1, i);
        }
        s(ctx, 2, 0);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_icon_virtuals(ctx: *mut c_void, step: Option<Step>) {
    let Some(s) = step else {
        return;
    };
    // SAFETY: get/call primitives retain the native pointer validation and ABI.
    unsafe {
        if s(ctx, 0, 0) == 0 {
            return;
        }
        let craft = s(ctx, 1, 0);
        s(ctx, 2, 0);
        if craft == 0 || s(ctx, 3, 0) == 0 {
            return;
        }
        for i in 0..96 {
            if s(ctx, 4, i) == 0 || s(ctx, 5, i) == 0 {
                continue;
            }
            if s(ctx, 6, i) != 0 {
                s(ctx, 7, i);
            }
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_icon_hook(kind: u32, ctx: *mut c_void, step: Option<Step>) {
    let Some(s) = step else {
        return;
    };
    // SAFETY: readiness is resampled at the same points around the original call;
    // the native adapter retains each exact gateway calling convention.
    unsafe {
        if kind <= 1 && s(ctx, 6, 0) != 0 {
            s(ctx, 0, 0);
        }
        if s(ctx, 1, 0) != 0 {
            s(ctx, 2, 0);
        }
        if kind == 1 {
            if s(ctx, 6, 0) != 0 {
                s(ctx, 3, 0);
            }
        } else if kind >= 2 && s(ctx, 6, 0) != 0 {
            if kind == 6 {
                s(ctx, 5, 0);
            } else {
                s(ctx, 4, 0);
            }
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_icon_install(ctx: *mut c_void, step: Option<Step>) -> u8 {
    let Some(s) = step else {
        return 0;
    };
    // SAFETY: all nine signature checks and all seven installs are attempted in
    // order; no short-circuit skips a later check/installation after failure.
    unsafe {
        if s(ctx, 0, 0) == 0 {
            return 0;
        }
        s(ctx, 1, 0);
        let mut valid = true;
        for i in 0..9 {
            valid = (s(ctx, 2, i) != 0) && valid;
        }
        if !valid {
            s(ctx, 5, 0);
            return 0;
        }
        let mut installed = true;
        for i in 0..7 {
            installed = (s(ctx, 3, i) != 0) && installed;
        }
        s(ctx, 4, u32::from(installed));
        s(ctx, 5, if installed { 1 } else { 2 });
        u8::from(installed)
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_icon_flash(
    phase: f32,
    elapsed: u32,
    state: u8,
    out: *mut f32,
    next: *mut u8,
) {
    if out.is_null() || next.is_null() {
        return;
    }
    let elapsed = f32::from_bits(elapsed);
    let mut state = state;
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    let phase = {
        let mut result = 0f32;
        let mut bits = state as u32;
        // SAFETY: bounded scalar inputs/output. Retain elapsed as the original
        // memory operand (including signaling-NaN precedence), extended loop phase,
        // FCOMI flags, caller CW and a balanced x87 stack.
        unsafe {
            core::arch::asm!("fld dword ptr [{p}]","fadd dword ptr [{e}]","fld1","fxch st(1)",
    "2:","fcomi st(0),st(1)","jbe 3f","fsub st(0),st(1)","xor {s:e},1","jmp 2b",
    "3:","fstp dword ptr [{r}]","fstp st(0)",p=in(reg)&phase,e=in(reg)&elapsed,r=in(reg)&mut result,s=inout(reg)bits,
    out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        state = bits as u8;
        result
    };
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    let phase = {
        let mut p = phase + elapsed;
        while p > 1.0 {
            p -= 1.0;
            state ^= 1;
        }
        p
    };
    // SAFETY: bounded independent caller outputs. The legacy loop intentionally
    // retains its behavior for unsupported non-decreasing infinite/huge inputs.
    unsafe {
        out.write(phase);
        next.write(state);
    }
}
