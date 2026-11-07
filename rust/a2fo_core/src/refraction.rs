//! D3D9 refraction resource and draw workflow. Handles are native and opaque.
// Keep the legacy field order, bounded indices and ABI-shaped arguments auditable.
#![allow(clippy::too_many_arguments)]
use core::{
    ffi::{c_char, c_void},
    ptr,
    sync::atomic::{AtomicUsize, Ordering},
};
#[repr(C)]
pub struct Params {
    clip: [f32; 16],
    lights: [[f32; 4]; 19],
    visibility: f32,
    time: f32,
    hull_floor: f32,
    bend: f32,
    ripple: f32,
    rim: f32,
    front: f32,
    shimmer: f32,
    snapshot_key: u64,
    bounds_center: [f32; 3],
    bounds_extent: [f32; 3],
}
#[repr(C)]
pub struct Draw {
    primitive_type: u32,
    base_vertex: i32,
    min_vertex: u32,
    num_vertices: u32,
    start_index: u32,
    primitive_count: u32,
}
#[repr(C)]
pub struct State {
    source: *mut c_void,
    device: *mut c_void,
    decl: *mut c_void,
    vs: *mut c_void,
    ps: *mut c_void,
    copy: *mut c_void,
    width: u32,
    height: u32,
    format: u32,
    snapshot_key: u64,
    snapshot_target: *mut c_void,
    snapshot_x: u32,
    snapshot_y: u32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Request {
    first: *mut c_void,
    second: *mut c_void,
    floats: *const f32,
    index: u32,
    count: u32,
    value: u32,
    extra: u32,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}
impl Default for Request {
    fn default() -> Self {
        Self {
            first: ptr::null_mut(),
            second: ptr::null_mut(),
            floats: ptr::null(),
            index: 0,
            count: 0,
            value: 0,
            extra: 0,
            left: 0,
            top: 0,
            right: 0,
            bottom: 0,
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Reply {
    hr: i32,
    handle: *mut c_void,
    other: *mut c_void,
    a: u32,
    b: u32,
    c: u32,
    d: u32,
    format: u32,
    version: u32,
    extra: u32,
    detail: *const c_char,
}
impl Default for Reply {
    fn default() -> Self {
        Self {
            hr: 0,
            handle: ptr::null_mut(),
            other: ptr::null_mut(),
            a: 0,
            b: 0,
            c: 0,
            d: 0,
            format: 0,
            version: 0,
            extra: 0,
            detail: ptr::null(),
        }
    }
}
type Call = unsafe extern "C" fn(*mut c_void, u32, *const Request, *mut Reply) -> i32;
fn op(ctx: *mut c_void, cb: Call, id: u32, r: Request) -> Option<Reply> {
    let mut out = Reply::default();
    (unsafe { cb(ctx, id, &r, &mut out) } != 0).then_some(out)
}
fn invoke(ctx: *mut c_void, cb: Call, id: u32) -> Option<Reply> {
    op(ctx, cb, id, Request::default())
}
fn release(ctx: *mut c_void, cb: Call, handle: *mut c_void) {
    if !handle.is_null() {
        let r = Request {
            first: handle,
            ..Request::default()
        };
        let _ = op(ctx, cb, 27, r);
    }
}
fn fail(ctx: *mut c_void, cb: Call, phase: u32, hr: i32) {
    let r = Request {
        index: phase,
        value: hr as u32,
        ..Request::default()
    };
    let _ = op(ctx, cb, 34, r);
}
fn say(ctx: *mut c_void, cb: Call, code: u32) {
    let r = Request {
        index: code,
        ..Request::default()
    };
    let _ = op(ctx, cb, 35, r);
}
fn finite(p: &Params) -> bool {
    p.clip.iter().all(|v| v.is_finite())
        && p.lights.iter().flatten().all(|v| v.is_finite())
        && p.bounds_center.iter().all(|v| v.is_finite())
        && p.bounds_extent.iter().all(|v| v.is_finite() && *v > 0.)
        && [
            p.visibility,
            p.time,
            p.hull_floor,
            p.bend,
            p.ripple,
            p.rim,
            p.front,
            p.shimmer,
        ]
        .iter()
        .all(|v| v.is_finite())
        && (0.0..=1.0).contains(&p.visibility)
        && (0.0..=1.0).contains(&p.hull_floor)
}
// Mirrors the original static, once-only compiler resolution. A successful
// module stays loaded for process lifetime, as in the C++ local static.
static COMPILER: AtomicUsize = AtomicUsize::new(0);
fn compiler(ctx: *mut c_void, cb: Call) -> *mut c_void {
    loop {
        let state = COMPILER.load(Ordering::Acquire);
        if state == 1 {
            core::hint::spin_loop();
            continue;
        }
        if state == 2 {
            return ptr::null_mut();
        }
        if state > 2 {
            return state as *mut c_void;
        }
        if COMPILER
            .compare_exchange(0, 1, Ordering::AcqRel, Ordering::Acquire)
            .is_ok()
        {
            let mut proc = ptr::null_mut();
            for id in [28, 29] {
                if let Some(dll) = invoke(ctx, cb, id)
                    && !dll.handle.is_null()
                {
                    let req = Request {
                        first: dll.handle,
                        ..Request::default()
                    };
                    if let Some(found) = op(ctx, cb, 30, req) {
                        proc = found.handle;
                    }
                    if !proc.is_null() {
                        break;
                    }
                    let _ = op(ctx, cb, 31, req);
                }
            }
            COMPILER.store(
                if proc.is_null() { 2 } else { proc as usize },
                Ordering::Release,
            );
            return proc;
        }
    }
}
fn compile(ctx: *mut c_void, cb: Call, which: u32) -> Option<*mut c_void> {
    let proc = compiler(ctx, cb);
    if proc.is_null() {
        say(ctx, cb, 3);
        return None;
    }
    let req = Request {
        first: proc,
        index: which,
        ..Request::default()
    };
    let result = op(ctx, cb, 32, req)?;
    if result.hr < 0 || result.handle.is_null() {
        let msg = Request {
            first: result.other,
            index: if which == 0 { 11 } else { 12 },
            value: result.hr as u32,
            ..Request::default()
        };
        let _ = op(ctx, cb, 34, msg);
        release(ctx, cb, result.other);
        release(ctx, cb, result.handle);
        return None;
    }
    release(ctx, cb, result.other);
    Some(result.handle)
}
fn shaders(s: &mut State, ctx: *mut c_void, cb: Call) -> bool {
    if !s.vs.is_null() && !s.ps.is_null() && !s.decl.is_null() {
        return true;
    }
    let Some(caps) = invoke(ctx, cb, 2) else {
        return false;
    };
    if caps.hr < 0 {
        fail(ctx, cb, 2, caps.hr);
        return false;
    }
    if caps.a == 0 || caps.b == 0 {
        say(ctx, cb, 2);
        return false;
    }
    let Some(vertex) = compile(ctx, cb, 0) else {
        return false;
    };
    let Some(pixel) = compile(ctx, cb, 1) else {
        release(ctx, cb, vertex);
        return false;
    };
    let decl = invoke(ctx, cb, 3);
    let mut hr = decl.map_or(0x80004005u32 as i32, |r| r.hr);
    if let Some(r) = decl {
        s.decl = r.handle;
    }
    if hr >= 0 {
        let req = Request {
            first: vertex,
            ..Request::default()
        };
        let r = op(ctx, cb, 4, req);
        hr = r.map_or(0x80004005u32 as i32, |v| v.hr);
        if let Some(v) = r {
            s.vs = v.handle;
        }
    }
    if hr >= 0 {
        let req = Request {
            first: pixel,
            ..Request::default()
        };
        let r = op(ctx, cb, 5, req);
        hr = r.map_or(0x80004005u32 as i32, |v| v.hr);
        if let Some(v) = r {
            s.ps = v.handle;
        }
    }
    release(ctx, cb, vertex);
    release(ctx, cb, pixel);
    if hr < 0 {
        fail(ctx, cb, 3, hr);
        release(ctx, cb, s.ps);
        release(ctx, cb, s.vs);
        release(ctx, cb, s.decl);
        s.ps = ptr::null_mut();
        s.vs = ptr::null_mut();
        s.decl = ptr::null_mut();
        return false;
    }
    true
}
fn reset(s: &mut State, ctx: *mut c_void, cb: Call) {
    release(ctx, cb, s.copy);
    release(ctx, cb, s.ps);
    release(ctx, cb, s.vs);
    release(ctx, cb, s.decl);
    release(ctx, cb, s.device);
    s.source = ptr::null_mut();
    s.device = ptr::null_mut();
    s.decl = ptr::null_mut();
    s.vs = ptr::null_mut();
    s.ps = ptr::null_mut();
    s.copy = ptr::null_mut();
    s.width = 0;
    s.height = 0;
    s.format = 0;
    s.snapshot_key = 0;
    s.snapshot_target = ptr::null_mut();
    s.snapshot_x = 0;
    s.snapshot_y = 0;
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_refraction_release(state: *mut State, ctx: *mut c_void, cb: Call) {
    if let Some(s) = unsafe { state.as_mut() } {
        reset(s, ctx, cb);
    }
}
fn set(
    ctx: *mut c_void,
    cb: Call,
    id: u32,
    handle: *mut c_void,
    index: u32,
    count: u32,
    value: u32,
    floats: *const f32,
) -> Option<Reply> {
    op(
        ctx,
        cb,
        id,
        Request {
            first: handle,
            index,
            count,
            value,
            floats,
            ..Request::default()
        },
    )
}
fn draw_inner(
    device8: *mut c_void,
    p: &Params,
    d: &Draw,
    s: &mut State,
    ctx: *mut c_void,
    cb: Call,
) -> u32 {
    if device8.is_null()
        || !finite(p)
        || d.primitive_type != 4
        || d.num_vertices == 0
        || d.primitive_count == 0
        || d.primitive_count > u32::MAX / 3
        || d.base_vertex < 0
    {
        say(ctx, cb, 1);
        return 0;
    }
    if s.source != device8 {
        reset(s, ctx, cb);
        let r = op(
            ctx,
            cb,
            1,
            Request {
                first: device8,
                ..Request::default()
            },
        );
        let hr = r.map_or(0x80004005u32 as i32, |v| v.hr);
        s.device = r.map_or(ptr::null_mut(), |v| v.handle);
        if hr < 0 || s.device.is_null() {
            fail(ctx, cb, 1, hr);
            return 0;
        }
        s.source = device8;
    }
    if !shaders(s, ctx, cb) {
        return 0;
    }
    let mut stream = ptr::null_mut();
    let mut indices = ptr::null_mut();
    let mut hull = ptr::null_mut();
    let mut target = ptr::null_mut();
    let mut hr = 0;
    let mut stride = 0;
    let mut vp = Reply::default();
    let mut rt = Reply::default();
    for id in [7, 8, 9, 10, 11, 12] {
        if hr < 0 {
            break;
        }
        let req = if id == 12 {
            Request {
                first: target,
                ..Request::default()
            }
        } else {
            Request::default()
        };
        let v = op(ctx, cb, id, req);
        hr = v.map_or(0x80004005u32 as i32, |x| x.hr);
        if let Some(x) = v {
            match id {
                7 => {
                    stream = x.handle;
                    stride = x.b;
                }
                8 => indices = x.handle,
                9 => hull = x.handle,
                10 => target = x.handle,
                11 => vp = x,
                12 => rt = x,
                _ => {}
            }
        }
    }
    if hr < 0
        || stream.is_null()
        || indices.is_null()
        || hull.is_null()
        || target.is_null()
        || stride != 68
        || vp.c == 0
        || vp.d == 0
        || vp.a > rt.a
        || vp.b > rt.b
        || vp.c > rt.a - vp.a
        || vp.d > rt.b - vp.b
    {
        fail(ctx, cb, 4, if hr < 0 { hr } else { 0x80070057u32 as i32 });
        release(ctx, cb, stream);
        release(ctx, cb, indices);
        release(ctx, cb, hull);
        release(ctx, cb, target);
        return 0;
    }
    release(ctx, cb, stream);
    release(ctx, cb, indices);
    release(ctx, cb, hull);
    if s.copy.is_null() || s.width != vp.c || s.height != vp.d || s.format != rt.format {
        release(ctx, cb, s.copy);
        s.copy = ptr::null_mut();
        let req = Request {
            index: vp.c,
            count: vp.d,
            value: rt.format,
            ..Request::default()
        };
        let r = op(ctx, cb, 6, req);
        hr = r.map_or(0x80004005u32 as i32, |v| v.hr);
        if let Some(x) = r {
            s.copy = x.handle;
        }
        if hr < 0 {
            fail(ctx, cb, 5, hr);
            release(ctx, cb, target);
            return 0;
        }
        s.width = vp.c;
        s.height = vp.d;
        s.format = rt.format;
        s.snapshot_key = 0;
        s.snapshot_target = ptr::null_mut();
    }
    let refresh = p.snapshot_key == 0
        || s.snapshot_key != p.snapshot_key
        || s.snapshot_target != target
        || s.snapshot_x != vp.a
        || s.snapshot_y != vp.b;
    if refresh {
        let r = op(
            ctx,
            cb,
            13,
            Request {
                first: s.copy,
                ..Request::default()
            },
        );
        hr = r.map_or(0x80004005u32 as i32, |v| v.hr);
        let surface = r.map_or(ptr::null_mut(), |v| v.handle);
        if hr >= 0 {
            let req = Request {
                first: target,
                second: surface,
                left: vp.a as i32,
                top: vp.b as i32,
                right: vp.a.wrapping_add(vp.c) as i32,
                bottom: vp.b.wrapping_add(vp.d) as i32,
                index: vp.c,
                count: vp.d,
                ..Request::default()
            };
            hr = op(ctx, cb, 14, req).map_or(0x80004005u32 as i32, |v| v.hr);
        }
        release(ctx, cb, surface);
        if hr < 0 {
            fail(ctx, cb, 6, hr);
            release(ctx, cb, target);
            s.snapshot_key = 0;
            s.snapshot_target = ptr::null_mut();
            return 0;
        }
        s.snapshot_key = p.snapshot_key;
        s.snapshot_target = target;
        s.snapshot_x = vp.a;
        s.snapshot_y = vp.b;
    }
    release(ctx, cb, target);
    let r = invoke(ctx, cb, 15);
    hr = r.map_or(0x80004005u32 as i32, |v| v.hr);
    let saved = r.map_or(ptr::null_mut(), |v| v.handle);
    if hr >= 0 {
        hr = op(
            ctx,
            cb,
            16,
            Request {
                first: saved,
                ..Request::default()
            },
        )
        .map_or(0x80004005u32 as i32, |v| v.hr);
    }
    if hr < 0 {
        fail(ctx, cb, 7, hr);
        release(ctx, cb, saved);
        return 0;
    }
    let max = p.bounds_extent[0].max(p.bounds_extent[1].max(p.bounds_extent[2]));
    let vs_effect = [p.visibility, p.time, p.ripple, max];
    let vs_bounds = [
        p.bounds_center[0],
        p.bounds_center[1],
        p.bounds_center[2],
        1.0 / max,
    ];
    let ps_effect = [p.visibility, p.time, p.bend, p.ripple];
    let ps_style = [p.rim, p.front, p.hull_floor, p.shimmer];
    let ps_view = [
        vp.c as f32,
        vp.d as f32,
        1.0 / vp.c as f32,
        1.0 / vp.d as f32,
    ];
    let ps_origin = [vp.a as f32, vp.b as f32, 1.0 / max, 0.];
    let mut dirs = [[0.; 4]; 8];
    let mut cols = [[0.; 4]; 8];
    for i in 0..8 {
        dirs[i] = p.lights[2 * i];
        cols[i] = p.lights[2 * i + 1];
    }
    let steps: [(u32, *mut c_void, u32, u32, u32, *const f32); 20] = [
        (19, s.decl, 0, 0, 0, ptr::null()),
        (20, s.vs, 0, 0, 0, ptr::null()),
        (21, s.ps, 0, 0, 0, ptr::null()),
        (22, ptr::null_mut(), 0, 4, 0, p.clip.as_ptr()),
        (22, ptr::null_mut(), 4, 1, 0, vs_effect.as_ptr()),
        (22, ptr::null_mut(), 5, 1, 0, vs_bounds.as_ptr()),
        (23, ptr::null_mut(), 0, 1, 0, ps_effect.as_ptr()),
        (23, ptr::null_mut(), 1, 1, 0, ps_style.as_ptr()),
        (23, ptr::null_mut(), 2, 1, 0, ps_view.as_ptr()),
        (23, ptr::null_mut(), 3, 1, 0, ps_origin.as_ptr()),
        (22, ptr::null_mut(), 6, 8, 0, dirs.as_ptr().cast()),
        (22, ptr::null_mut(), 14, 8, 0, cols.as_ptr().cast()),
        (22, ptr::null_mut(), 22, 1, 0, p.lights[18].as_ptr()),
        (24, s.copy, 1, 0, 0, ptr::null()),
        (25, ptr::null_mut(), 0, 0, 0, ptr::null()),
        (25, ptr::null_mut(), 1, 0, 0, ptr::null()),
        (25, ptr::null_mut(), 2, 0, 0, ptr::null()),
        (25, ptr::null_mut(), 3, 0, 0, ptr::null()),
        (26, ptr::null_mut(), 0, 0, 0, ptr::null()),
        (26, ptr::null_mut(), 1, 0, 0, ptr::null()),
    ];
    let mut attempted = false;
    for (id, handle, index, count, value, floats) in steps {
        hr = set(ctx, cb, id, handle, index, count, value, floats)
            .map_or(0x80004005u32 as i32, |v| v.hr);
        if hr < 0 {
            break;
        }
    }
    if hr >= 0 {
        attempted = true;
        let req = Request {
            index: d.primitive_type,
            count: d.primitive_count,
            value: d.min_vertex,
            extra: d.num_vertices,
            left: d.base_vertex,
            right: d.start_index as i32,
            ..Request::default()
        };
        hr = op(ctx, cb, 18, req).map_or(0x80004005u32 as i32, |v| v.hr);
    }
    let restored = op(
        ctx,
        cb,
        17,
        Request {
            first: saved,
            ..Request::default()
        },
    )
    .map_or(0x80004005u32 as i32, |v| v.hr);
    release(ctx, cb, saved);
    if restored < 0 {
        fail(ctx, cb, 8, restored);
        return 2;
    }
    if hr < 0 {
        fail(ctx, cb, if attempted { 9 } else { 10 }, hr);
        return if attempted { 2 } else { 0 };
    }
    say(ctx, cb, 4);
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_refraction_draw(
    device8: *mut c_void,
    p: *const Params,
    d: *const Draw,
    state: *mut State,
    ctx: *mut c_void,
    cb: Call,
) -> u32 {
    let (Some(p), Some(d), Some(s)) = (unsafe { p.as_ref() }, unsafe { d.as_ref() }, unsafe {
        state.as_mut()
    }) else {
        return 0;
    };
    draw_inner(device8, p, d, s, ctx, cb)
}
