use alloc::vec::Vec;
use core::ffi::c_void;

const METHOD_NAMES: [&[u8]; 5] = [
    b"legacy\0",
    b"construct\0",
    b"yard\0",
    b"research\0",
    b"evolve\0",
];
const COMMAND_PREFIXES: [&[u8]; 5] = [
    b"buildItem\0",
    b"constructItem\0",
    b"yardItem\0",
    b"researchItem\0",
    b"evolveItem\0",
];

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_hybrid_method_name(method: u8) -> *const u8 {
    METHOD_NAMES
        .get(method as usize)
        .copied()
        .unwrap_or(b"unknown\0")
        .as_ptr()
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_hybrid_command_prefix(method: u8) -> *const u8 {
    COMMAND_PREFIXES
        .get(method as usize)
        .copied()
        .unwrap_or(b"\0")
        .as_ptr()
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Job {
    queue_id: u32,
    project_id: u32,
    method: u8,
    active: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Entry {
    method: u8,
    index: usize,
    project_id: u32,
}
#[repr(C)]
pub struct Plan {
    result: u8,
    index: usize,
    clear: u8,
    activate: u8,
}
fn plan(result: u8, index: usize, clear: bool, activate: bool) -> Plan {
    Plan {
        result,
        index,
        clear: u8::from(clear),
        activate: u8::from(activate),
    }
}
fn queue(op: u8, jobs: &[Job], capacity: usize, candidate: Job, id: u32) -> Plan {
    let missing = jobs.len();
    let barrier = jobs.iter().any(|j| j.method == 4);
    match op {
        0 => plan(
            if candidate.project_id == 0 {
                1
            } else if candidate.queue_id == 0 {
                2
            } else if jobs.iter().any(|j| j.queue_id == candidate.queue_id) {
                3
            } else if barrier {
                5
            } else if jobs.len() >= capacity {
                4
            } else {
                0
            },
            missing,
            false,
            false,
        ),
        1 => plan(
            u8::from(!jobs.is_empty()),
            0,
            false,
            jobs.first().is_some_and(|j| j.active == 0),
        ),
        2 => plan(
            u8::from(jobs.first().is_some_and(|j| j.active != 0)),
            0,
            jobs.first().is_some_and(|j| j.active != 0 && j.method == 4),
            false,
        ),
        3 | 5 => {
            let index = jobs
                .iter()
                .position(|j| j.queue_id == id)
                .unwrap_or(missing);
            plan(u8::from(index != missing), index, false, false)
        }
        4 => plan(
            u8::from(jobs.first().is_some_and(|j| j.active != 0)),
            0,
            false,
            false,
        ),
        6 => plan(u8::from(barrier), 0, false, false),
        7 => plan(u8::from(!barrier && jobs.len() < capacity), 0, false, false),
        _ => plan(0, missing, false, false),
    }
}
fn list(op: u8, entries: &[Entry], method: u8, index: usize, project: u32) -> Plan {
    let slot = entries
        .iter()
        .position(|e| e.method == method && e.index == index);
    let target = entries.iter().position(|e| e.project_id == project);
    match op {
        0 => plan(
            if !(1..=4).contains(&method) {
                1
            } else if index >= 57 {
                2
            } else if project == 0 {
                3
            } else if slot.is_some() {
                4
            } else if target.is_some() {
                5
            } else {
                0
            },
            entries.len(),
            false,
            false,
        ),
        1 => plan(
            u8::from(slot.is_some()),
            slot.unwrap_or(entries.len()),
            false,
            false,
        ),
        2 => plan(
            u8::from(target.is_some()),
            target.unwrap_or(entries.len()),
            false,
            false,
        ),
        3 => plan(
            target.map_or(method, |i| entries[i].method),
            target.unwrap_or(entries.len()),
            false,
            false,
        ),
        _ => plan(0, entries.len(), false, false),
    }
}
unsafe fn input<'a>(ptr: *const u8, len: usize) -> Option<&'a [u8]> {
    if len == 0 {
        Some(&[])
    } else if ptr.is_null() {
        None
    } else {
        // SAFETY: The C caller guarantees a readable, immutable span of len
        // bytes for this synchronous call. No borrowed slice escapes it.
        Some(unsafe { core::slice::from_raw_parts(ptr, len) })
    }
}
unsafe fn rows<'a, T>(ptr: *const T, len: usize) -> Option<&'a [T]> {
    if len == 0 {
        Some(&[])
    } else if ptr.is_null() {
        None
    } else {
        // SAFETY: The C caller guarantees len aligned, initialized POD rows
        // for this synchronous call. No borrowed slice escapes it.
        Some(unsafe { core::slice::from_raw_parts(ptr, len) })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hybrid_queue(
    op: u8,
    ptr: *const Job,
    len: usize,
    capacity: usize,
    candidate: Job,
    id: u32,
) -> Plan {
    let Some(jobs) = (unsafe { rows(ptr, len) }) else {
        return plan(0, 0, false, false);
    };
    queue(op, jobs, capacity, candidate, id)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hybrid_list(
    op: u8,
    ptr: *const Entry,
    len: usize,
    method: u8,
    index: usize,
    project: u32,
) -> Plan {
    let Some(entries) = (unsafe { rows(ptr, len) }) else {
        return plan(0, 0, false, false);
    };
    list(op, entries, method, index, project)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hybrid_parse_command(
    ptr: *const u8,
    len: usize,
    method: *mut u8,
    index: *mut usize,
) -> u8 {
    let Some(bytes) = (unsafe { input(ptr, len) }) else {
        return 0;
    };
    if method.is_null() || index.is_null() {
        return 0;
    }
    for kind in [1usize, 2, 3, 4, 0] {
        let nul_terminated = COMMAND_PREFIXES[kind];
        let prefix = &nul_terminated[..nul_terminated.len() - 1];
        if bytes.len() <= prefix.len() || !bytes[..prefix.len()].eq_ignore_ascii_case(prefix) {
            continue;
        }
        let mut value: usize = 0;
        for &b in &bytes[prefix.len()..] {
            if !b.is_ascii_digit() {
                return 0;
            }
            let Some(next) = value
                .checked_mul(10)
                .and_then(|v| v.checked_add((b - b'0') as usize))
            else {
                return 0;
            };
            value = next;
        }
        if value >= 57 {
            return 0;
        }
        // SAFETY: The C caller provides writable method and index scalars.
        unsafe {
            *method = kind as u8;
            *index = value;
        }
        return 1;
    }
    0
}
fn space(b: u8) -> bool {
    matches!(b, b' ' | b'\t' | b'\r' | b'\n' | 0x0b | 0x0c)
}
fn trim(mut s: &[u8]) -> &[u8] {
    while s.first().is_some_and(|&b| space(b)) {
        s = &s[1..]
    }
    while s.last().is_some_and(|&b| space(b)) {
        s = &s[..s.len() - 1]
    }
    s
}
fn normalize(raw: &[u8]) -> Vec<u8> {
    let s = trim(raw);
    let s = s
        .iter()
        .rposition(|&b| b == b'/' || b == b'\\')
        .map_or(s, |i| &s[i + 1..]);
    let mut out: Vec<u8> = s.iter().map(u8::to_ascii_lowercase).collect();
    if out.len() > 4 && out.ends_with(b".odf") {
        out.truncate(out.len() - 4)
    }
    out
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hybrid_normalize(
    ptr: *const u8,
    len: usize,
    output: *mut u8,
    capacity: usize,
) -> usize {
    let Some(raw) = (unsafe { input(ptr, len) }) else {
        return 0;
    };
    let out = normalize(raw);
    if out.len() > capacity || (output.is_null() && !out.is_empty()) {
        return 0;
    }
    if !out.is_empty() {
        unsafe { core::ptr::copy_nonoverlapping(out.as_ptr(), output, out.len()) }
    }
    out.len()
}
fn strip_comments(raw: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(raw.len());
    let (mut quoted, mut escaped, mut line, mut block) = (false, false, false, false);
    let mut i = 0;
    while i < raw.len() {
        let b = raw[i];
        let next = raw.get(i + 1).copied().unwrap_or(0);
        if line {
            if b == b'\n' || b == b'\r' {
                line = false;
                out.push(b)
            } else {
                out.push(b' ')
            }
        } else if block {
            if b == b'*' && next == b'/' {
                out.extend_from_slice(b"  ");
                i += 1;
                block = false
            } else {
                out.push(if b == b'\n' || b == b'\r' { b } else { b' ' })
            }
        } else if !quoted && b == b'/' && next == b'/' {
            out.extend_from_slice(b"  ");
            i += 1;
            line = true
        } else if !quoted && b == b'/' && next == b'*' {
            out.extend_from_slice(b"  ");
            i += 1;
            block = true
        } else {
            out.push(b);
            if quoted {
                if escaped {
                    escaped = false
                } else if b == b'\\' {
                    escaped = true
                } else if b == b'"' {
                    quoted = false
                }
            } else if b == b'"' {
                quoted = true
            }
        }
        i += 1;
    }
    out
}
fn index(s: &[u8]) -> Option<usize> {
    if s.is_empty() {
        return None;
    }
    let mut value = 0usize;
    for &b in s {
        if !b.is_ascii_digit() {
            return None;
        }
        value = value * 10 + (b - b'0') as usize;
        if value >= 57 {
            return None;
        }
    }
    Some(value)
}
fn command(s: &[u8]) -> Option<(usize, usize)> {
    if s.len() <= 14 || !s.starts_with(b"builditem") {
        return None;
    }
    let rest = &s[9..];
    let split = rest.windows(5).position(|w| w == b"refit")?;
    if split == 0 || split + 5 == rest.len() {
        return None;
    }
    Some((index(&rest[..split])?, index(&rest[split + 5..])?))
}
fn value(line: &[u8], equal: usize) -> Option<Vec<u8>> {
    let mut s = line.get(equal + 1..)?;
    while s.first().is_some_and(|&b| space(b)) {
        s = &s[1..]
    }
    if s.is_empty() {
        return None;
    }
    if s[0] != b'"' {
        s = trim(s);
        if s.last() == Some(&b';') {
            s = trim(&s[..s.len() - 1])
        }
        return (!s.is_empty()).then(|| s.to_vec());
    }
    let mut out = Vec::new();
    let mut escaped = false;
    for &b in &s[1..] {
        if escaped {
            out.push(b);
            escaped = false
        } else if b == b'\\' {
            escaped = true
        } else if b == b'"' {
            return Some(out);
        } else {
            out.push(b)
        }
    }
    None
}
type Visitor = unsafe extern "C" fn(*mut c_void, usize, usize, *const u8, usize) -> u8;
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_hybrid_parse_submenu(
    ptr: *const u8,
    len: usize,
    context: *mut c_void,
    visitor: Option<Visitor>,
) -> u8 {
    let Some(raw) = (unsafe { input(ptr, len) }) else {
        return 0;
    };
    let Some(visitor) = visitor else { return 0 };
    let source = strip_comments(raw);
    for line in source.split(|&b| b == b'\n' || b == b'\r') {
        let Some(equal) = line.iter().position(|&b| b == b'=') else {
            continue;
        };
        let mut end = equal;
        while end > 0 && space(line[end - 1]) {
            end -= 1
        }
        let mut begin = end;
        while begin > 0 && (line[begin - 1].is_ascii_alphanumeric() || line[begin - 1] == b'_') {
            begin -= 1
        }
        let ident: Vec<u8> = line[begin..end]
            .iter()
            .map(u8::to_ascii_lowercase)
            .collect();
        let Some((parent, child)) = command(&ident) else {
            continue;
        };
        let Some(value) = value(line, equal) else {
            continue;
        };
        let normalized = normalize(&value);
        if unsafe {
            visitor(
                context,
                parent,
                child,
                normalized.as_ptr(),
                normalized.len(),
            )
        } == 0
        {
            return 0;
        }
    }
    1
}
