use super::*;
use std::sync::Arc;

#[repr(C)]
struct FakeObject {
    vtable: *const usize,
    state: *const Mutex<FakeState>,
}

#[derive(Default)]
struct FakeState {
    buffer: usize,
    bytes: Vec<u8>,
    play_cursor: u32,
    safe_cursor: u32,
    running: bool,
    add_refs: usize,
    device_releases: usize,
    buffer_releases: usize,
    stop_calls: usize,
    advance_on_stop: u32,
    writes: Vec<(u32, u32, bool)>,
}

unsafe fn state<'a>(object: *mut c_void) -> &'a Mutex<FakeState> {
    let object = unsafe { &*object.cast::<FakeObject>() };
    unsafe { &*object.state }
}

unsafe extern "system" fn add_ref(object: *mut c_void) -> u32 {
    unsafe { state(object) }.lock().unwrap().add_refs += 1;
    2
}
unsafe extern "system" fn release_device(object: *mut c_void) -> u32 {
    unsafe { state(object) }.lock().unwrap().device_releases += 1;
    1
}
unsafe extern "system" fn release_buffer(object: *mut c_void) -> u32 {
    unsafe { state(object) }.lock().unwrap().buffer_releases += 1;
    0
}
unsafe extern "system" fn create_buffer(
    object: *mut c_void,
    desc: *const BufferDesc,
    output: *mut *mut c_void,
    _outer: *mut c_void,
) -> i32 {
    let mut state = unsafe { state(object) }.lock().unwrap();
    let desc = unsafe { &*desc };
    assert_eq!(desc.size as usize, size_of::<BufferDesc>());
    state.bytes = vec![0; desc.bytes as usize];
    unsafe { *output = state.buffer as *mut c_void };
    0
}
unsafe extern "system" fn get_position(object: *mut c_void, play: *mut u32, safe: *mut u32) -> i32 {
    let state = unsafe { state(object) }.lock().unwrap();
    unsafe {
        *play = state.play_cursor;
        *safe = state.safe_cursor;
    }
    0
}
unsafe extern "system" fn lock_buffer(
    object: *mut c_void,
    offset: u32,
    length: u32,
    first: *mut *mut c_void,
    first_len: *mut u32,
    second: *mut *mut c_void,
    second_len: *mut u32,
    _flags: u32,
) -> i32 {
    let mut state = unsafe { state(object) }.lock().unwrap();
    let capacity = state.bytes.len() as u32;
    assert!(offset < capacity && length <= capacity);
    let running = state.running;
    state.writes.push((offset, length, running));
    let n1 = length.min(capacity - offset);
    unsafe {
        *first = state.bytes.as_mut_ptr().add(offset as usize).cast();
        *first_len = n1;
        *second = if n1 < length {
            state.bytes.as_mut_ptr().cast()
        } else {
            ptr::null_mut()
        };
        *second_len = length - n1;
    }
    0
}
unsafe extern "system" fn unlock_buffer(
    _object: *mut c_void,
    _first: *mut c_void,
    _first_len: u32,
    _second: *mut c_void,
    _second_len: u32,
) -> i32 {
    0
}
unsafe extern "system" fn play(object: *mut c_void, _: u32, _: u32, _: u32) -> i32 {
    unsafe { state(object) }.lock().unwrap().running = true;
    0
}
unsafe extern "system" fn set_position(object: *mut c_void, cursor: u32) -> i32 {
    unsafe { state(object) }.lock().unwrap().play_cursor = cursor;
    0
}
unsafe extern "system" fn set_volume(_object: *mut c_void, _db: i32) -> i32 {
    0
}
unsafe extern "system" fn stop(object: *mut c_void) -> i32 {
    let mut state = unsafe { state(object) }.lock().unwrap();
    if state.running && state.advance_on_stop != 0 {
        state.play_cursor = (state.play_cursor + state.advance_on_stop) % state.bytes.len() as u32;
        state.advance_on_stop = 0;
    }
    state.running = false;
    state.stop_calls += 1;
    0
}

struct FakeRig {
    state: Arc<Mutex<FakeState>>,
    device: Box<FakeObject>,
    _buffer: Box<FakeObject>,
    _device_vtable: Box<[usize]>,
    _buffer_vtable: Box<[usize]>,
}
impl FakeRig {
    fn new() -> Self {
        let state = Arc::new(Mutex::new(FakeState::default()));
        let mut device_vtable = vec![0usize; 7].into_boxed_slice();
        device_vtable[1] = add_ref as *const () as usize;
        device_vtable[2] = release_device as *const () as usize;
        device_vtable[3] = create_buffer as *const () as usize;
        let mut buffer_vtable = vec![0usize; 20].into_boxed_slice();
        buffer_vtable[2] = release_buffer as *const () as usize;
        buffer_vtable[4] = get_position as *const () as usize;
        buffer_vtable[11] = lock_buffer as *const () as usize;
        buffer_vtable[12] = play as *const () as usize;
        buffer_vtable[13] = set_position as *const () as usize;
        buffer_vtable[15] = set_volume as *const () as usize;
        buffer_vtable[18] = stop as *const () as usize;
        buffer_vtable[19] = unlock_buffer as *const () as usize;
        let device = Box::new(FakeObject {
            vtable: device_vtable.as_ptr(),
            state: Arc::as_ptr(&state),
        });
        let buffer = Box::new(FakeObject {
            vtable: buffer_vtable.as_ptr(),
            state: Arc::as_ptr(&state),
        });
        state.lock().unwrap().buffer = (&*buffer as *const FakeObject) as usize;
        Self {
            state,
            device,
            _buffer: buffer,
            _device_vtable: device_vtable,
            _buffer_vtable: buffer_vtable,
        }
    }
    fn pointer(&self) -> usize {
        (&*self.device as *const FakeObject) as usize
    }
    fn cursor(&self, play: u32, safe: u32) {
        let mut state = self.state.lock().unwrap();
        state.play_cursor = play;
        state.safe_cursor = safe;
    }
}

