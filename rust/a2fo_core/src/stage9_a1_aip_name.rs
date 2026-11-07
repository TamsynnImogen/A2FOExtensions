use core::ffi::c_void;

/// Synchronous native callback. All string storage, CRT classification and
/// case conversion, compare/assign/append, and exception capture stay native.
/// No Rust reference or slice into native storage survives a callback.
type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> usize;

const ERROR: usize = usize::MAX;

/// Direct the legacy AIP-name policy while native owns the mutable strings.
///
/// Operations: 0 clear legacy; 1 requested pointer present; 2 first byte
/// nonempty; 3 construct normalized and return its size; 4 read byte(index);
/// 5 CRT isalnum(byte); 6 CRT tolower(byte) and write normalized[index];
/// 7 CRT isdigit of normalized[index]; 8 compare normalized suffix against
/// native instant-action suffix; 9 assign legacy from normalized prefix;
/// 10 append native legacy suffix. Native passes its exact suffix length.
///
/// The callback returns `usize::MAX` after catching a C++ exception. Native
/// rethrows that captured exception after the Rust call returns, preserving
/// the original enclosing catch behavior. Boolean callbacks return 0 or 1.
///
/// # Safety
/// The callback/context must remain valid synchronously and never unwind into
/// Rust. Native strings must not be concurrently mutated outside callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_aip_name(
    context: *mut c_void,
    suffix_size: usize,
    step: Option<Step>,
) -> u8 {
    let Some(step) = step else { return 0 };
    if unsafe { step(context, 0, 0, 0) } == ERROR {
        return 0;
    }
    if unsafe { step(context, 1, 0, 0) } != 1 {
        return 0;
    }
    if unsafe { step(context, 2, 0, 0) } != 1 {
        return 0;
    }

    let length = unsafe { step(context, 3, 0, 0) };
    if length == ERROR || length > 192 {
        return 0;
    }

    for index in 0..length {
        let byte = unsafe { step(context, 4, index, 0) };
        if byte == ERROR || byte > u8::MAX as usize {
            return 0;
        }
        let alnum = unsafe { step(context, 5, 0, byte) };
        if alnum == ERROR {
            return 0;
        }
        if alnum == 0 && byte != b'_' as usize && byte != b'-' as usize {
            return 0;
        }
        if unsafe { step(context, 6, index, byte) } == ERROR {
            return 0;
        }
    }

    let mut stem_end = length;
    while stem_end != 0 {
        let digit = unsafe { step(context, 7, stem_end - 1, 0) };
        if digit == ERROR {
            return 0;
        }
        if digit == 0 {
            break;
        }
        stem_end -= 1;
    }
    if stem_end <= suffix_size {
        return 0;
    }
    let prefix_end = stem_end - suffix_size;
    if unsafe { step(context, 8, prefix_end, suffix_size) } != 1 {
        return 0;
    }
    if unsafe { step(context, 9, prefix_end, 0) } == ERROR {
        return 0;
    }
    if unsafe { step(context, 10, 0, 0) } == ERROR {
        return 0;
    }
    1
}

#[cfg(test)]
mod tests {
    include!("stage9_a1_aip_name_tests.rs");
}
