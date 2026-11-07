//! A1 officer-upgrade class, race and Build-button visibility workflows.
//! Native callbacks own raw game layouts, CRT comparisons, locks and ABI calls.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u64;
fn op(ctx: *mut c_void, step: Step, id: u32, arg: usize) -> Option<u64> {
    let value = unsafe { step(ctx, id, arg) };
    (value != u64::MAX).then_some(value)
}
macro_rules! value {
    ($ctx:expr,$step:expr,$id:expr,$arg:expr) => {
        match op($ctx, $step, $id, $arg) {
            Some(v) => v,
            None => return 0,
        }
    };
}

/// Returns native bool; kind 6 returns the original Build-button visibility.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_officer(kind: u32, ctx: *mut c_void, step: Step) -> u8 {
    match kind {
        // is_officer_upgrade_class: vtable fast path, then classlabel fallback.
        0 => {
            if value!(ctx, step, 0, 0) == 0
                || value!(ctx, step, 1, 0) == 0
                || value!(ctx, step, 2, 0) == 0
            {
                return 0;
            }
            if value!(ctx, step, 3, 0) != 0 {
                return 1;
            }
            if value!(ctx, step, 4, 0) == 0 {
                return 0;
            }
            u8::from(value!(ctx, step, 5, 0) != 0)
        }
        // is_starbase_label: native CRT comparisons retain classification.
        1 => {
            if value!(ctx, step, 0, 0) == 0 || value!(ctx, step, 1, 0) == 0 {
                return 0;
            }
            for id in 2..=4 {
                if value!(ctx, step, id, 0) != 0 {
                    return 1;
                }
            }
            0
        }
        // producer_class_has_officer_upgrade_build_item.
        2 => {
            if value!(ctx, step, 0, 0) == 0
                || value!(ctx, step, 1, 0) == 0
                || value!(ctx, step, 2, 0) == 0
            {
                return 0;
            }
            let count = value!(ctx, step, 3, 0) as usize;
            for index in 0..count {
                value!(ctx, step, 4, index);
                if value!(ctx, step, 5, 0) != 0
                    && value!(ctx, step, 6, 0) != 0
                    && value!(ctx, step, 7, 0) != 0
                {
                    return 1;
                }
            }
            0
        }
        // infer_starbase_race_from_object_name. Candidate order is significant.
        3 => {
            if value!(ctx, step, 0, 0) == 0 || value!(ctx, step, 1, 0) == 0 {
                return 0;
            }
            value!(ctx, step, 2, 0);
            for index in 0..13 {
                if value!(ctx, step, 3, index) != 0 {
                    value!(ctx, step, 4, index);
                    return 1;
                }
            }
            if value!(ctx, step, 5, 0) != 0 || value!(ctx, step, 6, 0) != 0 {
                value!(ctx, step, 7, 0);
                return 1;
            }
            0
        }
        // officer_upgrade_matches_race.
        4 => {
            if value!(ctx, step, 0, 0) == 0
                || value!(ctx, step, 1, 0) == 0
                || value!(ctx, step, 2, 0) == 0
            {
                return 0;
            }
            value!(ctx, step, 3, 0);
            if value!(ctx, step, 4, 0) == 0 || value!(ctx, step, 5, 0) == 0 {
                return 0;
            }
            u8::from(value!(ctx, step, 6, 0) != 0)
        }
        // officer_upgrade_matches_registered_starbase, preserving the native
        // class check inside the lock and its short-circuit on empty race.
        5 => {
            if value!(ctx, step, 0, 0) == 0
                || value!(ctx, step, 1, 0) == 0
                || value!(ctx, step, 2, 0) == 0
            {
                return 0;
            }
            value!(ctx, step, 3, 0);
            if value!(ctx, step, 4, 0) == 0 {
                return 0;
            }
            if value!(ctx, step, 5, 0) == 0 && value!(ctx, step, 6, 0) != 0 {
                return 0;
            }
            if value!(ctx, step, 7, 0) == 0 || value!(ctx, step, 8, 0) == 0 {
                return 0;
            }
            if value!(ctx, step, 9, 0) == 0 {
                return 0;
            }
            u8::from(value!(ctx, step, 10, 0) != 0)
        }
        // regparm Build-button hook; native callback retains original call and
        // rerereads the slot only after that call, as the frozen code does.
        6 => {
            if value!(ctx, step, 0, 0) == 0 || value!(ctx, step, 1, 0) == 0 {
                return 1;
            }
            if value!(ctx, step, 2, 0) == 0 || value!(ctx, step, 3, 0) == 0 {
                return 0;
            }
            let visible = value!(ctx, step, 4, 0) != 0;
            if value!(ctx, step, 5, 0) != 0 {
                return u8::from(visible);
            }
            if value!(ctx, step, 6, 0) != 0 {
                value!(ctx, step, 7, 0);
            }
            let mut officer = value!(ctx, step, 8, 0) != 0;
            if !officer && value!(ctx, step, 9, 0) != 0 {
                officer = value!(ctx, step, 10, 0) != 0;
            }
            let mut race_match = !officer;
            if officer {
                if value!(ctx, step, 11, 0) != 0 {
                    race_match = value!(ctx, step, 12, 0) != 0;
                } else if value!(ctx, step, 13, 0) != 0 {
                    value!(ctx, step, 14, 0);
                    if value!(ctx, step, 15, 0) != 0 {
                        if value!(ctx, step, 16, 0) != 0 {
                            race_match = false;
                            if value!(ctx, step, 17, 0) != 0 {
                                value!(ctx, step, 18, 0);
                                race_match = value!(ctx, step, 19, 0) != 0;
                                value!(ctx, step, 20, 0);
                            }
                        } else {
                            race_match = true;
                        }
                    } else {
                        race_match = true;
                    }
                }
            }
            u8::from(visible && race_match)
        }
        // producer_class_has_build_items.
        7 => {
            if value!(ctx, step, 0, 0) == 0
                || value!(ctx, step, 1, 0) == 0
                || value!(ctx, step, 2, 0) == 0
            {
                return 0;
            }
            let count = value!(ctx, step, 3, 0) as usize;
            for index in 0..count {
                value!(ctx, step, 4, index);
                if value!(ctx, step, 5, 0) != 0 && value!(ctx, step, 6, 0) != 0 {
                    return 1;
                }
            }
            0
        }
        _ => 0,
    }
}
