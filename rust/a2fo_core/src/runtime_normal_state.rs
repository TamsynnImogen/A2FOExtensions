//! Authoritative NormalWeaponTech fields. Native callers retain synchronization
//! and perform interlocked access without Rust references to the live fields.
use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::mem::{align_of, offset_of, size_of};

#[repr(C)]
struct State {
    api: *const c_void,
    armada: *mut c_void,
    fleet_ops: *mut c_void,
    runtime_ready: u8,
    logged_first_default_weapon: i32,
    logged_first_listed_allowed: i32,
    logged_first_listed_blocked: i32,
}
struct StateCell(UnsafeCell<State>);
// SAFETY: the original native initialization and interlocked access rules apply;
// this module only returns a raw pointer and never retains field references.
unsafe impl Sync for StateCell {}
static STATE: StateCell = StateCell(UnsafeCell::new(unsafe { core::mem::zeroed() }));

#[repr(C)]
pub struct Layout {
    values: [usize; 9],
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_normal_state() -> *mut c_void {
    STATE.0.get().cast()
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_normal_state_layout() -> Layout {
    Layout {
        values: [
            size_of::<State>(),
            align_of::<State>(),
            offset_of!(State, api),
            offset_of!(State, armada),
            offset_of!(State, fleet_ops),
            offset_of!(State, runtime_ready),
            offset_of!(State, logged_first_default_weapon),
            offset_of!(State, logged_first_listed_allowed),
            offset_of!(State, logged_first_listed_blocked),
        ],
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn slot_is_stable_and_fields_start_empty() {
        let pointer = a2fo_rs_normal_state();
        assert!(!pointer.is_null());
        assert_eq!(pointer, a2fo_rs_normal_state());
        let state = unsafe { &*pointer.cast::<State>() };
        assert!(state.api.is_null() && state.armada.is_null() && state.fleet_ops.is_null());
        assert_eq!(state.runtime_ready, 0);
        assert_eq!(
            (
                state.logged_first_default_weapon,
                state.logged_first_listed_allowed,
                state.logged_first_listed_blocked
            ),
            (0, 0, 0)
        );
    }
}
