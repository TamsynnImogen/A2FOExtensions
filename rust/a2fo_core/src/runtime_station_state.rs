//! Storage only for the Station Rotation feature. Native code retains all
//! callback, hook, and borrowed game/API pointer semantics.
use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::mem::{align_of, offset_of, size_of};

#[repr(C)]
struct PlacementControl {
    target: usize,
    turns: u32,
    r_down: u8,
}

#[repr(C)]
struct InlineHook {
    target: *mut c_void,
    gateway: *mut c_void,
    length: usize,
}

#[repr(C)]
struct State {
    api: *const c_void,
    armada: *mut u8,
    fleet: *mut u8,
    ready: u8,
    control: PlacementControl,
    keyboard: InlineHook,
    broadcast: InlineHook,
    queue: InlineHook,
    set_command: InlineHook,
    preview: InlineHook,
    sending_class: *mut c_void,
    sending_turns: u32,
    sending_activation: u64,
    previous_activation: u64,
    action: InlineHook,
    render_footprints: InlineHook,
    footprint: InlineHook,
    can_place: InlineHook,
    placement_clear: InlineHook,
    build: InlineHook,
    build_process: *const c_void,
    placement_class: *const c_void,
    placement_turns: u32,
    ui_footprint_depth: u32,
}

struct StateCell(UnsafeCell<State>);
// Native callers own synchronization; Rust creates no references to this slot.
unsafe impl Sync for StateCell {}
static STATE: StateCell = StateCell(UnsafeCell::new(unsafe { core::mem::zeroed() }));

#[repr(C)]
pub struct Layout {
    values: [usize; 36],
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_station_state() -> *mut c_void {
    STATE.0.get().cast()
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_station_state_layout() -> Layout {
    Layout {
        values: [
            size_of::<State>(),
            align_of::<State>(),
            offset_of!(State, api),
            offset_of!(State, armada),
            offset_of!(State, fleet),
            offset_of!(State, ready),
            offset_of!(State, control),
            offset_of!(State, keyboard),
            offset_of!(State, broadcast),
            offset_of!(State, queue),
            offset_of!(State, set_command),
            offset_of!(State, preview),
            offset_of!(State, sending_class),
            offset_of!(State, sending_turns),
            offset_of!(State, sending_activation),
            offset_of!(State, previous_activation),
            offset_of!(State, action),
            offset_of!(State, render_footprints),
            offset_of!(State, footprint),
            offset_of!(State, can_place),
            offset_of!(State, placement_clear),
            offset_of!(State, build),
            offset_of!(State, build_process),
            offset_of!(State, placement_class),
            offset_of!(State, placement_turns),
            offset_of!(State, ui_footprint_depth),
            size_of::<PlacementControl>(),
            align_of::<PlacementControl>(),
            offset_of!(PlacementControl, target),
            offset_of!(PlacementControl, turns),
            offset_of!(PlacementControl, r_down),
            size_of::<InlineHook>(),
            align_of::<InlineHook>(),
            offset_of!(InlineHook, target),
            offset_of!(InlineHook, gateway),
            offset_of!(InlineHook, length),
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn slot_is_stable_and_zero_initialized() {
        let first = a2fo_rs_station_state();
        assert!(!first.is_null());
        assert_eq!(first, a2fo_rs_station_state());
        // Inspect fields rather than padding, which Rust need not initialize.
        let state = unsafe { &*first.cast::<State>() };
        assert!(state.api.is_null() && state.armada.is_null() && state.fleet.is_null());
        assert_eq!(state.ready, 0);
        assert_eq!(
            (
                state.control.target,
                state.control.turns,
                state.control.r_down
            ),
            (0, 0, 0)
        );
        for hook in [
            &state.keyboard,
            &state.broadcast,
            &state.queue,
            &state.set_command,
            &state.preview,
            &state.action,
            &state.render_footprints,
            &state.footprint,
            &state.can_place,
            &state.placement_clear,
            &state.build,
        ] {
            assert!(hook.target.is_null() && hook.gateway.is_null());
            assert_eq!(hook.length, 0);
        }
        assert!(
            state.sending_class.is_null()
                && state.build_process.is_null()
                && state.placement_class.is_null()
        );
        assert_eq!(
            (
                state.sending_turns,
                state.sending_activation,
                state.previous_activation
            ),
            (0, 0, 0)
        );
        assert_eq!((state.placement_turns, state.ui_footprint_depth), (0, 0));
    }
}
