//! Graphics Options renderer selection and helper workflow.
//! Native callbacks retain INI, filesystem, process, VCL and exception ownership.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;
const ERROR: u64 = u64::MAX;
fn call(context: *mut c_void, op: Op, id: u32, arg: u32) -> Option<u64> {
    let value = unsafe { op(context, id, arg) };
    (value != ERROR).then_some(value)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_renderer_scaled_coordinate(
    reference: i32,
    measured: i32,
    nominal: i32,
) -> i32 {
    reference.wrapping_mul(measured).wrapping_add(nominal / 2) / nominal
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_renderer_valid_bump_bounds(
    left: i32,
    top: i32,
    height: i32,
) -> u8 {
    u8::from((1..=8192).contains(&left) && (1..=8192).contains(&top) && (1..=128).contains(&height))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_reconcile(context: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(context, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    let mut detected: u32 = 0;
    if step!(0) != 0 {
        if step!(1) == 0 || step!(2) == 0 {
            step!(3);
            return 1;
        }
        detected = 1;
    }
    if step!(4) == u64::from(detected) {
        return 1;
    }
    if step!(5, detected) == 0 {
        step!(6);
        return 1;
    }
    step!(7, detected);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_schedule(context: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(8) != 0 {
        return 1;
    }
    step!(9); // helper path
    if step!(10) == 0 {
        step!(11);
        return 0;
    }
    step!(12); // command and STARTUPINFO
    if step!(13) == 0 {
        step!(14);
        return 0;
    }
    step!(15); // native handles close and scheduled flag
    step!(16);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_select(
    context: *mut c_void,
    op: Op,
    backend: u32,
) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(context, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    let previous = step!(17) as u32;
    let applied = step!(18) as u32;
    if backend == previous && backend == applied && step!(19) != 0 {
        step!(20);
        return 1;
    }
    if backend == 1 && step!(21) == 0 && (step!(22) != 1 || step!(23) == 0) {
        step!(24, previous);
        return 1;
    }
    if backend == 0 && applied == 1 && step!(23) == 0 {
        step!(25, previous);
        return 1;
    }
    if step!(26, backend) == 0 {
        step!(27, previous);
        return 1;
    }
    step!(28);
    if step!(29) == 0 {
        step!(30);
        return 1;
    }
    step!(31);
    step!(32, backend);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_install(context: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(context, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(40) == 0 || step!(41) != 0 {
        return 0;
    }
    if step!(42) != 0 {
        return (step!(43) != 0 && step!(44) != 0) as u8;
    }
    step!(45); // publish module before reconciliation and signatures
    step!(46); // native root copy
    step!(47); // logger
    step!(48); // reconcile active renderer state
    for index in 0..24 {
        if step!(49, index) == 0 {
            step!(50);
            return 0;
        }
    }
    if step!(51) == 0 || step!(52) == 0 {
        step!(53);
        return 0;
    }
    step!(54); // installation diagnostic
    let requested = step!(55);
    let applied = step!(56);
    step!(57); // format/log both backend states
    step!(58); // audit both backend states
    step!(59); // read LastError after the diagnostics
    if requested != applied && step!(60) != 0 {
        if step!(61) != 0 {
            step!(62);
        } else {
            step!(63);
        }
    } else if requested == applied {
        step!(64);
    } else {
        step!(65);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_observe_monitor(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    for id in 70..=72 {
        if step!(id) == 0 {
            return;
        }
    }
    let selected = step!(73) as u32 as i32;
    if selected < 0 || selected as u32 as u64 >= step!(74) || selected == step!(75) as u32 as i32 {
        return;
    }
    step!(76); // observed selection before diagnostic and save
    step!(77);
    step!(78);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_monitor_timer(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(80) != 0 || step!(81) == 0 || step!(82) == 0 {
        return;
    }
    if step!(83) == 0 {
        step!(84);
        return;
    }
    step!(85);
    step!(86);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_window(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    let renderer = step!(100) != 0 && step!(101) != 0 && step!(102) != 0 && step!(103) != 0;
    let monitor = step!(104) != 0 && step!(105) != 0 && step!(106) != 0 && step!(107) != 0;
    if renderer {
        step!(108);
    }
    step!(109); // cache native WNDPROC before synchronous forwarding
    step!(110); // forward first; handlers may mutate selection and globals
    if renderer && step!(111) != 0 && step!(112) != 0 {
        let selected = step!(113) as u32 as i32;
        if selected == 0 || selected == 1 {
            let current = step!(114);
            let before = step!(115);
            if current == before && selected as u64 != before {
                step!(116);
                step!(117);
            }
        }
    }
    if monitor && step!(120) != 0 && step!(121) != 0 {
        let selected = step!(122) as u32 as i32;
        if selected >= 0 && (selected as u32 as u64) < step!(123) {
            let configured = step!(124) != 0;
            if !configured || step!(125) != 0 {
                step!(126);
                step!(127);
                step!(128);
            }
        }
    }
    if step!(130) != 0 && step!(131) != 0 {
        step!(132);
    }
    if step!(133) != 0 && step!(134) != 0 {
        step!(135);
        step!(136);
        step!(137);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_effect_controls(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(200) == 0 {
        return;
    }
    if step!(201) == 0 {
        step!(202);
        return;
    }
    if step!(203) != 0 && step!(204) != 0 && step!(205) != 0 && step!(206) != 0 && step!(207) != 0 {
        for id in 208..=215 {
            step!(id);
        }
        return;
    }
    step!(217); // first constructor
    step!(218); // second constructor even if first failed
    if step!(219) == 0 || step!(220) == 0 {
        step!(221);
        step!(222);
        return;
    }
    step!(223);
    step!(224); // native layout snapshot after both constructors
    step!(225);
    step!(226);
    if step!(227) == 0 || step!(228) == 0 {
        step!(229);
        return;
    }
    step!(230);
    step!(231);
    if step!(232) == 0 || step!(233) == 0 {
        step!(234);
        return;
    }
    for id in 235..=248 {
        step!(id);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_subclass(context: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(250) == 0 || step!(251) == 0 {
        return 0;
    }
    if step!(252) != 0 && step!(253) != 0 {
        return 1;
    }
    if step!(254) != 0 {
        step!(255);
        return 0;
    }
    step!(256);
    step!(257);
    step!(258);
    if step!(259) != 0 {
        step!(260);
        step!(261);
        return 0;
    }
    step!(262);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_refresh_monitor(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(context, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    for id in 270..=272 {
        if step!(id) == 0 {
            return;
        }
    }
    step!(273);
    step!(274);
    step!(275);
    let count = step!(276);
    for index in 0..count {
        step!(277, index as u32);
    }
    if step!(278) != 0 {
        for id in 279..=284 {
            step!(id);
        }
        return;
    }
    step!(285);
    step!(286);
    for id in 287..=291 {
        step!(id);
    }
    if step!(292) != 0 && step!(293) == 0 {
        step!(294);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_save_monitor(
    context: *mut c_void,
    op: Op,
    selected: i32,
) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if selected < 0 || selected as u64 >= step!(300) {
        return;
    }
    step!(301);
    if step!(302) == 0 {
        for id in 303..=305 {
            step!(id);
        }
        return;
    }
    step!(306);
    step!(307);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_monitor_controls(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    for id in 400..=402 {
        if step!(id) == 0 {
            return;
        }
    }
    let reusable = step!(403) != 0 && step!(404) != 0 && step!(405) != 0;
    if reusable {
        step!(406);
    }
    if reusable && step!(407) != 0 && step!(408) != 0 {
        for id in 409..=415 {
            step!(id);
        }
        return;
    }
    step!(416);
    step!(417);
    if step!(418) == 0 || step!(419) == 0 {
        step!(420);
        step!(421);
        return;
    }
    for id in 422..=426 {
        step!(id);
    }
    if step!(427) == 0 || step!(428) == 0 {
        step!(429);
        return;
    }
    for id in 430..=432 {
        step!(id);
    }
    if step!(433) != 0 && step!(434) != 0 {
        step!(435);
        step!(436);
        if step!(437) != 0 && step!(438) != 0 {
            step!(439);
        }
    }
    step!(440);
    if step!(441) == 0 || step!(442) == 0 {
        step!(443);
        return;
    }
    step!(444);
    if step!(445) == 0 {
        step!(446);
    }
    for id in 447..=453 {
        step!(id);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_controls(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(context, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(500) == 0 {
        return;
    }
    step!(501);
    if step!(502) == 0 || step!(503) == 0 || step!(504) == 0 {
        return;
    }
    for id in 505..=508 {
        step!(id);
    }
    let reusable = step!(509) != 0 && step!(510) != 0 && step!(511) != 0 && step!(512) != 0;
    if reusable {
        step!(513);
    }
    if step!(514) != 0 && reusable && step!(515) != 0 && step!(516) != 0 {
        step!(517);
        let requested = step!(518) as u32;
        step!(519, requested);
        for id in 520..=526 {
            step!(id);
        }
        return;
    }
    for id in 527..=533 {
        step!(id);
    }
    if step!(534) == 0 || step!(535) == 0 || step!(536) == 0 {
        step!(537);
        return;
    }
    for id in 538..=545 {
        step!(id);
    }
    if step!(546) == 0 || step!(547) == 0 || step!(548) == 0 {
        step!(549);
        return;
    }
    for id in 550..=556 {
        step!(id);
    }
    if step!(557) != 0 && step!(558) != 0 {
        step!(559);
        step!(560);
        if step!(561) != 0 && step!(563) != 0 {
            step!(564);
            step!(565);
        } else {
            step!(566);
        }
    } else {
        step!(567);
    }
    step!(568);
    if step!(569) == 0 || step!(570) == 0 {
        step!(571);
        return;
    }
    for id in 572..=574 {
        step!(id);
    }
    let requested = step!(575) as u32;
    step!(576, requested);
    if step!(577) == 0 {
        step!(578);
        return;
    }
    for id in 579..=592 {
        step!(id);
    }
    if step!(593) == 0 {
        step!(594);
    } else if requested as u64 != step!(595) {
        if step!(596) != 0 {
            step!(597);
        } else {
            step!(598);
        }
    }
    for id in 599..=601 {
        step!(id);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_read_effect(context: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(context, op, $id, $arg) {
                Some(value) => value,
                None => return 1,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(700) == 0 || step!(701) == 0 {
        return 1;
    }
    let cached = step!(702) as u32 as i32;
    if cached >= 0 {
        return u8::from(cached != 0);
    }
    let loaded = if step!(703) != 0 {
        1
    } else {
        u32::from(step!(704) != 0)
    };
    step!(705, loaded);
    u8::from(step!(706) as u32 as i32 != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_save_effect(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    for id in 720..=724 {
        if step!(id) == 0 {
            return;
        }
    }
    step!(725);
    if step!(726) == 0 {
        step!(727);
        step!(728);
        return;
    }
    step!(730);
    step!(731);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_preserve_payload(
    context: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(740) != 0 {
        return 1;
    }
    if step!(741) == 0 {
        return 0;
    }
    step!(742);
    if step!(743) == 0 {
        return 0;
    }
    u8::from(step!(744) != 0 || step!(745) != 0)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_position_effect(context: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(context, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    let left = step!(750) as u32 as i32;
    let top = step!(751) as u32 as i32;
    let height = step!(752) as u32 as i32;
    if !(0..=8192).contains(&left) || !(0..=8192).contains(&top) || !(1..=128).contains(&height) {
        step!(753);
        return;
    }
    let effects_top = top.wrapping_add(height).wrapping_add(8);
    step!(754, effects_top as u32);
    step!(755, effects_top as u32);
    step!(756);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_renderer_create_control(
    context: *mut c_void,
    op: Op,
) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(context, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(760) == 0 || step!(761) == 0 {
        return 0;
    }
    step!(762);
    if step!(763) == 0 || step!(764) == 0 {
        return 0;
    }
    step!(765);
    if step!(766) == 0 || step!(767) == 0 {
        return 0;
    }
    if step!(768) == 0 {
        step!(769);
        return 0;
    }
    step!(770);
    if step!(771) == 0 {
        step!(772);
        return 0;
    }
    1
}
