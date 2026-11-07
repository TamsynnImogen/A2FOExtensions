//! A1 BZN/MDF/AIP workflow order. Native operations retain strings, file I/O,
//! raw module layouts, checked pointer access, SDK calls and float kernels.
use core::ffi::c_void;

type Op = unsafe extern "C" fn(*mut c_void, u32, u64, u64) -> u64;
const ERROR: u64 = u64::MAX;
unsafe fn call(ctx: *mut c_void, op: Op, id: u32, a: u64, b: u64) -> Option<u64> {
    let value = unsafe { op(ctx, id, a, b) };
    (value != ERROR).then_some(value)
}

/// Native constructor for `supplied` happens inside the caller's original
/// catch region before this routine. Each root constructs all three paths
/// before the first attempt, preserving allocation and callback order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_find_bzn(ctx: *mut c_void, op: Op) -> u8 {
    let Some(size) = (unsafe { call(ctx, op, 0, 0, 0) }) else {
        return 0;
    };
    if size > 1024 {
        return 0;
    }
    for index in 0..size {
        let Some(ch) = (unsafe { call(ctx, op, 1, index, 0) }) else {
            return 0;
        };
        if ch == b'/' as u64 && unsafe { call(ctx, op, 2, index, 0) }.is_none() {
            return 0;
        }
    }
    let Some(found) = (unsafe { call(ctx, op, 3, 0, 0) }) else {
        return 0;
    };
    if found != 0 {
        return 1;
    }
    let Some(api) = (unsafe { call(ctx, op, 4, 0, 0) }) else {
        return 0;
    };
    if api == 0 {
        return 0;
    }
    if unsafe { call(ctx, op, 5, 0, 0) }.is_none() {
        return 0;
    }
    loop {
        let Some(prefix) = (unsafe { call(ctx, op, 6, 0, 0) }) else {
            return 0;
        };
        if prefix == 0 {
            break;
        }
        if unsafe { call(ctx, op, 7, 0, 0) }.is_none() {
            return 0;
        }
    }
    if unsafe { call(ctx, op, 8, 0, 0) }.is_none() {
        return 0;
    }
    let Some(count) = (unsafe { call(ctx, op, 9, 0, 0) }) else {
        return 0;
    };
    if count == 0 || count > 4096 {
        return 0;
    }
    for position in (0..count).rev() {
        let Some(root) = (unsafe { call(ctx, op, 10, position, 0) }) else {
            return 0;
        };
        if root == 0 {
            continue;
        }
        for candidate in 0..3 {
            let Some(found) = (unsafe { call(ctx, op, 11, candidate, 0) }) else {
                return 0;
            };
            if found != 0 {
                return 1;
            }
        }
        if unsafe { call(ctx, op, 12, 0, 0) }.is_none() {
            return 0;
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_read_mdf(ctx: *mut c_void, op: Op) -> u8 {
    let Some(nonempty) = (unsafe { call(ctx, op, 0, 0, 0) }) else {
        return 0;
    };
    if nonempty == 0 {
        return 0;
    }
    if unsafe { call(ctx, op, 1, 0, 0) }.is_none() {
        return 0;
    }
    let Some(separator) = (unsafe { call(ctx, op, 2, 0, 0) }) else {
        return 0;
    };
    let Some(extension) = (unsafe { call(ctx, op, 3, 0, 0) }) else {
        return 0;
    };
    let missing = u64::from(u32::MAX);
    let append = extension == missing || (separator != missing && extension < separator);
    if unsafe { call(ctx, op, 4, u64::from(append), 0) }.is_none() {
        return 0;
    }
    let Some(parsed) = (unsafe { call(ctx, op, 5, 0, 0) }) else {
        return 0;
    };
    if parsed == 0 {
        return 0;
    }
    let Some(published) = (unsafe { call(ctx, op, 6, 0, 0) }) else {
        return 0;
    };
    u8::from(published != 0)
}

/// Two passes: every raw record is validated before the first native write.
/// Active count is reread before each assignment because a native callback
/// can mutate caller-owned MDF data between entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_apply_mdf(ctx: *mut c_void, op: Op) -> u8 {
    let Some(admitted) = (unsafe { call(ctx, op, 0, 0, 0) }) else {
        return 0;
    };
    if admitted == 0 {
        return 0;
    }
    let Some(slots) = (unsafe { call(ctx, op, 1, 0, 0) }) else {
        return 0;
    };
    if slots == 0 {
        return 0;
    }
    for id in 2..=4 {
        let Some(valid) = (unsafe { call(ctx, op, id, 0, 0) }) else {
            return 0;
        };
        if valid == 0 {
            return 0;
        }
    }
    for index in 1..slots {
        let Some(valid) = (unsafe { call(ctx, op, 5, index, 0) }) else {
            return 0;
        };
        if valid == 0 {
            return 0;
        }
    }
    for index in 1..slots {
        if unsafe { call(ctx, op, 10, index, 0) }.is_none() {
            return 0;
        }
        let Some(count) = (unsafe { call(ctx, op, 9, 0, 0) }) else {
            return 0;
        };
        let active = index <= count;
        if unsafe { call(ctx, op, 6, index, u64::from(active)) }.is_none() {
            return 0;
        }
        if active {
            let Some(converted) = (unsafe { call(ctx, op, 7, index, 0) }) else {
                return 0;
            };
            if converted == 0 {
                return 0;
            }
        }
    }
    let Some(written) = (unsafe { call(ctx, op, 8, 0, 0) }) else {
        return 0;
    };
    u8::from(written != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_prefers_legacy_aip(ctx: *mut c_void, op: Op) -> u8 {
    let Some(api) = (unsafe { call(ctx, op, 0, 0, 0) }) else {
        return 0;
    };
    if api == 0 {
        return 0;
    }
    let Some(count) = (unsafe { call(ctx, op, 1, 0, 0) }) else {
        return 0;
    };
    if count == 0 || count > 4096 {
        return 0;
    }
    let mut a1_root = count;
    for index in 0..count {
        let Some(found) = (unsafe { call(ctx, op, 2, index, 0) }) else {
            return 0;
        };
        if found != 0 {
            a1_root = index;
            break;
        }
    }
    if a1_root == count {
        return 0;
    }
    for index in (a1_root..count).rev() {
        if unsafe { call(ctx, op, 3, index, 0) }.is_none() {
            return 0;
        }
        let Some(requested) = (unsafe { call(ctx, op, 4, 0, 0) }) else {
            return 0;
        };
        let Some(legacy) = (unsafe { call(ctx, op, 5, 0, 0) }) else {
            return 0;
        };
        if requested != 0 || legacy != 0 {
            return u8::from(legacy != 0 && requested == 0);
        }
    }
    0
}

/// Returns an opaque native pointer, or zero. Any caught native exception
/// aborts the compatibility attempt; the C++ caller then invokes the original
/// requested-name gateway outside the compatibility catch region.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_resolve_aip(ctx: *mut c_void, op: Op) -> u64 {
    let Some(built) = (unsafe { call(ctx, op, 0, 0, 0) }) else {
        return 0;
    };
    if built == 0 {
        return 0;
    }
    let Some(prefer) = (unsafe { call(ctx, op, 1, 0, 0) }) else {
        return 0;
    };
    if prefer == 0 {
        return 0;
    }
    let Some(legacy) = (unsafe { call(ctx, op, 2, 0, 0) }) else {
        return 0;
    };
    let Some(count) = (unsafe { call(ctx, op, 3, 0, 0) }) else {
        return 0;
    };
    let signed_count = count as u32 as i32;
    if signed_count <= 32 {
        if unsafe { call(ctx, op, 4, count, 0) }.is_none() {
            return 0;
        }
    } else if signed_count == 33 && unsafe { call(ctx, op, 5, 0, 0) }.is_none() {
        return 0;
    }
    legacy
}
