//! Shell movie hook admission and synchronous presentation workflow.
//! Native callbacks own SDK/COM/Win32 operations, patching, storage and ABI.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
fn call(c: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let result = unsafe { op(c, id) };
    (result != u64::MAX).then_some(result)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_initialize(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(0); // clear live gate before any API read
    for id in 1..=6 {
        if step!(id) == 0 {
            step!(7);
            return 0;
        }
    }
    if step!(8) == 0 {
        return 0;
    }
    step!(9); // publish FO module after successful lookup
    step!(10); // compute native addresses and instruction bytes
    for id in 11..=24 {
        if step!(id) == 0 {
            step!(25);
            return 0;
        }
    }
    step!(26); // publish native target/function owner pointers
    for id in 27..=32 {
        if step!(id) == 0 {
            step!(33);
            return 0;
        }
    }
    step!(34); // construct one atomic native byte-patch span
    if step!(35) == 0 {
        step!(36);
        return 0;
    }
    step!(37); // publish live gate only after all checked writes
    step!(38);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_shutdown(c: *mut c_void, op: Op) {
    let _ = call(c, op, 40);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_window(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(50); // invoke native getter before readiness/shell state
    if step!(51) != 0 && step!(52) == 0 {
        step!(53);
        step!(54);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_play(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(60) != 0 && step!(61) != 0 && step!(62) == 0 {
        for id in 63..=67 {
            step!(id);
        }
    }
    step!(68); // read nullable movie handle after window workflow
    u8::from(step!(69) != 0) // native Scope encloses play and closes synchronously
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_render(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(80) != 0 {
        step!(81);
    }
    step!(82);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_preserve_shell(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(90) == 0 {
        return 0;
    }
    step!(91); // raw native shell state
    step!(92); // optional caller output before atomic state publication
    if step!(93) != 0 {
        step!(94);
    }
    u8::from(step!(95) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_seed_presentation(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    step!(100);
    if step!(101) != 0 {
        return 1;
    }
    if step!(102) == 0 {
        return 0;
    }
    step!(103);
    if step!(104) == 0 {
        return 0;
    }
    for id in 105..=109 {
        step!(id);
    }
    if step!(110) != 0 || step!(111) != 0 {
        if step!(112) != 0 && step!(113) != 0 && step!(114) != 0 {
            step!(115);
            step!(116);
            if step!(117) != 0 {
                step!(118);
            }
        } else {
            step!(119);
            step!(120);
        }
    }
    if step!(121) != 0 {
        step!(122);
    }
    if step!(123) == 0 {
        return 0;
    }
    if step!(124) != 0 {
        return 1;
    }
    step!(125);
    if step!(126) != 0 {
        step!(127);
    }
    if step!(128) != 0 {
        step!(129);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_gdi(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    for id in 200..=207 {
        if step!(id) == 0 {
            step!(208);
            return;
        }
    }
    if step!(209) != 0 {
        step!(210);
        step!(208);
        return;
    }
    step!(211);
    let has_window = step!(212) != 0;
    if has_window {
        step!(213);
    }
    let target = has_window && step!(214) != 0 && step!(215) != 0;
    if !target && (step!(216) == 0 || step!(217) == 0) {
        step!(208);
        return;
    }
    step!(218);
    step!(219);
    if step!(220) == 0 || step!(221) == 0 {
        return;
    }
    step!(222);
    if step!(223) != 0 {
        step!(224);
    }
    step!(225);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_texture(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(300) == 0 {
        step!(301);
        return;
    }
    if step!(302) == 0 || step!(303) == 0 {
        return;
    }
    if step!(304) != 0 {
        step!(305);
        step!(301);
        return;
    }
    step!(306);
    step!(307);
    if step!(308) != 0 {
        step!(309);
        if step!(310) != 0 {
            if step!(311) != 0 {
                step!(312);
            } else {
                step!(313);
                step!(314);
            }
        }
    }
    if step!(315) != 0 {
        step!(316);
    }
    step!(317);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_prepare_viewport(c: *mut c_void, op: Op) {
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
    if step!(402) == 0 {
        return;
    }
    if step!(403) != 0 {
        if step!(404) != 0 && step!(405) != 0 {
            step!(406);
            if step!(407) != 0 && step!(408) != 0 {
                step!(409);
            }
        }
        step!(410);
    }
    if step!(411) == 0 {
        return;
    }
    step!(412);
    step!(413);
    for id in 414..=416 {
        if step!(id) == 0 {
            return;
        }
    }
    step!(417);
    step!(418);
    if step!(419) != 0 && step!(420) != 0 && step!(421) != 0 {
        step!(422);
    }
    step!(423);
    step!(424);
    step!(425);
    step!(426);
    if step!(427) != 0 && step!(428) != 0 {
        step!(429);
    }
    if step!(430) != 0 {
        step!(431);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_apply_window(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(500) == 0 {
        return 0;
    }
    if step!(501) == 0 && (step!(502) == 0 || step!(503) == 0) {
        return 0;
    }
    step!(504);
    if step!(505) == 0 || step!(506) == 0 {
        return 0;
    }
    let shell_window = step!(507) != 0 && step!(508) != 0;
    step!(if shell_window { 509 } else { 510 });
    step!(511);
    let have_monitor = step!(512) != 0 && step!(513) != 0 && step!(514) != 0;
    if step!(515) != 0 && have_monitor {
        step!(516);
    }
    step!(517);
    let geometry = step!(518) != 0 && step!(519) != 0 && step!(520) != 0 && step!(521) != 0;
    if step!(522) != 0 && step!(523) != 0 && geometry {
        return 1;
    }
    if step!(524) != 0 && step!(525) != 0 && geometry {
        step!(526);
        step!(527);
        return 1;
    }
    step!(528);
    step!(529);
    if step!(530) != 0 && step!(531) != 0 {
        step!(532);
        step!(533);
        step!(534);
    } else if step!(535) != 0 {
        step!(536);
    } else {
        step!(537);
    }
    step!(538);
    step!(539);
    step!(540);
    if step!(541) != 0 && step!(542) != 0 {
        step!(543);
        step!(544);
    }
    step!(545);
    if step!(546) == 0 && step!(547) == 0 {
        step!(548);
    }
    step!(549);
    if have_monitor {
        step!(550);
    }
    step!(551);
    let matched = step!(552) != 0 && step!(553) != 0 && step!(554) != 0 && step!(555) != 0;
    if matched {
        if step!(556) != 0 {
            step!(557);
            step!(558);
        }
        step!(559);
        step!(560);
    }
    if step!(561) != 0 {
        step!(562);
        step!(563);
    }
    u8::from(matched)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_log_once(c: *mut c_void, op: Op) {
    if let Some(value) = call(c, op, 600)
        && value != 0
    {
        let _ = call(c, op, 601);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_bink_fit_dimensions(
    source_width: u32,
    source_height: u32,
    target_width: u32,
    target_height: u32,
    fitted_width: *mut u32,
    fitted_height: *mut u32,
) {
    // All three callers admit a nonzero source width and height first.
    let mut width = target_width;
    let mut height =
        ((u64::from(source_height) * u64::from(target_width)) / u64::from(source_width)) as u32;
    if height > target_height {
        height = target_height;
        width = ((u64::from(source_width) * u64::from(target_height)) / u64::from(source_height))
            as u32;
    }
    unsafe {
        *fitted_width = width;
        *fitted_height = height;
    }
}
