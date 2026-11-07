//! Hybrid menu, queue preview and construction-event policy. Native storage
//! and game calls remain behind synchronous callbacks.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> u64;
fn call(step: Step, context: *mut c_void, op: u32, index: usize) -> Result<u64, ()> {
    let value = unsafe { step(context, op, index) };
    if value == u64::MAX {
        Err(())
    } else {
        Ok(value)
    }
}
fn run(kind: u32, context: *mut c_void, step: Step) -> Result<u64, ()> {
    match kind {
        // Selection membership. Native callback snapshots selected/count once.
        0 => {
            if call(step, context, 0, 0)? == 0 || call(step, context, 1, 0)? == 0 {
                return Ok(0);
            }
            call(step, context, 2, 0)?;
            let count = call(step, context, 3, 0)? as usize;
            if call(step, context, 4, count)? == 0 {
                return Ok(0);
            }
            for index in 0..count {
                if call(step, context, 5, index)? != 0 {
                    return Ok(1);
                }
            }
            Ok(0)
        }
        // BuildWireframe queue: active item first, then native FIFO, omitting
        // only the active ID's duplicate and preserving the native cursor.
        1 => {
            call(step, context, 0, 0)?;
            if call(step, context, 1, 0)? == 0 {
                return Ok(0);
            }
            let active = call(step, context, 2, 0)? != 0;
            call(step, context, 3, 0)?;
            let mut populated = 0usize;
            if active {
                call(step, context, 4, 0)?;
                populated = 1;
            }
            call(step, context, 5, 0)?;
            while populated < 10 && call(step, context, 6, 0)? != 0 {
                if call(step, context, 7, 0)? == 0 {
                    break;
                }
                if call(step, context, 8, populated)? != 0 {
                    populated += 1;
                }
                call(step, context, 9, 0)?;
            }
            Ok(populated as u64)
        }
        // Research conflict: active class, then at most ten queue nodes.
        2 => {
            if call(step, context, 0, 0)? == 0
                || call(step, context, 1, 0)? == 0
                || call(step, context, 2, 0)? == 0
            {
                return Ok(0);
            }
            call(step, context, 3, 0)?;
            if call(step, context, 4, 0)? != 0 {
                return Ok(1);
            }
            call(step, context, 5, 0)?;
            for _ in 0..10 {
                if call(step, context, 6, 0)? == 0 || call(step, context, 7, 0)? == 0 {
                    break;
                }
                call(step, context, 8, 0)?;
                if call(step, context, 4, 0)? != 0 {
                    return Ok(1);
                }
                call(step, context, 9, 0)?;
            }
            Ok(0)
        }
        // Premature construction button order is consumed until map placement.
        3 => {
            if call(step, context, 0, 0)? == 0
                || call(step, context, 1, 0)? == 0
                || call(step, context, 2, 0)? == 0
            {
                return Ok(0);
            }
            call(step, context, 3, 0)
        }
        // Research menu retention markers after an order.
        4 => {
            if call(step, context, 0, 0)? == 0 {
                return Ok(0);
            }
            if call(step, context, 1, 0)? != 0 {
                call(step, context, 2, 0)?;
            }
            call(step, context, 3, 0)?;
            call(step, context, 4, 0)?;
            call(step, context, 5, 0)?;
            if call(step, context, 6, 0)? == 0 {
                call(step, context, 7, 0)?;
            }
            Ok(0)
        }
        // Menu-specific list selection; native callback owns Producer slot and refresh.
        5 => {
            if call(step, context, 0, 0)? == 0 || call(step, context, 1, 0)? == 0 {
                return Ok(0);
            }
            call(step, context, 2, 0)?;
            if call(step, context, 3, 0)? == 0 {
                return Ok(0);
            }
            let menu = call(step, context, 4, 0)?;
            if menu == 2 && (call(step, context, 5, 0)? != 0 || call(step, context, 6, 0)? != 0) {
                let has_construct = call(step, context, 6, 0)? != 0;
                let construct = has_construct
                    && (call(step, context, 7, 0)? != 0 || call(step, context, 5, 0)? == 0);
                call(step, context, 8, usize::from(construct))?;
                return Ok(0);
            }
            if menu == 4 && call(step, context, 9, 0)? != 0 {
                call(step, context, 10, 0)?;
                return Ok(0);
            }
            if menu != 3 {
                return Ok(0);
            }
            if call(step, context, 11, 0)? != 0 {
                call(step, context, 12, 0)?;
            }
            Ok(0)
        }
        // Selected stations: sample count once, reread each selected slot at dispatch.
        6 => {
            let menu = call(step, context, 0, 0)?;
            if menu != 2 && menu != 3 && menu != 4 {
                return Ok(0);
            }
            if call(step, context, 1, 0)? == 0 {
                return Ok(0);
            }
            call(step, context, 2, 0)?;
            let count = call(step, context, 3, 0)? as usize;
            if call(step, context, 4, count)? == 0 {
                return Ok(0);
            }
            for index in 0..count {
                call(step, context, 5, index)?;
            }
            Ok(0)
        }
        // Synchronous submenu refresh, including the inherited root retry.
        7 => {
            for op in 0..5 {
                if call(step, context, op, 0)? == 0 {
                    return Ok(0);
                }
            }
            call(step, context, 5, 0)?;
            call(step, context, 6, 0)?;
            if call(step, context, 7, 0)? != 0 {
                call(step, context, 5, 0)?;
                call(step, context, 6, 0)?;
            }
            call(step, context, 5, 0)?;
            call(step, context, 8, 0)?;
            Ok(0)
        }
        // Parent item opens a child page; the non-item control backs out.
        8 => {
            if call(step, context, 0, 0)? == 0 || call(step, context, 1, 0)? == 0 {
                return Ok(0);
            }
            call(step, context, 2, 0)?;
            if call(step, context, 3, 0)? == 0 {
                return Ok(0);
            }
            call(step, context, 4, 0)?;
            if call(step, context, 5, 0)? != 0 {
                call(step, context, 6, 0)?;
                call(step, context, 7, 0)?;
                call(step, context, 8, 0)?;
                return Ok(1);
            }
            if call(step, context, 9, 0)? != 0 && call(step, context, 10, 0)? != 0 {
                call(step, context, 11, 0)?;
                call(step, context, 7, 0)?;
                call(step, context, 12, 0)?;
                return Ok(1);
            }
            Ok(0)
        }
        // Root palette button chooses construction before yard, with native
        // pointer slots sampled in the original order.
        9 => {
            for op in 0..4 {
                if call(step, context, op, 0)? == 0 {
                    return Ok(0);
                }
            }
            call(step, context, 4, 0)?;
            if call(step, context, 5, 0)? == 0 {
                return Ok(0);
            }
            call(step, context, 6, 0)?;
            if call(step, context, 7, 0)? != 0 && call(step, context, 8, 0)? != 0 {
                call(step, context, 9, 0)?;
                return Ok(0);
            }
            call(step, context, 10, 0)?;
            if call(step, context, 11, 0)? != 0 && call(step, context, 12, 0)? != 0 {
                call(step, context, 13, 0)?;
            }
            Ok(0)
        }
        // Probe each basename byte before extending the research hover sprite.
        10 => {
            call(step, context, 0, 0)?;
            if call(step, context, 1, 0)? == 0 || call(step, context, 2, 0)? == 0 {
                return Ok(0);
            }
            if call(step, context, 3, 0)? == 0 {
                return Ok(0);
            }
            for index in 0..257 {
                let byte = call(step, context, 4, index)?;
                if byte == 256 {
                    return Ok(0);
                }
                if byte == 0 {
                    call(step, context, 5, index)?;
                    return Ok(u64::from(index != 0));
                }
            }
            Ok(0)
        }
        // Mark live submenu parent controls after a root page refresh. Native
        // vector iterators retain the original range-for cursor semantics.
        11 => {
            if call(step, context, 0, 0)? == 0
                || call(step, context, 1, 0)? == 0
                || call(step, context, 2, 0)? == 0
            {
                return Ok(0);
            }
            call(step, context, 3, 0)?;
            if call(step, context, 4, 0)? == 0 || call(step, context, 5, 0)? == 0 {
                return Ok(0);
            }
            call(step, context, 6, 0)?;
            while call(step, context, 7, 0)? != 0 {
                for index in 0..64 {
                    call(step, context, 8, index)?;
                }
                call(step, context, 9, 0)?;
            }
            Ok(0)
        }
        // Disable conflicting research pod controls after Fleet Ops binds them.
        12 => {
            if call(step, context, 0, 0)? == 0
                || call(step, context, 1, 0)? == 0
                || call(step, context, 2, 0)? == 0
            {
                return Ok(0);
            }
            call(step, context, 3, 0)?;
            if call(step, context, 4, 0)? == 0 {
                return Ok(0);
            }
            let mut disabled = false;
            for index in 0..14 {
                if call(step, context, 5, index)? == 0 {
                    continue;
                }
                call(step, context, 6, index)?;
                call(step, context, 7, 0)?;
                for button in 0..64 {
                    call(step, context, 8, button)?;
                }
                disabled = true;
            }
            if disabled {
                call(step, context, 9, 0)?;
            }
            Ok(0)
        }
        _ => Err(()),
    }
}
/// Safety: native callback owns all foreign pointers, storage and exception
/// capture. No Rust reference spans a callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_hybrid_runtime(
    kind: u32,
    context: *mut c_void,
    step: Step,
) -> u64 {
    run(kind, context, step).unwrap_or(u64::MAX)
}
