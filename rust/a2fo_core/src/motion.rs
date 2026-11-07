// Keep the legacy field order, bounded indices and ABI-shaped arguments auditable.
#![allow(clippy::approx_constant, clippy::needless_range_loop)]
#[path = "motion_legacy.rs"]
mod fp;
#[cfg(all(target_arch = "x86", target_os = "windows"))]
#[path = "motion_production.rs"]
mod production;
// Portable movement, turret, and fire-volume algorithms behind POD C ABI.
use core::ffi::c_void;
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Vec3 {
    x: f32,
    y: f32,
    z: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Matrix34 {
    values: [f32; 12],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AimAngles {
    yaw: f32,
    pitch: f32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AimLimits {
    yaw_min: f32,
    yaw_max: f32,
    pitch_min: f32,
    pitch_max: f32,
    yaw_rate: f32,
    pitch_rate: f32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ArcConfig {
    mode: i32,
    yaw: f32,
    pitch: f32,
    yaw_angle: f32,
    pitch_angle: f32,
    cone_angle: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ArcLine {
    start: [f32; 3],
    end: [f32; 3],
    style: i32,
}
unsafe extern "C" {
    fn sqrtf(x: f32) -> f32;
    fn cbrtf(x: f32) -> f32;
    fn sinf(x: f32) -> f32;
    fn cosf(x: f32) -> f32;
    fn fmodf(x: f32, y: f32) -> f32;
    fn fmaxf(x: f32, y: f32) -> f32;
}
#[inline]
fn sqrt(x: f32) -> f32 {
    unsafe { sqrtf(x) }
}
#[inline]
fn cbrt(x: f32) -> f32 {
    unsafe { cbrtf(x) }
}
#[inline]
fn sin(x: f32) -> f32 {
    unsafe { sinf(x) }
}
#[inline]
fn cos(x: f32) -> f32 {
    unsafe { cosf(x) }
}
#[inline]
fn fmod(x: f32, y: f32) -> f32 {
    unsafe { fmodf(x, y) }
}
#[inline]
fn fmax(x: f32, y: f32) -> f32 {
    unsafe { fmaxf(x, y) }
}
#[inline]
fn cpp_max(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}
#[inline]
fn cpp_min(a: f32, b: f32) -> f32 {
    if b < a { b } else { a }
}
#[inline]
fn clamp(v: f32, low: f32, high: f32) -> f32 {
    cpp_max(low, cpp_min(v, high))
}
const EPS: f32 = 0.000001;
fn dot(a: Vec3, b: Vec3) -> f32 {
    fp::dot([a.x, a.y, a.z], [b.x, b.y, b.z])
}
fn cross(a: Vec3, b: Vec3) -> Vec3 {
    Vec3 {
        x: fp::pair(a.y, b.z, a.z, b.y, true),
        y: fp::pair(a.z, b.x, a.x, b.z, true),
        z: fp::pair(a.x, b.y, a.y, b.x, true),
    }
}
fn add(a: Vec3, b: Vec3) -> Vec3 {
    Vec3 {
        x: a.x + b.x,
        y: a.y + b.y,
        z: a.z + b.z,
    }
}
fn sub(a: Vec3, b: Vec3) -> Vec3 {
    Vec3 {
        x: a.x - b.x,
        y: a.y - b.y,
        z: a.z - b.z,
    }
}
fn mul(v: Vec3, s: f32) -> Vec3 {
    Vec3 {
        x: v.x * s,
        y: v.y * s,
        z: v.z * s,
    }
}
fn length_squared(v: Vec3) -> f32 {
    dot(v, v)
}
fn normalized(v: Vec3, f: Vec3) -> Vec3 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        production::normalized(v, f)
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let n = length_squared(v);
        if !n.is_finite() || n <= EPS {
            let m = length_squared(f);
            if !m.is_finite() || m <= EPS {
                return Vec3 {
                    x: 0.,
                    y: 0.,
                    z: 1.,
                };
            }
            return mul(f, fp::sqrt_ratio(1.0, m));
        }
        mul(v, fp::sqrt_ratio(1.0, n))
    }
}

fn project(p: Vec3, c: Vec3, r: f32, f: Vec3) -> Vec3 {
    let r = cpp_max(0., r);
    if r <= EPS {
        return p;
    }
    let relative = sub(p, c);
    let d = length_squared(relative);
    if d.is_finite() && d >= r * r {
        return p;
    }
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        let n = normalized(relative, f);
        Vec3 {
            x: fp::affine(c.x, r, n.x),
            y: fp::affine(c.y, r, n.y),
            z: fp::affine(c.z, r, n.z),
        }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        add(c, mul(normalized(relative, f), r))
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_random_seed(seed: u32) -> u32 {
    if seed == 0 { 0x6d2b79f5 } else { seed }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_next_u32(state: *mut u32) -> u32 {
    let mut v = unsafe { *state };
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    let v = if v == 0 { 0x6d2b79f5 } else { v };
    unsafe { *state = v };
    v
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_unit(state: *mut u32) -> f32 {
    ((unsafe { a2fo_rs_motion_next_u32(state) } >> 8) as f32) * (1. / 16777216.)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_range(state: *mut u32, mut min: f32, mut max: f32) -> f32 {
    if min > max {
        core::mem::swap(&mut min, &mut max);
    }
    fp::lerp(min, max, unsafe { a2fo_rs_motion_unit(state) })
}
pub type RangeCallback = extern "C" fn(*mut c_void, f32, f32) -> f32;
pub type UnitCallback = extern "C" fn(*mut c_void) -> f32;
#[allow(unreachable_code)]
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_shell(
    context: *mut c_void,
    _range: RangeCallback,
    unit: UnitCallback,
    min: f32,
    max: f32,
) -> Vec3 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::shell(context, unit, min, max);
    }

    let min = cpp_max(0., min);
    let max = cpp_max(min, max);
    let mut direction = Vec3 {
        x: 0.,
        y: 0.,
        z: 1.,
    };
    for _ in 0..16 {
        let x = _range(context, -1., 1.);
        let y = _range(context, -1., 1.);
        let squared = fp::pair(x, x, y, y, false);
        if squared <= EPS || squared >= 1. {
            continue;
        }
        let factor = fp::sqrt_scale(1.0 - squared, 2.0);
        direction = Vec3 {
            x: x * factor,
            y: y * factor,
            z: 1. - 2. * squared,
        };
        break;
    }
    let min3 = fp::cube(min);
    let max3 = fp::cube(max);
    let radius = cbrt(fp::lerp(min3, max3, unit(context)));
    mul(direction, radius)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_length_squared(v: Vec3) -> f32 {
    length_squared(v)
}
#[allow(unreachable_code)]
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_length(v: Vec3) -> f32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::length(v);
    }

    sqrt(length_squared(v))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_normalized(v: Vec3, f: Vec3) -> Vec3 {
    normalized(v, f)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_add(a: Vec3, b: Vec3) -> Vec3 {
    add(a, b)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_subtract(a: Vec3, b: Vec3) -> Vec3 {
    sub(a, b)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_multiply(v: Vec3, s: f32) -> Vec3 {
    mul(v, s)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_project(p: Vec3, c: Vec3, r: f32, f: Vec3) -> Vec3 {
    project(p, c, r, f)
}
fn direction(m: &Matrix34, v: Vec3) -> Vec3 {
    Vec3 {
        x: fp::dot([v.x, v.y, v.z], [m.values[0], m.values[3], m.values[6]]),
        y: fp::dot([v.x, v.y, v.z], [m.values[1], m.values[4], m.values[7]]),
        z: fp::dot([v.x, v.y, v.z], [m.values[2], m.values[5], m.values[8]]),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_transform_point(m: *const Matrix34, v: Vec3) -> Vec3 {
    let m = unsafe { &*m };
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        Vec3 {
            x: fp::dot_affine(
                [v.x, v.y, v.z],
                [m.values[0], m.values[3], m.values[6]],
                1.,
                m.values[9],
                true,
            ),
            y: fp::dot_affine(
                [v.x, v.y, v.z],
                [m.values[1], m.values[4], m.values[7]],
                1.,
                m.values[10],
                true,
            ),
            z: fp::dot_affine(
                [v.x, v.y, v.z],
                [m.values[2], m.values[5], m.values[8]],
                1.,
                m.values[11],
                true,
            ),
        }
    }
    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
    {
        let t = direction(m, v);
        Vec3 {
            x: t.x + m.values[9],
            y: t.y + m.values[10],
            z: t.z + m.values[11],
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_transform_direction(m: *const Matrix34, v: Vec3) -> Vec3 {
    direction(unsafe { &*m }, v)
}
#[unsafe(no_mangle)]
#[allow(unreachable_code)]
pub unsafe extern "C" fn a2fo_rs_motion_inverse_transform_point(
    m: *const Matrix34,
    v: Vec3,
) -> Vec3 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::inverse(unsafe { &*m }, v);
    }
    let m = unsafe { &*m };
    let rel = Vec3 {
        x: v.x - m.values[9],
        y: v.y - m.values[10],
        z: v.z - m.values[11],
    };
    let r = Vec3 {
        x: m.values[0],
        y: m.values[1],
        z: m.values[2],
    };
    let u = Vec3 {
        x: m.values[3],
        y: m.values[4],
        z: m.values[5],
    };
    let f = Vec3 {
        x: m.values[6],
        y: m.values[7],
        z: m.values[8],
    };
    Vec3 {
        x: dot(rel, r) / cpp_max(EPS, length_squared(r)),
        y: dot(rel, u) / cpp_max(EPS, length_squared(u)),
        z: dot(rel, f) / cpp_max(EPS, length_squared(f)),
    }
}
#[allow(unreachable_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_advance(
    p: *mut Vec3,
    d: *mut Vec3,
    t: Vec3,
    s: f32,
    tr: f32,
    e: f32,
    a: f32,
) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::advance(unsafe { &mut *p }, unsafe { &mut *d }, t, s, tr, e, a);
    }

    let p = unsafe { &mut *p };
    let d = unsafe { &mut *d };
    let e = clamp(e, 0., 0.25);
    let s = cpp_max(0., s);
    let tr = cpp_max(0., tr);
    let a = cpp_max(0.001, a);
    let delta = sub(t, *p);
    let distance_squared = length_squared(delta);
    let distance = sqrt(distance_squared);
    if !distance.is_finite() || fp::sqrt_le(distance_squared, a, 0.0) {
        *p = t;
        return 1;
    }
    let desired = mul(delta, fp::sqrt_ratio(1.0, distance_squared));
    let blend = clamp(tr * e, 0., 1.);
    *d = normalized(add(mul(*d, 1. - blend), mul(desired, blend)), desired);
    let step = s * e;
    if fp::sqrt_le(distance_squared, step, a) {
        *p = t;
        *d = desired;
        return 1;
    }
    *p = add(*p, mul(*d, step));
    0
}
#[allow(unreachable_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_constrain_radius(p: *mut Vec3, r: f32) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::constrain_radius(unsafe { &mut *p }, r);
    }

    let p = unsafe { &mut *p };
    let r = cpp_max(0., r);
    let squared = length_squared(*p);
    if squared.is_finite() && squared <= r * r {
        return 0;
    }
    if !squared.is_finite() || squared <= EPS || r <= EPS {
        *p = Vec3::default();
        return 1;
    }
    *p = mul(*p, fp::sqrt_ratio(r, squared));
    1
}
#[allow(unreachable_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_constrain_sphere(
    prev: Vec3,
    p: *mut Vec3,
    d: *mut Vec3,
    c: Vec3,
    r: f32,
) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::sphere(prev, unsafe { &mut *p }, unsafe { &mut *d }, c, r);
    }

    let p = unsafe { &mut *p };
    let d = unsafe { &mut *d };
    let r = cpp_max(0., r);
    if r <= EPS {
        return 0;
    }
    let start = sub(prev, c);
    let end = sub(*p, c);
    let step = sub(end, start);
    let start2 = length_squared(start);
    let end2 = length_squared(end);
    let r2 = r * r;
    let mut contact = !end2.is_finite() || end2 < r2;
    let mut fraction = 1.;
    let step2 = length_squared(step);
    if start2.is_finite() && start2 >= r2 && step2.is_finite() && step2 > EPS {
        let b = dot(start, step);
        let cc = start2 - r2;
        let disc = fp::pair(b, b, step2, cc, true);
        if b < 0. && disc >= 0. {
            let entry = fp::sqrt_entry(b, disc, step2);
            if (0. ..=1.).contains(&entry) {
                contact = true;
                fraction = entry;
            }
        }
    }
    if !contact {
        return 0;
    }
    let mut point = *p;
    if fraction < 1. {
        let entry = add(prev, mul(step, fraction));
        let radial = normalized(sub(entry, c), start);
        let remain = mul(step, 1. - fraction);
        let tangent = sub(remain, mul(radial, dot(remain, radial)));
        point = add(entry, tangent);
    }
    point = project(point, c, r, start);
    *p = point;
    let radial = normalized(sub(point, c), start);
    let inward = dot(*d, radial);
    let mut tangent = *d;
    if inward < 0. {
        tangent = sub(tangent, mul(radial, inward));
    }
    if length_squared(tangent) <= EPS {
        let mut axis = if radial.x.abs() < radial.y.abs() {
            Vec3 {
                x: 1.,
                y: 0.,
                z: 0.,
            }
        } else {
            Vec3 {
                x: 0.,
                y: 1.,
                z: 0.,
            }
        };
        if radial.z.abs() < dot(radial, axis).abs() {
            axis = Vec3 {
                x: 0.,
                y: 0.,
                z: 1.,
            };
        }
        tangent = cross(axis, radial);
    }
    *d = normalized(
        tangent,
        Vec3 {
            x: 0.,
            y: 0.,
            z: 1.,
        },
    );
    1
}
#[allow(unreachable_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_motion_separate(
    lp: *mut Vec3,
    ld: *mut Vec3,
    rp: *mut Vec3,
    rd: *mut Vec3,
    min: f32,
    f: Vec3,
) -> u8 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::separate(
            unsafe { &mut *lp },
            unsafe { &mut *ld },
            unsafe { &mut *rp },
            unsafe { &mut *rd },
            min,
            f,
        );
    }

    let lp = unsafe { &mut *lp };
    let ld = unsafe { &mut *ld };
    let rp = unsafe { &mut *rp };
    let rd = unsafe { &mut *rd };
    let min = cpp_max(0., min);
    if min <= EPS {
        return 0;
    }
    let delta = sub(*lp, *rp);
    let squared = length_squared(delta);
    if squared.is_finite() && squared >= min * min {
        return 0;
    }
    let distance = if squared.is_finite() && squared > EPS {
        sqrt(squared)
    } else {
        0.
    };
    let n = normalized(delta, f);
    let correction = (min - distance) * 0.5;
    *lp = add(*lp, mul(n, correction));
    *rp = sub(*rp, mul(n, correction));
    *ld = normalized(add(*ld, mul(n, 0.35)), *ld);
    *rd = normalized(sub(*rd, mul(n, 0.35)), *rd);
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_motion_facing(p: Vec3, f: Vec3, u: Vec3) -> Matrix34 {
    let f = normalized(
        f,
        Vec3 {
            x: 0.,
            y: 0.,
            z: 1.,
        },
    );
    let u = normalized(
        u,
        Vec3 {
            x: 0.,
            y: 1.,
            z: 0.,
        },
    );
    let mut r = cross(u, f);
    if length_squared(r) <= EPS {
        r = cross(
            Vec3 {
                x: 1.,
                y: 0.,
                z: 0.,
            },
            f,
        );
    }
    let r = normalized(
        r,
        Vec3 {
            x: 1.,
            y: 0.,
            z: 0.,
        },
    );
    let u = normalized(cross(f, r), u);
    Matrix34 {
        values: [r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z, p.x, p.y, p.z],
    }
}
const PI: f32 = 3.141_592_7;
const RAD_TO_DEG: f32 = 180. / PI;
const DEG_TO_RAD: f32 = PI / 180.;
fn turret_clamp(v: f32, mut min: f32, mut max: f32) -> f32 {
    if min > max {
        core::mem::swap(&mut min, &mut max);
    }
    if v < min {
        return min;
    }
    if v > max {
        return max;
    }
    v
}
fn turret_normalize(mut v: f32) -> f32 {
    while v > 180. {
        v -= 360.;
    }
    while v < -180. {
        v += 360.;
    }
    v
}
fn turret_dot(dir: [f32; 3], m: &Matrix34, axis: usize) -> f32 {
    let i = axis * 3;
    fp::dot(dir, [m.values[i], m.values[i + 1], m.values[i + 2]])
}
#[allow(unreachable_code)]
fn turret_axis(current: f32, desired: f32, rate: f32, elapsed: f32, wraps: bool) -> f32 {
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::turret_axis(current, desired, rate, elapsed, wraps);
    }

    let mut difference = desired - current;
    if wraps {
        difference = turret_normalize(difference);
    }
    let maximum = fmax(0., rate) * elapsed;
    if difference > maximum {
        difference = maximum;
    }
    if difference < -maximum {
        difference = -maximum;
    }
    let result = current + difference;
    if wraps {
        turret_normalize(result)
    } else {
        result
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_turret_clamp(v: f32, min: f32, max: f32) -> f32 {
    turret_clamp(v, min, max)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_turret_normalize(v: f32) -> f32 {
    turret_normalize(v)
}
#[allow(unreachable_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_turret_calculate(
    m: *const Matrix34,
    target: *const f32,
    l: AimLimits,
    fallback: AimAngles,
) -> AimAngles {
    if target.is_null() {
        return fallback;
    }
    let m = unsafe { &*m };
    let t = unsafe { core::slice::from_raw_parts(target, 3) };
    #[cfg(all(target_arch = "x86", target_os = "windows"))]
    {
        return production::turret_calculate(m, t, l, fallback);
    }
    let dir = [t[0] - m.values[9], t[1] - m.values[10], t[2] - m.values[11]];
    let x = turret_dot(dir, m, 0);
    let y = turret_dot(dir, m, 1);
    let z = turret_dot(dir, m, 2);
    let horizontal = sqrt(fp::pair(x, x, z, z, false));
    if horizontal < 0.0001 && y.abs() < 0.0001 {
        return fallback;
    }
    AimAngles {
        yaw: turret_clamp(fp::atan_scaled(x, z, RAD_TO_DEG), l.yaw_min, l.yaw_max),
        pitch: turret_clamp(
            fp::atan_scaled(y, horizontal, RAD_TO_DEG),
            l.pitch_min,
            l.pitch_max,
        ),
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_turret_advance(
    mut current: AimAngles,
    desired: AimAngles,
    l: AimLimits,
    elapsed: f32,
) -> AimAngles {
    let elapsed = turret_clamp(elapsed, 0., 1.);
    let wraps = l.yaw_max - l.yaw_min >= 359.;
    current.yaw = turret_axis(current.yaw, desired.yaw, l.yaw_rate, elapsed, wraps);
    current.pitch = turret_axis(current.pitch, desired.pitch, l.pitch_rate, elapsed, false);
    current.yaw = turret_clamp(current.yaw, l.yaw_min, l.yaw_max);
    current.pitch = turret_clamp(current.pitch, l.pitch_min, l.pitch_max);
    current
}
fn combine(m: &Matrix34, r: f32, u: f32, f: f32) -> [f32; 3] {
    let mut out = [0.; 3];
    for i in 0..3 {
        out[i] = fp::dot([r, u, f], [m.values[i], m.values[i + 3], m.values[i + 6]]);
    }
    out
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_turret_compose(m: *const Matrix34, a: AimAngles) -> Matrix34 {
    let m = unsafe { &*m };
    let yaw = a.yaw * DEG_TO_RAD;
    let pitch = a.pitch * DEG_TO_RAD;
    let sy = sin(yaw);
    let cy = cos(yaw);
    let sp = sin(pitch);
    let cp = cos(pitch);
    let r = combine(m, cy, 0., -sy);
    let u = combine(m, -sy * sp, cp, -cy * sp);
    let f = combine(m, sy * cp, sp, cy * cp);
    Matrix34 {
        values: [
            r[0],
            r[1],
            r[2],
            u[0],
            u[1],
            u[2],
            f[0],
            f[1],
            f[2],
            m.values[9],
            m.values[10],
            m.values[11],
        ],
    }
}
const DIR_EPS: f32 = 0.000001;
const BOUND_DEG: f32 = 0.0001;
const BOUND_DOT: f32 = 0.000001;
fn arc_clamp(v: f32, min: f32, max: f32) -> f32 {
    if v < min {
        min
    } else if v > max {
        max
    } else {
        v
    }
}
fn arc_normalize(mut v: f32) -> f32 {
    if !v.is_finite() {
        return 0.;
    }
    v = fmod(v, 360.);
    if v > 180. {
        v -= 360.;
    }
    if v < -180. {
        v += 360.;
    }
    v
}
fn arc_config(mut c: ArcConfig) -> ArcConfig {
    c.yaw = arc_normalize(c.yaw);
    c.pitch = arc_clamp(if c.pitch.is_finite() { c.pitch } else { 0. }, -90., 90.);
    c.yaw_angle = arc_clamp(
        if c.yaw_angle.is_finite() {
            c.yaw_angle
        } else {
            360.
        },
        0.,
        360.,
    );
    c.pitch_angle = arc_clamp(
        if c.pitch_angle.is_finite() {
            c.pitch_angle
        } else {
            180.
        },
        0.,
        180.,
    );
    c.cone_angle = arc_clamp(
        if c.cone_angle.is_finite() {
            c.cone_angle
        } else {
            360.
        },
        0.,
        360.,
    );
    c
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_arc_defer(cannon: u8, primary: u8) -> u8 {
    (cannon != 0 && primary == 0) as u8
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_arc_normalize(v: f32) -> f32 {
    arc_normalize(v)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_arc_normalize_config(c: *mut ArcConfig) {
    if !c.is_null() {
        unsafe {
            *c = arc_config(*c);
        }
    }
}
fn arc_dot(d: [f32; 3], m: &Matrix34, axis: usize) -> f32 {
    let i = axis * 3;
    fp::dot(d, [m.values[i], m.values[i + 1], m.values[i + 2]])
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_arc_allows(
    c: ArcConfig,
    m: *const Matrix34,
    target: *const f32,
) -> u8 {
    if target.is_null() {
        return 0;
    }
    let c = arc_config(c);
    let m = unsafe { &*m };
    let t = unsafe { core::slice::from_raw_parts(target, 3) };
    let d = [t[0] - m.values[9], t[1] - m.values[10], t[2] - m.values[11]];
    let right = arc_dot(d, m, 0);
    let up = arc_dot(d, m, 1);
    let forward = arc_dot(d, m, 2);
    let squared = fp::dot([right, up, forward], [right, up, forward]);
    if !squared.is_finite() || squared <= DIR_EPS {
        return 0;
    }
    if c.mode == 1 {
        if c.cone_angle >= 360. - BOUND_DEG {
            return 1;
        }
        let inv = 1. / sqrt(squared);
        let yaw = c.yaw * DEG_TO_RAD;
        let pitch = c.pitch * DEG_TO_RAD;
        let cp = cos(pitch);
        let cr = sin(yaw) * cp;
        let cu = sin(pitch);
        let cf = cos(yaw) * cp;
        let dot = inv * (right * cr + up * cu + forward * cf);
        let minimum = cos(c.cone_angle * 0.5 * DEG_TO_RAD);
        return (dot + BOUND_DOT >= minimum) as u8;
    }
    let horizontal = sqrt(fp::pair(right, right, forward, forward, false));
    let distance = sqrt(squared);
    let yaw = if horizontal <= distance * DIR_EPS {
        c.yaw
    } else {
        fp::atan_scaled(right, forward, RAD_TO_DEG)
    };
    let pitch = fp::atan_scaled(up, horizontal, RAD_TO_DEG);
    let yaw_diff = arc_normalize(yaw - c.yaw).abs();
    let pitch_diff = (pitch - c.pitch).abs();
    let yaw_ok = c.yaw_angle >= 360. - BOUND_DEG || yaw_diff <= c.yaw_angle * 0.5 + BOUND_DEG;
    let pitch_ok = pitch_diff <= c.pitch_angle * 0.5 + BOUND_DEG;
    (yaw_ok && pitch_ok) as u8
}
fn arc_direction(yaw: f32, pitch: f32) -> [f32; 3] {
    let yaw = yaw * DEG_TO_RAD;
    let pitch = pitch * DEG_TO_RAD;
    let cp = cos(pitch);
    [sin(yaw) * cp, sin(pitch), cos(yaw) * cp]
}
fn world_point(m: &Matrix34, origin: &[f32], direction: [f32; 3], r: f32) -> [f32; 3] {
    let mut out = [0.; 3];
    for i in 0..3 {
        out[i] = fp::dot_affine(
            direction,
            [m.values[i], m.values[i + 3], m.values[i + 6]],
            r,
            origin[i],
            true,
        );
    }
    out
}
struct Lines<'a> {
    output: &'a mut [ArcLine],
    count: usize,
}
impl Lines<'_> {
    fn line(&mut self, start: [f32; 3], end: [f32; 3], style: i32) {
        if self.count >= self.output.len() {
            return;
        }
        self.output[self.count] = ArcLine { start, end, style };
        self.count += 1;
    }
    fn direction(&mut self, m: &Matrix34, origin: &[f32], r: f32, start: [f32; 3], end: [f32; 3]) {
        self.line(
            world_point(m, origin, start, r),
            world_point(m, origin, end, r),
            0,
        );
    }
    fn radial(&mut self, m: &Matrix34, origin: &[f32], r: f32, d: [f32; 3], style: i32) {
        self.line(
            [origin[0], origin[1], origin[2]],
            world_point(m, origin, d, r),
            style,
        );
    }
    fn sphere(&mut self, m: &Matrix34, origin: &[f32], r: f32) {
        for plane in 0..3 {
            let mut previous = [0.; 3];
            for segment in 0..=32 {
                #[cfg(all(target_arch = "x86", target_os = "windows"))]
                let angle = production::sphere_angle(segment);
                #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
                let angle = segment as f32 * 2. * PI / 32.;
                let mut current = [0.; 3];
                if plane == 0 {
                    current[0] = sin(angle);
                    current[2] = cos(angle);
                } else if plane == 1 {
                    current[1] = sin(angle);
                    current[2] = cos(angle);
                } else {
                    current[0] = sin(angle);
                    current[1] = cos(angle);
                }
                if segment != 0 {
                    self.direction(m, origin, r, previous, current);
                }
                previous = current;
            }
        }
    }
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn arc_cross(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [
        fp::pair(a[1], b[2], a[2], b[1], true),
        fp::pair(a[2], b[0], a[0], b[2], true),
        fp::pair(a[0], b[1], a[1], b[0], true),
    ]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn arc_normalize_vector(v: &mut [f32; 3]) {
    let squared = fp::dot(*v, *v);
    let len = sqrt(squared);
    if len <= DIR_EPS {
        return;
    }
    v[0] = fp::sqrt_ratio(v[0], squared);
    v[1] = fp::sqrt_ratio(v[1], squared);
    v[2] = fp::sqrt_ratio(v[2], squared);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_arc_lines(
    c: ArcConfig,
    m: *const Matrix34,
    origin: *const f32,
    r: f32,
    output: *mut ArcLine,
    capacity: usize,
) -> usize {
    if origin.is_null() || output.is_null() || capacity == 0 || !r.is_finite() || r <= 0. {
        return 0;
    }
    let c = arc_config(c);
    let m = unsafe { &*m };
    let origin = unsafe { core::slice::from_raw_parts(origin, 3) };
    let output = unsafe { core::slice::from_raw_parts_mut(output, capacity) };
    let mut lines = Lines { output, count: 0 };
    let centre = arc_direction(c.yaw, c.pitch);
    if c.mode == 1 {
        if c.cone_angle >= 360. - BOUND_DEG {
            lines.sphere(m, origin, r);
        } else if c.cone_angle > BOUND_DEG {
            let half = c.cone_angle * 0.5 * DEG_TO_RAD;
            let ch = cos(half);
            let sh = sin(half);
            let reference = if centre[1].abs() > 0.95 {
                [1., 0., 0.]
            } else {
                [0., 1., 0.]
            };
            #[cfg(all(target_arch = "x86", target_os = "windows"))]
            let (ta, tb) = production::arc_tangents(centre, reference);
            #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
            let (ta, tb) = {
                let mut ta = arc_cross(reference, centre);
                arc_normalize_vector(&mut ta);
                let mut tb = arc_cross(centre, ta);
                arc_normalize_vector(&mut tb);
                (ta, tb)
            };
            let mut previous = [0.; 3];
            for segment in 0..=32 {
                #[cfg(all(target_arch = "x86", target_os = "windows"))]
                let around = production::sphere_angle(segment);
                #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
                let around = segment as f32 * 2. * PI / 32.;
                let mut current = [0.; 3];
                for i in 0..3 {
                    current[i] = fp::cone_component(
                        centre[i],
                        ch,
                        ta[i],
                        cos(around),
                        tb[i],
                        sin(around),
                        sh,
                    );
                }
                if segment != 0 {
                    lines.direction(m, origin, r, previous, current);
                }
                if segment < 32 && segment % 8 == 0 {
                    lines.radial(m, origin, r, current, 0);
                }
                previous = current;
            }
        }
    } else {
        let half_yaw = c.yaw_angle * 0.5;
        let half_pitch = c.pitch_angle * 0.5;
        let minimum_yaw = c.yaw - half_yaw;
        let maximum_yaw = c.yaw + half_yaw;
        let minimum_pitch = arc_clamp(c.pitch - half_pitch, -90., 90.);
        let maximum_pitch = arc_clamp(c.pitch + half_pitch, -90., 90.);
        let full_yaw = c.yaw_angle >= 360. - BOUND_DEG;
        let full_pitch = minimum_pitch <= -90. + BOUND_DEG && maximum_pitch >= 90. - BOUND_DEG;
        if full_yaw && full_pitch {
            lines.sphere(m, origin, r);
        } else {
            for edge in 0..2 {
                #[allow(unused_variables)]
                let pitch = if edge == 0 {
                    minimum_pitch
                } else {
                    maximum_pitch
                };
                let mut previous = [0.; 3];
                for segment in 0..=24 {
                    let fraction = segment as f32 / 24.;
                    #[allow(unused_variables)]
                    let yaw = if full_yaw {
                        fp::affine(c.yaw, 360.0, fraction)
                    } else {
                        fp::affine(minimum_yaw, c.yaw_angle, fraction)
                    };
                    #[cfg(all(target_arch = "x86", target_os = "windows"))]
                    let current = production::arc_rectangle(c, edge, segment, false, full_yaw);
                    #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
                    let current = arc_direction(yaw, pitch);
                    if segment != 0 {
                        lines.direction(m, origin, r, previous, current);
                    }
                    previous = current;
                }
            }
            if !full_yaw {
                for edge in 0..2 {
                    #[allow(unused_variables)]
                    let yaw = if edge == 0 { minimum_yaw } else { maximum_yaw };
                    let mut previous = [0.; 3];
                    for segment in 0..=12 {
                        let fraction = segment as f32 / 12.;
                        #[allow(unused_variables)]
                        let pitch = fp::lerp(minimum_pitch, maximum_pitch, fraction);
                        #[cfg(all(target_arch = "x86", target_os = "windows"))]
                        let current = production::arc_rectangle(c, edge, segment, true, full_yaw);
                        #[cfg(not(all(target_arch = "x86", target_os = "windows")))]
                        let current = arc_direction(yaw, pitch);
                        if segment != 0 {
                            lines.direction(m, origin, r, previous, current);
                        }
                        previous = current;
                    }
                }
                for yaw in [minimum_yaw, maximum_yaw] {
                    for pitch in [minimum_pitch, maximum_pitch] {
                        lines.radial(m, origin, r, arc_direction(yaw, pitch), 0);
                    }
                }
            }
        }
    }
    lines.radial(m, origin, r, centre, 1);
    lines.count
}
