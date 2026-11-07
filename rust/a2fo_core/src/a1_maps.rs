//! Armada 1 map front matter, object tail, and MDF policy.
//! The private ABI uses byte spans and value-only structures.
use core::{ptr, slice};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Header {
    version: u32,
    metadata_offset: usize,
    mission_name: [u8; 64],
    has_map_bounds: u32,
    minimum_extent: [f32; 3],
    map_size: [f32; 3],
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Tail {
    first_object_offset: usize,
    mission_offset: usize,
    object_count: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Mdf {
    start_location_count: usize,
    locations: [[u32; 2]; 8],
}
fn u32_at(data: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_le_bytes(
        data.get(at..at.checked_add(4)?)?.try_into().ok()?,
    ))
}
fn field<'a>(data: &'a [u8], cursor: &mut usize, kind: u8, len: usize) -> Option<&'a [u8]> {
    let start = *cursor;
    if *data.get(start)? != kind
        || data.len().checked_sub(start)? < 8
        || u32_at(data, start + 4)? as usize != len
    {
        return None;
    }
    let value = data.get(start + 8..start.checked_add(8 + len)?)?;
    *cursor = start + 8 + len;
    Some(value)
}
fn mission_name(data: &[u8], at: usize, h: &mut Header) {
    if data.get(at) != Some(&2) || data.len().saturating_sub(at) < 8 {
        return;
    }
    let Some(n) = u32_at(data, at + 4).map(|v| v as usize) else {
        return;
    };
    if n == 0 || n > 256 {
        return;
    }
    let Some(value) = data.get(at + 8..at + 8 + n) else {
        return;
    };
    let mut len = 0;
    while len < n && value[len] != 0 {
        let c = value[len];
        if len >= 63 || !(c.is_ascii_alphanumeric() || b"_-.\\/".contains(&c)) {
            return;
        }
        len += 1;
    }
    if len == 0
        || len == n
        || len < 4
        || value[len - 4] != b'.'
        || !value[len - 3..len].eq_ignore_ascii_case(b"bzn")
    {
        return;
    }
    h.mission_name[..len].copy_from_slice(&value[..len]);
    h.mission_name[len] = 0;
}
fn map_bounds(data: &[u8], at: usize, h: &mut Header) {
    if data.get(at) != Some(&2) || data.len().saturating_sub(at) < 8 {
        return;
    }
    let Some(n) = u32_at(data, at + 4).map(|v| v as usize) else {
        return;
    };
    if n == 0 || n > 256 || data.len().saturating_sub(at + 8) < n {
        return;
    }
    let mut cursor = at + 8 + n;
    if field(data, &mut cursor, 1, 1).is_none() || field(data, &mut cursor, 2, 100).is_none() {
        return;
    }
    let mut bounds = [0.0_f32; 6];
    for b in &mut bounds {
        let Some(bytes) = field(data, &mut cursor, 5, 4) else {
            return;
        };
        *b = f32::from_bits(u32::from_le_bytes(bytes.try_into().unwrap()));
        if !b.is_finite() {
            return;
        }
    }
    let sizes = [
        bounds[1] - bounds[0],
        bounds[3] - bounds[2],
        bounds[5] - bounds[4],
    ];
    if sizes
        .iter()
        .any(|&n| n.is_nan() || n <= 0.0 || n > 1_000_000.0)
    {
        return;
    }
    h.minimum_extent = [bounds[0], bounds[2], bounds[4]];
    h.map_size = sizes;
    h.has_map_bounds = 1;
}
struct Cursor<'a> {
    data: &'a [u8],
    at: usize,
}
impl Cursor<'_> {
    fn consume(&mut self, value: &[u8]) -> bool {
        if self.data.get(self.at..self.at.saturating_add(value.len())) != Some(value) {
            return false;
        }
        self.at += value.len();
        true
    }
    fn newline(&mut self) -> bool {
        if self.consume(b"\n") {
            true
        } else {
            self.consume(b"\r\n")
        }
    }
    fn decimal(&mut self) -> Option<u32> {
        let mut n = 0_u32;
        let start = self.at;
        while let Some(c @ b'0'..=b'9') = self.data.get(self.at).copied() {
            n = n.checked_mul(10)?.checked_add(u32::from(c - b'0'))?;
            self.at += 1;
        }
        (self.at != start).then_some(n)
    }
    fn hex(&mut self, len: usize) -> bool {
        let Some(bytes) = self.data.get(self.at..self.at.saturating_add(len)) else {
            return false;
        };
        if !bytes.iter().all(u8::is_ascii_hexdigit) {
            return false;
        }
        self.at += len;
        true
    }
}
fn parse_header(data: &[u8], h: Option<&mut Header>) -> bool {
    let mut c = Cursor { data, at: 0 };
    if !c.consume(b"version [1] =") || !c.newline() {
        return false;
    }
    let Some(version) = c.decimal() else {
        return false;
    };
    if !c.newline()
        || !(2050..=2053).contains(&version)
        || !c.consume(b"saveGameDesc = ")
        || !c.hex(256)
        || !c.newline()
        || !c.consume(b"binarySave [1] =")
        || !c.newline()
        || !c.consume(b"true")
        || !c.newline()
    {
        return false;
    }
    if let Some(h) = h {
        h.version = version;
        h.metadata_offset = c.at;
        mission_name(data, c.at, h);
        map_bounds(data, c.at, h);
    }
    true
}
fn mission_at(data: &[u8]) -> bool {
    if data.len() < 48 {
        return false;
    }
    let kind = u32_at(data, 0).unwrap();
    let name = &data[8..48];
    kind & 0xff == 2
        && u32_at(data, 4) == Some(40)
        && [b"EmptyMission".as_slice(), b"Inst4XMission".as_slice()]
            .iter()
            .any(|expected| name.starts_with(expected) && name[expected.len()] == 0)
}
fn object_at(data: &[u8]) -> bool {
    if data.len() < 64 || u32_at(data, 0).unwrap() & 0xff != 7 {
        return false;
    }
    let n = u32_at(data, 4).unwrap() as usize;
    if n == 0 || n > 64 || data.len() < 8 + n {
        return false;
    }
    let mut saw_char = false;
    let mut saw_zero = false;
    for &c in &data[8..8 + n] {
        if c == 0 {
            saw_zero = true;
            continue;
        }
        if saw_zero || !(0x20..=0x7e).contains(&c) {
            return false;
        }
        saw_char = true;
    }
    if !saw_char {
        return false;
    }
    let mut at = 8 + n;
    for (kind, len, advance) in [(4, 4, 12), (9, 12, 20), (4, 4, 12), (2, 40, 0)] {
        if data.len().saturating_sub(at) < 8
            || u32_at(data, at).unwrap() & 0xff != kind
            || u32_at(data, at + 4) != Some(len)
        {
            return false;
        }
        at += advance;
    }
    true
}
fn locate_tail(data: &[u8]) -> Option<Tail> {
    let mut located = Tail::default();
    let mut markers = 0;
    for at in 0..=data.len().checked_sub(48)? {
        if mission_at(&data[at..]) {
            markers += 1;
            located.mission_offset = at;
            if markers > 1 {
                return None;
            }
        }
    }
    if markers != 1 || located.mission_offset == 0 {
        return None;
    }
    let mut found = false;
    for at in 0..located.mission_offset {
        if object_at(&data[at..]) {
            if !found {
                located.first_object_offset = at;
                found = true;
            }
            located.object_count += 1;
            if located.object_count > 1024 {
                return None;
            }
        }
    }
    (found && located.first_object_offset <= 65536).then_some(located)
}
fn trim(mut s: &[u8]) -> &[u8] {
    while matches!(s.first(), Some(b' ' | b'\t' | b'\r' | b'\n')) {
        s = &s[1..];
    }
    while matches!(s.last(), Some(b' ' | b'\t' | b'\r' | b'\n')) {
        s = &s[..s.len() - 1];
    }
    s
}
fn decimal_token(value: &mut &[u8]) -> Option<u32> {
    *value = trim(value);
    let mut n = 0_u32;
    let mut digits = 0;
    for &c in *value {
        if !c.is_ascii_digit() {
            break;
        }
        n = n.checked_mul(10)?.checked_add(u32::from(c - b'0'))?;
        digits += 1;
    }
    if digits == 0 {
        return None;
    }
    *value = &value[digits..];
    Some(n)
}
fn assignment(line: &[u8]) -> Option<(&[u8], &[u8])> {
    let line = trim(line);
    if line.is_empty() || matches!(line[0], b'#' | b';') || line.starts_with(b"//") {
        return None;
    }
    let at = line.iter().position(|&c| c == b'=')?;
    let (key, value) = (trim(&line[..at]), trim(&line[at + 1..]));
    (!key.is_empty() && !value.is_empty()).then_some((key, value))
}
fn parse_mdf(data: &[u8]) -> Option<Mdf> {
    let mut m = Mdf::default();
    let mut found = [false; 8];
    let mut found_count = false;
    let mut at = 0;
    while at < data.len() {
        let start = at;
        while at < data.len() && data[at] != b'\r' && data[at] != b'\n' {
            let c = data[at];
            if (c < 0x20 && c != b'\t') || c > 0x7e {
                return None;
            }
            at += 1;
        }
        let line = &data[start..at];
        if data.get(at) == Some(&b'\r') {
            at += 1;
        }
        if data.get(at) == Some(&b'\n') {
            at += 1;
        }
        let Some((key, mut value)) = assignment(line) else {
            continue;
        };
        if key == b"StartLocations" {
            let count = decimal_token(&mut value)?;
            if found_count || !trim(value).is_empty() || count > 8 {
                return None;
            }
            m.start_location_count = count as usize;
            found_count = true;
            continue;
        }
        if key.len() <= 5 || !key.starts_with(b"Start") {
            continue;
        }
        let mut index_text = &key[5..];
        let index = decimal_token(&mut index_text)?;
        if !trim(index_text).is_empty() || index == 0 || index > 8 {
            return None;
        }
        let (x, y) = (decimal_token(&mut value)?, decimal_token(&mut value)?);
        if found[index as usize - 1] || !trim(value).is_empty() || x > 117 || y > 117 {
            return None;
        }
        m.locations[index as usize - 1] = [x, y];
        found[index as usize - 1] = true;
    }
    if !found_count || found[..m.start_location_count].iter().any(|&v| !v) {
        return None;
    }
    Some(m)
}
// The caller guarantees a live span with length <= isize::MAX.
unsafe fn bytes<'a>(p: *const u8, n: usize) -> Option<&'a [u8]> {
    if p.is_null() || n == 0 || n > isize::MAX as usize {
        None
    } else {
        Some(unsafe { slice::from_raw_parts(p, n) })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_mission_record_at(p: *const u8, n: usize) -> u8 {
    u8::from(unsafe { bytes(p, n) }.is_some_and(mission_at))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_parse_header(p: *const u8, n: usize, out: *mut Header) -> u8 {
    let Some(data) = (unsafe { bytes(p, n) }) else {
        return 0;
    };
    let mut h = if out.is_null() {
        None
    } else {
        Some(unsafe { out.read() })
    };
    let success = parse_header(data, h.as_mut());
    if success && let Some(h) = h {
        unsafe { out.write(h) };
    }
    u8::from(success)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_compatible_bounds(
    h: *const Header,
    min: *mut f32,
    size: *mut f32,
) -> u8 {
    if h.is_null() || min.is_null() || size.is_null() {
        return 0;
    }
    let h = unsafe { h.read() };
    if h.has_map_bounds == 0 {
        return 0;
    }
    let mut minima = h.minimum_extent;
    let mut sizes = h.map_size;
    let source_maximum_y = legacy_add(h.minimum_extent[1], h.map_size[1]);
    minima[1] = if -1250.0 < h.minimum_extent[1] {
        -1250.0
    } else {
        h.minimum_extent[1]
    };
    let maximum_y = if source_maximum_y < 1250.0 {
        1250.0
    } else {
        source_maximum_y
    };
    minima[1] = legacy_load(minima[1]);
    sizes[1] = legacy_sub(maximum_y, minima[1]);
    unsafe {
        ptr::copy_nonoverlapping(minima.as_ptr(), min, 3);
        ptr::copy_nonoverlapping(sizes.as_ptr(), size, 3);
    }
    u8::from(sizes[1].is_finite() && sizes[1] > 0.0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_locate_tail(p: *const u8, n: usize, out: *mut Tail) -> u8 {
    if out.is_null() {
        return 0;
    }
    let Some(data) = (unsafe { bytes(p, n) }) else {
        return 0;
    };
    let Some(t) = locate_tail(data) else {
        return 0;
    };
    unsafe { out.write(t) };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_parse_mdf(p: *const u8, n: usize, out: *mut Mdf) -> u8 {
    if out.is_null() {
        return 0;
    }
    let Some(data) = (unsafe { bytes(p, n) }) else {
        return 0;
    };
    let Some(m) = parse_mdf(data) else {
        return 0;
    };
    unsafe { out.write(m) };
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_a1_world_position(
    h: *const Header,
    x: u32,
    y: u32,
    out: *mut f32,
) -> u8 {
    if h.is_null() || out.is_null() || x > 117 || y > 117 {
        return 0;
    }
    let h = unsafe { h.read() };
    if h.has_map_bounds == 0 {
        return 0;
    }
    let result = [
        legacy_coordinate(h.minimum_extent[0], h.map_size[0], x, true),
        legacy_coordinate(h.minimum_extent[1], h.map_size[1], 0, false),
        legacy_coordinate(h.minimum_extent[2], h.map_size[2], 117 - y, true),
    ];
    unsafe { ptr::copy_nonoverlapping(result.as_ptr(), out, 3) };
    u8::from(result.iter().all(|v| v.is_finite()))
}

#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn legacy_add(a: f32, b: f32) -> f32 {
    a + b
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn legacy_sub(a: f32, b: f32) -> f32 {
    a - b
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn legacy_load(a: f32) -> f32 {
    a
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn legacy_load(a: f32) -> f32 {
    let mut v = [a, 0.0];
    // SAFETY: two valid slots, x87 stack balanced; preserve signaling-NaN
    // quieting performed by the original min/max loads and stores.
    unsafe {
        core::arch::asm!("fld dword ptr [{p}]","fstp dword ptr [{p}+4]",
        p=in(reg) v.as_mut_ptr(),out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
        out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[1]
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn legacy_add(a: f32, b: f32) -> f32 {
    legacy_operation(a, b, false)
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn legacy_sub(a: f32, b: f32) -> f32 {
    legacy_operation(a, b, true)
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn legacy_operation(a: f32, b: f32, subtract: bool) -> f32 {
    let mut v = [a, b, 0.0];
    let op = u32::from(subtract);
    // SAFETY: three valid slots, explicit float result spill, balanced stack.
    unsafe {
        core::arch::asm!("fld dword ptr [{p}]","cmp {op:e},0","je 2f",
        "fsub dword ptr [{p}+4]","jmp 3f","2:","fadd dword ptr [{p}+4]",
        "3:","fstp dword ptr [{p}+8]",p=in(reg) v.as_mut_ptr(),op=in(reg) op,
        out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
        out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    v[2]
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
fn legacy_coordinate(min: f32, size: f32, index: u32, grid: bool) -> f32 {
    min + size * if grid { index as f32 / 117.0 } else { 0.5 }
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
fn legacy_coordinate(min: f32, size: f32, index: u32, grid: bool) -> f32 {
    let mut v = [
        min.to_bits(),
        size.to_bits(),
        index,
        117.0_f32.to_bits(),
        0.5_f32.to_bits(),
        0,
    ];
    let mode = u32::from(grid);
    // SAFETY: six local slots; index validated <=117. Original MinGW keeps
    // ratio, product and addition extended until the final float output spill.
    unsafe {
        core::arch::asm!("cmp {mode:e},0","je 2f","fild dword ptr [{p}+8]",
        "fdiv dword ptr [{p}+12]","jmp 3f","2:","fld dword ptr [{p}+16]",
        "3:","fmul dword ptr [{p}+4]","fadd dword ptr [{p}]","fstp dword ptr [{p}+20]",
        p=in(reg) v.as_mut_ptr(),mode=in(reg) mode,
        out("st(0)") _,out("st(1)") _,out("st(2)") _,out("st(3)") _,
        out("st(4)") _,out("st(5)") _,out("st(6)") _,out("st(7)") _,options(nostack));
    }
    f32::from_bits(v[5])
}
