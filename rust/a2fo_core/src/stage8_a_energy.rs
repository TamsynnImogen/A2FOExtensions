//! Portable lifecycle sequencing for ammunition stores and resupply.

use core::ffi::c_void;

type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

unsafe fn call(step: Step, context: *mut c_void, op: u32, arg: usize) -> Option<usize> {
    let value = unsafe { step(context, op, arg) };
    (value != usize::MAX).then_some(value)
}

unsafe fn install_hooks(step: Step, context: *mut c_void, kind: usize) -> Option<bool> {
    let signature_count = if kind == 0 { 6 } else { 4 };
    let install_count = if kind == 0 { 6 } else { 2 };
    for index in 0..signature_count {
        match unsafe { call(step, context, 22, kind * 256 + index) } {
            Some(1) => {}
            Some(_) => return Some(false),
            None => return None,
        }
    }
    for index in 0..install_count {
        match unsafe { call(step, context, 23, kind * 256 + index) } {
            Some(1) => {}
            Some(_) => return Some(false),
            None => return None,
        }
    }
    Some(true)
}

/// Safety: the native context retains its SDK pointers for the synchronous
/// pass and captures C++ exceptions for rethrow after Rust returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_init(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return 0;
    }
    if unsafe { call(step, context, 2, 0) }.is_none() {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 3, 0) }, Some(1)) {
        return 0;
    }
    for index in 0..7 {
        if !matches!(unsafe { call(step, context, 4, index) }, Some(1)) {
            return 0;
        }
    }
    if unsafe { call(step, context, 5, 0) }.is_none() {
        return 0;
    }
    for op in 6..=8 {
        match unsafe { call(step, context, op, 0) } {
            Some(1) => {}
            Some(_) => {
                unsafe { call(step, context, 9, 0) };
                return 0;
            }
            None => return 0,
        }
    }
    let trigger = match unsafe { call(step, context, 10, 0) } {
        Some(value) => value != 0,
        None => return 0,
    };
    let alias = match unsafe { call(step, context, 11, 0) } {
        Some(value) => value != 0,
        None => return 0,
    };
    let signature = if trigger && alias {
        match unsafe { call(step, context, 12, 0) } {
            Some(value) => value != 0,
            None => return 0,
        }
    } else {
        false
    };
    if unsafe {
        call(
            step,
            context,
            13,
            usize::from(trigger && alias && signature) | (usize::from(alias) << 1),
        )
    }
    .is_none()
    {
        return 0;
    }
    match unsafe { install_hooks(step, context, 0) } {
        Some(true) => {}
        Some(false) => {
            unsafe { call(step, context, 9, 0) };
            return 0;
        }
        None => return 0,
    }
    let persistence = match unsafe { install_hooks(step, context, 1) } {
        Some(value) => value,
        None => return 0,
    };
    if unsafe { call(step, context, 24, usize::from(persistence)) }.is_none() {
        return 0;
    }
    let mut native_ui = true;
    for index in 0..3 {
        if !native_ui {
            break;
        }
        native_ui = match unsafe { call(step, context, 16, index) } {
            Some(value) => value != 0,
            None => return 0,
        };
    }
    let ui_enabled = match unsafe { call(step, context, 21, 0) } {
        Some(value) => value != 0,
        None => return 0,
    };
    let observer = if ui_enabled
        && native_ui
        && matches!(unsafe { call(step, context, 25, 0) }, Some(1))
        && matches!(unsafe { call(step, context, 26, 0) }, Some(1))
    {
        match unsafe { call(step, context, 27, 0) } {
            Some(value) => value != 0,
            None => return 0,
        }
    } else {
        false
    };
    if unsafe {
        call(
            step,
            context,
            18,
            usize::from(ui_enabled && native_ui && observer),
        )
    }
    .is_none()
    {
        return 0;
    }
    if unsafe { call(step, context, 19, 0) }.is_none() {
        return 0;
    }
    if unsafe { call(step, context, 20, 0) }.is_none() {
        return 0;
    }
    1
}

