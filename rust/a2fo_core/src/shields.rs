//! Directional shield policy. The native adapter owns all engine state.
//! Numeric kernels retain the original MinGW x87 expression/spill boundaries.
const AMOUNT_EPSILON: f32 = 0.0001;
const DIRECTION_EPSILON: f32 = 0.000001;

pub(super) fn non_negative(x: f32) -> f32 {
    if x.is_finite() && x > 0.0 { x } else { 0.0 }
}
// std::min/max operand order is observable for NaNs and signed zero.
fn minimum(a: f32, b: f32) -> f32 {
    if b < a { b } else { a }
}
pub(super) fn clamp(x: f32, maximum: f32) -> f32 {
    if !x.is_finite() {
        return 0.0;
    }
    let upper = minimum(x, maximum);
    if 0.0 < upper { upper } else { 0.0 }
}
fn target(x: f32, maximum: f32) -> f32 {
    if !x.is_finite() || x <= 0.0 {
        0.0
    } else {
        minimum(x, maximum)
    }
}
fn active(mode: u32, facing: u32) -> bool {
    match mode {
        1 => true,
        2 => facing != 1,
        3 => facing != 0,
        4 => facing <= 1,
        5 => facing == 2 || facing == 3,
        _ => false,
    }
}
fn normalize(mode: u32, mut p: [f32; 4]) -> [f32; 4] {
    for (i, v) in p.iter_mut().enumerate() {
        *v = if active(mode, i as u32) {
            non_negative(*v)
        } else {
            0.0
        };
    }
    p
}
fn total(p: &[f32; 4]) -> f32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return crate::shield_wide::sum(p).float();
    }
    #[allow(unreachable_code)]
    p.iter()
        .fold(0.0, |sum, x| numeric::add(sum, non_negative(*x)))
}
fn select(mode: u32, right: f32, forward: f32) -> Option<u32> {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return crate::shield_wide::select(mode, right, forward);
    }
    #[allow(unreachable_code)]
    if !(1..=5).contains(&mode) {
        return None;
    }
    let length = numeric::length(right, forward);
    if !length.is_finite() || length <= DIRECTION_EPSILON {
        return None;
    }
    Some(match mode {
        1 => {
            if forward.abs() >= right.abs() {
                u32::from(forward < 0.0)
            } else {
                if right >= 0.0 { 3 } else { 2 }
            }
        }
        2 | 3 => {
            let (port, starboard) = numeric::scores(right, forward, mode == 3);
            let (mut facing, mut score) = if mode == 2 {
                (0, forward)
            } else {
                (1, -forward)
            };
            if port > score {
                facing = 2;
                score = port;
            }
            if starboard > score {
                facing = 3;
            }
            facing
        }
        4 => u32::from(forward < 0.0),
        _ => {
            if right <= 0.0 {
                2
            } else {
                3
            }
        }
    })
}

