use alloc::vec::Vec;
use core::{
    ffi::{c_char, c_void},
    slice,
};
#[repr(C)]
#[derive(Clone, Copy)]
struct Monitor {
    name: [u8; 128],
    label: [u8; 128],
    handle: usize,
    flags: u32,
    index: i32,
    left: i32,
    top: i32,
    width: i32,
    height: i32,
    work_left: i32,
    work_top: i32,
    work_right: i32,
    work_bottom: i32,
}
impl Default for Monitor {
    fn default() -> Self {
        unsafe { core::mem::zeroed() }
    }
}
type Visit = unsafe extern "C" fn(*mut c_void, usize) -> i32;
#[repr(C)]
pub struct Api {
    context: *mut c_void,
    action: unsafe extern "C" fn(
        *mut c_void,
        u32,
        usize,
        usize,
        *mut Monitor,
        *const c_char,
        *const c_char,
    ) -> i32,
    enumerate: unsafe extern "C" fn(*mut c_void, Visit, *mut c_void),
}
impl Api {
    fn call(&self, op: u32, a: usize, b: usize, out: &mut Monitor, s: &[u8], t: &[u8]) -> i32 {
        unsafe {
            (self.action)(
                self.context,
                op,
                a,
                b,
                out,
                s.as_ptr().cast(),
                t.as_ptr().cast(),
            )
        }
    }
    fn good(&self) -> bool {
        self.call(0, 0, 0, &mut Monitor::default(), b"\0", b"\0") == 0
    }
}
fn cstr(s: &[u8]) -> &[u8] {
    &s[..s.iter().position(|&v| v == 0).unwrap_or(s.len())]
}
fn terminated(s: &[u8]) -> Vec<u8> {
    let mut v = s.to_vec();
    v.push(0);
    v
}
struct Search<'a> {
    api: &'a Api,
    name: &'a [u8],
    found: Option<Monitor>,
    fallback: bool,
    count: i32,
}
unsafe extern "C" fn visit(p: *mut c_void, h: usize) -> i32 {
    let s = unsafe { &mut *p.cast::<Search<'_>>() };
    let mut m = Monitor::default();
    if s.api.call(3, h, 0, &mut m, b"\0", b"\0") == 0 {
        return 1;
    }
    if s.fallback {
        m.label = m.name;
        m.index = s.count;
        s.count += 1;
        m.flags = (m.flags & 1 != 0) as u32;
        s.api.call(5, 0, 0, &mut m, b"\0", b"\0");
        return s.api.good() as i32;
    }
    if s.api
        .call(14, 0, 0, &mut Monitor::default(), &m.name, s.name)
        != 0
    {
        s.found = Some(m);
        0
    } else {
        1
    }
}
fn find(api: &Api, name: &[u8]) -> Option<Monitor> {
    let mut s = Search {
        api,
        name,
        found: None,
        fallback: false,
        count: 0,
    };
    unsafe { (api.enumerate)(api.context, visit, (&mut s as *mut Search<'_>).cast()) };
    s.found
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_monitor_list(a: *const Api) {
    let a = unsafe { &*a };
    let mut count = 0;
    for i in 0..64 {
        let mut m = Monitor::default();
        if a.call(1, i, 0, &mut m, b"\0", b"\0") == 0 {
            break;
        }
        if m.flags & 1 == 0 || m.flags & 8 != 0 || m.name[0] == 0 {
            continue;
        }
        let name = m.name;
        let mut label = m.label;
        let mut child = 0;
        loop {
            let mut c = Monitor::default();
            if a.call(2, child, 0, &mut c, &name, b"\0") == 0 {
                break;
            }
            if c.label[0] != 0 && c.flags & 8 == 0 {
                label = c.label;
                break;
            }
            child += 1;
        }
        let primary = m.flags & 4 != 0;
        m.index = count;
        count += 1;
        m.label = label;
        if let Some(info) = find(a, &name) {
            m.left = info.left;
            m.top = info.top;
            m.width = info.width;
            m.height = info.height;
            m.flags = (primary || info.flags & 1 != 0) as u32
        } else {
            a.call(4, 0, 0, &mut m, &name, b"\0");
            m.flags = primary as u32;
        }
        a.call(5, 0, 0, &mut m, b"\0", b"\0");
        if !a.good() {
            return;
        }
    }
    if count == 0 {
        let mut s = Search {
            api: a,
            name: b"\0",
            found: None,
            fallback: true,
            count: 0,
        };
        unsafe { (a.enumerate)(a.context, visit, (&mut s as *mut Search<'_>).cast()) };
    }
}
fn path(root: &[u8]) -> Vec<u8> {
    let mut v = root.to_vec();
    if !v.is_empty() && !matches!(v.last(), Some(b'/' | b'\\')) {
        v.push(b'\\')
    }
    v.extend_from_slice(b"A2FORenderer.ini\0");
    v
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_monitor_read(a: *const Api, root: *const u8, n: usize) -> u8 {
    if n == 0 {
        return 0;
    }
    let a = unsafe { &*a };
    let p = path(unsafe { slice::from_raw_parts(root, n) });
    let mut m = Monitor::default();
    a.call(6, 0, 0, &mut m, &p, b"\0");
    let value = cstr(&m.name);
    if value.is_empty() || value == b"<A2FO-unset>" {
        return 0;
    }
    a.call(7, 0, 0, &mut Monitor::default(), &m.name, b"\0");
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_monitor_write(
    a: *const Api,
    root: *const u8,
    n: usize,
    name: *const u8,
    k: usize,
) -> u8 {
    if n == 0 || k == 0 {
        return 0;
    }
    let a = unsafe { &*a };
    let p = path(unsafe { slice::from_raw_parts(root, n) });
    let value = terminated(unsafe { slice::from_raw_parts(name, k) });
    (a.call(8, 0, 0, &mut Monitor::default(), &p, &value) != 0) as u8
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_monitor_window(
    a: *const Api,
    w: usize,
    name: *const u8,
    n: usize,
    check: u8,
) -> u8 {
    let a = unsafe { &*a };
    let mut m = Monitor::default();
    if w == 0 || a.call(9, w, 0, &mut m, b"\0", b"\0") == 0 {
        return 0;
    }
    let name = terminated(unsafe { slice::from_raw_parts(name, n) });
    let Some(info) = find(a, &name) else { return 0 };
    if check != 0 {
        return (a.call(13, w, info.handle, &mut m, b"\0", b"\0") != 0) as u8;
    }
    if a.call(10, w, 0, &mut m, b"\0", b"\0") == 0 || m.width <= 0 || m.height <= 0 {
        return 0;
    }
    let (width, height) = (m.width, m.height);
    a.call(11, w, 0, &mut m, b"\0", b"\0");
    let (left, top) = if m.flags != 0 {
        (
            info.work_left
                + (info
                    .work_right
                    .wrapping_sub(info.work_left)
                    .wrapping_sub(width)
                    / 2)
                .max(0),
            info.work_top
                + (info
                    .work_bottom
                    .wrapping_sub(info.work_top)
                    .wrapping_sub(height)
                    / 2)
                .max(0),
        )
    } else {
        (info.left, info.top)
    };
    m.left = left;
    m.top = top;
    (a.call(12, w, 0, &mut m, b"\0", b"\0") != 0) as u8
}