/// Safety: native callback owns its CraftPolicy and copies each ODF field at
/// the original read point. C++ exceptions are reported as `usize::MAX`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_craft_class(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    for field in 0..9 {
        if unsafe { call(step, context, 1, field) }.is_none() {
            return;
        }
    }
    for store in 0..3 {
        if unsafe { call(step, context, 2, store) }.is_none() {
            return;
        }
    }
    for op in 3..=5 {
        if unsafe { call(step, context, op, 0) }.is_none() {
            return;
        }
    }
    let mut metadata_failed = false;
    let has_self = match unsafe { call(step, context, 6, 0) } {
        Some(value) => value != 0,
        None => {
            metadata_failed = true;
            false
        }
    };
    if !metadata_failed && has_self {
        let valid = match unsafe { call(step, context, 7, 0) } {
            Some(value) => value != 0,
            None => {
                metadata_failed = true;
                true
            }
        };
        if !metadata_failed && !valid && unsafe { call(step, context, 8, 0) }.is_none() {
            metadata_failed = true;
        }
    }
    if !metadata_failed {
        for op in 9..=11 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                metadata_failed = true;
                break;
            }
        }
    }
    if metadata_failed && unsafe { call(step, context, 12, 0) }.is_none() {
        return;
    }
    let mut has_maximum = false;
    for store in 0..3 {
        if has_maximum {
            break;
        }
        match unsafe { call(step, context, 13, store) } {
            Some(value) => has_maximum |= value != 0,
            None => return,
        }
    }
    if !has_maximum {
        match unsafe { call(step, context, 13, 3) } {
            Some(0) => return,
            Some(_) => {}
            None => return,
        }
    }
    let mut has_store = false;
    for store in 0..3 {
        if has_store {
            break;
        }
        match unsafe { call(step, context, 14, store) } {
            Some(value) => has_store |= value != 0,
            None => return,
        }
    }
    if has_store {
        if unsafe { call(step, context, 15, 0) }.is_none() {
            return;
        }
        let mut requires = false;
        for store in 0..3 {
            if requires {
                break;
            }
            match unsafe { call(step, context, 16, store) } {
                Some(value) => requires |= value != 0,
                None => return,
            }
        }
        if requires && unsafe { call(step, context, 17, 0) }.is_none() {
            return;
        }
    }
    unsafe { call(step, context, 18, 0) };
}

/// Safety: native context owns resupply filter/string storage and copied
/// scalar costs for the whole synchronous class callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_weapon_class(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let original = unsafe { call(step, context, 2, 0) };
    let has_label = match original {
        Some(1) => true,
        Some(_) => matches!(unsafe { call(step, context, 3, 0) }, Some(1)),
        None => false,
    };
    let resupply = has_label && matches!(unsafe { call(step, context, 4, 0) }, Some(1));
    if resupply {
        let parsed = unsafe { call(step, context, 5, 0) }.is_some();
        let retained = parsed && unsafe { call(step, context, 6, 0) }.is_some();
        if !retained && unsafe { call(step, context, 7, 0) }.is_none() {
            return;
        }
    }
    for index in 0..3 {
        if unsafe { call(step, context, 8, index) }.is_none() {
            return;
        }
    }
    let mut positive_count = 0;
    let mut selected = 0;
    for index in 0..3 {
        match unsafe { call(step, context, 9, index) } {
            Some(1) => {
                positive_count += 1;
                if positive_count == 1 {
                    selected = index;
                }
            }
            Some(_) => {}
            None => return,
        }
    }
    if positive_count == 0 {
        return;
    }
    if positive_count != 1 {
        unsafe { call(step, context, 10, 0) };
        return;
    }
    unsafe { call(step, context, 11, selected) };
}

