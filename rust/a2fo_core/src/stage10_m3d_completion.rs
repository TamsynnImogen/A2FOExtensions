//! Bounded M3D name traversal and texture candidate order. Native callbacks
//! own pointer validation and C++ string/vector operations.
use core::ffi::c_void;

type NameStep = unsafe extern "C" fn(*mut c_void, usize, u32, *mut u8) -> u8;
type TextureStep = unsafe extern "C" fn(*mut c_void, u32, *mut u8) -> u8;

/// Returns 1 only after a checked read yields NUL. A failed read/append or
/// 128 non-NUL bytes returns 0, matching the native empty-name result.
///
/// # Safety
/// `step` must be callable for the duration of this call; `context` must be
/// valid for that callback. The callback must not unwind across this ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_m3d_bounded_name(
    step: NameStep,
    context: *mut c_void,
) -> u8 {
    for index in 0..128 {
        let mut byte = 0u8;
        if unsafe { step(context, index, 0, &mut byte) } == 0 {
            return 0;
        }
        if byte == 0 {
            return 1;
        }
        if unsafe { step(context, index, 1, &mut byte) } == 0 {
            return 0;
        }
    }
    0
}

/// Ops: 0 base empty, 1 append base, 2 no dot, 3 append DDS, 4 append TGA.
/// The dot query is deliberately made only after appending a nonempty base.
///
/// # Safety
/// `step` must be callable for the duration of this call; `context` must be
/// valid for that callback. The callback must not unwind across this ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_m3d_texture_candidates(
    step: TextureStep,
    context: *mut c_void,
) {
    let mut value = 0u8;
    if unsafe { step(context, 0, &mut value) } == 0 || value != 0 {
        return;
    }
    if unsafe { step(context, 1, &mut value) } == 0 {
        return;
    }
    if unsafe { step(context, 2, &mut value) } == 0 || value == 0 {
        return;
    }
    if unsafe { step(context, 3, &mut value) } == 0 {
        return;
    }
    unsafe { step(context, 4, &mut value) };
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;

    struct NameFixture {
        bytes: Vec<u8>,
        calls: Vec<(usize, u32)>,
        fail_at: Option<(usize, u32)>,
    }
    unsafe extern "C" fn name_step(
        raw: *mut c_void,
        index: usize,
        phase: u32,
        byte: *mut u8,
    ) -> u8 {
        let f = unsafe { &mut *raw.cast::<NameFixture>() };
        f.calls.push((index, phase));
        if f.fail_at == Some((index, phase)) || index >= f.bytes.len() {
            return 0;
        }
        if phase == 0 {
            unsafe { *byte = f.bytes[index] };
        }
        1
    }
    fn name(bytes: Vec<u8>, fail_at: Option<(usize, u32)>) -> (u8, Vec<(usize, u32)>) {
        let mut f = NameFixture {
            bytes,
            calls: Vec::new(),
            fail_at,
        };
        let result = unsafe {
            a2fo_rs_stage10_m3d_bounded_name(name_step, (&mut f as *mut NameFixture).cast())
        };
        (result, f.calls)
    }
    #[test]
    fn name_nul_stops_before_later_bytes() {
        assert_eq!(
            name(vec![b'a', 0, b'z'], None),
            (1, vec![(0, 0), (0, 1), (1, 0)])
        );
        assert_eq!(name(vec![0], None), (1, vec![(0, 0)]));
    }
    #[test]
    fn name_failure_aborts_at_checked_byte() {
        assert_eq!(
            name(vec![b'a', b'b', 0], Some((1, 0))),
            (0, vec![(0, 0), (0, 1), (1, 0)])
        );
        assert_eq!(name(vec![b'a', 0], Some((0, 1))), (0, vec![(0, 0), (0, 1)]));
    }
    #[test]
    fn name_bound_requires_nul_within_128() {
        let (result, calls) = name(vec![b'a'; 129], None);
        assert_eq!((result, calls.len(), calls[255]), (0, 256, (127, 1)));
        let mut bytes = vec![b'a'; 128];
        bytes[127] = 0;
        assert_eq!(name(bytes, None).0, 1);
    }
    #[test]
    fn name_ff_is_data_not_status() {
        assert_eq!(name(vec![0xff, 0], None), (1, vec![(0, 0), (0, 1), (1, 0)]));
    }

    struct TextureFixture {
        empty: bool,
        no_dot: bool,
        fail_at: Option<u32>,
        calls: Vec<u32>,
    }
    unsafe extern "C" fn texture_step(raw: *mut c_void, op: u32, value: *mut u8) -> u8 {
        let f = unsafe { &mut *raw.cast::<TextureFixture>() };
        f.calls.push(op);
        if f.fail_at == Some(op) {
            return 0;
        }
        if op == 0 {
            unsafe { *value = u8::from(f.empty) }
        }
        if op == 2 {
            unsafe { *value = u8::from(f.no_dot) }
        }
        1
    }
    fn texture(empty: bool, no_dot: bool, fail_at: Option<u32>) -> Vec<u32> {
        let mut f = TextureFixture {
            empty,
            no_dot,
            fail_at,
            calls: Vec::new(),
        };
        unsafe {
            a2fo_rs_stage10_m3d_texture_candidates(
                texture_step,
                (&mut f as *mut TextureFixture).cast(),
            )
        };
        f.calls
    }
    #[test]
    fn texture_empty_skips_dot_and_candidates() {
        assert_eq!(texture(true, true, None), vec![0]);
    }
    #[test]
    fn texture_existing_suffix_keeps_only_base() {
        assert_eq!(texture(false, false, None), vec![0, 1, 2]);
    }
    #[test]
    fn texture_missing_suffix_appends_dds_then_tga() {
        assert_eq!(texture(false, true, None), vec![0, 1, 2, 3, 4]);
    }
    #[test]
    fn texture_callback_failure_stops_later_operations() {
        assert_eq!(texture(false, true, Some(1)), vec![0, 1]);
        assert_eq!(texture(false, true, Some(3)), vec![0, 1, 2, 3]);
    }
}
