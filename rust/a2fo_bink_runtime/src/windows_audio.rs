//! DirectSound output using the device supplied by FleetOpsHook/FM0D.
//! COM slots and DSBUFFERDESC match the installed MinGW dsound.h definitions.
//! A worker owns every buffer operation and joins before the sink is released.
use crate::state::AudioSink;
use core::{ffi::c_void, ptr};
use std::collections::VecDeque;
use std::sync::{
    Arc, Mutex,
    atomic::{AtomicU64, AtomicUsize, Ordering},
    mpsc,
};
use std::thread::{self, JoinHandle};
use std::time::Duration;
use windows_sys::Win32::Media::Audio::WAVEFORMATEX;
use windows_sys::Win32::UI::WindowsAndMessaging::GetDesktopWindow;

#[repr(C)]
struct BufferDesc {
    size: u32,
    flags: u32,
    bytes: u32,
    reserved: u32,
    format: *const WAVEFORMATEX,
    algorithm: [u8; 16],
}
#[link(name = "dsound")]
unsafe extern "system" {
    fn DirectSoundCreate8(guid: *const c_void, device: *mut *mut c_void, outer: *mut c_void)
    -> i32;
}

unsafe fn method<T: Copy>(object: *mut c_void, slot: usize) -> T {
    // The caller supplies a live IDirectSound/IDirectSoundBuffer, not an arbitrary pointer.
    assert_eq!(size_of::<T>(), size_of::<usize>());
    let table = unsafe { *object.cast::<*const usize>() };
    let address = unsafe { *table.add(slot) };
    unsafe { std::mem::transmute_copy(&address) }
}
unsafe fn release(object: *mut c_void) {
    if !object.is_null() {
        let call: unsafe extern "system" fn(*mut c_void) -> u32 = unsafe { method(object, 2) };
        unsafe {
            call(object);
        }
    }
}
fn check(hr: i32, operation: &str) -> Result<(), String> {
    if hr < 0 {
        Err(format!("{operation}: HRESULT 0x{:08x}", hr as u32))
    } else {
        Ok(())
    }
}

/// Owns one reference, including for a borrowed FMOD output device.
pub struct Device(*mut c_void);
// DirectSound is free-threaded; reference ownership is transferred to the output worker.
// Buffer calls are further serialized on that single worker.
unsafe impl Send for Device {}
impl Device {
    pub fn pointer(&self) -> usize {
        self.0 as usize
    }
    /// Caller guarantees a live IDirectSound-compatible COM interface or null.
    pub unsafe fn acquire(pointer: usize) -> Result<Self, String> {
        if pointer != 0 {
            let raw = pointer as *mut c_void;
            let add: unsafe extern "system" fn(*mut c_void) -> u32 = unsafe { method(raw, 1) };
            unsafe {
                add(raw);
            }
            Ok(Self(raw))
        } else {
            let mut raw = ptr::null_mut();
            check(
                unsafe { DirectSoundCreate8(ptr::null(), &mut raw, ptr::null_mut()) },
                "DirectSoundCreate8",
            )?;
            if raw.is_null() {
                return Err("DirectSoundCreate8 returned null".into());
            }
            let owned = Self(raw);
            let cooperative: unsafe extern "system" fn(*mut c_void, *mut c_void, u32) -> i32 =
                unsafe { method(raw, 6) };
            check(
                unsafe { cooperative(raw, GetDesktopWindow(), 1) },
                "SetCooperativeLevel",
            )?;
            Ok(owned)
        }
    }
}
impl Drop for Device {
    fn drop(&mut self) {
        unsafe { release(self.0) }
    }
}

