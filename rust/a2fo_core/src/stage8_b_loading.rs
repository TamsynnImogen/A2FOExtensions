//! Loading observer admission and hook publication, with native resources in C++.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32, u32) -> u64;
const ERROR: u64 = u64::MAX;
fn call(c: *mut c_void, op: Op, id: u32, arg: u32) -> Option<u64> {
    let value = unsafe { op(c, id, arg) };
    (value != ERROR).then_some(value)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_init(c: *mut c_void, op: Op) -> u8 {
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
    }
    if step!(1) != 0 {
        return 1;
    }
    step!(2); // record API and engine pointers before signature reads
    for id in 3..=8 {
        if step!(id) == 0 {
            step!(9);
            return 0;
        }
    }
    if step!(10) != 0 {
        let count = step!(11);
        if count > 32 {
            return 0;
        }
        for index in 0..count {
            step!(12, index as u32);
        }
    }
    if step!(13) != 0 {
        step!(14);
    }
    if step!(15) == 0 {
        return 0;
    }
    step!(16); // publish gateway
    if step!(17) != 0 {
        step!(18);
    } else {
        step!(19);
    }
    let status = if step!(20) != 0 && step!(21) != 0 {
        step!(22) != 0
    } else {
        false
    };
    if status {
        step!(23);
    }
    if step!(24) != 0 && (!status || step!(25) == 0) {
        step!(26);
    }
    step!(27); // publish enable state
    step!(28);
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_mission(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 0..=7 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(8);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_release(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(29) == 0 {
        return 0;
    } // release-tail signature
    if step!(30) != 0 {
        if step!(31) == 0 {
            return 0;
        }
        step!(32); // native gateway publication
        return 1;
    }
    if step!(33) == 0 || step!(34) == 0 {
        return 0;
    }
    let bytes = step!(35); // first and sixth bytes of the transfer
    let first = bytes & 0xff;
    let sixth = (bytes >> 8) & 0xff;
    let transfer = if first == 0x68 && sixth == 0xc3 {
        1
    } else if first == 0xe9 {
        2
    } else {
        0
    };
    if transfer == 0 {
        return 0;
    }
    if call(c, op, 36, transfer).is_none() {
        return 0;
    }
    if step!(37) == 0 || step!(38) == 0 {
        return 0;
    }
    if step!(39) == 0
        && (step!(40) == 0 || step!(41) == 0 || step!(42) == 0 || step!(43) == 0 || step!(44) == 0)
    {
        return 0;
    }
    step!(45); // publish original target before checked jump
    if step!(46) == 0 {
        step!(47);
        return 0;
    }
    step!(48);
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_animation(c: *mut c_void, op: Op) -> u8 {
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
    if step!(49) == 0 {
        return 0;
    }
    step!(50); // construct checked native instruction patterns
    for id in 51..=54 {
        if step!(id) == 0 {
            return 0;
        }
    }
    if step!(55) == 0 {
        return 0;
    } // kill observer first
    step!(56); // publish kill gateway
    if step!(57) == 0 {
        return 0;
    }
    step!(58); // publish splash gateway
    if step!(59) != 0 {
        step!(60);
        step!(61);
    } else {
        step!(62);
    }
    step!(63); // animation gate after both splash hooks
    let mut inner = 0;
    for index in 0..3 {
        inner += step!(64, index);
    }
    if step!(65) != 0 && step!(66) != 0 && step!(67) != 0 {
        step!(68); // publish observed pixel target
        if step!(69) != 0 {
            inner += 1;
        } else {
            step!(70);
        }
    }
    step!(71, inner as u32); // native formatted diagnostic
    1
}

/// Preserve the native bar's callback order, including its caught presentation
/// failure and the mandatory original Fleet Ops callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_bar(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(72) != 0 && step!(73) != 0 && step!(74) != 0 {
        step!(75); // thread-local recursion gate
        let result = step!(76); // 0 unavailable, 1 drawn, 2 caught failure
        let logged = if result == 1 {
            step!(77)
        } else if result == 0 {
            step!(78)
        } else {
            2
        };
        if logged == 2 {
            step!(79);
        }
        step!(80);
    }
    if step!(81) != 0 {
        step!(82);
    }
    step!(83); // untouched original callback exactly once
    if step!(84) != 0 {
        step!(85);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_splash_begin(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(86) != 0
        && step!(87) != 0
        && step!(88) != 0
        && step!(89) != 0
        && step!(90) != 0
        && step!(91) != 0
    {
        step!(92); // presentation handover failure is deliberately contained
    }
    let continuing = step!(93) != 0;
    if continuing {
        step!(95);
    } else {
        step!(94);
    }
    if step!(96) != 0 && step!(97) != 0 {
        step!(98);
    }
    step!(99); // original splash constructor
    if step!(100) != 0 {
        if continuing {
            step!(101);
        } else {
            step!(102);
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_splash_end(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(103) != 0 && step!(104) != 0 {
        for id in 105..=109 {
            step!(id);
        }
    }
    step!(110); // original teardown after native cleanup
    1
}

/// Frame admission and loading presentation phases. Device/bitmap ownership,
/// engine callbacks, and the x87-sensitive native rectangle geometry stay in C++.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_present(
    c: *mut c_void,
    op: Op,
    progress: f32,
) -> u8 {
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
    if !progress.is_finite() || step!(112) == 0 || step!(113) == 0 {
        return 0;
    }
    let width = step!(114);
    let height = step!(115);
    if width < 320 || height < 240 || width > 3840 || height > 2160 || width * height > 8_000_000 {
        return 0;
    }
    step!(116); // map details
    step!(117); // current menu epoch
    let restart = step!(118) != 0
        || (step!(119) != 0 && (step!(120) != 0 || step!(121) != 0 || step!(122) != 0));
    if step!(119) == 0 || restart {
        for id in 123..=127 {
            step!(id);
        }
        if step!(128) != 0 {
            step!(129);
        }
        step!(130);
        step!(131);
    }
    step!(132);
    step!(133, progress.clamp(0.0, 1.0).to_bits());
    step!(134);
    if step!(135) != 0 && step!(136) != 0 {
        if step!(137) != 0 {
            step!(138);
        }
        if step!(139) != 0 {
            step!(140);
        }
        if step!(141) != 0 {
            step!(142);
        }
    }
    if step!(143) != 0 {
        step!(144);
    }
    step!(145);
    if step!(146) == 0 {
        return 0;
    }
    step!(147, restart as u32);
    step!(148);
    if step!(149) == 0 {
        step!(150);
    }
    step!(151);
    let layered = step!(152) != 0;
    step!(153);
    if (step!(154) != 0 || step!(155) != 0 || step!(156) != 0 || step!(157) != 0) && step!(158) == 0
    {
        return 0;
    }
    step!(159);
    let cached = step!(160) != 0;
    if layered {
        let drawn = step!(161, cached as u32) != 0;
        if drawn {
            step!(162);
        }
        return drawn as u8;
    }
    if step!(160) != 0 {
        (step!(163) != 0) as u8
    } else {
        (step!(164) != 0) as u8
    }
}

/// Idle loading frame sequence; native owns COM state blocks and all callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_extra_frame(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    for id in 165..=170 {
        if step!(id) == 0 {
            return 0;
        }
    }
    step!(171);
    if step!(172) != 0 || step!(173) == 0 || step!(174) == 0 {
        return 0;
    }
    let remaining = f32::from_bits(step!(175) as u32);
    if !remaining.is_finite()
        || !(0.0..=1.0).contains(&remaining)
        || step!(176) == 0
        || step!(177) == 0
        || step!(178) == 0
        || step!(179) == 0
        || step!(180) == 0
        || step!(181) == 0
        || step!(182) == 0
    {
        return 0;
    }
    step!(183);
    step!(184);
    let mut drawn = step!(185) == 1;
    if drawn {
        drawn = step!(186) == 1;
    }
    let ended = step!(187) != 0;
    if drawn && ended && step!(188) != 0 {
        step!(189);
    } else {
        step!(190);
    }
    step!(191);
    step!(192);
    1
}

/// Player panel admission and slot walk. Native retains engine calls, theme
/// strings, transport objects, and the original float clamping primitive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_players(c: *mut c_void, op: Op) -> u8 {
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
    step!(200); // clear model players
    if step!(201) == 0 || step!(202) == 0 || step!(203) == 0 {
        return 0;
    }
    let instant = step!(204) != 0;
    let single = step!(205) != 0;
    step!(
        206,
        if instant {
            1
        } else if single {
            2
        } else {
            3
        }
    );
    if step!(207) == 0 {
        return 0;
    }
    for slot in 0..8 {
        if step!(208, slot) != 0 || step!(209, slot) == 0 {
            continue;
        }
        step!(210, slot);
        step!(211, slot);
        if step!(212) != 0 {
            step!(213, slot);
        }
        step!(214);
        let team = step!(215, slot) as u32 as i32;
        step!(216, team as u32);
        if !(0..=8).contains(&team) {
            step!(231);
        }
        if !single && step!(217) != 0 && step!(218) != 0 && step!(219, slot) != 0 {
            let fraction = f32::from_bits(step!(220, slot) as u32);
            if fraction.is_finite() {
                step!(221);
            }
        }
        if step!(222, slot) != 0 {
            step!(223);
        }
        step!(224);
    }
    if single && !instant && step!(225) != 0 && step!(226) != 0 {
        step!(227);
    }
    if step!(228) != 0 {
        step!(229);
    }
    step!(230);
    1
}

/// Exact race-hook coexistence admission followed by the six slot signatures.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_player_bindings(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    let mut race = step!(232) != 0;
    if !race && step!(233) != 0 && step!(234) != 0 && step!(235) != 0 {
        step!(236);
        if step!(237) != 0 && step!(238) != 0 {
            race = step!(239) != 0;
            if !race && step!(240) != 0 {
                race = step!(241) != 0;
            }
        }
        if race {
            step!(242);
        }
    }
    if !race {
        return 0;
    }
    for id in 243..=248 {
        if step!(id) == 0 {
            return 0;
        }
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_loading_status(c: *mut c_void, op: Op) -> u8 {
    macro_rules! step {
        ($id:expr) => {
            match call(c, op, $id, 0) {
                Some(value) => value,
                None => return 0,
            }
        };
    }
    if step!(249) != 0 {
        step!(250);
    }
    step!(251); // native status callback before frame acknowledgment
    if step!(252) != 0 && step!(253) != 0 && step!(254) != 0 && step!(255) != 0 {
        step!(256);
    }
    step!(257);
    1
}
