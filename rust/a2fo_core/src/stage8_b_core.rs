//! Main-hook Mods and graphics options portable admission/workflow policies.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
type IndexedOp = unsafe extern "C" fn(*mut c_void, u32, u64) -> u64;
type IsSpace = unsafe extern "C" fn(u8) -> u8;
const ERROR: u64 = u64::MAX;
fn call(c: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let value = unsafe { op(c, id) };
    (value != ERROR).then_some(value)
}
fn call_indexed(c: *mut c_void, op: IndexedOp, id: u32, index: u64) -> Option<u64> {
    let value = unsafe { op(c, id, index) };
    (value != ERROR).then_some(value)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_has_switch(
    lower: *const u8,
    lower_len: usize,
    token: *const u8,
    token_len: usize,
    is_space: IsSpace,
) -> u8 {
    if lower_len == 0 || token_len == 0 || lower.is_null() || token.is_null() {
        return 0;
    }
    let command = unsafe { core::slice::from_raw_parts(lower, lower_len) };
    let target = unsafe { core::slice::from_raw_parts(token, token_len) };
    if token_len > lower_len {
        return 0;
    }
    for position in 0..=lower_len - token_len {
        if &command[position..position + token_len] != target {
            continue;
        }
        let start_ok = position == 0 || unsafe { is_space(command[position - 1]) } != 0;
        let end = position + token_len;
        let end_ok = end == lower_len || unsafe { is_space(command[end]) } != 0;
        if start_ok && end_ok {
            return 1;
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_append_switches(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(500) != 0 && step!(501) == 0 {
        step!(502);
    }
    if step!(503) != 0 && step!(504) == 0 {
        step!(505);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_selected_mod(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(600);
    step!(601);
    if step!(602) == 0 || step!(603) == 0 {
        return 0;
    }
    step!(604);
    if step!(605) == 0 || step!(606) == 0 {
        return 0;
    }
    let index = step!(607) as u32 as i32;
    if !(0..=100000).contains(&index) {
        return 0;
    }
    step!(608);
    if step!(609) == 0 || step!(610) == 0 {
        return 0;
    }
    step!(611);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_resolve_mod(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(620) == 0 {
        step!(621);
        return 0;
    }
    if step!(622) != 0 {
        step!(623);
    } else {
        step!(624);
        step!(625);
        if step!(626) != 0 {
            step!(627);
        }
        if step!(628) == 0 {
            step!(629);
            return 0;
        }
        step!(630);
        step!(631);
        if step!(632) != 0 || step!(633) == 0 {
            step!(634);
            return 0;
        }
    }
    step!(635);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_dialog(c: *mut c_void, op: IndexedOp) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call_indexed(c, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(700) == 0 {
        step!(701);
        return;
    }
    for id in 702..=706 {
        step!(id);
    }
    let count = step!(707);
    for index in 0..count {
        if step!(708, index) != 0 {
            step!(709);
        }
    }
    step!(710);
    if step!(711) == 0 {
        step!(712);
        return;
    }
    for id in 713..=716 {
        step!(id);
    }
    if step!(717) == 0 {
        step!(718);
        return;
    }
    for id in 719..=722 {
        step!(id);
    }
    while step!(723) != 0 {
        if step!(724) as u32 as i32 <= 0 {
            break;
        }
        if step!(725) == 0 {
            step!(726);
            step!(727);
        }
    }
    step!(728);
    step!(729);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_import_for(
    c: *mut c_void,
    op: IndexedOp,
) -> u32 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call_indexed(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(800) == 0 {
        return 0;
    }
    let mut hooked = 0;
    for index in 0..7 {
        if step!(801, index) != 0 {
            hooked += 1;
            step!(802, index);
        }
    }
    hooked
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_import_all(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    let fleet = step!(810) as u32;
    step!(811);
    let armada = if step!(812) != 0 && step!(813) != 0 {
        step!(814) as u32
    } else {
        0
    };
    step!(815);
    u8::from(fleet + armada != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_launch_intercept(
    c: *mut c_void,
    op: Op,
    kind: u32,
) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if (kind == 4 || kind == 5) && step!(940) == 0 {
        step!(902);
        return;
    }
    if step!(901) != 0 {
        step!(902);
        return;
    }
    match kind {
        0 | 1 => {
            step!(910);
            if step!(911) != 0 && step!(912) != 0 {
                step!(913);
            }
            step!(914);
            step!(915);
            step!(916);
            if kind == 0 {
                step!(917);
            }
            step!(918);
            step!(919);
            if step!(920) != 0 {
                step!(921);
                step!(922);
            }
        }
        2 | 3 => {
            step!(930);
            if kind == 2 {
                step!(931);
            }
            step!(932);
            step!(933);
            if step!(934) != 0 {
                step!(935);
                step!(922);
            }
        }
        4 | 5 => {
            step!(941);
            step!(942);
            if kind == 4 {
                step!(943);
            }
            step!(944);
            step!(945);
            step!(946);
            step!(947);
            if step!(948) != 0 {
                step!(949);
                step!(922);
            }
        }
        6 => {
            for id in 950..=954 {
                step!(id);
            }
            if step!(955) != 0 {
                step!(956);
                step!(922);
            }
        }
        _ => {}
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_dialog_message(
    c: *mut c_void,
    op: Op,
) -> u64 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(1000); // native user data
    if step!(1001) != 0 {
        step!(1002);
    }
    if step!(1003) == 0 {
        return step!(1004);
    }
    if step!(1005) != 0 {
        step!(1006);
        return 0;
    }
    if step!(1007) != 0 {
        if step!(1008) != 0 && step!(1009) != 0 {
            if step!(1010) != 0 {
                step!(1011);
            }
            return 0;
        }
        if step!(1012) != 0 && step!(1009) != 0 {
            step!(1011);
            return 0;
        }
        return step!(1020);
    }
    if step!(1013) != 0 {
        return step!(1014);
    }
    if step!(1015) != 0 {
        step!(1016);
        return step!(1017);
    }
    if step!(1018) != 0 {
        return step!(1019);
    }
    if step!(1021) != 0 {
        step!(1011);
        return 0;
    }
    if step!(1022) != 0 {
        step!(1023);
        return 0;
    }
    step!(1020)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_module_menu_button_left(left: i32, width: i32) -> i32 {
    8.max(left.wrapping_sub(width).wrapping_sub(18))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_install(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(0) == 0 {
        return 0;
    }
    step!(1); // publish native image, root, and logger before signatures
    for id in 2..=19 {
        if step!(id) == 0 {
            step!(20);
            return 0;
        }
    }
    // All three checked hooks are attempted before the shared admission gate.
    let form = step!(21);
    let launch = step!(22);
    let click = step!(23);
    if form == 0 || launch == 0 || click == 0 {
        step!(24);
        return 0;
    }
    if step!(25) == 0 {
        step!(26);
    }
    step!(27);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_button(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(30); // read window before Launch field
    step!(31);
    if step!(32) == 0 {
        step!(33);
        return;
    }
    if step!(34) == 0 || step!(35) == 0 {
        step!(36);
        return;
    }
    let left = step!(37) as u32 as i32;
    let top = step!(38) as u32 as i32;
    let width = step!(39) as u32 as i32;
    let height = step!(40) as u32 as i32;
    if left < 0 || top < 0 || width <= 0 || height <= 0 || width > 4096 || height > 4096 {
        step!(41);
        return;
    }
    step!(42); // native calculated left
    let create = step!(43) == 0 || step!(44) == 0 || step!(45) == 0;
    if create {
        step!(46); // class field
        if step!(47) == 0 || step!(48) == 0 {
            step!(49);
            return;
        }
        step!(50); // constructor
        if step!(51) == 0 || step!(52) == 0 {
            step!(53);
            return;
        }
        for id in 54..=56 {
            step!(id);
        }
    }
    for id in 57..=64 {
        step!(id);
    }
    if step!(66) == 0 {
        step!(67);
    }
    step!(68);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_launch_admit(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(80) == 0 {
        step!(81);
        step!(82);
        return 0;
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_launch_commit(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    let nointro = step!(90) != 0;
    let noai = step!(91) != 0;
    if nointro || noai {
        step!(92);
    }
    step!(93);
    step!(94);
    step!(95);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_click(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(100) != 0 && step!(101) != 0 {
        step!(102);
        return;
    }
    if step!(103) != 0 && step!(104) != 0 {
        for id in 105..=107 {
            step!(id);
        }
        return;
    }
    if step!(108) != 0 && step!(109) != 0 {
        for id in 110..=112 {
            step!(id);
        }
        return;
    }
    if step!(113) != 0 && step!(114) != 0 {
        step!(115); // nointro, then noai and selection are read after native state setup
        step!(116);
        let resolved = step!(117) != 0;
        step!(118); // timestamp
        step!(119); // Fleet Ops creates its shortcut
        if resolved && (step!(120) != 0 || step!(121) != 0) {
            let updated = step!(122);
            if updated == 1 {
                step!(123);
            } else if updated == 0 {
                step!(124);
            } else {
                step!(126);
            }
        }
        return;
    }
    step!(125);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_save(c: *mut c_void, op: IndexedOp) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call_indexed(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    step!(200); // reporting root
    if step!(201) != 0 {
        step!(202);
    }
    let reporting_enabled = step!(203) != 0 && step!(204) != 0;
    let all_sessions = step!(205) != 0 && step!(206) != 0;
    let mut index = 0;
    while index < step!(207) {
        if step!(208, index) != 0 && step!(209, index) == 0 && step!(210, index) != 0 {
            step!(211, index);
        }
        index += 1;
    }
    step!(220); // acquire native lock
    step!(221); // read previous config under lock
    let reporting_changed = step!(222) != 0;
    let mut saved = false;
    if step!(223) == 0 {
        step!(224);
    } else if reporting_enabled && step!(225) != 0 {
        step!(226);
    } else if reporting_enabled && step!(227) == 0 {
        step!(228);
    } else if reporting_changed
        && (step!(229) == 0
            || step!(
                230,
                u64::from(reporting_enabled) | (u64::from(all_sessions) << 1)
            ) == 0)
    {
        step!(231);
    } else {
        saved = step!(232) != 0;
        if !saved && reporting_changed && step!(233) == 0 {
            step!(234);
        }
    }
    step!(235); // native lock release before any message box
    if !saved {
        step!(236);
        return 0;
    }
    step!(237);
    step!(238);
    step!(239);
    step!(240);
    if step!(241) == 0 {
        step!(242);
        step!(243);
    } else if step!(244) != 0 {
        step!(245);
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_module_menu_flag_row(
    modules_left: i32,
    modules_top: i32,
    button_width: i32,
    button_height: i32,
    row_left: &mut i32,
    row_top: &mut i32,
) {
    if modules_left >= 156 {
        *row_left = modules_left - 150;
        *row_top = modules_top + 0.max((button_height - 18) / 2);
    } else {
        *row_left = 8.max(modules_left + 0.max((button_width - 142) / 2));
        *row_top = 4.max(modules_top - 23);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_module_menu_toggle_row(
    modules_left: i32,
    modules_top: i32,
    button_width: i32,
    button_height: i32,
    row_left: &mut i32,
    row_top: &mut i32,
) {
    *row_left = modules_left.wrapping_sub(2 * button_width).wrapping_sub(36);
    *row_top = modules_top;
    if *row_left < 8 {
        *row_left = 8.max(modules_left);
        *row_top = 4.max(modules_top - button_height - 6);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_flags(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(300); // row geometry
    if step!(301) == 0 {
        step!(302);
        if step!(303) != 0 {
            step!(304);
        }
    }
    if step!(305) != 0 {
        if step!(306) != 0 && step!(307) != 0 && step!(308) != 0 {
            for id in 309..=313 {
                step!(id);
            }
            return;
        }
        for id in 314..=316 {
            step!(id);
        }
        if step!(317) != 0 && step!(318) != 0 {
            for id in 319..=322 {
                step!(id);
            }
            return;
        }
        for id in 324..=328 {
            step!(id);
        }
    } else {
        step!(329);
    }
    step!(330); // fallback row geometry
    if step!(331) != 0 {
        step!(332);
    } else {
        step!(333);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_jet_flags(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(340) == 0 || step!(341) == 0 || step!(342) == 0 {
        return 0;
    }
    if step!(343) != 0 && step!(344) != 0 && step!(345) != 0 {
        for id in 346..=353 {
            step!(id);
        }
        return 1;
    }
    for id in 354..=356 {
        step!(id);
    }
    if step!(357) == 0 || step!(358) == 0 {
        if step!(359) != 0 {
            step!(360);
        }
        if step!(361) != 0 {
            step!(362);
        }
        for id in 363..=365 {
            step!(id);
        }
        return 0;
    }
    step!(366);
    step!(367);
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_module_menu_state_locked(state: u32) -> u8 {
    u8::from(matches!(state, 2..=6))
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_module_menu_state_checked(state: u32) -> u8 {
    u8::from(matches!(state, 1 | 2))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_module_menu_notification(
    c: *mut c_void,
    op: IndexedOp,
) -> u64 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call_indexed(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(400) == 0 || step!(401) == 0 {
        return 0;
    }
    if step!(402) != 0 && step!(403) != 0 {
        let index = step!(404) as u32 as i32;
        if index >= 0 && (index as u64) < step!(405) {
            let old_image = step!(406);
            let new_image = step!(407);
            if old_image != new_image
                && a2fo_rs_stage8_b_module_menu_state_locked(step!(408) as u32) != 0
            {
                return step!(409);
            }
        }
    } else if step!(410) != 0 {
        if step!(411) != 0 {
            return step!(412);
        }
        if step!(413) != 0 {
            let index = step!(414);
            if index < step!(415) {
                let state = step!(416) as u32;
                let color = if state == 4 || state == 6 {
                    0
                } else if state == 2 {
                    1
                } else if a2fo_rs_stage8_b_module_menu_state_locked(state) != 0 {
                    2
                } else {
                    3
                };
                step!(417, color);
                step!(418);
            }
            return step!(419);
        }
    }
    0
}
