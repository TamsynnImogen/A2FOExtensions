use std::ffi::c_void;
use a1_race_workflow::{a2fo_rs_stage9_a1_race_loaded,
                       a2fo_rs_stage9_a1_race_record,
                       a2fo_rs_stage9_a1_race_present};

const ERROR: u64 = u64::MAX;

struct Loaded {
    declared: i32,
    race: i32,
    pending: bool,
    apply_ok: bool,
    synthetic: i32,
    resources_ok: bool,
    records: usize,
    trace: Vec<u32>,
    error_at: Option<u32>,
}

unsafe extern "C" fn loaded_step(opaque: *mut c_void, op: u32) -> u64 {
    let state = unsafe { &mut *opaque.cast::<Loaded>() };
    state.trace.push(op);
    if state.error_at == Some(op) { return ERROR; }
    match op {
        0 => state.declared as u32 as u64,
        1 => 0,
        2 => state.race as u32 as u64,
        3 => { let prior = state.pending; state.pending = false; prior as u64 },
        4 => state.apply_ok as u64,
        5 | 6 | 10 | 14 => 0,
        7 => { state.pending = true; 0 },
        8 => state.synthetic as u32 as u64,
        9 => state.resources_ok as u64,
        11 => (state.records == 0) as u64,
        12 => { state.records = 0; 0 },
        13 => state.records as u64,
        15 => 0,
        16 => { state.records += 1; 0 },
        _ => panic!("unexpected loaded op {op}"),
    }
}

fn loaded() -> Loaded {
    Loaded { declared: 2, race: 0, pending: true, apply_ok: false,
             synthetic: -1, resources_ok: false, records: 0,
             trace: Vec::new(), error_at: None }
}

fn run_loaded(state: &mut Loaded) {
    unsafe { a2fo_rs_stage9_a1_race_loaded(
        (state as *mut Loaded).cast(), loaded_step) }
}

#[test]
fn loaded_failure_restores_pending_then_reads_live_registry() {
    let mut state = loaded();
    run_loaded(&mut state);
    assert!(state.pending);
    assert_eq!(state.records, 1);
    assert_eq!(state.trace,
        [0, 2, 3, 4, 6, 7, 8, 9, 10, 11, 13, 11, 15, 16]);
    state.trace.clear();
    state.race = 0;
    state.synthetic = 0;
    run_loaded(&mut state);
    assert_eq!(state.records, 1);
    assert_eq!(state.trace, [0, 2, 3, 4, 6, 7, 8, 11, 12, 13, 11, 15, 16]);
}

#[test]
fn loaded_admission_nonsequential_and_error_stop() {
    let mut state = loaded();
    state.declared = 0;
    run_loaded(&mut state);
    assert_eq!(state.trace, [0, 1]);
    state.trace.clear(); state.declared = 2; state.race = 2;
    run_loaded(&mut state);
    assert_eq!(state.trace, [0, 2]);
    state.trace.clear(); state.race = 1; state.pending = false;
    run_loaded(&mut state);
    assert_eq!(state.trace, [0, 2, 3, 8, 9, 10, 11, 13, 14]);
    state.trace.clear(); state.race = 0; state.pending = true;
    state.error_at = Some(6);
    run_loaded(&mut state);
    assert_eq!(state.trace, [0, 2, 3, 4, 6]);
    assert!(!state.pending); // Native failure log threw before restore op 7.
}

struct Record { trace: Vec<u32>, slot: i32, observed_slot: i32,
                error_at: Option<u32> }
unsafe extern "C" fn record_step(opaque: *mut c_void, op: u32) -> u64 {
    let state = unsafe { &mut *opaque.cast::<Record>() };
    state.trace.push(op);
    if state.error_at == Some(op) { return ERROR; }
    if op == 1 { state.slot = 17; }
    if op == 2 { state.observed_slot = state.slot; }
    0
}

#[test]
fn record_fields_preserve_order_and_stop_on_captured_error() {
    let mut state = Record { trace: Vec::new(), slot: 3,
                             observed_slot: 0, error_at: None };
    unsafe { a2fo_rs_stage9_a1_race_record(
        (&mut state as *mut Record).cast(), record_step) };
    assert_eq!(state.trace, [0, 1, 2, 3, 4, 5]);
    assert_eq!(state.observed_slot, 17);
    state.trace.clear(); state.error_at = Some(3);
    unsafe { a2fo_rs_stage9_a1_race_record(
        (&mut state as *mut Record).cast(), record_step) };
    assert_eq!(state.trace, [0, 1, 2, 3]);
}

struct Presentation { trace: Vec<u32>, fallback: bool, logged: bool,
                      records: usize, declared: usize, applied: bool,
                      error_at: Option<u32> }
unsafe extern "C" fn presentation_step(opaque: *mut c_void, op: u32) -> u64 {
    let state = unsafe { &mut *opaque.cast::<Presentation>() };
    state.trace.push(op);
    if state.error_at == Some(op) { return ERROR; }
    match op {
        0 => state.fallback as u64,
        1 => state.logged as u64,
        2 => { state.logged = true; 0 },
        3 => { state.applied = true; state.records = state.declared; 0 },
        4 => (state.records == state.declared) as u64,
        5 => 0,
        _ => panic!("unexpected presentation op {op}"),
    }
}

fn run_present(state: &mut Presentation) {
    unsafe { a2fo_rs_stage9_a1_race_present(
        (state as *mut Presentation).cast(), presentation_step) }
}

#[test]
fn presentation_rereads_size_after_apply_and_stops_on_error() {
    let mut state = Presentation { trace: Vec::new(), fallback: true,
        logged: false, records: 1, declared: 2, applied: false,
        error_at: None };
    run_present(&mut state);
    assert!(state.applied && state.logged && state.records == 2);
    assert_eq!(state.trace, [0, 1, 2, 3, 4, 5]);
    state.trace.clear(); state.error_at = Some(2); state.logged = false;
    run_present(&mut state);
    assert_eq!(state.trace, [0, 1, 2]);
    state.trace.clear(); state.error_at = None;
    state.fallback = false; state.records = 1;
    run_present(&mut state);
    assert_eq!(state.trace, [0, 4]);
}
