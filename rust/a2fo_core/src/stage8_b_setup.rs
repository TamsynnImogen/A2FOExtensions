//! Instant Action setup observer admission and checked-install ordering.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
const ERROR: u64 = u64::MAX;
fn call(c: *mut c_void, op: Op, id: u32) -> Option<u64> {
    let value = unsafe { op(c, id) };
    (value != ERROR).then_some(value)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_start(c: *mut c_void, op: Op) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(0) == 0 {
        return;
    }
    step!(1); // started is committed before native identity lookup
    step!(2); // Armada module
    if step!(3) != 0 {
        return;
    }
    step!(4); // ShellDisplay provider
    // All three required GetProcAddress calls precede their shared gate.
    let copy = step!(5);
    let presenter = step!(6);
    let bridge = step!(7);
    if copy == 0 || presenter == 0 || bridge == 0 {
        step!(8);
        return;
    }
    step!(9); // copy required function pointers
    if step!(10) != 0 {
        step!(11); // copy optional seed pointer; record owner thread
        if step!(12) == 0 {
            step!(13);
        }
        if step!(14) != 0 && step!(15) != 0 {
            step!(16); // copy optional refresh pointer
            if step!(17) != 0 {
                step!(18);
            } else {
                step!(19);
            }
        }
    }
    if step!(20) == 0 {
        return;
    }
    if step!(21) == 0 {
        step!(22);
        return;
    }
    step!(23); // owner thread
    if step!(24) == 0 {
        step!(25);
        return;
    }
    step!(26); // publish gateway
    step!(27);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_asset(
    data: *const u8,
    len: usize,
    context: *mut c_void,
    append: unsafe extern "C" fn(*mut c_void, u8) -> u8,
) -> u8 {
    if data.is_null() && len != 0 {
        return 0;
    }
    let path = if len == 0 {
        &[][..]
    } else {
        unsafe { core::slice::from_raw_parts(data, len) }
    };
    const NAMES: [&[u8]; 4] = [
        b"bitmaps/multisetup/instactionbkg.png",
        b"bitmaps/multisetup/instactionbkg.bmp",
        b"multisetup/instactionbkg.png",
        b"multisetup/instactionbkg.bmp",
    ];
    let mut matches = [true; 4];
    for (index, &original) in path.iter().enumerate() {
        let slash = if original == b'\\' { b'/' } else { original };
        let folded = if slash.is_ascii_uppercase() {
            slash + 32
        } else {
            slash
        };
        if unsafe { append(context, folded) } == 0 {
            return 0;
        }
        for (candidate, expected) in NAMES.iter().enumerate() {
            if expected.get(index).copied() != Some(folded) {
                matches[candidate] = false;
            }
        }
    }
    u8::from(
        NAMES
            .iter()
            .enumerate()
            .any(|(index, expected)| matches[index] && expected.len() == path.len()),
    )
}

type OpArg = unsafe extern "C" fn(*mut c_void, u32) -> u64;
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_observe(c: *mut c_void, op: OpArg) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(100) == 0 || step!(101) != 0 {
        return;
    }
    if step!(102) == 0 {
        step!(103);
        return;
    }
    if step!(104) == 0 {
        step!(105);
        return;
    }
    step!(106); // previous accepted snapshot invalidated before later admission
    if step!(107) == 0 {
        step!(108);
        return;
    }
    if step!(109) == 0 {
        step!(110);
        return;
    }
    if step!(111) == 0 {
        step!(112);
        return;
    }
    if step!(113) == 0 {
        step!(114);
        return;
    }
    step!(115); // native session allocation
    if step!(116) == 0 {
        step!(117);
        return;
    }
    step!(118); // publish session before presenter registration
    if step!(119) == 0 {
        step!(120);
        return;
    }
    step!(121); // parent snapshot
    if step!(122) != 0 {
        step!(123);
    }
    step!(124); // observed set
    step!(125); // diagnostic
    step!(126); // paint invalidation
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_handover(c: *mut c_void, op: OpArg) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(130) == 0 || step!(131) == 0 {
        return 0;
    }
    if step!(132) == 0 {
        step!(133);
        return 0;
    }
    step!(134);
    if step!(135) == 0 || step!(136) == 0 || step!(137) == 0 {
        step!(138);
        return 0;
    }
    step!(139);
    if step!(140) == 0 {
        return 0;
    }
    step!(141);
    let started = step!(142) != 0;
    step!(143);
    if started {
        step!(144);
    }
    started as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_shutdown(c: *mut c_void, op: OpArg) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    step!(150);
    if step!(151) != 0 {
        step!(152);
    }
    if step!(153) != 0 && step!(154) != 0 {
        step!(155);
    }
    if step!(156) != 0 && step!(157) != 0 {
        step!(158);
    }
    step!(159);
}

