use alloc::format;
use core::{ffi::c_void, ptr};
#[repr(C)]
#[derive(Default)]
struct Data {
    words: [usize; 12],
    ints: [i32; 16],
    pixels: *mut u8,
    count: usize,
}
#[repr(C)]
pub struct Ops {
    context: *mut c_void,
    read: unsafe extern "C" fn(*mut c_void, usize, *mut c_void, usize) -> u8,
    callable: unsafe extern "C" fn(*mut c_void, usize) -> u8,
    action: unsafe extern "C" fn(*mut c_void, u32, *mut Data, *const u8, usize) -> usize,
}
impl Ops {
    fn call(&self, op: u32, d: &mut Data, s: &[u8]) -> usize {
        unsafe { (self.action)(self.context, op, d, s.as_ptr(), s.len()) }
    }
    fn op(&self, op: u32) -> usize {
        self.call(op, &mut Data::default(), &[])
    }
    fn note(&self, s: &[u8]) {
        self.call(5, &mut Data::default(), s);
    }
    fn read(&self, p: usize, out: &mut [u8]) -> bool {
        unsafe { (self.read)(self.context, p, out.as_mut_ptr().cast(), out.len()) != 0 }
    }
    fn word(&self, p: usize, offset: usize) -> Option<usize> {
        if p == 0 {
            return None;
        }
        let at = p.checked_add(offset)?;
        let mut v = [0u8; 4];
        self.read(at, &mut v)
            .then(|| u32::from_le_bytes(v) as usize)
    }
    fn matches(&self, p: usize, wanted: &[u8]) -> bool {
        let mut actual = alloc::vec![0;wanted.len()];
        self.read(p, &mut actual) && actual == wanted
    }
    fn callable(&self, p: usize) -> bool {
        unsafe { (self.callable)(self.context, p) != 0 }
    }
}
const CHAT: usize = 0x11d3c0;
const PROLOGUE: &[u8] = &[0x55, 0x8b, 0xec, 0x83, 0xec, 0x18];
fn address(base: usize, rva: usize) -> usize {
    base.wrapping_add(rva)
}
fn supported(a: &Ops, base: usize, fo: usize) -> bool {
    if a.matches(address(base, CHAT), PROLOGUE) {
        return true;
    }
    if fo == 0 {
        return false;
    }
    let callback = address(fo, 0x1cf0b4);
    let mut transfer = [0x68, 0, 0, 0, 0, 0xc3];
    transfer[1..5].copy_from_slice(&(callback as u32).to_le_bytes());
    if !a.matches(address(base, CHAT), &transfer) {
        return false;
    }
    let counter = address(fo, 0x20fff4);
    let next = address(fo, 0x2469d4);
    let mut expected = [
        0x83, 0x3d, 0, 0, 0, 0, 0, 0x75, 0x06, 0xff, 0x15, 0, 0, 0, 0, 0xc3,
    ];
    expected[2..6].copy_from_slice(&(counter as u32).to_le_bytes());
    expected[11..15].copy_from_slice(&(next as u32).to_le_bytes());
    if !a.callable(callback)
        || !a.callable(callback.wrapping_add(15))
        || !a.matches(callback, &expected)
    {
        return false;
    }
    let Some(gateway) = a.word(next, 0) else {
        return false;
    };
    if !a.callable(gateway) || !a.callable(gateway.wrapping_add(11)) {
        return false;
    }
    let mut wanted = [0u8; 12];
    wanted[..6].copy_from_slice(PROLOGUE);
    wanted[6] = 0x68;
    wanted[11] = 0xc3;
    wanted[7..11].copy_from_slice(&(address(base, CHAT + 6) as u32).to_le_bytes());
    a.matches(gateway, &wanted)
}
fn reject(a: &Ops, base: usize, label: &str, rva: usize) -> u8 {
    let mut bytes = [0u8; 6];
    a.read(address(base, rva), &mut bytes);
    let msg = format!(
        "Mission backdrop refused: {label} at Armada RVA 0x{rva:08x} ({:02x} {:02x} {:02x} {:02x} {:02x} {:02x})",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]
    );
    a.note(msg.as_bytes());
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_backdrop_init(
    a: *const Ops,
    base: usize,
    fo: usize,
    valid: u8,
) -> u8 {
    let a = unsafe { &*a };
    if valid == 0 || base == 0 {
        return 0;
    }
    if a.op(1) != 0 {
        return 1;
    }
    a.op(0);
    let checks: [(&str, usize, &[u8]); 3] = [
        (
            "mission chat receiver",
            0x198a0c,
            &[0x8b, 0x0d, 0xe8, 0x8d, 0x76, 0, 0x85, 0xc9, 0x74, 5],
        ),
        (
            "mission creation",
            0x8587a,
            &[0x89, 0x35, 0xe8, 0x8d, 0x76, 0],
        ),
        (
            "mission deletion",
            0x11d82b,
            &[0xc7, 5, 0xe8, 0x8d, 0x76, 0, 0, 0, 0, 0],
        ),
    ];
    for (label, rva, code) in checks {
        if !a.matches(address(base, rva), code) {
            return reject(a, base, label, rva);
        }
    }
    if !supported(a, base, fo) {
        return reject(a, base, "unknown chat entry/FO hook chain", CHAT);
    }
    if !a.matches(
        address(base, 0x198f36),
        &[
            0xa1, 0x08, 0xd5, 0x7a, 0x00, 0x8b, 0x88, 0xc0, 0x00, 0x00, 0x00, 0x8b, 0x8c, 0x88,
            0xcc, 0x00, 0x00, 0x00, 0x8b, 0x11, 0xff, 0x52, 0x38,
        ],
    ) {
        return reject(a, base, "active device selector", 0x198f36);
    }
    if !a.matches(
        address(base, 0x226440),
        &[
            0x56, 0x8b, 0xf1, 0xe8, 0x68, 0xb0, 0xff, 0xff, 0x8d, 0x8e, 0xe4, 0, 0, 0, 0xe8, 0x3d,
            0xb6, 0xff, 0xff, 0x5e, 0xc3,
        ],
    ) {
        return reject(a, base, "device flush", 0x226440);
    }
    let mut d = Data::default();
    d.words[0] = address(base, 0x198a16);
    if a.call(6, &mut d, &[0xe8, 0xa5, 0x49, 0xf8, 0xff]) == 0 {
        return reject(a, base, "mission CALL install", 0x198a16);
    }
    a.op(3);
    a.note(if a.matches(address(base,CHAT),PROLOGUE){b"Mission menu backdrop armed: original chat entry; game frame retained over paused/nested menus"}else{b"Mission menu backdrop armed: verified Fleet Ops Cleanshot chain preserved; game frame retained over paused/nested menus"});
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_backdrop_observe(a: *const Ops, chat: usize) {
    let a = unsafe { &*a };
    if a.op(2) != 0 {
        a.op(7);
        let mut d = Data::default();
        d.words[0] = a.op(8);
        a.call(9, &mut d, &[]);
        d.words[0] = chat;
        a.call(10, &mut d, &[]);
        if a.op(11) == 0 {
            a.note(b"Mission backdrop observer received its first gameplay frame")
        }
    }
}
fn active(a: &Ops, base: usize) -> bool {
    a.op(2) != 0
        && a.op(12) != 0
        && a.word(address(base, 0x368de8), 0)
            .is_some_and(|chat| chat != 0 && chat == a.op(13))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_backdrop_active(a: *const Ops, base: usize) -> u8 {
    active(unsafe { &*a }, base) as u8
}
fn capture(a: &Ops, base: usize, d: &mut Data) -> bool {
    let work = || -> Option<(usize, usize)> {
        let storm = a.word(address(base, 0x3ad508), 0)?;
        let index = a.word(storm, 0xc0)?;
        if index > 3 {
            return None;
        }
        let wrapper = a.word(storm, 0xcc + index * 4)?;
        let table = a.word(wrapper, 0)?;
        let flush = a.word(table, 0x38)?;
        if flush != address(base, 0x226440) {
            return None;
        }
        let unknown = a.word(wrapper, 0x90)?;
        if unknown == 0 {
            return None;
        }
        let table = a.word(unknown, 0)?;
        let query = a.word(table, 0)?;
        a.callable(query).then_some((unknown, query))
    };
    let Some((unknown, query)) = work() else {
        return false;
    };
    d.words[0] = unknown;
    d.words[1] = query;
    if a.call(22, d, &[]) == 0 || d.words[2] == 0 {
        return false;
    }
    let ok = capture_device(a, d);
    a.call(37, d, &[]);
    ok
}
fn capture_device(a: &Ops, d: &mut Data) -> bool {
    if a.call(23, d, &[]) == 0 || a.call(24, d, &[]) == 0 || a.call(25, d, &[]) == 0 {
        return false;
    }
    let (w, h) = (d.words[3], d.words[4]);
    if w == 0 || h == 0 || w > 8192 || h > 8192 || w as u64 * h as u64 > 16000000 {
        return false;
    }
    if a.call(26, d, &[]) == 0 || d.words[7] == 0 {
        return false;
    }
    let ok = capture_surface(a, d);
    a.call(36, d, &[]);
    ok
}
fn capture_surface(a: &Ops, d: &mut Data) -> bool {
    if a.call(27, d, &[]) == 0
        || d.words[5] == 0
        || a.call(28, d, &[]) == 0
        || a.call(29, d, &[]) == 0
    {
        return false;
    }
    if a.call(30, d, &[]) != 0 && d.words[8] != 0 {
        a.call(31, d, &[]);
        a.call(32, d, &[]);
    }
    if d.words[9] != 0 && a.call(33, d, &[]) != 0 {
        d.ints[2] = d.ints[2].wrapping_sub(d.ints[4]);
        d.ints[3] = d.ints[3].wrapping_sub(d.ints[5]);
    }
    let (w, h, x, y) = (d.ints[0], d.ints[1], d.ints[2], d.ints[3]);
    if w <= 0
        || h <= 0
        || w as u64 * h as u64 > 8000000
        || x < 0
        || y < 0
        || x as u64 + w as u64 > d.words[3] as u64
        || y as u64 + h as u64 > d.words[4] as u64
    {
        return false;
    }
    if a.call(34, d, &[]) == 0 {
        return false;
    }
    let mut ok = !d.pixels.is_null() && d.ints[6] >= d.words[3] as i32 * 4;
    if ok {
        d.count = w as usize * h as usize;
        ok = a.call(38, d, &[]) != 0;
        if ok {
            for row in 0..h as usize {
                let offset = (row + y as usize) * d.ints[6] as usize + x as usize * 4;
                unsafe {
                    ptr::copy_nonoverlapping(
                        d.pixels.add(offset),
                        (d.words[10] as *mut u8).add(row * w as usize * 4),
                        w as usize * 4,
                    )
                }
            }
        }
    }
    a.call(35, d, &[]);
    ok
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_backdrop_frame(a: *const Ops, base: usize) -> u8 {
    let a = unsafe { &*a };
    if a.op(8) != a.op(14) {
        return 0;
    }
    if !active(a, base) {
        a.op(18);
        return 0;
    }
    let current = a.op(12);
    let chat = a.op(13);
    let mut d = Data::default();
    a.call(16, &mut d, &[]);
    if a.op(15) != 0 || d.words[0] != current || d.words[1] != chat {
        d.words[0] = current;
        d.words[1] = chat;
        a.call(17, &mut d, &[]);
        d = Data::default();
        if capture(a, base, &mut d) {
            a.call(39, &mut d, &[]);
            a.note(b"Menu backdrop: last gameplay frame captured; fleet scene paused")
        } else {
            a.op(19);
            if a.op(20) == 0 {
                a.note(b"Gameplay backdrop unavailable; neutral menu backdrop retained instead of front-end fleet");
                a.op(21);
            }
        }
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_backdrop_reset(a: *const Ops) {
    unsafe { &*a }.op(18);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_backdrop_shutdown(a: *const Ops) {
    unsafe { &*a }.op(4);
}
