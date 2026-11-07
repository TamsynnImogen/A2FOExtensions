//! A1 Instant Action relationship traversal. Native callbacks retain SDK
//! calls, side ownership, atomics and stream/log formatting.
use core::ffi::c_void;

type Op = unsafe extern "C" fn(*mut c_void, u32, u64, u64, u64, u64, u64) -> u64;
const ERROR: u64 = u64::MAX;

unsafe fn call(ctx: *mut c_void, op: Op, id: u32, args: [u64; 5]) -> Option<u64> {
    let value = unsafe { op(ctx, id, args[0], args[1], args[2], args[3], args[4]) };
    (value != ERROR).then_some(value)
}

/// Helper validation and entry admission occur natively before this call.
/// Return 1 only after all native writes and the publication/log phase succeed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_relationship_restore(ctx: *mut c_void, op: Op) -> u8 {
    let mut team = [0_i32; 9];
    let mut active_side = [false; 9];
    for (side, value) in team.iter_mut().enumerate() {
        *value = side as i32 + 9;
    }
    let mut active_count = 0_u64;
    if unsafe { call(ctx, op, 15, [0; 5]) }.is_none() {
        return 0;
    }
    for slot in 0..8_u64 {
        let Some(active) = (unsafe { call(ctx, op, 0, [slot, 0, 0, 0, 0]) }) else {
            return 0;
        };
        if active == 0 {
            continue;
        }
        let Some(side) = (unsafe { call(ctx, op, 1, [slot, 0, 0, 0, 0]) }) else {
            return 0;
        };
        let Some(current_team) = (unsafe { call(ctx, op, 2, [slot, 0, 0, 0, 0]) }) else {
            return 0;
        };
        let side = side as u32 as i32;
        if !(1..9).contains(&side) {
            continue;
        }
        let current_team = current_team as u32 as i32;
        team[side as usize] = current_team;
        active_side[side as usize] = true;
        active_count += 1;
        if unsafe {
            call(
                ctx,
                op,
                3,
                [
                    slot,
                    side as u32 as u64,
                    current_team as u32 as u64,
                    active_count,
                    0,
                ],
            )
        }
        .is_none()
        {
            return 0;
        }
    }
    if active_count == 0 {
        let _ = unsafe { call(ctx, op, 4, [0; 5]) };
        return 0;
    }
    let mut present = [false; 9];
    for side in 0..9_u64 {
        let Some(value) = (unsafe { call(ctx, op, 5, [side, 0, 0, 0, 0]) }) else {
            return 0;
        };
        present[side as usize] = value != 0;
    }
    if unsafe { call(ctx, op, 14, [0; 5]) }.is_none() {
        return 0;
    }
    let mut pair_count = 0_u64;
    for side in 0..9_u64 {
        if !present[side as usize] {
            continue;
        }
        for other in 0..9_u64 {
            let relationship = if side == 0 || other == 0 {
                1
            } else if team[side as usize] == team[other as usize] {
                2
            } else {
                0
            };
            let Some(previous) = (unsafe { call(ctx, op, 6, [side, other, relationship, 0, 0]) })
            else {
                return 0;
            };
            if side < other && active_side[side as usize] && active_side[other as usize] {
                pair_count += 1;
                if unsafe {
                    call(
                        ctx,
                        op,
                        7,
                        [side, other, previous, relationship, pair_count],
                    )
                }
                .is_none()
                {
                    return 0;
                }
            }
        }
    }
    if unsafe { call(ctx, op, 8, [0; 5]) }.is_none() {
        return 0;
    }
    if unsafe { call(ctx, op, 9, [active_count, pair_count, 0, 0, 0]) }.is_none() {
        return 0;
    }
    1
}

/// Native operations implement the original lazy transport pointer reads and
/// get-game-setup call. The final operation invokes the restore entrypoint.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_a1_relationships_before_elimination(
    ctx: *mut c_void,
    op: Op,
) {
    for id in 10..=13 {
        let Some(result) = (unsafe { call(ctx, op, id, [0; 5]) }) else {
            return;
        };
        if result == 0 {
            return;
        }
    }
}