/// Presentation admission, handover, paint, native blit and reporting order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_present(c: *mut c_void, op: OpArg) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(160) != 0 {
        return 0;
    }
    if step!(161) != 0 {
        return 1;
    }
    if step!(162) == 0 || step!(163) == 0 || step!(164) == 0 {
        step!(165);
        return 0;
    }
    step!(166);
    step!(167);
    step!(168); // native Done scope follows the paint gate
    if step!(169) == 0 {
        step!(170);
        return 0;
    }
    step!(171);
    if step!(172) == 0 {
        step!(170);
        return 0;
    }
    step!(173);
    step!(174);
    step!(175);
    step!(176);
    if step!(177) == 0 {
        step!(170);
        return 0;
    }
    step!(178);
    if step!(179) == 0 {
        step!(180);
        return 0;
    }
    if step!(181) == 0 {
        step!(170);
        return 0;
    }
    let okay = step!(182) != 0;
    if okay {
        step!(183);
    }
    step!(184);
    if okay && step!(185) != 0 {
        step!(186);
    }
    if okay {
        step!(187);
    } else {
        step!(188);
    }
    step!(189);
    if okay && (step!(190) != 0 || step!(191) != 0) {
        step!(192);
    }
    if okay && step!(193) != 0 {
        step!(194);
    }
    step!(195);
    okay as u8
}

/// Active dialog message path; native callback owns message projection,
/// USER32 dispatch, painting, artwork and reentrant lifetime scopes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_window_active(c: *mut c_void, op: OpArg) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(200) != 0 {
        step!(201);
    }
    if step!(202) != 0 {
        step!(203);
    }
    if step!(204) != 0 {
        step!(205);
    }
    if step!(206) != 0 && step!(207) != 0 {
        step!(208);
        return 1;
    }
    if step!(209) != 0 && step!(210) == 0 {
        step!(211);
    }
    if step!(212) != 0 && step!(213) != 0 && step!(214) != 0 {
        step!(215);
        return 1;
    }
    if step!(216) != 0 && step!(217) != 0 {
        step!(218);
        step!(219);
        if step!(220) != 0 {
            step!(221);
        }
        if step!(222) != 0 && step!(223) != 0 && step!(224) != 0 {
            step!(225);
        }
        step!(226);
    }
    step!(227); // forwarded flag before any reentrant native dispatch
    step!(228); // native HoverDispatch/LaunchDispatch scopes
    if step!(229) != 0 {
        if step!(230) != 0 {
            step!(231);
        } else if step!(232) != 0 {
            step!(233);
        }
    }
    0
}

