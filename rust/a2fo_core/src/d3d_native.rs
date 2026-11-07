//! Win32 D3D11 resource operations and ownership. The C++ firewall only catches
//! foreign exceptions around the exact SDK method selected here; no exception
//! may unwind a Rust frame. Caller-owned STL image/error data stays borrowed.
use crate::d3d_resources::{Constants, Image, Reply, ResultState, TexturePlan};
use alloc::{
    alloc::{alloc, dealloc},
    vec::Vec,
};
use core::{
    alloc::Layout,
    ffi::c_void,
    mem::{align_of, offset_of, size_of},
    ptr,
};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct GuardApi {
    feature: unsafe extern "C" fn(usize, *mut c_void, *mut u32, *mut c_void) -> i32,
    check_format:
        unsafe extern "C" fn(usize, *mut c_void, u32, *mut u32, *mut i32, *mut c_void) -> i32,
    create_texture: unsafe extern "C" fn(
        usize,
        *mut c_void,
        *const c_void,
        *const c_void,
        *mut *mut c_void,
        *mut i32,
        *mut c_void,
    ) -> i32,
    create_view: unsafe extern "C" fn(
        usize,
        *mut c_void,
        *mut c_void,
        *const c_void,
        *mut *mut c_void,
        *mut i32,
        *mut c_void,
    ) -> i32,
    addref_view: unsafe extern "C" fn(usize, *mut c_void, *mut c_void) -> i32,
    release_view: unsafe extern "C" fn(usize, *mut c_void, *mut c_void) -> i32,
    release_texture: unsafe extern "C" fn(usize, *mut c_void, *mut c_void) -> i32,
}
#[repr(C)]
pub struct Host {
    context: *mut c_void,
    validate: unsafe extern "C" fn(*mut c_void) -> i32,
    read: unsafe extern "C" fn(*mut c_void, u32) -> usize,
    exception_sink: *mut c_void,
}
#[repr(C)]
struct SampleDesc {
    count: u32,
    quality: u32,
}
#[repr(C)]
struct TextureDesc {
    width: u32,
    height: u32,
    mip_levels: u32,
    array_size: u32,
    format: u32,
    sample: SampleDesc,
    usage: u32,
    bind: u32,
    cpu: u32,
    misc: u32,
}
#[repr(C)]
struct Subresource {
    bytes: *const u8,
    pitch: u32,
    slice: u32,
}
#[repr(C)]
struct ViewDesc {
    format: u32,
    dimension: u32,
    data: [u32; 4],
}
#[repr(C)]
struct TextureView {
    first_mip: u32,
    mip_levels: u32,
}

