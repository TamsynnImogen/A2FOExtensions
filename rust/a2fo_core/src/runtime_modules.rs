//! Rust-owned loaded-module records. The opaque allocation owns storage only;
//! shutdown and library release remain explicit operations in loader policy.
extern crate alloc;
use alloc::alloc::{Layout, alloc, dealloc};
use alloc::vec::Vec;
use core::ffi::c_void;

struct Module {
    handle: usize,
    shutdown: usize,
    path: Vec<u8>, // includes a trailing NUL for the native path view
}

#[repr(C)]
pub struct View {
    handle: usize,
    shutdown: usize,
    path: *const u8,
    path_size: usize,
}

fn new_header() -> *mut Vec<Module> {
    let layout = Layout::new::<Vec<Module>>();
    let ptr = unsafe { alloc(layout) as *mut Vec<Module> };
    if !ptr.is_null() {
        unsafe { ptr.write(Vec::new()) };
    }
    ptr
}

fn copy_path(path: *const u8, size: usize) -> Option<Vec<u8>> {
    if path.is_null() && size != 0 {
        return None;
    }
    let total = size.checked_add(1)?;
    let mut result = Vec::new();
    result.try_reserve_exact(total).ok()?;
    if size != 0 {
        let bytes = unsafe { core::slice::from_raw_parts(path, size) };
        result.extend_from_slice(bytes);
    }
    result.push(0);
    Some(result)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_modules_new() -> *mut c_void {
    new_header().cast()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_modules_clone(source: *const c_void) -> *mut c_void {
    let target = new_header();
    if target.is_null() {
        return core::ptr::null_mut();
    }
    if !source.is_null() {
        let input = unsafe { &*source.cast::<Vec<Module>>() };
        let output = unsafe { &mut *target };
        if output.try_reserve_exact(input.len()).is_err() {
            unsafe { a2fo_rs_runtime_modules_drop(target.cast()) };
            return core::ptr::null_mut();
        }
        for item in input {
            let path = match copy_path(item.path.as_ptr(), item.path.len() - 1) {
                Some(path) => path,
                None => {
                    unsafe { a2fo_rs_runtime_modules_drop(target.cast()) };
                    return core::ptr::null_mut();
                }
            };
            output.push(Module {
                handle: item.handle,
                shutdown: item.shutdown,
                path,
            });
        }
    }
    target.cast()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_modules_drop(modules: *mut c_void) {
    if modules.is_null() {
        return;
    }
    let ptr = modules.cast::<Vec<Module>>();
    unsafe { ptr.drop_in_place() };
    unsafe { dealloc(ptr.cast(), Layout::new::<Vec<Module>>()) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_modules_len(modules: *const c_void) -> usize {
    if modules.is_null() {
        0
    } else {
        unsafe { (&*modules.cast::<Vec<Module>>()).len() }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_modules_push(
    modules: *mut c_void,
    handle: usize,
    shutdown: usize,
    path: *const u8,
    size: usize,
) -> u8 {
    if modules.is_null() {
        return 0;
    }
    let path = match copy_path(path, size) {
        Some(path) => path,
        None => return 0,
    };
    let list = unsafe { &mut *modules.cast::<Vec<Module>>() };
    if list.try_reserve(1).is_err() {
        return 0;
    }
    list.push(Module {
        handle,
        shutdown,
        path,
    });
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_modules_clear(modules: *mut c_void) {
    if !modules.is_null() {
        unsafe { (&mut *modules.cast::<Vec<Module>>()).clear() };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_modules_get(
    modules: *const c_void,
    index: usize,
    out: *mut View,
) -> u8 {
    if modules.is_null() || out.is_null() {
        return 0;
    }
    let item = match (unsafe { &*modules.cast::<Vec<Module>>() }).get(index) {
        Some(item) => item,
        None => return 0,
    };
    unsafe {
        out.write(View {
            handle: item.handle,
            shutdown: item.shutdown,
            path: item.path.as_ptr(),
            path_size: item.path.len() - 1,
        })
    };
    1
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn byte_paths_and_full_width_values_survive_clone_and_clear() {
        unsafe {
            let list = a2fo_rs_runtime_modules_new();
            assert!(!list.is_null());
            let path = b"Data\\modules\\\xff.dll";
            let handle = usize::MAX;
            let shutdown = usize::MAX - 1;
            assert_eq!(
                a2fo_rs_runtime_modules_push(list, handle, shutdown, path.as_ptr(), path.len()),
                1
            );
            let copy = a2fo_rs_runtime_modules_clone(list);
            assert!(!copy.is_null());
            a2fo_rs_runtime_modules_clear(list);
            assert_eq!(a2fo_rs_runtime_modules_len(list), 0);
            let mut view = View {
                handle: 0,
                shutdown: 0,
                path: core::ptr::null(),
                path_size: 0,
            };
            assert_eq!(a2fo_rs_runtime_modules_get(copy, 0, &mut view), 1);
            assert_eq!((view.handle, view.shutdown), (handle, shutdown));
            assert_eq!(core::slice::from_raw_parts(view.path, view.path_size), path);
            assert_eq!(*view.path.add(view.path_size), 0);
            a2fo_rs_runtime_modules_drop(list);
            a2fo_rs_runtime_modules_drop(copy);
        }
    }

    #[test]
    fn growth_reverse_query_and_invalid_arguments() {
        unsafe {
            let list = a2fo_rs_runtime_modules_new();
            for index in 0..256 {
                let path = format!("module-{index}");
                assert_eq!(
                    a2fo_rs_runtime_modules_push(
                        list,
                        index,
                        255 - index,
                        path.as_ptr(),
                        path.len()
                    ),
                    1
                );
            }
            assert_eq!(a2fo_rs_runtime_modules_len(list), 256);
            for index in (0..256).rev() {
                let mut view = View {
                    handle: 0,
                    shutdown: 0,
                    path: core::ptr::null(),
                    path_size: 0,
                };
                assert_eq!(a2fo_rs_runtime_modules_get(list, index, &mut view), 1);
                assert_eq!((view.handle, view.shutdown), (index, 255 - index));
            }
            assert_eq!(
                a2fo_rs_runtime_modules_push(list, 1, 2, core::ptr::null(), 1),
                0
            );
            assert_eq!(
                a2fo_rs_runtime_modules_push(list, 1, 2, b"x".as_ptr(), usize::MAX),
                0
            );
            assert_eq!(
                a2fo_rs_runtime_modules_get(list, 256, core::ptr::null_mut()),
                0
            );
            assert_eq!(a2fo_rs_runtime_modules_len(list), 256);
            a2fo_rs_runtime_modules_drop(list);
        }
    }
}