/// Safety: native context owns Craft/Stores pointers for this synchronous
/// event pass. Admission precedes every event field and craft read.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_craft_event(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    let Some(kind) = (unsafe { call(step, context, 1, 0) }) else {
        return;
    };
    if kind == 2 {
        for op in 2..=4 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                return;
            }
        }
        return;
    }
    if (kind == 0 || kind == 1) && !matches!(unsafe { call(step, context, 5, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 6, 0) }, Some(1)) {
        return;
    }
    if (kind == 0 || kind == 3) && matches!(unsafe { call(step, context, 7, 0) }, Some(1)) {
        if unsafe { call(step, context, 8, 0) }.is_none() {
            return;
        }
        if kind == 0 && unsafe { call(step, context, 9, 0) }.is_none() {
            return;
        }
    }
    if kind != 0 && kind != 1 && kind != 3 {
        return;
    }
    let mut enabled = false;
    for store in 0..3 {
        if enabled {
            break;
        }
        match unsafe { call(step, context, 10, store) } {
            Some(value) => enabled = value != 0,
            None => return,
        }
    }
    if !enabled {
        return;
    }
    if kind == 0 || kind == 3 {
        unsafe { call(step, context, 11, 0) };
        return;
    }
    if !matches!(unsafe { call(step, context, 11, 0) }, Some(1)) {
        return;
    }
    let mut requires = false;
    for store in 0..3 {
        if requires {
            break;
        }
        match unsafe { call(step, context, 12, store) } {
            Some(value) => requires = value != 0,
            None => return,
        }
    }
    let supplied = if requires {
        match unsafe { call(step, context, 13, 0) } {
            Some(value) => value != 0,
            None => return,
        }
    } else {
        true
    };
    if unsafe { call(step, context, 14, 0) }.is_none() {
        return;
    }
    for store in 0..3 {
        if unsafe { call(step, context, 15, store | (usize::from(supplied) << 2)) }.is_none() {
            return;
        }
    }
    if !requires || !matches!(unsafe { call(step, context, 16, 0) }, Some(1)) {
        return;
    }
    let mut replenished = false;
    for store in 0..3 {
        if replenished {
            break;
        }
        match unsafe { call(step, context, 17, store) } {
            Some(2) => {
                // These two native operations only copy bits; every f32 bit
                // pattern, including the callback sentinel, is valid data.
                let old = f32::from_bits(unsafe { step(context, 19, store) } as u32);
                let new = f32::from_bits(unsafe { step(context, 20, store) } as u32);
                replenished = new > old;
            }
            Some(_) => {}
            None => return,
        }
    }
    if replenished {
        unsafe { call(step, context, 18, 0) };
    }
}

/// Safety: the native context owns its original unordered-set iterator and
/// policy pointers. Each provider is sampled after preceding credit callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_in_range(
    step: Option<Step>,
    context: *mut c_void,
    credit: u8,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return 0;
    }
    if unsafe { call(step, context, 2, 0) }.is_none() {
        return 0;
    }
    let mut found = false;
    loop {
        if !matches!(unsafe { call(step, context, 3, 0) }, Some(1)) {
            break;
        }
        let mut eligible = true;
        for op in 4..=7 {
            if !matches!(unsafe { call(step, context, op, 0) }, Some(1)) {
                eligible = false;
                break;
            }
        }
        if eligible {
            if credit == 0 {
                return 1;
            }
            found = true;
            if unsafe { call(step, context, 8, 0) }.is_none() {
                return 0;
            }
        }
        if unsafe { call(step, context, 9, 0) }.is_none() {
            return 0;
        }
    }
    u8::from(found)
}

