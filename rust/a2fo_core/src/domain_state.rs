//! Catalog and player state policies. STL storage remains in the C++ adapter.
extern crate alloc;
use alloc::collections::BTreeSet;
use core::slice;
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Bytes {
    data: *const u8,
    size: usize,
}
impl Bytes {
    fn empty(self) -> bool {
        self.size == 0
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Rank {
    id: Bytes,
    level: u32,
    next_xp: f64,
    next_class: Bytes,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Resolution {
    status: i32,
    class_id: Bytes,
    level: u32,
    has_next: u8,
    next_xp: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Player {
    enabled: u8,
    level: u32,
    pool: f64,
    multiplier: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Unit {
    id: u64,
    player: u32,
    base_class: Bytes,
    current_class: Bytes,
    native_capable: u8,
    completed: u8,
    live: u8,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct PlayerRecord {
    id: u32,
    state: Player,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Progress {
    status: i32,
    consumed: u8,
    levels_gained: u32,
    state: Player,
    threshold: f64,
    commit: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CatalogOps {
    context: *mut core::ffi::c_void,
    rank_lookup: extern "C" fn(*mut core::ffi::c_void, Bytes, *mut Rank) -> u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct XpOps {
    context: *mut core::ffi::c_void,
    get_player: extern "C" fn(*mut core::ffi::c_void, u32, *mut Player) -> u8,
    get_unit: extern "C" fn(*mut core::ffi::c_void, u64, *mut Unit) -> u8,
    next_unit: extern "C" fn(*mut core::ffi::c_void, *mut u64, *mut Unit) -> u8,
    erase_unit: extern "C" fn(*mut core::ffi::c_void, u64),
    erase_player: extern "C" fn(*mut core::ffi::c_void, u32),
    erase_cache: extern "C" fn(*mut core::ffi::c_void, u32),
    cache_get: extern "C" fn(*mut core::ffi::c_void, u32, u32, *mut f64) -> u8,
    cache_set: extern "C" fn(*mut core::ffi::c_void, u32, u32, f64),
    resolve: extern "C" fn(*mut core::ffi::c_void, Bytes, u32, Bytes) -> Resolution,
    native_level: extern "C" fn(*mut core::ffi::c_void, Bytes) -> u32,
    weighted_reset: extern "C" fn(*mut core::ffi::c_void),
    weighted_add: extern "C" fn(*mut core::ffi::c_void, f64, f64) -> u8,
    weighted_result: extern "C" fn(*mut core::ffi::c_void, *mut f64) -> u8,
}
const OK: i32 = 0;
const MISSING: i32 = 1;
const MALFORMED: i32 = 2;
const CYCLE: i32 = 3;
const NOT_FAMILY: i32 = 4;
const INVALID: i32 = 5;
const OVERFLOW: i32 = 6;
const UNKNOWN_UNIT: i32 = 7;
const UNKNOWN_PLAYER: i32 = 8;
fn failed(status: i32) -> Resolution {
    Resolution {
        status,
        ..Resolution::default()
    }
}
fn same(a: Bytes, b: Bytes) -> bool {
    if a.size != b.size {
        return false;
    }
    if a.size == 0 {
        return true;
    }
    // Both spans are borrowed from live C++ strings for this synchronous call.
    unsafe { slice::from_raw_parts(a.data, a.size) == slice::from_raw_parts(b.data, b.size) }
}
fn rank_valid(r: Rank) -> bool {
    !r.id.empty()
        && r.level >= 1
        && r.level <= 64
        && (if r.next_class.empty() {
            r.next_xp == 0.
        } else {
            r.next_xp.is_finite() && r.next_xp > 0.
        })
        && !same(r.next_class, r.id)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_rank_valid(r: Rank) -> u8 {
    rank_valid(r) as u8
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_resolve(
    ops: CatalogOps,
    base: Bytes,
    level: u32,
    current: Bytes,
) -> Resolution {
    if base.empty() || !(1..=64).contains(&level) {
        return failed(INVALID);
    }
    let mut visited: [Bytes; 64] = [Bytes::default(); 64];
    let mut id = base;
    let mut target: Option<Rank> = None;
    let mut found = current.empty();
    for depth in 0..64 {
        let mut rank = Rank {
            id: Bytes::default(),
            level: 0,
            next_xp: 0.,
            next_class: Bytes::default(),
        };
        if (ops.rank_lookup)(ops.context, id, &mut rank) == 0 {
            return failed(MISSING);
        }
        if visited[..depth].iter().any(|v| same(*v, id)) {
            return failed(CYCLE);
        }
        visited[depth] = id;
        if rank.level != depth as u32 + 1
            || (if rank.next_class.empty() {
                rank.next_xp != 0.
            } else {
                !rank.next_xp.is_finite() || rank.next_xp <= 0.
            })
        {
            return failed(MALFORMED);
        }
        if same(id, current) {
            found = true;
        }
        if rank.level <= level {
            target = Some(rank);
        }
        if rank.next_class.empty() {
            break;
        }
        if depth == 63 {
            return failed(MALFORMED);
        }
        id = rank.next_class;
    }
    if !found {
        return failed(NOT_FAMILY);
    }
    let Some(rank) = target else {
        return failed(MALFORMED);
    };
    Resolution {
        status: OK,
        class_id: rank.id,
        level: rank.level,
        has_next: (!rank.next_class.empty()) as u8,
        next_xp: rank.next_xp,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_configure_valid(multiplier: f64) -> u8 {
    (multiplier.is_finite() && multiplier > 0.) as u8
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_reset(ops: XpOps, player: u32) {
    (ops.erase_player)(ops.context, player);
    (ops.erase_cache)(ops.context, player);
    let mut cursor = 0;
    let mut unit = Unit::default();
    while (ops.next_unit)(ops.context, &mut cursor, &mut unit) != 0 {
        if unit.player == player {
            (ops.erase_unit)(ops.context, unit.id);
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_remove_allowed(ops: XpOps, id: u64) -> u8 {
    let mut unit = Unit::default();
    (ops.get_unit)(ops.context, id, &mut unit)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_transfer_allowed(ops: XpOps, id: u64) -> i32 {
    if a2fo_rs_xp_remove_allowed(ops, id) == 0 {
        UNKNOWN_UNIT
    } else {
        OK
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_upsert_valid(ops: XpOps, u: Unit) -> i32 {
    if u.id == 0 || u.base_class.empty() || u.current_class.empty() {
        return INVALID;
    }
    let r = (ops.resolve)(ops.context, u.base_class, 64, u.current_class);
    r.status
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_target(ops: XpOps, id: u64) -> Resolution {
    let mut u = Unit::default();
    if (ops.get_unit)(ops.context, id, &mut u) == 0 {
        return failed(UNKNOWN_UNIT);
    }
    let mut p = Player::default();
    if (ops.get_player)(ops.context, u.player, &mut p) == 0 {
        return failed(UNKNOWN_PLAYER);
    }
    let native = (ops.resolve)(ops.context, u.base_class, 64, u.current_class);
    if native.status != OK {
        return native;
    }
    let level = (ops.native_level)(ops.context, u.current_class);
    (ops.resolve)(
        ops.context,
        u.base_class,
        p.level.max(level),
        u.current_class,
    )
}
fn threshold_for(ops: XpOps, player: u32, level: u32) -> (f64, i32) {
    let mut owner = Player::default();
    if (ops.get_player)(ops.context, player, &mut owner) == 0 {
        return (0., UNKNOWN_PLAYER);
    }
    (ops.weighted_reset)(ops.context);
    let mut cursor = 0;
    let mut unit = Unit::default();
    while (ops.next_unit)(ops.context, &mut cursor, &mut unit) != 0 {
        if unit.player != player
            || unit.native_capable == 0
            || unit.completed == 0
            || unit.live == 0
        {
            continue;
        }
        let native = (ops.resolve)(ops.context, unit.base_class, 64, unit.current_class);
        if native.status != OK {
            return (0., native.status);
        }
        let current = (ops.native_level)(ops.context, unit.current_class);
        let effective = (ops.resolve)(
            ops.context,
            unit.base_class,
            level.max(current),
            unit.current_class,
        );
        if effective.status != OK {
            return (0., effective.status);
        }
        if effective.has_next == 0 {
            continue;
        }
        if (ops.weighted_add)(ops.context, effective.next_xp, owner.multiplier) == 0 {
            return (0., OVERFLOW);
        }
    }
    let mut value = 0.;
    if (ops.weighted_result)(ops.context, &mut value) == 0 {
        return (0., OVERFLOW);
    }
    (value, OK)
}
fn cached_threshold(ops: XpOps, player: u32, level: u32) -> (f64, i32) {
    let mut value = 0.;
    if (ops.cache_get)(ops.context, player, level, &mut value) != 0 {
        return (value, OK);
    }
    let (value, status) = threshold_for(ops, player, level);
    if status == OK {
        (ops.cache_set)(ops.context, player, level, value);
    }
    (value, status)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_xp_threshold(
    ops: XpOps,
    player: u32,
    level: u32,
    status: *mut i32,
) -> f64 {
    let (value, s) = cached_threshold(ops, player, level);
    if !status.is_null() {
        unsafe { *status = s };
    }
    value
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_process(ops: XpOps, player: u32, xp: f64, award: u8) -> Progress {
    let mut result = Progress {
        state: Player {
            enabled: 0,
            level: 1,
            pool: 0.,
            multiplier: 1.,
        },
        ..Progress::default()
    };
    let mut state = Player::default();
    if (ops.get_player)(ops.context, player, &mut state) == 0 {
        result.status = UNKNOWN_PLAYER;
        return result;
    }
    result.state = state;
    if award != 0 && (!xp.is_finite() || xp < 0.) {
        result.status = INVALID;
        return result;
    }
    if state.enabled == 0 {
        return result;
    }
    let mut next = state;
    if award != 0 {
        if crate::wide_math::above_difference(xp, f64::MAX, next.pool) {
            result.status = OVERFLOW;
            return result;
        }
        next.pool = crate::wide_math::assignment(next.pool, xp, false);
    }
    while next.level < 64 {
        let (requirement, status) = cached_threshold(ops, player, next.level);
        if status != OK {
            result.status = status;
            return result;
        }
        if requirement == 0. || next.pool < requirement {
            break;
        }
        next.pool = crate::wide_math::assignment(next.pool, requirement, true);
        next.level += 1;
        result.levels_gained += 1;
    }
    let (threshold, status) = cached_threshold(ops, player, next.level);
    if status != OK {
        result.status = status;
        return result;
    }
    result.threshold = threshold;
    result.state = next;
    result.consumed = award;
    result.commit = 1;
    result
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_xp_state_valid(s: Player) -> u8 {
    (s.level >= 1
        && s.level <= 64
        && s.pool.is_finite()
        && s.pool >= 0.
        && s.multiplier.is_finite()
        && s.multiplier > 0.) as u8
}
fn put32(out: &mut [u8], at: &mut usize, v: u32) {
    out[*at..*at + 4].copy_from_slice(&v.to_le_bytes());
    *at += 4;
}
fn put64(out: &mut [u8], at: &mut usize, v: u64) {
    out[*at..*at + 8].copy_from_slice(&v.to_le_bytes());
    *at += 8;
}
fn get32(input: &[u8], at: &mut usize) -> Option<u32> {
    let bytes: [u8; 4] = input.get(*at..*at + 4)?.try_into().ok()?;
    *at += 4;
    Some(u32::from_le_bytes(bytes))
}
fn get64(input: &[u8], at: &mut usize) -> Option<u64> {
    let bytes: [u8; 8] = input.get(*at..*at + 8)?.try_into().ok()?;
    *at += 8;
    Some(u64::from_le_bytes(bytes))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_xp_snapshot(
    records: *const PlayerRecord,
    count: usize,
    output: *mut u8,
    capacity: usize,
) -> usize {
    let Some(size) = count.checked_mul(25).and_then(|n| n.checked_add(8)) else {
        return 0;
    };
    if output.is_null() || capacity < size || count > u32::MAX as usize {
        return 0;
    }
    let records = if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(records, count) }
    };
    let out = unsafe { slice::from_raw_parts_mut(output, size) };
    out[..4].copy_from_slice(b"CXP1");
    let mut at = 4;
    put32(out, &mut at, count as u32);
    for record in records {
        put32(out, &mut at, record.id);
        out[at] = (record.state.enabled != 0) as u8;
        at += 1;
        put32(out, &mut at, record.state.level);
        put64(out, &mut at, record.state.pool.to_bits());
        put64(out, &mut at, record.state.multiplier.to_bits());
    }
    size
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_xp_restore(
    bytes: *const u8,
    size: usize,
    records: *mut PlayerRecord,
    capacity: usize,
    count: *mut usize,
) -> u8 {
    if bytes.is_null() || count.is_null() || size < 8 {
        return 0;
    }
    let input = unsafe { slice::from_raw_parts(bytes, size) };
    if &input[..4] != b"CXP1" {
        return 0;
    }
    let mut at = 4;
    let Some(n) = get32(input, &mut at) else {
        return 0;
    };
    let n = n as usize;
    if n > (size - at) / 25 || n > capacity || (n != 0 && records.is_null()) {
        return 0;
    }
    let out = if n == 0 {
        &mut []
    } else {
        unsafe { slice::from_raw_parts_mut(records, n) }
    };
    let mut seen = BTreeSet::new();
    for slot in out.iter_mut() {
        let Some(id) = get32(input, &mut at) else {
            return 0;
        };
        let Some(enabled) = input.get(at).copied() else {
            return 0;
        };
        if enabled > 1 {
            return 0;
        }
        at += 1;
        let Some(level) = get32(input, &mut at) else {
            return 0;
        };
        let Some(pool) = get64(input, &mut at) else {
            return 0;
        };
        let Some(multiplier) = get64(input, &mut at) else {
            return 0;
        };
        let state = Player {
            enabled,
            level,
            pool: f64::from_bits(pool),
            multiplier: f64::from_bits(multiplier),
        };
        if a2fo_rs_xp_state_valid(state) == 0 || !seen.insert(id) {
            return 0;
        }
        *slot = PlayerRecord { id, state };
    }
    if at != size {
        return 0;
    }
    unsafe { *count = n };
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_xp_restore_one(
    player: u32,
    bytes: *const u8,
    size: usize,
    record: *mut PlayerRecord,
) -> u8 {
    if record.is_null() {
        return 0;
    }
    let mut count = 0;
    if unsafe { a2fo_rs_xp_restore(bytes, size, record, 1, &mut count) } == 0 || count != 1 {
        return 0;
    }
    (unsafe { *record }.id == player) as u8
}

// Squadron policies. Every callback exposes one elementary view or stages one
// presentation result; Rust never retains borrowed bytes beyond the call.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SqSlot {
    row: u32,
    ordinal: u32,
    odf: Bytes,
    member: u64,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SqRow {
    row: u32,
    count: u32,
    odf: Bytes,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SqCraft {
    id: u64,
    team: u32,
    odf_key: Bytes,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SqRepair {
    yard: u64,
    team: u32,
    compatible: u8,
    accepted: u8,
    serviced: *const u64,
    serviced_count: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SqReplacement {
    ticket: u64,
    squad: u64,
    yard: u64,
    team: u32,
    slot: u32,
    odf: Bytes,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SqView {
    id: u64,
    team: u32,
    slot_count: usize,
    reinforce: u8,
    has_replacement: u8,
    replacement: SqReplacement,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SqTile {
    squad: u64,
    representative: u64,
    live: u32,
    maximum: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SqPlan {
    code: i32,
    id: u64,
    slot: u32,
    retire: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SqOps {
    context: *mut core::ffi::c_void,
    get_squad: extern "C" fn(*mut core::ffi::c_void, u64, *mut SqView) -> u8,
    next_squad: extern "C" fn(*mut core::ffi::c_void, *mut u64, *mut SqView) -> u8,
    get_slot: extern "C" fn(*mut core::ffi::c_void, u64, usize, *mut SqSlot) -> u8,
    member_squad: extern "C" fn(*mut core::ffi::c_void, u64) -> u64,
    launching: extern "C" fn(*mut core::ffi::c_void, u64) -> u8,
    emit_tile: extern "C" fn(*mut core::ffi::c_void, SqTile),
    emit_member: extern "C" fn(*mut core::ffi::c_void, u64),
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SqSavedOps {
    context: *mut core::ffi::c_void,
    get_squad: extern "C" fn(*mut core::ffi::c_void, usize, *mut SqView) -> u8,
    get_row: extern "C" fn(*mut core::ffi::c_void, usize, usize, *mut SqRow) -> u8,
    get_slot: extern "C" fn(*mut core::ffi::c_void, usize, usize, *mut SqSlot) -> u8,
    validate_definition: extern "C" fn(*mut core::ffi::c_void, usize) -> u8,
}
const SQ_OK: i32 = 0;
const SQ_NO: i32 = 1;
const SQ_INCOMPLETE: i32 = 2;
const SQ_ID_EXHAUSTED: i32 = 3;
const SQ_GLOBAL_LIMIT: i32 = 4;
const SQ_TEAM_LIMIT: i32 = 5;
const SQ_MEMBER_MISMATCH: i32 = 6;
const SQ_MEMBER_DUPLICATE: i32 = 7;
const SQ_FIRST_MISMATCH: i32 = 8;
const SQ_MISSING: i32 = 9;
const SQ_LAUNCHING: i32 = 10;
const SQ_ALREADY_RESERVED: i32 = 11;
const SQ_REPAIR_NOT_READY: i32 = 12;
const SQ_YARD_MEMBER: i32 = 13;
const SQ_TICKET_EXHAUSTED: i32 = 14;
const SQ_FULL: i32 = 15;
const SQ_STALE_OWNER: i32 = 16;
const SQ_CAPACITY: i32 = 17;
const SQ_SAVED_HEADER: i32 = 18;
const SQ_SAVED_ID: i32 = 19;
const SQ_SAVED_TEAM: i32 = 20;
const SQ_SAVED_DEF: i32 = 21;
const SQ_SAVED_SLOTS: i32 = 22;
const SQ_SAVED_SLOT_MISMATCH: i32 = 23;
const SQ_SAVED_MEMBER_DUP: i32 = 24;
const SQ_SAVED_REPLACEMENT: i32 = 25;
const SQ_SAVED_YARD: i32 = 26;
fn plan(code: i32) -> SqPlan {
    SqPlan {
        code,
        ..SqPlan::default()
    }
}
fn slots<'a>(ptr: *const SqSlot, count: usize) -> &'a [SqSlot] {
    if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(ptr, count) }
    }
}
fn rows<'a>(ptr: *const SqRow, count: usize) -> &'a [SqRow] {
    if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(ptr, count) }
    }
}
fn crafts<'a>(ptr: *const SqCraft, count: usize) -> &'a [SqCraft] {
    if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(ptr, count) }
    }
}
pub type SqSlotAt = extern "C" fn(*mut core::ffi::c_void, usize, *mut SqSlot) -> u8;
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_live_at(
    context: *mut core::ffi::c_void,
    slot_at: SqSlotAt,
    count: usize,
) -> u32 {
    let mut live = 0;
    for i in 0..count {
        let mut slot = SqSlot::default();
        if slot_at(context, i, &mut slot) != 0 && slot.member != 0 {
            live += 1;
        }
    }
    live
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_representative_at(
    context: *mut core::ffi::c_void,
    slot_at: SqSlotAt,
    count: usize,
) -> u64 {
    for i in 0..count {
        let mut slot = SqSlot::default();
        if slot_at(context, i, &mut slot) != 0 && slot.member != 0 {
            return slot.member;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_live(ptr: *const SqSlot, count: usize) -> u32 {
    slots(ptr, count).iter().filter(|s| s.member != 0).count() as u32
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_representative(ptr: *const SqSlot, count: usize) -> u64 {
    slots(ptr, count)
        .iter()
        .find(|s| s.member != 0)
        .map_or(0, |s| s.member)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_sq_badge(
    live: u32,
    maximum: usize,
    output: *mut u8,
    capacity: usize,
) -> usize {
    let text = alloc::format!("{live}/{maximum}");
    if output.is_null() || capacity < text.len() {
        return 0;
    }
    unsafe { core::ptr::copy_nonoverlapping(text.as_ptr(), output, text.len()) };
    text.len()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_sq_expand(
    ptr: *const SqRow,
    count: usize,
    output: *mut SqSlot,
    capacity: usize,
) -> usize {
    let rows = rows(ptr, count);
    let Some(total) = rows
        .iter()
        .try_fold(0usize, |n, r| n.checked_add(r.count as usize))
    else {
        return 0;
    };
    if total > capacity || (total != 0 && output.is_null()) {
        return 0;
    }
    let out = if total == 0 {
        &mut []
    } else {
        unsafe { slice::from_raw_parts_mut(output, total) }
    };
    let mut i = 0;
    for row in rows {
        for ordinal in 0..row.count {
            out[i] = SqSlot {
                row: row.row,
                ordinal,
                odf: row.odf,
                member: 0,
            };
            i += 1;
        }
    }
    total
}
fn next_squad(ops: SqOps, cursor: &mut u64) -> Option<SqView> {
    let mut view = SqView::default();
    if (ops.next_squad)(ops.context, cursor, &mut view) == 0 {
        None
    } else {
        Some(view)
    }
}
fn get_squad(ops: SqOps, id: u64) -> Option<SqView> {
    let mut view = SqView::default();
    if (ops.get_squad)(ops.context, id, &mut view) == 0 {
        None
    } else {
        Some(view)
    }
}
fn get_slot(ops: SqOps, id: u64, index: usize) -> SqSlot {
    let mut slot = SqSlot::default();
    (ops.get_slot)(ops.context, id, index, &mut slot);
    slot
}
fn live(ops: SqOps, view: SqView) -> u32 {
    (0..view.slot_count)
        .filter(|&i| get_slot(ops, view.id, i).member != 0)
        .count() as u32
}
fn reserved_yard(ops: SqOps, yard: u64) -> bool {
    let mut cursor = 0;
    while let Some(s) = next_squad(ops, &mut cursor) {
        if s.has_replacement != 0 && s.replacement.yard == yard {
            return true;
        }
    }
    false
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_sq_snapshot_ids(
    ops: SqOps,
    ids: *mut u64,
    capacity: usize,
) -> usize {
    let mut cursor = 0;
    let mut count = 0;
    while let Some(view) = next_squad(ops, &mut cursor) {
        if count >= capacity {
            return 0;
        }
        unsafe { *ids.add(count) = view.id };
        count += 1;
    }
    count
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_reserved_yard(ops: SqOps, yard: u64) -> u8 {
    reserved_yard(ops, yard) as u8
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_create(
    ops: SqOps,
    next_id: u64,
    total: usize,
    team: u32,
    slot_ptr: *const SqSlot,
    slot_count: usize,
    craft_ptr: *const SqCraft,
    craft_count: usize,
    launching: u8,
) -> SqPlan {
    let slots = slots(slot_ptr, slot_count);
    let crafts = crafts(craft_ptr, craft_count);
    if launching == 0 && craft_count != slot_count {
        return plan(SQ_INCOMPLETE);
    }
    if next_id == u64::MAX {
        return plan(SQ_ID_EXHAUSTED);
    }
    if total >= 4096 {
        return plan(SQ_GLOBAL_LIMIT);
    }
    let mut team_count = 0;
    let mut cursor = 0;
    while let Some(s) = next_squad(ops, &mut cursor) {
        if s.team == team {
            team_count += 1;
        }
    }
    if team_count >= 256 {
        return plan(SQ_TEAM_LIMIT);
    }
    if launching != 0 {
        let c = crafts[0];
        if c.id == 0
            || c.team != team
            || !same(c.odf_key, slots[0].odf)
            || (ops.member_squad)(ops.context, c.id) != 0
            || reserved_yard(ops, c.id)
        {
            return plan(SQ_FIRST_MISMATCH);
        }
        return SqPlan {
            code: SQ_OK,
            id: next_id,
            slot: 0,
            retire: 0,
        };
    }
    for (i, c) in crafts.iter().enumerate() {
        if c.id == 0 || reserved_yard(ops, c.id) || c.team != team || !same(c.odf_key, slots[i].odf)
        {
            return plan(SQ_MEMBER_MISMATCH);
        }
        if (ops.member_squad)(ops.context, c.id) != 0
            || crafts[..i].iter().any(|prior| prior.id == c.id)
        {
            return plan(SQ_MEMBER_DUPLICATE);
        }
    }
    SqPlan {
        code: SQ_OK,
        id: next_id,
        slot: 0,
        retire: 0,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_add_launching(
    ops: SqOps,
    id: u64,
    index: usize,
    craft: SqCraft,
) -> SqPlan {
    let Some(s) = get_squad(ops, id) else {
        return plan(SQ_NO);
    };
    if (ops.launching)(ops.context, id) == 0
        || index >= s.slot_count
        || craft.id == 0
        || craft.team != s.team
        || (ops.member_squad)(ops.context, craft.id) != 0
        || reserved_yard(ops, craft.id)
    {
        return plan(SQ_NO);
    }
    let slot = get_slot(ops, id, index);
    if slot.member != 0 || !same(craft.odf_key, slot.odf) {
        return plan(SQ_NO);
    }
    SqPlan {
        code: SQ_OK,
        id,
        slot: index as u32,
        retire: 0,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_finish_launching(ops: SqOps, id: u64) -> SqPlan {
    if (ops.launching)(ops.context, id) == 0 {
        return plan(SQ_NO);
    }
    let retire = get_squad(ops, id).is_some_and(|s| live(ops, s) == 0);
    SqPlan {
        code: SQ_OK,
        id,
        slot: 0,
        retire: retire as u8,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_remove(
    ops: SqOps,
    member: u64,
    new_owner: u32,
    ownership: u8,
) -> SqPlan {
    let id = (ops.member_squad)(ops.context, member);
    if id == 0 {
        return plan(SQ_NO);
    }
    let Some(s) = get_squad(ops, id) else {
        return plan(SQ_NO);
    };
    if ownership != 0 && s.team == new_owner {
        return plan(SQ_NO);
    }
    let remaining = (0..s.slot_count)
        .filter(|&i| {
            let m = get_slot(ops, id, i).member;
            m != 0 && m != member
        })
        .count();
    SqPlan {
        code: SQ_OK,
        id,
        slot: 0,
        retire: (remaining == 0 && (ops.launching)(ops.context, id) == 0) as u8,
    }
}
fn repair_ready(ops: SqOps, s: SqView, r: SqRepair) -> bool {
    if s.reinforce == 0 || r.yard == 0 || r.team != s.team || r.compatible == 0 || r.accepted == 0 {
        return false;
    }
    let expected: BTreeSet<u64> = (0..s.slot_count)
        .map(|i| get_slot(ops, s.id, i).member)
        .filter(|&id| id != 0)
        .collect();
    if expected.is_empty() || r.serviced_count != expected.len() {
        return false;
    }
    let serviced = if r.serviced_count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(r.serviced, r.serviced_count) }
    };
    let present: BTreeSet<u64> = serviced.iter().copied().collect();
    present.len() == serviced.len() && present == expected
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_reserve(ops: SqOps, id: u64, next_ticket: u64, r: SqRepair) -> SqPlan {
    let Some(s) = get_squad(ops, id) else {
        return plan(SQ_MISSING);
    };
    if (ops.launching)(ops.context, id) != 0 {
        return plan(SQ_LAUNCHING);
    }
    if s.has_replacement != 0 {
        return plan(SQ_ALREADY_RESERVED);
    }
    if !repair_ready(ops, s, r) {
        return plan(SQ_REPAIR_NOT_READY);
    }
    if (ops.member_squad)(ops.context, r.yard) != 0 {
        return plan(SQ_YARD_MEMBER);
    }
    if next_ticket == u64::MAX {
        return plan(SQ_TICKET_EXHAUSTED);
    }
    for i in 0..s.slot_count {
        if get_slot(ops, id, i).member == 0 {
            return SqPlan {
                code: SQ_OK,
                id,
                slot: i as u32,
                retire: 0,
            };
        }
    }
    plan(SQ_FULL)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_complete(
    ops: SqOps,
    ticket: u64,
    craft: SqCraft,
    r: SqRepair,
) -> SqPlan {
    if ticket == 0
        || craft.id == 0
        || (ops.member_squad)(ops.context, craft.id) != 0
        || reserved_yard(ops, craft.id)
    {
        return plan(SQ_NO);
    }
    let mut cursor = 0;
    while let Some(s) = next_squad(ops, &mut cursor) {
        if s.has_replacement == 0 || s.replacement.ticket != ticket {
            continue;
        }
        let request = s.replacement;
        if r.yard != request.yard
            || !repair_ready(ops, s, r)
            || craft.team != request.team
            || !same(craft.odf_key, request.odf)
            || craft.id == r.yard
            || get_slot(ops, s.id, request.slot as usize).member != 0
        {
            return plan(SQ_NO);
        }
        return SqPlan {
            code: SQ_OK,
            id: s.id,
            slot: request.slot,
            retire: 0,
        };
    }
    plan(SQ_NO)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_cancel(ops: SqOps, ticket: u64) -> SqPlan {
    let mut cursor = 0;
    while let Some(s) = next_squad(ops, &mut cursor) {
        if s.has_replacement != 0 && s.replacement.ticket == ticket {
            return SqPlan {
                code: SQ_OK,
                id: s.id,
                slot: 0,
                retire: 0,
            };
        }
    }
    plan(SQ_NO)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_sq_cancel_yard(
    ops: SqOps,
    yard: u64,
    tickets: *mut u64,
    capacity: usize,
) -> usize {
    let mut count = 0;
    let mut cursor = 0;
    while let Some(s) = next_squad(ops, &mut cursor) {
        if s.has_replacement != 0 && s.replacement.yard == yard {
            if count >= capacity {
                return 0;
            }
            unsafe { *tickets.add(count) = s.replacement.ticket };
            count += 1;
        }
    }
    count
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_select(
    ops: SqOps,
    selected: *const SqCraft,
    count: usize,
    team: u32,
    limit: usize,
) -> i32 {
    let mut groups = BTreeSet::new();
    let mut ordinary = BTreeSet::new();
    for craft in crafts(selected, count) {
        if craft.id == 0 || craft.team != team {
            continue;
        }
        let squad = (ops.member_squad)(ops.context, craft.id);
        if squad != 0 {
            let Some(s) = get_squad(ops, squad) else {
                return SQ_STALE_OWNER;
            };
            if s.team != team {
                return SQ_STALE_OWNER;
            }
            groups.insert(squad);
        } else {
            ordinary.insert(craft.id);
        }
    }
    let mut used = 0usize;
    for id in groups {
        let Some(s) = get_squad(ops, id) else {
            return SQ_STALE_OWNER;
        };
        let live = live(ops, s) as usize;
        if live > limit.saturating_sub(used) {
            return SQ_CAPACITY;
        }
        let representative = (0..s.slot_count)
            .map(|i| get_slot(ops, id, i).member)
            .find(|&m| m != 0)
            .unwrap_or(0);
        (ops.emit_tile)(
            ops.context,
            SqTile {
                squad: id,
                representative,
                live: live as u32,
                maximum: s.slot_count as u32,
            },
        );
        for i in 0..s.slot_count {
            let m = get_slot(ops, id, i).member;
            if m != 0 {
                (ops.emit_member)(ops.context, m);
            }
        }
        used += live;
    }
    if ordinary.len() > limit.saturating_sub(used) {
        return SQ_CAPACITY;
    }
    for id in ordinary {
        (ops.emit_tile)(
            ops.context,
            SqTile {
                squad: 0,
                representative: id,
                live: 1,
                maximum: 1,
            },
        );
        (ops.emit_member)(ops.context, id);
    }
    SQ_OK
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_sq_restore_check(
    ops: SqSavedOps,
    version: u32,
    next_squad: u64,
    next_ticket: u64,
    count: usize,
) -> i32 {
    if version != 1 || next_squad == 0 || next_ticket == 0 || count > 4096 {
        return SQ_SAVED_HEADER;
    }
    let mut ids = BTreeSet::new();
    let mut members = BTreeSet::new();
    let mut tickets = BTreeSet::new();
    let mut teams = alloc::collections::BTreeMap::<u32, u32>::new();
    let mut yards = alloc::vec::Vec::new();
    for index in 0..count {
        let mut s = SqView::default();
        if (ops.get_squad)(ops.context, index, &mut s) == 0 {
            return SQ_SAVED_ID;
        }
        if s.id == 0 || s.id >= next_squad || !ids.insert(s.id) {
            return SQ_SAVED_ID;
        }
        let entry = teams.entry(s.team).or_default();
        *entry += 1;
        if *entry > 256 {
            return SQ_SAVED_TEAM;
        }
        if (ops.validate_definition)(ops.context, index) == 0 {
            return SQ_SAVED_DEF;
        }
        let mut configured = alloc::vec::Vec::new();
        let mut row_index = 0;
        loop {
            let mut row = SqRow::default();
            if (ops.get_row)(ops.context, index, row_index, &mut row) == 0 {
                break;
            }
            for ordinal in 0..row.count {
                configured.push(SqSlot {
                    row: row.row,
                    ordinal,
                    odf: row.odf,
                    member: 0,
                });
            }
            row_index += 1;
        }
        let mut live = 0;
        for slot_index in 0..s.slot_count {
            let mut actual = SqSlot::default();
            if (ops.get_slot)(ops.context, index, slot_index, &mut actual) == 0 {
                return SQ_SAVED_SLOTS;
            }
            if actual.member != 0 {
                live += 1;
            }
        }
        if s.slot_count != configured.len() || live == 0 {
            return SQ_SAVED_SLOTS;
        }
        for (slot_index, expected) in configured.iter().enumerate() {
            let mut actual = SqSlot::default();
            if (ops.get_slot)(ops.context, index, slot_index, &mut actual) == 0 {
                return SQ_SAVED_SLOTS;
            }
            if actual.row != expected.row
                || actual.ordinal != expected.ordinal
                || !same(actual.odf, expected.odf)
            {
                return SQ_SAVED_SLOT_MISMATCH;
            }
            if actual.member != 0 && !members.insert(actual.member) {
                return SQ_SAVED_MEMBER_DUP;
            }
        }
        if s.has_replacement != 0 {
            let r = s.replacement;
            if s.reinforce == 0
                || r.ticket == 0
                || r.ticket >= next_ticket
                || !tickets.insert(r.ticket)
                || r.squad != s.id
                || r.team != s.team
                || r.yard == 0
                || r.slot as usize >= s.slot_count
            {
                return SQ_SAVED_REPLACEMENT;
            }
            let mut slot = SqSlot::default();
            (ops.get_slot)(ops.context, index, r.slot as usize, &mut slot);
            if slot.member != 0 || !same(r.odf, slot.odf) {
                return SQ_SAVED_REPLACEMENT;
            }
            yards.push(r.yard);
        }
    }
    for yard in yards {
        if members.contains(&yard) {
            return SQ_SAVED_YARD;
        }
    }
    SQ_OK
}
