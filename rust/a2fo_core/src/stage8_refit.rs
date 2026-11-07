//! Refit runtime control flow. Native storage, engine calls and ABI-exact
//! coordinate kernels remain behind synchronous callbacks, never Rust references.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;
unsafe fn call(step: Step, ctx: *mut c_void, op: u32) -> u64 {
    unsafe { step(ctx, op, 0, 0) }
}
/// Safety: each flow borrows its matching native context and callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_refit_flow(kind: u32, step: Step, ctx: *mut c_void) -> u64 {
    unsafe {
        match kind {
            0 => {
                if call(step, ctx, 0) == 0
                    || call(step, ctx, 1) == 0
                    || call(step, ctx, 2) != 0
                    || call(step, ctx, 3) == 0
                {
                    return 0;
                }
                if call(step, ctx, 4) == 0 || call(step, ctx, 5) != 0 || call(step, ctx, 6) == 0 {
                    return 0;
                }
                if call(step, ctx, 7) == 0 {
                    return 0;
                }
                call(step, ctx, 8);
                call(step, ctx, 9);
                call(step, ctx, 10);
                1
            }
            1 => {
                if call(step, ctx, 0) == 0
                    || call(step, ctx, 1) == 0
                    || call(step, ctx, 2) != 0
                    || call(step, ctx, 3) == 0
                {
                    return 0;
                }
                let refitting = call(step, ctx, 4) != 0;
                let yard = call(step, ctx, 5) != 0;
                let (mut count, mut index) = (0usize, 0usize);
                while (index as u64) < call(step, ctx, 6) {
                    if step(ctx, 7, index, 0) != 0 {
                        if step(ctx, 8, count, 0) != 0 {
                            step(ctx, 9, count, usize::from(!refitting && yard));
                        }
                        count += 1;
                    }
                    index += 1;
                }
                count as u64
            }
            2 => {
                if call(step, ctx, 0) == 0
                    || call(step, ctx, 1) == 0
                    || call(step, ctx, 2) == 0
                    || call(step, ctx, 3) != 0
                    || call(step, ctx, 4) == 0
                    || call(step, ctx, 5) != 0
                    || call(step, ctx, 6) == 0
                {
                    return 0;
                }
                call(step, ctx, 7);
                1
            }
            3 => {
                if call(step, ctx, 0) == 0
                    || call(step, ctx, 1) == 0
                    || call(step, ctx, 2) != 0
                    || call(step, ctx, 3) == 0
                {
                    return 0;
                }
                call(step, ctx, 4);
                1
            }
            4 => {
                if call(step, ctx, 0) == 0 {
                    return 0;
                }
                call(step, ctx, 1);
                while call(step, ctx, 2) != 0 {
                    if call(step, ctx, 3) != 0 && call(step, ctx, 4) != 0 {
                        return 1;
                    }
                    call(step, ctx, 5);
                }
                0
            }
            5 => {
                call(step, ctx, 0);
                call(step, ctx, 1);
                while call(step, ctx, 2) != 0 {
                    call(step, ctx, 3);
                    call(step, ctx, 4);
                }
                call(step, ctx, 5);
                0
            }
            6 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 1) == 0 || call(step, ctx, 2) == 0 {
                    return 0;
                }
                call(step, ctx, 3);
                if call(step, ctx, 4) == 2 {
                    call(step, ctx, 5);
                    if call(step, ctx, 6) != 0 {
                        call(step, ctx, 7);
                    }
                    return 0;
                }
                if call(step, ctx, 4) != 1 || call(step, ctx, 8) != 0 {
                    return 0;
                }
                if call(step, ctx, 9) != 0 {
                    call(step, ctx, 10);
                }
                0
            }
            7 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 1) == 0 || call(step, ctx, 2) != 0 {
                    return 0;
                }
                if call(step, ctx, 3) != 0 {
                    call(step, ctx, 4);
                }
                1
            }
            8 => {
                if call(step, ctx, 0) != 0 && call(step, ctx, 1) == 3 && call(step, ctx, 2) != 0 {
                    call(step, ctx, 3);
                }
                call(step, ctx, 4);
                0
            }
            9 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 1) == 0 {
                    return 0;
                }
                if call(step, ctx, 2) == u64::MAX {
                    call(step, ctx, 12);
                    return 0;
                }
                if call(step, ctx, 3) != 0 {
                    let count = call(step, ctx, 4).min(16) as usize;
                    step(ctx, 5, count, 0);
                    for index in 0..count {
                        if step(ctx, 6, index, 0) == u64::MAX {
                            call(step, ctx, 12);
                            return 0;
                        }
                    }
                    if call(step, ctx, 7) == u64::MAX {
                        call(step, ctx, 12);
                        return 0;
                    }
                }
                if call(step, ctx, 8) != 0 {
                    if call(step, ctx, 9) == u64::MAX {
                        call(step, ctx, 12);
                        return 0;
                    }
                } else if call(step, ctx, 10) != 0 {
                    call(step, ctx, 11);
                }
                0
            }
            10 => {
                if call(step, ctx, 0) == u64::MAX || call(step, ctx, 1) == u64::MAX {
                    return 0;
                }
                if call(step, ctx, 2) == 0 || call(step, ctx, 3) == 0 {
                    call(step, ctx, 11);
                    return 0;
                }
                let hook = call(step, ctx, 4);
                if hook == u64::MAX {
                    return 0;
                }
                if hook == 0 {
                    call(step, ctx, 12);
                    return 0;
                }
                if call(step, ctx, 5) == 0 {
                    call(step, ctx, 13);
                    return 0;
                }
                let classes = call(step, ctx, 6);
                if classes == u64::MAX {
                    return 0;
                }
                let craft = call(step, ctx, 7);
                if craft == u64::MAX {
                    return 0;
                }
                if classes == 0 || craft == 0 {
                    call(step, ctx, 14);
                    return 1;
                }
                if call(step, ctx, 8) == 0 {
                    call(step, ctx, 15);
                    return 1;
                }
                call(step, ctx, 9);
                call(step, ctx, 10);
                1
            }
            11 => {
                if call(step, ctx, 0) == 0 {
                    return 0;
                }
                let phase = call(step, ctx, 1);
                if phase == 4 {
                    return 1;
                }
                if phase != 3 || call(step, ctx, 2) == 0 {
                    return 0;
                }
                call(step, ctx, 3);
                call(step, ctx, 4);
                if call(step, ctx, 5) == 0
                    || call(step, ctx, 6) != 0
                    || call(step, ctx, 7) == 0
                    || call(step, ctx, 8) != 0
                    || call(step, ctx, 9) == 0
                {
                    return 0;
                }
                call(step, ctx, 10);
                call(step, ctx, 11);
                if call(step, ctx, 12) == 0 || call(step, ctx, 13) != call(step, ctx, 14) {
                    return 0;
                }
                call(step, ctx, 15);
                call(step, ctx, 16);
                call(step, ctx, 17);
                1
            }
            12 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 1) == 0 {
                    return 0;
                }
                if call(step, ctx, 2) != 0 {
                    return call(step, ctx, 4);
                }
                call(step, ctx, 3);
                if call(step, ctx, 4) == 0 {
                    return 0;
                }
                call(step, ctx, 5);
                1
            }
            13 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 2) == 0 {
                    return 1;
                }
                if call(step, ctx, 1) == 0 || call(step, ctx, 6) == 0 || call(step, ctx, 7) == 0 {
                    return 0;
                }
                call(step, ctx, 8);
                1
            }
            14 => {
                call(step, ctx, 0);
                call(step, ctx, 1);
                if call(step, ctx, 2) == 0 || call(step, ctx, 3) == 0 || call(step, ctx, 4) == 0 {
                    call(step, ctx, 9);
                    return 0;
                }
                if call(step, ctx, 5) == 0 || call(step, ctx, 6) == 0 {
                    call(step, ctx, 10);
                    return 0;
                }
                if call(step, ctx, 7) == 0 {
                    return 0;
                }
                call(step, ctx, 8)
            }
            15 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 1) == 0 {
                    return 0;
                }
                let mut index = 0;
                while (index as u64) < call(step, ctx, 2) {
                    if step(ctx, 3, index, 0) != 0 {
                        return 1;
                    }
                    index += 1;
                }
                0
            }
            16 => {
                if call(step, ctx, 0) == 0 {
                    return 0;
                }
                if call(step, ctx, 1) != 0 {
                    return call(step, ctx, 2);
                }
                call(step, ctx, 3);
                if call(step, ctx, 4) != 0 {
                    call(step, ctx, 5);
                }
                if call(step, ctx, 2) == 0 {
                    call(step, ctx, 6);
                }
                call(step, ctx, 2)
            }
            17 => u64::from(
                call(step, ctx, 0) == 0
                    || call(step, ctx, 1) != 0
                    || (call(step, ctx, 2) != 0 && call(step, ctx, 3) != 0),
            ),
            18 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 1) == 0 {
                    return 0;
                }
                call(step, ctx, 2);
                call(step, ctx, 3);
                0
            }
            19 => {
                call(step, ctx, 0);
                while call(step, ctx, 1) != 0 {
                    if call(step, ctx, 2) != 0 {
                        call(step, ctx, 3);
                        call(step, ctx, 4);
                    } else {
                        call(step, ctx, 5);
                    }
                }
                0
            }
            20 => {
                if call(step, ctx, 0) != call(step, ctx, 1) || call(step, ctx, 2) == 0 {
                    return 0;
                }
                if call(step, ctx, 3) == 0 {
                    return 0;
                }
                call(step, ctx, 4);
                call(step, ctx, 5);
                if call(step, ctx, 6) == 0
                    || call(step, ctx, 7) != 0
                    || (call(step, ctx, 8) != 0 && call(step, ctx, 9) != 0)
                {
                    call(step, ctx, 10);
                }
                0
            }
            21 => {
                if call(step, ctx, 0) == 0 || call(step, ctx, 1) == 0 {
                    return 0;
                }
                call(step, ctx, 2);
                call(step, ctx, 3);
                if call(step, ctx, 4) != 0 && call(step, ctx, 5) != 0 {
                    call(step, ctx, 6);
                }
                call(step, ctx, 7)
            }
            22 => {
                call(step, ctx, 0);
                while call(step, ctx, 1) != 0 {
                    if call(step, ctx, 2) == call(step, ctx, 3)
                        && call(step, ctx, 4) == call(step, ctx, 5)
                        && call(step, ctx, 6) == call(step, ctx, 7)
                    {
                        call(step, ctx, 8);
                        return 1;
                    }
                    call(step, ctx, 9);
                }
                0
            }
            _ => 0,
        }
    }
}
/// Safety: callbacks borrow live native context for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_refit_update(step: Step, ctx: *mut c_void) {
    unsafe {
        if call(step, ctx, 0) == 0 {
            return;
        }
        if call(step, ctx, 1) == 0
            || call(step, ctx, 2) != 0
            || call(step, ctx, 3) != call(step, ctx, 4)
        {
            call(step, ctx, 5);
            call(step, ctx, 6);
            return;
        }
        if call(step, ctx, 7) == 4 {
            let restored = call(step, ctx, 5) != 0;
            let stage = call(step, ctx, 8);
            if restored && stage == 0 {
                call(step, ctx, 6);
                if call(step, ctx, 48) == 0 {
                    call(step, ctx, 49);
                }
            }
            return;
        }
        if call(step, ctx, 9) == 0 || call(step, ctx, 10) == 0 {
            return;
        }
        call(step, ctx, 11);
        if call(step, ctx, 7) == 0 {
            call(step, ctx, 12);
            if call(step, ctx, 13) == 0 {
                call(step, ctx, 14);
            }
            if call(step, ctx, 15) != 0 {
                call(step, ctx, 16);
            }
            if call(step, ctx, 17) != 0 {
                call(step, ctx, 18);
                call(step, ctx, 19);
                call(step, ctx, 20);
                if call(step, ctx, 21) == 0 {
                    call(step, ctx, 22);
                }
            } else if call(step, ctx, 23) != 0 {
                call(step, ctx, 24);
                if call(step, ctx, 25) == 0 {
                    call(step, ctx, 26);
                }
                call(step, ctx, 16);
                call(step, ctx, 27);
            }
            return;
        }
        if call(step, ctx, 7) == 1 {
            if call(step, ctx, 28) != 0 {
                return;
            }
            call(step, ctx, 29);
            let queue = call(step, ctx, 30);
            if queue == 0 {
                return;
            }
            step(ctx, 31, queue as usize, 0);
            call(step, ctx, 32);
            if call(step, ctx, 33) == 0 {
                call(step, ctx, 34);
            }
            return;
        }
        let current_id = call(step, ctx, 35);
        let current_class = call(step, ctx, 36);
        let active = current_id == call(step, ctx, 37) && current_class == call(step, ctx, 38);
        if active {
            let entering = call(step, ctx, 7) != 3;
            call(step, ctx, 40);
            if entering {
                call(step, ctx, 18);
                call(step, ctx, 41);
                call(step, ctx, 42);
            }
            call(step, ctx, 39);
            call(step, ctx, 43);
            call(step, ctx, 44);
            call(step, ctx, 45);
            if call(step, ctx, 46) == 0 {
                call(step, ctx, 47);
            }
        } else if call(step, ctx, 23) != 0 {
            call(step, ctx, 24);
            call(step, ctx, 27);
        }
    }
}
/// Safety: native iterator/storage access stays inside the callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_refit_cancel(
    eject: u8,
    step: Step,
    ctx: *mut c_void,
) -> u8 {
    unsafe {
        if call(step, ctx, 0) == 0 {
            return 0;
        }
        call(step, ctx, 1); // snapshot original yard/queue IDs before deletion
        if eject == 0 {
            call(step, ctx, 2);
            call(step, ctx, 3);
        }
        if call(step, ctx, 4) != 0 && call(step, ctx, 5) != 0 {
            call(step, ctx, 6);
        }
        if eject == 0 {
            return 1;
        }
        // Producer deletion may remove/reallocate a job: reacquire it now.
        if call(step, ctx, 0) == 0 {
            return 1;
        }
        if call(step, ctx, 7) != 0 {
            return 1;
        }
        call(step, ctx, 2);
        call(step, ctx, 3);
        1
    }
}
/// Safety: callbacks preserve native object and map lifetimes/read order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_refit_finished(step: Step, ctx: *mut c_void) {
    unsafe {
        if call(step, ctx, 0) == 0 {
            return;
        }
        call(step, ctx, 1);
        if call(step, ctx, 2) == 0 || call(step, ctx, 3) == 0 || call(step, ctx, 4) != 0 {
            if call(step, ctx, 2) != 0 && call(step, ctx, 4) == 0 {
                call(step, ctx, 5);
            }
            call(step, ctx, 6);
            call(step, ctx, 7);
            return;
        }
        call(step, ctx, 6);
        if call(step, ctx, 8) != 0 {
            call(step, ctx, 9);
        }
        call(step, ctx, 10);
        call(step, ctx, 11);
        if call(step, ctx, 12) == 0 {
            call(step, ctx, 13);
        }
    }
}
/// Safety: called for one currently valid native map iterator.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_refit_removed(
    producer_destroyed: u8,
    step: Step,
    ctx: *mut c_void,
) -> u8 {
    unsafe {
        let exact = call(step, ctx, 0) != 0 && call(step, ctx, 2) != 0 && call(step, ctx, 3) != 0;
        if producer_destroyed != 0 && call(step, ctx, 0) != 0 {
            call(step, ctx, 1);
            return 1;
        }
        if exact {
            if call(step, ctx, 4) != 0 {
                return 0;
            }
            call(step, ctx, 1);
            return 1;
        }
        0
    }
}
