//! GNU++17 production x87 arithmetic: locals remain extended until actual
//! native array/argument stores. Each asm block balances x87 and preserves CW.
use core::arch::asm;
#[derive(Clone, Copy)]
pub(super) struct Wide([u8; 16]);
impl Wide {
    pub(super) fn new(value: f32) -> Self {
        let mut r = Self([0; 16]);
        // SAFETY: initialized scalar input, bounded 80-bit output, stack balanced.
        unsafe {
            asm!("fld dword ptr [{v}]","fstp tbyte ptr [{r}]",v=in(reg)&value,r=in(reg)r.0.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        r
    }
    pub(super) fn float(self) -> f32 {
        let mut r = 0f32;
        // SAFETY: bounded 80-bit input and float output, one balanced conversion.
        unsafe {
            asm!("fld tbyte ptr [{v}]","fstp dword ptr [{r}]",v=in(reg)self.0.as_ptr(),r=in(reg)&mut r,out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        r
    }
    pub(super) fn call_unary(function: unsafe extern "C" fn(f32) -> f32, arg: f32) -> Self {
        let mut r = Self([0; 16]);
        // SAFETY: private i686 cdecl CRT call. Its float argument is in the
        // outgoing stack slot. Capture ST0 before LLVM narrows the declared
        // float result; all volatile registers are declared clobbered. Stack
        // storage is restored exactly, and no Rust references enter the call.
        unsafe {
            asm!(
            "sub esp,16", "mov dword ptr [esp+12],{r}",
            "mov dword ptr [esp],{arg:e}", "call {function}",
            "mov eax,dword ptr [esp+12]", "fstp tbyte ptr [eax]", "add esp,16",
            r=in(reg)r.0.as_mut_ptr(),arg=in(reg)arg.to_bits(),function=in(reg)function,
            clobber_abi("C"),
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _);
        }
        r
    }
    pub(super) fn sqrt(self) -> Self {
        let mut r = Self([0; 16]);
        // SAFETY: bounded extended input/output, balanced x87 stack, CW retained.
        unsafe {
            asm!("fld tbyte ptr [{v}]", "fsqrt", "fstp tbyte ptr [{r}]",
            v=in(reg)self.0.as_ptr(), r=in(reg)r.0.as_mut_ptr(),
            out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
            out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
        }
        r
    }
    pub(super) fn abs(mut self) -> Self {
        self.0[9] &= 0x7f;
        self
    }
    pub(super) fn neg(mut self) -> Self {
        self.0[9] ^= 0x80;
        self
    }
}
macro_rules! arithmetic {
 ($name:ident,$opcode:literal)=>{impl Wide{pub(super) fn $name(self,other:Self)->Self{
  let mut r=Self([0;16]);
  // SAFETY: two bounded extended inputs and output; no control-word mutation.
  unsafe{asm!("fld tbyte ptr [{a}]","fld tbyte ptr [{b}]",$opcode,"fstp tbyte ptr [{r}]",a=in(reg)self.0.as_ptr(),b=in(reg)other.0.as_ptr(),r=in(reg)r.0.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));}r
 }}};
}
arithmetic!(add, "faddp st(1),st(0)");
arithmetic!(sub, "fsubp st(1),st(0)");
arithmetic!(mul, "fmulp st(1),st(0)");
arithmetic!(div, "fdivp st(1),st(0)");
macro_rules! comparison {
 ($name:ident,$condition:literal)=>{impl Wide{pub(super) fn $name(self,other:Self)->bool{
  let result:u8;let ordered:u8;
  // SAFETY: comparison only, balanced stack and explicit unordered exclusion.
  unsafe{asm!("fld tbyte ptr [{b}]","fld tbyte ptr [{a}]","fucomip st(0),st(1)",$condition,"setnp {o}","and {r},{o}","fstp st(0)",a=in(reg)self.0.as_ptr(),b=in(reg)other.0.as_ptr(),r=out(reg_byte)result,o=out(reg_byte)ordered,out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));}let _=ordered;result!=0
 }}};
}
comparison!(gt, "seta {r}");
comparison!(le, "setbe {r}");
comparison!(lt, "setb {r}");
comparison!(ge, "setae {r}");
fn zero() -> Wide {
    Wide::new(0.0)
}
fn min(a: Wide, b: Wide) -> Wide {
    if b.lt(a) { b } else { a }
}
pub(super) fn sum(values: &[f32; 4]) -> Wide {
    values.iter().fold(zero(), |acc, &v| {
        acc.add(Wide::new(crate::shields::non_negative(v)))
    })
}
fn target(v: f32, maximum: Wide) -> Wide {
    if !v.is_finite() || v <= 0.0 {
        zero()
    } else {
        min(Wide::new(v), maximum)
    }
}
pub(super) fn aggregate(p: &[f32; 4], c: &[f32; 4], i: usize) -> Wide {
    let maximum = sum(p);
    if p[i] <= 0.0 || maximum.le(zero()) {
        return zero();
    }
    maximum.mul(Wide::new(crate::shields::clamp(c[i], p[i])).div(Wide::new(p[i])))
}
pub(super) fn initialize(p: &[f32; 4], native: f32) -> [f32; 4] {
    let maximum = sum(p);
    let target = target(native, maximum);
    if maximum.le(zero()) || target.le(zero()) {
        return [0.0; 4];
    }
    let ratio = target.div(maximum);
    p.map(|v| Wide::new(v).mul(ratio).float())
}
pub(super) fn reconcile(p: &[f32; 4], mut values: [f32; 4], native: f32) -> [f32; 4] {
    for i in 0..4 {
        values[i] = crate::shields::clamp(values[i], p[i]);
    }
    let target = target(native, sum(p));
    let current = sum(&values);
    let epsilon = Wide::new(0.0001);
    if target.sub(current).abs().le(epsilon) {
        return values;
    }
    if target.lt(current) {
        if target.le(zero()) || current.le(zero()) {
            return [0.0; 4];
        }
        let ratio = target.div(current);
        for v in &mut values {
            *v = Wide::new(*v).mul(ratio).float();
        }
        return values;
    }
    let mut remaining = target.sub(current);
    for _ in 0..4 {
        if remaining.le(epsilon) {
            break;
        }
        let depleted = (0..4)
            .filter(|&i| Wide::new(p[i]).sub(Wide::new(values[i])).gt(epsilon))
            .count();
        if depleted == 0 {
            break;
        }
        let share = remaining.div(Wide::new(depleted as f32));
        let mut applied = zero();
        for i in 0..4 {
            let deficit = Wide::new(p[i]).sub(Wide::new(values[i]));
            if deficit.le(epsilon) {
                continue;
            }
            let addition = min(deficit, share);
            values[i] = Wide::new(values[i]).add(addition).float();
            applied = applied.add(addition);
        }
        if applied.le(epsilon) {
            break;
        }
        remaining = remaining.sub(applied);
    }
    for i in 0..4 {
        values[i] = crate::shields::clamp(values[i], p[i]);
    }
    values
}
pub(super) fn select(mode: u32, r: f32, f: f32) -> Option<u32> {
    if !(1..=5).contains(&mode) {
        return None;
    }
    let r = Wide::new(r);
    let f = Wide::new(f);
    let length = r.mul(r).add(f.mul(f));
    if !length.abs().le(Wide::new(f32::MAX)) || !length.gt(Wide::new(0.000001)) {
        return None;
    }
    Some(match mode {
        1 => {
            if f.abs().ge(r.abs()) {
                u32::from(f.lt(zero()))
            } else if r.ge(zero()) {
                3
            } else {
                2
            }
        }
        2 | 3 => {
            let c = Wide::new(0.8660254);
            let half = Wide::new(0.5).mul(f);
            let port = if mode == 2 {
                c.neg().mul(r).sub(half)
            } else {
                c.neg().mul(r).add(half)
            };
            let starboard = if mode == 2 {
                c.mul(r).sub(half)
            } else {
                c.mul(r).add(half)
            };
            let (mut facing, mut best) = if mode == 2 { (0, f) } else { (1, f.neg()) };
            if port.gt(best) {
                facing = 2;
                best = port;
            }
            if starboard.gt(best) {
                facing = 3;
            }
            facing
        }
        4 => u32::from(f.lt(zero())),
        _ => {
            if r.le(zero()) {
                2
            } else {
                3
            }
        }
    })
}
pub(super) fn direction(t: &[f32; 12], a: &[f32; 3]) -> (f32, f32) {
    let d = [
        Wide::new(a[0]).sub(Wide::new(t[9])),
        Wide::new(a[1]).sub(Wide::new(t[10])),
        Wide::new(a[2]).sub(Wide::new(t[11])),
    ];
    let dot = |offset: usize| {
        d[0].mul(Wide::new(t[offset]))
            .add(d[1].mul(Wide::new(t[offset + 1])))
            .add(d[2].mul(Wide::new(t[offset + 2])))
            .float()
    };
    (dot(0), dot(6))
}
