//! Production GNU++17 x87 evaluation, including unspilled normalization scale.
use super::{Matrix34, Vec3};
use crate::shield_wide::Wide as W;
fn w(v: f32) -> W {
    W::new(v)
}
fn vector(v: Vec3) -> [W; 3] {
    [w(v.x), w(v.y), w(v.z)]
}
fn output(v: [W; 3]) -> Vec3 {
    Vec3 {
        x: v[0].float(),
        y: v[1].float(),
        z: v[2].float(),
    }
}
fn dot(a: [W; 3], b: [W; 3]) -> W {
    a[0].mul(b[0]).add(a[1].mul(b[1])).add(a[2].mul(b[2]))
}
fn finite(v: W) -> bool {
    v.abs().le(w(f32::MAX))
}
pub(super) fn normalized(value: Vec3, fallback: Vec3) -> Vec3 {
    let mut v = vector(value);
    let mut n = dot(v, v);
    if !finite(n) || n.le(w(super::EPS)) {
        v = vector(fallback);
        n = dot(v, v);
        if !finite(n) || n.le(w(super::EPS)) {
            return Vec3 {
                x: 0.,
                y: 0.,
                z: 1.,
            };
        }
    }
    let scale = w(1.).div(n.sqrt());
    output(v.map(|c| c.mul(scale)))
}
pub(super) fn inverse(m: &Matrix34, value: Vec3) -> Vec3 {
    let v = vector(value);
    let rel = core::array::from_fn(|i| w(v[i].sub(w(m.values[9 + i])).float()));
    let out = core::array::from_fn(|i| {
        let axis = core::array::from_fn(|j| w(m.values[i * 3 + j]));
        let n = dot(axis, axis);
        let n = if w(super::EPS).lt(n) {
            n
        } else {
            w(super::EPS)
        };
        dot(rel, axis).div(n)
    });
    output(out)
}
pub(super) fn length(v: Vec3) -> f32 {
    let v = vector(v);
    dot(v, v).sqrt().float()
}
pub(super) fn constrain_radius(p: &mut Vec3, r: f32) -> u8 {
    let r = if 0. < r { r } else { 0. };
    let v = vector(*p);
    let n = dot(v, v);
    if finite(n) && n.le(w(r).mul(w(r))) {
        return 0;
    }
    if !finite(n) || n.le(w(super::EPS)) || r <= super::EPS {
        *p = Vec3::default();
        return 1;
    }
    let scale = w(r).div(n.sqrt());
    *p = output(v.map(|x| x.mul(scale)));
    1
}
fn max(a: W, b: W) -> W {
    if a.lt(b) { b } else { a }
}
fn min(a: W, b: W) -> W {
    if b.lt(a) { b } else { a }
}
fn clamp(v: W, a: W, b: W) -> W {
    max(a, min(v, b))
}
pub(super) fn advance(p: &mut Vec3, d: &mut Vec3, t: Vec3, s: f32, tr: f32, e: f32, a: f32) -> u8 {
    let e = clamp(w(e), w(0.), w(0.25));
    let s = max(w(0.), w(s));
    let tr = max(w(0.), w(tr));
    let a = max(w(0.001), w(a));
    let pv = vector(*p);
    let tv = vector(t);
    let delta = core::array::from_fn(|i| tv[i].sub(pv[i]));
    let distance = dot(delta, delta).sqrt();
    if !finite(distance) || distance.le(a) {
        *p = t;
        return 1;
    }
    let scale = w(1.).div(distance);
    let dv = delta.map(|x| x.mul(scale));
    let desired = output(dv);
    let blend = clamp(tr.mul(e), w(0.), w(1.));
    let current = vector(*d);
    let keep = w(1.).sub(blend);
    let changed = output(core::array::from_fn(|i| {
        current[i].mul(keep).add(dv[i].mul(blend))
    }));
    *d = normalized(changed, desired);
    let step = s.mul(e);
    if step.add(a).ge(distance) {
        *p = t;
        *d = desired;
        return 1;
    }
    let dir = vector(*d);
    *p = output(core::array::from_fn(|i| pv[i].add(dir[i].mul(step))));
    0
}
pub(super) fn turret_axis(current: f32, desired: f32, rate: f32, elapsed: f32, wraps: bool) -> f32 {
    let mut difference = w(desired).sub(w(current));
    if wraps {
        while difference.gt(w(180.)) {
            difference = difference.sub(w(360.));
        }
        while difference.lt(w(-180.)) {
            difference = difference.add(w(360.));
        }
    }
    let difference_rounded = w(difference.float());
    difference = difference_rounded;
    let maximum = w(super::fmax(0., rate)).mul(w(elapsed));
    if difference.gt(maximum) {
        difference = maximum;
    }
    if difference.lt(maximum.neg()) {
        difference = maximum.neg();
    }
    let mut r = w(current).add(difference);
    if wraps {
        while r.gt(w(180.)) {
            r = r.sub(w(360.));
        }
        while r.lt(w(-180.)) {
            r = r.add(w(360.));
        }
    }
    r.float()
}
fn direction(yaw: W, pitch: W) -> [f32; 3] {
    unsafe extern "C" {
        fn sincosf(v: f32, s: *mut f32, c: *mut f32);
    }
    let mut sy = 0.;
    let mut cy = 0.;
    let mut sp = 0.;
    let mut cp = 0.;
    // SAFETY: native CRT arguments are rounded at the original call boundaries;
    // outputs are live disjoint float slots, exactly as production sincosf.
    unsafe {
        sincosf(yaw.mul(w(super::DEG_TO_RAD)).float(), &mut sy, &mut cy);
        sincosf(pitch.mul(w(super::DEG_TO_RAD)).float(), &mut sp, &mut cp);
    }
    [w(sy).mul(w(cp)).float(), sp, w(cy).mul(w(cp)).float()]
}
pub(super) fn arc_rectangle(
    c: super::ArcConfig,
    edge: usize,
    segment: usize,
    pitch_edge: bool,
    full_yaw: bool,
) -> [f32; 3] {
    let halfyaw = w(c.yaw_angle).mul(w(0.5));
    let halfpitch = w(c.pitch_angle).mul(w(0.5));
    let ymin = w(w(c.yaw).sub(halfyaw).float());
    let ymax = w(w(c.yaw).add(halfyaw).float());
    let pmin = w(clamp(w(c.pitch).sub(halfpitch), w(-90.), w(90.)).float());
    let pmax = w(clamp(w(c.pitch).add(halfpitch), w(-90.), w(90.)).float());
    let (yaw, pitch) = if pitch_edge {
        (
            if edge == 0 { ymin } else { ymax },
            pmin.add(w(pmax.sub(pmin).float()).mul(w(segment as f32).div(w(12.)))),
        )
    } else {
        (
            if full_yaw {
                w(c.yaw).add(w(360.).mul(w(segment as f32).div(w(24.))))
            } else {
                ymin.add(w(c.yaw_angle).mul(w(segment as f32).div(w(24.))))
            },
            if edge == 0 { pmin } else { pmax },
        )
    };
    direction(yaw, pitch)
}
pub(super) fn sphere_angle(segment: usize) -> f32 {
    w(segment as f32)
        .mul(w(2.))
        .mul(w(super::PI))
        .div(w(32.))
        .float()
}
pub(super) fn turret_calculate(
    m: &Matrix34,
    t: &[f32],
    l: super::AimLimits,
    fallback: super::AimAngles,
) -> super::AimAngles {
    let dir = [
        w(t[0]).sub(w(m.values[9])),
        w(t[1]).sub(w(m.values[10])),
        w(t[2]).sub(w(m.values[11])),
    ];
    let axis = |i: usize| dot(dir, core::array::from_fn(|j| w(m.values[i * 3 + j])));
    let x = axis(0);
    let y = axis(1);
    let z = axis(2);
    let horizontal = x.mul(x).add(z.mul(z)).sqrt();
    if horizontal.lt(w(0.0001)) && y.abs().lt(w(0.0001)) {
        return fallback;
    }
    super::AimAngles {
        yaw: super::turret_clamp(
            super::fp::atan_scaled(x.float(), z.float(), super::RAD_TO_DEG),
            l.yaw_min,
            l.yaw_max,
        ),
        pitch: super::turret_clamp(
            super::fp::atan_scaled(y.float(), horizontal.float(), super::RAD_TO_DEG),
            l.pitch_min,
            l.pitch_max,
        ),
    }
}
pub(super) fn shell(
    context: *mut core::ffi::c_void,
    unit: super::UnitCallback,
    minimum: f32,
    maximum: f32,
) -> Vec3 {
    let minimum = super::cpp_max(0., minimum);
    let maximum = super::cpp_max(minimum, maximum);
    let mut direction = [w(0.), w(0.), w(1.)];
    for _ in 0..16 {
        let x = w(-1.).add(w(2.).mul(w(unit(context))));
        let y = w(-1.).add(w(2.).mul(w(unit(context))));
        let n = x.mul(x).add(y.mul(y));
        if n.le(w(super::EPS)) || n.ge(w(1.)) {
            continue;
        }
        let factor = w(1.).sub(n).sqrt().mul(w(2.));
        direction = [
            w(x.mul(factor).float()),
            w(y.mul(factor).float()),
            w(w(1.).sub(w(2.).mul(n)).float()),
        ];
        break;
    }
    let lo = w(minimum).mul(w(minimum)).mul(w(minimum));
    let hi = w(maximum).mul(w(maximum)).mul(w(maximum));
    let argument = lo.add(hi.sub(lo).mul(w(unit(context)))).float();
    unsafe extern "C" {
        fn cbrtf(x: f32) -> f32;
    }
    let radius = W::call_unary(cbrtf, argument);
    output(direction.map(|v| v.mul(radius)))
}
fn cross(a: [W; 3], b: [W; 3]) -> [W; 3] {
    [
        a[1].mul(b[2]).sub(a[2].mul(b[1])),
        a[2].mul(b[0]).sub(a[0].mul(b[2])),
        a[0].mul(b[1]).sub(a[1].mul(b[0])),
    ]
}
fn norm_arc(v: [W; 3]) -> [W; 3] {
    let n = dot(v, v).sqrt();
    if n.le(w(super::DIR_EPS)) {
        v
    } else {
        v.map(|x| x.div(n))
    }
}
pub(super) fn arc_tangents(centre: [f32; 3], reference: [f32; 3]) -> ([f32; 3], [f32; 3]) {
    let c = centre.map(w);
    let a = norm_arc(cross(reference.map(w), c));
    let b = norm_arc(cross(c, a));
    (a.map(W::float), b.map(W::float))
}
fn add(a: [W; 3], b: [W; 3]) -> [W; 3] {
    core::array::from_fn(|i| a[i].add(b[i]))
}
fn sub(a: [W; 3], b: [W; 3]) -> [W; 3] {
    core::array::from_fn(|i| a[i].sub(b[i]))
}
fn mul(a: [W; 3], s: W) -> [W; 3] {
    a.map(|v| v.mul(s))
}
fn narrow(v: [W; 3]) -> [W; 3] {
    v.map(|x| w(x.float()))
}
pub(super) fn sphere(prev: Vec3, p: &mut Vec3, d: &mut Vec3, c: Vec3, r: f32) -> u8 {
    let r = max(w(0.), w(r));
    if r.le(w(super::EPS)) {
        return 0;
    }
    let cv = vector(c);
    let start = sub(vector(prev), cv);
    let end = sub(vector(*p), cv);
    let step = sub(end, start);
    let start2 = dot(start, start);
    let end2 = dot(end, end);
    let r2 = r.mul(r);
    let step2 = dot(step, step);
    let mut contact = !finite(end2) || end2.lt(r2);
    let mut fraction = w(1.);
    if finite(start2) && start2.ge(r2) && finite(step2) && step2.gt(w(super::EPS)) {
        let b = dot(start, step);
        let cc = start2.sub(r2);
        let disc = b.mul(b).sub(step2.mul(cc));
        if b.lt(w(0.)) && disc.ge(w(0.)) {
            let entry = b.neg().sub(disc.sqrt()).div(step2);
            if entry.ge(w(0.)) && entry.le(w(1.)) {
                contact = true;
                fraction = entry;
            }
        }
    }
    if !contact {
        return 0;
    }
    let mut point = *p;
    if fraction.lt(w(1.)) {
        let entry = add(vector(prev), mul(step, fraction));
        let radial = vector(normalized(output(sub(entry, cv)), output(start)));
        let entry = narrow(entry);
        let step = narrow(step);
        let fraction = w(fraction.float());
        let remain = mul(step, w(1.).sub(fraction));
        let tangent = sub(remain, mul(radial, dot(remain, radial)));
        point = output(add(entry, tangent));
    }
    point = super::project(point, c, r.float(), output(start));
    *p = point;
    let radial = vector(normalized(output(sub(vector(point), cv)), output(start)));
    let dv = vector(*d);
    let inward = dot(dv, radial);
    let mut tangent = dv;
    if inward.lt(w(0.)) {
        tangent = sub(tangent, mul(radial, inward));
    }
    if dot(tangent, tangent).le(w(super::EPS)) {
        let mut axis = if radial[0].abs().lt(radial[1].abs()) {
            [w(1.), w(0.), w(0.)]
        } else {
            [w(0.), w(1.), w(0.)]
        };
        if radial[2].abs().lt(dot(radial, axis).abs()) {
            axis = [w(0.), w(0.), w(1.)];
        }
        tangent = cross(axis, radial);
    }
    *d = normalized(
        output(tangent),
        Vec3 {
            x: 0.,
            y: 0.,
            z: 1.,
        },
    );
    1
}
pub(super) fn separate(
    lp: &mut Vec3,
    ld: &mut Vec3,
    rp: &mut Vec3,
    rd: &mut Vec3,
    minimum: f32,
    fallback: Vec3,
) -> u8 {
    let minimum = max(w(0.), w(minimum));
    if minimum.le(w(super::EPS)) {
        return 0;
    }
    let l = vector(*lp);
    let r = vector(*rp);
    let delta = sub(l, r);
    let n = dot(delta, delta);
    if finite(n) && n.ge(minimum.mul(minimum)) {
        return 0;
    }
    let distance = if finite(n) && n.gt(w(super::EPS)) {
        n.sqrt()
    } else {
        w(0.)
    };
    let difference = w(minimum.sub(distance).float());
    let normal = vector(normalized(output(delta), fallback));
    let correction = mul(normal, difference.mul(w(0.5)));
    *lp = output(add(l, correction));
    *rp = output(sub(r, correction));
    let bias = mul(normal, w(0.35));
    *ld = normalized(output(add(vector(*ld), bias)), *ld);
    *rd = normalized(output(sub(vector(*rd), narrow(bias))), *rd);
    1
}
