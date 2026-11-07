//! Residual resupply range arithmetic. Native storage stays in the adapter.

#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn in_range(receiver: [f32; 3], provider: [f32; 3], range: f32) -> bool {
    let values = [
        receiver[0],
        receiver[1],
        receiver[2],
        provider[0],
        provider[1],
        provider[2],
        range,
    ];
    let admitted: u8;
    // SAFETY: seven initialized local dwords, balanced x87 stack, no calls or
    // control-word changes. Preserve GNU++17's extended local differences and
    // left-associated sum until the final unordered-aware comparison. The
    // fsubr/register order and fcomip match the actual GNU++17/O2 callback
    // disassembly (including the native invalid flag on a quiet NaN). An
    // unordered native `distance > range_squared` is false, hence admitted.
    unsafe {
        core::arch::asm!(
            "fld dword ptr [{p}+12]", "fld dword ptr [{p}+16]",
            "fld dword ptr [{p}+20]", "fxch st(2)",
            "fsubr dword ptr [{p}]", "fxch st(1)",
            "fsubr dword ptr [{p}+4]", "fxch st(2)",
            "fsubr dword ptr [{p}+8]", "fld dword ptr [{p}+24]",
            "fxch st(2)", "fmul st(0),st(0)", "fxch st(3)",
            "fmul st(0),st(0)", "faddp st(3),st(0)",
            "fmul st(0),st(0)", "faddp st(2),st(0)",
            "fmul st(0),st(0)", "fxch st(1)",
            "fcomip st(0),st(1)", "fstp st(0)", "setbe {admitted}",
            p = in(reg) values.as_ptr(), admitted = out(reg_byte) admitted,
            out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
            out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
            options(nostack)
        );
    }
    admitted != 0
}

#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn in_range(receiver: [f32; 3], provider: [f32; 3], range: f32) -> bool {
    let x = receiver[0] - provider[0];
    let y = receiver[1] - provider[1];
    let z = receiver[2] - provider[2];
    // A positive admission comparison would reject NaN, unlike the native
    // rejection condition. Keep its explicit unordered behavior.
    #[allow(clippy::neg_cmp_op_on_partial_ord)]
    {
        !(x * x + y * y + z * z > range * range)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_energy_range_admit(
    rx: f32,
    ry: f32,
    rz: f32,
    px: f32,
    py: f32,
    pz: f32,
    range: f32,
) -> u8 {
    u8::from(in_range([rx, ry, rz], [px, py, pz], range))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn range_boundary_and_unordered_admission() {
        assert!(in_range([3.0, 4.0, 0.0], [0.0; 3], 5.0));
        assert!(!in_range([3.0, 4.0, 1.0], [0.0; 3], 5.0));
        assert!(in_range([3.0, 4.0, 0.0], [0.0; 3], -5.0));
        assert!(in_range([0.0; 3], [0.0; 3], 0.0));
        assert!(in_range([f32::NAN, 0.0, 0.0], [0.0; 3], 1.0));
        assert!(in_range([f32::INFINITY, 0.0, 0.0], [0.0; 3], f32::INFINITY));
        assert!(!in_range([f32::INFINITY, 0.0, 0.0], [0.0; 3], 1.0));
    }
}