struct Buffer {
    raw: *mut c_void,
    _device: Device,
    capacity: u32,
    align: u32,
    written: u64,
    played: u64,
    origin: u64,
    cursor_bytes: u64,
    last_cursor: u32,
    running: bool,
    shadow: Vec<u8>,
    silence: u8,
}
impl Buffer {
    fn new(device: Device, rate: u32, channels: u16, bits: u16) -> Result<Self, String> {
        if !(8000..=65535).contains(&rate)
            || !(1..=2).contains(&channels)
            || !matches!(bits, 8 | 16)
        {
            return Err("unsupported Bink output format".into());
        }
        let align = u32::from(channels) * u32::from(bits / 8);
        let capacity = rate
            .checked_mul(align)
            .and_then(|v| v.checked_mul(4))
            .ok_or("sound extent overflow")?
            .max(4096);
        let capacity = capacity - capacity % align;
        let format = WAVEFORMATEX {
            wFormatTag: 1,
            nChannels: channels,
            nSamplesPerSec: rate,
            nAvgBytesPerSec: rate * align,
            nBlockAlign: align as u16,
            wBitsPerSample: bits,
            cbSize: 0,
        };
        let desc = BufferDesc {
            size: size_of::<BufferDesc>() as u32,
            flags: 0x80 | 0x8000 | 0x10000,
            bytes: capacity,
            reserved: 0,
            format: &format,
            algorithm: [0; 16],
        };
        let create: unsafe extern "system" fn(
            *mut c_void,
            *const BufferDesc,
            *mut *mut c_void,
            *mut c_void,
        ) -> i32 = unsafe { method(device.0, 3) };
        let mut raw = ptr::null_mut();
        check(
            unsafe { create(device.0, &desc, &mut raw, ptr::null_mut()) },
            "CreateSoundBuffer",
        )?;
        if raw.is_null() {
            return Err("CreateSoundBuffer returned null".into());
        }
        let silence = if bits == 8 { 128 } else { 0 };
        let mut result = Self {
            raw,
            _device: device,
            capacity,
            align,
            written: 0,
            played: 0,
            origin: 0,
            cursor_bytes: 0,
            last_cursor: 0,
            running: false,
            shadow: vec![silence; capacity as usize],
            silence,
        };
        result.clear(bits)?;
        Ok(result)
    }
    fn write_region(&mut self, offset: u32, data: &[u8]) -> Result<(), String> {
        let lock: unsafe extern "system" fn(
            *mut c_void,
            u32,
            u32,
            *mut *mut c_void,
            *mut u32,
            *mut *mut c_void,
            *mut u32,
            u32,
        ) -> i32 = unsafe { method(self.raw, 11) };
        let unlock: unsafe extern "system" fn(
            *mut c_void,
            *mut c_void,
            u32,
            *mut c_void,
            u32,
        ) -> i32 = unsafe { method(self.raw, 19) };
        let (mut first, mut second) = (ptr::null_mut(), ptr::null_mut());
        let (mut n1, mut n2) = (0, 0);
        check(
            unsafe {
                lock(
                    self.raw,
                    offset,
                    data.len() as u32,
                    &mut first,
                    &mut n1,
                    &mut second,
                    &mut n2,
                    0,
                )
            },
            "DirectSound Lock",
        )?;
        let valid = (n1 as usize).checked_add(n2 as usize) == Some(data.len())
            && (n1 == 0 || !first.is_null())
            && (n2 == 0 || !second.is_null());
        if valid {
            unsafe {
                if n1 != 0 {
                    ptr::copy_nonoverlapping(data.as_ptr(), first.cast(), n1 as usize);
                }
                if n2 != 0 {
                    ptr::copy_nonoverlapping(
                        data.as_ptr().add(n1 as usize),
                        second.cast(),
                        n2 as usize,
                    );
                }
            }
        }
        let result = unsafe { unlock(self.raw, first, n1, second, n2) };
        if !valid {
            return Err("DirectSound returned invalid locked spans".into());
        }
        check(result, "DirectSound Unlock")?;
        for (i, byte) in data.iter().enumerate() {
            self.shadow[(offset as usize + i) % self.capacity as usize] = *byte;
        }
        Ok(())
    }
    fn clear(&mut self, bits: u16) -> Result<(), String> {
        self.write_region(
            0,
            &vec![if bits == 8 { 128 } else { 0 }; self.capacity as usize],
        )
    }
    fn reset(&mut self, bits: u16) -> Result<(), String> {
        let stop: unsafe extern "system" fn(*mut c_void) -> i32 = unsafe { method(self.raw, 18) };
        let position: unsafe extern "system" fn(*mut c_void, u32) -> i32 =
            unsafe { method(self.raw, 13) };
        check(unsafe { stop(self.raw) }, "DirectSound Stop")?;
        check(
            unsafe { position(self.raw, 0) },
            "DirectSound SetCurrentPosition",
        )?;
        self.running = false;
        self.written = 0;
        self.played = 0;
        self.origin = 0;
        self.cursor_bytes = 0;
        self.last_cursor = 0;
        self.clear(bits)
    }
    // Rebase the physical ring without discarding unplayed logical samples.
    // This also avoids writes between the driver's play and safe-write cursors.
    fn rebase(&mut self, queue: &mut VecDeque<u8>, pending: &AtomicUsize) -> Result<(), String> {
        let stop: unsafe extern "system" fn(*mut c_void) -> i32 = unsafe { method(self.raw, 18) };
        let position: unsafe extern "system" fn(*mut c_void, u32) -> i32 =
            unsafe { method(self.raw, 13) };
        check(unsafe { stop(self.raw) }, "DirectSound Stop before rebase")?;
        // Stop can complete after the service cursor sample. Measure again
        // while stopped, or the tail would replay bytes consumed meanwhile.
        self.sample_cursor()?;
        self.running = false;
        let remaining = (self.written - self.played) as usize;
        for i in (0..remaining).rev() {
            let at = ((self.played - self.origin + i as u64) % u64::from(self.capacity)) as usize;
            queue.push_front(self.shadow[at]);
        }
        pending.fetch_add(remaining, Ordering::AcqRel);
        self.written = self.played;
        self.origin = self.played;
        self.cursor_bytes = 0;
        self.last_cursor = 0;
        check(
            unsafe { position(self.raw, 0) },
            "DirectSound SetCurrentPosition at rebase",
        )?;
        self.write_region(0, &vec![self.silence; self.capacity as usize])
    }
    fn volume(&mut self, linear: u32) -> Result<(), String> {
        let db = if linear == 0 {
            -10000
        } else {
            (2000.0 * (f64::from(linear.min(32768)) / 32768.0).log10())
                .round()
                .clamp(-10000.0, 0.0) as i32
        };
        let set: unsafe extern "system" fn(*mut c_void, i32) -> i32 =
            unsafe { method(self.raw, 15) };
        check(unsafe { set(self.raw, db) }, "DirectSound SetVolume")
    }
    fn sample_cursor(&mut self) -> Result<u32, String> {
        let get: unsafe extern "system" fn(*mut c_void, *mut u32, *mut u32) -> i32 =
            unsafe { method(self.raw, 4) };
        let (mut cursor, mut safe_write) = (0, 0);
        check(
            unsafe { get(self.raw, &mut cursor, &mut safe_write) },
            "DirectSound GetCurrentPosition",
        )?;
        if cursor >= self.capacity || safe_write >= self.capacity {
            return Err("invalid DirectSound cursor".into());
        }
        let delta = (cursor + self.capacity - self.last_cursor) % self.capacity;
        self.cursor_bytes += u64::from(delta);
        self.played = (self.origin
            + self.cursor_bytes / u64::from(self.align) * u64::from(self.align))
        .min(self.written);
        self.last_cursor = cursor;
        Ok((safe_write + self.capacity - cursor) % self.capacity)
    }
    fn service(
        &mut self,
        queue: &mut VecDeque<u8>,
        pending: &AtomicUsize,
        position: &AtomicU64,
    ) -> Result<(), String> {
        let mut safe_distance = 0;
        if self.running {
            safe_distance = self.sample_cursor()?;
        }
        let buffered = self
            .written
            .checked_sub(self.played)
            .ok_or("audio counter reversed")?;
        if self.running
            && (buffered == 0
                || (!queue.is_empty() && buffered < u64::from(safe_distance + self.align)))
        {
            self.rebase(queue, pending)?;
        }
        position.store(self.played / u64::from(self.align), Ordering::Release);
        let buffered = self
            .written
            .checked_sub(self.played)
            .ok_or("audio counter reversed")?;
        let free = (u64::from(self.capacity - self.align)).saturating_sub(buffered);
        let amount =
            (queue.len().min(free as usize).min(65536) / self.align as usize) * self.align as usize;
        if amount != 0 {
            let bytes: Vec<u8> = queue.iter().take(amount).copied().collect();
            self.write_region(
                ((self.written - self.origin) % u64::from(self.capacity)) as u32,
                &bytes,
            )?;
            queue.drain(..amount);
            pending.fetch_sub(amount, Ordering::AcqRel);
            self.written += amount as u64;
            // Leave silence beyond the new tail, so a late poll at drain does
            // not replay PCM left over from an earlier trip around the ring.
            let guard = ((free as usize - amount).min(self.capacity as usize / 4)
                / self.align as usize)
                * self.align as usize;
            if guard != 0 {
                self.write_region(
                    ((self.written - self.origin) % u64::from(self.capacity)) as u32,
                    &vec![self.silence; guard],
                )?;
            }
        }
        if !self.running && self.written > self.played {
            let play: unsafe extern "system" fn(*mut c_void, u32, u32, u32) -> i32 =
                unsafe { method(self.raw, 12) };
            check(unsafe { play(self.raw, 0, 0, 1) }, "DirectSound Play")?;
            self.running = true;
        }
        Ok(())
    }
}
impl Drop for Buffer {
    fn drop(&mut self) {
        unsafe {
            let stop: unsafe extern "system" fn(*mut c_void) -> i32 = method(self.raw, 18);
            stop(self.raw);
            release(self.raw);
        }
    }
}
enum Command {
    Submit(Vec<u8>),
    Reset(mpsc::SyncSender<Result<(), String>>),
    Volume(u32, mpsc::SyncSender<Result<(), String>>),
    Quit,
}
pub struct DirectSoundSink {
    tx: mpsc::SyncSender<Command>,
    worker: Option<JoinHandle<()>>,
    position: Arc<AtomicU64>,
    pending: Arc<AtomicUsize>,
    error: Arc<Mutex<Option<String>>>,
    bits: u16,
}
impl DirectSoundSink {
    /// The device must be a live IDirectSound interface or zero for default.
    pub unsafe fn open(device: usize, rate: u32, channels: u16, bits: u16) -> Result<Self, String> {
        let device = unsafe { Device::acquire(device) }?;
        let (tx, rx) = mpsc::sync_channel(4);
        let (ready_tx, ready_rx) = mpsc::sync_channel(1);
        let position = Arc::new(AtomicU64::new(0));
        let pending = Arc::new(AtomicUsize::new(0));
        let error = Arc::new(Mutex::new(None));
        let (p, n, e) = (position.clone(), pending.clone(), error.clone());
        let worker = thread::Builder::new()
            .name("A2FO Bink audio".into())
            .spawn(move || {
                let mut buffer = match Buffer::new(device, rate, channels, bits) {
                    Ok(b) => {
                        let _ = ready_tx.send(Ok(()));
                        b
                    }
                    Err(err) => {
                        let _ = ready_tx.send(Err(err));
                        return;
                    }
                };
                let mut queue = VecDeque::new();
                loop {
                    let result = match rx.recv_timeout(Duration::from_millis(2)) {
                        Ok(Command::Submit(bytes)) => {
                            queue.extend(bytes);
                            Ok(())
                        }
                        Ok(Command::Volume(v, reply)) => {
                            let r = buffer.volume(v);
                            let _ = reply.send(r.clone());
                            r
                        }
                        Ok(Command::Reset(reply)) => {
                            n.fetch_sub(queue.len(), Ordering::AcqRel);
                            queue.clear();
                            let r = buffer.reset(bits);
                            p.store(0, Ordering::Release);
                            let _ = reply.send(r.clone());
                            r
                        }
                        Ok(Command::Quit) | Err(mpsc::RecvTimeoutError::Disconnected) => break,
                        Err(mpsc::RecvTimeoutError::Timeout) => Ok(()),
                    }
                    .and_then(|_| buffer.service(&mut queue, &n, &p));
                    if let Err(err) = result {
                        *e.lock().unwrap_or_else(|x| x.into_inner()) = Some(err);
                        break;
                    }
                }
            })
            .map_err(|e| e.to_string())?;
        match ready_rx.recv().map_err(|e| e.to_string())? {
            Ok(()) => Ok(Self {
                tx,
                worker: Some(worker),
                position,
                pending,
                error,
                bits,
            }),
            Err(err) => {
                let _ = worker.join();
                Err(err)
            }
        }
    }
    fn healthy(&self) -> Result<(), String> {
        match &*self.error.lock().unwrap_or_else(|x| x.into_inner()) {
            Some(e) => Err(e.clone()),
            None => Ok(()),
        }
    }
}
impl AudioSink for DirectSoundSink {
    fn submit(&mut self, samples: &[i16]) -> Result<(), String> {
        self.healthy()?;
        let bytes = samples
            .len()
            .checked_mul(usize::from(self.bits / 8))
            .ok_or("audio extent overflow")?;
        self.pending
            .fetch_update(Ordering::AcqRel, Ordering::Acquire, |n| {
                n.checked_add(bytes).filter(|v| *v <= 8 * 1024 * 1024)
            })
            .map_err(|_| "audio queue exceeds bound")?;
        let data = if self.bits == 8 {
            samples
                .iter()
                .map(|v| ((i32::from(*v) >> 8) + 128).clamp(0, 255) as u8)
                .collect()
        } else {
            samples.iter().flat_map(|v| v.to_le_bytes()).collect()
        };
        if self.tx.try_send(Command::Submit(data)).is_err() {
            self.pending.fetch_sub(bytes, Ordering::AcqRel);
            return Err("audio worker queue unavailable".into());
        }
        Ok(())
    }
    fn position_frames(&mut self) -> Result<u64, String> {
        self.healthy()?;
        Ok(self.position.load(Ordering::Acquire))
    }
    fn service(&mut self) -> Result<(), String> {
        self.healthy()
    }
    fn reset(&mut self) -> Result<(), String> {
        self.healthy()?;
        let (tx, rx) = mpsc::sync_channel(1);
        self.tx
            .send(Command::Reset(tx))
            .map_err(|e| e.to_string())?;
        rx.recv().map_err(|e| e.to_string())?
    }
    fn set_volume(&mut self, v: u32) -> Result<(), String> {
        self.healthy()?;
        let (tx, rx) = mpsc::sync_channel(1);
        self.tx
            .send(Command::Volume(v, tx))
            .map_err(|e| e.to_string())?;
        rx.recv().map_err(|e| e.to_string())?
    }
}
impl Drop for DirectSoundSink {
    fn drop(&mut self) {
        let _ = self.tx.send(Command::Quit);
        if let Some(t) = self.worker.take() {
            let _ = t.join();
        }
    }
}

#[cfg(test)]
#[path = "windows_audio_tests.rs"]
mod tests;
