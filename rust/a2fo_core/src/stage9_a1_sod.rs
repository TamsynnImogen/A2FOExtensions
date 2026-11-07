//! A1 SOD bounded mesh traversal and exact GNU i686 x87 bounds kernel.
use core::ffi::c_void;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Stats {
    visited: usize,
    mesh_nodes: usize,
    changed: usize,
    invalid: usize,
}
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;
fn repair(lo: [u32; 3], hi: [u32; 3]) -> Option<[u32; 6]> {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        #[repr(C)]
        struct Kernel {
            lo: [u32; 3],
            hi: [u32; 3],
            out: [u32; 6],
            max: u32,
            half: u32,
            inflation: u32,
            valid: u32,
        }
        let mut k = Kernel {
            lo,
            hi,
            out: [0; 6],
            max: 0x7f7fffff,
            half: 0x3f000000,
            inflation: 0x40800000,
            valid: 0,
        };
        // Recorded from the canonical GNU++17/O2 DLL. No control-word changes,
        // no SSE substitutions or intermediate f32 rounding are introduced.
        unsafe {
            core::arch::asm!(
            "xor eax,eax",
            "2:","fld dword ptr [{p}+eax*4]","fld st(0)","fabs",
            "fld dword ptr [{p}+48]","fucomi st(0),st(1)","fstp st(1)","jb 5f",
            "fld dword ptr [{p}+12+eax*4]","fld st(0)","fabs","fxch st(2)",
            "fucomip st(0),st(2)","fstp st(1)","jb 5f",
            "fxch st(1)","fcomip st(0),st(1)","fstp st(0)","ja 6f",
            "inc eax","cmp eax,3","jne 2b",
            "xor eax,eax",
            "3:","fld dword ptr [{p}+eax*4]","fld dword ptr [{p}+12+eax*4]",
            "fld st(1)","fadd st(0),st(1)","fld dword ptr [{p}+52]",
            "fmul st(1),st(0)","fxch st(2)","fsubrp st(3),st(0)","fxch st(2)",
            "fmul st(0),st(1)","fxch st(1)","fcomi st(0),st(1)",
            "fcmovbe st(0),st(1)","fstp st(1)","fmul dword ptr [{p}+56]",
            "fld st(1)","fsub st(0),st(1)","fstp dword ptr [{p}+24+eax*4]",
            "faddp st(1),st(0)","fstp dword ptr [{p}+36+eax*4]",
            "inc eax","cmp eax,3","jne 3b","mov dword ptr [{p}+60],1","jmp 6f",
            "5:","fstp st(0)","fstp st(0)","6:",
            p=in(reg)&mut k,out("eax")_,
            out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
            out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        if k.valid != 0 { Some(k.out) } else { None }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let l = lo.map(f32::from_bits);
        let h = hi.map(f32::from_bits);
        for a in 0..3 {
            if !l[a].is_finite() || !h[a].is_finite() || h[a] < l[a] {
                return None;
            }
        }
        let mut out = [0; 6];
        for a in 0..3 {
            let centre = (l[a] + h[a]) * 0.5;
            let half = ((h[a] - l[a]) * 0.5).max(0.5) * 4.0;
            out[a] = (centre - half).to_bits();
            out[a + 3] = (centre + half).to_bits();
        }
        Some(out)
    }
}
unsafe fn walk(node: usize, context: *mut c_void, step: Step, stats: &mut Stats) {
    if node == 0 || stats.visited >= 8192 {
        return;
    }
    stats.visited = stats.visited.wrapping_add(1);
    let ty = unsafe { step(context, 0, node, 0) } as i32;
    if ty == 1 || ty == 9 {
        stats.mesh_nodes = stats.mesh_nodes.wrapping_add(1);
        if unsafe { step(context, 1, node, 0) } != 0 {
            let mut lo = [0; 3];
            let mut hi = [0; 3];
            for (a, value) in lo.iter_mut().enumerate() {
                *value = unsafe { step(context, 2, node, a) } as u32;
            }
            for (a, value) in hi.iter_mut().enumerate() {
                *value = unsafe { step(context, 3, node, a) } as u32;
            }
            if let Some(output) = repair(lo, hi) {
                unsafe { step(context, 4, node, output.as_ptr() as usize) };
                stats.changed = stats.changed.wrapping_add(1);
            } else {
                stats.invalid = stats.invalid.wrapping_add(1);
            }
        } else {
            stats.invalid = stats.invalid.wrapping_add(1);
        }
    }
    let mut child = unsafe { step(context, 5, node, 0) } as usize;
    while child != 0 && stats.visited < 8192 {
        let next = unsafe { step(context, 6, child, 0) } as usize;
        unsafe { walk(child, context, step, stats) };
        child = next;
    }
}
/// # Safety
/// Stats is private caller-owned state, disjoint from the native graph. Native
/// callbacks cannot alias it and validate all node accesses. The callback and
/// context stay live synchronously; output arrays are consumed within op4.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_sod_walk(
    node: usize,
    context: *mut c_void,
    step: Step,
    stats: *mut Stats,
) -> u8 {
    if node == 0 || stats.is_null() {
        return 0;
    }
    let mut local = unsafe { stats.read() };
    if local.visited >= 8192 {
        return 0;
    }
    unsafe {
        walk(node, context, step, &mut local);
        stats.write(local)
    };
    u8::from(local.changed != 0)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn thin_box() {
        assert_eq!(
            repair([0; 3], [0; 3]),
            Some(
                [0xc0000000; 3]
                    .into_iter()
                    .chain([0x40000000; 3])
                    .collect::<std::vec::Vec<_>>()
                    .try_into()
                    .unwrap()
            )
        );
    }
    #[test]
    fn invalid_box() {
        assert_eq!(repair([1.0f32.to_bits(); 3], [0; 3]), None);
        assert_eq!(repair([f32::NAN.to_bits(); 3], [0; 3]), None);
    }
}
