//! Opaque callback records; leases preserve bytes after registry mutation.
extern crate alloc;
use alloc::alloc::{Layout, alloc, dealloc};
use alloc::vec::Vec;
use core::ffi::c_void;
use core::ptr::NonNull;
use core::sync::atomic::{AtomicU8, AtomicUsize, Ordering};

struct Registration {
    refs: AtomicUsize,
    owner: Vec<u8>,
    fields: Vec<Vec<u8>>,
    handler: AtomicUsize,
    user_data: usize,
    enabled: AtomicU8,
}
type Store = Vec<NonNull<Registration>>;

#[repr(C)]
pub struct HandlerView {
    lease: *mut c_void,
    owner: *const u8,
    owner_size: usize,
    field_count: usize,
    handler: usize,
    user_data: usize,
    enabled: u8,
}
#[repr(C)]
pub struct FieldView {
    bytes: *const u8,
    size: usize,
}

fn header() -> *mut Store {
    let pointer = unsafe { alloc(Layout::new::<Store>()) as *mut Store };
    if !pointer.is_null() {
        unsafe { pointer.write(Vec::new()) };
    }
    pointer
}
fn copy_bytes(pointer: *const u8, size: usize) -> Option<Vec<u8>> {
    if pointer.is_null() && size != 0 {
        return None;
    }
    let mut value = Vec::new();
    value.try_reserve_exact(size.checked_add(1)?).ok()?;
    if size != 0 {
        value.extend_from_slice(unsafe { core::slice::from_raw_parts(pointer, size) });
    }
    value.push(0);
    Some(value)
}
fn new_record(
    owner: Vec<u8>,
    fields: Vec<Vec<u8>>,
    handler: usize,
    user_data: usize,
    enabled: u8,
) -> Option<NonNull<Registration>> {
    let pointer =
        NonNull::new(unsafe { alloc(Layout::new::<Registration>()) as *mut Registration })?;
    unsafe {
        pointer.as_ptr().write(Registration {
            refs: AtomicUsize::new(1),
            owner,
            fields,
            handler: AtomicUsize::new(handler),
            user_data,
            enabled: AtomicU8::new(enabled),
        });
    }
    Some(pointer)
}
fn copy_record(source: &Registration) -> Option<NonNull<Registration>> {
    let mut fields = Vec::new();
    fields.try_reserve_exact(source.fields.len()).ok()?;
    for field in &source.fields {
        fields.push(copy_bytes(field.as_ptr(), field.len() - 1)?);
    }
    new_record(
        copy_bytes(source.owner.as_ptr(), source.owner.len() - 1)?,
        fields,
        source.handler.load(Ordering::Relaxed),
        source.user_data,
        source.enabled.load(Ordering::Relaxed),
    )
}
fn release(pointer: NonNull<Registration>) {
    if unsafe { pointer.as_ref() }
        .refs
        .fetch_sub(1, Ordering::AcqRel)
        == 1
    {
        unsafe {
            pointer.as_ptr().drop_in_place();
            dealloc(pointer.as_ptr().cast(), Layout::new::<Registration>());
        }
    }
}
fn retain(pointer: NonNull<Registration>) -> bool {
    unsafe { pointer.as_ref() }
        .refs
        .fetch_update(Ordering::AcqRel, Ordering::Acquire, |count| {
            count.checked_add(1)
        })
        .is_ok()
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_handlers_new() -> *mut c_void {
    header().cast()
}
/// # Safety
/// `source` must be null or a live registry without concurrent mutation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_clone(source: *const c_void) -> *mut c_void {
    let target = header();
    if target.is_null() {
        return core::ptr::null_mut();
    }
    if !source.is_null() {
        let input = unsafe { &*source.cast::<Store>() };
        let output = unsafe { &mut *target };
        if output.try_reserve_exact(input.len()).is_err() {
            unsafe { a2fo_rs_runtime_handlers_drop(target.cast()) };
            return core::ptr::null_mut();
        }
        for pointer in input {
            match copy_record(unsafe { pointer.as_ref() }) {
                Some(copy) => output.push(copy),
                None => {
                    unsafe { a2fo_rs_runtime_handlers_drop(target.cast()) };
                    return core::ptr::null_mut();
                }
            }
        }
    }
    target.cast()
}
/// # Safety
/// `store` must be null or a live registry allocated here and dropped once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_drop(store: *mut c_void) {
    if store.is_null() {
        return;
    }
    let pointer = store.cast::<Store>();
    for record in (unsafe { &mut *pointer }).drain(..) {
        release(record);
    }
    unsafe {
        pointer.drop_in_place();
        dealloc(pointer.cast(), Layout::new::<Store>());
    }
}
/// # Safety
/// `store` must be null or a live registry without concurrent mutation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_len(store: *const c_void) -> usize {
    if store.is_null() {
        0
    } else {
        unsafe { (&*store.cast::<Store>()).len() }
    }
}
/// # Safety
/// `store` must be a live registry with exclusive access. Input spans must be valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_push(
    store: *mut c_void,
    owner: *const u8,
    owner_size: usize,
    fields: *const FieldView,
    field_count: usize,
    handler: usize,
    user_data: usize,
    enabled: u8,
) -> u8 {
    if store.is_null() || (fields.is_null() && field_count != 0) {
        return 0;
    }
    let owner = match copy_bytes(owner, owner_size) {
        Some(v) => v,
        None => return 0,
    };
    let mut copied = Vec::new();
    if copied.try_reserve_exact(field_count).is_err() {
        return 0;
    }
    if field_count != 0 {
        for field in unsafe { core::slice::from_raw_parts(fields, field_count) } {
            match copy_bytes(field.bytes, field.size) {
                Some(v) => copied.push(v),
                None => return 0,
            }
        }
    }
    let target = unsafe { &mut *store.cast::<Store>() };
    if target.try_reserve(1).is_err() {
        return 0;
    }
    let Some(record) = new_record(owner, copied, handler, user_data, enabled) else {
        return 0;
    };
    target.push(record);
    1
}
/// # Safety
/// `store` must be a live registry and `out` writable. The returned lease
/// must be released once; its byte views remain valid until then.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_get(
    store: *const c_void,
    index: usize,
    out: *mut HandlerView,
) -> u8 {
    if store.is_null() || out.is_null() {
        return 0;
    }
    let Some(pointer) = (unsafe { &*store.cast::<Store>() }).get(index) else {
        return 0;
    };
    if !retain(*pointer) {
        return 0;
    }
    let entry = unsafe { pointer.as_ref() };
    unsafe {
        out.write(HandlerView {
            lease: pointer.as_ptr().cast(),
            owner: entry.owner.as_ptr(),
            owner_size: entry.owner.len() - 1,
            field_count: entry.fields.len(),
            handler: entry.handler.load(Ordering::Relaxed),
            user_data: entry.user_data,
            enabled: entry.enabled.load(Ordering::Relaxed),
        });
    }
    1
}
/// # Safety
/// `lease` must be live and `out` writable. Its bytes live until lease release.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_get_field(
    lease: *const c_void,
    field_index: usize,
    out: *mut FieldView,
) -> u8 {
    if lease.is_null() || out.is_null() {
        return 0;
    }
    let entry = unsafe { &*lease.cast::<Registration>() };
    let Some(field) = entry.fields.get(field_index) else {
        return 0;
    };
    unsafe {
        out.write(FieldView {
            bytes: field.as_ptr(),
            size: field.len() - 1,
        });
    }
    1
}
/// # Safety
/// `lease` must be a live leased record.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_retain(lease: *mut c_void) -> u8 {
    let Some(pointer) = NonNull::new(lease.cast::<Registration>()) else {
        return 0;
    };
    retain(pointer) as u8
}
/// # Safety
/// `lease` must be null or a live leased record with a corresponding retain.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_release(lease: *mut c_void) {
    if let Some(pointer) = NonNull::new(lease.cast::<Registration>()) {
        release(pointer);
    }
}
/// # Safety
/// `store` must be a live registry with exclusive access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_set_enabled(
    store: *mut c_void,
    index: usize,
    enabled: u8,
) -> u8 {
    if store.is_null() {
        return 0;
    }
    let Some(pointer) = (unsafe { &mut *store.cast::<Store>() }).get(index) else {
        return 0;
    };
    unsafe { pointer.as_ref() }
        .enabled
        .store(enabled, Ordering::Relaxed);
    1
}
/// # Safety
/// `store` must be a live registry with exclusive access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_set_handler(
    store: *mut c_void,
    index: usize,
    handler: usize,
) -> u8 {
    if store.is_null() {
        return 0;
    }
    let Some(pointer) = (unsafe { &mut *store.cast::<Store>() }).get(index) else {
        return 0;
    };
    unsafe { pointer.as_ref() }
        .handler
        .store(handler, Ordering::Relaxed);
    1
}
/// # Safety
/// `store` must be null or a live registry with exclusive access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_handlers_clear(store: *mut c_void) {
    if !store.is_null() {
        for record in (unsafe { &mut *store.cast::<Store>() }).drain(..) {
            release(record);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn leases_clone_bytes_and_mutation() {
        unsafe {
            let store = a2fo_rs_runtime_handlers_new();
            let owner = [b'a', 0, 0xff];
            let field = [0xff, 0, b'z'];
            let fields = [
                FieldView {
                    bytes: field.as_ptr(),
                    size: 3,
                },
                FieldView {
                    bytes: field.as_ptr(),
                    size: 3,
                },
            ];
            assert_eq!(
                a2fo_rs_runtime_handlers_push(
                    store,
                    owner.as_ptr(),
                    3,
                    fields.as_ptr(),
                    2,
                    usize::MAX,
                    0x1234,
                    1
                ),
                1
            );
            let clone = a2fo_rs_runtime_handlers_clone(store);
            assert!(!clone.is_null());
            let mut view = HandlerView {
                lease: core::ptr::null_mut(),
                owner: core::ptr::null(),
                owner_size: 0,
                field_count: 0,
                handler: 0,
                user_data: 0,
                enabled: 0,
            };
            assert_eq!(a2fo_rs_runtime_handlers_get(store, 0, &mut view), 1);
            assert_eq!(
                core::slice::from_raw_parts(view.owner, view.owner_size),
                owner
            );
            assert_eq!(
                (view.handler, view.user_data, view.field_count),
                (usize::MAX, 0x1234, 2)
            );
            let mut f = FieldView {
                bytes: core::ptr::null(),
                size: 0,
            };
            for i in 0..2 {
                assert_eq!(a2fo_rs_runtime_handlers_get_field(view.lease, i, &mut f), 1);
                assert_eq!(core::slice::from_raw_parts(f.bytes, f.size), field);
            }
            assert_eq!(a2fo_rs_runtime_handlers_retain(view.lease), 1);
            assert_eq!(a2fo_rs_runtime_handlers_set_handler(store, 0, 0), 1);
            assert_eq!(a2fo_rs_runtime_handlers_set_enabled(store, 0, 0), 1);
            a2fo_rs_runtime_handlers_clear(store);
            a2fo_rs_runtime_handlers_drop(store);
            assert_eq!(
                core::slice::from_raw_parts(view.owner, view.owner_size),
                owner
            );
            a2fo_rs_runtime_handlers_release(view.lease);
            a2fo_rs_runtime_handlers_release(view.lease);
            let mut cloned = HandlerView {
                lease: core::ptr::null_mut(),
                owner: core::ptr::null(),
                owner_size: 0,
                field_count: 0,
                handler: 0,
                user_data: 0,
                enabled: 0,
            };
            assert_eq!(a2fo_rs_runtime_handlers_get(clone, 0, &mut cloned), 1);
            assert_eq!((cloned.handler, cloned.enabled), (usize::MAX, 1));
            a2fo_rs_runtime_handlers_release(cloned.lease);
            a2fo_rs_runtime_handlers_drop(clone);
        }
    }
    #[test]
    fn invalid_push_rolls_back() {
        unsafe {
            let store = a2fo_rs_runtime_handlers_new();
            assert_eq!(
                a2fo_rs_runtime_handlers_push(
                    store,
                    core::ptr::null(),
                    1,
                    core::ptr::null(),
                    0,
                    0,
                    0,
                    1
                ),
                0
            );
            assert_eq!(
                a2fo_rs_runtime_handlers_push(
                    store,
                    core::ptr::null(),
                    0,
                    core::ptr::null(),
                    1,
                    0,
                    0,
                    1
                ),
                0
            );
            let bad = FieldView {
                bytes: core::ptr::null(),
                size: 1,
            };
            assert_eq!(
                a2fo_rs_runtime_handlers_push(store, core::ptr::null(), 0, &bad, 1, 0, 0, 1),
                0
            );
            assert_eq!(a2fo_rs_runtime_handlers_len(store), 0);
            a2fo_rs_runtime_handlers_drop(store);
        }
    }
}
