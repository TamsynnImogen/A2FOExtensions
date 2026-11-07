//! Remaining module runtime orchestration. Native adapters own SDK, Win32 and
//! C++ storage; borrowed POD and callback results carry decisions into Rust.
use core::ffi::c_void;
type MailerStep = unsafe extern "C" fn(*mut c_void, u32) -> u8;
type MailerLog = unsafe extern "C" fn(*mut c_void, u32);
/// Safety: callbacks and their native context remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_mailer_init(
    valid_api: u8,
    already_started: u8,
    step: Option<MailerStep>,
    log: Option<MailerLog>,
    context: *mut c_void,
) -> u8 {
    if valid_api == 0 {
        return 0;
    }
    if already_started != 0 {
        return 1;
    }
    let (Some(step), Some(log)) = (step, log) else {
        return 0;
    };
    if unsafe { step(context, 0) } == 0 {
        return 0;
    } // root and config
    if unsafe { step(context, 1) } == 0 {
        unsafe { log(context, 0) };
        return 1;
    }
    if unsafe { step(context, 2) } == 0 {
        unsafe { log(context, 1) };
        return 1;
    }
    if unsafe { step(context, 3) } == 0 {
        return 0;
    } // duplicate parent
    if unsafe { step(context, 4) } == 0 {
        unsafe { step(context, 8) };
        return 0;
    } // event
    let initialized = unsafe { step(context, 5) } != 0;
    let success =
        initialized && unsafe { step(context, 6) } != 0 && unsafe { step(context, 7) } != 0;
    unsafe { step(context, 9) }; // attribute list, parent and failure event cleanup
    if !success {
        unsafe { log(context, 2) };
        return 0;
    }
    unsafe {
        step(context, 10);
        log(context, if step(context, 11) != 0 { 3 } else { 4 })
    };
    1
}
/// Safety: callback and context remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_mailer_shutdown(
    step: Option<MailerStep>,
    context: *mut c_void,
) {
    if let Some(step) = step {
        unsafe { step(context, 12) };
    }
}

