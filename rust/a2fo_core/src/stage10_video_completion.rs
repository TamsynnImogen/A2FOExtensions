//! Remaining portable A2FOVideo frame admission and wait workflows.
//! FFmpeg, WinMM, QPC arithmetic, GDI and C++ resource ownership stay native.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
const THREW: usize = 3;

/// # Safety
/// `context` must remain valid for every callback invocation. `step`, when
/// present, must use the declared C ABI and must not unwind into Rust. The
/// native wrapper must catch callback exceptions and report them as the
/// action-specific failure value; it owns all frame and resampler resources.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_video_audio_frame(
    step: Option<Step>,
    context: *mut c_void,
    frame_present: u8,
    needs_setup: u8,
) -> u8 {
    let Some(step) = step else { return 0 };
    if frame_present == 0 {
        return 0;
    }
    if needs_setup != 0 {
        match unsafe { step(context, 0, 0) } {
            1 => {}
            THREW => return 3,
            _ => return 0,
        }
    }
    match unsafe { step(context, 1, 0) } {
        1 => return 1, // No output samples: successful no-op.
        2 => {}
        THREW => return 3,
        _ => return 0,
    }
    match unsafe { step(context, 2, 0) } {
        1 => 1,
        THREW => 3,
        _ => 0,
    }
}

/// # Safety
/// `context` must remain valid throughout the polling loop. `step`, when
/// present, must use the declared C ABI and must not unwind into Rust. It
/// must terminate the loop by eventually reporting elapsed time or a failed
/// event poll; native code owns the clock arithmetic and sleeping.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_video_wait(
    step: Option<Step>,
    context: *mut c_void,
    negative_seconds: u8,
) -> u8 {
    let Some(step) = step else { return 0 };
    if negative_seconds != 0 {
        return (unsafe { step(context, 0, 0) } == 1) as u8;
    }
    loop {
        if unsafe { step(context, 0, 0) } != 1 {
            return 0;
        }
        match unsafe { step(context, 1, 0) } {
            1 => return 1,
            2 => unsafe { step(context, 2, 0) },
            3 => unsafe { step(context, 2, 1) },
            _ => return 0,
        };
    }
}

// Return 0 on skip, 1 to compose, 2 on window/event cancellation, 3 on
// caught native exception. The C++ wrapper applies flags at the original site.
/// # Safety
/// `context` must remain valid for all callback invocations. `step`, when
/// present, must use the declared C ABI and must not unwind into Rust. The
/// callback must keep its frame and timing state valid until this call returns;
/// native code applies the returned admission status to its playback flags.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_video_present_admission(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else { return 0 };
    match unsafe { step(context, 0, 0) } {
        0 => {}
        THREW => return 3,
        _ => return 0,
    }
    if unsafe { step(context, 1, 0) } == THREW {
        return 3;
    }
    match unsafe { step(context, 2, 0) } {
        1 => 1,
        THREW => 3,
        _ => 2,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    struct Calls {
        actions: Vec<(u32, usize)>,
        answers: [usize; 3],
    }
    unsafe extern "C" fn step(ctx: *mut c_void, action: u32, index: usize) -> usize {
        let ctx = unsafe { &mut *ctx.cast::<Calls>() };
        ctx.actions.push((action, index));
        ctx.answers[action as usize]
    }
    #[test]
    fn audio_setup_failure_and_no_samples_stop_before_convert() {
        let mut c = Calls {
            actions: vec![],
            answers: [0, 2, 1],
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_audio_frame(Some(step), (&raw mut c).cast(), 1, 1) },
            0
        );
        assert_eq!(c.actions, [(0, 0)]);
        c.actions.clear();
        c.answers = [1, 1, 0];
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_audio_frame(Some(step), (&raw mut c).cast(), 1, 1) },
            1
        );
        assert_eq!(c.actions, [(0, 0), (1, 0)]);
    }
    #[test]
    fn present_skip_prevents_timing_and_wait() {
        let mut c = Calls {
            actions: vec![],
            answers: [1, 0, 1],
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_present_admission(Some(step), (&raw mut c).cast()) },
            0
        );
        assert_eq!(c.actions, [(0, 0)]);
        c.actions.clear();
        c.answers[0] = 0;
        c.answers[2] = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_present_admission(Some(step), (&raw mut c).cast()) },
            2
        );
        assert_eq!(c.actions, [(0, 0), (1, 0), (2, 0)]);
    }
    #[test]
    fn audio_exception_and_conversion_failure_do_not_run_later_steps() {
        let mut c = Calls {
            actions: vec![],
            answers: [THREW, 2, 1],
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_audio_frame(Some(step), (&raw mut c).cast(), 1, 1) },
            3
        );
        assert_eq!(c.actions, [(0, 0)]);
        c.actions.clear();
        c.answers = [1, 2, 0];
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_audio_frame(Some(step), (&raw mut c).cast(), 1, 0) },
            0
        );
        assert_eq!(c.actions, [(1, 0), (2, 0)]);
        c.actions.clear();
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_audio_frame(Some(step), (&raw mut c).cast(), 0, 1) },
            0
        );
        assert!(c.actions.is_empty());
    }
    struct WaitCalls {
        actions: Vec<(u32, usize)>,
        polls: usize,
    }
    unsafe extern "C" fn wait_step(ctx: *mut c_void, action: u32, index: usize) -> usize {
        let ctx = unsafe { &mut *ctx.cast::<WaitCalls>() };
        ctx.actions.push((action, index));
        if action == 0 {
            ctx.polls += 1;
            return 1;
        }
        if action == 1 {
            return if ctx.polls == 1 { 3 } else { 1 };
        }
        0
    }
    #[test]
    fn wait_polls_before_clock_and_sleeps_only_while_early() {
        let mut c = WaitCalls {
            actions: vec![],
            polls: 0,
        };
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_wait(Some(wait_step), (&raw mut c).cast(), 0) },
            1
        );
        assert_eq!(c.actions, [(0, 0), (1, 0), (2, 1), (0, 0), (1, 0)]);
        c.actions.clear();
        c.polls = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage10_video_wait(Some(wait_step), (&raw mut c).cast(), 1) },
            1
        );
        assert_eq!(c.actions, [(0, 0)]);
    }
}
