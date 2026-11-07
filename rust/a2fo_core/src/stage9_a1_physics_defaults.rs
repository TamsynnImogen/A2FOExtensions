use core::ffi::{c_char, c_void};

pub type Read = unsafe extern "C" fn(*mut c_void, u32) -> u8;
pub type Compare = unsafe extern "C" fn(*mut c_void, *const c_char) -> u8;

const NAMES: [&[u8]; 15] = [
    b"forwardAccel\0",
    b"backwardAccel\0",
    b"turnOmega\0",
    b"turnAlpha\0",
    b"pitchOmega\0",
    b"pitchAlpha\0",
    b"rollCoupling\0",
    b"turnOmegaFractionAtRest\0",
    b"turnAlphaFractionAtRest\0",
    b"pitchOmegaFractionAtRest\0",
    b"pitchAlphaFractionAtRest\0",
    b"pitchDefault\0",
    b"pitchDefaultSpeed\0",
    b"turnControlAngle\0",
    b"forwardControlDistance\0",
];

// Bit patterns of the original float literals. Columns are destroyer,
// construction, battle, destroyer combat, battle combat.
const DEFAULTS: [[u32; 5]; 15] = [
    [0x3f800000, 0x3f800000, 0x40400000, 0x3f000000, 0x3f000000],
    [0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000],
    [0x3f800000, 0x3f4ccccd, 0x3f800000, 0x3dcccccd, 0x3dcccccd],
    [0x3f800000, 0x3f4ccccd, 0x3f800000, 0x3f000000, 0x3f800000],
    [0x3f800000, 0x3f800000, 0x3f800000, 0x3dcccccd, 0x3dcccccd],
    [0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000],
    [0x40000000, 0x3f000000, 0x40400000, 0x40000000, 0x40400000],
    [0x3e4ccccd, 0x3f4ccccd, 0x00000000, 0x3e4ccccd, 0x00000000],
    [0x3e4ccccd, 0x3f4ccccd, 0x00000000, 0x3e4ccccd, 0x00000000],
    [0x3e4ccccd, 0x3f4ccccd, 0x00000000, 0x3e4ccccd, 0x00000000],
    [0x3e4ccccd, 0x3f4ccccd, 0x00000000, 0x3e4ccccd, 0x00000000],
    [0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000],
    [0xbf800000, 0x3f000000, 0x3f800000, 0xbf800000, 0x3f800000],
    [0x41a00000, 0x40a00000, 0x41f00000, 0x41a00000, 0x41f00000],
    [0x428c0000, 0x41200000, 0x42c80000, 0x428c0000, 0x42c80000],
];

/// # Safety
/// `context` and `read` must remain valid for synchronous native reads;
/// the callback must obey its operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_legacy_db(
    context: *mut c_void,
    read: Read,
) -> u8 {
    if unsafe { read(context, 0) } != 0 {
        1
    } else if unsafe { read(context, 1) } != 0 {
        2
    } else {
        0
    }
}

/// # Safety
/// `context` and `read` must remain valid for synchronous native reads;
/// the callback must obey its operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_declared(
    context: *mut c_void,
    read: Read,
) -> u8 {
    if unsafe { read(context, 0) } != 0 {
        1
    } else {
        u8::from(unsafe { read(context, 1) } != 0)
    }
}

/// # Safety
/// `context` and `read` must remain valid for both synchronous native reads;
/// the callback must obey its operation contract and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_declared_both(
    context: *mut c_void,
    read: Read,
) -> u8 {
    let primary = unsafe { read(context, 0) } != 0;
    let fallback = unsafe { read(context, 1) } != 0;
    u8::from(primary || fallback)
}

/// # Safety
/// `context` and `compare` must remain valid for synchronous comparisons;
/// the callback must accept the supplied NUL-terminated static names and
/// must not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_default_key(
    context: *mut c_void,
    compare: Compare,
) -> i32 {
    for (index, name) in NAMES.iter().enumerate() {
        if unsafe { compare(context, name.as_ptr().cast()) } != 0 {
            return index as i32;
        }
    }
    -1
}

/// # Safety
/// `bits` and `combat_controller` must point to separate writable outputs.
/// `legacy_db` and `fallback_db` are compared only by address.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_default_bits(
    index: i32,
    profile: u32,
    legacy_db: *const c_void,
    fallback_db: *const c_void,
    bits: *mut u32,
    combat_controller: *mut u8,
) {
    let Some(bits) = (unsafe { bits.as_mut() }) else {
        return;
    };
    let Some(combat_controller) = (unsafe { combat_controller.as_mut() }) else {
        return;
    };
    let Some(row) = usize::try_from(index).ok().and_then(|i| DEFAULTS.get(i)) else {
        return;
    };
    let combat = legacy_db == fallback_db;
    *combat_controller = u8::from(combat);
    let column = match profile {
        1 => 1, // construction
        2 => {
            if combat {
                4
            } else {
                2
            }
        } // battle
        _ => {
            if combat {
                3
            } else {
                0
            }
        } // destroyer
    };
    *bits = row[column];
}

/// # Safety
/// When `key` is nonnull, `context` and `compare` must remain valid for a
/// synchronous call. The callback must accept a static C string and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_turn_key(
    key: *const c_char,
    context: *mut c_void,
    compare: Compare,
) -> u8 {
    u8::from(!key.is_null() && unsafe { compare(context, c"turnControlSquared".as_ptr()) } != 0)
}

