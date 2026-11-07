use alloc::format;
use core::{ffi::c_void, slice};
#[repr(C)]
#[derive(Clone, Copy)]
struct Text {
    data: *const u8,
    size: usize,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct Entry {
    name: Text,
    filename: Text,
    path: Text,
    handle: usize,
    shutdown: usize,
    state: u32,
    installed: u32,
}
impl Default for Entry {
    fn default() -> Self {
        unsafe { core::mem::zeroed() }
    }
}
#[repr(C)]
pub struct Api {
    context: *mut c_void,
    get: unsafe extern "C" fn(*mut c_void, u32, usize, *mut Entry) -> usize,
    act: unsafe extern "C" fn(*mut c_void, u32, usize, usize, usize, *const u8, usize) -> usize,
}
impl Api {
    fn count(&self, k: u32) -> usize {
        unsafe { (self.get)(self.context, k, 0, core::ptr::null_mut()) }
    }
    fn entry(&self, k: u32, i: usize) -> Entry {
        let mut e = Entry::default();
        unsafe { (self.get)(self.context, k, i, &mut e) };
        e
    }
    fn call(&self, op: u32, i: usize, a: usize, b: usize, s: &[u8]) -> Result<usize, ()> {
        let v = unsafe { (self.act)(self.context, op, i, a, b, s.as_ptr(), s.len()) };
        if unsafe { (self.act)(self.context, 0, 0, 0, 0, core::ptr::null(), 0) } != 0 {
            Err(())
        } else {
            Ok(v)
        }
    }
    fn log(&self, s: &[u8]) -> Result<(), ()> {
        self.call(3, 0, 0, 0, s).map(|_| ())
    }
    fn message(&self, prefix: &[u8], s: Text) -> Result<(), ()> {
        let mut t = prefix.to_vec();
        t.extend_from_slice(bytes(&s));
        self.log(&t)
    }
}
fn bytes(t: &Text) -> &[u8] {
    if t.size == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(t.data, t.size) }
    }
}
fn run(a: &Api) -> Result<bool, ()> {
    let roots = a.count(0);
    if roots == 0 {
        a.log(b"Module loader: no extension roots available")?;
        return Ok(true);
    }
    let installed = a.call(1, 0, 0, 0, &[])?;
    for i in 0..a.count(1) {
        a.message(b"Module loader: ", a.entry(1, i).path)?
    }
    for i in 1..roots {
        let mut path = bytes(&a.entry(0, i).path).to_vec();
        if !path.is_empty() && !matches!(path.last(), Some(b'\\' | b'/')) {
            path.push(b'\\')
        }
        path.extend_from_slice(b"modules");
        if a.call(4, 0, 0, 0, &path)? != 0 {
            let mut line = b"Module loader: ignored mod-local module directory: ".to_vec();
            line.extend(path);
            a.log(&line)?
        }
    }
    if installed == 0 {
        a.log(b"Module loader: no native modules found")?;
        return Ok(true);
    }
    a.call(2, 0, 0, 0, &[])?;
    for i in 0..a.count(2) {
        a.message(b"Module policy: ", a.entry(2, i).path)?
    }
    let policy = a.entry(3, 0);
    a.log(if policy.installed != 0 {
        b"Module policy: managed selection enabled"
    } else {
        b"Module policy: no [modules] section; legacy load-all mode"
    })?;
    let mut ok = policy.state != 0;
    a.call(5, 0, 0, 0, &[])?;
    for i in 0..a.count(4) {
        let e = a.entry(4, i);
        let wanted = matches!(e.state, 1 | 2);
        let builtin = a.call(6, i, 0, 0, &[])?;
        if builtin != 0 {
            if matches!(e.state, 3 | 4) {
                continue;
            }
            let feature = builtin - 1;
            a.call(7, i, 0, 0, &[])?;
            let result = a.call(9, i, feature, wanted as usize, &[])?;
            let initialized = result == 1;
            if result == 2 {
                a.message(b"Built-in module threw during preparation: ", e.name)?
            }
            a.call(8, i, initialized as usize, 0, &[])?;
            if initialized && a.call(10, i, feature, 0, &[])? != 0 && wanted {
                a.call(11, i, feature, 0, &[])?;
            }
            a.message(
                if initialized {
                    b"Built-in module prepared: "
                } else {
                    b"Built-in module unavailable: "
                },
                e.name,
            )?;
            if !initialized && e.state == 2 {
                ok = false
            }
            continue;
        }
        if !wanted {
            if e.installed != 0 {
                a.message(b"Module loader: skipped ", e.filename)?
            }
            continue;
        }
        let handle = a.call(12, i, 0, 0, &[])?;
        if handle == 0 {
            let error = a.call(13, 0, 0, 0, &[])?;
            let prefix = format!("Module loader: LoadLibrary failed (error {error}): ");
            a.message(prefix.as_bytes(), e.path)?;
            ok = false;
            continue;
        }
        let init = a.call(14, i, handle, 0, &[])?;
        if init == 0 {
            a.message(b"Module loader: missing A2FO_ModuleInit: ", e.path)?;
            a.call(16, i, handle, 0, &[])?;
            ok = false;
            continue;
        }
        a.call(7, i, 0, 0, &[])?;
        let result = a.call(15, i, init, 0, &[])?;
        let initialized = result == 1;
        if result == 2 {
            a.message(
                b"Module loader: module threw during initialization: ",
                e.path,
            )?
        }
        a.call(8, i, initialized as usize, 0, &[])?;
        if !initialized {
            a.message(b"Module loader: module rejected initialization: ", e.path)?;
            a.call(16, i, handle, 0, &[])?;
            ok = false;
            continue;
        }
        a.call(17, i, handle, 0, &[])?;
        a.message(b"Module loader: loaded ", e.path)?;
    }
    Ok(ok)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_loader_run(a: *const Api) -> u8 {
    run(unsafe { &*a }).unwrap_or(false) as u8
}
/// # Safety
/// The native adapter exclusively owns the detached retiring generation for
/// this entire call. Shutdown/log callbacks cannot mutate its records or paths.
/// Native exceptions stay in the adapter and are rethrown after all cleanup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_loader_unload(a: *const Api) {
    let a = unsafe { &*a };
    for i in (0..a.count(5)).rev() {
        let e = a.entry(5, i);
        if e.shutdown != 0 {
            // Cleanup must continue after an earlier native logging failure;
            // unlike call(), the raw result still distinguishes shutdown throws.
            let threw = unsafe { (a.act)(a.context, 21, i, 0, 0, core::ptr::null(), 0) } != 0;
            if threw {
                let prefix = b"Module loader: module threw during shutdown: ";
                let mut message = alloc::vec::Vec::new();
                match e.path.size.checked_add(prefix.len()) {
                    Some(capacity) if message.try_reserve_exact(capacity).is_ok() => {
                        message.extend_from_slice(prefix);
                        message.extend_from_slice(bytes(&e.path));
                        let _ = a.log(&message);
                    }
                    _ => {
                        let _ = a.call(23, 0, 0, 0, &[]);
                    }
                }
            }
        }
        if e.handle != 0 {
            let _ = a.call(16, i, e.handle, 0, &[]);
        }
    }
    let _ = a.call(22, 0, 0, 0, &[]);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_loader_producer(a: *const Api) -> u8 {
    let a = unsafe { &*a };
    for i in 0..a.count(5) {
        if bytes(&a.entry(5, i).path) == b"builtin:A2FOFeaturePack" {
            return a.call(19, 0, 0, 0, &[]).unwrap_or(0) as u8;
        }
    }
    for i in 0..a.count(5) {
        let e = a.entry(5, i);
        let path = bytes(&e.path);
        let name = path
            .rsplit(|c| matches!(c, b'/' | b'\\'))
            .next()
            .unwrap_or(&[]);
        let end = name.iter().position(|&x| x == 0).unwrap_or(name.len());
        if e.handle != 0 && name[..end].eq_ignore_ascii_case(b"A2FOFeaturePack.dll") {
            return a.call(20, i, e.handle, 0, &[]).unwrap_or(0) as u8;
        }
    }
    0
}

#[cfg(test)]
mod ownership_tests {
    use super::*;
    use alloc::vec::Vec;

    struct Mock {
        count: usize,
        shutdowns: Vec<usize>,
        freed: Vec<usize>,
        logs: Vec<Vec<u8>>,
        log_throws: bool,
        failed: bool,
        clears: usize,
        unexpected: bool,
    }
    unsafe extern "C" fn get(
        pointer: *mut c_void,
        kind: u32,
        index: usize,
        output: *mut Entry,
    ) -> usize {
        let mock = unsafe { &mut *pointer.cast::<Mock>() };
        if kind != 5 || (!output.is_null() && index >= mock.count) {
            mock.unexpected = true;
            return 0;
        }
        if !output.is_null() {
            let names: [&[u8]; 3] = [b"first", b"middle", b"last"];
            let name = names[index];
            unsafe {
                output.write(Entry {
                    path: Text {
                        data: name.as_ptr(),
                        size: name.len(),
                    },
                    handle: 0x100 + index,
                    shutdown: 1,
                    ..Entry::default()
                })
            };
        }
        mock.count
    }
    unsafe extern "C" fn act(
        pointer: *mut c_void,
        operation: u32,
        index: usize,
        value: usize,
        _second: usize,
        bytes: *const u8,
        size: usize,
    ) -> usize {
        let mock = unsafe { &mut *pointer.cast::<Mock>() };
        match operation {
            0 => usize::from(mock.failed),
            3 => {
                mock.logs
                    .push(unsafe { slice::from_raw_parts(bytes, size) }.to_vec());
                if mock.log_throws {
                    mock.failed = true;
                }
                0
            }
            16 => {
                mock.freed.push(value);
                0
            }
            21 => {
                mock.shutdowns.push(index);
                usize::from(index == 1)
            }
            22 => {
                mock.count = 0;
                mock.clears += 1;
                0
            }
            _ => {
                mock.unexpected = true;
                0
            }
        }
    }
    fn run_cleanup(log_throws: bool) {
        let mut mock = Mock {
            count: 3,
            shutdowns: Vec::new(),
            freed: Vec::new(),
            logs: Vec::new(),
            log_throws,
            failed: false,
            clears: 0,
            unexpected: false,
        };
        let api = Api {
            context: (&raw mut mock).cast(),
            get,
            act,
        };
        unsafe { a2fo_rs_loader_unload(&api) };
        assert!(!mock.unexpected);
        assert_eq!(mock.shutdowns, [2, 1, 0]);
        assert_eq!(mock.freed, [0x102, 0x101, 0x100]);
        assert_eq!(mock.count, 0);
        assert_eq!(mock.clears, 1);
        assert_eq!(mock.failed, log_throws);
        assert_eq!(
            mock.logs,
            [b"Module loader: module threw during shutdown: middle".to_vec()]
        );
    }
    #[test]
    fn retiring_shutdowns_and_handles_release_in_reverse_order() {
        run_cleanup(false);
    }
    #[test]
    fn native_log_failure_does_not_interrupt_retiring_cleanup() {
        run_cleanup(true);
    }
}
