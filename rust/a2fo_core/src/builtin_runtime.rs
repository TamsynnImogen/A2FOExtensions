//! Built-in lifecycle planning. Native init/ready and gate procedures remain
//! synchronous callbacks; Rust owns snapshots, atomic gates, and policy logic.
// Keep the legacy field order, bounded indices and ABI-shaped arguments auditable.
#![allow(clippy::needless_range_loop)]
use crate::builtin_state as state;
use core::{
    ffi::c_void,
    slice,
    sync::atomic::{AtomicBool, AtomicU8, AtomicU64, Ordering},
};
const N: usize = 48;
static FLAGS: [AtomicU64; N] = [const { AtomicU64::new(0) }; N];
static STARTED: [AtomicBool; N] = [const { AtomicBool::new(false) }; N];
static OK: [AtomicBool; N] = [const { AtomicBool::new(false) }; N];
static REQUESTED: [AtomicBool; N] = [const { AtomicBool::new(false) }; N];
static INITIAL: [AtomicU8; N] = [const { AtomicU8::new(0) }; N];
static ACCEPTED: [AtomicBool; N] = [const { AtomicBool::new(false) }; N];
static SAVED: [AtomicBool; N] = [const { AtomicBool::new(false) }; N];
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}
#[repr(C)]
pub struct Entry {
    name: Bytes,
    state: u32,
}
type Init = unsafe extern "C" fn(*mut c_void, i32) -> u8;
type Gate = unsafe extern "C" fn(u8);
type Emit = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type Count = unsafe extern "C" fn(*mut c_void) -> usize;
type Get = unsafe extern "C" fn(*mut c_void, usize) -> Entry;
fn index(f: i32) -> Option<usize> {
    (0..N as i32).contains(&f).then_some(f as usize)
}
fn restart(f: i32) -> bool {
    state::a2fo_rs_builtin_requires_restart(f) != 0
}
fn flags(i: usize) -> u64 {
    FLAGS[i].load(Ordering::Acquire)
}
fn activation(i: usize) -> u64 {
    unsafe { state::a2fo_rs_builtin_activation(FLAGS[i].as_ptr()) }
}
fn gates(camera: Gate, view: Gate) {
    unsafe {
        camera(u8::from(activation(9) != 0));
        view(u8::from(activation(10) != 0));
    }
}
fn emit(cb: Emit, c: *mut c_void, v: &[u8]) {
    unsafe { cb(c, v.as_ptr(), v.len()) }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_startup_index(f: i32) -> i32 {
    if restart(f) { f - 24 } else { -1 }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_startup_initialize(f: i32, c: *mut c_void, init: Init) -> u8 {
    let i = a2fo_rs_startup_index(f);
    if i < 0 { 0 } else { unsafe { init(c, i) } }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_startup_selected(
    f: i32,
    c: *mut c_void,
    count: Count,
    get: Get,
) -> u8 {
    if !restart(f) {
        return 0;
    }
    let snapshot = INITIAL[f as usize].load(Ordering::Acquire);
    if snapshot != 0 {
        return u8::from(snapshot & 2 != 0);
    }
    let n = unsafe { count(c) };
    for i in 0..n {
        let e = unsafe { get(c, i) };
        let candidate = unsafe { state::a2fo_rs_builtin_find(e.name.data, e.name.size) };
        if candidate == f {
            return u8::from(matches!(e.state, 1 | 2));
        }
    }
    0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_state(f: i32, kind: u32) -> u64 {
    index(f).map_or(0, |i| if kind == 0 { activation(i) } else { flags(i) })
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_initial(f: i32, out: *mut u8) -> u8 {
    let Some(i) = index(f) else {
        return 0;
    };
    let v = INITIAL[i].load(Ordering::Acquire);
    if v == 0 || out.is_null() {
        return 0;
    }
    unsafe { out.write(u8::from(v & 2 != 0)) };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_prepare(
    f: i32,
    requested: u8,
    c: *mut c_void,
    init: Init,
    ready: Init,
) -> u8 {
    let Some(i) = index(f) else {
        return 0;
    };
    if STARTED[i].swap(true, Ordering::Relaxed) {
        return u8::from(OK[i].load(Ordering::Relaxed));
    }
    let requested = requested != 0;
    REQUESTED[i].store(requested, Ordering::Relaxed);
    INITIAL[i].store(if requested { 3 } else { 1 }, Ordering::Release);
    SAVED[i].store(requested, Ordering::Release);
    let ok = if restart(f) {
        !requested || unsafe { init(c, f) } != 0
    } else {
        let initialized = unsafe { init(c, f) } != 0;
        initialized && (matches!(f, 2 | 3) || unsafe { ready(c, f) } != 0)
    };
    OK[i].store(ok, Ordering::Relaxed);
    if restart(f) {
        ACCEPTED[i].store(requested && ok, Ordering::Release);
    }
    u8::from(ok)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_finish(dispatch: u32, camera: Gate, view: Gate) {
    for i in 0..N {
        let required = match i {
            2 => 1,
            1 => 2,
            4 | 6 => 4,
            3 => 8,
            5 => 4 | 16,
            14 => 32 | 16 | 128,
            15 => 32 | 4 | 2 | 16,
            17 => 64 | 16,
            18 => 4 | 2,
            19 => 32 | 16 | 4 | 2 | 128,
            _ => 0,
        };
        let ok = OK[i].load(Ordering::Relaxed) && (dispatch & required == required);
        OK[i].store(ok, Ordering::Relaxed);
        unsafe {
            state::a2fo_rs_builtin_prepare(
                FLAGS[i].as_ptr(),
                u8::from(ok),
                u8::from(REQUESTED[i].load(Ordering::Relaxed)),
            )
        };
    }
    gates(camera, view);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_roots_match(
    a: *const Bytes,
    n: usize,
    b: *const Bytes,
    m: usize,
) -> u8 {
    if n == 0 || n != m || a.is_null() || b.is_null() {
        return 0;
    }
    let a = unsafe { slice::from_raw_parts(a, n) };
    let b = unsafe { slice::from_raw_parts(b, m) };
    u8::from(a.iter().zip(b).all(
        |(a, b)| unsafe { state::a2fo_rs_builtin_same_root(a.data, a.size, b.data, b.size) } != 0,
    ))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_apply(
    same_roots: u8,
    valid: u8,
    entries: *const Entry,
    n: usize,
    cb: Emit,
    c: *mut c_void,
    camera: Gate,
    view: Gate,
) -> u8 {
    if same_roots == 0 {
        return 1;
    }
    if valid == 0 {
        emit(
            cb,
            c,
            b"Module choices were saved, but policy conflicts prevent applying them.".as_slice(),
        );
        return 0;
    }
    let entries = if n == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(entries, n) }
    };
    let mut desired = [false; N];
    for e in entries {
        let f = unsafe { state::a2fo_rs_builtin_find(e.name.data, e.name.size) };
        if let Some(i) = index(f) {
            desired[i] = matches!(e.state, 1 | 2);
        }
    }
    for i in 0..N {
        if !restart(i as i32) && desired[i] && flags(i) & 2 == 0 {
            let p = state::a2fo_rs_builtin_name(i as i32);
            let mut text = unsafe { core::ffi::CStr::from_ptr(p) }.to_bytes().to_vec();
            text.extend_from_slice(b" is unavailable in this session. Choices were saved; no live changes were applied.".as_slice());
            emit(cb, c, &text);
            return 0;
        }
    }
    for i in 0..N {
        if restart(i as i32) {
            SAVED[i].store(desired[i], Ordering::Release);
        } else {
            unsafe { state::a2fo_rs_builtin_request(FLAGS[i].as_ptr(), u8::from(desired[i])) };
        }
    }
    gates(camera, view);
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_status(
    name: Bytes,
    same_roots: u8,
    cb: Emit,
    c: *mut c_void,
) {
    let f = unsafe { state::a2fo_rs_builtin_find(name.data, name.size) };
    let text = if f < 0 {
        b"Next launch".as_slice()
    } else if same_roots == 0 {
        b"On mod launch".as_slice()
    } else {
        let i = f as usize;
        let v = flags(i);
        if v & 1 == 0 {
            b"Starting".as_slice()
        } else if restart(f) {
            let saved = SAVED[i].load(Ordering::Acquire);
            if saved != REQUESTED[i].load(Ordering::Relaxed) {
                if saved {
                    b"Restart to turn on".as_slice()
                } else {
                    b"Restart to turn off".as_slice()
                }
            } else if v & 2 == 0 {
                b"Unavailable".as_slice()
            } else if activation(i) != 0 {
                b"On (restart required to change)".as_slice()
            } else {
                b"Off (restart required to change)".as_slice()
            }
        } else if v & 2 == 0 {
            b"Unavailable".as_slice()
        } else if activation(i) != 0 {
            b"On (live)".as_slice()
        } else {
            b"Off (live)".as_slice()
        }
    };
    emit(cb, c, text);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_startup_ready(name: Bytes) -> u8 {
    let f = unsafe { state::a2fo_rs_builtin_find(name.data, name.size) };
    if restart(f) {
        u8::from(ACCEPTED[f as usize].load(Ordering::Acquire))
    } else {
        0
    }
}