/// # Safety
/// When `key` is nonnull, `context` and `compare` must remain valid for a
/// synchronous call. The callback must accept a static C string and not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_forward_key(
    key: *const c_char,
    context: *mut c_void,
    compare: Compare,
) -> u8 {
    u8::from(!key.is_null() && unsafe { compare(context, c"forwardAccel".as_ptr()) } != 0)
}

/// # Safety
/// When the native guards admit comparison, `context` and `compare` must
/// remain valid for a synchronous call. The callback must accept a static C
/// string and not unwind; `key` itself is checked only for null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_physics_model_admit(
    found: usize,
    key: *const c_char,
    context: *mut c_void,
    compare: Compare,
) -> u8 {
    u8::from(
        (found & 0xff) == 0
            && !key.is_null()
            && unsafe { compare(context, c"physics".as_ptr()) } != 0,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_physics_model_name(
    borg_found: usize,
    borg_physics: i32,
) -> *const c_char {
    if (borg_found & 0xff) != 0 && borg_physics != 0 {
        c"borg".as_ptr()
    } else {
        c"smooth".as_ptr()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_physics_log_case(incident: i32) -> u8 {
    if incident <= 64 {
        1
    } else if incident == 65 {
        2
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate std;
    use self::std::vec::Vec;

    struct Trace {
        calls: Vec<u32>,
        primary: u8,
        fallback: u8,
    }
    unsafe extern "C" fn read(ctx: *mut c_void, op: u32) -> u8 {
        let t = unsafe { &mut *ctx.cast::<Trace>() };
        t.calls.push(op);
        if op == 0 { t.primary } else { t.fallback }
    }
    #[test]
    fn reads_short_circuit_primary_then_fallback() {
        let mut t = Trace {
            calls: Vec::new(),
            primary: 1,
            fallback: 1,
        };
        let p = &mut t as *mut Trace as *mut c_void;
        assert_eq!(unsafe { a2fo_rs_stage9_a1_physics_legacy_db(p, read) }, 1);
        assert_eq!(t.calls, [0]);
        t.calls.clear();
        t.primary = 0;
        assert_eq!(unsafe { a2fo_rs_stage9_a1_physics_legacy_db(p, read) }, 2);
        assert_eq!(t.calls, [0, 1]);
        t.calls.clear();
        t.primary = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_physics_declared_both(p, read) },
            1
        );
        assert_eq!(t.calls, [0, 1]);
        t.calls.clear();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_physics_declared(p, read) }, 1);
        assert_eq!(t.calls, [0]);
        t.calls.clear();
        t.primary = 0;
        assert_eq!(unsafe { a2fo_rs_stage9_a1_physics_declared(p, read) }, 1);
        assert_eq!(t.calls, [0, 1]);
    }
    unsafe extern "C" fn same(ctx: *mut c_void, candidate: *const c_char) -> u8 {
        let key = unsafe { &*(ctx as *const &[u8]) };
        let mut n = 0;
        while unsafe { *candidate.add(n) } != 0 {
            n += 1;
        }
        u8::from(unsafe { core::slice::from_raw_parts(candidate.cast::<u8>(), n) } == *key)
    }
    #[test]
    fn table_bits_profiles_and_pointer_identity() {
        let key: &[u8] = b"forwardAccel";
        let index = unsafe {
            a2fo_rs_stage9_a1_physics_default_key((&key as *const &[u8]).cast_mut().cast(), same)
        };
        assert_eq!(index, 0);
        let first = 1u8;
        let second = 2u8;
        let a = (&first as *const u8).cast::<c_void>();
        let b = (&second as *const u8).cast::<c_void>();
        let mut bits = 0;
        let mut combat = 0;
        unsafe { a2fo_rs_stage9_a1_physics_default_bits(index, 2, a, b, &mut bits, &mut combat) };
        assert_eq!((bits, combat), (0x40400000, 0));
        unsafe { a2fo_rs_stage9_a1_physics_default_bits(index, 2, a, a, &mut bits, &mut combat) };
        assert_eq!((bits, combat), (0x3f000000, 1));
        assert_eq!(
            DEFAULTS[12],
            [0xbf800000, 0x3f000000, 0x3f800000, 0xbf800000, 0x3f800000]
        );
    }
    #[test]
    fn model_and_signed_report_cap() {
        assert_eq!(
            unsafe { core::ffi::CStr::from_ptr(a2fo_rs_stage9_a1_physics_model_name(1, -1)) }
                .to_bytes(),
            b"borg"
        );
        assert_eq!(
            unsafe { core::ffi::CStr::from_ptr(a2fo_rs_stage9_a1_physics_model_name(0, 1)) }
                .to_bytes(),
            b"smooth"
        );
        assert_eq!(a2fo_rs_stage9_a1_physics_log_case(i32::MIN), 1);
        assert_eq!(a2fo_rs_stage9_a1_physics_log_case(64), 1);
        assert_eq!(a2fo_rs_stage9_a1_physics_log_case(65), 2);
        assert_eq!(a2fo_rs_stage9_a1_physics_log_case(66), 0);
    }
}
