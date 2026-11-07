//! Core event workflow policies. SDK payload reads, native handlers and their
//! storage stay behind synchronous exception-containing native callbacks.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;
#[repr(C)]
pub struct ProducerRules {
    pub size: u32,
    pub maximum: u32,
    pub destroying: u32,
    pub admit: u32,
    pub finishing: u32,
    pub starting_effect: u32,
}
unsafe fn checked(step: Step, ctx: *mut c_void, op: u32, index: usize) -> Result<u64, ()> {
    let value = unsafe { step(ctx, op, index, 0) };
    if value == u64::MAX {
        Err(())
    } else {
        Ok(value)
    }
}
fn veto_kind(kind: u64, rules: &ProducerRules) -> bool {
    kind == u64::from(rules.admit)
        || kind == u64::from(rules.finishing)
        || kind == u64::from(rules.starting_effect)
}
unsafe fn dispatch_producer(
    rules: &ProducerRules,
    step: Step,
    ctx: *mut c_void,
) -> Result<u64, ()> {
    unsafe {
        if checked(step, ctx, 0, 0)? == 0
            || checked(step, ctx, 1, 0)? < u64::from(rules.size)
            || checked(step, ctx, 2, 0)? == 0
        {
            return Ok(0);
        }
        if checked(step, ctx, 3, 0)? > u64::from(rules.maximum)
            || (checked(step, ctx, 3, 0)? != u64::from(rules.destroying)
                && checked(step, ctx, 4, 0)? == 0)
            || checked(step, ctx, 5, 0)? == 0
        {
            return Ok(0);
        }
        let copy = (|| -> Result<(), ()> {
            checked(step, ctx, 6, 0)?;
            checked(step, ctx, 7, 0)?;
            checked(step, ctx, 8, 0)?;
            Ok(())
        })();
        if copy.is_err() {
            checked(step, ctx, 9, 0)?;
            return Ok(u64::from(!veto_kind(checked(step, ctx, 3, 0)?, rules)));
        }
        let count = checked(step, ctx, 10, 0)? as usize;
        for i in 0..count {
            if checked(step, ctx, 11, i)? == 0 {
                continue;
            }
            let accepted = checked(step, ctx, 12, i)? != 0;
            // Read only after the foreign handler has returned: it may mutate
            // the producer event or reenter registration/dispatch.
            if veto_kind(checked(step, ctx, 3, 0)?, rules) && !accepted {
                return Ok(0);
            }
        }
        Ok(1)
    }
}
/// Safety: rules is a valid immutable C POD. The synchronous native context
/// owns the snapshot, recursive lock and native exception. No SDK event layout
/// or native function pointer is interpreted by Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_dispatch_producer(
    rules: *const ProducerRules,
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    if rules.is_null() {
        return 2;
    }
    (unsafe { dispatch_producer(&*rules, step, ctx) }).unwrap_or(2)
}

unsafe fn register_handler(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        if checked(step, ctx, 0, 0)? == 0 || checked(step, ctx, 1, 0)? == 0 {
            return Ok(0);
        }
        checked(step, ctx, 2, 0)?;
        let admitted = checked(step, ctx, 3, 0)? != 0
            && checked(step, ctx, 4, 0)? != 0
            && checked(step, ctx, 5, 0)? < 64;
        if admitted {
            checked(step, ctx, 6, 0)?;
        }
        checked(step, ctx, 7, 0)?;
        checked(step, ctx, 8, usize::from(admitted))?;
        Ok(u64::from(admitted))
    }
}
/// Safety: native context owns the registration vector and recursive lock;
/// native construction failures are rethrown after the synchronous program.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_register_handler(step: Step, ctx: *mut c_void) -> u64 {
    (unsafe { register_handler(step, ctx) }).unwrap_or(2)
}

fn hash_byte(hash: u32, byte: u8) -> u32 {
    (hash ^ u32::from(byte)).wrapping_mul(16777619)
}
/// Safety: odf and transform expose the original native readable byte ranges.
/// Transform remains opaque bytes, preserving every native float bit pattern.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_object_hash(
    odf: *const u8,
    len: usize,
    handle: u32,
    team: i32,
    transform: *const u8,
    transform_len: usize,
) -> u32 {
    let mut hash = 2166136261;
    for i in 0..len {
        hash = hash_byte(hash, unsafe { odf.add(i).read() });
    }
    for byte in handle.to_ne_bytes() {
        hash = hash_byte(hash, byte);
    }
    for byte in team.to_ne_bytes() {
        hash = hash_byte(hash, byte);
    }
    for i in 0..transform_len {
        hash = hash_byte(hash, unsafe { transform.add(i).read() });
    }
    hash
}

