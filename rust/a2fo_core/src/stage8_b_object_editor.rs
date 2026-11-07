//! Per-ship editor save, admission, lifecycle and dialog workflow decisions.
//! Native callbacks retain SDK/Delphi ABI, Win32 controls, owner maps and errors.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
const ERROR: u64 = u64::MAX;
fn call(context: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let value = unsafe { op(context, id) };
    (value != ERROR).then_some(value)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_initialize(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(0) != 0 {
        return 1;
    }
    for id in 1..=3 {
        step!(id);
    }
    for id in 4..=13 {
        if step!(id) == 0 {
            step!(14);
            return 0;
        }
    }
    if step!(15) == 0 {
        step!(16);
        return 0;
    }
    let save = step!(17) != 0;
    let load = save && step!(18) != 0;
    step!(19); // native ready publication from both patch results
    if !load {
        step!(20);
        return 0;
    }
    let legacy = step!(21) != 0
        && step!(22) != 0
        && step!(23) != 0
        && step!(24) != 0
        && step!(25) != 0
        && step!(26) != 0;
    if legacy {
        step!(27);
    } else {
        step!(28);
    }
    step!(29); // Fleet Ops dialog bootstrap
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_read_field(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(40); // native original pointer before ready gate
    if step!(41) == 0 {
        return u8::from(step!(42) != 0);
    }
    step!(43); // destination address
    if step!(44) == 0 || step!(45) == 0 || step!(46) == 0 {
        return 0;
    }
    step!(47); // craft address
    if step!(48) == 0 {
        return 0;
    }
    step!(49); // stale entry erase before reader cursor validation
    for id in 50..=53 {
        step!(id);
    }
    if step!(54) == 0 || step!(55) == 0 {
        return 0;
    }
    step!(56); // bounded available bytes
    if step!(57) == 0 {
        return 0;
    }
    for id in 58..=60 {
        step!(id);
    }
    let result = step!(61);
    if result == 2 {
        step!(62);
        return 0;
    }
    if result == 1 {
        if step!(63) == 0 {
            return 0;
        }
        step!(64);
        step!(65);
    }
    if step!(66) == 0 {
        step!(67);
        return 0;
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_event(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(80) == 0 || step!(81) == 0 || step!(82) == 0 {
        return;
    }
    if step!(83) != 0 {
        step!(84);
        if step!(85) != 0 {
            step!(86);
        }
        return;
    }
    if step!(87) == 0 || step!(88) == 0 {
        return;
    }
    if step!(89) != 0 {
        step!(90);
        step!(91);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_write_field(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(100); // native original function
    if step!(101) != 0 {
        step!(102);
        if step!(103) == 0 || step!(104) == 0 {
            return 0;
        }
        if step!(105) != 0 {
            step!(106);
            if step!(107) == 0 {
                return 0;
            }
        }
    }
    u8::from(step!(108) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_find_entry(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(120) == 0 {
        return 0;
    }
    if step!(121) == 0 {
        step!(124);
        return 0;
    }
    if step!(122) == 0 && step!(123) == 0 {
        step!(124);
        return 0;
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_prepare_entry(
    c: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(130) != 0 {
        return 1;
    }
    if step!(131) == 0 {
        return 0;
    }
    step!(132);
    step!(133);
    u8::from(step!(134) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_assign_entry(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(140);
    if step!(141) == 0 {
        step!(142);
    }
    step!(143);
    step!(144);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_resolve_shields(
    c: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(150) != 0 && step!(151) != 0 && step!(152) != 0 {
        return 1;
    }
    for id in 153..=156 {
        step!(id);
    }
    u8::from(step!(157) != 0 && step!(158) != 0 && step!(159) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_make_save_state(
    c: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(160);
    if step!(161) != 0 {
        step!(162);
    }
    if step!(163) != 0 && step!(164) != 0 {
        step!(165);
        if step!(166) == 0 {
            return 0;
        }
        step!(167);
    }
    u8::from(step!(168) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_read_values(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 180..=184 {
        if step!(id) == 0 {
            return 0;
        }
    }
    let shields = step!(185) != 0;
    if shields {
        step!(186);
    }
    step!(187); // publish directional after SDK query
    step!(188); // defaults initialize
    if step!(189) != 0 {
        step!(190);
        step!(191);
    }
    if step!(192) == 0 {
        step!(193);
    }
    if step!(194) == 0 {
        step!(195);
    }
    u8::from(step!(196) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_apply_values(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 200..=203 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(204) != 0 {
        if step!(205) == 0 || step!(206) == 0 {
            return 0;
        }
        step!(207);
        if step!(208) == 0 {
            return 0;
        }
    }
    step!(209);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_take_loaded(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 220..=222 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(223) == 0 || step!(224) == 0 || step!(225) == 0 {
        return 0;
    }
    step!(226);
    step!(227);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_read_shield_controls(
    c: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for index in 0..4 {
        step!(240 + index);
        step!(244); // current control text
        step!(245); // maximum control text, before either parse
        if step!(246) == 0 || step!(247) == 0 {
            return 0;
        }
    }
    step!(248); // commit kShields only after four parsed facings
    u8::from(step!(249) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_undo_controls(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(250); // deactivate before destroying children and reentrant messages
    for index in 0u32.. {
        if u64::from(index) >= step!(251) {
            break;
        } // re-read owner control count
        step!(0x10000 + index); // publish current index
        if step!(256) != 0 {
            step!(257);
        }
    }
    if step!(258) != 0 {
        step!(259);
        step!(260);
    }
    step!(261); // native geometry restoration
    step!(262); // clear owner after restore
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_accept_controls(
    c: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(300);
    if step!(301) == 0 {
        step!(302);
        return 0;
    }
    step!(303);
    if step!(304) != 0 {
        step!(305);
    }
    for id in 306..=307 {
        step!(id);
    }
    if step!(308) != 0 {
        step!(309);
        step!(310);
    }
    if step!(311) != 0 {
        step!(312);
        step!(313);
    }
    if step!(314) != 0 && step!(315) == 0 {
        step!(316);
        return 0;
    }
    if step!(317) == 0 {
        step!(318);
        return 0;
    }
    step!(319); // snapshot even without active shields
    if step!(320) != 0 && (step!(321) == 0 || step!(322) == 0) {
        step!(323);
        return 0;
    }
    for id in 324..=327 {
        step!(id);
    }
    if step!(328) != 0 {
        step!(329);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_dialog_proc(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(340); // native gateway before readiness
    if step!(341) == 0 {
        return;
    }
    if step!(342) == 0 || step!(343) == 0 {
        step!(344);
        return;
    }
    if step!(345) != 0 {
        step!(344);
        step!(346);
        return;
    }
    if step!(347) != 0 && step!(348) != 0 {
        step!(349);
        step!(350);
        if step!(351) != 0 {
            if step!(352) == 0 || step!(353) != 0 {
                step!(381);
                return;
            }
            if step!(354) != 0 && (step!(355) != 0 || step!(356) != 0) {
                step!(357);
            } else if step!(358) != 0 && (step!(359) != 0 || step!(360) != 0) && step!(361) != 0 {
                step!(362);
                step!(363);
                step!(364);
            } else if step!(365) != 0 && step!(354) != 0 && step!(366) != 0 && step!(367) != 0 {
                step!(368);
            }
            step!(381);
            return;
        }
        if step!(369) != 0 && step!(370) != 0 && step!(371) == 0 {
            step!(381);
            return;
        }
        if step!(372) != 0 {
            step!(373);
        }
    }
    if step!(374) != 0 && step!(375) != 0 {
        step!(376);
    }
    step!(377);
    if step!(378) != 0 && step!(379) != 0 {
        step!(380);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_dialog_destructor(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(400) == 0 {
        return;
    }
    let accepted = step!(401) != 0
        && step!(402) != 0
        && step!(403) != 0
        && step!(404) != 0
        && step!(405) != 0
        && step!(406) != 0;
    step!(407); // native thiscall destructor after admission snapshot
    if accepted && step!(408) != 0 && step!(409) != 0 && step!(410) == 0 {
        step!(411);
    }
    step!(412);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_object_editor_add_controls(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(500);
    step!(501);
    step!(502);
    if step!(503) == 0 {
        return 0;
    }
    step!(504);
    if step!(505) == 0 || step!(506) == 0 || step!(507) == 0 {
        step!(514);
        return 0;
    }
    step!(508);
    step!(509);
    if step!(510) != 0 {
        step!(511);
    }
    step!(512);
    if step!(513) != 0 {
        step!(514);
        step!(515);
        return 0;
    }
    step!(516);
    if step!(517) == 0 {
        step!(514);
        return 0;
    }
    step!(519);
    for id in 520..=530 {
        step!(id);
    }
    for facing in 0..4 {
        step!(535 + facing); // publish native facing index
        for id in 539..=541 {
            step!(id);
        } // label, current, maximum
    }
    step!(550);
    if step!(551) != 0 {
        step!(552);
    }
    if step!(553) != 0 {
        step!(554);
        step!(555);
    }
    for id in 556..=560 {
        step!(id);
    }
    let shields = step!(561) != 0 && step!(562) != 0 && step!(563) != 0;
    step!(if shields { 564 } else { 565 }); // publish after all shield callbacks
    for facing in 0..4 {
        step!(566 + facing);
        step!(570); // both HWND lookups
        if step!(571) != 0 {
            step!(572);
            step!(573);
        }
        step!(574); // active facing from live shield owner and maximum
        step!(575);
        step!(576); // enable both controls
    }
    step!(577); // explanatory label
    if step!(578) != 0 {
        for id in 579..=582 {
            step!(id);
        }
        if step!(583) == 0 || step!(584) == 0 || step!(585) == 0 || step!(586) == 0 {
            step!(587);
        }
    }
    if step!(588) == 0 {
        step!(589);
        step!(590);
        return 0;
    }
    if step!(591) != 0 {
        step!(592);
        step!(593);
    }
    step!(594);
    step!(595);
    1
}
