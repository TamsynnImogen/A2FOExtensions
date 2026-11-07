//! Module identities and coherent policy generations. Bytes intentionally use
//! ASCII folding, matching the legacy ANSI path/module contract.
use core::sync::atomic::{AtomicU64, Ordering};

const PREPARED: u64 = 1;
const AVAILABLE: u64 = 2;
const REQUESTED: u64 = 4;
const GENERATION: u64 = 8;
const STARTUP_FIRST: i32 = 24;
const NAMES: [&[u8]; 48] = [
    b"A2FOBuildTooltips\0",
    b"A2FONormalWeaponTech\0",
    b"A2FOWreckage\0",
    b"A2FOInfoIni\0",
    b"A2FOMuzzleFlashes\0",
    b"A2FOTeamChangeWeapons\0",
    b"A2FOWeaponDamageControls\0",
    b"A1Fallbacks\0",
    b"A2FOAnimatedHardpoints\0",
    b"A2FOCameraControls\0",
    b"A2FOViewDistance\0",
    b"A2FOInstantActionSettings\0",
    b"A2FOEditMenu\0",
    b"A2FOM3D\0",
    b"A2FOBreeders\0",
    b"A2FOInfestation\0",
    b"A2FOSwarmSystem\0",
    b"A2FOODFVariants\0",
    b"A2FOFireArcs\0",
    b"A2FOAnimations\0",
    b"A2FOAlwaysShowShields\0",
    b"A2FOStationRotation\0",
    b"A2FORadarDiagnostics\0",
    b"A2FORecoveryTests\0",
    b"A2FOCheats\0",
    b"A2FOCollectiveXP\0",
    b"A2FODirectionalShields\0",
    b"A2FOEnergySystems\0",
    b"A2FOHybridBuild\0",
    b"A2FOMultiYardBuild\0",
    b"A2FONebulaRenderer\0",
    b"A2FOPointDefenseCycles\0",
    b"A2FORefitYards\0",
    b"A2FOTurrets\0",
    b"A2FORGBTextures\0",
    b"A2FOTextureVariants\0",
    b"A1Compat\0",
    b"A2FOCraftIdentity\0",
    b"A2FOFeaturePack\0",
    b"A2FOLogMailer\0",
    b"A2FOMissionSelector\0",
    b"A2FOMissions\0",
    b"A2FORefitMenus\0",
    b"A2FOResources\0",
    b"A2FOSOLMaps\0",
    b"A2FOShellDisplay\0",
    b"A2FOSquadrons\0",
    b"A2FOVideo\0",
];

fn find(mut name: &[u8]) -> i32 {
    if name.is_empty()
        || name.iter().any(|c| b"/\\:".contains(c))
        || name.windows(2).any(|pair| pair == b"..")
    {
        return -1;
    }
    if name.len() >= 4 && name[name.len() - 4..].eq_ignore_ascii_case(b".dll") {
        name = &name[..name.len() - 4];
    }
    NAMES
        .iter()
        .position(|n| name.eq_ignore_ascii_case(&n[..n.len() - 1]))
        .map_or(-1, |index| index as i32)
}

fn separator(c: u8) -> bool {
    c == b'/' || c == b'\\'
}
fn same_root(mut left: &[u8], mut right: &[u8]) -> bool {
    if left.is_empty() || right.is_empty() {
        return false;
    }
    while left.len() > 1 && separator(left[left.len() - 1]) {
        left = &left[..left.len() - 1];
    }
    while right.len() > 1 && separator(right[right.len() - 1]) {
        right = &right[..right.len() - 1];
    }
    left.len() == right.len()
        && left
            .iter()
            .zip(right)
            .all(|(&l, &r)| (separator(l) && separator(r)) || l.eq_ignore_ascii_case(&r))
}

// Safety: nonempty range must point to length readable bytes in one allocation.
unsafe fn bytes<'a>(ptr: *const u8, length: usize) -> Option<&'a [u8]> {
    if length == 0 {
        return Some(&[]);
    }
    if ptr.is_null() || length > isize::MAX as usize {
        return None;
    }
    Some(unsafe { core::slice::from_raw_parts(ptr, length) })
}