type IndexedOp = unsafe extern "C" fn(*mut c_void, u32, usize) -> u64;
fn indexed(c: *mut c_void, op: IndexedOp, id: u32, index: usize) -> Option<u64> {
    let value = unsafe { op(c, id, index) };
    (value != ERROR).then_some(value)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_restore_logs(c: *mut c_void, op: IndexedOp) {
    macro_rules! step {
        ($id:expr, $index:expr) => {
            match indexed(c, op, $id, $index) {
                Some(value) => value,
                None => return,
            }
        };
    }
    let mut index = 0;
    while index < step!(300, 0) as usize {
        if step!(301, index) != 0 {
            step!(302, index); // clear ownership before reentrant USER32 call
            if step!(303, index) != 0
                && step!(304, index) != 0
                && step!(305, index) == 0
                && step!(306, index) != 0
                && step!(307, index) != 0
            {
                step!(308, index);
            }
        }
        index += 1;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_hide_logs(c: *mut c_void, op: IndexedOp) -> u8 {
    macro_rules! step {
        ($id:expr, $index:expr) => {
            match indexed(c, op, $id, $index) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    let mut index = 0;
    while index < step!(310, 0) as usize {
        if step!(311, index) != 0 {
            step!(312, index);
            if step!(313, index) == 0 || step!(314, index) == 0 {
                step!(315, index);
                step!(316, index);
                return 0;
            }
        }
        index += 1;
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_snapshot(c: *mut c_void, op: OpArg) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 400..=404 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(405);
    step!(406); // native widget model iteration
    step!(407); // native child scan
    if step!(408) != 0 {
        step!(409);
        return 0;
    }
    let key = step!(410);
    if key != step!(411) {
        step!(412);
    }
    if step!(413) == 0 {
        return 0;
    }
    step!(414); // arrange and publish native model/geometry
    let valid = step!(415) != 0;
    if valid {
        step!(416);
    } else if step!(417) != 0 {
        step!(418);
    }
    valid as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_seed(c: *mut c_void, op: OpArg) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 500..=507 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(508);
    step!(509);
    for id in 510..=513 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(514) == 0 {
        return 0;
    }
    step!(515);
    let restored = step!(516) != 0;
    let drawn = step!(517) != 0;
    (drawn && restored) as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_erase(c: *mut c_void, op: OpArg) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(520) == 0 || step!(521) != 0 || step!(522) == 0 {
        return 0;
    }
    let bridging = step!(523) != 0;
    if step!(524) != 0 && !bridging && step!(525) != 0 && step!(526) != 0 {
        return (step!(527) != 0) as u8;
    }
    if step!(528) == 0 || !bridging {
        return 0;
    }
    step!(529);
    if step!(530) == 0 || step!(531) == 0 {
        return 0;
    }
    step!(532);
    let restored = step!(533) != 0;
    let drawn = step!(534) != 0;
    (restored && drawn) as u8
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_admission(c: *mut c_void, op: OpArg, reason: u32) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(600) >= 64 && step!(601) == 0 {
        return;
    }
    let seen = step!(602) as u32;
    if seen & reason != 0 {
        return;
    }
    step!(603);
    step!(604);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_widget(c: *mut c_void, op: OpArg) {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return,
            }
        };
    }
    if step!(700) == 0 || step!(701) == 0 {
        return;
    }
    step!(702);
    if step!(703) < 4096 {
        step!(704);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_setup_control(c: *mut c_void, op: OpArg) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(800) == 0 {
        return 0;
    }
    let vt = step!(801);
    if vt < step!(802) || vt >= step!(803) || vt == step!(804) {
        return 0;
    }
    if step!(805) == 0 {
        return 0;
    }
    let combo = step!(806) != 0;
    let button = combo || step!(807) != 0;
    let label = step!(808) != 0;
    if !button && !label {
        return 0;
    }
    if step!(809) == 0 {
        return 0;
    }
    step!(810);
    if label {
        if step!(811) == 0 || step!(812) == 0 {
            return 0;
        }
        step!(813);
        if step!(814) != 0 {
            step!(815);
        }
        return 1;
    }
    if step!(816) == 0 || step!(817) == 0 {
        return 0;
    }
    step!(818);
    if !combo {
        return 1;
    }
    step!(819);
    for id in 820..=823 {
        if step!(id) == 0 {
            return 0;
        }
    }
    let count = step!(824) as u32 as i32;
    let selected = step!(825) as u32 as i32;
    if !(0..=128).contains(&count) || selected < -1 || selected >= count {
        return 0;
    }
    if selected >= 0 && step!(826) == 0 {
        return 0;
    }
    step!(827);
    if step!(828) != 0 {
        let height = step!(829) as u32 as i32;
        if !(4..=64).contains(&height) || step!(830) == 0 {
            return 0;
        }
        for _ in 0..count {
            if step!(831) == 0 {
                return 0;
            }
        }
    }
    1
}
