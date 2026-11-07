//! Scalar workflow decisions for the radar and recovery observers. Native
//! memory reads, patching, counters, locks and ABI thunks stay with C++.
#![allow(dead_code)]
use core::slice;

#[repr(C)]
pub struct RecoveryConfig {
    enabled: u32,
    engine_context: u32,
    node_transform: u32,
    capture_first_match: u32,
    sample_every: u32,
    max_samples: u32,
    max_reports: u32,
}
#[repr(C)]
pub struct RadarSeen {
    object_class: *const core::ffi::c_void,
    pass: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_radar_api_valid(
    present: u8,
    version: u32,
    expected: u32,
    size: usize,
    minimum: usize,
    log: u8,
    image: u8,
    patch: u8,
    root: u8,
) -> u8 {
    u8::from(
        present != 0
            && version == expected
            && size >= minimum
            && log != 0
            && image != 0
            && patch != 0
            && root != 0,
    )
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_radar_mark_seen(
    entries: *mut RadarSeen,
    count: *mut u32,
    object_class: *const core::ffi::c_void,
    pass: i32,
) -> u8 {
    if entries.is_null() || count.is_null() {
        return 0;
    }
    let n = unsafe { *count } as usize;
    if n > 96 {
        return 0;
    }
    if unsafe { slice::from_raw_parts(entries, n) }
        .iter()
        .any(|entry| entry.object_class == object_class && entry.pass == pass)
    {
        return 0;
    }
    if n == 96 {
        return 0;
    }
    unsafe {
        entries.add(n).write(RadarSeen { object_class, pass });
        *count = (n + 1) as u32;
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_parse_number(
    text: *const u8,
    length: usize,
    minimum: u32,
    maximum: u32,
    out: *mut u32,
) -> u8 {
    if text.is_null() || out.is_null() || length == 0 {
        return 0;
    }
    let mut value = 0u32;
    for &digit in unsafe { slice::from_raw_parts(text, length) } {
        if !digit.is_ascii_digit() {
            return 0;
        }
        let Some(next) = value
            .checked_mul(10)
            .and_then(|v| v.checked_add((digit - b'0') as u32))
        else {
            return 0;
        };
        value = next;
    }
    if value < minimum || value > maximum {
        return 0;
    }
    unsafe {
        *out = value;
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_config_valid(config: *const RecoveryConfig) -> u8 {
    let Some(c) = (unsafe { config.as_ref() }) else {
        return 0;
    };
    u8::from(
        c.enabled <= 1
            && c.engine_context <= 1
            && c.node_transform <= 1
            && c.capture_first_match <= 1
            && (1..=1_000_000).contains(&c.sample_every)
            && (1..=1_000_000).contains(&c.max_samples)
            && c.max_reports <= 256,
    )
}
type Op = unsafe extern "C" fn(*mut core::ffi::c_void, u32) -> u64;
fn operation(context: *mut core::ffi::c_void, op: Op, id: u32) -> u64 {
    unsafe { op(context, id) }
}
// Operation numbers are documented at the native callback switches. Each
// operation is synchronous and catches C++ exceptions before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_radar_init(
    valid: u8,
    installed: u8,
    context: *mut core::ffi::c_void,
    op: Op,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    if installed != 0 {
        return 1;
    }
    match operation(context, op, 0) {
        0 => return 0,
        1 => {
            operation(context, op, 10);
            return 1;
        }
        _ => (),
    }
    if operation(context, op, 1) == 0 {
        operation(context, op, 11);
        return 0;
    }
    if operation(context, op, 2) == 0 || operation(context, op, 3) == 0 {
        return 0;
    }
    if unsafe { a2fo_rs_radar_install(context, op) } == 0 {
        operation(context, op, 12);
        return 0;
    }
    operation(context, op, 13);
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_radar_install(context: *mut core::ffi::c_void, op: Op) -> u8 {
    if operation(context, op, 4) == 0 || operation(context, op, 5) == 0 {
        return 0;
    }
    if operation(context, op, 6) != 0 {
        operation(context, op, 9);
        return 1;
    }
    if operation(context, op, 7) != 0 {
        return 0;
    }
    operation(context, op, 8);
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_radar_object(
    activation: u64,
    context: *mut core::ffi::c_void,
    op: Op,
) {
    if activation == 0 {
        operation(context, op, 1);
        return;
    }
    operation(context, op, 0); // enter TLS scope
    if operation(context, op, 1) == 0 {
        operation(context, op, 2);
        return;
    } // original
    operation(context, op, 2); // restore TLS scope
    if activation == operation(context, op, 3) {
        operation(context, op, 4);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_radar_clear(
    activation: u64,
    scope: u8,
    context: *mut core::ffi::c_void,
    op: Op,
) -> u32 {
    let sample = activation != 0 && scope != 0 && operation(context, op, 0) as i32 <= 24;
    if sample {
        operation(context, op, 1);
    } // viewport
    let result = operation(context, op, 2) as u32; // original Clear
    if activation != 0 && activation == operation(context, op, 3) && scope != 0 {
        operation(context, op, 4); // dots
    }
    if sample && activation == operation(context, op, 3) {
        operation(context, op, 5);
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_init(
    valid: u8,
    context: *mut core::ffi::c_void,
    op: Op,
) -> u8 {
    if valid == 0 {
        return 0;
    }
    for gate in 0..10 {
        if operation(context, op, gate) == 0 {
            operation(context, op, gate + 10);
            return 1;
        }
    }
    let mut ok = true;
    if operation(context, op, 20) != 0 {
        ok = operation(context, op, 22) != 0;
    }
    if operation(context, op, 21) != 0 {
        ok = operation(context, op, 23) != 0 && ok;
    }
    operation(context, op, if ok { 24 } else { 25 });
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_before(
    kind: u32,
    activation: u64,
    context: *mut core::ffi::c_void,
    op: Op,
) {
    if activation == 0 || kind > 1 || operation(context, op, 0) == 0 {
        return;
    }
    let visit = operation(context, op, 1) as u32;
    let attempted = operation(context, op, 2) as u32;
    let every = operation(context, op, 7) as u32;
    let max = operation(context, op, 8) as u32;
    if visit == 0 || every == 0 || !visit.is_multiple_of(every) || attempted >= max {
        return;
    }
    if operation(context, op, 3) == 0 {
        return;
    }
    if operation(context, op, 4) >= max as u64 {
        operation(context, op, 6);
        return;
    }
    operation(context, op, 5);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_after(
    admitted: u32,
    activation: u64,
    context: *mut core::ffi::c_void,
    op: Op,
) {
    if admitted == 0 {
        return;
    }
    if activation == 0 || activation != operation(context, op, 0) {
        operation(context, op, 1);
        return;
    }
    match operation(context, op, 2) {
        0 => {
            operation(context, op, 3);
        }
        1 => {
            let matched = operation(context, op, 4);
            if matched == 1 && operation(context, op, 11) != 0 && operation(context, op, 6) == 3 {
                operation(context, op, 3);
                operation(context, op, 12);
                operation(context, op, 1);
                return;
            }
        }
        2 => {
            if operation(context, op, 5) == 3 || operation(context, op, 6) == 3 {
                operation(context, op, 3);
                operation(context, op, 12);
                operation(context, op, 1);
                return;
            }
        }
        _ => {
            operation(context, op, 3);
            operation(context, op, 12);
            operation(context, op, 1);
            return;
        }
    }
    let attempted = operation(context, op, 9) as u32;
    if (attempted == 1
        || attempted.is_multiple_of(32)
        || attempted == operation(context, op, 10) as u32)
        && operation(context, op, 7) == 3
    {
        operation(context, op, 3);
        operation(context, op, 12);
    }
    operation(context, op, 1);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_radar_activation(
    capturing: u8,
    installed: u8,
    context: *mut core::ffi::c_void,
    op: Op,
) -> u64 {
    if capturing == 0 {
        return 0;
    }
    if installed == 0 {
        return operation(context, op, 1);
    }
    operation(context, op, 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_activation(
    closing: u8,
    context: *mut core::ffi::c_void,
    op: Op,
) -> u64 {
    if closing != 0 {
        0
    } else {
        operation(context, op, 0)
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_radar_shutdown(context: *mut core::ffi::c_void, op: Op) {
    operation(context, op, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_shutdown(context: *mut core::ffi::c_void, op: Op) {
    for id in 0..5 {
        operation(context, op, id);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_recovery_status(
    activation: u64,
    context: *mut core::ffi::c_void,
    op: Op,
) {
    if activation != 0 {
        operation(context, op, 0);
        operation(context, op, 1);
    }
}