use alloc::vec::Vec;
use core::{ffi::c_int, slice};
unsafe extern "C" {
    fn tolower(ch: c_int) -> c_int;
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}
impl Bytes {
    unsafe fn get<'a>(self) -> Option<&'a [u8]> {
        if self.size == 0 {
            Some(&[])
        } else if self.data.is_null() || self.size > isize::MAX as usize {
            None
        } else {
            Some(unsafe { slice::from_raw_parts(self.data, self.size) })
        }
    }
}
fn span(bytes: &[u8]) -> Bytes {
    Bytes {
        data: bytes.as_ptr(),
        size: bytes.len(),
    }
}
fn lower(bytes: &[u8]) -> Vec<u8> {
    bytes
        .iter()
        .map(|&ch| unsafe { tolower(ch as c_int) as u8 })
        .collect()
}
fn join(left: &[u8], right: &[u8]) -> Vec<u8> {
    if left.is_empty() {
        return right.to_vec();
    }
    if right.is_empty() {
        return left.to_vec();
    }
    let mut out = left.to_vec();
    if !matches!(left.last(), Some(b'\\' | b'/')) {
        out.push(b'\\');
    }
    out.extend_from_slice(right);
    out
}
fn odf(name: &[u8]) -> bool {
    let low = lower(name);
    low.len() >= 4 && low.ends_with(b".odf")
}
struct Child {
    name: Vec<u8>,
    directory: bool,
    reparse: bool,
}
type ChildEmit = unsafe extern "C" fn(*mut c_void, Bytes, u8, u8);
type TextEmit = unsafe extern "C" fn(*mut c_void, Bytes);
type RootCount = unsafe extern "C" fn(*mut c_void) -> u32;
type Root = unsafe extern "C" fn(*mut c_void, u32, *mut Bytes) -> u8;
type List = unsafe extern "C" fn(*mut c_void, Bytes, ChildEmit, *mut c_void);
type Store = unsafe extern "C" fn(*mut c_void, Bytes, Bytes);
type Find = unsafe extern "C" fn(*mut c_void, Bytes, *mut Bytes) -> u8;
type Read = unsafe extern "C" fn(*mut c_void, Bytes, TextEmit, *mut c_void) -> u8;
type Parse = unsafe extern "C" fn(*mut c_void, Bytes) -> u8;
type Register = unsafe extern "C" fn(*mut c_void, usize, Bytes) -> u8;
type Step = unsafe extern "C" fn(*mut c_void, u32) -> u32;
type HybridLog = unsafe extern "C" fn(*mut c_void, u32, Bytes, u32);
unsafe extern "C" fn collect_child(p: *mut c_void, name: Bytes, directory: u8, reparse: u8) {
    let children = unsafe { &mut *(p.cast::<Vec<Child>>()) };
    if let Some(name) = unsafe { name.get() } {
        children.push(Child {
            name: name.to_vec(),
            directory: directory != 0,
            reparse: reparse != 0,
        });
    }
}
unsafe extern "C" fn collect_text(p: *mut c_void, bytes: Bytes) {
    let out = unsafe { &mut *(p.cast::<Vec<u8>>()) };
    if let Some(bytes) = unsafe { bytes.get() } {
        out.extend_from_slice(bytes);
    }
}
unsafe fn index_directory(
    directory: &[u8],
    depth: usize,
    list: List,
    store: Store,
    ctx: *mut c_void,
) {
    if depth > 64 {
        return;
    }
    let mut children: Vec<Child> = Vec::new();
    unsafe {
        list(
            ctx,
            span(directory),
            collect_child,
            (&mut children as *mut Vec<Child>).cast(),
        )
    };
    children.retain(|c| {
        c.name != b"."
            && c.name != b".."
            && !(c.directory && c.reparse)
            && (c.directory || odf(&c.name))
    });
    children.sort_unstable_by_key(|a| lower(&a.name));
    for child in children {
        let path = join(directory, &child.name);
        if child.directory {
            unsafe { index_directory(&path, depth + 1, list, store, ctx) }
        } else {
            let key = lower(&child.name);
            unsafe { store(ctx, span(&key), span(&path)) };
        }
    }
}
unsafe fn index(count: RootCount, root: Root, list: List, store: Store, ctx: *mut c_void) {
    let roots = unsafe { count(ctx) };
    for i in 0..roots {
        let mut value = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if unsafe { root(ctx, i, &mut value) } == 0 {
            continue;
        }
        if let Some(root) = unsafe { value.get() }
            && !root.is_empty()
        {
            let path = join(root, b"odf");
            unsafe { index_directory(&path, 0, list, store, ctx) }
        }
    }
}
/// Safety: callbacks and borrowed path spans remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_hybrid_index(
    count: Option<RootCount>,
    root: Option<Root>,
    list: Option<List>,
    store: Option<Store>,
    ctx: *mut c_void,
) {
    if let (Some(count), Some(root), Some(list), Some(store)) = (count, root, list, store) {
        unsafe { index(count, root, list, store, ctx) };
    }
}
fn source_name(raw: &[u8]) -> Vec<u8> {
    if raw.is_empty() || raw.len() > 1024 {
        return Vec::new();
    }
    let mut name = raw.to_vec();
    for ch in &mut name {
        if *ch == b'/' {
            *ch = b'\\'
        }
    }
    if let Some(slash) = name.iter().rposition(|&ch| ch == b'\\') {
        name.drain(..=slash);
    }
    let mut name = lower(&name);
    if !odf(&name) {
        name.extend_from_slice(b".odf")
    }
    name
}
/// Safety: callbacks and source span remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_hybrid_handler(
    valid: u8,
    object_class: usize,
    source: Bytes,
    find: Option<Find>,
    read: Option<Read>,
    parse: Option<Parse>,
    register: Option<Register>,
    log: Option<HybridLog>,
    ctx: *mut c_void,
) {
    if valid == 0 || object_class == 0 {
        return;
    }
    let (Some(find), Some(read), Some(parse), Some(register), Some(log)) =
        (find, read, parse, register, log)
    else {
        return;
    };
    let Some(source) = (unsafe { source.get() }) else {
        return;
    };
    let name = source_name(source);
    if name.is_empty() {
        return;
    }
    let mut path = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    if unsafe { find(ctx, span(&name), &mut path) } == 0 {
        return;
    }
    let mut contents = Vec::new();
    if unsafe {
        read(
            ctx,
            path,
            collect_text,
            (&mut contents as *mut Vec<u8>).cast(),
        )
    } == 0
    {
        return;
    }
    if unsafe { parse(ctx, span(&contents)) } == 0 {
        return;
    }
    if unsafe { register(ctx, object_class, span(&name)) } == 0 {
        unsafe { log(ctx, 7, span(&name), 0) };
    }
}
/// Safety: callbacks and context remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_hybrid_init(
    valid: u8,
    step: Option<Step>,
    count: Option<RootCount>,
    root: Option<Root>,
    list: Option<List>,
    store: Option<Store>,
    log: Option<HybridLog>,
    ctx: *mut c_void,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    let (Some(step), Some(log)) = (step, log) else {
        return 0;
    };
    if unsafe { step(ctx, 0) } == 0 {
        return 0;
    }
    let alias = unsafe { step(ctx, 1) } != 0;
    let cocoon = unsafe { step(ctx, 2) } != 0;
    if !alias || !cocoon {
        unsafe { log(ctx, 0, span(&[]), 0) };
        return 0;
    }
    for (operation, code) in [(3, 1), (4, 2), (5, 3), (6, 4)] {
        if unsafe { step(ctx, operation) } == 0 {
            unsafe { log(ctx, code, span(&[]), 0) };
            return 0;
        }
    }
    unsafe { log(ctx, 5, span(&[]), 0) };
    unsafe { step(ctx, 7) }; // clear native index before traversal
    if let (Some(count), Some(root), Some(list), Some(store)) = (count, root, list, store) {
        unsafe { index(count, root, list, store, ctx) };
    }
    if unsafe { step(ctx, 8) } != 0 {
        unsafe { log(ctx, 6, span(&[]), 0) }
    };
    let runtime = unsafe { step(ctx, 9) } != 0;
    let parser = if runtime {
        if unsafe { step(ctx, 10) } == 0 {
            unsafe { log(ctx, 8, span(&[]), 0) };
            false
        } else {
            (unsafe { step(ctx, 11) }) != 0
        }
    } else {
        false
    };
    let size = unsafe { step(ctx, 12) };
    unsafe {
        log(
            ctx,
            if runtime {
                if parser { 9 } else { 10 }
            } else {
                11
            },
            span(&[]),
            size,
        )
    };
    1
}
