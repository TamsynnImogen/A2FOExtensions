//! Portable A2FOVideo lookup, playback routing, skip and completion policy.
//! Native callbacks retain C++ strings, filesystem, codecs and Windows state.

use core::ffi::{CStr, c_char, c_void};

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

// A callback returns 3 after catching a native exception. Stop immediately;
// the C++ wrapper rethrows after Rust has returned.
const THREW: usize = 3;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_resolve(
    step: Option<Step>,
    context: *mut c_void,
    has_path: u8,
    root_count: *const u32,
) -> u8 {
    let Some(step) = step else { return 0 };
    if root_count.is_null() {
        return 0;
    }
    if has_path != 0 {
        match unsafe { step(context, 0, 0) } {
            1 => return 1,
            THREW => return 0,
            _ => {}
        }
    }
    if unsafe { step(context, 1, 0) } == THREW {
        return 0;
    }
    let count = unsafe { *root_count } as usize;
    for index in (0..count).rev() {
        match unsafe { step(context, 2, index) } {
            1 => return 1,
            THREW => return 0,
            2 => continue,
            _ => {}
        }
        match unsafe { step(context, 3, index) } {
            1 => return 1,
            THREW => return 0,
            _ => {}
        }
    }
    match unsafe { step(context, 4, 0) } {
        1 => return 1,
        THREW => return 0,
        2 => return 0,
        _ => {}
    }
    (unsafe { step(context, 5, 0) } == 1) as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_replacement_kind(extension: *const c_char) -> u8 {
    if extension.is_null() {
        return 0;
    }
    let ext = unsafe { CStr::from_ptr(extension) }.to_bytes();
    if matches!(ext, b".mp4" | b".webm" | b".mkv") {
        return 1;
    }
    if !ext.is_empty() && ext != b".bik" {
        return 0;
    }
    2
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_replacement(
    step: Option<Step>,
    context: *mut c_void,
    kind: u8,
) -> u8 {
    let Some(step) = step else { return 0 };
    if kind == 1 {
        return (unsafe { step(context, 0, 0) } == 1) as u8;
    }
    if kind != 2 {
        return 0;
    }
    for index in 0..3 {
        match unsafe { step(context, 1, index) } {
            1 => return 1,
            THREW => return 0,
            _ => {}
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_play(step: Option<Step>, context: *mut c_void) -> u8 {
    let Some(step) = step else { return 0 };
    let found = unsafe { step(context, 0, 0) };
    if found == THREW {
        return 0;
    }
    if found == 1 {
        let played = unsafe { step(context, 1, 0) };
        if played == THREW {
            return 0;
        }
        if played == 1 {
            return 1;
        }
    }
    (unsafe { step(context, 2, (found == 1) as usize) } == 1) as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_event_poll(
    step: Option<Step>,
    context: *mut c_void,
    stopped: u8,
    dispatch_paints: u8,
) -> u8 {
    let Some(step) = step else { return 0 };
    if stopped != 0 || unsafe { step(context, 0, 0) } != 0 || unsafe { step(context, 1, 0) } == 0 {
        return 0;
    }
    if dispatch_paints == 0 {
        return 1;
    }
    for _ in 0..64 {
        let found = unsafe { step(context, 2, 0) };
        if found == 2 || unsafe { step(context, 3, 0) } == 0 {
            return 0;
        }
        if found == 0 {
            break;
        }
        if unsafe { step(context, 4, 0) } == 0 {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_wave_submit(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 1, 0) } != 0 {
        if unsafe { step(context, 2, 0) } == 0 {
            return 0;
        }
        unsafe { step(context, 3, 0) };
    }
    if unsafe { step(context, 4, 0) } == THREW {
        return 0;
    }
    if unsafe { step(context, 5, 0) } == 0 {
        return 0;
    }
    if unsafe { step(context, 6, 0) } == 0 {
        return 0;
    }
    unsafe { step(context, 7, 0) };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_wave_drain(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) -> u8 {
    let Some(step) = step else { return 0 };
    for index in 0..count {
        if unsafe { step(context, 0, index) } != 0 && unsafe { step(context, 1, index) } == 0 {
            return 0;
        }
    }
    unsafe { step(context, 2, 0) as u8 }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_wave_close(
    step: Option<Step>,
    context: *mut c_void,
    count: usize,
) {
    let Some(step) = step else { return };
    if unsafe { step(context, 0, 0) } == 0 {
        return;
    }
    unsafe { step(context, 1, 0) };
    for index in 0..count {
        if unsafe { step(context, 2, index) } != 0 {
            unsafe { step(context, 3, index) };
        }
    }
    unsafe { step(context, 4, 0) };
}

unsafe fn receive_audio(step: Step, context: *mut c_void) -> u8 {
    loop {
        match unsafe { step(context, 4, 0) } {
            1 | 3 => return 1,
            2 => return 0,
            _ => {}
        }
        match unsafe { step(context, 5, 0) } {
            THREW => return 2,
            0 => return 0,
            _ => {}
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_audio_decode(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 1 };
    let mut failed = false;
    while unsafe { step(context, 0, 0) } != 0 {
        if unsafe { step(context, 1, 0) } != 0 {
            let mut sent = unsafe { step(context, 2, 0) };
            if sent == 1 {
                match unsafe { receive_audio(step, context) } {
                    2 => return 2,
                    0 => failed = true,
                    _ => {}
                }
                if !failed {
                    sent = unsafe { step(context, 2, 0) };
                }
            }
            if !failed && sent != 0 {
                unsafe { step(context, 3, 0) };
                failed = true;
            }
            if !failed {
                match unsafe { receive_audio(step, context) } {
                    2 => return 2,
                    0 => failed = true,
                    _ => {}
                }
            }
        }
        unsafe { step(context, 6, 0) };
        if failed {
            break;
        }
    }
    if !failed && unsafe { step(context, 12, 0) } == 0 {
        if unsafe { step(context, 7, 0) } != 0 || unsafe { step(context, 8, 0) } == 0 {
            failed = true;
        } else {
            match unsafe { receive_audio(step, context) } {
                2 => return 2,
                0 => failed = true,
                _ => {}
            }
        }
    }
    if !failed && unsafe { step(context, 9, 0) } != 0 {
        loop {
            match unsafe { step(context, 10, 0) } {
                0 => break,
                1 => {}
                THREW => return 2,
                _ => {
                    failed = true;
                    break;
                }
            }
        }
    }
    if !failed && unsafe { step(context, 12, 0) } == 0 && unsafe { step(context, 11, 0) } == 0 {
        failed = true;
    }
    failed as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_video_decode(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    while unsafe { step(context, 0, 0) } != 0 {
        if unsafe { step(context, 1, 0) } != 0 {
            if unsafe { step(context, 2, 0) } == 0 {
                unsafe { step(context, 6, 0) };
                break;
            }
            while unsafe { step(context, 7, 0) } & 1 == 0 {
                match unsafe { step(context, 3, 0) } {
                    1 | 2 => break,
                    _ => {}
                }
                if unsafe { step(context, 4, 0) } == THREW {
                    return 2;
                }
                unsafe { step(context, 5, 0) };
                if unsafe { step(context, 7, 0) } & 2 != 0 {
                    break;
                }
            }
        }
        unsafe { step(context, 6, 0) };
        if unsafe { step(context, 7, 0) } & 2 != 0 {
            break;
        }
    }
    if unsafe { step(context, 7, 0) } == 0 {
        unsafe { step(context, 8, 0) };
        loop {
            match unsafe { step(context, 3, 0) } {
                1 | 2 => break,
                _ => {}
            }
            if unsafe { step(context, 4, 0) } == THREW {
                return 2;
            }
            unsafe { step(context, 5, 0) };
            if unsafe { step(context, 7, 0) } != 0 {
                break;
            }
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_audio_join(
    step: Option<Step>,
    context: *mut c_void,
    skipped: u8,
    failed: u8,
) {
    let Some(step) = step else { return };
    if skipped != 0 || failed != 0 {
        unsafe { step(context, 0, 0) };
    }
    if unsafe { step(context, 1, 0) } == 0 {
        return;
    }
    while unsafe { step(context, 2, 0) } != 0 {
        unsafe { step(context, 3, 0) };
        unsafe { step(context, 4, 0) };
    }
    unsafe { step(context, 5, 0) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_open_decoder(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    for action in 0..6 {
        if unsafe { step(context, action, 0) } != 1 {
            return 0;
        }
    }
    1
}

#[repr(C)]
pub struct A2FO_Stage9VideoSkipState {
    pub ready_at: u64,
    pub previous: u32,
    pub armed: u8,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_video_skip_poll(
    state: *mut A2FO_Stage9VideoSkipState,
    down: u32,
    owned_window: u8,
    now: u64,
) -> u32 {
    let Some(state) = (unsafe { state.as_mut() }) else {
        return 0;
    };
    if owned_window == 0 || now < state.ready_at {
        state.armed = 0;
        state.previous = down;
        return 0;
    }
    if state.armed == 0 {
        state.armed = (down == 0) as u8;
        state.previous = down;
        return 0;
    }
    let fresh = down & !state.previous;
    state.previous = down;
    fresh
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_video_result(cancelled: u8, frames: u64, failed: u8) -> u8 {
    (cancelled != 0 || (frames > 0 && failed == 0)) as u8
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Lookup {
        count: u32,
        calls: Vec<(u32, usize)>,
    }

    unsafe extern "C" fn lookup(context: *mut c_void, action: u32, index: usize) -> usize {
        let ctx = unsafe { &mut *context.cast::<Lookup>() };
        ctx.calls.push((action, index));
        match action {
            1 => {
                ctx.count = 3;
                0
            }
            3 if index == 1 => 1,
            _ => 0,
        }
    }

    #[test]
    fn latest_root_and_direct_probe_order() {
        let mut ctx = Lookup {
            count: 0,
            calls: Vec::new(),
        };
        let found = unsafe {
            a2fo_rs_stage9_video_resolve(
                Some(lookup),
                (&raw mut ctx).cast(),
                1,
                &raw const ctx.count,
            )
        };
        assert_eq!(found, 1);
        assert_eq!(ctx.calls, [(0, 0), (1, 0), (2, 2), (3, 2), (2, 1), (3, 1)]);
    }

    #[test]
    fn fresh_press_requires_release_and_owned_window() {
        let mut state = A2FO_Stage9VideoSkipState {
            ready_at: 250,
            previous: 1,
            armed: 0,
        };
        let poll = |state: &mut A2FO_Stage9VideoSkipState, down, owned, now| unsafe {
            a2fo_rs_stage9_video_skip_poll(state, down, owned, now)
        };
        assert_eq!(poll(&mut state, 1, 1, 300), 0);
        assert_eq!(poll(&mut state, 0, 1, 300), 0);
        assert_eq!(poll(&mut state, 1, 0, 300), 0);
        assert_eq!(poll(&mut state, 1, 1, 300), 0);
        assert_eq!(poll(&mut state, 0, 1, 300), 0);
        assert_eq!(poll(&mut state, 1, 1, 300), 1);
    }

    unsafe extern "C" fn close_steps(context: *mut c_void, action: u32, index: usize) -> usize {
        let calls = unsafe { &mut *context.cast::<Vec<(u32, usize)>>() };
        calls.push((action, index));
        match action {
            0 | 2 => 1,
            _ => 0,
        }
    }

    #[test]
    fn partial_output_cleanup_keeps_reset_then_unprepare_then_close() {
        let mut calls: Vec<(u32, usize)> = Vec::new();
        unsafe { a2fo_rs_stage9_video_wave_close(Some(close_steps), (&raw mut calls).cast(), 2) };
        assert_eq!(
            calls,
            [(0, 0), (1, 0), (2, 0), (3, 0), (2, 1), (3, 1), (4, 0)]
        );
    }

    struct DecodeTrace {
        calls: Vec<u32>,
        read: bool,
        failed: bool,
    }

    unsafe extern "C" fn audio_failure(context: *mut c_void, action: u32, _: usize) -> usize {
        let ctx = unsafe { &mut *context.cast::<DecodeTrace>() };
        ctx.calls.push(action);
        match action {
            0 if !ctx.read => {
                ctx.read = true;
                1
            }
            1 => 1,
            2 => 2,
            _ => 0,
        }
    }

    #[test]
    fn audio_send_failure_unrefs_packet_without_flush_or_drain() {
        let mut ctx = DecodeTrace {
            calls: Vec::new(),
            read: false,
            failed: false,
        };
        let result = unsafe {
            a2fo_rs_stage9_video_audio_decode(Some(audio_failure), (&raw mut ctx).cast())
        };
        assert_eq!(result, 1);
        assert_eq!(ctx.calls, [0, 1, 2, 3, 6]);
    }

    unsafe extern "C" fn video_failure(context: *mut c_void, action: u32, _: usize) -> usize {
        let ctx = unsafe { &mut *context.cast::<DecodeTrace>() };
        ctx.calls.push(action);
        match action {
            0 if !ctx.read => {
                ctx.read = true;
                1
            }
            1 => 1,
            2 => {
                ctx.failed = true;
                0
            }
            7 => (ctx.failed as usize) << 1,
            _ => 0,
        }
    }

    #[test]
    fn video_send_failure_unrefs_packet_without_frame_or_flush() {
        let mut ctx = DecodeTrace {
            calls: Vec::new(),
            read: false,
            failed: false,
        };
        let result = unsafe {
            a2fo_rs_stage9_video_video_decode(Some(video_failure), (&raw mut ctx).cast())
        };
        assert_eq!(result, 0);
        assert_eq!(ctx.calls, [0, 1, 2, 6, 7]);
    }
}
