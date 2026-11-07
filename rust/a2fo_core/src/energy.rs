//! Ammunition/store policy. No allocation or native object pointers.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Policy {
    maximum: f32,
    recharge_per_second: f32,
    mode: u32,
}

fn normalize(mut p: Policy) -> Policy {
    if !p.maximum.is_finite() || p.maximum < 0.0 {
        p.maximum = 0.0;
    }
    if !p.recharge_per_second.is_finite() || p.recharge_per_second < 0.0 {
        p.recharge_per_second = 0.0;
    }
    if p.mode > 2 {
        p.mode = 1;
    }
    p
}
fn enabled(p: Policy) -> bool {
    p.maximum.is_finite() && p.maximum > 0.0
}
fn clamp(amount: f32, p: Policy) -> f32 {
    let p = normalize(p);
    if !amount.is_finite() {
        return 0.0;
    }
    if amount < 0.0 {
        0.0
    } else if p.maximum < amount {
        p.maximum
    } else {
        amount
    }
}
// Preserve the original MinGW x87 evaluation on Windows, including its
// current control word and extended intermediates. SSE/f64 substitutions can
// change admission or a final f32 bit at a rounding seam.
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn threshold(amount: f32, cost: f32) -> bool {
    x87::threshold(amount, cost)
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn threshold(amount: f32, cost: f32) -> bool {
    amount + 0.0001_f32 >= cost
}
fn can_consume(amount: f32, cost: f32) -> bool {
    amount.is_finite() && cost.is_finite() && cost >= 0.0 && threshold(amount, cost)
}
fn positive(value: f32) -> f32 {
    if 0.0 < value { value } else { 0.0 }
}
fn consume(amount: f32, cost: f32) -> f32 {
    if !amount.is_finite() {
        0.0
    } else if can_consume(amount, cost) {
        positive(amount - cost)
    } else {
        positive(amount)
    }
}
fn can_recharge(p: Policy, in_range: bool) -> bool {
    p.recharge_per_second > 0.0 && p.mode != 0 && (p.mode != 2 || in_range)
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn added(amount: f32, rate: f32, elapsed: f32) -> f32 {
    x87::added(amount, rate, elapsed)
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn added(amount: f32, rate: f32, elapsed: f32) -> f32 {
    amount + rate * elapsed
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn duration(maximum: f32, amount: f32, rate: f32) -> f32 {
    x87::duration(maximum, amount, rate)
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn duration(maximum: f32, amount: f32, rate: f32) -> f32 {
    (maximum - amount) / rate
}
fn recharge(amount: f32, p: Policy, elapsed: f32, in_range: bool) -> f32 {
    let p = normalize(p);
    let amount = clamp(amount, p);
    if !elapsed.is_finite() || elapsed <= 0.0 || !can_recharge(p, in_range) {
        return amount;
    }
    let next = added(amount, p.recharge_per_second, elapsed);
    if next < p.maximum { next } else { p.maximum }
}
fn reload(amount: f32, p: Policy, in_range: bool) -> f32 {
    let p = normalize(p);
    let amount = clamp(amount, p);
    if threshold(amount, p.maximum) {
        return 0.0;
    }
    if !can_recharge(p, in_range) {
        return -1.0;
    }
    duration(p.maximum, amount, p.recharge_per_second)
}
// Safety: non-null policy/output pointers are live, aligned Policy objects.
// Outputs may alias inputs: copy the input before writing.
unsafe fn input(p: *const Policy) -> Policy {
    if p.is_null() {
        Policy {
            maximum: 0.0,
            recharge_per_second: 0.0,
            mode: 1,
        }
    } else {
        unsafe { p.read() }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_energy_normalize(p: *const Policy, out: *mut Policy) {
    if !out.is_null() {
        let value = normalize(unsafe { input(p) });
        unsafe { out.write(value) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_energy_enabled(p: *const Policy) -> u8 {
    u8::from(enabled(unsafe { input(p) }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_energy_requires_resupply(p: *const Policy) -> u8 {
    let p = unsafe { input(p) };
    u8::from(enabled(p) && p.mode == 2)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_energy_clamp(amount: f32, p: *const Policy) -> f32 {
    clamp(amount, unsafe { input(p) })
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_energy_can_consume(amount: f32, cost: f32) -> u8 {
    u8::from(can_consume(amount, cost))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_energy_consume(amount: f32, cost: f32) -> f32 {
    consume(amount, cost)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_energy_recharge(
    amount: f32,
    p: *const Policy,
    elapsed: f32,
    in_range: u8,
) -> f32 {
    recharge(amount, unsafe { input(p) }, elapsed, in_range != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_energy_reload(amount: f32, p: *const Policy, in_range: u8) -> f32 {
    reload(amount, unsafe { input(p) }, in_range != 0)
}

// Rust Reference asm.rules.x86-x87 requires clobbering the entire register
// stack to guarantee empty entry, then leaving it empty on every exit.
// These helpers only borrow local f32 slots; they leave the control word intact.
#[cfg(all(target_arch = "x86", target_os = "windows"))]
mod x87 {
    use core::arch::asm;
    pub fn threshold(amount: f32, cost: f32) -> bool {
        let epsilon = 0.0001_f32;
        let result: u8;
        // SAFETY: every pointer addresses a live local f32. Two loads and
        // two pops balance the x87 stack; all x87 registers are clobbered.
        unsafe {
            asm!(
                "fld dword ptr [{amount}]", "fadd dword ptr [{epsilon}]",
                "fld dword ptr [{cost}]", "fucomip st(0), st(1)",
                "fstp st(0)", "setbe {result}",
                amount=in(reg) &amount, epsilon=in(reg) &epsilon,
                cost=in(reg) &cost, result=out(reg_byte) result,
                out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
                out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
                options(nostack)
            );
        }
        result != 0
    }
    pub fn added(amount: f32, rate: f32, elapsed: f32) -> f32 {
        let mut result = 0.0_f32;
        // SAFETY: all four local slots remain live; one load and one pop
        // balance the stack. No change to x87 rounding/precision control.
        unsafe {
            asm!(
                "fld dword ptr [{rate}]", "fmul dword ptr [{elapsed}]",
                "fadd dword ptr [{amount}]", "fstp dword ptr [{result}]",
                rate=in(reg) &rate, elapsed=in(reg) &elapsed,
                amount=in(reg) &amount, result=in(reg) &mut result,
                out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
                out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
                options(nostack)
            );
        }
        result
    }
    pub fn duration(maximum: f32, amount: f32, rate: f32) -> f32 {
        let mut result = 0.0_f32;
        // SAFETY: live local slots, balanced x87 stack and unchanged control.
        unsafe {
            asm!(
                "fld dword ptr [{maximum}]", "fsub dword ptr [{amount}]",
                "fdiv dword ptr [{rate}]", "fstp dword ptr [{result}]",
                maximum=in(reg) &maximum, amount=in(reg) &amount,
                rate=in(reg) &rate, result=in(reg) &mut result,
                out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
                out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
                options(nostack)
            );
        }
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn invalid_policy_and_self_consistent_stores() {
        let p = normalize(Policy {
            maximum: f32::NAN,
            recharge_per_second: -1.0,
            mode: u32::MAX,
        });
        assert_eq!((p.maximum, p.recharge_per_second, p.mode), (0.0, 0.0, 1));
        let p = Policy {
            maximum: 10.0,
            recharge_per_second: 2.0,
            mode: 2,
        };
        assert_eq!(recharge(3.0, p, 2.0, false), 3.0);
        assert_eq!(recharge(3.0, p, 2.0, true), 7.0);
        assert_eq!(reload(3.0, p, false), -1.0);
        assert_eq!(reload(3.0, p, true), 3.5);
        assert_eq!(recharge(9.0, p, f32::MAX, true), 10.0);
        assert!(!can_consume(1.0, f32::NAN));
        assert_eq!(consume(f32::INFINITY, 2.0), 0.0);
        assert_eq!(clamp(-0.0, p).to_bits(), (-0.0_f32).to_bits());
    }
}
