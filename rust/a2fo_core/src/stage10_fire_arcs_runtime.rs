//! FireArcs residual admission and snapshot routing. Native callbacks retain
//! object access, storage, Win32 calls, and all exception/lock lifetimes.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

/// # Safety
/// The synchronous native context/callback follows the operation contract in
/// the corresponding C++ adapter and does not unwind through this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_firearc_flow(
    kind: u32,
    context: *mut c_void,
    step: Step,
) -> usize {
    let call = |op| unsafe { step(context, op, 0) };
    match kind {
        0 => usize::from(call(0) != 0 && call(1) != 0), // runtime enabled
        1 => {
            // class-policy lookup; reset output before any guard
            call(0);
            if call(1) == 0 || call(2) == 0 {
                return 0;
            }
            call(3)
        }
        2 => {
            // fail-open geometry; copy both native values before FP work
            if call(0) == 0 {
                return 1;
            }
            let owner = call(1);
            let transform = owner != 0 && call(2) != 0;
            let target = call(3) != 0;
            if !transform || !target {
                return 1;
            }
            call(4)
        }
        3 => {
            // native vector reads remain eager and ordered
            call(0);
            call(1);
            let begin = call(2);
            let end = call(3);
            if begin == 0 || end == 0 || end < begin {
                return 0;
            }
            let bytes = end - begin;
            let count = bytes / 4;
            if bytes % 4 != 0 || count == 0 || count > 256 {
                return 0;
            }
            if unsafe { step(context, 4, count) } == 0 {
                return 0;
            }
            unsafe { step(context, 5, count) };
            1
        }
        4 => {
            // UI colour cache state
            call(0);
            if call(1) != 0 && call(2) != 0 {
                return 0;
            }
            if call(3) != 0 { 2 } else { 1 }
        }
        5 => {
            // UI colour query and private validation before publication
            if call(0) == 0 {
                return 0;
            }
            if call(1) & 0xff == 0 || call(2) == 0 {
                return 0;
            }
            call(3);
            1
        }
        6 => usize::from(call(0) != 0 && call(1) != 0 && call(2) != 0),
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_firearc_colour_index(value: usize) -> u32 {
    match value {
        2 => 2,
        1 => 1,
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_firearc_root_count(value: u32) -> u32 {
    value.min(4097)
}

// These x87 kernels follow GCC's original GNU++17/O2 instruction order,
// including comparisons, caller precision/rounding, and status-word effects.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_firearc_radius(bits: u32) -> u32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let constants = [
            120.0f32.to_bits(),
            500.0f32.to_bits(),
            0x7f7fffff,
            0.35f32.to_bits(),
            80.0f32.to_bits(),
        ];
        let mut result = 0u32;
        unsafe {
            core::arch::asm!(
                "fld dword ptr [{v}]", "fld st(0)", "fabs",
                "fld dword ptr [{c}+8]", "fucomip st(0),st(1)", "fstp st(0)",
                "jb 2f", "fldz", "fcomip st(0),st(1)", "jnb 2f",
                "fmul dword ptr [{c}+12]", "fld dword ptr [{c}+4]",
                "fcomi st(0),st(1)", "ja 3f", "fstp st(1)", "jmp 4f",
                "2:", "fstp st(0)", "fld dword ptr [{c}]", "jmp 4f",
                "3:", "fstp st(0)", "fld dword ptr [{c}+16]", "fxch st(1)",
                "fcomi st(0),st(1)", "fcmovbe st(0),st(1)", "fstp st(1)",
                "4:", "fstp dword ptr [{o}]",
                v=in(reg)&bits,c=in(reg)constants.as_ptr(),o=in(reg)&mut result,
                out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
                out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        result
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let value = f32::from_bits(bits);
        if !value.is_finite() || value <= 0.0 {
            120.0f32.to_bits()
        } else {
            (value * 0.35).clamp(80.0, 500.0).to_bits()
        }
    }
}

/// # Safety
/// `words` points to three writable private f32 bit patterns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_firearc_colour(words: *mut u32) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let maximum = 0x7f7fffffu32;
        let ptr = words;
        let end = unsafe { words.add(3) };
        let valid: u32;
        unsafe {
            core::arch::asm!(
                "fld1", "2:", "fld dword ptr [{p}]", "fld st(0)", "fabs",
                "fld dword ptr [{m}]", "fucomip st(0),st(1)", "fstp st(0)",
                "jb 3f", "fxch st(1)", "fcomi st(0),st(1)", "ja 4f",
                "fstp st(1)", "fld st(0)", "5:", "fstp dword ptr [{p}]",
                "add {p},4", "cmp {p},{e}", "jne 2b", "fstp st(0)",
                "mov {r:e},1", "jmp 6f",
                "3:", "fstp st(0)", "fstp st(0)", "xor {r:e},{r:e}", "jmp 6f",
                "4:", "fldz", "fxch st(2)", "fcomi st(0),st(2)",
                "fcmovbe st(0),st(2)", "fstp st(2)", "fxch st(1)", "jmp 5b", "6:",
                p=inout(reg)ptr=>_,e=in(reg)end,m=in(reg)&maximum,r=out(reg)valid,
                out("st(0)")_,out("st(1)")_,out("st(2)")_,out("st(3)")_,
                out("st(4)")_,out("st(5)")_,out("st(6)")_,out("st(7)")_,options(nostack));
        }
        valid as u8
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        for index in 0..3 {
            let value = f32::from_bits(unsafe { words.add(index).read() });
            if !value.is_finite() {
                return 0;
            }
            let selected = if value > 0.0 { value.min(1.0) } else { 0.0 };
            unsafe { words.add(index).write(selected.to_bits()) };
        }
        1
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[derive(Default)]
    struct State {
        values: [usize; 6],
        calls: std::vec::Vec<(u32, usize)>,
    }
    unsafe extern "C" fn step(p: *mut c_void, op: u32, arg: usize) -> usize {
        let s = unsafe { &mut *p.cast::<State>() };
        s.calls.push((op, arg));
        s.values[op as usize]
    }
    fn flow(kind: u32, s: &mut State) -> usize {
        unsafe { a2fo_rs_stage10_firearc_flow(kind, (s as *mut State).cast(), step) }
    }
    #[test]
    fn fail_open_owner_and_target_reads() {
        let mut s = State::default();
        assert_eq!(flow(2, &mut s), 1);
        assert_eq!(s.calls, [(0, 0)]);
        s.values[0] = 1;
        s.calls.clear();
        assert_eq!(flow(2, &mut s), 1);
        assert_eq!(s.calls, [(0, 0), (1, 0), (3, 0)]);
        s.values = [1, 1, 1, 1, 0, 0];
        s.calls.clear();
        assert_eq!(flow(2, &mut s), 0);
        assert_eq!(s.calls, [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0)]);
    }
    #[test]
    fn vector_reads_and_validation_before_publication() {
        for (begin, end, valid) in [
            (0, 8, false),
            (8, 4, false),
            (8, 8, false),
            (8, 11, false),
            (8, 12, true),
            (8, 1032, true),
            (8, 1036, false),
        ] {
            let mut s = State {
                values: [0, 0, begin, end, 1, 0],
                ..State::default()
            };
            assert_eq!(flow(3, &mut s), usize::from(valid));
            assert_eq!(&s.calls[..4], [(0, 0), (1, 0), (2, 0), (3, 0)]);
            if valid {
                assert_eq!(s.calls[4], (4, (end - begin) / 4));
                assert_eq!(s.calls[5], (5, (end - begin) / 4));
            } else {
                assert_eq!(s.calls.len(), 4);
            }
        }
    }
    #[test]
    fn cache_reloads_and_guards_remain_lazy() {
        let mut s = State {
            values: [0, 1, 1, 0, 0, 0],
            ..State::default()
        };
        assert_eq!(flow(4, &mut s), 0);
        assert_eq!(s.calls, [(0, 0), (1, 0), (2, 0)]);
        s.values[1] = 0;
        s.values[3] = 1;
        s.calls.clear();
        assert_eq!(flow(4, &mut s), 2);
        assert_eq!(s.calls, [(0, 0), (1, 0), (3, 0)]);
        s.values[0] = 0;
        s.calls.clear();
        assert_eq!(flow(0, &mut s), 0);
        assert_eq!(s.calls, [(0, 0)]);
    }
    #[test]
    fn radius_colour_words_and_bounds() {
        for (value, expected) in [(0., 120.), (-1., 120.), (100., 80.), (10000., 500.)] {
            assert_eq!(
                a2fo_rs_stage10_firearc_radius(f32::to_bits(value)),
                f32::to_bits(expected)
            );
        }
        let mut c = [(-0.0f32).to_bits(), (-2.0f32).to_bits(), 2.0f32.to_bits()];
        assert_eq!(unsafe { a2fo_rs_stage10_firearc_colour(c.as_mut_ptr()) }, 1);
        assert_eq!(c, [0, 0, 1.0f32.to_bits()]);
        let mut c = [0.5f32.to_bits(), f32::NAN.to_bits(), 2.0f32.to_bits()];
        assert_eq!(unsafe { a2fo_rs_stage10_firearc_colour(c.as_mut_ptr()) }, 0);
        assert_eq!(c[2], 2.0f32.to_bits());
    }
}
