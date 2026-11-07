//! Ordering for the A1 wireframe fallback and station placement observers.
//! Every operation is a synchronous native primitive; hooks and raw SDK stay C++.
use core::ffi::c_void;
type Op = unsafe extern "C" fn(*mut c_void, u32) -> u64;
fn call(c: *mut c_void, op: Op, id: u32) -> u64 {
    unsafe { op(c, id) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 {
        return 0;
    }
    call(c, op, 0); // save API and obtain Armada image
    if call(c, op, 1) == 0 {
        call(c, op, 4);
        return 1;
    }
    if call(c, op, 2) == 0 {
        call(c, op, 5);
        return 1;
    }
    call(c, op, 3); // publish ready only after successful patch
    call(c, op, 6);
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_dispatch(c: *mut c_void, op: Op) -> usize {
    let native = call(c, op, 0) as usize;
    if call(c, op, 1) == 0 || call(c, op, 2) == 0 || call(c, op, 3) != 0 {
        return native;
    }
    if call(c, op, 4) == 0 {
        return native;
    }
    if call(c, op, 5) == 0 || call(c, op, 10) != 0 {
        return native;
    }
    let build = call(c, op, 6);
    if call(c, op, 10) != 0 {
        return native;
    }
    if build != 0 {
        return call(c, op, 8) as usize;
    }
    let faction = call(c, op, 7);
    if call(c, op, 10) != 0 {
        return native;
    }
    if faction != 0 {
        return call(c, op, 9) as usize;
    }
    native
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_report(c: *mut c_void, op: Op) {
    if call(c, op, 0) >= 64 {
        return;
    }
    if call(c, op, 1) == 0 {
        return;
    }
    call(c, op, 2);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_shutdown(c: *mut c_void, op: Op) {
    for id in 0..3 {
        call(c, op, id);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_station_init(valid: u8, c: *mut c_void, op: Op) -> u8 {
    if valid == 0 {
        return 0;
    }
    if call(c, op, 0) == 0 {
        return 0;
    }
    if call(c, op, 1) != 0 {
        call(c, op, 11);
        return 1;
    }
    let mut ok = call(c, op, 2) != 0;
    if ok {
        for id in 3..10 {
            if call(c, op, id) == 0 {
                ok = false;
                break;
            }
        }
    }
    call(c, op, if ok { 10 } else { 15 });
    call(c, op, if ok { 13 } else { 14 });
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_station_enabled(
    activation: u64,
    previous: *mut u64,
    c: *mut c_void,
    op: Op,
) -> u8 {
    if !previous.is_null() {
        let old = unsafe { *previous };
        if old != activation {
            call(c, op, 0);
            unsafe { *previous = activation };
        }
    }
    u8::from(activation != 0)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_station_queue(
    command: u32,
    enabled: u8,
    same_generation: u8,
    same_class: u8,
    has_class: u8,
    turns: u32,
) -> u32 {
    if enabled != 0 && same_generation != 0 && command == 0x19 && same_class != 0 && has_class != 0
    {
        if turns & 3 != 0 {
            0xb1 + (turns & 3) - 1
        } else {
            0x19
        }
    } else {
        command
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_station_broadcast(action: u32, c: *mut c_void, op: Op) -> u8 {
    call(c, op, 0); // preserve and clear sending state
    if call(c, op, 4) != 0 && action == 0x19 {
        call(c, op, 1);
    }
    let result = call(c, op, 2); // original broadcast
    call(c, op, 3); // restore sending state
    u8::from(result != 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_station_shutdown(c: *mut c_void, op: Op) {
    for id in 0..3 {
        call(c, op, id);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_station_keyboard(c: *mut c_void, op: Op) -> i32 {
    let result = call(c, op, 0) as u32 as i32;
    if call(c, op, 1) != 0 && result == 0 {
        call(c, op, 2);
    }
    result
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_station_set_command(turns: u32, c: *mut c_void, op: Op) {
    if turns != 0 {
        call(c, op, 0);
    } // replace tagged wire order with native build
    if call(c, op, 1) == 0 {
        return;
    } // native SetCommand clears parameter
    if turns != 0 {
        call(c, op, 2);
    } // write tagged angle after native call
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_station_preview(c: *mut c_void, op: Op) {
    call(c, op, 0); // selected class and observe
    let flags = call(c, op, 1) as u32;
    let rotate = flags & 31 == 31;
    if rotate {
        call(c, op, 2);
    } // save and rotate native basis
    call(c, op, 3); // original preview
    if rotate {
        call(c, op, 4);
    } // restore basis
}