#[repr(C)]
pub struct ReplacementRules {
    pub size: u32,
    pub known_flags: u32,
    pub inherit_position: u32,
    pub inherit_rotation: u32,
    pub maximum_owner: u32,
    pub original_owner: u32,
}
unsafe fn resolve_destroyed(
    rules: &ReplacementRules,
    step: Step,
    ctx: *mut c_void,
) -> Result<u64, ()> {
    unsafe {
        if checked(step, ctx, 0, 0)? == 0 {
            return Ok(0);
        }
        checked(step, ctx, 1, 0)?;
        let mut index = 0;
        while index < (checked(step, ctx, 2, 0)? as usize) {
            if checked(step, ctx, 3, index)? != 0 && checked(step, ctx, 4, 0)? != 0 {
                checked(step, ctx, 5, 0)?;
                if checked(step, ctx, 6, index)? == 1 {
                    // The original bounded native C-string scan precedes even
                    // the replacement struct-size check; keep that read order.
                    checked(step, ctx, 7, 0)?;
                    if checked(step, ctx, 8, 0)? < u64::from(rules.size)
                        || checked(step, ctx, 9, 0)? == 0
                        || checked(step, ctx, 10, 0)? == 0
                        || (checked(step, ctx, 11, 0)? & !u64::from(rules.known_flags)) != 0
                        || checked(step, ctx, 12, 0)? > u64::from(rules.maximum_owner)
                    {
                        checked(step, ctx, 13, 0)?;
                    } else {
                        checked(step, ctx, 14, 0)?;
                        let position =
                            checked(step, ctx, 11, 0)? & u64::from(rules.inherit_position) != 0;
                        checked(step, ctx, 16, usize::from(position))?;
                        let rotation =
                            checked(step, ctx, 11, 0)? & u64::from(rules.inherit_rotation) != 0;
                        checked(step, ctx, 17, usize::from(rotation))?;
                        let original =
                            checked(step, ctx, 12, 0)? == u64::from(rules.original_owner);
                        checked(step, ctx, 18, usize::from(original))?;
                        return Ok(1);
                    }
                }
            }
            index += 1;
        }
        Ok(0)
    }
}
/// Safety: rules is immutable C POD. Native context owns all SDK event/response
/// layouts, snapshot containers and callback exception regions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_resolve_destroyed(
    rules: *const ReplacementRules,
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    if rules.is_null() {
        return 2;
    }
    (unsafe { resolve_destroyed(&*rules, step, ctx) }).unwrap_or(2)
}

unsafe fn spawn_replacement(step: Step, ctx: *mut c_void) -> Result<u64, ()> {
    unsafe {
        if checked(step, ctx, 0, 0)? == 0 {
            checked(step, ctx, 1, 0)?;
            return Ok(0);
        }
        checked(step, ctx, 2, 0)?;
        if checked(step, ctx, 3, 0)? != 0 {
            checked(step, ctx, 4, 0)?;
        }
        if checked(step, ctx, 5, 0)? != 0 {
            checked(step, ctx, 6, 0)?;
        }
        let original = checked(step, ctx, 7, 0)? != 0;
        if checked(step, ctx, 8, usize::from(original))? == 0 {
            checked(step, ctx, 9, 0)?;
            return Ok(0);
        }
        if checked(step, ctx, 10, 0)? == 0 {
            checked(step, ctx, 11, 0)?;
            return Ok(0);
        }
        if checked(step, ctx, 12, 0)? == 0 || checked(step, ctx, 13, 0)? == 0 {
            checked(step, ctx, 14, 0)?;
            return Ok(0);
        }
        checked(step, ctx, 15, 0)?;
        checked(step, ctx, 16, 0)?;
        Ok(0)
    }
}
/// Safety: native context retains every engine pointer, native method ABI and
/// transform byte copy. Foreign failures stay native and are rethrown after
/// this synchronous workflow returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_core_spawn_replacement(
    step: Step,
    ctx: *mut c_void,
) -> u64 {
    (unsafe { spawn_replacement(step, ctx) }).unwrap_or(2)
}
