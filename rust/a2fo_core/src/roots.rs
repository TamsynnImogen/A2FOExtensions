//! Fleet Ops overlay roots and settings path policy. Byte-oriented, CRT locale.
use alloc::{collections::BTreeSet, vec::Vec};
use core::{
    ffi::{c_char, c_int, c_long, c_void},
    slice,
};
#[repr(C)]
pub struct Bytes {
    p: *const u8,
    n: usize,
}
type Emit = unsafe extern "C" fn(*mut c_void, u32, *const u8, usize);
type Directory = unsafe extern "C" fn(*mut c_void, *const u8, usize) -> u8;
type Read = unsafe extern "C" fn(*mut c_void, *const u8, usize, Emit, *mut c_void);
unsafe extern "C" {
    fn isspace(c: c_int) -> c_int;
    fn tolower(c: c_int) -> c_int;
    fn isalpha(c: c_int) -> c_int;
    fn strtol(p: *const c_char, end: *mut *mut c_char, base: c_int) -> c_long;
}
#[cfg(target_os = "windows")]
unsafe extern "C" {
    fn _errno() -> *mut c_int;
}
#[cfg(not(target_os = "windows"))]
unsafe extern "C" {
    fn __errno_location() -> *mut c_int;
}
fn errno() -> *mut c_int {
    #[cfg(target_os = "windows")]
    {
        unsafe { _errno() }
    }
    #[cfg(not(target_os = "windows"))]
    {
        unsafe { __errno_location() }
    }
}
impl Bytes {
    unsafe fn get(&self) -> &[u8] {
        if self.n == 0 {
            &[]
        } else {
            unsafe { slice::from_raw_parts(self.p, self.n) }
        }
    }
}
fn space(b: u8) -> bool {
    unsafe { isspace(b as c_int) != 0 }
}
fn lower(v: &[u8]) -> Vec<u8> {
    v.iter()
        .map(|&b| unsafe { tolower(b as c_int) as u8 })
        .collect()
}
fn trim(mut v: &[u8]) -> &[u8] {
    while v.first().is_some_and(|&b| space(b)) {
        v = &v[1..];
    }
    while v.last().is_some_and(|&b| space(b)) {
        v = &v[..v.len() - 1];
    }
    v
}
fn unquote(v: &[u8]) -> &[u8] {
    let v = trim(v);
    if v.len() >= 2
        && ((v[0] == b'"' && v[v.len() - 1] == b'"') || (v[0] == b'\'' && v[v.len() - 1] == b'\''))
    {
        trim(&v[1..v.len() - 1])
    } else {
        trim(v)
    }
}
fn info<'a>(v: &'a [u8], key: &[u8]) -> &'a [u8] {
    let wanted = lower(key);
    let mut in_mod = false;
    for line in v.split(|&b| b == b'\n') {
        let line = trim(line);
        if line.is_empty() || matches!(line[0], b';' | b'#') {
            continue;
        }
        if line[0] == b'[' && line.last() == Some(&b']') {
            in_mod = lower(trim(&line[1..line.len() - 1])) == b"mod";
            continue;
        }
        if !in_mod {
            continue;
        }
        if let Some(i) = line.iter().position(|&b| b == b'=')
            && lower(trim(&line[..i])) == wanted
        {
            return unquote(&line[i + 1..]);
        }
    }
    &[]
}
fn active(v: &[u8]) -> Vec<u8> {
    for i in 0..v.len() {
        if i + 4 > v.len() || !matches!(v[i], b'/' | b'-') || lower(&v[i + 1..i + 4]) != b"mod" {
            continue;
        }
        if i != 0 && !space(v[i - 1]) && !matches!(v[i - 1], b'"' | b'\'') {
            continue;
        }
        if i + 4 < v.len() && !space(v[i + 4]) && !matches!(v[i + 4], b'=' | b':' | b'"' | b'\\') {
            continue;
        }
        let mut c = i + 4;
        while c < v.len() && space(v[c]) {
            c += 1;
        }
        if c < v.len() && matches!(v[c], b'=' | b':') {
            c += 1;
            while c < v.len() && space(v[c]) {
                c += 1;
            }
        }
        let mut escaped = false;
        let mut quote = 0;
        if c + 1 < v.len() && &v[c..c + 2] == b"\\\"" {
            escaped = true;
            quote = b'"';
            c += 2;
        } else if c < v.len() && matches!(v[c], b'"' | b'\'') {
            quote = v[c];
            c += 1;
        }
        let start = c;
        if quote != 0 {
            while c < v.len() {
                if escaped && c + 1 < v.len() && v[c] == b'\\' && v[c + 1] == quote {
                    break;
                }
                if !escaped && v[c] == quote {
                    break;
                }
                c += 1;
            }
        } else {
            while c < v.len() && !space(v[c]) && v[c] != b'"' {
                c += 1;
            }
        }
        return trim(&v[start..c]).to_vec();
    }
    Vec::new()
}
fn safe(v: &[u8]) -> bool {
    let v = trim(v);
    !v.is_empty()
        && v != b"."
        && v != b".."
        && !v.iter().any(|&b| b < 0x20 || b"/\\:*?\"<>|".contains(&b))
}
fn absolute(v: &[u8]) -> bool {
    (v.len() >= 3
        && unsafe { isalpha(v[0] as c_int) != 0 }
        && v[1] == b':'
        && matches!(v[2], b'\\' | b'/'))
        || (v.len() >= 2 && matches!(v[0], b'\\' | b'/') && matches!(v[1], b'\\' | b'/'))
}
fn normalize(v: &[u8]) -> Vec<u8> {
    let mut v: Vec<u8> = v
        .iter()
        .map(|&b| if b == b'/' { b'\\' } else { b })
        .collect();
    if !v.is_empty() && v.last() != Some(&b'\\') {
        v.push(b'\\');
    }
    v
}
fn settings(normal: &[u8], configured: &[u8], fallback: &[u8], shared: &[u8]) -> Vec<u8> {
    let config = trim(configured);
    if config.is_empty() {
        return Vec::new();
    }
    let mut config = normalize(config);
    if absolute(&config) {
        return config;
    }
    let bare = config.iter().position(|&b| b == b'\\') == Some(config.len() - 1);
    if bare && !trim(shared).is_empty() {
        let mut root = normalize(trim(shared));
        if !absolute(&root) {
            root = settings(normal, &root, fallback, &[]);
        }
        if !root.is_empty() {
            root.extend_from_slice(b"mods\\");
            root.extend_from_slice(&config);
            return normalize(&root);
        }
    }
    let mut base = normalize(normal);
    if let Some(i) = lower(&base).windows(6).rposition(|w| w == b"\\mods\\") {
        base.truncate(i + 1);
    } else {
        base = normalize(fallback);
    }
    if base.is_empty() {
        return Vec::new();
    }
    if bare {
        base.extend_from_slice(b"mods\\");
    }
    base.append(&mut config);
    normalize(&base)
}
fn send(cb: Emit, ctx: *mut c_void, kind: u32, v: &[u8]) {
    unsafe { cb(ctx, kind, v.as_ptr(), v.len()) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_roots_active(v: Bytes, cb: Emit, c: *mut c_void) {
    send(cb, c, 0, &active(unsafe { v.get() }));
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_roots_parent(v: Bytes, cb: Emit, c: *mut c_void) {
    send(cb, c, 0, info(unsafe { v.get() }, b"ParentMod"));
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_roots_defaults(
    v: Bytes,
    speed: *mut i32,
    cb: Emit,
    c: *mut c_void,
) {
    let v = unsafe { v.get() };
    send(cb, c, 0, info(v, b"SettingsDirectory"));
    let speed_text = info(v, b"DefaultGameSpeed");
    if speed_text.is_empty() {
        return;
    }
    let mut text = speed_text.to_vec();
    text.push(0);
    let mut end = core::ptr::null_mut();
    unsafe {
        *errno() = 0;
        let value = strtol(text.as_ptr().cast(), &mut end, 10);
        if *errno() == 0
            && !end.is_null()
            && *end == 0
            && (1..=6).contains(&value)
            && !speed.is_null()
        {
            speed.write(value as i32);
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_roots_settings(
    n: Bytes,
    v: Bytes,
    f: Bytes,
    s: Bytes,
    cb: Emit,
    c: *mut c_void,
) {
    send(
        cb,
        c,
        0,
        &settings(
            unsafe { n.get() },
            unsafe { v.get() },
            unsafe { f.get() },
            unsafe { s.get() },
        ),
    );
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_roots_safe(v: Bytes) -> u8 {
    u8::from(safe(unsafe { v.get() }))
}
fn join(a: &[u8], b: &[u8]) -> Vec<u8> {
    if a.is_empty() {
        return b.to_vec();
    }
    if b.is_empty() {
        return a.to_vec();
    }
    let mut out = a.to_vec();
    if !matches!(a.last(), Some(b'\\' | b'/')) {
        out.push(b'\\');
    }
    out.extend_from_slice(b);
    out
}
unsafe extern "C" fn collect(c: *mut c_void, _kind: u32, p: *const u8, n: usize) {
    let out = unsafe { &mut *c.cast::<Vec<u8>>() };
    out.extend_from_slice(if n == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(p, n) }
    });
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_roots_discover(
    data: Bytes,
    cmd: Bytes,
    is_dir: Directory,
    read: Read,
    io: *mut c_void,
    cb: Emit,
    c: *mut c_void,
) {
    let data = unsafe { data.get() };
    send(cb, c, 0, data);
    let mut current = active(unsafe { cmd.get() });
    send(cb, c, 2, &current);
    if current.is_empty() {
        send(
            cb,
            c,
            1,
            b"Extension roots: no /mod argument; using Data only",
        );
        return;
    }
    if !safe(&current) {
        send(cb, c, 1, b"Extension roots: rejected unsafe mod name");
        send(cb, c, 2, &[]);
        return;
    }
    let mut chain = Vec::new();
    let mut seen = BTreeSet::new();
    for _ in 0..32 {
        if !seen.insert(lower(&current)) {
            let mut m = b"Extension roots: ParentMod cycle stopped at ".to_vec();
            m.extend_from_slice(&current);
            send(cb, c, 1, &m);
            break;
        }
        let root = join(&join(data, b"Mods"), &current);
        if unsafe { is_dir(io, root.as_ptr(), root.len()) } == 0 {
            let mut m = b"Extension roots: mod directory not found: ".to_vec();
            m.extend_from_slice(&root);
            send(cb, c, 1, &m);
            break;
        }
        chain.push(root.clone());
        let mut text = Vec::new();
        let path = join(&root, b"info.ini");
        unsafe {
            read(
                io,
                path.as_ptr(),
                path.len(),
                collect,
                (&mut text as *mut Vec<u8>).cast(),
            )
        };
        let parent = info(&text, b"ParentMod");
        if parent.is_empty() {
            break;
        }
        if !safe(parent) {
            let mut m = b"Extension roots: rejected unsafe ParentMod for ".to_vec();
            m.extend_from_slice(&current);
            send(cb, c, 1, &m);
            break;
        }
        current = parent.to_vec();
    }
    if chain.len() == 32 {
        send(cb, c, 1, b"Extension roots: ParentMod depth limit reached");
    }
    for root in chain.iter().rev() {
        send(cb, c, 0, root);
    }
}
