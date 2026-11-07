//! Refit native UI workflows. Native primitives retain all resource, C++
//! exception, thread and foreign-ABI ownership. No UI input is synthesized.
use core::ffi::c_void;
#[repr(C)]
pub struct ResultValue {
    value: u64,
    failed: u8,
}
type Step = unsafe extern "C" fn(*mut c_void, u32, u32, *mut ResultValue);
unsafe fn call(ctx: *mut c_void, s: Step, op: u32, i: u32) -> Option<u64> {
    let mut r = ResultValue {
        value: 0,
        failed: 0,
    };
    // SAFETY: synchronous callback fills its bounded POD result; exceptions stay
    // in the native context and are propagated only after the Rust call returns.
    unsafe {
        s(ctx, op, i, &mut r);
    }
    if r.failed != 0 { None } else { Some(r.value) }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_skin_dimensions(w: i32, h: i32) -> u8 {
    u8::from(w > 0 && h > 0 && w <= 8192 && h <= 8192 && w as u64 * h as u64 <= 8 * 1024 * 1024)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_skin_background_name(p: *const u8, n: usize) -> u8 {
    if p.is_null() || n != 15 {
        return 0;
    } // "BackgroundImage"
    // SAFETY: private caller supplies a live native string span.
    u8::from(unsafe { core::slice::from_raw_parts(p, n) } == b"BackgroundImage")
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_skin_legacy_coordinate(index: u32, value: i32) -> i32 {
    if index != 2 {
        value as u16 as i32
    } else {
        value
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_skin_shell_class(p: *const u8, n: usize) -> u8 {
    if p.is_null() || n > 96 {
        return 0;
    }
    // SAFETY: a bounded class name returned by the native adapter.
    let name = unsafe { core::slice::from_raw_parts(p, n) };
    let classes: &[&[u8]] = &[
        b"TMainForm",
        b"TSingleForm",
        b"TOptionsMenuForm",
        b"TGraphicOptionsForm",
        b"TSoundSettingsForm",
        b"TAdvancedOptionsForm",
        b"TAdvSettingsForm",
        b"TModSettingsForm",
        b"TMapSelectForm",
        b"TMapEditForm",
        b"TMapEditCreateForm",
        b"TManualIPForm",
        b"TMultiplayerConnectionForm",
        b"TMultiplayerSetup",
        b"TWaitingForm",
        b"TYesNoForm",
    ];
    u8::from(classes.contains(&name))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_skin_preflight(
    count: u32,
    ctx: *mut c_void,
    step: Option<Step>,
) -> u8 {
    let Some(s) = step else {
        return 0;
    };
    for i in 0..count {
        if unsafe { call(ctx, s, 0, i) } != Some(1) {
            return 0;
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_skin_init(count: u32, ctx: *mut c_void, step: Option<Step>) -> u8 {
    let Some(s) = step else {
        return 0;
    };
    // SAFETY: only fine-grained native reads/installs/stores occur synchronously.
    unsafe {
        if a2fo_rs_skin_preflight(count, ctx, Some(s)) == 0 {
            return 0;
        }
        if call(ctx, s, 1, 0).is_none() {
            return 0;
        }
        for i in 0..6 {
            if call(ctx, s, 2, i) != Some(1) {
                return 0;
            }
            if call(ctx, s, 3, i).is_none() {
                return 0;
            }
        }
        u8::from(call(ctx, s, 4, 0).is_some())
    }
}
unsafe fn skin_post(ctx: *mut c_void, s: Step) -> Option<()> {
    // SAFETY: the native context retains artwork/style strings and pixel storage.
    unsafe {
        if call(ctx, s, 0, 0)? == 0 || call(ctx, s, 12, 0)? == 0 {
            return Some(());
        }
        let styled = call(ctx, s, 13, 0)? != 0;
        if styled && call(ctx, s, 2, 0)? != 0 && call(ctx, s, 14, 0)? != 0 {
            call(ctx, s, 15, 0)?;
        }
        if styled && call(ctx, s, 16, 0)? < 32 {
            call(ctx, s, 17, 0)?;
        }
    }
    Some(())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_skin_load(ctx: *mut c_void, step: Option<Step>) -> u8 {
    let Some(s) = step else {
        return 1;
    };
    // SAFETY: native primitives contain exceptions. Pre-style errors are ignored,
    // original-call errors propagate after Rust returns; post-style errors log.
    unsafe {
        if call(ctx, s, 0, 0) == Some(1)
            && call(ctx, s, 1, 0).is_some()
            && call(ctx, s, 2, 0) == Some(1)
            && call(ctx, s, 3, 0) == Some(1)
        {
            let _ = call(ctx, s, 4, 0);
        }
        let _ = call(ctx, s, 19, 0);
        if call(ctx, s, 10, 0).is_none() || call(ctx, s, 11, 0).is_none() {
            return 1;
        }
        if skin_post(ctx, s).is_none() && call(ctx, s, 18, 0).is_none() {
            return 1;
        }
        let _ = call(ctx, s, 19, 0);
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_skin_compose(ctx: *mut c_void, step: Option<Step>) -> u8 {
    let Some(s) = step else {
        return 0;
    };
    // SAFETY: lock/storage ownership remains native; no references cross calls.
    unsafe {
        if call(ctx, s, 0, 0) != Some(1) || call(ctx, s, 1, 0) != Some(1) {
            return 0;
        }
        let Some(asset) = call(ctx, s, 2, 0) else {
            return 0;
        };
        if asset != 0 && call(ctx, s, 3, 0).is_none() {
            return 0;
        }
        let Some(requested) = call(ctx, s, 4, 0) else {
            return 0;
        };
        if requested == 0 && call(ctx, s, 5, 0) != Some(1) {
            return 0;
        }
        if call(ctx, s, 6, 0) != Some(1) || call(ctx, s, 7, 0) != Some(1) {
            return 0;
        }
        u8::from(call(ctx, s, 8, 0).is_some())
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_skin_paint(kind: u32, ctx: *mut c_void, step: Option<Step>) -> u8 {
    let Some(s) = step else {
        return 0;
    };
    // SAFETY: native primitives decode admitted objects, resolve native DC/window
    // ownership, compose into caller-owned storage and perform the original blit.
    unsafe {
        if call(ctx, s, 0, 0) != Some(1) || call(ctx, s, 1, 0) != Some(1) {
            return 0;
        }
        if kind == 0 {
            let Some(mut window) = call(ctx, s, 2, 0) else {
                return 0;
            };
            if window == 0 && call(ctx, s, 3, 0).is_some_and(|v| v != 0) {
                let Some(w) = call(ctx, s, 4, 0) else {
                    return 0;
                };
                window = w;
            }
            if window == 0 || call(ctx, s, 5, 0) != Some(1) {
                return 0;
            }
            if call(ctx, s, 6, 0).is_none() {
                return 0;
            }
            let (Some(w), Some(h)) = (call(ctx, s, 7, 0), call(ctx, s, 8, 0)) else {
                return 0;
            };
            if w as i64 <= 0 || h as i64 <= 0 || w as i64 > 8192 || h as i64 > 8192 {
                return 0;
            }
        } else {
            let Some(window) = call(ctx, s, 2, 0) else {
                return 0;
            };
            if window == 0 && call(ctx, s, 3, 0).is_none() {
                return 0;
            }
            let Some(valid) = call(ctx, s, 4, 0) else {
                return 0;
            };
            if valid == 0 && call(ctx, s, 6, 0).is_none() {
                return 0;
            }
            if call(ctx, s, 4, 0) != Some(1) || call(ctx, s, 5, 0) != Some(1) {
                return 0;
            }
        }
        if call(ctx, s, 9, 0) != Some(1) {
            return 0;
        }
        u8::from(call(ctx, s, 10, 0).is_some())
    }
}
