//! A1 legacy physics admission and profile routing. Native operations retain
//! engine getters, output buffer writes, CRT comparisons and native strings.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, usize) -> u64;
fn call(ctx: *mut c_void, op: Op, id: u32, arg: usize) -> Option<u64> {
    let result = unsafe { op(ctx, id, arg) };
    (result != u64::MAX).then_some(result)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_physics_database(ctx: *mut c_void, op: Op) -> u8 {
    if call(ctx, op, 0, 0) != Some(1) || call(ctx, op, 1, 0) != Some(1) {
        return 0;
    }
    match call(ctx, op, 2, 0) {
        Some(0) => {}
        _ => return 0,
    }
    let impulse = match call(ctx, op, 3, 0) {
        Some(value) => value != 0,
        None => return 0,
    };
    let warp = match call(ctx, op, 4, 0) {
        Some(value) => value != 0,
        None => return 0,
    };
    u8::from(impulse || warp)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_physics_profile(ctx: *mut c_void, op: Op) -> u8 {
    if call(ctx, op, 0, 0) == Some(1)
        && (call(ctx, op, 1, 0).is_none() || call(ctx, op, 2, 0).is_none())
    {
        return 0;
    }
    let has_name = match call(ctx, op, 3, 0) {
        Some(value) => value != 0,
        None => return 0,
    };
    let mut basename = 0usize;
    if has_name {
        let mut index = 0usize;
        loop {
            let byte = match call(ctx, op, 4, index) {
                Some(value) => value as u8,
                None => return 0,
            };
            if byte == 0 {
                break;
            }
            if byte == b'/' || byte == b'\\' {
                basename = index + 1;
            }
            index += 1;
        }
    }
    match call(ctx, op, 5, basename) {
        Some(1) => return 1,
        Some(_) => {}
        None => return 0,
    }
    match call(ctx, op, 6, basename) {
        Some(1) => return 1,
        Some(_) => {}
        None => return 0,
    }
    match call(ctx, op, 7, basename) {
        Some(1) => return 2,
        Some(_) => {}
        None => return 0,
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_a1_physics_profile_name(profile: u32) -> u8 {
    match profile {
        1 => 1,
        2 => 2,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    struct Mock {
        calls: Vec<u32>,
        path: &'static [u8],
        results: [u64; 5],
    }
    unsafe extern "C" fn callback(ctx: *mut c_void, id: u32, arg: usize) -> u64 {
        let mock = unsafe { &mut *ctx.cast::<Mock>() };
        mock.calls.push(id);
        match id {
            0..=3 => mock.results[id as usize],
            4 if !mock.path.is_empty() => u64::from(*mock.path.get(arg).unwrap_or(&0)),
            4 => mock.results[4],
            5 => u64::from(
                mock.path
                    .get(arg..)
                    .is_some_and(|s| s.starts_with(b"cnstphys")),
            ),
            6 => u64::from(
                mock.path
                    .get(arg..)
                    .is_some_and(|s| s.starts_with(b"constphys")),
            ),
            7 => u64::from(
                mock.path
                    .get(arg..)
                    .is_some_and(|s| s.starts_with(b"battphys")),
            ),
            _ => 0,
        }
    }
    #[test]
    fn physics_reads_both_fallback_keys() {
        let mut mock = Mock {
            calls: Vec::new(),
            path: b"",
            results: [1, 1, 0, 1, 0],
        };
        let ptr = (&raw mut mock).cast::<c_void>();
        assert_eq!(
            unsafe { a2fo_rs_stage8_a1_physics_database(ptr, callback) },
            1
        );
        assert_eq!(mock.calls, [0, 1, 2, 3, 4]);
        mock.calls.clear();
        mock.results[2] = 1;
        assert_eq!(
            unsafe { a2fo_rs_stage8_a1_physics_database(ptr, callback) },
            0
        );
        assert_eq!(mock.calls, [0, 1, 2]);
    }
    #[test]
    fn profile_scans_basename_then_uses_native_comparisons() {
        let mut mock = Mock {
            calls: Vec::new(),
            path: b"folder\\battphys.odf\0",
            results: [1, 0, 0, 1, 0],
        };
        let ptr = (&raw mut mock).cast::<c_void>();
        assert_eq!(
            unsafe { a2fo_rs_stage8_a1_physics_profile(ptr, callback) },
            2
        );
        assert_eq!(mock.calls[..4], [0, 1, 2, 3]);
        assert_eq!(mock.calls[mock.calls.len() - 3..], [5, 6, 7]);
        assert_eq!(a2fo_rs_stage8_a1_physics_profile_name(99), 0);
    }
}
