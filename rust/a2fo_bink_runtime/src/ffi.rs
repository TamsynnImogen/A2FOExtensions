//! Observed primary Armada/FleetOpsHook ABI. No native Bink is loaded here.
use crate::{
    pixels,
    state::{AudioSink, Movie, Prefix},
    surface,
    windows_audio::{Device, DirectSoundSink},
};
use a2fo_bink_candidate::Session;
use core::{ffi::c_void, ptr};
use std::os::windows::ffi::OsStringExt;
use std::{
    cell::RefCell,
    collections::BTreeMap,
    ffi::CString,
    path::PathBuf,
    sync::{Arc, Mutex, OnceLock},
    time::Instant,
};
use windows_sys::Win32::{
    Globalization::{CP_ACP, MultiByteToWideChar},
    System::Memory::*,
};

type Handle = *mut Prefix;
fn movies() -> &'static Mutex<BTreeMap<usize, Box<Movie>>> {
    static M: OnceLock<Mutex<BTreeMap<usize, Box<Movie>>>> = OnceLock::new();
    M.get_or_init(|| Mutex::new(BTreeMap::new()))
}
struct Provider {
    device: Option<Device>,
    parameter: usize,
    enabled: bool,
}
fn provider() -> &'static Mutex<Provider> {
    static P: OnceLock<Mutex<Provider>> = OnceLock::new();
    P.get_or_init(|| {
        Mutex::new(Provider {
            device: None,
            parameter: 0,
            enabled: false,
        })
    })
}
thread_local! {static ERROR:RefCell<CString>=RefCell::new(CString::new("").unwrap());}
fn error(message: impl AsRef<str>) {
    let s = message.as_ref().replace('\0', "?");
    ERROR.with(|e| *e.borrow_mut() = CString::new(s).unwrap_or_default());
}
fn boundary<T>(fallback: T, action: impl FnOnce() -> Result<T, String>) -> T {
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(action)) {
        Ok(Ok(v)) => v,
        Ok(Err(e)) => {
            error(e);
            fallback
        }
        Err(_) => {
            error("Rust Bink operation panicked");
            fallback
        }
    }
}
fn with_movie<T>(
    handle: Handle,
    action: impl FnOnce(&mut Movie) -> Result<T, String>,
) -> Result<T, String> {
    let mut all = movies().lock().map_err(|_| "Bink registry poisoned")?;
    action(
        all.get_mut(&(handle as usize))
            .ok_or("invalid or closed Bink handle")?,
    )
}
/// Checks mapped pages, not the lifetime or semantic validity of caller memory.
unsafe fn mapped(pointer: *const u8, len: usize, write: bool) -> bool {
    if len == 0 {
        return true;
    }
    if pointer.is_null() {
        return false;
    }
    let start = pointer as usize;
    let Some(end) = start.checked_add(len) else {
        return false;
    };
    let mut at = start;
    while at < end {
        let mut info: MEMORY_BASIC_INFORMATION = unsafe { std::mem::zeroed() };
        if unsafe {
            VirtualQuery(
                at as *const c_void,
                &mut info,
                size_of::<MEMORY_BASIC_INFORMATION>(),
            )
        } == 0
        {
            return false;
        }
        if info.State != MEM_COMMIT || info.Protect & (PAGE_GUARD | PAGE_NOACCESS) != 0 {
            return false;
        }
        let allowed = if write {
            PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY
        } else {
            PAGE_READONLY
                | PAGE_READWRITE
                | PAGE_WRITECOPY
                | PAGE_EXECUTE_READ
                | PAGE_EXECUTE_READWRITE
                | PAGE_EXECUTE_WRITECOPY
        };
        if info.Protect & allowed == 0 {
            return false;
        }
        let Some(next) = (info.BaseAddress as usize).checked_add(info.RegionSize) else {
            return false;
        };
        if next <= at {
            return false;
        }
        at = next;
    }
    true
}
unsafe fn name(pointer: *const u8) -> Result<PathBuf, String> {
    let mut bytes = Vec::new();
    for n in 0..32768 {
        let p = pointer.wrapping_add(n);
        if !unsafe { mapped(p, 1, false) } {
            return Err("invalid Bink filename pointer".into());
        }
        let c = unsafe { *p };
        if c == 0 {
            let len = unsafe {
                MultiByteToWideChar(
                    CP_ACP,
                    0,
                    bytes.as_ptr(),
                    bytes.len() as i32,
                    ptr::null_mut(),
                    0,
                )
            };
            if len <= 0 {
                return Err("invalid or empty Bink filename".into());
            }
            let mut wide = vec![0u16; len as usize];
            if unsafe {
                MultiByteToWideChar(
                    CP_ACP,
                    0,
                    bytes.as_ptr(),
                    bytes.len() as i32,
                    wide.as_mut_ptr(),
                    len,
                )
            } != len
            {
                return Err("Bink filename conversion failed".into());
            }
            return Ok(PathBuf::from(std::ffi::OsString::from_wide(&wide)));
        }
        bytes.push(c);
    }
    Err("Bink filename exceeds bound".into())
}
unsafe fn source(pointer: *const u8, flags: u32) -> Result<Session, String> {
    if flags & !(0x0400_0000 | 0x2000) != 0 {
        return Err(format!("unsupported BinkOpen flags 0x{flags:08x}"));
    }
    if flags & 0x0400_0000 != 0 {
        if !unsafe { mapped(pointer, 44, false) } {
            return Err("invalid memory BIK header".into());
        }
        let declared = unsafe { ptr::read_unaligned(pointer.add(4).cast::<u32>()) } as usize;
        let len = declared
            .checked_add(8)
            .filter(|n| (44..=512 * 1024 * 1024).contains(n))
            .ok_or("memory BIK size exceeds bound")?;
        if !unsafe { mapped(pointer, len, false) } {
            return Err("memory BIK spans unreadable pages".into());
        }
        let bytes: Arc<[u8]> = unsafe { std::slice::from_raw_parts(pointer, len) }.into();
        Session::from_bytes(bytes).map_err(|e| e.to_string())
    } else {
        let path = unsafe { name(pointer) }?;
        if flags & 0x2000 != 0 {
            let len = std::fs::metadata(&path).map_err(|e| e.to_string())?.len();
            if len > 512 * 1024 * 1024 {
                return Err("preloaded BIK exceeds bound".into());
            }
            use std::io::Read;
            let mut bytes = Vec::new();
            std::fs::File::open(path)
                .map_err(|e| e.to_string())?
                .take(512 * 1024 * 1024 + 1)
                .read_to_end(&mut bytes)
                .map_err(|e| e.to_string())?;
            if bytes.len() > 512 * 1024 * 1024 {
                return Err("preloaded BIK grew beyond bound".into());
            }
            Session::from_bytes(bytes.into()).map_err(|e| e.to_string())
        } else {
            Session::open(path).map_err(|e| e.to_string())
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_open(pointer: *const u8, flags: u32) -> Handle {
    boundary(ptr::null_mut(), || {
        let session = unsafe { source(pointer, flags) }?;
        let audio: Option<Box<dyn AudioSink>> =
            if let Some(t) = session.header().audio_tracks.first() {
                let cfg = provider().lock().map_err(|_| "Bink provider poisoned")?;
                if cfg.enabled {
                    let bits = if t
                        .flags
                        .contains(infinitier_bik_decoder::AudioFlags::BITS_16)
                    {
                        16
                    } else {
                        8
                    };
                    let pointer = cfg.device.as_ref().map_or(0, Device::pointer);
                    Some(Box::new(unsafe {
                        DirectSoundSink::open(
                            pointer,
                            u32::from(t.sample_rate),
                            t.flags.channels(),
                            bits,
                        )
                    }?))
                } else {
                    None
                }
            } else {
                None
            };
        let mut movie = Box::new(Movie::new(session, flags, audio)?);
        let handle = &mut movie.prefix as *mut Prefix;
        let mut all = movies().lock().map_err(|_| "Bink registry poisoned")?;
        if all.len() >= 256 {
            return Err("too many concurrent Bink handles".into());
        }
        all.insert(handle as usize, movie);
        Ok(handle)
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_close(handle: Handle) {
    boundary((), || {
        let removed = movies()
            .lock()
            .map_err(|_| "Bink registry poisoned")?
            .remove(&(handle as usize));
        // Release/join after the registry lock has been dropped.
        drop(removed);
        Ok(())
    });
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_do_frame(handle: Handle) -> i32 {
    boundary(1, || {
        with_movie(handle, |m| m.do_frame(Instant::now()).map(|_| 0))
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_next_frame(handle: Handle) {
    boundary((), || with_movie(handle, |m| m.next_frame(Instant::now())));
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_wait(handle: Handle) -> i32 {
    boundary(0, || {
        with_movie(handle, |m| m.wait(Instant::now()).map(i32::from))
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_goto(handle: Handle, frame: u32, flags: u32) {
    boundary((), || {
        with_movie(handle, |m| m.goto(frame, flags, Instant::now()))
    });
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_copy(
    handle: Handle,
    destination: *mut u8,
    pitch: i32,
    height: u32,
    x: u32,
    y: u32,
    flags: u32,
) -> i32 {
    boundary(1, || {
        with_movie(handle, |m| {
            if pitch <= 0 {
                return Err("unsupported nonpositive Bink destination pitch".into());
            }
            let len = (pitch as usize)
                .checked_mul(height as usize)
                .filter(|n| *n <= 128 * 1024 * 1024)
                .ok_or("Bink destination extent exceeds bound")?;
            if !unsafe { mapped(destination, len, true) } {
                return Err("unwritable Bink destination".into());
            }
            let image = m.image().ok_or("no decompressed Bink frame")?;
            let target = if len == 0 {
                &mut []
            } else {
                unsafe { std::slice::from_raw_parts_mut(destination, len) }
            };
            pixels::copy_frame(
                image,
                target,
                pitch as usize,
                height as usize,
                x as usize,
                y as usize,
                flags,
            )?;
            Ok(0)
        })
    })
}
// The installed export is decorated @8 but actually consumes THREE words.
// FleetOpsHook's checked callback adds track 0 and cleans the engine's two words.
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_set_volume(handle: Handle, track: u32, volume: i32) {
    boundary((), || {
        if track != 0 {
            return Err("unsupported Bink volume track".into());
        }
        with_movie(handle, |m| m.set_volume(volume.clamp(0, 32768) as u32))
    });
}
unsafe extern "system" fn sound_token(
    _snd: *mut c_void,
    _freq: u32,
    _bits: i32,
    _channels: i32,
    _flags: u32,
    _bink: Handle,
) -> i32 {
    error("external BINKSND callback protocol is unsupported");
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_open_direct_sound(parameter: usize) -> *const c_void {
    boundary(ptr::null(), || {
        let mut cfg = provider().lock().map_err(|_| "Bink provider poisoned")?;
        if parameter == 0 {
            // The engine also registers zero while closing its audio system.
            // Release the borrowed device now; create a default device lazily
            // only if a later movie actually needs audio.
            cfg.device = None;
            cfg.parameter = 0;
        } else if cfg.device.is_none() || cfg.parameter != parameter {
            // DirectSound's supplied pointer remains alive under this owned reference.
            let device = unsafe { Device::acquire(parameter) }?;
            cfg.device = Some(device);
            cfg.parameter = parameter;
        }
        cfg.enabled = true;
        Ok(sound_token as *const c_void)
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_set_sound_system(
    factory: *const c_void,
    parameter: usize,
) -> i32 {
    boundary(0, || {
        if factory.is_null() {
            return Ok(0);
        }
        if factory != a2fo_bink_open_direct_sound as *const c_void {
            return Err("unsupported external Bink sound factory".into());
        }
        Ok(i32::from(
            !unsafe { a2fo_bink_open_direct_sound(parameter) }.is_null(),
        ))
    })
}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_get_error() -> *const u8 {
    ERROR.with(|e| e.borrow().as_ptr().cast())
}

/// The caller supplies a live IDirectDrawSurface-compatible interface.
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_dd_surface_type(object: *mut c_void) -> u32 {
    unsafe { dd_surface_type(object, false) }
}

/// Explicit older-code namespace; never infer the ABI from a surface pointer.
#[unsafe(no_mangle)]
pub unsafe extern "system" fn a2fo_bink_dd_surface_type_legacy(object: *mut c_void) -> u32 {
    unsafe { dd_surface_type(object, true) }
}

unsafe fn dd_surface_type(object: *mut c_void, legacy: bool) -> u32 {
    boundary(u32::MAX, || {
        if object.is_null() {
            return Ok(u32::MAX);
        }
        if !unsafe { mapped(object.cast(), size_of::<usize>(), false) } {
            return Err("unreadable DirectDraw surface interface".into());
        }
        let table = unsafe { ptr::read_unaligned(object.cast::<*const usize>()) };
        // IDirectDrawSurface::GetPixelFormat is slot 21 for the observed
        // interfaces. The object and vtable must stay alive during this call.
        if !unsafe { mapped(table.cast(), 22 * size_of::<usize>(), false) } {
            return Err("unreadable DirectDraw surface vtable".into());
        }
        let entry = unsafe { ptr::read_unaligned(table.add(21)) };
        if entry == 0 || !unsafe { mapped(entry as *const u8, 1, false) } {
            return Err("invalid DirectDraw GetPixelFormat method".into());
        }
        let get: unsafe extern "system" fn(*mut c_void, *mut surface::PixelFormat) -> i32 =
            unsafe { std::mem::transmute(entry) };
        let mut format = surface::PixelFormat {
            size: size_of::<surface::PixelFormat>() as u32,
            ..Default::default()
        };
        // Both observed Bink versions classify the initialized output without
        // consulting HRESULT. Preserve that behavior, including failed queries
        // which leave the zero-initialized fields unchanged.
        unsafe { get(object, &mut format) };
        Ok(if legacy {
            surface::classify_legacy(&format)
        } else {
            surface::classify(&format)
        })
    })
}

/// Called explicitly outside loader lock before a test/runtime unload.
pub fn shutdown() -> Result<(), String> {
    let all = std::mem::take(&mut *movies().lock().map_err(|_| "Bink registry poisoned")?);
    drop(all);
    let mut cfg = provider().lock().map_err(|_| "Bink provider poisoned")?;
    cfg.device = None;
    cfg.enabled = false;
    Ok(())
}
pub fn link_anchor() -> usize {
    let entries = [
        a2fo_bink_open as *const () as usize,
        a2fo_bink_close as *const () as usize,
        a2fo_bink_do_frame as *const () as usize,
        a2fo_bink_next_frame as *const () as usize,
        a2fo_bink_wait as *const () as usize,
        a2fo_bink_copy as *const () as usize,
        a2fo_bink_goto as *const () as usize,
        a2fo_bink_set_volume as *const () as usize,
        a2fo_bink_set_sound_system as *const () as usize,
        a2fo_bink_open_direct_sound as *const () as usize,
        a2fo_bink_get_error as *const () as usize,
        a2fo_bink_dd_surface_type as *const () as usize,
        a2fo_bink_dd_surface_type_legacy as *const () as usize,
    ];
    std::hint::black_box(entries)[0]
}