/// Safety: native context owns the original Carrier vector bounds and reads
/// each live slot only after the previous trigger callback returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_trigger_resupply(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return;
    }
    let Some(count) = (unsafe { call(step, context, 2, 0) }) else {
        return;
    };
    if count == 0 {
        return;
    }
    for index in 0..(count - 1) {
        match unsafe { call(step, context, 3, index) } {
            Some(1) => {}
            Some(_) => continue,
            None => return,
        }
        match unsafe { call(step, context, 4, 0) } {
            Some(1) => {}
            Some(_) => continue,
            None => return,
        }
        match unsafe { call(step, context, 5, 0) } {
            Some(1) => match unsafe { call(step, context, 6, 0) } {
                Some(1) => continue,
                Some(_) => {}
                None => return,
            },
            Some(_) => {}
            None => return,
        }
        for op in 7..=10 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                return;
            }
        }
    }
}

/// Safety: native receiver vector is stable until the final trigger callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_credit_provider(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let Some(count) = (unsafe { call(step, context, 2, 0) }) else {
        return;
    };
    let mut existing = None;
    for index in 0..count {
        match unsafe { call(step, context, 3, index) } {
            Some(1) => {
                existing = Some(index);
                break;
            }
            Some(_) => {}
            None => return,
        }
    }
    let operation = if existing.is_some() { 4 } else { 5 };
    if unsafe { call(step, context, operation, existing.unwrap_or(0)) }.is_none() {
        return;
    }
    unsafe { call(step, context, 6, 0) };
}

/// Safety: native context owns the receiver vector and map iterator during
/// subtraction, stable compaction and final erase.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_age_provider(
    step: Option<Step>,
    context: *mut c_void,
    elapsed: f32,
) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if (!elapsed.is_finite() || elapsed < 0.0) && unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    let Some(count) = (unsafe { call(step, context, 2, 0) }) else {
        return;
    };
    for index in 0..count {
        if unsafe { call(step, context, 3, index) }.is_none() {
            return;
        }
    }
    let mut retained = 0;
    for index in 0..count {
        match unsafe { call(step, context, 4, index) } {
            Some(1) => {
                if retained != index
                    && (unsafe { call(step, context, 5, index) }.is_none()
                        || unsafe { call(step, context, 6, retained) }.is_none())
                {
                    return;
                }
                retained += 1;
            }
            Some(_) => {}
            None => return,
        }
    }
    if unsafe { call(step, context, 7, retained) }.is_none() {
        return;
    }
    if retained == 0 {
        unsafe { call(step, context, 8, 0) };
    }
}

/// Safety: native context owns both map iterators and each provider's vector
/// while Rust sequences stable filtering and target cleanup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_forget_craft(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    if unsafe { call(step, context, 0, 0) }.is_none() {
        return;
    }
    if unsafe { call(step, context, 1, 0) }.is_none() {
        return;
    }
    loop {
        if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
            break;
        }
        let Some(count) = (unsafe { call(step, context, 3, 0) }) else {
            return;
        };
        let mut retained = 0;
        for index in 0..count {
            match unsafe { call(step, context, 4, index) } {
                Some(1) => {
                    if retained != index
                        && (unsafe { call(step, context, 5, index) }.is_none()
                            || unsafe { call(step, context, 6, retained) }.is_none())
                    {
                        return;
                    }
                    retained += 1;
                }
                Some(_) => {}
                None => return,
            }
        }
        if unsafe { call(step, context, 7, retained) }.is_none() {
            return;
        }
        if unsafe { call(step, context, 8, 0) }.is_none() {
            return;
        }
    }
    if unsafe { call(step, context, 9, 0) }.is_none() {
        return;
    }
    loop {
        if !matches!(unsafe { call(step, context, 10, 0) }, Some(1)) {
            break;
        }
        match unsafe { call(step, context, 11, 0) } {
            Some(1) => {
                if unsafe { call(step, context, 12, 0) }.is_none() {
                    return;
                }
            }
            Some(_) => {
                if unsafe { call(step, context, 13, 0) }.is_none() {
                    return;
                }
            }
            None => return,
        }
    }
}

/// Safety: the native activity vector remains live during this lookup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_receiver_active(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let Some(count) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    for index in 0..count {
        match unsafe { call(step, context, 2, index) } {
            Some(1) => return 1,
            Some(_) => {}
            None => return 0,
        }
    }
    0
}

