//! Native output only; codec and resource ownership remain in Rust.
use crate::host::{Host, Poll, Sound};
use std::{
    ptr::{addr_of, null_mut},
    sync::{Arc, Mutex, OnceLock},
    thread::ThreadId,
    time::{Duration, Instant},
};
use windows_sys::Win32::{
    Foundation::*,
    Graphics::Gdi::*,
    Media::{Audio::*, MMTIME, TIME_BYTES, TIME_MS, TIME_SAMPLES},
    System::Threading::GetCurrentProcessId,
    UI::{Input::KeyboardAndMouse::GetAsyncKeyState, WindowsAndMessaging::*},
};
struct Canvas {
    dc: HDC,
    bitmap: HBITMAP,
    old: HGDIOBJ,
    bits: *mut u8,
    width: u32,
    height: u32,
}
impl Default for Canvas {
    fn default() -> Self {
        Self {
            dc: null_mut(),
            bitmap: null_mut(),
            old: null_mut(),
            bits: null_mut(),
            width: 0,
            height: 0,
        }
    }
}
// Canvas is accessed only on its creating UI thread; mutex protects ownership.
unsafe impl Send for Canvas {}
impl Drop for Canvas {
    fn drop(&mut self) {
        unsafe {
            if !self.dc.is_null() && !self.old.is_null() {
                SelectObject(self.dc, self.old);
            }
            if !self.bitmap.is_null() {
                DeleteObject(self.bitmap);
            }
            if !self.dc.is_null() {
                DeleteDC(self.dc);
            }
        }
    }
}
impl Canvas {
    fn resize(&mut self, w: u32, h: u32) -> Result<(), String> {
        if self.width == w && self.height == h {
            return Ok(());
        }
        *self = Self::default();
        unsafe {
            self.dc = CreateCompatibleDC(null_mut());
            if self.dc.is_null() {
                return Err("CreateCompatibleDC failed".into());
            }
            let mut bi = BITMAPINFO::default();
            bi.bmiHeader.biSize = size_of::<BITMAPINFOHEADER>() as u32;
            bi.bmiHeader.biWidth = w as i32;
            bi.bmiHeader.biHeight = -(h as i32);
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            let mut bits = null_mut();
            self.bitmap = CreateDIBSection(self.dc, &bi, DIB_RGB_COLORS, &mut bits, null_mut(), 0);
            if self.bitmap.is_null() || bits.is_null() {
                return Err("CreateDIBSection failed".into());
            }
            let old = SelectObject(self.dc, self.bitmap);
            if old.is_null() || old as isize == -1 {
                return Err("SelectObject failed".into());
            }
            self.old = old;
            self.bits = bits.cast();
            self.width = w;
            self.height = h;
        }
        Ok(())
    }
    fn present(&mut self, window: HWND, data: &[u8], w: u32, h: u32) -> Result<(), String> {
        self.resize(w, h)?;
        unsafe {
            if GdiFlush() == 0 {
                return Err("GDI flush failed".into());
            }
            std::ptr::copy_nonoverlapping(data.as_ptr(), self.bits, data.len());
            let output = GetDCEx(window, null_mut(), DCX_CACHE | DCX_CLIPSIBLINGS);
            if output.is_null() {
                return Err("GetDCEx failed".into());
            }
            let saved = SaveDC(output);
            let identity = XFORM {
                eM11: 1.,
                eM12: 0.,
                eM21: 0.,
                eM22: 1.,
                eDx: 0.,
                eDy: 0.,
            };
            let ok = saved != 0
                && SetGraphicsMode(output, GM_ADVANCED) != 0
                && SetWorldTransform(output, &identity) != 0
                && SetMapMode(output, MM_TEXT) != 0
                && SetWindowOrgEx(output, 0, 0, null_mut()) != 0
                && SetViewportOrgEx(output, 0, 0, null_mut()) != 0
                && SelectClipRgn(output, null_mut()) != 0
                && BitBlt(output, 0, 0, w as i32, h as i32, self.dc, 0, 0, SRCCOPY) != 0
                && GdiFlush() != 0;
            let restored = saved != 0 && RestoreDC(output, saved) != 0;
            ReleaseDC(window, output);
            if !ok || !restored {
                return Err("movie presentation failed".into());
            }
        }
        Ok(())
    }
}
struct Native {
    window: usize,
    owner: ThreadId,
    start: Instant,
    canvas: Mutex<Canvas>,
}
impl Native {
    fn own_thread(&self) -> Result<(), String> {
        if self.owner != std::thread::current().id() {
            Err("movie UI called from wrong thread".into())
        } else {
            Ok(())
        }
    }
}
impl Host for Native {
    fn now(&self) -> i64 {
        self.start.elapsed().as_micros().min(i64::MAX as u128) as i64
    }
    fn sleep(&self, ms: u32) {
        std::thread::sleep(Duration::from_millis(ms as u64));
    }
    fn poll(&self, paints: bool) -> Result<Poll, String> {
        self.own_thread()?;
        let hwnd = self.window as HWND;
        unsafe {
            let mut msg = MSG::default();
            let mut closing = false;
            if PeekMessageW(&mut msg, null_mut(), WM_CLOSE, WM_CLOSE, PM_NOREMOVE) != 0
                && (msg.message == WM_CLOSE || msg.message == WM_QUIT)
            {
                closing = true;
            }
            if !closing && IsWindow(hwnd) != 0 && paints {
                for _ in 0..64 {
                    if PeekMessageW(
                        &mut msg,
                        null_mut(),
                        WM_PAINT,
                        WM_PAINT,
                        PM_REMOVE | PM_NOYIELD,
                    ) == 0
                    {
                        break;
                    }
                    if msg.message == WM_QUIT {
                        PostQuitMessage(msg.wParam as i32);
                        closing = true;
                        break;
                    }
                    if IsWindow(hwnd) == 0 {
                        break;
                    }
                    DispatchMessageW(&msg);
                    if IsWindow(hwnd) == 0 {
                        break;
                    }
                }
            }
            let mut pid = 0;
            GetWindowThreadProcessId(hwnd, &mut pid);
            let valid =
                IsWindow(hwnd) != 0 && IsWindowVisible(hwnd) != 0 && pid == GetCurrentProcessId();
            let mut rect = RECT::default();
            if valid && GetClientRect(hwnd, &mut rect) == 0 {
                return Err("GetClientRect failed".into());
            }
            let keys =
                [0x1b, 0x20, 0x0d, 0x01, 0x02]
                    .iter()
                    .enumerate()
                    .fold(0, |mask, (i, key)| {
                        mask | if GetAsyncKeyState(*key) < 0 {
                            1 << i
                        } else {
                            0
                        }
                    });
            let mut foreground_pid = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &mut foreground_pid);
            Ok(Poll {
                width: (rect.right - rect.left).max(0) as u32,
                height: (rect.bottom - rect.top).max(0) as u32,
                keys,
                foreground: u32::from(foreground_pid == GetCurrentProcessId()),
                valid: u32::from(valid),
                closing: u32::from(closing),
            })
        }
    }
    fn present(&self, data: &[u8], w: u32, h: u32) -> Result<(), String> {
        self.own_thread()?;
        if data.len() != w as usize * h as usize * 4 {
            return Err("invalid presentation buffer".into());
        }
        self.canvas
            .lock()
            .unwrap_or_else(|p| p.into_inner())
            .present(self.window as HWND, data, w, h)
    }
    fn sound(&self) -> Result<Box<dyn Sound>, String> {
        Ok(Box::new(NativeSound::open()?))
    }
}
pub fn native(window: usize) -> Result<Arc<dyn Host>, String> {
    unsafe {
        let mut pid = 0;
        let thread = GetWindowThreadProcessId(window as HWND, &mut pid);
        if thread == 0 || pid != GetCurrentProcessId() || IsWindow(window as HWND) == 0 {
            return Err("movie requires a process-owned window".into());
        }
    }
    Ok(Arc::new(Native {
        window,
        owner: std::thread::current().id(),
        start: Instant::now(),
        canvas: Mutex::new(Canvas::default()),
    }))
}
struct Slot {
    header: Box<WAVEHDR>,
    _pcm: Box<[i16]>,
    frames: u64,
}
#[derive(Clone, Copy)]
struct WaveOps {
    reset: unsafe extern "system" fn(HWAVEOUT) -> u32,
    unprepare: unsafe extern "system" fn(HWAVEOUT, *mut WAVEHDR, u32) -> u32,
    close: unsafe extern "system" fn(HWAVEOUT) -> u32,
}
const WAVE_OPS: WaveOps = WaveOps {
    reset: waveOutReset,
    unprepare: waveOutUnprepareHeader,
    close: waveOutClose,
};
struct Wave {
    ops: WaveOps,
    handle: HWAVEOUT,
    slots: Vec<Slot>,
    submitted: u64,
    completed: u64,
    last: u64,
    raw_last: u32,
    wrap: u64,
    kind: u32,
}
// WAVEHDR/data boxes remain at fixed addresses until the driver unprepares them.
// The entire device moves to one audio worker; retired devices are mutex owned.
unsafe impl Send for Wave {}
impl Wave {
    fn reap(&mut self) -> Result<(), String> {
        let mut i = 0;
        while i < self.slots.len() {
            let slot = &mut self.slots[i];
            let done =
                unsafe { std::ptr::read_volatile(addr_of!(slot.header.dwFlags).cast::<u8>()) }
                    & WHDR_DONE as u8
                    != 0;
            if done {
                let result = unsafe {
                    (self.ops.unprepare)(
                        self.handle,
                        slot.header.as_mut(),
                        size_of::<WAVEHDR>() as u32,
                    )
                };
                if result != 0 {
                    return Err(format!("waveOutUnprepareHeader: {result}"));
                }
                self.completed += slot.frames;
                self.slots.remove(i);
            } else {
                i += 1;
            }
        }
        Ok(())
    }
    fn release(&mut self) -> bool {
        unsafe {
            if self.handle.is_null() {
                return true;
            }
            if (self.ops.reset)(self.handle) != 0 {
                return false;
            }
            let mut i = 0;
            while i < self.slots.len() {
                if (self.ops.unprepare)(
                    self.handle,
                    self.slots[i].header.as_mut(),
                    size_of::<WAVEHDR>() as u32,
                ) == 0
                {
                    self.slots.remove(i);
                } else {
                    i += 1;
                }
            }
            if !self.slots.is_empty() {
                return false;
            }
            if (self.ops.close)(self.handle) != 0 {
                return false;
            }
            self.handle = null_mut();
            true
        }
    }
}
fn retired() -> &'static Mutex<Vec<Wave>> {
    static R: OnceLock<Mutex<Vec<Wave>>> = OnceLock::new();
    R.get_or_init(|| Mutex::new(Vec::new()))
}
pub fn cleanup() -> bool {
    let mut list = retired().lock().unwrap_or_else(|p| p.into_inner());
    list.retain_mut(|wave| !wave.release());
    list.is_empty()
}
struct NativeSound {
    wave: Option<Wave>,
}
impl NativeSound {
    fn open() -> Result<Self, String> {
        let mut handle = null_mut();
        let format = WAVEFORMATEX {
            wFormatTag: 1,
            nChannels: 2,
            nSamplesPerSec: 48000,
            nAvgBytesPerSec: 192000,
            nBlockAlign: 4,
            wBitsPerSample: 16,
            cbSize: 0,
        };
        let status = unsafe { waveOutOpen(&mut handle, WAVE_MAPPER, &format, 0, 0, 0) };
        if status != 0 {
            return Err(format!("waveOutOpen: {status}"));
        }
        Ok(Self {
            wave: Some(Wave {
                ops: WAVE_OPS,
                handle,
                slots: Vec::new(),
                submitted: 0,
                completed: 0,
                last: 0,
                raw_last: 0,
                wrap: 0,
                kind: TIME_SAMPLES,
            }),
        })
    }
}
impl Sound for NativeSound {
    fn write(&mut self, pcm: &[i16]) -> Result<bool, String> {
        let w = self.wave.as_mut().unwrap();
        w.reap()?;
        if w.slots.len() >= 6 {
            return Ok(false);
        }
        if pcm.len() % 2 != 0 || pcm.len() > 2048 {
            return Err("invalid PCM device chunk".into());
        }
        let mut data = pcm.to_vec().into_boxed_slice();
        let mut header = Box::new(WAVEHDR {
            lpData: data.as_mut_ptr().cast(),
            dwBufferLength: (data.len() * 2) as u32,
            ..Default::default()
        });
        let status =
            unsafe { waveOutPrepareHeader(w.handle, header.as_mut(), size_of::<WAVEHDR>() as u32) };
        if status != 0 {
            return Err(format!("waveOutPrepareHeader: {status}"));
        }
        // Transfer ownership before Write; even a failed Write must unprepare first.
        let frames = (pcm.len() / 2) as u64;
        w.slots.push(Slot {
            header,
            _pcm: data,
            frames,
        });
        let slot = w.slots.last_mut().unwrap();
        let status =
            unsafe { waveOutWrite(w.handle, slot.header.as_mut(), size_of::<WAVEHDR>() as u32) };
        if status != 0 {
            return Err(format!("waveOutWrite: {status}"));
        }
        w.submitted += frames;
        Ok(true)
    }
    fn position(&mut self) -> u64 {
        let w = self.wave.as_mut().unwrap();
        let mut time = MMTIME {
            wType: TIME_SAMPLES,
            ..Default::default()
        };
        if unsafe { waveOutGetPosition(w.handle, &mut time, size_of::<MMTIME>() as u32) } == 0 {
            let kind = time.wType;
            let raw = unsafe {
                match kind {
                    TIME_SAMPLES => time.u.sample,
                    TIME_BYTES => time.u.cb,
                    TIME_MS => time.u.ms,
                    _ => return w.last,
                }
            };
            if kind != w.kind {
                w.kind = kind;
                w.raw_last = raw;
                w.wrap = 0;
            } else if raw < w.raw_last && w.raw_last - raw > 0x80000000 {
                w.wrap += 1u64 << 32;
            }
            w.raw_last = raw;
            let value = match kind {
                TIME_BYTES => (w.wrap + raw as u64) / 4,
                TIME_MS => (w.wrap + raw as u64) * 48,
                _ => w.wrap + raw as u64,
            };
            w.last = w.last.max(value.min(w.submitted));
        }
        w.last = w.last.max(w.completed);
        w.last
    }
    fn pending(&mut self) -> Result<bool, String> {
        let w = self.wave.as_mut().unwrap();
        w.reap()?;
        Ok(!w.slots.is_empty())
    }
    fn reset(&mut self) {
        if let Some(w) = &mut self.wave {
            unsafe {
                (w.ops.reset)(w.handle);
            }
        }
    }
}
impl Drop for NativeSound {
    fn drop(&mut self) {
        if let Some(mut wave) = self.wave.take() {
            if !wave.release() {
                retired()
                    .lock()
                    .unwrap_or_else(|p| p.into_inner())
                    .push(wave);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicBool, Ordering};
    static FAIL: AtomicBool = AtomicBool::new(true);
    unsafe extern "system" fn reset(_: HWAVEOUT) -> u32 {
        0
    }
    unsafe extern "system" fn unprepare(_: HWAVEOUT, _: *mut WAVEHDR, _: u32) -> u32 {
        if FAIL.load(Ordering::Acquire) { 33 } else { 0 }
    }
    unsafe extern "system" fn close(_: HWAVEOUT) -> u32 {
        0
    }
    #[test]
    fn failed_unprepare_retains_buffer_until_retry() {
        FAIL.store(true, Ordering::Release);
        let mut pcm = vec![1i16; 2048].into_boxed_slice();
        let header = Box::new(WAVEHDR {
            lpData: pcm.as_mut_ptr().cast(),
            dwFlags: WHDR_DONE,
            dwBufferLength: 4096,
            ..Default::default()
        });
        let pointer = header.lpData;
        let mut wave = Wave {
            ops: WaveOps {
                reset,
                unprepare,
                close,
            },
            handle: 1usize as HWAVEOUT,
            slots: vec![Slot {
                header,
                _pcm: pcm,
                frames: 1024,
            }],
            submitted: 1024,
            completed: 0,
            last: 0,
            raw_last: 0,
            wrap: 0,
            kind: TIME_SAMPLES,
        };
        assert!(wave.reap().is_err());
        assert_eq!(wave.slots.len(), 1);
        assert_eq!(
            wave.slots[0]._pcm.as_ptr().cast::<u8>() as usize,
            pointer as usize
        );
        assert!(!wave.release());
        assert_eq!(wave.slots.len(), 1);
        FAIL.store(false, Ordering::Release);
        assert!(wave.release());
        assert!(wave.slots.is_empty());
        assert!(wave.handle.is_null());
    }
}
