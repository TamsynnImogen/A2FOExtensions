//! Versioned, panic-contained C boundary. Registry entries are opaque integers.
use crate::{
    codec::{AudioReader, SourceInfo, VideoReader},
    control::Control,
    host::{self, HostApi},
    player,
};
use std::{
    collections::BTreeMap,
    ffi::{CString, c_char, c_void},
    path::PathBuf,
    sync::{Arc, Mutex, OnceLock, atomic::Ordering},
};
#[repr(C)]
pub struct Open {
    size: u32,
    version: u32,
    path: *const u16,
    path_length: u32,
    threads: u32,
    log: Option<unsafe extern "C" fn(*mut c_void, *const c_char)>,
    log_context: *mut c_void,
}
#[repr(C)]
pub struct Info {
    size: u32,
    width: u32,
    height: u32,
    has_audio: u32,
    duration_us: i64,
}
#[repr(C)]
pub struct Play {
    size: u32,
    version: u32,
    window: usize,
    host: *const HostApi,
}
#[derive(Clone, Copy)]
struct Logger {
    callback: Option<unsafe extern "C" fn(*mut c_void, *const c_char)>,
    context: usize,
}
impl Logger {
    fn write(self, message: &str) {
        if let (Some(callback), Ok(text)) = (self.callback, CString::new(message)) {
            unsafe { callback(self.context as *mut c_void, text.as_ptr()) }
        }
    }
}
struct Session {
    path: PathBuf,
    threads: usize,
    info: SourceInfo,
    opened: Mutex<Option<(VideoReader, Option<AudioReader>)>>,
    control: Arc<Control>,
    logger: Logger,
}
struct State {
    enabled: bool,
    next: u32,
    active: Option<(u32, Arc<Session>)>,
    sessions: BTreeMap<u32, Arc<Session>>,
    opening: BTreeMap<u32, Arc<Control>>,
    shutdowns: u32,
}
fn state() -> &'static Mutex<State> {
    static S: OnceLock<Mutex<State>> = OnceLock::new();
    S.get_or_init(|| {
        Mutex::new(State {
            enabled: true,
            next: 1,
            active: None,
            sessions: BTreeMap::new(),
            opening: BTreeMap::new(),
            shutdowns: 0,
        })
    })
}
fn lock() -> std::sync::MutexGuard<'static, State> {
    state().lock().unwrap_or_else(|p| p.into_inner())
}
fn get(id: u32) -> Option<Arc<Session>> {
    lock().sessions.get(&id).cloned()
}
fn contained(f: impl FnOnce() -> u32) -> u32 {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)).unwrap_or(0)
}
struct Opening {
    id: u32,
    control: Arc<Control>,
}
impl Drop for Opening {
    fn drop(&mut self) {
        let mut s = lock();
        s.opening.remove(&self.id);
        self.control.finish();
    }
}
struct Shutting;
impl Drop for Shutting {
    fn drop(&mut self) {
        lock().shutdowns -= 1;
    }
}
unsafe extern "C" fn open(request: *const Open) -> u32 {
    contained(|| {
        if request.is_null() {
            return 0;
        }
        // SAFETY: caller supplies valid, aligned C structures and readable buffers.
        if unsafe { request.cast::<u32>().read() } < std::mem::size_of::<Open>() as u32 {
            return 0;
        }
        let r = unsafe { &*request };
        if r.version != 1 || r.path.is_null() || r.path_length == 0 || r.path_length > 32767 {
            return 0;
        }
        let opening = {
            let mut s = lock();
            if !s.enabled || s.next == 0 {
                return 0;
            }
            let id = s.next;
            s.next = s.next.checked_add(1).unwrap_or(0);
            let control = Arc::new(Control::new());
            control.start();
            s.opening.insert(id, control.clone());
            Opening { id, control }
        };
        let logger = Logger {
            callback: r.log,
            context: r.log_context as usize,
        };
        #[cfg(windows)]
        if !crate::windows::cleanup() {
            logger.write(
                "Native audio buffers are still owned by the driver; playback admission refused",
            );
            return 0;
        }
        let path = unsafe { std::slice::from_raw_parts(r.path, r.path_length as usize) };
        #[cfg(windows)]
        let path = {
            use std::os::windows::ffi::OsStringExt;
            PathBuf::from(std::ffi::OsString::from_wide(path))
        };
        #[cfg(not(windows))]
        let path = match String::from_utf16(path) {
            Ok(p) => PathBuf::from(p),
            Err(_) => return 0,
        };
        let threads = (r.threads as usize).clamp(1, 8).min(
            std::thread::available_parallelism()
                .map(|n| n.get())
                .unwrap_or(1),
        );
        let opened = (|| {
            let video = VideoReader::open(&path, threads)?;
            let audio = AudioReader::open(&path, 0)?;
            Ok::<_, String>((video, audio))
        })();
        let (video, audio) = match opened {
            Ok(v) => v,
            Err(e) => {
                logger.write(&format!("Rust media open failed: {e}"));
                return 0;
            }
        };
        let i = video.info();
        let info = SourceInfo {
            width: i.width,
            height: i.height,
            duration_us: i.duration_us,
            has_audio: audio.is_some(),
        };
        let mut s = lock();
        if !s.enabled || opening.control.cancelled() {
            return 0;
        }
        let id = opening.id;
        s.sessions.insert(
            id,
            Arc::new(Session {
                path,
                threads,
                info,
                opened: Mutex::new(Some((video, audio))),
                control: Arc::new(Control::new()),
                logger,
            }),
        );
        id
    })
}
unsafe extern "C" fn info(id: u32, result: *mut Info) -> u32 {
    contained(|| {
        if result.is_null() {
            return 0;
        }
        if unsafe { result.cast::<u32>().read() } < std::mem::size_of::<Info>() as u32 {
            return 0;
        }
        let r = unsafe { &mut *result };
        let Some(s) = get(id) else {
            return 0;
        };
        r.width = s.info.width;
        r.height = s.info.height;
        r.has_audio = u32::from(s.info.has_audio);
        r.duration_us = s.info.duration_us;
        1
    })
}
struct Playing {
    id: u32,
    session: Arc<Session>,
}
impl Drop for Playing {
    fn drop(&mut self) {
        let mut s = lock();
        if s.active.as_ref().is_some_and(|(id, _)| *id == self.id) {
            s.active = None;
        }
        self.session.control.finish();
    }
}
unsafe extern "C" fn play(id: u32, request: *const Play) -> u32 {
    contained(|| {
        if request.is_null() {
            return 0;
        }
        if unsafe { request.cast::<u32>().read() } < std::mem::size_of::<Play>() as u32 {
            return 0;
        }
        let r = unsafe { &*request };
        if r.version != 1 {
            return 0;
        }
        let session = {
            let mut s = lock();
            if !s.enabled {
                return 2;
            }
            if s.active.is_some() {
                return 3;
            }
            let Some(session) = s.sessions.get(&id).cloned() else {
                return 0;
            };
            if session.control.cancelled() {
                return 2;
            }
            session.control.start();
            s.active = Some((id, session.clone()));
            session
        };
        let guard = Playing {
            id,
            session: session.clone(),
        };
        let host = if !r.host.is_null() {
            if unsafe { r.host.cast::<u32>().read() } < std::mem::size_of::<HostApi>() as u32 {
                return 0;
            }
            unsafe { host::external(*r.host) }
        } else {
            #[cfg(windows)]
            {
                crate::windows::native(r.window)
            }
            #[cfg(not(windows))]
            {
                Err::<Arc<dyn host::Host>, String>("native playback requires Windows".into())
            }
        };
        let result = (|| {
            let host = host?;
            let opened = guard
                .session
                .opened
                .lock()
                .unwrap_or_else(|p| p.into_inner())
                .take();
            let (video, audio) = opened.ok_or("session already played")?;
            player::play(
                &session.path,
                session.threads,
                video,
                audio,
                host,
                session.control.clone(),
                &|message| session.logger.write(message),
            )
        })();
        match result {
            Ok(code) => code,
            Err(e) => {
                session
                    .logger
                    .write(&format!("Rust media playback failed: {e}"));
                0
            }
        }
    })
}
unsafe extern "C" fn seek(id: u32, target: i64) -> u32 {
    contained(|| {
        let Some(s) = get(id) else {
            return 0;
        };
        let active = lock()
            .active
            .as_ref()
            .is_some_and(|(active, _)| *active == id);
        if target < 0
            || s.control.cancelled()
            || (!active && s.opened.lock().unwrap_or_else(|p| p.into_inner()).is_none())
        {
            return 0;
        }
        *s.control.seek.lock().unwrap_or_else(|p| p.into_inner()) = Some(target);
        1
    })
}
unsafe extern "C" fn cancel(id: u32) {
    let _ = contained(|| {
        if let Some(s) = get(id) {
            s.control.cancel.store(true, Ordering::Release);
        }
        1
    });
}
unsafe extern "C" fn close(id: u32) {
    let _ = contained(|| {
        let session = { lock().sessions.remove(&id) };
        if let Some(s) = session {
            s.control.cancel.store(true, Ordering::Release);
            s.control.wait();
        }
        1
    });
}
unsafe extern "C" fn shutdown() -> u32 {
    contained(|| {
        let (sessions, opening) = {
            let mut s = lock();
            s.enabled = false;
            s.shutdowns += 1;
            let mut sessions = std::mem::take(&mut s.sessions);
            if let Some((id, session)) = &s.active {
                sessions.insert(*id, session.clone());
            }
            (sessions, s.opening.values().cloned().collect::<Vec<_>>())
        };
        let _shutting = Shutting;
        for session in sessions.values() {
            session.control.cancel.store(true, Ordering::Release);
        }
        for control in &opening {
            control.cancel.store(true, Ordering::Release);
        }
        let mut complete = true;
        for control in opening {
            complete &= control.wait();
        }
        for session in sessions.values() {
            complete &= session.control.wait();
        }
        // Keep the shutdown admission gate closed through idle decoder/file
        // destruction as well as active worker completion.
        drop(sessions);
        #[cfg(windows)]
        {
            if complete {
                complete = crate::windows::cleanup();
            }
        }
        u32::from(complete)
    })
}
unsafe extern "C" fn startup() -> u32 {
    contained(|| {
        let mut s = lock();
        if s.active.is_some() || !s.opening.is_empty() || s.shutdowns != 0 {
            return 0;
        }
        #[cfg(windows)]
        if !crate::windows::cleanup() {
            return 0;
        }
        s.enabled = true;
        1
    })
}
#[repr(C)]
pub struct Api {
    size: u32,
    version: u32,
    open: unsafe extern "C" fn(*const Open) -> u32,
    info: unsafe extern "C" fn(u32, *mut Info) -> u32,
    play: unsafe extern "C" fn(u32, *const Play) -> u32,
    seek: unsafe extern "C" fn(u32, i64) -> u32,
    cancel: unsafe extern "C" fn(u32),
    close: unsafe extern "C" fn(u32),
    shutdown: unsafe extern "C" fn() -> u32,
    startup: unsafe extern "C" fn() -> u32,
}
static API: Api = Api {
    size: std::mem::size_of::<Api>() as u32,
    version: 1,
    open,
    info,
    play,
    seek,
    cancel,
    close,
    shutdown,
    startup,
};
#[unsafe(no_mangle)]
pub extern "C" fn A2FOMedia_GetApiV1(version: u32) -> *const Api {
    if version == 1 { &API } else { std::ptr::null() }
}
