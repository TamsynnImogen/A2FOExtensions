//! Native Bink presentation orchestration. C++ owns TLS, GDI resources and ABI
//! calls. All callbacks are synchronous; no locked pixel pointer is retained.
use core::ffi::c_void;
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Plan {
    source_width: u32,
    source_height: u32,
    width: i32,
    height: i32,
    fitted_width: i32,
    fitted_height: i32,
    left: i32,
    top: i32,
    pitch: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Frame {
    draw: u8,
    drawn: u8,
    copy_ok: u8,
}
type Step = unsafe extern "C" fn(*mut c_void, u32, *mut Plan) -> usize;
type Next = unsafe extern "C" fn(*mut c_void, u32) -> i32;
type Draw = unsafe extern "C" fn(*mut c_void) -> u8;
fn sane(n: u32) -> bool {
    n > 0 && n <= 16384
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_intro_draw(
    bink: u8,
    pixels: u8,
    pitch: i32,
    buffer_height: u32,
    x: u32,
    y: u32,
    flags: u32,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let format = flags & 0x7fff_ffff;
    if bink == 0
        || pixels == 0
        || (format != 3 && format != 5)
        || x != 0
        || y != 0
        || pitch <= 0
        || pitch % 4 != 0
        || !sane((pitch / 4) as u32)
    {
        return 0;
    }
    let Some(step) = step else {
        return 0;
    };
    let mut p = Plan {
        pitch,
        ..Plan::default()
    };
    let plan = core::ptr::addr_of_mut!(p);
    // SAFETY: private native adapter fills only the live POD plan and owns all
    // native handles/pixels. No Rust reference is held across a native call.
    unsafe {
        step(context, 0, plan);
        if !sane(p.source_width)
            || !sane(p.source_height)
            || p.source_height > buffer_height
            || p.source_width > (pitch / 4) as u32
        {
            return 0;
        }
        if step(context, 1, plan) == 0
            || p.width <= 0
            || p.height <= 0
            || p.width > 16384
            || p.height > 16384
        {
            return 0;
        }
        p.fitted_width = p.width;
        p.fitted_height = (p.source_height as u64 * p.width as u64 / p.source_width as u64) as i32;
        if p.fitted_height > p.height {
            p.fitted_height = p.height;
            p.fitted_width =
                (p.source_width as u64 * p.height as u64 / p.source_height as u64) as i32;
        }
        if p.fitted_width < 1 || p.fitted_height < 1 {
            return 0;
        }
        p.left = (p.width - p.fitted_width) / 2;
        p.top = (p.height - p.fitted_height) / 2;
        core::ptr::write(plan, p);
        if step(context, 2, plan) == 0 {
            return 0;
        }
        step(context, 3, plan);
        let drawn = if step(context, 4, plan) != 0 {
            step(context, 5, plan) as u32 as i32
        } else {
            -1
        };
        let flushed = step(context, 6, plan) != 0;
        if drawn == 0 || drawn == -1 || !flushed {
            return 0;
        }
        step(context, 7, plan);
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_intro_present(
    buffer: u8,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if buffer == 0 {
        return 0;
    }
    // SAFETY: native context owns DC acquisition/release and offscreen storage.
    unsafe {
        if step(context, 0, core::ptr::null_mut()) == 0 {
            return 0;
        }
        let copied = step(context, 1, core::ptr::null_mut()) != 0;
        let flushed = step(context, 2, core::ptr::null_mut()) != 0;
        step(context, 3, core::ptr::null_mut());
        if !copied || !flushed {
            return 0;
        }
        if step(context, 4, core::ptr::null_mut()) == 0 {
            step(context, 5, core::ptr::null_mut());
        }
    }
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_intro_scope(enabled: u8, ready: u8, bink: u8, window: u8) -> u8 {
    u8::from(enabled != 0 && ready != 0 && bink != 0 && window != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_intro_copy(
    state: *mut Frame,
    ready: u8,
    enabled: u8,
    same: u8,
    result: i32,
    draw: Option<Draw>,
    context: *mut c_void,
) {
    if state.is_null() {
        return;
    }
    // SAFETY: caller-owned TLS state remains live, including during reentry.
    // Access through raw pointers avoids holding &mut across native callbacks.
    unsafe {
        if ready == 0 || enabled == 0 || same == 0 || (*state).draw == 0 {
            return;
        }
        (*state).copy_ok = u8::from(result == 0);
        if result == 0 {
            (*state).drawn = draw.map_or(0, |f| f(context));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_intro_next(
    state: *mut Frame,
    ready: u8,
    enabled: u8,
    same: u8,
    show: u8,
    next: Option<Next>,
    context: *mut c_void,
) -> i32 {
    let Some(next) = next else {
        return 0;
    };
    // SAFETY: native callback owns ABI arguments; TLS state is never borrowed
    // across a callback and nested calls restore the previous complete state.
    unsafe {
        if state.is_null() || ready == 0 || enabled == 0 || same == 0 || show == 0 {
            return next(context, 0);
        }
        let previous = state.read();
        state.write(Frame {
            draw: 1,
            drawn: 0,
            copy_ok: 0,
        });
        let result = next(context, 1);
        if (*state).drawn != 0 {
            (*state).drawn = u8::from(next(context, 2) != 0);
        }
        if (*state).drawn == 0 && (*state).copy_ok != 0 {
            next(context, 3);
        }
        state.write(previous);
        result
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_intro_init(
    valid: u8,
    expected: usize,
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if valid == 0 {
        return 0;
    }
    // SAFETY: callbacks are the checked native primitives, synchronously invoked
    // in original order; partial installation leaves readiness false.
    unsafe {
        let p = core::ptr::null_mut();
        step(context, 0, p);
        if step(context, 1, p) != expected || step(context, 2, p) == 0 || step(context, 3, p) == 0 {
            step(context, 10, p);
            return 0;
        }
        if step(context, 4, p) == 0 {
            return 0;
        }
        let exported = step(context, 5, p);
        let imported = step(context, 6, p);
        if exported == 0 || imported != exported {
            step(context, 11, p);
            return 0;
        }
        step(context, 7, p);
        if step(context, 8, p) == 0 || step(context, 9, p) == 0 {
            step(context, 12, p);
            return 0;
        }
        step(context, 13, p);
    }
    1
}