// Safety: caller supplies exclusive Rust-atomic storage for the full lifetime
// of the returned reference; there must be no simultaneous non-atomic access.
unsafe fn atomic<'a>(ptr: *const u64) -> Option<&'a AtomicU64> {
    if ptr.is_null() || !ptr.cast::<AtomicU64>().is_aligned() {
        return None;
    }
    Some(unsafe { AtomicU64::from_ptr(ptr.cast_mut()) })
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_builtin_count() -> u32 {
    NAMES.len() as u32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_builtin_requires_restart(feature: i32) -> u8 {
    u8::from((STARTUP_FIRST..NAMES.len() as i32).contains(&feature))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_builtin_name(feature: i32) -> *const core::ffi::c_char {
    NAMES
        .get(feature as usize)
        .map_or(core::ptr::null(), |name| name.as_ptr().cast())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_builtin_find(ptr: *const u8, length: usize) -> i32 {
    unsafe { bytes(ptr, length) }.map_or(-1, find)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_builtin_same_root(
    left: *const u8,
    ll: usize,
    right: *const u8,
    rl: usize,
) -> u8 {
    match (unsafe { bytes(left, ll) }, unsafe { bytes(right, rl) }) {
        (Some(l), Some(r)) => u8::from(same_root(l, r)),
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_builtin_prepare(ptr: *mut u64, available: u8, requested: u8) {
    if let Some(state) = unsafe { atomic(ptr) } {
        let initial = PREPARED
            | if available != 0 { AVAILABLE } else { 0 }
            | if requested != 0 { REQUESTED } else { 0 };
        let _ = state.compare_exchange(0, initial, Ordering::AcqRel, Ordering::Acquire);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_builtin_request(ptr: *mut u64, enabled: u8) -> u8 {
    let Some(state) = (unsafe { atomic(ptr) }) else {
        return 0;
    };
    let enabled = enabled != 0;
    let mut current = state.load(Ordering::Acquire);
    loop {
        if enabled && current & (PREPARED | AVAILABLE) != PREPARED | AVAILABLE {
            return 0;
        }
        if (current & REQUESTED != 0) == enabled {
            return 1;
        }
        let next = (if enabled {
            current | REQUESTED
        } else {
            current & !REQUESTED
        })
        .wrapping_add(GENERATION);
        match state.compare_exchange_weak(current, next, Ordering::AcqRel, Ordering::Acquire) {
            Ok(_) => return 1,
            Err(observed) => current = observed,
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_builtin_flags(ptr: *const u64) -> u64 {
    unsafe { atomic(ptr) }.map_or(0, |state| state.load(Ordering::Acquire))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_builtin_activation(ptr: *const u64) -> u64 {
    let flags = unsafe { a2fo_rs_builtin_flags(ptr) };
    if flags & (PREPARED | AVAILABLE | REQUESTED) == PREPARED | AVAILABLE | REQUESTED {
        flags
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn identity_and_restart_boundaries() {
        for (index, name) in NAMES.iter().enumerate() {
            let mut alternate = name[..name.len() - 1].to_ascii_lowercase();
            alternate.extend_from_slice(b".DLL");
            assert_eq!(find(&alternate), index as i32);
            assert_eq!(
                a2fo_rs_builtin_requires_restart(index as i32),
                u8::from(index >= 24)
            );
        }
        for bad in [
            b"".as_slice(),
            b"../A2FOVideo",
            b"C:A2FOVideo",
            b"A2FOVideo\0",
            b"A2FOVideo.dll.dll",
            b"\xff",
        ] {
            assert_eq!(find(bad), -1);
        }
        for id in [-1, 48, i32::MAX] {
            assert!(a2fo_rs_builtin_name(id).is_null());
            assert_eq!(a2fo_rs_builtin_requires_restart(id), 0);
        }
    }
    #[test]
    fn roots_are_ordered_bytes_not_unicode() {
        assert!(same_root(b"C:\\Fleet Ops\\Data\\", b"c:/fleet ops/data///"));
        assert!(same_root(b"/", b"\\\\"));
        assert!(!same_root(b"", b""));
        assert!(!same_root(b"c:/data", b"c:/other"));
        assert!(!same_root(b"\xc0", b"\xe0"));
    }
    #[test]
    fn preparation_and_generations() {
        let mut storage = AtomicU64::new(0);
        let p = storage.get_mut() as *mut u64;
        unsafe {
            assert_eq!(a2fo_rs_builtin_request(p, 1), 0);
            assert_eq!(a2fo_rs_builtin_request(p, 0), 1);
            a2fo_rs_builtin_prepare(p, 1, 0);
            assert_eq!(a2fo_rs_builtin_request(p, 1), 1);
            let first = a2fo_rs_builtin_activation(p);
            assert_ne!(first, 0);
            assert_eq!(a2fo_rs_builtin_request(p, 1), 1);
            assert_eq!(a2fo_rs_builtin_activation(p), first);
            a2fo_rs_builtin_request(p, 0);
            assert_eq!(a2fo_rs_builtin_activation(p), 0);
            a2fo_rs_builtin_request(p, 1);
            assert_ne!(a2fo_rs_builtin_activation(p), first);
            a2fo_rs_builtin_prepare(p, 0, 0);
            assert_eq!(a2fo_rs_builtin_flags(p) & 7, 7);
        }
    }
    #[test]
    fn unavailable_and_null_fail_closed() {
        let mut state = AtomicU64::new(0);
        let p = state.get_mut() as *mut u64;
        unsafe {
            a2fo_rs_builtin_prepare(p, 0, 1);
            assert_eq!(a2fo_rs_builtin_activation(p), 0);
            assert_eq!(a2fo_rs_builtin_request(p, 1), 0);
            a2fo_rs_builtin_request(p, 0);
            a2fo_rs_builtin_prepare(p, 1, 1);
            assert_eq!(a2fo_rs_builtin_flags(p), 9);
            assert_eq!(a2fo_rs_builtin_find(core::ptr::null(), 3), -1);
            assert_eq!(a2fo_rs_builtin_flags(core::ptr::null()), 0);
        }
    }
}
