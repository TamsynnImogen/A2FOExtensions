use core::ffi::c_void;

pub type BoolRead = unsafe extern "C" fn(*mut c_void) -> u8;
pub type PointerRead = unsafe extern "C" fn(*mut c_void) -> usize;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct RegistrationDecision {
    pub constructor_like: u8,
    pub admit: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CursorSlotDecision {
    pub capture_native: u8,
    pub replace_cache: u8,
}

/// # Safety
/// `output` is writable and disjoint from `context`; `classify` is a valid
/// synchronous, nonthrowing native callback. It reads the live object class.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_state_registration(
    has_classlabel: u8,
    has_officer_items: u8,
    has_maximum: u8,
    has_gain: u8,
    context: *mut c_void,
    classify: BoolRead,
    output: *mut RegistrationDecision,
) {
    let constructor_like =
        has_classlabel != 0 || has_officer_items != 0 || unsafe { classify(context) } != 0;
    let admit = constructor_like || has_maximum != 0 || has_gain != 0;
    unsafe {
        output.write(RegistrationDecision {
            constructor_like: u8::from(constructor_like),
            admit: u8::from(admit),
        });
    }
}

/// # Safety
/// `has_race` is a valid synchronous, nonthrowing native callback. It reads
/// race[0] at this expression, only if both earlier clues were false.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_state_has_policy(
    has_maximum: u8,
    has_gain: u8,
    context: *mut c_void,
    has_race: BoolRead,
) -> u8 {
    u8::from(has_maximum != 0 || has_gain != 0 || unsafe { has_race(context) } != 0)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_state_completed_admit(has_maximum: u8, has_gain: u8) -> u8 {
    u8::from(has_maximum != 0 || has_gain != 0)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_state_credit_transition(
    credited_team: usize,
    new_team: usize,
) -> u8 {
    u8::from(credited_team != new_team)
}

/// # Safety
/// `output` is writable and disjoint from `context`; `read_native` is a valid
/// synchronous, nonthrowing native callback. It reads this live cursor slot.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_state_cursor_slot(
    cached: usize,
    fallback: usize,
    context: *mut c_void,
    read_native: PointerRead,
    output: *mut CursorSlotDecision,
) {
    let differs = cached != fallback;
    let capture_native = cached != 0 && differs && unsafe { read_native(context) } == 0;
    unsafe {
        output.write(CursorSlotDecision {
            capture_native: u8::from(capture_native),
            replace_cache: u8::from(differs),
        });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_state_cursor_active(
    active_replaced: u8,
    native_cursor: usize,
    active_cursor: usize,
) -> u8 {
    u8::from(active_replaced == 0 && native_cursor != 0 && active_cursor == native_cursor)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_state_cursor_changed(
    active_replaced: u8,
    cache_replacements: u32,
) -> u8 {
    u8::from(active_replaced != 0 || cache_replacements != 0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Live {
        reads: u32,
        value: usize,
    }
    unsafe extern "C" fn read_bool(context: *mut c_void) -> u8 {
        let live = unsafe { &mut *context.cast::<Live>() };
        live.reads += 1;
        u8::from(live.value != 0)
    }
    unsafe extern "C" fn read_pointer(context: *mut c_void) -> usize {
        let live = unsafe { &mut *context.cast::<Live>() };
        live.reads += 1;
        live.value
    }

    #[test]
    fn registration_keeps_classifier_lazy_and_observes_mutation() {
        let mut live = Live::default();
        let context = (&mut live as *mut Live).cast();
        let mut result = RegistrationDecision::default();
        unsafe {
            a2fo_rs_stage9_a1_state_registration(1, 0, 0, 0, context, read_bool, &mut result)
        };
        assert_eq!(
            result,
            RegistrationDecision {
                constructor_like: 1,
                admit: 1
            }
        );
        assert_eq!(live.reads, 0);
        unsafe {
            a2fo_rs_stage9_a1_state_registration(0, 1, 0, 0, context, read_bool, &mut result)
        };
        assert_eq!(live.reads, 0);
        unsafe {
            a2fo_rs_stage9_a1_state_registration(0, 0, 1, 0, context, read_bool, &mut result)
        };
        assert_eq!(
            result,
            RegistrationDecision {
                constructor_like: 0,
                admit: 1
            }
        );
        assert_eq!(live.reads, 1);
        live.value = 7;
        unsafe {
            a2fo_rs_stage9_a1_state_registration(0, 0, 0, 0, context, read_bool, &mut result)
        };
        assert_eq!(
            result,
            RegistrationDecision {
                constructor_like: 1,
                admit: 1
            }
        );
        assert_eq!(live.reads, 2);
        live.value = 0;
        unsafe {
            a2fo_rs_stage9_a1_state_registration(0, 0, 0, 0, context, read_bool, &mut result)
        };
        assert_eq!(
            result,
            RegistrationDecision {
                constructor_like: 0,
                admit: 0
            }
        );
        assert_eq!(live.reads, 3);
    }

    #[test]
    fn policy_storage_race_read_is_lazy_and_rechecked_at_each_call() {
        let mut live = Live::default();
        let context = (&mut live as *mut Live).cast();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_state_has_policy(1, 0, context, read_bool) },
            1
        );
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_state_has_policy(0, 1, context, read_bool) },
            1
        );
        assert_eq!(live.reads, 0);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_state_has_policy(0, 0, context, read_bool) },
            0
        );
        live.value = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_state_has_policy(0, 0, context, read_bool) },
            1
        );
        assert_eq!(live.reads, 2);
    }

    #[test]
    fn completed_gate_and_credit_compare_use_live_values() {
        assert_eq!(a2fo_rs_stage9_a1_state_completed_admit(0, 0), 0);
        assert_eq!(a2fo_rs_stage9_a1_state_completed_admit(0, 1), 1);
        assert_eq!(a2fo_rs_stage9_a1_state_completed_admit(1, 0), 1);
        let mut credited = 0x1000usize;
        assert_eq!(
            a2fo_rs_stage9_a1_state_credit_transition(credited, 0x1000),
            0
        );
        credited = 0x2000;
        assert_eq!(
            a2fo_rs_stage9_a1_state_credit_transition(credited, 0x1000),
            1
        );
        assert_eq!(a2fo_rs_stage9_a1_state_credit_transition(0, 0), 0);
    }

    #[test]
    fn cursor_capture_reads_native_only_after_two_short_circuits() {
        let mut live = Live::default();
        let context = (&mut live as *mut Live).cast();
        let mut result = CursorSlotDecision::default();
        unsafe { a2fo_rs_stage9_a1_state_cursor_slot(0, 9, context, read_pointer, &mut result) };
        assert_eq!(
            result,
            CursorSlotDecision {
                capture_native: 0,
                replace_cache: 1
            }
        );
        assert_eq!(live.reads, 0);
        unsafe { a2fo_rs_stage9_a1_state_cursor_slot(9, 9, context, read_pointer, &mut result) };
        assert_eq!(
            result,
            CursorSlotDecision {
                capture_native: 0,
                replace_cache: 0
            }
        );
        assert_eq!(live.reads, 0);
        unsafe { a2fo_rs_stage9_a1_state_cursor_slot(5, 9, context, read_pointer, &mut result) };
        assert_eq!(
            result,
            CursorSlotDecision {
                capture_native: 1,
                replace_cache: 1
            }
        );
        live.value = 7;
        unsafe { a2fo_rs_stage9_a1_state_cursor_slot(5, 9, context, read_pointer, &mut result) };
        assert_eq!(
            result,
            CursorSlotDecision {
                capture_native: 0,
                replace_cache: 1
            }
        );
        assert_eq!(live.reads, 2);
    }

    #[test]
    fn cursor_active_and_log_gate_preserve_nonzero_semantics() {
        assert_eq!(a2fo_rs_stage9_a1_state_cursor_active(0, 0, 0), 0);
        assert_eq!(a2fo_rs_stage9_a1_state_cursor_active(0, 5, 5), 1);
        assert_eq!(a2fo_rs_stage9_a1_state_cursor_active(1, 5, 5), 0);
        assert_eq!(a2fo_rs_stage9_a1_state_cursor_active(0, 5, 6), 0);
        assert_eq!(a2fo_rs_stage9_a1_state_cursor_changed(0, 0), 0);
        assert_eq!(a2fo_rs_stage9_a1_state_cursor_changed(1, 0), 1);
        assert_eq!(a2fo_rs_stage9_a1_state_cursor_changed(0, 2), 1);
    }
}