/// Safety: native callback owns the save block, reader/writer, stores and
/// policy. The original hook result is returned without changing high bits.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_persistence(
    step: Option<Step>,
    context: *mut c_void,
    load: u8,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    let native = unsafe { step(context, 0, 0) };
    if native & 0xff == 0 {
        return native;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return native;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return native;
    }
    if load == 0 {
        if unsafe { call(step, context, 3, 0) }.is_none() {
            return 0;
        }
        return usize::from(matches!(unsafe { call(step, context, 4, 0) }, Some(1)));
    }
    if !matches!(unsafe { call(step, context, 5, 0) }, Some(1)) {
        unsafe { call(step, context, 6, 0) };
        return 0;
    }
    if !matches!(unsafe { call(step, context, 7, 0) }, Some(1)) {
        return native;
    }
    let Some(current) = (unsafe { call(step, context, 8, 0) }) else {
        return 0;
    };
    if current == 1 {
        for store in 0..3 {
            if unsafe { call(step, context, 9, store) }.is_none() {
                return 0;
            }
        }
    } else {
        if !matches!(unsafe { call(step, context, 10, 0) }, Some(1)) {
            unsafe { call(step, context, 6, 0) };
            return 0;
        }
        for store in 0..2 {
            if unsafe { call(step, context, 11, store) }.is_none() {
                return 0;
            }
        }
    }
    native
}

unsafe fn shot_policy(step: Step, context: *mut c_void) -> bool {
    match unsafe { call(step, context, 2, 0) } {
        Some(0) => true,
        Some(_) => matches!(unsafe { call(step, context, 3, 0) }, Some(1)),
        None => false,
    }
}

unsafe fn shot_target(step: Step, context: *mut c_void, has_target: bool) -> bool {
    match unsafe { call(step, context, 4, 0) } {
        Some(0) => true,
        Some(_) if !has_target => false,
        Some(_) => {
            matches!(unsafe { call(step, context, 5, 0) }, Some(1))
                && matches!(unsafe { call(step, context, 6, 0) }, Some(1))
        }
        None => false,
    }
}

/// Safety: the native context owns its weapon/target map lookup and copies
/// provider identity before the receiver-activity callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_shot_allowed(
    step: Option<Step>,
    context: *mut c_void,
    has_target: u8,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    u8::from(unsafe { shot_target(step, context, has_target != 0) })
}

unsafe fn shot_debit(step: Step, context: *mut c_void) {
    if !matches!(unsafe { call(step, context, 11, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 12, 0) }, Some(1)) {
        return;
    }
    unsafe { call(step, context, 13, 0) };
}

/// Safety: native context owns weapon policy/store pointers only during each
/// synchronous step. Gateway and amount arithmetic retain their native ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_shot(
    step: Option<Step>,
    context: *mut c_void,
    kind: u32,
    has_target: u8,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if (kind == 0 || kind == 3 || kind == 4) && unsafe { call(step, context, 0, 0) }.is_none() {
        return 0;
    }
    let ready = matches!(unsafe { call(step, context, 1, 0) }, Some(1));
    match kind {
        0 | 3 | 4 => {
            if ready
                && (!unsafe { shot_policy(step, context) }
                    || !unsafe { shot_target(step, context, has_target != 0) })
            {
                return 0;
            }
            // Every uintptr bit pattern is a valid native gateway result.
            let result = unsafe { step(context, 7, kind as usize) };
            if result != 0 && matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
                unsafe { call(step, context, 8, 0) };
            }
            return result;
        }
        1 => {
            let configured = ready && matches!(unsafe { call(step, context, 2, 0) }, Some(1));
            if configured && !unsafe { shot_policy(step, context) } {
                if matches!(unsafe { call(step, context, 9, 0) }, Some(1)) {
                    unsafe { call(step, context, 10, 0) };
                }
                return 0;
            }
            unsafe { call(step, context, 14, kind as usize) };
            if configured {
                if matches!(unsafe { call(step, context, 9, 0) }, Some(1)) {
                    unsafe { call(step, context, 10, 0) };
                }
                unsafe { shot_debit(step, context) };
            }
        }
        2 => {
            let configured = ready
                && matches!(unsafe { call(step, context, 9, 0) }, Some(1))
                && matches!(unsafe { call(step, context, 2, 0) }, Some(1));
            if configured && !unsafe { shot_policy(step, context) } {
                unsafe { call(step, context, 10, 0) };
                return 0;
            }
            unsafe { call(step, context, 14, kind as usize) };
            if configured {
                unsafe { call(step, context, 10, 0) };
                unsafe { shot_debit(step, context) };
            }
        }
        5 => {
            if ready && matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
                if matches!(unsafe { call(step, context, 9, 0) }, Some(1)) {
                    unsafe { call(step, context, 10, 0) };
                }
                unsafe { shot_debit(step, context) };
            }
            unsafe { call(step, context, 14, kind as usize) };
        }
        _ => {}
    }
    0
}

