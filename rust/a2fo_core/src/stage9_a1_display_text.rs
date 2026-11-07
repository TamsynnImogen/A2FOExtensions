use core::ffi::c_void;

/// The synchronous native callback owns manager/key reads, localization lookup,
/// pointer validation, CRT comparisons, and fallback storage. It must not
/// unwind into Rust. Rust never dereferences or retains a returned pointer.
type Step = unsafe extern "C" fn(*mut c_void, u32) -> usize;

static EMPTY_LABEL: [u8; 1] = [0];

/// Select the source pointer for native snprintf copying.
///
/// Op 3 returns the looked-up pointer and also stores it in native context.
/// Ops 4-7 inspect that captured pointer and the *current* key bytes. Op 8
/// returns the fallback pointer. Returned strings must remain live until the
/// native caller completes its copy.
///
/// # Safety
/// `context` and `step` must remain valid for synchronous calls. The callback
/// must catch C++ exceptions and must not retain a pointer into Rust state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_display_text_label(
    context: *mut c_void,
    step: Option<Step>,
) -> usize {
    let Some(step) = step else {
        return EMPTY_LABEL.as_ptr() as usize;
    };

    if unsafe { step(context, 0) } != 0
        && unsafe { step(context, 1) } != 0
        && unsafe { step(context, 2) } != 0
    {
        let resolved = unsafe { step(context, 3) };
        if resolved != 0
            && unsafe { step(context, 4) } == 0
            && unsafe { step(context, 5) } != 0
            && !(unsafe { step(context, 6) } != 0 && unsafe { step(context, 7) } != 0)
        {
            return resolved;
        }
    }

    let fallback = unsafe { step(context, 8) };
    if fallback != 0 {
        fallback
    } else {
        EMPTY_LABEL.as_ptr() as usize
    }
}

#[cfg(test)]
mod tests {
    include!("stage9_a1_display_text_tests.rs");
}
