//! Camera/View and shell display runtime decisions. Native operations remain
//! at ABI, Win32, atomic and checked patch boundaries.
use core::{ffi::c_void, slice};
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
#[repr(C)]
pub struct RefitConfig {
    enabled: Bytes,
    value: Bytes,
    enabled_length: u32,
    value_length: u32,
}
type RefitRead = unsafe extern "C" fn(*mut c_void, *mut RefitConfig) -> u8;
type RefitNumber = unsafe extern "C" fn(*mut c_void, Bytes, *mut f32) -> u8;
type RefitStep = unsafe extern "C" fn(*mut c_void, u32, u32) -> u8;
type RefitLog = unsafe extern "C" fn(*mut c_void, u32);
type RefitShutdown = unsafe extern "C" fn(*mut c_void, u32);
fn space(ch: u8) -> bool {
    matches!(ch, b' ' | b'\t' | b'\n' | b'\r' | b'\x0c' | b'\x0b')
}
fn trim(mut value: &[u8]) -> &[u8] {
    while value.first().is_some_and(|&ch| space(ch)) {
        value = &value[1..]
    }
    while value.last().is_some_and(|&ch| space(ch)) {
        value = &value[..value.len() - 1]
    }
    value
}
fn span(value: &[u8]) -> Bytes {
    Bytes {
        data: value.as_ptr(),
        size: value.len(),
    }
}
unsafe fn parse(
    camera: u8,
    config: RefitConfig,
    number: RefitNumber,
    ctx: *mut c_void,
) -> Option<(bool, f32)> {
    let enabled = trim(unsafe { config.enabled.get() }?);
    let enabled = match enabled {
        b"0" => false,
        b"1" => true,
        _ => return None,
    };
    let value = trim(unsafe { config.value.get() }?);
    if value.is_empty() {
        return None;
    }
    let mut parsed = 0.0;
    if unsafe { number(ctx, span(value), &mut parsed) } == 0 {
        return None;
    }
    let (minimum, maximum) = if camera != 0 {
        (0.25, 10.0)
    } else {
        (0.0, 1_000_000.0)
    };
    if !parsed.is_finite() || parsed < minimum || parsed > maximum {
        return None;
    }
    Some((enabled, parsed))
}
/// Safety: callbacks and borrowed config storage remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_refit_init(
    valid: u8,
    already: u8,
    camera: u8,
    builtin: u8,
    read: Option<RefitRead>,
    number: Option<RefitNumber>,
    step: Option<RefitStep>,
    log: Option<RefitLog>,
    ctx: *mut c_void,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    if already != 0 {
        return 1;
    }
    let (Some(read), Some(number), Some(step), Some(log)) = (read, number, step, log) else {
        return 0;
    };
    unsafe { step(ctx, 0, 0) }; // publish API and initialized latch
    if unsafe { step(ctx, 1, 0) } == 0 {
        unsafe { log(ctx, 0) };
        return 1;
    }
    let mut config = RefitConfig {
        enabled: Bytes {
            data: core::ptr::null(),
            size: 0,
        },
        value: Bytes {
            data: core::ptr::null(),
            size: 0,
        },
        enabled_length: 0,
        value_length: 0,
    };
    if unsafe { read(ctx, &mut config) } == 0
        || config.enabled_length == 0
        || config.enabled_length >= 31
        || config.value_length == 0
        || config.value_length >= 63
    {
        unsafe { log(ctx, 1) };
        return 1;
    }
    let Some((enabled, value)) = (unsafe { parse(camera, config, number, ctx) }) else {
        unsafe { log(ctx, 2) };
        return 1;
    };
    let value_bits = value.to_bits();
    unsafe { step(ctx, 2, value_bits) }; // configured_value
    if !enabled || value == if camera != 0 { 1.0 } else { 0.0 } {
        unsafe {
            step(ctx, 3, 0);
            log(ctx, 3)
        };
        return 1;
    }
    if unsafe { step(ctx, 4, 0) } == 0 {
        unsafe { log(ctx, 4) };
        return 1;
    }
    if unsafe { step(ctx, 5, 0) } == 0 {
        unsafe { log(ctx, 5) };
        return 1;
    }
    if unsafe { step(ctx, 6, 0) } == 0 {
        unsafe { log(ctx, 6) };
        return 1;
    }
    if unsafe { step(ctx, 7, 0) } == 0 {
        return 1;
    }
    if builtin == 0 {
        unsafe { step(ctx, 8, value_bits) };
    }
    if unsafe { step(ctx, 9, 0) } == 0 {
        unsafe {
            step(ctx, 10, if camera != 0 { 0x3f800000 } else { 0 });
            log(ctx, 7)
        };
        return 1;
    }
    unsafe {
        step(ctx, 11, 0);
        log(ctx, 8)
    };
    1
}
/// The native aligned value word remains owned and updated by C++.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_refit_value_bits(
    camera: u8,
    installed: u8,
    enabled: u8,
    configured_bits: u32,
) -> u32 {
    if installed != 0 && enabled != 0 {
        configured_bits
    } else if camera != 0 {
        0x3f800000
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_refit_armed(camera: u8, installed: u8, bits: u32) -> u8 {
    u8::from(installed != 0 && bits != if camera != 0 { 0x3f800000 } else { 0 })
}
/// Safety: callback remains valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_refit_shutdown(
    step: Option<RefitShutdown>,
    ctx: *mut c_void,
) {
    if let Some(step) = step {
        unsafe {
            step(ctx, 0);
            step(ctx, 1);
            step(ctx, 2)
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ShellScope {
    device: usize,
    selected: usize,
    width: i32,
    height: i32,
    depth: i32,
    borderless: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ShellCandidate {
    profile_present: u8,
    editor: u8,
    config_readable: u8,
    mode_readable: u8,
    scope: ShellScope,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Rect {
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}
type ActivateStep = unsafe extern "C" fn(*mut c_void, u32, *const ShellScope, isize) -> isize;
type ResizeStep = unsafe extern "C" fn(*mut c_void, u32, isize, isize, *mut Rect) -> usize;
type ShellInitStep = unsafe extern "C" fn(*mut c_void, u32) -> u8;
type ShellLog = unsafe extern "C" fn(*mut c_void, u32);
fn valid_mode(s: ShellScope) -> bool {
    (640..=16384).contains(&s.width)
        && (480..=16384).contains(&s.height)
        && matches!(s.depth, 16 | 32)
}
fn selected(ready: u8, display: i32, mode: i32, c: ShellCandidate) -> ShellScope {
    if ready != 0
        && display == 0
        && mode == 3
        && c.profile_present != 0
        && c.editor == 0
        && c.config_readable != 0
        && c.mode_readable != 0
        && c.scope.device != 0
        && c.scope.selected != 0
        && valid_mode(c.scope)
    {
        c.scope
    } else {
        ShellScope::default()
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_shell_select(
    ready: u8,
    display: i32,
    mode: i32,
    candidate: ShellCandidate,
) -> ShellScope {
    selected(ready, display, mode, candidate)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_shell_lookup_route(
    ready: u8,
    selected: usize,
    scope_device: usize,
    device: usize,
) -> u8 {
    u8::from(ready != 0 && selected != 0 && device == scope_device)
}
/// Safety: callback and native scope storage remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_shell_activate(
    ready: u8,
    display: i32,
    mode: i32,
    force: u8,
    candidate: ShellCandidate,
    applied: ShellScope,
    step: Option<ActivateStep>,
    ctx: *mut c_void,
) -> i32 {
    let Some(step) = step else {
        return 0;
    };
    let scope = selected(ready, display, mode, candidate);
    unsafe { step(ctx, 0, &scope, 0) };
    if ready != 0
        && display == 0
        && mode == 3
        && scope.selected == 0
        && unsafe { step(ctx, 1, &scope, 0) } < 4
    {
        unsafe { step(ctx, 2, &scope, 0) };
    }
    let changed = scope.selected != 0
        && (applied.selected == 0
            || scope.device != applied.device
            || scope.borderless != applied.borderless
            || scope.width != applied.width
            || scope.height != applied.height
            || scope.depth != applied.depth);
    if display == 0 {
        unsafe { step(ctx, 3, &scope, 0) };
        if scope.selected != 0 {
            unsafe { step(ctx, 4, &scope, 0) };
        }
    }
    let result = unsafe { step(ctx, 5, &scope, isize::from(force != 0 || changed)) } as i32;
    if display == 0 {
        unsafe { step(ctx, 6, &scope, result as isize) };
        if result != 0 || scope.selected == 0 {
            unsafe { step(ctx, 7, &scope, 0) };
        }
        unsafe { step(ctx, 8, &scope, 0) };
    }
    unsafe { step(ctx, 9, &scope, 0) };
    result
}
fn centered(start: i32, end: i32, size: i32) -> i32 {
    start.wrapping_add(if end.wrapping_sub(start) > size {
        end.wrapping_sub(start).wrapping_sub(size) / 2
    } else {
        0
    })
}
/// Safety: callback and native window state remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_shell_resize(
    ready: u8,
    scope: ShellScope,
    window_present: u8,
    width: i32,
    height: i32,
    fullscreen: u8,
    adapter: u32,
    step: Option<ResizeStep>,
    ctx: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let scoped = ready != 0 && scope.selected != 0;
    let borderless = scoped && scope.borderless != 0;
    let (width, height) = if scoped {
        (scope.width, scope.height)
    } else {
        (width, height)
    };
    unsafe {
        step(
            ctx,
            10,
            isize::from(if scoped { borderless } else { fullscreen != 0 }),
            0,
            core::ptr::null_mut(),
        )
    };
    unsafe {
        step(
            ctx,
            0,
            width as isize,
            height as isize,
            core::ptr::null_mut(),
        )
    };
    // The native gateway can replace or clear the HWND. Borrow it only after
    // that callback, and never dereference an unscoped caller's window pointer.
    if !scoped || window_present == 0 || unsafe { step(ctx, 11, 0, 0, core::ptr::null_mut()) } == 0
    {
        return;
    }
    let form = unsafe { step(ctx, 1, 0, 0, core::ptr::null_mut()) };
    let mut monitor = Rect::default();
    let mut work = Rect::default();
    let have_monitor = unsafe { step(ctx, 2, adapter as isize, 0, &mut monitor) } != 0
        && unsafe { step(ctx, 3, adapter as isize, 0, &mut work) } != 0
        && monitor.right > monitor.left
        && monitor.bottom > monitor.top;
    let client_width = if borderless && have_monitor {
        monitor.right - monitor.left
    } else {
        width
    };
    let client_height = if borderless && have_monitor {
        monitor.bottom - monitor.top
    } else {
        height
    };
    if form != 0 {
        unsafe {
            step(
                ctx,
                4,
                client_height as isize,
                client_width as isize,
                core::ptr::null_mut(),
            )
        };
    }
    let mut outer = Rect::default();
    if form != 0 && have_monitor && (borderless || unsafe { step(ctx, 5, 0, 0, &mut outer) } != 0) {
        let area = if borderless { monitor } else { work };
        let left = if borderless {
            area.left
        } else {
            centered(area.left, area.right, outer.right - outer.left)
        };
        let top = if borderless {
            area.top
        } else {
            centered(area.top, area.bottom, outer.bottom - outer.top)
        };
        unsafe { step(ctx, 6, left as isize, top as isize, core::ptr::null_mut()) };
    }
    let mut client = Rect::default();
    unsafe { step(ctx, 7, 0, 0, &mut client) };
    unsafe {
        step(
            ctx,
            8,
            (client.right - client.left) as isize,
            (client.bottom - client.top) as isize,
            core::ptr::null_mut(),
        )
    };
    unsafe {
        step(
            ctx,
            9,
            width as isize,
            height as isize,
            core::ptr::null_mut(),
        )
    };
}
/// Safety: callbacks remain valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_shell_init(
    valid: u8,
    step: Option<ShellInitStep>,
    log: Option<ShellLog>,
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
    if unsafe { step(ctx, 1) } == 0 {
        unsafe { log(ctx, 0) };
        return 0;
    }
    if unsafe { step(ctx, 2) } == 0 {
        unsafe { log(ctx, 0) };
        return 0;
    }
    if unsafe { step(ctx, 3) } == 0 {
        unsafe { log(ctx, 1) };
        return 0;
    }
    if unsafe { step(ctx, 4) } == 0 {
        unsafe { log(ctx, 2) };
        return 0;
    }
    if unsafe { step(ctx, 5) } == 0 {
        unsafe { log(ctx, 3) };
        return 0;
    }
    if unsafe { step(ctx, 6) } == 0 {
        return 0;
    }
    unsafe { step(ctx, 7) };
    if unsafe { step(ctx, 8) } != 0 {
        unsafe { step(ctx, 9) };
    }
    if unsafe { step(ctx, 10) } == 0 || unsafe { step(ctx, 11) } == 0 {
        unsafe { log(ctx, 4) };
        return 1;
    }
    unsafe { step(ctx, 12) };
    if unsafe { step(ctx, 13) } == 0 {
        unsafe { log(ctx, 5) };
    }
    if unsafe { step(ctx, 14) } == 0 {
        unsafe { log(ctx, 6) };
    } else if unsafe { step(ctx, 15) } == 0 {
        unsafe { log(ctx, 7) };
    }
    unsafe { log(ctx, 8) };
    1
}
/// Safety: callback remains valid synchronously.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_runtime_shell_shutdown(
    step: Option<ShellInitStep>,
    ctx: *mut c_void,
) {
    if let Some(step) = step {
        for op in 16..=20 {
            unsafe { step(ctx, op) };
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_runtime_shell_scope_eligible(
    ready: u8,
    display: i32,
    mode: i32,
    profile_present: u8,
    editor: u8,
) -> u8 {
    u8::from(ready != 0 && display == 0 && mode == 3 && profile_present != 0 && editor == 0)
}
