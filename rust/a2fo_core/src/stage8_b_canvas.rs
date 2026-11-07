//! Shell canvas registration, patch admission, ownership and draw workflow.
//! Native callbacks retain Win32/GDI, VCL ABI, TLS containers and resources.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
fn call(c: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let value = unsafe { op(c, id) };
    (value != u64::MAX).then_some(value)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_initialize(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(0); // original publishes module pointers before API admission
    for id in 1..=4 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(5); // raw patch site addresses
    for id in 6..=8 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(9) == 0 {
        return 0;
    }
    step!(10); // native import vector with GetProcAddress read order
    if step!(11) == 0 || step!(12) == 0 {
        return 0;
    }
    let imports = step!(13);
    for index in 0..imports {
        step!(0x10000 + index as u32);
        if step!(14) == 0 || step!(15) == 0 {
            return 0;
        }
    }
    if step!(16) == 0 {
        return 0;
    }
    step!(17); // publish first gateway before next hook
    if step!(18) == 0 {
        return 0;
    }
    step!(19);
    if step!(20) == 0 {
        return 0;
    }
    let patches = step!(21);
    for index in 0..patches {
        step!(0x20000 + index as u32);
        if step!(22) == 0 {
            return 0;
        }
    }
    step!(23);
    step!(24);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_active_mode(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(40); // mode copy before readiness/validity
    if step!(41) == 0 || step!(42) == 0 {
        return;
    }
    step!(43); // owner root snapshot before any window movement
    let roots = step!(44);
    for index in 0..roots {
        step!(0x30000 + index as u32);
        step!(45); // output scale
        step!(46); // propagate to all tracked children
        step!(47); // native bounds
        step!(48); // MoveWindow
        step!(49); // fit children
        step!(50); // invalidate
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_shutdown(c: *mut c_void, op: Op) {
    let _ = call(c, op, 60);
    let _ = call(c, op, 61);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_copy(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(70) == 0 || step!(71) == 0 {
        return 0;
    }
    step!(72);
    if step!(73) == 0 {
        return 0;
    }
    step!(74);
    if step!(75) == 0 {
        return 0;
    }
    step!(76);
    if step!(77) == 0 {
        return 0;
    }
    step!(78);
    if step!(79) == 0 {
        return 1;
    }
    for id in 80..=83 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(84);
    if step!(85) == 0 {
        return 0;
    }
    step!(86);
    for id in 87..=89 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(90) == 0 {
        return 0;
    }
    step!(91);
    step!(92);
    step!(93);
    if step!(94) != 0 && step!(95) != 0 {
        step!(96);
    }
    u8::from(step!(97) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_presenter(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(110) == 0 {
        return 0;
    }
    if step!(111) == 0 {
        if step!(112) == 0 {
            return 0;
        }
        step!(113);
        step!(114);
        return 1;
    }
    if step!(115) != 0 {
        return u8::from(step!(116) != 0 && step!(117) != 0);
    }
    step!(118);
    step!(119);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_register(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    while step!(130) == 0 {
        step!(131);
    }
    let mut admitted = true;
    for id in 132..=139 {
        if step!(id) == 0 {
            admitted = false;
            break;
        }
    }
    if admitted {
        step!(140);
        if step!(141) != 0 && step!(142) != 0 && step!(143) != 0 {
            if step!(144) != 0 {
                if step!(145) != 0 {
                    step!(146);
                }
            } else {
                step!(147);
                step!(148);
                step!(149);
            }
            if step!(150) != 0 {
                step!(151);
                step!(152);
            }
        }
    }
    if step!(153) != 0 {
        step!(154);
        if step!(155) != 0 {
            step!(156);
            step!(157);
        }
    }
    step!(158); // genuine Delphi registration always runs
    if step!(159) != 0 {
        step!(160);
        step!(161);
        if step!(162) != 0 {
            step!(163);
            step!(164);
        }
        step!(165);
        step!(166);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_scroll(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(200);
    step!(201);
    if step!(202) == 0 || step!(203) == 0 || step!(204) == 0 || (step!(205) != 0 && step!(206) == 0)
    {
        step!(207);
        return;
    }
    step!(208);
    step!(209);
    step!(210);
    if step!(211) == 0 {
        step!(212);
        step!(213);
        return;
    }
    step!(214);
    step!(215);
    if step!(216) != 0 {
        if step!(217) != 0 {
            step!(218);
        }
        if step!(219) != 0 {
            step!(220);
            step!(221);
            step!(222);
            if step!(223) != 0 {
                step!(224);
            } else {
                step!(225);
                if step!(226) != 0 {
                    step!(227);
                } else {
                    step!(228);
                }
            }
        }
        step!(229);
        let cached = step!(230) != 0;
        if cached {
            step!(231);
        }
        if cached && step!(232) != 0 && step!(233) != 0 {
            step!(234);
            if step!(235) != 0 {
                step!(236);
                if step!(237) != 0 {
                    if step!(238) != 0 {
                        step!(239);
                    }
                    step!(240);
                }
                step!(241);
            }
            if step!(242) == 0 {
                step!(243);
            }
            if step!(244) != 0 {
                step!(245);
                if step!(246) != 0 {
                    step!(247);
                } else {
                    step!(248);
                }
            }
            if step!(249) == 0 {
                step!(250);
            }
        } else {
            step!(251);
        }
        if step!(252) != 0 {
            step!(253);
        }
    }
    step!(254);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_acquire(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(300);
    step!(301);
    if step!(302) == 0 || step!(303) == 0 || step!(304) == 0 {
        return 0;
    }
    step!(305);
    step!(306);
    if step!(307) == 0 {
        return 0;
    }
    if step!(308) != 0 && step!(309) != 0 && step!(310) != 0 {
        step!(311);
        step!(312);
        if step!(313) != 0 {
            step!(314);
            if step!(315) == 0 {
                step!(316);
            }
        }
        if step!(317) == 0 {
            step!(318);
        }
        step!(319);
        if step!(320) != 0 {
            step!(321);
        }
    }
    step!(322);
    step!(323);
    if step!(324) == 0 {
        return 0;
    }
    step!(325);
    if step!(326) == 0 {
        return 0;
    }
    if step!(327) != 0 {
        step!(328);
        if step!(329) == 0 {
            step!(330);
            return 0;
        }
    }
    step!(331);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_finish(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    let count = step!(340);
    for index in (0..count).rev() {
        step!(0x40000 + index as u32);
        if step!(341) == 0 {
            continue;
        }
        step!(342);
        step!(343);
        if step!(344) != 0 {
            step!(345);
        }
        if step!(346) == 0 {
            step!(347);
        }
        if step!(348) != 0 {
            step!(349);
        }
        if step!(350) != 0 {
            step!(351);
        }
        if step!(352) != 0 {
            step!(353);
        }
        if step!(354) != 0 {
            step!(355);
        }
        step!(356);
        return 1;
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_canvas_mouse_message(message: u32) -> u8 {
    u8::from(matches!(message, 0x0200..=0x0209 | 0x020b..=0x020d))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_logical_parameter(
    c: *mut c_void,
    op: Op,
    message: u32,
) {
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
    step!(401);
    if step!(402) != 0 && a2fo_rs_stage8_b_canvas_mouse_message(message) != 0 {
        step!(404);
        return;
    }
    if step!(405) != 0 {
        step!(406);
        return;
    }
    if step!(407) != 0 && step!(408) != 0 {
        step!(409);
        step!(410);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_dialog_dispatch(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(420);
    if step!(421) == 0 || step!(422) == 0 {
        return;
    }
    step!(423);
    step!(424);
    if step!(425) != 0 && step!(426) != 0 && step!(427) != 0 && step!(428) != 0 {
        step!(429);
    }
    step!(430);
    step!(431);
    step!(432);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_window_dispatch(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(440);
    if step!(441) == 0 {
        step!(442);
        return;
    }
    if step!(443) != 0 && step!(444) != 0 && step!(445) != 0 {
        step!(467);
        return;
    }
    step!(446);
    step!(447);
    step!(448);
    if step!(449) != 0 {
        step!(450);
    }
    step!(451);
    step!(452);
    let show = step!(453) != 0 && step!(454) != 0;
    let resize_show =
        !show && step!(455) != 0 && step!(456) != 0 && (step!(457) != 0 || step!(458) == 0);
    if show || resize_show {
        step!(459);
    }
    if step!(460) != 0 {
        if step!(461) != 0 && step!(462) != 0 {
            step!(463);
        }
        step!(464);
        if step!(465) != 0 {
            step!(466);
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_adopt(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(700) != 0 {
        return 1;
    }
    for id in 701..=704 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(705) == 0 || step!(706) == 0 {
        return 0;
    }
    step!(707);
    step!(708);
    step!(709);
    if step!(710) == 0 {
        return 0;
    }
    step!(711);
    step!(712);
    step!(713);
    if step!(714) != 0 && step!(715) != 0 {
        step!(716);
        return 0;
    }
    step!(717);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_adopt_child(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(780);
    if step!(781) == 0 || step!(782) != 0 {
        return;
    }
    if step!(783) == 0 {
        return;
    }
    step!(784);
    step!(785);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_fit_children(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(760);
    step!(761);
    let children = step!(762);
    for index in 0..children {
        step!(0x50000 + index as u32);
        step!(763);
        step!(764);
        step!(765);
    }
    step!(766);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_ensure_child(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(740) != 0 {
        return 1;
    }
    for id in 741..=744 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(745) == 0 {
        return 0;
    }
    step!(746);
    let found = step!(747) != 0;
    if found {
        for id in 748..=751 {
            step!(id);
        }
    }
    u8::from(found)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_bridge_dialog(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(800) == 0 || step!(801) == 0 {
        return;
    }
    if step!(802) == 0 || step!(803) == 0 || step!(804) == 0 || step!(805) == 0 {
        step!(806);
        return;
    }
    step!(807);
    step!(808);
    step!(809);
    if step!(810) != 0 && step!(811) != 0 {
        step!(812);
        step!(813);
        return;
    }
    if step!(814) == 0 {
        step!(815);
        step!(816);
        step!(817);
        return;
    }
    step!(818);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_update_font(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(820) != 0 {
        step!(821);
    }
    if step!(822) != 0 && step!(823) != 0 && step!(824) != 0 && step!(825) != 0 {
        return;
    }
    if step!(826) == 0 || step!(827) == 0 {
        return;
    }
    step!(828);
    step!(829);
    if step!(830) == 0 {
        return;
    }
    step!(831);
    step!(832);
    step!(833);
    if step!(834) != 0 {
        step!(835);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_refresh_seed(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(900);
    if step!(901) == 0 || step!(902) == 0 || step!(903) == 0 {
        return;
    }
    step!(904);
    if step!(906) == 0 {
        return;
    }
    step!(907);
    if step!(908) != 0 {
        step!(909);
        if step!(910) == 0 {
            step!(911);
        }
    }
    if step!(912) != 0 {
        step!(913);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_present_seed(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 920..=924 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(925);
    if step!(926) == 0 {
        return 0;
    }
    step!(927);
    step!(928);
    if step!(929) == 0 {
        return 0;
    }
    if step!(930) != 0 {
        step!(931);
        step!(932);
        if step!(933) != 0 {
            step!(934);
            if step!(935) == 0 {
                step!(936);
            }
        }
        if step!(937) == 0 {
            step!(938);
        }
    } else {
        step!(939);
    }
    if step!(940) == 0 || step!(941) == 0 {
        return 0;
    }
    if step!(942) == 0 {
        step!(943);
        step!(944);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_represent_seed(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    for id in 950..=956 {
        if step!(id) == 0 {
            return;
        }
    }
    step!(957);
    if step!(958) == 0 || step!(959) == 0 {
        return;
    }
    step!(960);
    step!(961);
    if step!(962) != 0 {
        step!(963);
        step!(964);
        step!(965);
        if step!(966) != 0 && step!(967) == 0 {
            step!(968);
            step!(969);
        }
    }
    step!(970);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_normalize_fit(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 1000..=1005 {
        if step!(id) == 0 {
            return 0;
        }
    }
    for id in 1006..=1016 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(1017) != 0 {
        step!(1018);
    } else {
        step!(1019);
    }
    step!(1020);
    if step!(1021) == 0 {
        step!(1022);
        step!(1023);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_nested_parent(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(1100) == 0 {
        return 0;
    }
    step!(1101);
    if step!(1102) != 0 {
        return 1;
    }
    step!(1103);
    for id in 1104..=1109 {
        if step!(id) == 0 {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_quiet_secondary(
    c: *mut c_void,
    op: Op,
    name: *const u8,
) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(1200) != 0 || step!(1201) != 0 || step!(1202) == 0 {
        return;
    }
    // The native GetClassNameA filled this fixed local buffer and terminated it.
    let bytes = unsafe { core::slice::from_raw_parts(name, 64) };
    let end = bytes.iter().position(|&byte| byte == 0).unwrap_or(64);
    let class = &bytes[..end];
    let known: [&[u8]; 15] = [
        b"TSingleForm",
        b"TOptionsMenuForm",
        b"TGraphicOptionsForm",
        b"TSoundSettingsForm",
        b"TAdvancedOptionsForm",
        b"TAdvSettingsForm",
        b"TModSettingsForm",
        b"TMapSelectForm",
        b"TMapEditForm",
        b"TMapEditCreateForm",
        b"TManualIPForm",
        b"TMultiplayerConnectionForm",
        b"TMultiplayerSetup",
        b"TWaitingForm",
        b"TYesNoForm",
    ];
    let mut secondary = known.contains(&class);
    if !secondary && class == b"#32770" {
        secondary = step!(1203) != 0 && step!(1204) != 0;
    }
    if !secondary {
        return;
    }
    step!(1205);
    step!(1206);
    if step!(1207) != 0 {
        return;
    }
    step!(1208);
    step!(1209);
    if step!(1210) != 0 && step!(1211) != 0 {
        return;
    }
    step!(1212);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_move_window(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(1300);
    if step!(1301) != 0 && step!(1302) == 0 {
        step!(1303);
        step!(1304);
        step!(1305);
    }
    step!(1306);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_canvas_position_window(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(1320);
    if step!(1321) != 0 && step!(1322) == 0 {
        step!(1323);
        if step!(1324) != 0 {
            step!(1325);
        } else {
            step!(1326);
        }
        if step!(1327) != 0 {
            step!(1328);
        } else {
            step!(1329);
        }
        step!(1330);
    }
    step!(1331);
}