#[test]
fn drain_overshoot_rebases_next_submit_at_zero_without_losing_samples() {
    let rig = FakeRig::new();
    let device = unsafe { Device::acquire(rig.pointer()) }.unwrap();
    let mut buffer = Buffer::new(device, 8_000, 1, 16).unwrap();
    let pending = AtomicUsize::new(8);
    let position = AtomicU64::new(0);
    let mut queue = VecDeque::from(vec![1, 2, 3, 4, 5, 6, 7, 8]);
    buffer.service(&mut queue, &pending, &position).unwrap();
    assert_eq!(buffer.written, 8);
    assert!(queue.is_empty());
    rig.cursor(12, 14);
    buffer.service(&mut queue, &pending, &position).unwrap();
    assert_eq!(position.load(Ordering::Acquire), 4);
    assert_eq!(buffer.origin, 8);
    assert_eq!(buffer.written, 8);
    queue.extend([9, 10, 11, 12]);
    pending.fetch_add(4, Ordering::AcqRel);
    buffer.service(&mut queue, &pending, &position).unwrap();
    assert_eq!(&rig.state.lock().unwrap().bytes[..4], &[9, 10, 11, 12]);
    assert_eq!(buffer.written, 12);
    assert_eq!(pending.load(Ordering::Acquire), 0);
    rig.cursor(4, 6);
    buffer.service(&mut queue, &pending, &position).unwrap();
    assert_eq!(position.load(Ordering::Acquire), 6);
}

#[test]
fn near_safe_cursor_requeues_unplayed_tail_before_any_active_write() {
    let rig = FakeRig::new();
    let device = unsafe { Device::acquire(rig.pointer()) }.unwrap();
    let mut buffer = Buffer::new(device, 8_000, 1, 16).unwrap();
    let pending = AtomicUsize::new(16);
    let position = AtomicU64::new(0);
    let mut queue = VecDeque::from((1u8..=16).collect::<Vec<_>>());
    buffer.service(&mut queue, &pending, &position).unwrap();
    rig.cursor(4, 16);
    rig.state.lock().unwrap().advance_on_stop = 4;
    queue.extend([17, 18, 19, 20]);
    pending.fetch_add(4, Ordering::AcqRel);
    buffer.service(&mut queue, &pending, &position).unwrap();
    let (stops, bytes, active_writes) = {
        let state = rig.state.lock().unwrap();
        (
            state.stop_calls,
            state.bytes[..12].to_vec(),
            state.writes.iter().any(|&(_, _, running)| running),
        )
    };
    assert!(stops >= 1);
    assert_eq!(&bytes[..], &[9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]);
    assert!(!active_writes);
    assert_eq!(buffer.played, 8);
    assert_eq!(buffer.written, 20);
    assert_eq!(position.load(Ordering::Acquire), 4);
    assert_eq!(pending.load(Ordering::Acquire), 0);
}

#[test]
fn fractional_cursor_steps_accumulate_to_complete_sample_frames() {
    let rig = FakeRig::new();
    let device = unsafe { Device::acquire(rig.pointer()) }.unwrap();
    let mut buffer = Buffer::new(device, 8_000, 1, 16).unwrap();
    let pending = AtomicUsize::new(8);
    let position = AtomicU64::new(0);
    let mut queue = VecDeque::from(vec![1, 2, 3, 4, 5, 6, 7, 8]);
    buffer.service(&mut queue, &pending, &position).unwrap();
    for (cursor, expected_frames) in [(1, 0), (2, 1), (3, 1), (4, 2)] {
        rig.cursor(cursor, cursor + 1);
        buffer.service(&mut queue, &pending, &position).unwrap();
        assert_eq!(position.load(Ordering::Acquire), expected_frames);
    }
    assert_eq!(buffer.cursor_bytes, 4);
    assert_eq!(buffer.played, 4);
}

#[test]
fn dropping_worker_joins_and_releases_borrowed_device_and_buffer() {
    let rig = FakeRig::new();
    let sink = unsafe { DirectSoundSink::open(rig.pointer(), 8_000, 1, 16) }.unwrap();
    drop(sink);
    let state = rig.state.lock().unwrap();
    assert_eq!(state.add_refs, 1);
    assert_eq!(state.device_releases, 1);
    assert_eq!(state.buffer_releases, 1);
    assert!(state.stop_calls >= 1);
}