unsafe fn amount_current(step: Step, context: *mut c_void) -> u32 {
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return 0;
    }
    // The native step copies all 32 bits of the stored float.
    unsafe { step(context, 3, 0) as u32 }
}

unsafe fn amount_set(step: Step, context: *mut c_void) {
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    let stores = matches!(unsafe { call(step, context, 1, 0) }, Some(1));
    let policy = matches!(unsafe { call(step, context, 2, 0) }, Some(1));
    if !stores || !policy {
        return;
    }
    unsafe { call(step, context, 8, 0) };
}

/// Safety: native callback owns CraftPolicy/Stores and preserves the original
/// arithmetic and clamping kernel for each exported API call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_amount(
    step: Option<Step>,
    context: *mut c_void,
    kind: u32,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    match kind {
        0 => unsafe { amount_current(step, context) },
        1 => {
            if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
                return 0;
            }
            if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
                return 0;
            }
            unsafe { step(context, 4, 0) as u32 }
        }
        2 => {
            if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
                return (-1.0f32).to_bits();
            }
            let stores = matches!(unsafe { call(step, context, 1, 0) }, Some(1));
            let policy = matches!(unsafe { call(step, context, 2, 0) }, Some(1));
            if !stores || !policy {
                return (-1.0f32).to_bits();
            }
            let mode = match unsafe { call(step, context, 5, 0) } {
                Some(value) => value,
                None => return (-1.0f32).to_bits(),
            };
            let supplied = if mode != 2 {
                true
            } else {
                matches!(unsafe { call(step, context, 6, 0) }, Some(1))
            };
            unsafe { step(context, 7, usize::from(supplied)) as u32 }
        }
        3 => {
            unsafe { amount_set(step, context) };
            0
        }
        4 => {
            let current = unsafe { amount_current(step, context) };
            if unsafe { call(step, context, 9, current as usize) }.is_none() {
                return 0;
            }
            unsafe { amount_set(step, context) };
            0
        }
        _ => 0,
    }
}