// Checked against the actual compiler SDK before any native dispatch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_native_layout(out: *mut usize, count: usize) -> usize {
    let values = [
        size_of::<TextureDesc>(),
        align_of::<TextureDesc>(),
        offset_of!(TextureDesc, width),
        offset_of!(TextureDesc, height),
        offset_of!(TextureDesc, mip_levels),
        offset_of!(TextureDesc, array_size),
        offset_of!(TextureDesc, format),
        offset_of!(TextureDesc, sample),
        offset_of!(TextureDesc, usage),
        offset_of!(TextureDesc, bind),
        offset_of!(TextureDesc, cpu),
        offset_of!(TextureDesc, misc),
        size_of::<SampleDesc>(),
        align_of::<SampleDesc>(),
        offset_of!(SampleDesc, count),
        offset_of!(SampleDesc, quality),
        size_of::<Subresource>(),
        align_of::<Subresource>(),
        offset_of!(Subresource, bytes),
        offset_of!(Subresource, pitch),
        offset_of!(Subresource, slice),
        size_of::<ViewDesc>(),
        align_of::<ViewDesc>(),
        offset_of!(ViewDesc, format),
        offset_of!(ViewDesc, dimension),
        offset_of!(ViewDesc, data),
        size_of::<TextureView>(),
        align_of::<TextureView>(),
        offset_of!(TextureView, first_mip),
        offset_of!(TextureView, mip_levels),
        1,
        2,
        5,
        7,
        29,
        37,
        1,
        8,
        4,
    ];
    if !out.is_null() && count == values.len() {
        unsafe { ptr::copy_nonoverlapping(values.as_ptr(), out, values.len()) };
    }
    values.len()
}
// Native caller supplies a live correctly typed COM interface. Read the method
// afresh for every call, including Release after a possibly reentrant method.
unsafe fn method(object: *mut c_void, slot: usize) -> usize {
    unsafe { object.cast::<*const usize>().read().add(slot).read() }
}
unsafe fn release(view: bool, object: *mut c_void, api: GuardApi, sink: *mut c_void) -> i32 {
    let callback = if view {
        api.release_view
    } else {
        api.release_texture
    };
    unsafe { callback(method(object, 2), object, sink) }
}
struct Upload {
    device: *mut c_void,
    host: *const Host,
    api: GuardApi,
    texture: *mut c_void,
    view: *mut c_void,
    exception: i32,
}
impl Drop for Upload {
    fn drop(&mut self) {
        for (view, object) in [(true, self.view), (false, self.texture)] {
            if !object.is_null() && unsafe { release(view, object, self.api, ptr::null_mut()) } != 1
            {
                // Original native Owned destructor is noexcept: a foreign
                // exception during final cleanup must terminate as before.
                unsafe extern "C" {
                    fn abort() -> !;
                }
                unsafe { abort() };
            }
        }
    }
}
unsafe extern "C" fn upload_call(
    raw: *mut c_void,
    op: u32,
    input: *const c_void,
    reply: *mut Reply,
) -> i32 {
    // Only this invocation owns Upload; callbacks cannot obtain its address.
    let c = unsafe { &mut *raw.cast::<Upload>() };
    let r = unsafe { &mut *reply };
    let status = unsafe {
        match op {
            1 => {
                r.hr = ((*c.host).validate)((*c.host).context);
                1
            }
            2 => (c.api.feature)(
                method(c.device, 37),
                c.device,
                &mut r.value,
                (*c.host).exception_sink,
            ),
            3 => {
                let format = ((*c.host).read)((*c.host).context, 0) as u32;
                (c.api.check_format)(
                    method(c.device, 29),
                    c.device,
                    format,
                    &mut r.value,
                    &mut r.hr,
                    (*c.host).exception_sink,
                )
            }
            4 => {
                let plan = &*input.cast::<TexturePlan>();
                let mut data = Vec::<Subresource>::new();
                if data.try_reserve_exact(plan.mip_levels as usize).is_err() {
                    c.exception = 2;
                    return 0;
                }
                for mip in &plan.mips[..plan.mip_levels as usize] {
                    // The public image's byte storage can change during
                    // CheckFormatSupport. Do not reuse the initial snapshot.
                    let base = ((*c.host).read)((*c.host).context, 1) as *const u8;
                    data.push(Subresource {
                        bytes: base.wrapping_add(mip.offset),
                        pitch: mip.row_pitch,
                        slice: mip.slice_pitch,
                    });
                }
                let desc = TextureDesc {
                    width: plan.width,
                    height: plan.height,
                    mip_levels: plan.mip_levels,
                    array_size: 1,
                    format: plan.format,
                    sample: SampleDesc {
                        count: 1,
                        quality: 0,
                    },
                    usage: 1,
                    bind: 8,
                    cpu: 0,
                    misc: 0,
                };
                (c.api.create_texture)(
                    method(c.device, 5),
                    c.device,
                    (&desc as *const TextureDesc).cast(),
                    data.as_ptr().cast(),
                    &mut c.texture,
                    &mut r.hr,
                    (*c.host).exception_sink,
                )
            }
            5 => {
                let plan = &*input.cast::<TexturePlan>();
                let desc = ViewDesc {
                    format: plan.format,
                    dimension: 4,
                    data: [0, plan.mip_levels, 0, 0],
                };
                (c.api.create_view)(
                    method(c.device, 7),
                    c.device,
                    c.texture,
                    (&desc as *const ViewDesc).cast(),
                    &mut c.view,
                    &mut r.hr,
                    (*c.host).exception_sink,
                )
            }
            6 | 7 => {
                let object = if op == 6 { &mut c.texture } else { &mut c.view };
                if object.is_null() {
                    1
                } else {
                    let status = release(op == 7, *object, c.api, (*c.host).exception_sink);
                    if status == 1 {
                        *object = ptr::null_mut();
                    }
                    status
                }
            }
            _ => return 0,
        }
    };
    if status != 1 {
        c.exception = if status == 2 { 2 } else { 3 };
        return 0;
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_native_upload(
    image: *const Image,
    constants: *const Constants,
    device: *mut c_void,
    has_output: i32,
    host: *const Host,
    guards: *const GuardApi,
    view_out: *mut *mut c_void,
    result: *mut ResultState,
) -> i32 {
    if !view_out.is_null() {
        unsafe { view_out.write(ptr::null_mut()) };
    }
    if host.is_null() || guards.is_null() || view_out.is_null() {
        return 0;
    }
    let mut c = Upload {
        device,
        host,
        api: unsafe { *guards },
        texture: ptr::null_mut(),
        view: ptr::null_mut(),
        exception: 0,
    };
    let called = unsafe {
        crate::d3d_resources::a2fo_rs_d3d_compressed_upload(
            image,
            constants,
            i32::from(!device.is_null()),
            has_output,
            (&mut c as *mut Upload).cast(),
            upload_call,
            result,
        )
    };
    if c.exception != 0 {
        return c.exception;
    }
    if called != 0 && unsafe { (*result).hr } >= 0 {
        unsafe { view_out.write(c.view) };
        c.view = ptr::null_mut();
    }
    called
}

#[repr(C)]
struct Handle {
    view: *mut c_void,
    api: GuardApi,
}
const E_FAIL: i32 = 0x80004005u32 as i32;
const E_OUTOFMEMORY: i32 = 0x8007000eu32 as i32;
type Create = unsafe extern "C" fn(*mut c_void, *mut *mut c_void) -> i32;
unsafe fn destroy(handle: *mut Handle) {
    if handle.is_null() {
        return;
    }
    let Handle { view, api } = unsafe { handle.read() };
    if !view.is_null() {
        let _ = unsafe { release(true, view, api, ptr::null_mut()) };
    }
    unsafe { dealloc(handle.cast(), Layout::new::<Handle>()) };
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_native_handle_initialize(
    owner: *mut *mut c_void,
    guards: *const GuardApi,
    context: *mut c_void,
    create: Create,
) -> i32 {
    if owner.is_null() || guards.is_null() {
        return E_FAIL;
    }
    // Fallible allocation precedes validation, matching make_unique<Impl>.
    let pending = unsafe { alloc(Layout::new::<Handle>()).cast::<Handle>() };
    if pending.is_null() {
        return E_OUTOFMEMORY;
    }
    unsafe {
        pending.write(Handle {
            view: ptr::null_mut(),
            api: *guards,
        })
    };
    let hr = unsafe { create(context, &mut (*pending).view) };
    if hr < 0 {
        unsafe { destroy(pending) };
        return hr;
    }
    // Publish first; releasing the former view may reenter its owner.
    let old = unsafe { owner.replace(pending.cast()).cast::<Handle>() };
    unsafe { destroy(old) };
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_native_handle_get(
    owner: *const c_void,
    output: *mut *mut c_void,
) -> i32 {
    if output.is_null() {
        return 0x80004003u32 as i32;
    }
    unsafe { output.write(ptr::null_mut()) };
    if owner.is_null() {
        return 0x8000ffffu32 as i32;
    }
    // Copy before calling COM: AddRef can reenter the owner.
    let (view, api) = unsafe {
        (
            (*owner.cast::<Handle>()).view,
            (*owner.cast::<Handle>()).api,
        )
    };
    if view.is_null() {
        return 0x8000ffffu32 as i32;
    }
    if unsafe { (api.addref_view)(method(view, 1), view, ptr::null_mut()) } != 1 {
        return E_FAIL;
    }
    unsafe { output.write(view) };
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3d_native_handle_drop(owner: *mut c_void) {
    unsafe { destroy(owner.cast()) };
}
