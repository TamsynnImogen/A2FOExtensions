use std::{ffi::c_void, sync::Arc};
#[repr(C)]
#[derive(Default, Clone, Copy, Debug)]
pub struct Poll {
    pub width: u32,
    pub height: u32,
    pub keys: u32,
    pub foreground: u32,
    pub valid: u32,
    pub closing: u32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct HostApi {
    pub size: u32,
    pub version: u32,
    pub context: *mut c_void,
    pub now_us: Option<unsafe extern "C" fn(*mut c_void) -> i64>,
    pub sleep_ms: Option<unsafe extern "C" fn(*mut c_void, u32)>,
    pub poll: Option<unsafe extern "C" fn(*mut c_void, u32, *mut Poll) -> u32>,
    pub present: Option<unsafe extern "C" fn(*mut c_void, *const u8, u32, u32, u32) -> u32>,
    pub audio_open: Option<unsafe extern "C" fn(*mut c_void) -> u32>,
    pub audio_write: Option<unsafe extern "C" fn(*mut c_void, *const i16, u32) -> u32>,
    pub audio_position: Option<unsafe extern "C" fn(*mut c_void) -> u64>,
    pub audio_pending: Option<unsafe extern "C" fn(*mut c_void) -> u32>,
    pub audio_reset: Option<unsafe extern "C" fn(*mut c_void)>,
    pub audio_close: Option<unsafe extern "C" fn(*mut c_void)>,
}
pub trait Sound: Send {
    fn write(&mut self, pcm: &[i16]) -> Result<bool, String>;
    fn position(&mut self) -> u64;
    fn pending(&mut self) -> Result<bool, String>;
    fn reset(&mut self);
}
pub trait Host: Send + Sync {
    fn now(&self) -> i64;
    fn sleep(&self, ms: u32);
    fn poll(&self, paints: bool) -> Result<Poll, String>;
    fn present(&self, bgra: &[u8], width: u32, height: u32) -> Result<(), String>;
    fn sound(&self) -> Result<Box<dyn Sound>, String>;
}
#[derive(Clone, Copy)]
struct Callbacks(HostApi);
// SAFETY: the HostApi contract explicitly requires thread-safe context/clock
// callbacks, serialized UI callbacks and serialized audio callbacks. The caller
// keeps its context and code live until the synchronous play call returns.
unsafe impl Send for Callbacks {}
unsafe impl Sync for Callbacks {}
struct External {
    api: Callbacks,
}
struct ExternalSound {
    api: Callbacks,
}
impl Host for External {
    fn now(&self) -> i64 {
        unsafe { self.api.0.now_us.unwrap()(self.api.0.context) }
    }
    fn sleep(&self, ms: u32) {
        unsafe { self.api.0.sleep_ms.unwrap()(self.api.0.context, ms) }
    }
    fn poll(&self, paints: bool) -> Result<Poll, String> {
        let mut result = Poll::default();
        if unsafe { self.api.0.poll.unwrap()(self.api.0.context, u32::from(paints), &mut result) }
            == 0
        {
            return Err("host event poll failed".into());
        }
        Ok(result)
    }
    fn present(&self, bgra: &[u8], w: u32, h: u32) -> Result<(), String> {
        if unsafe { self.api.0.present.unwrap()(self.api.0.context, bgra.as_ptr(), w, h, w * 4) }
            == 0
        {
            Err("host presentation failed".into())
        } else {
            Ok(())
        }
    }
    fn sound(&self) -> Result<Box<dyn Sound>, String> {
        if unsafe { self.api.0.audio_open.unwrap()(self.api.0.context) } == 0 {
            return Err("host audio open failed".into());
        }
        Ok(Box::new(ExternalSound { api: self.api }))
    }
}
impl Sound for ExternalSound {
    fn write(&mut self, pcm: &[i16]) -> Result<bool, String> {
        match unsafe {
            self.api.0.audio_write.unwrap()(
                self.api.0.context,
                pcm.as_ptr(),
                (pcm.len() / 2) as u32,
            )
        } {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err("host audio write failed".into()),
        }
    }
    fn position(&mut self) -> u64 {
        unsafe { self.api.0.audio_position.unwrap()(self.api.0.context) }
    }
    fn pending(&mut self) -> Result<bool, String> {
        match unsafe { self.api.0.audio_pending.unwrap()(self.api.0.context) } {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err("host audio drain failed".into()),
        }
    }
    fn reset(&mut self) {
        unsafe { self.api.0.audio_reset.unwrap()(self.api.0.context) }
    }
}
impl Drop for ExternalSound {
    fn drop(&mut self) {
        unsafe { self.api.0.audio_close.unwrap()(self.api.0.context) }
    }
}
/// # Safety
/// `api` is a valid copy of the C callback table and satisfies its lifetime and
/// synchronization contract until all player-owned workers have joined.
pub unsafe fn external(api: HostApi) -> Result<Arc<dyn Host>, String> {
    if api.version != 1
        || (api.size as usize) < std::mem::size_of::<HostApi>()
        || api.now_us.is_none()
        || api.sleep_ms.is_none()
        || api.poll.is_none()
        || api.present.is_none()
        || api.audio_open.is_none()
        || api.audio_write.is_none()
        || api.audio_position.is_none()
        || api.audio_pending.is_none()
        || api.audio_reset.is_none()
        || api.audio_close.is_none()
    {
        return Err("incomplete media host interface".into());
    }
    Ok(Arc::new(External {
        api: Callbacks(api),
    }))
}