/// Safety: native callback owns ODF text and retains CRT float parsing and
/// locale-dependent byte folding. Only numeric bits and folded bytes cross.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_parse(
    step: Option<Step>,
    context: *mut c_void,
    kind: u32,
) -> i32 {
    let Some(step) = step else {
        return -1;
    };
    if kind != 2 && matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        let bits = unsafe { step(context, 1, 0) } as u32;
        let value = f32::from_bits(bits);
        if kind == 1 {
            return i32::from(value != 0.0);
        }
        if value == 0.0 {
            return 0;
        }
        if value == 1.0 {
            return 1;
        }
        if value == 2.0 {
            return 2;
        }
        return -1;
    }
    if kind == 0 {
        return -1;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return if kind == 2 { 0 } else { -1 };
    }
    let Some(length) = (unsafe { call(step, context, 3, 0) }) else {
        return if kind == 2 { 0 } else { -1 };
    };
    let mut folded = [0u8; 10];
    let mut count = 0;
    for index in 0..length {
        let Some(byte) = (unsafe { call(step, context, 4, index) }) else {
            return if kind == 2 { 0 } else { -1 };
        };
        if kind == 2 && byte == u32::from(b'"') as usize {
            continue;
        }
        if count >= folded.len() {
            return if kind == 2 { 0 } else { -1 };
        }
        folded[count] = byte as u8;
        count += 1;
    }
    let value = &folded[..count];
    if kind == 2 {
        return i32::from(value == b"shipyard" || value == b"repairship");
    }
    if value == b"true" || value == b"yes" || value == b"on" {
        return 1;
    }
    if value == b"false" || value == b"no" || value == b"off" {
        return 0;
    }
    -1
}

/// Safety: the native context owns the temporary UI configuration and all
/// parameter DB calls; Rust preserves primary/fallback and read order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_ui_refresh(
    step: Option<Step>,
    context: *mut c_void,
) {
    let Some(step) = step else {
        return;
    };
    let has_db = matches!(unsafe { call(step, context, 0, 0) }, Some(1));
    if has_db {
        let anchor = matches!(unsafe { call(step, context, 1, 0) }, Some(1));
        if !anchor && unsafe { call(step, context, 2, 0) }.is_none() {
            return;
        }
        for op in 3..=9 {
            if unsafe { call(step, context, op, 0) }.is_none() {
                return;
            }
        }
    }
    unsafe { call(step, context, 10, 0) };
}

/// Safety: native context owns the selected Craft, native rectangles, colours,
/// text buffers and draw interface throughout this synchronous render pass.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_ui_render(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return;
    }
    let mut has_maximum = false;
    for index in 0..3 {
        if has_maximum {
            break;
        }
        match unsafe { call(step, context, 2, index) } {
            Some(value) => has_maximum = value != 0,
            None => return,
        }
    }
    if !has_maximum {
        return;
    }
    for op in 3..=6 {
        if !matches!(unsafe { call(step, context, op, 0) }, Some(1)) {
            return;
        }
    }
    if unsafe { call(step, context, 7, 0) }.is_none() {
        return;
    }
    for index in 0..3 {
        let override_found = match unsafe { call(step, context, 8, index) } {
            Some(value) => value != 0,
            None => return,
        };
        if override_found && unsafe { call(step, context, 9, index) }.is_none() {
            return;
        }
    }
    if unsafe { call(step, context, 10, 0) }.is_none() {
        return;
    }
    let shared = match unsafe { call(step, context, 11, 0) } {
        Some(value) => value != 0,
        None => return,
    };
    for index in 0..3 {
        let specific = match unsafe { call(step, context, 12, index) } {
            Some(value) => value != 0,
            None => return,
        };
        let source = if specific {
            2
        } else if shared {
            1
        } else {
            0
        };
        if unsafe { call(step, context, 13, index | (source << 2)) }.is_none() {
            return;
        }
    }
    let supplied = match unsafe { call(step, context, 14, 0) } {
        Some(value) => value != 0,
        None => return,
    };
    for index in 0..3 {
        match unsafe { call(step, context, 2, index) } {
            Some(0) => continue,
            Some(_) => {}
            None => return,
        }
        let Some(mode) = (unsafe { call(step, context, 15, index) }) else {
            return;
        };
        let status = if mode == 1 {
            1
        } else if mode == 2 {
            if supplied { 2 } else { 3 }
        } else {
            0
        };
        if unsafe { call(step, context, 16, index | (status << 2)) }.is_none() {
            return;
        }
        if unsafe { call(step, context, 17, index) }.is_none() {
            return;
        }
    }
}

