//! Shader token/format decoding and small legacy presentation/save policies.
use alloc::{format, string::String, vec::Vec};
use core::{
    ffi::{c_char, c_void},
    ptr, slice,
};
type Visitor = unsafe extern "C" fn(*mut c_void, u32, *const u8, usize);

unsafe fn words<'a>(p: *const u32, n: usize) -> Option<&'a [u32]> {
    if p.is_null() || !p.is_aligned() || n > isize::MAX as usize / 4 {
        return None;
    }
    Some(unsafe { slice::from_raw_parts(p, n) })
}
unsafe fn bytes<'a>(p: *const u8, n: usize) -> Option<&'a [u8]> {
    if n == 0 {
        Some(&[])
    } else if p.is_null() || n > isize::MAX as usize {
        None
    } else {
        Some(unsafe { slice::from_raw_parts(p, n) })
    }
}
fn parameters(op: u32, pixel: bool, minor: u32) -> Option<usize> {
    Some(match op {
        0 => 0,
        1 => 2,
        2 | 3 | 5 | 8..=13 | 17 | 20..=24 => 3,
        4 | 18 => 4,
        6 | 7 | 14..=16 | 19 | 78 | 79 => 2,
        64 | 66 if pixel => {
            if minor == 4 {
                2
            } else {
                1
            }
        }
        65 | 87 if pixel => 1,
        67..=74 | 77 | 82..=86 if pixel => 2,
        76 | 89 if pixel => 3,
        80 | 88 if pixel => 4,
        81 => 5,
        0xfffd if pixel && minor == 4 => 0,
        _ => return None,
    })
}
fn declaration(w: &[u32]) -> (u32, usize) {
    let mut at = 0;
    while at < w.len() {
        let t = w[at];
        at += 1;
        let kind = t >> 29;
        if kind == 7 {
            return if t == u32::MAX { (0, at) } else { (3, 0) };
        }
        let payload = match kind {
            0 => {
                if t != 0 {
                    return (3, 0);
                }
                0
            }
            1..=3 => 0,
            4 => ((t >> 25) & 15) as usize * 4,
            5 => ((t >> 24) & 31) as usize,
            _ => return (2, 0),
        };
        if payload > w.len() - at {
            return (1, 0);
        }
        at += payload;
    }
    (1, 0)
}
fn shader(w: &[u32]) -> (u32, usize) {
    if w.is_empty() {
        return (1, 0);
    }
    let version = w[0];
    let pixel = version & 0xffffff00 == 0xffff0100 && version & 255 <= 4;
    if !matches!(version, 0xfffe0100 | 0xfffe0101) && !pixel {
        return (2, 0);
    }
    let mut at = 1;
    while at < w.len() {
        let t = w[at];
        at += 1;
        let op = t & 65535;
        if op == 65535 {
            return if t == 65535 { (0, at) } else { (3, 0) };
        }
        let n = if op == 65534 {
            ((t >> 16) & 0x7fff) as usize
        } else {
            match parameters(op, pixel, version & 255) {
                Some(n) => n,
                None => return (2, 0),
            }
        };
        if n > w.len() - at {
            return (1, 0);
        }
        at += n;
    }
    (1, 0)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_tokens(
    p: *const u32,
    n: usize,
    kind: u8,
    status: *mut u32,
    used: *mut usize,
) {
    let result = match unsafe { words(p, n) } {
        Some(w) => {
            if kind == 0 {
                declaration(w)
            } else {
                shader(w)
            }
        }
        None => (3, 0),
    };
    if !status.is_null() {
        unsafe { status.write(result.0) }
    }
    if !used.is_null() {
        unsafe { used.write(result.1) }
    }
}

fn fvf(value: u32) -> Option<[u32; 27]> {
    let position = value & 0xe;
    let textures = (value & 0xf00) >> 8;
    if !matches!(position, 2 | 4) || textures > 8 || (position == 4 && value & 0x10 != 0) {
        return None;
    }
    let mut allowed = 0xe | 0x10 | 0x40 | 0x80 | 0xf00;
    for stage in 0..textures {
        allowed |= 3 << (16 + 2 * stage)
    }
    if value & !allowed != 0 {
        return None;
    }
    let mut out = [0_u32; 27];
    out[1] = u32::from(position == 4);
    out[2] = if position == 4 { 4 } else { 3 };
    out[6] = textures;
    out[0] = out[2] * 4;
    for (flag, marker, offset, size) in [(0x10, 3, 8, 12), (0x40, 4, 9, 4), (0x80, 5, 10, 4)] {
        if value & flag != 0 {
            out[marker] = 1;
            out[offset] = out[0];
            out[0] += size;
        }
    }
    for stage in 0..textures as usize {
        let encoded = (value >> (16 + stage * 2)) & 3;
        let components = match encoded {
            3 => 1,
            1 => 3,
            2 => 4,
            _ => 2,
        };
        out[11 + stage] = out[0];
        out[19 + stage] = components;
        out[0] += components * 4;
    }
    Some(out)
}
fn primitive(kind: u32, count: u32) -> Option<u32> {
    if !(1..=6).contains(&kind) {
        return None;
    }
    if count == 0 {
        return Some(0);
    }
    let (mult, extra) = match kind {
        1 => (1, 0),
        2 => (2, 0),
        3 => (1, 1),
        4 => (3, 0),
        _ => (1, 2),
    };
    count.checked_mul(mult)?.checked_add(extra)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_fvf(value: u32, out: *mut u32) -> u8 {
    if out.is_null() {
        return 0;
    }
    let decoded = fvf(value);
    let data = decoded.unwrap_or([0; 27]);
    unsafe { ptr::copy_nonoverlapping(data.as_ptr(), out, 27) };
    u8::from(decoded.is_some())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_primitive(kind: u32, count: u32, out: *mut u32) -> u8 {
    if out.is_null() {
        return 0;
    }
    let n = primitive(kind, count);
    unsafe { out.write(n.unwrap_or(0)) };
    u8::from(n.is_some())
}
unsafe fn index(p: *const u8, format: u32, i: u32) -> u32 {
    if p.is_null() {
        i
    } else if format == 101 {
        unsafe { ptr::read_unaligned(p.add(i as usize * 2).cast::<u16>()) as u32 }
    } else {
        unsafe { ptr::read_unaligned(p.add(i as usize * 4).cast::<u32>()) }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_fan(
    count: u32,
    p: *const u8,
    format: u32,
    vertices: u32,
    out: *mut u32,
    capacity: usize,
) -> u8 {
    if !matches!(format, 101 | 102) {
        return 0;
    }
    let Some(source_count) = primitive(6, count) else {
        return 0;
    };
    if count == 0 {
        return 1;
    }
    if (p.is_null() && source_count > vertices) || count > u32::MAX / 12 {
        return 0;
    }
    for i in 0..source_count {
        if unsafe { index(p, format, i) } >= vertices {
            return 0;
        }
    }
    if out.is_null() {
        return 1;
    }
    let needed = count as usize * 3;
    if capacity < needed {
        return 0;
    }
    let centre = unsafe { index(p, format, 0) };
    for i in 0..count {
        unsafe {
            out.add(i as usize * 3).write(centre);
            out.add(i as usize * 3 + 1).write(index(p, format, i + 1));
            out.add(i as usize * 3 + 2).write(index(p, format, i + 2));
        }
    }
    1
}

fn mask(bits: u32) -> String {
    let mut s = String::from(".");
    for i in 0..4 {
        if bits & (1 << i) != 0 {
            s.push(b"xyzw"[i] as char)
        }
    }
    s
}
#[derive(Default)]
struct Translation {
    hlsl: String,
    inputs: u16,
    outputs: [u8; 12],
    count: usize,
}
struct Translator {
    out: Translation,
    body: String,
    initialized: [u32; 12],
    at: usize,
}
impl Translator {
    fn fail(&self, why: &str) -> String {
        format!("VS1.1 word {}: {}", self.at, why)
    }
    fn destination(&mut self, t: u32) -> Result<(String, u32), String> {
        if t & 0x80000000 == 0 || t & !0xf00f07ff != 0 {
            return Err(self.fail("unsupported destination encoding/modifier"));
        }
        let kind = (t >> 28) & 7;
        let i = t & 0x7ff;
        let write = (t >> 16) & 15;
        if write == 0 {
            return Err(self.fail("empty destination mask"));
        }
        let name = if kind == 0 && i < 12 {
            format!("r{i}")
        } else {
            let (slot, name) = if kind == 4 && i == 0 {
                (0, String::from("o.position"))
            } else if kind == 5 && i < 2 {
                (
                    i + 1,
                    String::from(if i == 0 { "o.diffuse" } else { "o.specular" }),
                )
            } else if kind == 6 && i < 8 {
                (i + 3, format!("o.tex{i}"))
            } else {
                return Err(self
                    .fail("unsupported destination register (including fog/point size/address)"));
            };
            self.out.outputs[slot as usize] |= write as u8;
            name
        };
        Ok((name, write))
    }
    fn source(&mut self, t: u32, needed: u32) -> Result<String, String> {
        if t & 0x80000000 == 0 || t & !0xffff07ff != 0 {
            return Err(self.fail("unsupported source encoding/relative address"));
        }
        let kind = (t >> 28) & 7;
        let i = t & 0x7ff;
        let modifier = (t >> 24) & 15;
        let swizzle = (t >> 16) & 255;
        if modifier > 1 {
            return Err(self.fail("unsupported source modifier"));
        }
        let mut required = 0;
        for c in 0..4 {
            if needed & (1 << c) != 0 {
                required |= 1 << ((swizzle >> (c * 2)) & 3)
            }
        }
        let name = if kind == 0 && i < 12 {
            if self.initialized[i as usize] & required != required {
                return Err(self.fail("read of unwritten temporary component"));
            }
            format!("r{i}")
        } else if kind == 1 && i < 16 {
            self.out.inputs |= 1 << i;
            format!("i.v{i}")
        } else if kind == 2 && i < 96 {
            format!("c[{i}]")
        } else {
            return Err(self.fail("unsupported source register"));
        };
        let mut s = format!("{}{}.", if modifier != 0 { "(-" } else { "(" }, name);
        for c in 0..4 {
            s.push(b"xyzw"[((swizzle >> (c * 2)) & 3) as usize] as char)
        }
        s.push(')');
        Ok(s)
    }
    fn instruction(&mut self, p: &[u32], op: u32) -> Result<(), String> {
        let sources = if op == 4 {
            3
        } else if op == 1 || op == 7 {
            1
        } else {
            2
        };
        let (dest, write) = self.destination(p[0])?;
        let needed = if op == 8 {
            7
        } else if op == 7 {
            1
        } else if op == 9 || op == 20 {
            15
        } else {
            write
        };
        let mut s = [String::new(), String::new(), String::new()];
        s[0] = self.source(p[1], needed)?;
        if op != 20 {
            for n in 1..sources {
                s[n] = self.source(p[n + 1], needed)?;
            }
        }
        let value = match op {
            1 => s[0].clone(),
            2 => format!("{} + {}", s[0], s[1]),
            4 => format!("{} * {} + {}", s[0], s[1], s[2]),
            5 => format!("{} * {}", s[0], s[1]),
            7 => format!("rsqrt(abs({}.x))", s[0]),
            8 => format!("dot({}.xyz,{}.xyz)", s[0], s[1]),
            9 => format!("dot({},{})", s[0], s[1]),
            10 => format!("min({},{})", s[0], s[1]),
            11 => format!("max({},{})", s[0], s[1]),
            20 => {
                let m = p[2];
                if p[1] & 0x0fff0000 != 0x00e40000 {
                    return Err(
                        self.fail("m4x4 vector modifiers not admitted by this captured subset")
                    );
                }
                if write != 15 || m & 0xfffff800 != 0xa0e40000 || m & 0x7ff > 92 {
                    return Err(self.fail(
                        "m4x4 requires full destination and four direct, unmodified constants",
                    ));
                }
                if p[0] & 0x700007ff == p[1] & 0x700007ff {
                    return Err(self.fail("m4x4 destination/source alias"));
                }
                let mut v = String::from("float4(");
                for row in 0..4 {
                    if row != 0 {
                        v.push(',')
                    }
                    v.push_str(&format!("dot({},c[{}])", s[0], (m & 0x7ff) + row));
                }
                v.push(')');
                v
            }
            _ => return Err(self.fail("unsupported opcode")),
        };
        let temp = format!("value{}", self.out.count);
        self.out.count += 1;
        self.body.push_str(&format!(
            "  precise float4 {temp} = {value};\n  {dest}{} = {temp}{};\n",
            mask(write),
            mask(write)
        ));
        if (p[0] >> 28) & 7 == 0 {
            self.initialized[(p[0] & 0x7ff) as usize] |= write;
        }
        Ok(())
    }
}
fn interface() -> String {
    let mut s = String::from(
        "struct VSOutput { float4 position : SV_Position; float4 diffuse : COLOR0; float4 specular : COLOR1;\n",
    );
    for t in 0..8 {
        s.push_str(&format!("  float4 tex{t} : TEXCOORD{t};\n"));
    }
    s.push_str("};\n");
    s
}
fn preamble(inputs: u16) -> String {
    let mut s = String::from(
        "// A2FO original VS1.1 compatibility subset; persistent-cache source key.\ncbuffer LegacyConstants : register(b0) { float4 c[96]; };\ncbuffer RasterConversion : register(b1) { float2 viewport_inv_size; float2 raster_padding; };\n",
    );
    if inputs != 0 {
        s.push_str("struct VSInput {\n");
        for v in 0..16 {
            if inputs & (1 << v) != 0 {
                s.push_str(&format!("  float4 v{v} : TEXCOORD{v};\n"));
            }
        }
        s.push_str("};\n");
    }
    s.push_str(&interface());
    s.push_str("VSOutput VSMain(");
    if inputs != 0 {
        s.push_str("VSInput i")
    }
    s.push_str(") {\n  VSOutput o = (VSOutput)0;\n");
    for r in 0..12 {
        s.push_str(&format!("  float4 r{r} = 0;\n"));
    }
    s
}
fn translate(w: &[u32]) -> Result<Translation, String> {
    let parsed = shader(w);
    if parsed.0 != 0 || !matches!(w[0], 0xfffe0100 | 0xfffe0101) || parsed.1 != w.len() {
        return Err(String::from(
            "VS1.0/1.1 requires one complete supported stream without trailing words",
        ));
    }
    let mut t = Translator {
        out: Translation::default(),
        body: String::new(),
        initialized: [0; 12],
        at: 1,
    };
    while t.at < w.len() {
        let token = w[t.at];
        let op = token & 65535;
        if op == 65535 {
            break;
        }
        if op == 65534 {
            if token & 0x80000000 != 0 {
                return Err(String::from("VS1.1 invalid comment encoding"));
            }
            t.at += 1 + ((token >> 16) & 0x7fff) as usize;
            continue;
        }
        if token != op {
            return Err(t.fail("unsupported instruction flags/coissue"));
        }
        let operands = match op {
            1 | 7 => 2,
            2 | 5 | 8..=11 | 20 => 3,
            4 => 4,
            _ => return Err(t.fail("unsupported opcode")),
        };
        t.instruction(&w[t.at + 1..t.at + 1 + operands], op)?;
        t.at += operands + 1;
    }
    if t.out.outputs[0] != 15 {
        return Err(String::from("VS1.1 must write all oPos components"));
    }
    t.out.hlsl = preamble(t.out.inputs)
        + &t.body
        + "  o.position.xy += float2(viewport_inv_size.x,-viewport_inv_size.y) * o.position.w;\n  o.diffuse = saturate(o.diffuse); o.specular = saturate(o.specular);\n  return o;\n}\n";
    Ok(t.out)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_vs_interface(ctx: *mut c_void, visitor: Option<Visitor>) {
    if let Some(v) = visitor {
        let text = interface();
        unsafe { v(ctx, 0, text.as_ptr(), text.len()) }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_vs11(
    p: *const u32,
    n: usize,
    ctx: *mut c_void,
    visitor: Option<Visitor>,
    inputs: *mut u16,
    masks: *mut u8,
    count: *mut usize,
) -> u8 {
    let result = if p.is_null() || n > 4096 {
        Err(String::from("VS1.1 invalid/oversize bounded input"))
    } else {
        match unsafe { words(p, n) } {
            Some(w) => translate(w),
            None => Err(String::from("VS1.1 invalid/oversize bounded input")),
        }
    };
    match result {
        Ok(t) => {
            if !inputs.is_null() {
                unsafe { inputs.write(t.inputs) }
            }
            if !masks.is_null() {
                unsafe { ptr::copy_nonoverlapping(t.outputs.as_ptr(), masks, 12) }
            }
            if !count.is_null() {
                unsafe { count.write(t.count) }
            }
            if let Some(v) = visitor {
                unsafe { v(ctx, 0, t.hlsl.as_ptr(), t.hlsl.len()) }
            }
            1
        }
        Err(e) => {
            if let Some(v) = visitor {
                unsafe { v(ctx, 1, e.as_ptr(), e.len()) }
            }
            0
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_fallback(
    object: *const u8,
    on: usize,
    faction: *const u8,
    fn_: usize,
    ctx: *mut c_void,
    visitor: Option<Visitor>,
) {
    let (Some(object), Some(faction), Some(v)) = (
        unsafe { bytes(object, on) },
        unsafe { bytes(faction, fn_) },
        visitor,
    ) else {
        return;
    };
    for (kind, prefix, stem, suffix) in [
        (0, b"b_".as_slice(), object, b"".as_slice()),
        (1, b"".as_slice(), faction, b"_icon".as_slice()),
    ] {
        let mut text = Vec::new();
        if !stem.is_empty() && prefix.len() + stem.len() + suffix.len() <= 255 {
            text.extend_from_slice(prefix);
            text.extend_from_slice(stem);
            text.extend_from_slice(suffix);
        }
        unsafe { v(ctx, kind, text.as_ptr(), text.len()) }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_lead_shield_action(configured: u8, current: f32, effect: i32) -> u32 {
    if configured == 0 {
        return 0;
    }
    let up = current.is_finite() && current > 0.0;
    if up && effect < 0 {
        1
    } else if !up && effect >= 0 {
        2
    } else {
        0
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_lead_scan_due(now: u32, last: u32, scanned: u8, interval: u32) -> u8 {
    u8::from(scanned == 0 || interval == 0 || now.wrapping_sub(last) >= interval)
}

fn u32_at(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes(b[at..at + 4].try_into().unwrap())
}
fn editor_valid(b: &[u8]) -> bool {
    if b.len() != 1076
        || &b[..8] != b"A2FOEDIT"
        || u32_at(b, 8) != 1
        || u32_at(b, 12) != 1076
        || u32_at(b, 16) & !7 != 0
        || !b[52..564].contains(&0)
        || !b[564..1076].contains(&0)
    {
        return false;
    }
    if u32_at(b, 16) & 4 == 0 {
        return true;
    }
    let mut current_total = 0.0_f32;
    let mut maximum_total = 0.0_f32;
    let mut active = false;
    for i in 0..4 {
        let c = f32::from_bits(u32_at(b, 20 + i * 4));
        let m = f32::from_bits(u32_at(b, 36 + i * 4));
        if !c.is_finite() || !m.is_finite() || m < 0.0 || c < 0.0 {
            return false;
        }
        if m == 0.0 {
            if c != 0.0 {
                return false;
            }
        } else {
            if c > m {
                return false;
            }
            active = true;
        }
        current_total += c;
        maximum_total += m;
    }
    active && current_total.is_finite() && maximum_total.is_finite() && maximum_total > 0.0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_editor_valid(p: *const u8, n: usize) -> u8 {
    u8::from(unsafe { bytes(p, n) }.is_some_and(editor_valid))
}
unsafe extern "C" {
    fn strtof(p: *const c_char, end: *mut *mut c_char) -> f32;
}
#[cfg(target_os = "windows")]
unsafe extern "C" {
    fn _errno() -> *mut i32;
}
#[cfg(target_os = "linux")]
unsafe extern "C" {
    fn __errno_location() -> *mut i32;
}
unsafe fn errno() -> *mut i32 {
    #[cfg(target_os = "windows")]
    {
        unsafe { _errno() }
    }
    #[cfg(target_os = "linux")]
    {
        unsafe { __errno_location() }
    }
}
fn space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\r' | b'\n')
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_editor_number(text: *const c_char, out: *mut f32) -> u8 {
    if text.is_null() || out.is_null() {
        return 0;
    }
    unsafe { *errno() = 0 }
    let mut end = ptr::null_mut();
    let parsed = unsafe { strtof(text, &mut end) };
    if ptr::eq(end, text) || unsafe { *errno() } == 34 || !parsed.is_finite() {
        return 0;
    }
    while unsafe { *end } != 0 && space(unsafe { *end } as u8) {
        end = unsafe { end.add(1) }
    }
    if unsafe { *end } != 0 {
        return 0;
    }
    unsafe { out.write(parsed) };
    1
}
fn trim(mut b: &[u8]) -> &[u8] {
    while b.first().is_some_and(|&c| space(c)) {
        b = &b[1..]
    }
    while b.last().is_some_and(|&c| space(c)) {
        b = &b[..b.len() - 1]
    }
    b
}
fn hex(c: u8) -> Option<u8> {
    match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        b'A'..=b'F' => Some(c - b'A' + 10),
        _ => None,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_lead_editor_decode(
    p: *const u8,
    n: usize,
    binary: u8,
    tagged: u8,
    out: *mut u8,
    used: *mut usize,
) -> u32 {
    if !used.is_null() {
        unsafe { used.write(0) }
    }
    if out.is_null() || used.is_null() {
        return 2;
    }
    let Some(b) = (unsafe { bytes(p, n) }) else {
        return 2;
    };
    let mut candidate = [0_u8; 1076];
    let consumed;
    if binary != 0 {
        let header = if tagged != 0 { 8 } else { 0 };
        if b.len() < header + 8 {
            return 0;
        }
        if &b[header..header + 8] != b"A2FOEDIT" {
            return 0;
        }
        if tagged != 0 && (u32_at(b, 0) & 255 != 0 || u32_at(b, 4) != 1076) {
            return 2;
        }
        if b.len() < header + 1076 {
            return 2;
        }
        candidate.copy_from_slice(&b[header..header + 1076]);
        consumed = header + 1076;
    } else {
        let newline = b.iter().position(|&c| c == b'\n');
        let line = &b[..newline.unwrap_or(b.len())];
        let Some(equals) = line.iter().position(|&c| c == b'=') else {
            return 0;
        };
        if trim(&line[..equals]) != b"a2fo_objectEditor" {
            return 0;
        }
        let encoded = trim(&line[equals + 1..]);
        if newline.is_none() || encoded.len() != 2152 {
            return 2;
        }
        for i in 0..1076 {
            let (Some(h), Some(l)) = (hex(encoded[i * 2]), hex(encoded[i * 2 + 1])) else {
                return 2;
            };
            candidate[i] = h * 16 + l;
        }
        consumed = newline.unwrap() + 1;
    }
    if !editor_valid(&candidate) {
        return 2;
    }
    unsafe {
        ptr::copy_nonoverlapping(candidate.as_ptr(), out, 1076);
        used.write(consumed)
    };
    1
}