// SAFETY throughout this private ABI: non-null arrays refer to live, aligned
// storage of the documented length for the duration of the synchronous call.
// Copy inputs before mutating outputs so caller aliasing is harmless.
unsafe fn read4(p: *const f32) -> [f32; 4] {
    unsafe { p.cast::<[f32; 4]>().read() }
}
unsafe fn write4(p: *mut f32, a: [f32; 4]) {
    unsafe { p.cast::<[f32; 4]>().write(a) }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shields_index(facing: u32) -> u32 {
    if facing < 4 { facing } else { 0 }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shields_active(mode: u32, facing: u32) -> u8 {
    u8::from(active(mode, facing))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_normalize(mode: u32, p: *mut f32) {
    if !p.is_null() {
        unsafe { write4(p, normalize(mode, read4(p))) }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_valid(mode: u32, p: *const f32) -> u8 {
    if p.is_null() || !(1..=5).contains(&mode) {
        return 0;
    }
    let p = unsafe { read4(p) };
    u8::from(p.iter().enumerate().all(|(i, &x)| {
        x.is_finite()
            && if active(mode, i as u32) {
                x > 0.0
            } else {
                x == 0.0
            }
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_total(p: *const f32) -> f32 {
    if p.is_null() {
        0.0
    } else {
        total(&unsafe { read4(p) })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_aggregate(
    mode: u32,
    p: *const f32,
    c: *const f32,
    facing: u32,
) -> f32 {
    if p.is_null() || c.is_null() {
        return 0.0;
    }
    let p = normalize(mode, unsafe { read4(p) });
    let c = unsafe { read4(c) };
    let i = a2fo_rs_shields_index(facing) as usize;
    let maximum = total(&p);
    if p[i] <= 0.0 || maximum <= 0.0 {
        return 0.0;
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return crate::shield_wide::aggregate(&p, &c, i).float();
    }
    #[allow(unreachable_code)]
    numeric::aggregate(maximum, clamp(c[i], p[i]), p[i])
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shields_clear(remaining: f32, effect: i32) -> u8 {
    u8::from(effect >= 0 && (!remaining.is_finite() || remaining <= AMOUNT_EPSILON))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_shields_suppress(kind: i32, resolved: u8, remaining: f32) -> u8 {
    u8::from(kind == 0 && resolved != 0 && (!remaining.is_finite() || remaining <= AMOUNT_EPSILON))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_select(
    mode: u32,
    transform: *const f32,
    attacker: *const f32,
    out: *mut u32,
) -> u8 {
    if transform.is_null() || attacker.is_null() || out.is_null() {
        return 0;
    }
    let t = unsafe { transform.cast::<[f32; 12]>().read() };
    let a = unsafe { attacker.cast::<[f32; 3]>().read() };
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let (r, f) = crate::shield_wide::direction(&t, &a);
        if let Some(v) = select(mode, r, f) {
            unsafe { out.write(v) };
            return 1;
        }
        return 0;
    }
    #[allow(unreachable_code)]
    let d = [
        numeric::sub(a[0], t[9]),
        numeric::sub(a[1], t[10]),
        numeric::sub(a[2], t[11]),
    ];
    if let Some(f) = select(mode, numeric::dot(d, &t[0..3]), numeric::dot(d, &t[6..9])) {
        unsafe { out.write(f) };
        1
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_select_local(
    mode: u32,
    transform: *const f32,
    out: *mut u32,
) -> u8 {
    if transform.is_null() || out.is_null() {
        return 0;
    }
    let t = unsafe { transform.cast::<[f32; 12]>().read() };
    if let Some(f) = select(mode, t[9], t[11]) {
        unsafe { out.write(f) };
        1
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_initialize(
    mode: u32,
    p: *const f32,
    c: *mut f32,
    native: f32,
) {
    if p.is_null() || c.is_null() {
        return;
    }
    let p = normalize(mode, unsafe { read4(p) });
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        unsafe { write4(c, crate::shield_wide::initialize(&p, native)) };
        return;
    }
    #[allow(unreachable_code)]
    let maximum = total(&p);
    let target = target(native, maximum);
    let values = if maximum <= 0.0 || target <= 0.0 {
        [0.0; 4]
    } else {
        let ratio = numeric::div(target, maximum);
        p.map(|x| numeric::mul(x, ratio))
    };
    unsafe { write4(c, values) };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shields_reconcile(
    mode: u32,
    p: *const f32,
    c: *mut f32,
    native: f32,
) {
    if p.is_null() || c.is_null() {
        return;
    }
    let p = normalize(mode, unsafe { read4(p) });
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        unsafe { write4(c, crate::shield_wide::reconcile(&p, read4(c), native)) };
        return;
    }
    #[allow(unreachable_code)]
    let mut values = unsafe { read4(c) };
    for i in 0..4 {
        values[i] = clamp(values[i], p[i]);
    }
    let target = target(native, total(&p));
    let current = total(&values);
    if numeric::sub(target, current).abs() <= AMOUNT_EPSILON {
        unsafe { write4(c, values) };
        return;
    }
    if target < current {
        if target <= 0.0 || current <= 0.0 {
            values.fill(0.0);
        } else {
            let ratio = numeric::div(target, current);
            for v in &mut values {
                *v = numeric::mul(*v, ratio);
            }
        }
        unsafe { write4(c, values) };
        return;
    }
    let mut remaining = numeric::sub(target, current);
    for _ in 0..4 {
        if remaining <= AMOUNT_EPSILON {
            break;
        }
        let depleted = (0..4)
            .filter(|&i| numeric::depleted(p[i], values[i]))
            .count();
        if depleted == 0 {
            break;
        }
        let share = numeric::div(remaining, depleted as f32);
        let mut applied = 0.0;
        for i in 0..4 {
            let deficit = numeric::sub(p[i], values[i]);
            if deficit <= AMOUNT_EPSILON {
                continue;
            }
            let addition = minimum(deficit, share);
            values[i] = numeric::add(values[i], addition);
            applied = numeric::add(applied, addition);
        }
        if applied <= AMOUNT_EPSILON {
            break;
        }
        remaining = numeric::sub(remaining, applied);
    }
    for i in 0..4 {
        values[i] = clamp(values[i], p[i]);
    }
    unsafe { write4(c, values) };
}

#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
mod numeric {
    pub fn add(a: f32, b: f32) -> f32 {
        a + b
    }
    pub fn sub(a: f32, b: f32) -> f32 {
        a - b
    }
    pub fn mul(a: f32, b: f32) -> f32 {
        a * b
    }
    pub fn div(a: f32, b: f32) -> f32 {
        a / b
    }
    pub fn length(a: f32, b: f32) -> f32 {
        a * a + b * b
    }
    pub fn dot(d: [f32; 3], t: &[f32]) -> f32 {
        d[0] * t[0] + d[1] * t[1] + d[2] * t[2]
    }
    pub fn scores(r: f32, f: f32, rear: bool) -> (f32, f32) {
        let half = if rear { 0.5 * f } else { -0.5 * f };
        (-0.8660254 * r + half, 0.8660254 * r + half)
    }
    pub fn aggregate(total: f32, current: f32, maximum: f32) -> f32 {
        total * (current / maximum)
    }
    pub fn depleted(maximum: f32, current: f32) -> bool {
        maximum - current > super::AMOUNT_EPSILON
    }
}

#[cfg(all(target_arch = "x86", target_os = "windows"))]
mod numeric {
    use core::arch::asm;
    // Each block uses only bounded local arrays, empties its x87 stack, declares
    // all eight x87 registers clobbered, and leaves the caller's CW unchanged.
    macro_rules! binary {
        ($name:ident, $op:literal) => {
            pub fn $name(a: f32, b: f32) -> f32 {
                let mut v = [a, b, 0.0];
                // SAFETY: two initialized dword inputs and a live dword output.
                unsafe { asm!("fld dword ptr [{p}]", concat!($op, " dword ptr [{p}+4]"), "fstp dword ptr [{p}+8]",
                    p=in(reg)v.as_mut_ptr(), out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
                    out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack)); }
                v[2]
            }
        }
    }
    binary!(add, "fadd");
    binary!(sub, "fsub");
    binary!(mul, "fmul");
    binary!(div, "fdiv");
    pub fn length(a: f32, b: f32) -> f32 {
        let mut v = [a, b, 0.0];
        // SAFETY: bounded local dwords; products remain extended until sum spill.
        unsafe {
            asm!("fld dword ptr [{p}]","fmul st(0),st(0)","fld dword ptr [{p}+4]","fmul st(0),st(0)",
            "faddp st(1),st(0)","fstp dword ptr [{p}+8]",p=in(reg)v.as_mut_ptr(),
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        v[2]
    }
    pub fn dot(d: [f32; 3], t: &[f32]) -> f32 {
        let mut v = [d[0], d[1], d[2], t[0], t[1], t[2], 0.0];
        // SAFETY: bounded local dwords; original left-associated dot expression.
        unsafe {
            asm!("fld dword ptr [{p}]","fmul dword ptr [{p}+12]","fld dword ptr [{p}+4]","fmul dword ptr [{p}+16]",
            "faddp st(1),st(0)","fld dword ptr [{p}+8]","fmul dword ptr [{p}+20]","faddp st(1),st(0)",
            "fstp dword ptr [{p}+24]",p=in(reg)v.as_mut_ptr(),
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        v[6]
    }
    pub fn scores(r: f32, f: f32, rear: bool) -> (f32, f32) {
        let mut v = [r, f, 0.8660254, if rear { 0.5 } else { -0.5 }, 0.0, 0.0];
        // SAFETY: local dwords; each score has one final float spill.
        unsafe {
            asm!("fld dword ptr [{p}]","fmul dword ptr [{p}+8]","fchs","fld dword ptr [{p}+4]",
            "fmul dword ptr [{p}+12]","faddp st(1),st(0)","fstp dword ptr [{p}+16]",
            "fld dword ptr [{p}]","fmul dword ptr [{p}+8]","fld dword ptr [{p}+4]",
            "fmul dword ptr [{p}+12]","faddp st(1),st(0)","fstp dword ptr [{p}+20]",p=in(reg)v.as_mut_ptr(),
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        (v[4], v[5])
    }
    pub fn aggregate(total: f32, current: f32, maximum: f32) -> f32 {
        let mut v = [total, current, maximum, 0.0];
        // SAFETY: local dwords, retaining unspilled division before multiply.
        unsafe {
            asm!("fld dword ptr [{p}+4]","fdiv dword ptr [{p}+8]","fmul dword ptr [{p}]","fstp dword ptr [{p}+12]",
            p=in(reg)v.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        v[3]
    }
    pub fn depleted(maximum: f32, current: f32) -> bool {
        let v = [maximum, current, super::AMOUNT_EPSILON];
        let result: u8;
        // SAFETY: bounded inputs. Original depletion test compares unspilled difference.
        unsafe {
            asm!("fld dword ptr [{p}]","fsub dword ptr [{p}+4]","fld dword ptr [{p}+8]",
            "fxch st(1)","fucomip st(0),st(1)","seta {r}","fstp st(0)",
            p=in(reg)v.as_ptr(),r=out(reg_byte)result,out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        result != 0
    }
}