/// Safety: native callback checks the event size before exposing the weapon.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_trigger_precheck(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 1;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 1;
    }
    if matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return 1;
    }
    u8::from(!matches!(unsafe { call(step, context, 2, 0) }, Some(1)))
}

/// Safety: target and provider are sampled from native storage only after
/// readiness and class admission; no native reference crosses callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_resupply_state(
    step: Option<Step>,
    context: *mut c_void,
) -> i32 {
    let Some(step) = step else {
        return -1;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return -1;
    }
    if !matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return -1;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return 0;
    }
    i32::from(matches!(unsafe { call(step, context, 3, 0) }, Some(1)))
}

/// Safety: native callback writes only the module-owned lifecycle fields.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_shutdown(step: Option<Step>, context: *mut c_void) {
    let Some(step) = step else {
        return;
    };
    if unsafe { call(step, context, 0, 0) }.is_some() {
        unsafe { call(step, context, 1, 0) };
    }
}

/// Safety: native callback reads one borrowed ODF view at a time and owns the
/// destination string. CRT name comparison and exception propagation remain
/// in the native wrapper.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_field(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let Some(count) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    for index in 0..count {
        match unsafe { call(step, context, 2, index) } {
            Some(1) => {
                return u8::from(matches!(unsafe { call(step, context, 3, index) }, Some(1)));
            }
            Some(_) => {}
            None => return 0,
        }
    }
    0
}

/// Safety: native context owns map iterators and preserves the original
/// exception containment for insertion into module-owned store storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_stores(
    step: Option<Step>,
    context: *mut c_void,
    create: u8,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    if matches!(unsafe { call(step, context, 1, 0) }, Some(1)) {
        return 1;
    }
    if create == 0 {
        return 0;
    }
    if !matches!(unsafe { call(step, context, 2, 0) }, Some(1)) {
        return 0;
    }
    let mut positive = false;
    for index in 0..3 {
        if positive {
            break;
        }
        match unsafe { call(step, context, 3, index) } {
            Some(value) => positive = value != 0,
            None => return 0,
        }
    }
    if !positive {
        return 0;
    }
    u8::from(matches!(unsafe { call(step, context, 4, 0) }, Some(1)))
}

/// Safety: native context owns its NUL-terminated string and CRT `strtof`
/// result. Each byte is read synchronously before the native string changes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_energy_parse_float(
    step: Option<Step>,
    context: *mut c_void,
) -> u8 {
    let Some(step) = step else {
        return 0;
    };
    if !matches!(unsafe { call(step, context, 0, 0) }, Some(1)) {
        return 0;
    }
    let Some(mut offset) = (unsafe { call(step, context, 1, 0) }) else {
        return 0;
    };
    let Some(bits) = (unsafe { call(step, context, 2, 0) }) else {
        return 0;
    };
    let parsed = f32::from_bits(bits as u32);
    if offset == 0 || !parsed.is_finite() {
        return 0;
    }
    loop {
        let Some(byte) = (unsafe { call(step, context, 3, offset) }) else {
            return 0;
        };
        if byte != b' ' as usize
            && byte != b'\t' as usize
            && byte != b'\r' as usize
            && byte != b'\n' as usize
        {
            break;
        }
        offset += 1;
    }
    let Some(byte) = (unsafe { call(step, context, 3, offset) }) else {
        return 0;
    };
    if byte == b'f' as usize || byte == b'F' as usize {
        offset += 1;
    }
    loop {
        let Some(byte) = (unsafe { call(step, context, 3, offset) }) else {
            return 0;
        };
        if byte != b' ' as usize
            && byte != b'\t' as usize
            && byte != b'\r' as usize
            && byte != b'\n' as usize
        {
            break;
        }
        offset += 1;
    }
    if !matches!(unsafe { call(step, context, 3, offset) }, Some(0)) || parsed < 0.0 {
        return 0;
    }
    u8::from(matches!(unsafe { call(step, context, 4, 0) }, Some(1)))
}
