//! Refit scene worker admission and catalogue selection. Native callbacks own
//! the Context, handles, and the indivisible worker ownership transition.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;
const ERROR: u64 = u64::MAX;
fn call(c: *mut c_void, op: Op, id: u32, arg: u32) -> Option<u64> {
    let value = unsafe { op(c, id, arg) };
    (value != ERROR).then_some(value)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_start(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(0) != 0 {
        return 1;
    }
    if step!(1) != 0 || step!(2) == 0 {
        return 0;
    }
    step!(3); // allocate native worker context
    if step!(4) == 0 {
        return 0;
    }
    if step!(5) != 0 {
        let count = step!(6);
        if count > 32 {
            return 0;
        }
        for index in 0..count {
            step!(7, index as u32);
        }
    }
    step!(8); // root fallback when extension list is empty
    if step!(9) == 0 || step!(10) == 0 {
        return 0;
    }
    // Native op 11 keeps CreateThread, Context release, and handle publication
    // adjacent: the new worker takes Context ownership synchronously.
    if step!(11) == 0 {
        return 0;
    }
    step!(12); // UI-thread started flag
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_configure(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    step!(0); // publish api, render width, wanted
    if step!(1) == 0 {
        return;
    }
    if step!(2) == 0 || step!(3) == 0 {
        return;
    }
    step!(4); // replace catalogue and reset selected
    let count = step!(5);
    for index in 0..count {
        if step!(6, index as u32) != 0 {
            step!(7, index as u32);
        }
    }
    step!(8); // selected page
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_choice_at(c: *mut c_void, op: Op) -> i32 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return -1,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(0) == 0 {
        return -1;
    }
    let mut index = 0;
    while index < step!(1) as i32 {
        if step!(2, index as u32) != 0 {
            let size = step!(3) as i32;
            let page = step!(4) as i32;
            let page_size = step!(5) as i32;
            let items = page_size.min(size - page * page_size);
            return if index < items {
                page * page_size + index
            } else {
                size + index - items
            };
        }
        index += 1;
    }
    -1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_choose(index: i32, c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if index < 0 {
        return;
    }
    let size = step!(3) as i32;
    if index >= size {
        if step!(6) > 1 && index <= size + 1 {
            let page = step!(4) as i32;
            let direction = if index == size { -1 } else { 1 };
            let numerator = page + step!(6) as i32 + direction;
            let denominator = step!(6) as i32;
            step!(7, (numerator % denominator) as u32);
        }
        return;
    }
    step!(8, index as u32);
    step!(9); // native mailbox snapshot
    if step!(10) != 0 {
        step!(11);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_select(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(0) == 0 {
        return 0;
    } // UI ownership
    if step!(1) != 0 {
        return 1;
    } // null or empty ID preserves selection
    for index in 0..=128 {
        let byte = step!(2, index);
        if byte == 0 {
            break;
        }
        if index == 128 || byte < 32 {
            return 0;
        }
        let folded = if byte >= b'A' as u64 && byte <= b'Z' as u64 {
            byte + 32
        } else {
            byte
        };
        step!(3, folded as u32); // native string append keeps allocation behavior
    }
    let mut index = 0;
    while index < step!(4) {
        if step!(5, index as u32) != 0 {
            step!(6, index as u32);
            step!(7); // snapshot request after selection
            if step!(8) != 0 {
                step!(9);
            }
            return 1;
        }
        index += 1;
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_departure(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(10) == 0 || step!(0) == 0 || step!(11) == 0 {
        return 0;
    }
    if step!(12) == 0 || step!(13) == 0 {
        return 0;
    }
    step!(14); // native mailbox snapshot
    if step!(15) != 0 {
        step!(9);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_loading_frame(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(16); // mark loading seen before the disabled gate
    if step!(10) == 0 || step!(12) == 0 {
        return;
    }
    if step!(17) == 0 || step!(13) == 0 {
        return;
    }
    step!(14); // native mailbox snapshot
    step!(18); // renew departure lease regardless of returned change flag
    step!(9); // wake even for an unchanged request
    step!(19); // drain logs
    step!(20); // return immutable image snapshot
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_frame(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(21) == 0 || step!(22) == 0 || step!(23) == 0 || step!(0) == 0 {
        return;
    }
    if step!(24) != 0 {
        step!(25);
        return;
    } // retained launch frame
    if step!(26) != 0 {
        step!(27);
    }
    if step!(28) != 0 {
        step!(29);
        step!(30);
        step!(31);
        return;
    }
    step!(32); // reset mission observer
    if step!(10) == 0 {
        return;
    }
    step!(9); // end prior departure
    if step!(12) == 0 {
        return;
    }
    step!(33); // drain worker diagnostics
    if step!(34) == 0 {
        return;
    }
    if step!(35) != 0 {
        if step!(36) == 0 || step!(37) == 0 || step!(38) == 0 || step!(13) == 0 {
            return;
        }
        step!(39); // set presentation owner
        step!(40); // store redraw-children preference
        if step!(41) == 0 {
            if step!(42) == 0 {
                return;
            }
            if step!(43) == 0 {
                step!(44);
                return;
            }
            step!(45); // remember timer
        }
        step!(46); // choose base view
        if step!(47) != 0 {
            step!(48);
        }
        if step!(49) != 0 {
            step!(52);
        }
    } else if step!(50) != 0 {
        step!(30);
    }
    step!(51); // snapshot and return displayed frame
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_handover(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(100) == 0 || step!(101) == 0 || step!(102) == 0 {
        return 0;
    }
    if step!(103) == 0
        || step!(104) == 0
        || (step!(105) == 0
            && (step!(106) == 0 || step!(107) == 0 || step!(108) == 0 || step!(109) == 0))
    {
        step!(110); // native admission diagnostic retains GetParent timing
        return 0;
    }
    step!(111); // current thread after window relation validation
    if (step!(105) == 0 && step!(112) == 0) || step!(113) == 0 {
        return 0;
    }
    if step!(114) == 0 || step!(115) == 0 || step!(116) == 0 {
        return 0;
    }
    step!(117); // end prior handover before token publication
    step!(118);
    if step!(119) == 0 {
        return 0;
    }
    if step!(120) != 0 && step!(121) == 0 {
        step!(122);
        return 0;
    }
    step!(123);
    step!(124);
    if step!(125) == 0 && step!(126) != 0 {
        step!(127);
    }
    step!(128);
    step!(129);
    step!(130);
    step!(131);
    if step!(132) != 0 {
        step!(133);
    }
    step!(134);
    step!(135);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_worker_step(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    step!(200); // native worker message pump
    let bridging = step!(201) != 0;
    if bridging {
        step!(202);
    }
    step!(203); // acquire one immutable request snapshot
    if step!(204) != 0 {
        return 0;
    }
    let now = step!(205) as u32;
    let view_changed = step!(206) != 0;
    let leased = step!(207) == 0 || step!(208) != 0;
    let rendering = (step!(209) != 0 || bridging) && leased;
    let pause = step!(210) as u32;
    let cooldown = step!(211) as u32;
    let pending = step!(212) != 0;
    let completed = step!(213) as u32;
    if rendering
        && !pending
        && cooldown == 0
        && (completed == 0 || view_changed || now.wrapping_sub(completed) >= pause)
    {
        step!(214, u32::from(view_changed || completed == 0));
        if step!(215) == 0 {
            step!(216); // clear loading before reset
            step!(217); // failure time
            step!(218); // native device reset
            if step!(219) >= 3 {
                step!(220);
                return 0;
            }
            step!(221); // clear drawn view
            step!(222); // retry diagnostic
            return 1;
        }
        step!(223); // clear failures
        step!(224); // completion time and rendered cost
        step!(225, u32::from(view_changed)); // steady-state cost
        step!(226); // view and epoch
        step!(227); // publish immutable frame
        if bridging {
            step!(202);
        }
        if step!(228) < 3 || view_changed {
            step!(229);
        }
        return 1;
    }
    let elapsed = now.wrapping_sub(completed);
    let scheduled = if rendering && !pending {
        if cooldown != 0 {
            cooldown
        } else {
            pause.saturating_sub(elapsed)
        }
    } else {
        u32::MAX
    };
    let timeout = if bridging {
        scheduled.min(16)
    } else {
        scheduled
    };
    step!(230, timeout);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_present_handover(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(300); // request snapshot
    if step!(301) != 0 || (step!(302) == 0 && step!(303) == 0) {
        return;
    }
    step!(304); // image snapshot after request admission
    if step!(305) == 0 {
        return;
    }
    step!(306); // clock
    step!(307); // handover state
    step!(308); // transition image
    if step!(309) != 0 {
        step!(310);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_destination(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
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
    step!(403); // snapshot handover state after UI admission
    for id in 404..=407 {
        if step!(id) == 0 {
            return;
        }
    }
    step!(408);
    if step!(409) != 0 && step!(410) != 0 && step!(411) != 0 && step!(412) != 0 && step!(413) != 0 {
        step!(414);
    }
    step!(415);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_end_handover(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    step!(420); // native state finish first
    if step!(421) != 0 && step!(422) != 0 {
        step!(423);
    }
    for index in 0..3 {
        if step!(424, index) != 0 && step!(425) != 0 && step!(426, index) != 0 {
            step!(427, index);
        }
    }
    if step!(428) != 0 && step!(429) != 0 {
        step!(430);
    }
    step!(431);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_draw_handover(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(500) == 0 {
        return 0;
    }
    let chosen = if step!(501) != 0 {
        if step!(502, 2) != 0 { 2 } else { 3 }
    } else if step!(502, 0) != 0 {
        0
    } else if step!(502, 1) != 0 {
        1
    } else if step!(502, 2) != 0 {
        2
    } else {
        3
    };
    if chosen != 3 {
        step!(503, chosen);
    }
    for id in 504..=510 {
        if step!(id) == 0 {
            return 0;
        }
    }
    let mut okay = false;
    if step!(511) != 0 {
        step!(512);
        okay = step!(513) != 0;
        if okay {
            okay = step!(514) != 0;
        }
        step!(515); // RestoreDC even after failed draw or flush
    }
    step!(516); // ReleaseDC after both SaveDC paths
    okay as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_frame_presented(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(600); // snapshot before pointer comparison
    if step!(601) != 0 && step!(602) != 0 && step!(603) != 0 {
        step!(604);
        step!(605);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_canvas_completed(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(610) == 0 {
        return;
    }
    step!(611); // move image then erase native pending entry
    step!(612); // snapshot after map mutation
    if step!(613) != 0 && step!(614) != 0 && step!(615) != 0 {
        step!(616);
        step!(617);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_frame_drawn(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(620) == 0 {
        return;
    }
    step!(621);
    if step!(622) == 0 {
        return;
    }
    step!(623); // provider and resolver lookup
    if step!(624) != 0 && step!(625) != 0 {
        if step!(626) == 0 {
            step!(627);
        }
        if step!(628) != 0 {
            step!(629);
            return;
        }
    }
    step!(630); // existing native frame_presented path
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_shutdown(c: *mut c_void, op: Op) {
    for id in 640..=642 {
        if call(c, op, id, 0).is_none() {
            return;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_erase(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(700) == 0 || step!(701) == 0 || step!(702) == 0 {
        return 0;
    }
    if step!(703) == 0 {
        return (step!(704) != 0) as u8;
    }
    step!(705);
    for id in 706..=708 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(709);
    step!(710);
    for id in 711..=717 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(718);
    if step!(719) == 0 {
        return 0;
    }
    (step!(720) != 0) as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_window(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(800) != 0 {
        step!(801);
        if step!(802) != 0 && step!(803) != 0 && step!(804) != 0 {
            if step!(805) == 0 {
                return 1;
            }
            step!(806);
            let visible = step!(807) != 0
                && step!(808) != 0
                && step!(809) != 0
                && step!(810) != 0
                && step!(811) != 0;
            step!(812, u32::from(visible));
            if visible && step!(813) != 0 {
                step!(814);
            }
            return 1;
        }
    }
    if step!(815) != 0 && step!(816) != 0 && step!(817) != 0 {
        step!(818);
    }
    if step!(819) != 0 {
        step!(820);
        if step!(821) != 0 {
            step!(822);
        }
        step!(823);
        step!(824);
        if step!(825) != 0 {
            step!(826);
        }
    }
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_backend_load(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(900) != 0 {
        return 1;
    }
    step!(901); // native module path construction
    if step!(902) == 0 {
        return 0;
    }
    step!(903); // all provider symbols are resolved before validation
    if step!(904) == 0 || step!(905) != 1 || step!(906) == 0 || step!(907) == 0 || step!(908) == 0 {
        return 0;
    }
    if step!(909) == 0 {
        return 0;
    }
    step!(910); // publish pinned provider
    let clock = step!(911) != 0;
    step!(912, u32::from(clock));
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_resize(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(1000) != 0 {
        return 1;
    }
    for id in 1001..=1007 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(1008); // native COM surface ownership swap
    step!(1009); // diagnostic after dimensions publish
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_backend_create(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    step!(1100); // native COM release before every create attempt
    if step!(1101) == 0 && (step!(1102) == 0 || step!(1103) == 0) {
        return 0;
    }
    if step!(1104) == 0 {
        step!(1105);
    }
    if step!(1106) == 0 {
        return 0;
    }
    step!(1107); // native D3D9 presentation descriptor
    let mut created = false;
    for processing in 0..2 {
        for depth in 0..2 {
            if step!(1108, processing * 2 + depth) != 0 {
                created = true;
                break;
            }
        }
        if created {
            break;
        }
    }
    if step!(1109) == 0 || step!(1110) == 0 || step!(1111) == 0 {
        return 0;
    }
    if step!(1112) == 0 {
        if step!(1113) == 0 {
            return 0;
        }
    } else if step!(1114) == 0 {
        return 0;
    }
    step!(1115);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_backend_render(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return 0,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    step!(1200); // timing begins before provider admission
    if step!(1201) == 0 {
        return 0;
    }
    if step!(1202) == 0 && step!(1203) == 0 {
        return 0;
    }
    if step!(1204) == 0 || step!(1205) == 0 || step!(1206) == 0 {
        return 0;
    }
    step!(1207);
    if step!(1208) != 0 {
        step!(1209);
    }
    if step!(1210) != 0 && step!(1211) != 0 {
        if step!(1212) != 0 {
            step!(1213);
        } else if step!(1214) != 0 {
            step!(1215);
        }
        step!(1216);
    }
    let use_warp = step!(1210) != 0 && step!(1214) != 0;
    step!(1217, u32::from(use_warp));
    let drawn = step!(1218) != 0;
    let ended = step!(1219) != 0;
    if !drawn || !ended {
        return 0;
    }
    step!(1220);
    if step!(1221) == 0 {
        return 0;
    }
    let copied = step!(1222) != 0;
    step!(1223); // release target before copy-result gate
    if !copied || step!(1224) == 0 {
        return 0;
    }
    step!(1225);
    let valid = step!(1226) != 0 && step!(1227) != 0;
    if valid {
        step!(1228);
        let height = step!(1232) as u32 as i32;
        for row in 0..height {
            step!(1229, row as u32);
        }
    }
    step!(1230); // UnlockRect even when pitch/pixels invalid
    step!(1231);
    valid as u8
}

#[repr(C)]
pub struct SceneRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_scene_page_count(size: i32, page_size: i32) -> i32 {
    1.max(size.wrapping_add(page_size).wrapping_sub(1) / page_size)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_scene_choice_count(size: i32, page: i32, page_size: i32) -> i32 {
    let items = page_size.min(size.wrapping_sub(page.wrapping_mul(page_size)));
    items.wrapping_add(i32::from(a2fo_rs_stage8_b_scene_page_count(size, page_size) > 1) * 2)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage8_b_scene_choice_rect(
    index: i32,
    width: i32,
    height: i32,
    title_x: i32,
    title_width: i32,
    count: i32,
    out: *mut SceneRect,
) {
    if out.is_null() {
        return;
    }
    let left = title_x.wrapping_add(title_width).wrapping_add(20);
    let room = width.wrapping_sub(left).wrapping_sub(16);
    let rect = if room >= 400 {
        let w = 130.min(room.wrapping_sub(count.wrapping_sub(1).wrapping_mul(6)) / count);
        SceneRect {
            x: width
                .wrapping_sub(16)
                .wrapping_sub(count.wrapping_mul(w))
                .wrapping_sub(count.wrapping_sub(1).wrapping_mul(6))
                .wrapping_add(index.wrapping_mul(w.wrapping_add(6))),
            y: height.wrapping_sub(66),
            width: w,
            height: 28,
        }
    } else {
        let w = 56.max(110.min(room));
        SceneRect {
            x: width.wrapping_sub(12).wrapping_sub(w),
            y: height
                .wrapping_sub(70)
                .wrapping_sub(count.wrapping_sub(1).wrapping_sub(index).wrapping_mul(34)),
            width: w,
            height: 28,
        }
    };
    unsafe {
        *out = rect;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_scene_paint_choices(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr, $arg:expr) => {
            match call(c, op, $id, $arg) {
                Some(value) => value,
                None => return,
            }
        };
        ($id:expr) => {
            step!($id, 0)
        };
    }
    if step!(1300) == 0 || step!(1301) == 0 {
        return;
    }
    step!(1302);
    let size = step!(1303) as u32 as i32;
    let initial_page = step!(1304) as u32 as i32;
    let items = 5.min(size.wrapping_sub(initial_page.wrapping_mul(5)));
    let mut i = 0i32;
    while i < step!(1305) as u32 as i32 {
        let page = step!(1304) as u32 as i32; // native page can change during GDI callbacks
        let index = page.wrapping_mul(5).wrapping_add(i);
        let chosen = i < items && index == step!(1306) as u32 as i32;
        step!(1307, index as u32); // native layout and RECT for choice i
        step!(1308, u32::from(chosen));
        step!(1309);
        step!(1310, u32::from(chosen));
        let label_kind = if i < items {
            0
        } else if i == items {
            1
        } else {
            2
        };
        step!(1311, label_kind);
        i += 1;
    }
    if step!(1312) != 0 {
        step!(1313);
    }
    step!(1314);
}
