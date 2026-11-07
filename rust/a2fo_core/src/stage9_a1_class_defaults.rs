use core::ffi::c_void;

pub type Probe = unsafe extern "C" fn(*mut c_void, u32) -> u8;
pub type MissingStep = unsafe extern "C" fn(*mut c_void, u32, u32) -> u8;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Capability {
    command: *const u8,
    bit: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Missing {
    mask: u32,
    admitted: u32,
    failed: u8,
}

// The callback owns CRT comparison and the existing numbered-family parser.
// Class loading checks mdmoon first; resource lookup checks mmooninf first.
/// # Safety
/// `context` and `probe` must remain valid for synchronous native probes;
/// the callback must obey its operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_class_family(context: *mut c_void, probe: Probe) -> u8 {
    let mdmoon = unsafe { probe(context, 0) };
    if mdmoon != 0 {
        return mdmoon;
    }
    unsafe { probe(context, 1) }
}

/// # Safety
/// `context` and `probe` must remain valid for synchronous native probes;
/// the callback must obey its operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_resource_family(
    context: *mut c_void,
    probe: Probe,
) -> u8 {
    let inf = unsafe { probe(context, 1) };
    if inf == 2 {
        return 3;
    }
    if inf == 1 {
        return 2;
    }
    let dilithium = unsafe { probe(context, 0) };
    if dilithium == 2 {
        3
    } else if dilithium == 1 {
        1
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_class_groups(scout: u8, station: u8, moon: u8) -> u32 {
    u32::from(scout != 0) | (u32::from(station != 0) << 1) | (u32::from(moon != 0) << 2)
}

/// # Safety
/// If `count` is at most 32, `capabilities` must cover `count` readable
/// entries. `output` must be writable and separate from callback-owned
/// storage and the capabilities span. `context` and `step` must remain valid
/// for synchronous calls; the callback must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_missing(
    capabilities: *const Capability,
    count: u32,
    context: *mut c_void,
    step: MissingStep,
    output: *mut Missing,
) {
    let Some(output) = (unsafe { output.as_mut() }) else {
        return;
    };
    if capabilities.is_null() || count > 32 {
        *output = Missing {
            failed: 1,
            ..Missing::default()
        };
        return;
    }
    let mut result = Missing::default();
    for index in 0..count {
        let present = unsafe { step(context, 0, index) };
        if present == 2 {
            result.failed = 1;
            *output = result;
            return;
        }
        if present == 0 {
            result.mask |= unsafe { (*capabilities.add(index as usize)).bit };
            result.admitted |= 1 << index;
            if unsafe { step(context, 1, index) } == 2 {
                result.failed = 1;
                *output = result;
                return;
            }
        }
    }
    *output = result;
}

// 1 = detailed report, 2 = one suppression report, 0 = silent.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_report(incident: i32, cap: i32) -> u8 {
    if incident <= cap {
        1
    } else if incident == cap + 1 {
        2
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    struct State {
        seen: [u32; 8],
        len: usize,
        present: u32,
        fail: u32,
    }
    unsafe extern "C" fn probe(context: *mut c_void, index: u32) -> u8 {
        let state = unsafe { &mut *(context as *mut State) };
        state.seen[state.len] = index;
        state.len += 1;
        if index == state.fail {
            2
        } else {
            u8::from(state.present & (1 << index) != 0)
        }
    }
    unsafe extern "C" fn missing_step(context: *mut c_void, operation: u32, index: u32) -> u8 {
        let state = unsafe { &mut *(context as *mut State) };
        let recorded = if operation == 0 { index } else { 100 + index };
        state.seen[state.len] = recorded;
        state.len += 1;
        if recorded == state.fail {
            2
        } else if operation == 1 {
            1
        } else {
            u8::from(state.present & (1 << index) != 0)
        }
    }
    #[test]
    fn family_order_and_short_circuit() {
        let mut state = State {
            seen: [99; 8],
            len: 0,
            present: 2,
            fail: 99,
        };
        let context = (&mut state as *mut State).cast();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_class_family(context, probe) }, 1);
        assert_eq!(&state.seen[..state.len], &[0, 1]);
        state.len = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_family(context, probe) },
            2
        );
        assert_eq!(&state.seen[..state.len], &[1]);
        state.len = 0;
        state.present = 3;
        assert_eq!(unsafe { a2fo_rs_stage9_a1_class_family(context, probe) }, 1);
        assert_eq!(&state.seen[..state.len], &[0]);
        state.len = 0;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_family(context, probe) },
            2
        );
        assert_eq!(&state.seen[..state.len], &[1]);
        state.len = 0;
        state.fail = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_family(context, probe) },
            3
        );
        assert_eq!(&state.seen[..state.len], &[1]);
    }
    #[test]
    fn missing_keeps_order_and_accepts_explicit_zero_as_present() {
        let mut state = State {
            seen: [99; 8],
            len: 0,
            present: 0b1010,
            fail: 99,
        };
        let capabilities = [1, 8, 0x400, 0x1000].map(|bit| Capability {
            command: core::ptr::null(),
            bit,
        });
        let mut result = Missing::default();
        unsafe {
            a2fo_rs_stage9_a1_missing(
                capabilities.as_ptr(),
                4,
                (&mut state as *mut State).cast(),
                missing_step,
                &mut result,
            )
        };
        assert_eq!(
            result,
            Missing {
                mask: 0x401,
                admitted: 0b0101,
                failed: 0
            }
        );
        assert_eq!(&state.seen[..state.len], &[0, 100, 1, 2, 102, 3]);
        state.len = 0;
        state.fail = 2;
        unsafe {
            a2fo_rs_stage9_a1_missing(
                capabilities.as_ptr(),
                4,
                (&mut state as *mut State).cast(),
                missing_step,
                &mut result,
            )
        };
        assert_eq!(result.failed, 1);
        assert_eq!(&state.seen[..state.len], &[0, 100, 1, 2]);
        state.len = 0;
        state.fail = 100;
        unsafe {
            a2fo_rs_stage9_a1_missing(
                capabilities.as_ptr(),
                4,
                (&mut state as *mut State).cast(),
                missing_step,
                &mut result,
            )
        };
        assert_eq!(result.failed, 1);
        assert_eq!(&state.seen[..state.len], &[0, 100]);
        let mut invalid = Missing::default();
        unsafe {
            a2fo_rs_stage9_a1_missing(
                core::ptr::null(),
                4,
                (&mut state as *mut State).cast(),
                missing_step,
                &mut invalid,
            )
        };
        assert_eq!(invalid.failed, 1);
    }
    #[test]
    fn groups_and_caps() {
        assert_eq!(a2fo_rs_stage9_a1_class_groups(1, 1, 1), 7);
        assert_eq!(a2fo_rs_stage9_a1_report(32, 32), 1);
        assert_eq!(a2fo_rs_stage9_a1_report(33, 32), 2);
        assert_eq!(a2fo_rs_stage9_a1_report(34, 32), 0);
        assert_eq!(a2fo_rs_stage9_a1_report(-1, 32), 1);
    }
}
