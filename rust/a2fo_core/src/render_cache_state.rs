//! Shader-cache, BC-catalog, and D3D8 state decisions. Native calls use
//! synchronous byte-span and POD callbacks owned by the C++ boundary.
// Keep the legacy field order, bounded indices and ABI-shaped arguments auditable.
#![allow(clippy::needless_range_loop)]
use alloc::{format, vec, vec::Vec};
use core::{ffi::c_void, slice};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Span {
    data: *const u8,
    size: usize,
}
impl Span {
    fn of(v: &[u8]) -> Self {
        Self {
            data: v.as_ptr(),
            size: v.len(),
        }
    }
}
#[repr(C)]
pub struct ShaderInput {
    source: Span,
    entry: Span,
    target: Span,
    directory: Span,
    flags: u32,
    invalid_arg: i32,
    fail: i32,
    out_of_memory: i32,
    already_exists: u32,
    max_vector_size: usize,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ShaderReply {
    hr: i32,
    value: u32,
    extra: u32,
    size64: i64,
    clock_start: i64,
    clock_end: i64,
    frequency: i64,
    file_size: u64,
    file_time: u64,
    first: Span,
    second: Span,
}
impl Default for ShaderReply {
    fn default() -> Self {
        Self {
            hr: 0,
            value: 0,
            extra: 0,
            size64: 0,
            clock_start: 0,
            clock_end: 0,
            frequency: 0,
            file_size: 0,
            file_time: 0,
            first: Span::of(&[]),
            second: Span::of(&[]),
        }
    }
}
#[repr(C)]
struct ShaderPublish {
    bytes: Span,
    diagnostics: Span,
    compile_us: u64,
    cache_hit: u32,
    cache_written: u32,
}
type ShaderCall = unsafe extern "C" fn(*mut c_void, u32, Span, *mut ShaderReply) -> i32;
fn bytes<'a>(s: Span) -> &'a [u8] {
    if s.size == 0 || s.data.is_null() {
        &[]
    } else {
        unsafe { slice::from_raw_parts(s.data, s.size) }
    }
}
fn op(ctx: *mut c_void, cb: ShaderCall, id: u32, data: &[u8]) -> Option<ShaderReply> {
    let mut out = ShaderReply::default();
    (unsafe { cb(ctx, id, Span::of(data), &mut out) } != 0).then_some(out)
}
fn op_read(ctx: *mut c_void, cb: ShaderCall, data: &mut [u8]) -> Option<ShaderReply> {
    let mut out = ShaderReply::default();
    let span = Span {
        data: data.as_mut_ptr(),
        size: data.len(),
    };
    (unsafe { cb(ctx, 7, span, &mut out) } != 0).then_some(out)
}
fn ok(r: &ShaderReply) -> bool {
    r.value != 0
}
fn hash(data: &[u8], mut state: u64) -> u64 {
    for &v in data {
        state = (state ^ v as u64).wrapping_mul(1099511628211);
    }
    state
}
const OFFSET: u64 = 14695981039346656037;
fn field(out: &mut Vec<u8>, value: &[u8]) {
    out.extend_from_slice(&(value.len() as u32).to_le_bytes());
    out.extend_from_slice(value);
}
fn dxbc(data: &[u8]) -> bool {
    data.starts_with(b"DXBC")
}
fn publish(
    ctx: *mut c_void,
    cb: ShaderCall,
    compiled: &[u8],
    diagnostics: &[u8],
    compile_us: u64,
    cache_hit: bool,
    cache_written: bool,
) -> bool {
    let p = ShaderPublish {
        bytes: Span::of(compiled),
        diagnostics: Span::of(diagnostics),
        compile_us,
        cache_hit: cache_hit as u32,
        cache_written: cache_written as u32,
    };
    let raw = unsafe {
        slice::from_raw_parts(
            (&p as *const ShaderPublish).cast::<u8>(),
            core::mem::size_of::<ShaderPublish>(),
        )
    };
    op(ctx, cb, 18, raw).is_some()
}
fn read_cache(ctx: *mut c_void, cb: ShaderCall, path: &[u8], key: &[u8]) -> Option<Vec<u8>> {
    let opened = op(ctx, cb, 5, path)?;
    if !ok(&opened) {
        return None;
    }
    let result = (|| {
        let size = op(ctx, cb, 6, &[])?;
        if !ok(&size) || size.size64 < 28 || size.size64 > (28 + 1024 * 1024 + 8 * 1024 * 1024) {
            return None;
        }
        let mut header = [0u8; 28];
        if !ok(&op_read(ctx, cb, &mut header)?) {
            return None;
        }
        if &header[..8] != b"A2FOSM5\0" || u32::from_le_bytes(header[8..12].try_into().ok()?) != 1 {
            return None;
        }
        let key_size = u32::from_le_bytes(header[12..16].try_into().ok()?) as usize;
        let payload_size = u32::from_le_bytes(header[16..20].try_into().ok()?) as usize;
        if key_size != key.len()
            || !(4..=8 * 1024 * 1024).contains(&payload_size)
            || size.size64 as usize != 28 + key_size + payload_size
        {
            return None;
        }
        let mut stored = vec![0u8; key_size];
        if !ok(&op_read(ctx, cb, &mut stored)?) || stored != key {
            return None;
        }
        let mut payload = vec![0u8; payload_size];
        if !ok(&op_read(ctx, cb, &mut payload)?) || !dxbc(&payload) {
            return None;
        }
        let checksum = hash(&payload, hash(key, OFFSET));
        if checksum != u64::from_le_bytes(header[20..28].try_into().ok()?) {
            return None;
        }
        Some(payload)
    })();
    let _ = op(ctx, cb, 8, &[]);
    result
}
fn write_cache(
    ctx: *mut c_void,
    cb: ShaderCall,
    directory: &[u8],
    path: &[u8],
    key: &[u8],
    payload: &[u8],
    already_exists: u32,
) -> bool {
    if payload.len() > 8 * 1024 * 1024 || !dxbc(payload) {
        return false;
    }
    let Some(created) = op(ctx, cb, 10, directory) else {
        return false;
    };
    if !ok(&created) && created.hr as u32 != already_exists {
        return false;
    }
    let Some(suffix) = op(ctx, cb, 11, &[]) else {
        return false;
    };
    let mut temp = path.to_vec();
    temp.extend_from_slice(
        format!(
            ".{}.{}.{}.tmp",
            suffix.value, suffix.extra, suffix.size64 as i32
        )
        .as_bytes(),
    );
    let Some(opened) = op(ctx, cb, 12, &temp) else {
        return false;
    };
    if !ok(&opened) {
        return false;
    }
    let mut header = Vec::with_capacity(28);
    header.extend_from_slice(b"A2FOSM5\0");
    header.extend_from_slice(&1u32.to_le_bytes());
    header.extend_from_slice(&(key.len() as u32).to_le_bytes());
    header.extend_from_slice(&(payload.len() as u32).to_le_bytes());
    header.extend_from_slice(&hash(payload, hash(key, OFFSET)).to_le_bytes());
    let written = op(ctx, cb, 13, &header).is_some_and(|r| ok(&r))
        && op(ctx, cb, 13, key).is_some_and(|r| ok(&r))
        && op(ctx, cb, 13, payload).is_some_and(|r| ok(&r))
        && op(ctx, cb, 14, &[]).is_some_and(|r| ok(&r));
    let _ = op(ctx, cb, 15, &[]);
    let replaced = written && op(ctx, cb, 16, path).is_some_and(|r| ok(&r));
    if !replaced {
        let _ = op(ctx, cb, 17, &temp);
    }
    replaced
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_shader_compile(
    input: *const ShaderInput,
    ctx: *mut c_void,
    cb: ShaderCall,
) -> i32 {
    if input.is_null() {
        return 0;
    }
    let input = unsafe { &*input };
    let source = bytes(input.source);
    let entry = bytes(input.entry);
    let target = bytes(input.target);
    let directory = bytes(input.directory);
    if source.len() > 1024 * 1024
        || entry.is_empty()
        || target.is_empty()
        || source.contains(&0)
        || entry.contains(&0)
        || target.contains(&0)
    {
        return input.invalid_arg;
    }
    let Some(mut module) = op(ctx, cb, 1, &[]) else {
        return input.fail;
    };
    if !ok(&module) {
        module = match op(ctx, cb, 2, &[]) {
            Some(v) => v,
            None => return input.fail,
        };
    }
    if !ok(&module) {
        return module.hr;
    }
    let Some(symbol) = op(ctx, cb, 3, &[]) else {
        return input.fail;
    };
    if !ok(&symbol) {
        return symbol.hr;
    }
    let mut path = Vec::new();
    let mut key = Vec::new();
    if !directory.is_empty()
        && let Some(identity) = op(ctx, cb, 4, &[])
        && ok(&identity)
    {
        field(&mut key, source);
        field(&mut key, entry);
        field(&mut key, target);
        key.extend_from_slice(&input.flags.to_le_bytes());
        let mut compiler = bytes(identity.first).to_vec();
        compiler.extend_from_slice(&identity.file_size.to_le_bytes());
        compiler.extend_from_slice(&identity.file_time.to_le_bytes());
        field(&mut key, &compiler);
        if key.len() <= 1024 * 1024 {
            path.extend_from_slice(directory);
            if !path.ends_with(b"\\") && !path.ends_with(b"/") {
                path.push(b'\\');
            }
            path.extend_from_slice(format!("shader-{:016x}.bin", hash(&key, OFFSET)).as_bytes());
            if let Some(hit) = read_cache(ctx, cb, &path, &key) {
                if !publish(ctx, cb, &hit, &[], 0, true, false) {
                    return input.fail;
                }
                return 0;
            }
        }
    }
    let Some(compiled) = op(ctx, cb, 9, &[]) else {
        return input.fail;
    };
    let compile_us = if compiled.frequency > 0 && compiled.clock_end >= compiled.clock_start {
        ((compiled.clock_end - compiled.clock_start) as u64).wrapping_mul(1_000_000)
            / (compiled.frequency as u64)
    } else {
        0
    };
    let diagnostics = bytes(compiled.second).to_vec();
    if compiled.hr < 0 {
        if !publish(ctx, cb, &[], &diagnostics, compile_us, false, false) {
            return input.fail;
        }
        return compiled.hr;
    }
    if !ok(&compiled) || compiled.first.size > input.max_vector_size {
        if !publish(ctx, cb, &[], &diagnostics, compile_us, false, false) {
            return input.fail;
        }
        return input.fail;
    }
    let shader = bytes(compiled.first).to_vec();
    let cache_written = if path.is_empty() {
        false
    } else {
        write_cache(
            ctx,
            cb,
            directory,
            &path,
            &key,
            &shader,
            input.already_exists,
        )
    };
    if !publish(
        ctx,
        cb,
        &shader,
        &diagnostics,
        compile_us,
        false,
        cache_written,
    ) {
        return input.fail;
    }
    0
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct BcMip {
    width: u32,
    height: u32,
    row_pitch: u32,
    slice_pitch: u32,
    offset: usize,
}
#[repr(C)]
pub struct BcEntryView {
    name: Span,
    width: u32,
    height: u32,
    mips: u32,
    format: u32,
    normal: u32,
    top_hash: u64,
    file_hash: u64,
    mip_hashes: *const u64,
}
#[repr(C)]
struct BcEntry {
    name: Span,
    width: u32,
    height: u32,
    mips: u32,
    format: u32,
    normal: u32,
    top_hash: u64,
    file_hash: u64,
    mip_hashes: [u64; 15],
}
#[repr(C)]
struct BcPublication {
    directory: Span,
    identity: Span,
}
#[repr(C)]
struct BcReadRequest {
    path: Span,
    max_size: usize,
}
#[repr(C)]
struct BcPublishImage {
    name: Span,
    normal: u32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BcImageView {
    hr: i32,
    format: u32,
    width: u32,
    height: u32,
    alpha: u32,
    mip_count: usize,
    mips: [BcMip; 15],
    bytes: Span,
    parse_error: Span,
}
impl Default for BcImageView {
    fn default() -> Self {
        Self {
            hr: 0,
            format: 0,
            width: 0,
            height: 0,
            alpha: 0,
            mip_count: 0,
            mips: [BcMip::default(); 15],
            bytes: Span::of(&[]),
            parse_error: Span::of(&[]),
        }
    }
}
#[repr(C)]
struct BcChain {
    bytes: Span,
    name: Span,
    format: u32,
    width: u32,
    height: u32,
    alpha: u32,
    normal: u32,
    mip_count: usize,
    mips: [BcMip; 15],
}
#[repr(C)]
#[derive(Default)]
pub struct BcReply {
    value: u32,
    span: Span,
    image: BcImageView,
}
impl Default for Span {
    fn default() -> Self {
        Span::of(&[])
    }
}
type BcCall = unsafe extern "C" fn(*mut c_void, u32, Span, *mut BcReply) -> i32;
fn bc_op(ctx: *mut c_void, cb: BcCall, id: u32, data: &[u8]) -> Option<BcReply> {
    let mut out = BcReply::default();
    (unsafe { cb(ctx, id, Span::of(data), &mut out) } != 0).then_some(out)
}
fn pod_bytes<T>(value: &T) -> &[u8] {
    unsafe { slice::from_raw_parts((value as *const T).cast::<u8>(), core::mem::size_of::<T>()) }
}
fn bc_error(ctx: *mut c_void, cb: BcCall, message: &[u8]) {
    let _ = bc_op(ctx, cb, 107, message);
}
fn ascii_lower(v: &[u8]) -> Vec<u8> {
    v.iter()
        .map(|x: &u8| if x.is_ascii_uppercase() { x + 32 } else { *x })
        .collect()
}
fn normalize_root(v: &[u8]) -> Vec<u8> {
    let mut out: Vec<u8> = v
        .iter()
        .map(|x: &u8| {
            if *x == b'/' {
                b'\\'
            } else if x.is_ascii_uppercase() {
                x + 32
            } else {
                *x
            }
        })
        .collect();
    while out.len() > 3 && out.last() == Some(&b'\\') {
        out.pop();
    }
    out
}
fn name_ok(name: &[u8]) -> bool {
    if name.len() < 5 || ascii_lower(&name[name.len() - 4..]) != b".dds" {
        return false;
    }
    name.iter()
        .all(|&x| x.is_ascii_alphanumeric() || x == b'_' || x == b'-' || x == b'.' || x == b' ')
        && name != b"."
        && name != b".."
}
fn decimal(v: &[u8]) -> Option<u32> {
    if v.is_empty() || v.len() > 10 {
        return None;
    }
    let mut n = 0u64;
    for &x in v {
        if !x.is_ascii_digit() {
            return None;
        }
        n = n * 10 + (x - b'0') as u64;
    }
    if n == 0 || n > u32::MAX as u64 {
        None
    } else {
        Some(n as u32)
    }
}
fn hex64(v: &[u8]) -> Option<u64> {
    if v.len() != 16 {
        return None;
    }
    let mut n = 0u64;
    for &x in v {
        let digit = match x {
            b'0'..=b'9' => x - b'0',
            b'A'..=b'F' => x - b'A' + 10,
            b'a'..=b'f' => x - b'a' + 10,
            _ => return None,
        };
        n = (n << 4) | digit as u64;
    }
    Some(n)
}
fn split_fields(line: &[u8]) -> Vec<&[u8]> {
    line.split(|&x| x == b'|').collect()
}
fn line(ctx: *mut c_void, cb: BcCall) -> Option<Vec<u8>> {
    let next = bc_op(ctx, cb, 103, &[])?;
    if next.value == 0 {
        None
    } else {
        Some(bytes(next.span).to_vec())
    }
}
fn parse_bc_entry(
    ctx: *mut c_void,
    cb: BcCall,
    index: usize,
    line: &[u8],
) -> Result<(Vec<u8>, BcEntry), Vec<u8>> {
    let invalid = || format!("invalid or duplicate BC catalog entry {}", index).into_bytes();
    let f = split_fields(line);
    if f.len() != 9 || !name_ok(f[0]) {
        return Err(invalid());
    }
    let Some(width) = decimal(f[1]) else {
        return Err(invalid());
    };
    let Some(height) = decimal(f[2]) else {
        return Err(invalid());
    };
    let Some(mips) = decimal(f[3]) else {
        return Err(invalid());
    };
    let Some(format) = decimal(f[4]) else {
        return Err(invalid());
    };
    if (format != 83 && format != 98)
        || (f[5] != b"raw" && f[5] != b"normal_xy")
        || ((format == 83) != (f[5] == b"normal_xy"))
    {
        return Err(invalid());
    }
    let Some(top_hash) = hex64(f[6]) else {
        return Err(invalid());
    };
    let Some(file_hash) = hex64(f[7]) else {
        return Err(invalid());
    };
    if width > 16384 || height > 16384 || mips > 15 {
        return Err(invalid());
    }
    let name = f[0].to_vec();
    let folded = ascii_lower(&name);
    let unique = bc_op(ctx, cb, 104, &folded).ok_or_else(invalid)?;
    if unique.value == 0 {
        return Err(invalid());
    }
    let mut hashes = [0u64; 15];
    let parts: Vec<&[u8]> = f[8].split(|&x| x == b',').collect();
    for (i, part) in parts.iter().enumerate() {
        let Some(value) = hex64(part) else {
            return Err(format!("invalid BC mip hash for entry {}", index).into_bytes());
        };
        if i < 15 {
            hashes[i] = value;
        }
    }
    if parts.len() != mips as usize || hashes[0] != top_hash {
        return Err(format!("BC mip count/hash mismatch for entry {}", index).into_bytes());
    }
    let entry = BcEntry {
        name: Span::of(&name),
        width,
        height,
        mips,
        format,
        normal: (f[5] == b"normal_xy") as u32,
        top_hash,
        file_hash,
        mip_hashes: hashes,
    };
    Ok((name, entry))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_bc_load(
    modroot: Span,
    enabled: i32,
    old_identity: Span,
    ctx: *mut c_void,
    cb: BcCall,
) -> i32 {
    let modroot = bytes(modroot);
    let identity = normalize_root(modroot);
    if enabled != 0 {
        if identity == bytes(old_identity) {
            return 1;
        }
        bc_error(
            ctx,
            cb,
            b"BC catalog already loaded from a different mod root",
        );
        return 0;
    }
    if bc_op(ctx, cb, 101, &[]).is_none() {
        return 0;
    }
    if modroot.is_empty() {
        bc_error(ctx, cb, b"empty BC mod root");
        return 0;
    }
    let mut directory = modroot.to_vec();
    if !directory.ends_with(b"\\") && !directory.ends_with(b"/") {
        directory.push(b'\\');
    }
    let Some(opened) = bc_op(ctx, cb, 102, &directory) else {
        return 0;
    };
    if opened.value == 0 {
        bc_error(ctx, cb, b"cannot open BC catalog");
        return 0;
    }
    let Some(header) = line(ctx, cb) else {
        bc_error(ctx, cb, b"invalid BC catalog header");
        return 0;
    };
    if header != b"A2FO_BC_CATALOG 2" {
        bc_error(ctx, cb, b"invalid BC catalog header");
        return 0;
    }
    let Some(count_line) = line(ctx, cb) else {
        bc_error(ctx, cb, b"invalid BC catalog header");
        return 0;
    };
    if !count_line.starts_with(b"COUNT ") {
        bc_error(ctx, cb, b"invalid BC catalog header");
        return 0;
    }
    if decimal(&count_line[6..]) != Some(407) {
        bc_error(ctx, cb, b"invalid BC catalog count");
        return 0;
    }
    for i in 0..407 {
        let Some(source) = line(ctx, cb) else {
            bc_error(ctx, cb, b"truncated BC catalog");
            return 0;
        };
        let (name, entry) = match parse_bc_entry(ctx, cb, i, &source) {
            Ok(value) => value,
            Err(message) => {
                bc_error(ctx, cb, &message);
                return 0;
            }
        };
        if bc_op(ctx, cb, 105, pod_bytes(&entry)).is_none() {
            return 0;
        }
        drop(name);
    }
    if line(ctx, cb).is_some() {
        bc_error(ctx, cb, b"extra BC catalog entries");
        return 0;
    }
    let publication = BcPublication {
        directory: Span::of(&directory),
        identity: Span::of(&identity),
    };
    let Some(done) = bc_op(ctx, cb, 106, pod_bytes(&publication)) else {
        return 0;
    };
    done.value as i32
}
fn basename(filename: &[u8]) -> &[u8] {
    let at = filename.iter().rposition(|&x| x == b'\\' || x == b'/');
    &filename[at.map_or(0, |v| v + 1)..]
}
fn entries<'a>(ptr: *const BcEntryView, count: usize) -> &'a [BcEntryView] {
    if count == 0 || ptr.is_null() {
        &[]
    } else {
        unsafe { slice::from_raw_parts(ptr, count) }
    }
}
fn join_bytes(parts: &[&[u8]]) -> Vec<u8> {
    let mut out = Vec::new();
    for p in parts {
        out.extend_from_slice(p);
    }
    out
}
fn canonical(
    ctx: *mut c_void,
    cb: BcCall,
    root: &[u8],
    entry: &BcEntryView,
) -> Result<BcImageView, Vec<u8>> {
    let name = bytes(entry.name);
    let path = join_bytes(&[root, b"Textures\\", name]);
    let request = BcReadRequest {
        path: Span::of(&path),
        max_size: 64 * 1024 * 1024,
    };
    let Some(file) = bc_op(ctx, cb, 108, pod_bytes(&request)) else {
        return Err(Vec::new());
    };
    let raw = bytes(file.span);
    if file.value == 0 || hash(raw, OFFSET) != entry.file_hash {
        return Err(join_bytes(&[
            b"canonical BC file missing or changed: ",
            name,
        ]));
    }
    let Some(parsed) = bc_op(ctx, cb, 109, &[]) else {
        return Err(Vec::new());
    };
    let image = parsed.image;
    let payload = bytes(image.bytes);
    let parse_error = bytes(image.parse_error);
    let hashes = if entry.mips == 0 || entry.mip_hashes.is_null() {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(entry.mip_hashes, entry.mips as usize) }
    };
    let top_ok = image.mip_count != 0
        && image.mips[0].slice_pitch as usize <= payload.len()
        && hash(&payload[..image.mips[0].slice_pitch as usize], OFFSET) == entry.top_hash;
    if image.hr < 0
        || image.format != entry.format
        || image.width != entry.width
        || image.height != entry.height
        || image.mip_count != entry.mips as usize
        || image.mip_count == 0
        || hashes.len() != image.mip_count
        || !top_ok
    {
        return Err(join_bytes(&[
            b"canonical BC header/payload mismatch: ",
            name,
            b": ",
            parse_error,
        ]));
    }
    let header = if raw.len() >= 88 && &raw[84..88] == b"DX10" {
        148
    } else {
        128
    };
    if raw.len() != header + payload.len() {
        return Err(join_bytes(&[
            b"canonical BC file has trailing data: ",
            name,
        ]));
    }
    for i in 0..image.mip_count {
        let mip = image.mips[i];
        let start = mip.offset;
        let end = start + mip.slice_pitch as usize;
        if end > payload.len() || hash(&payload[start..end], OFFSET) != hashes[i] {
            return Err(join_bytes(&[
                b"canonical BC mip hash mismatch: ",
                name,
                b" mip ",
                format!("{}", i).as_bytes(),
            ]));
        }
    }
    Ok(image)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_bc_asset(
    filename: Span,
    root: Span,
    ptr: *const BcEntryView,
    count: usize,
    ctx: *mut c_void,
    cb: BcCall,
) -> i32 {
    if filename.data.is_null() || count == 0 {
        return 0;
    }
    let name = basename(bytes(filename));
    if !name_ok(name) {
        return 0;
    }
    let folded = ascii_lower(name);
    for e in entries(ptr, count) {
        if ascii_lower(bytes(e.name)) != folded {
            continue;
        }
        match canonical(ctx, cb, bytes(root), e) {
            Ok(_) => {
                let published = BcPublishImage {
                    name: e.name,
                    normal: e.normal,
                };
                return bc_op(ctx, cb, 110, pod_bytes(&published)).is_some() as i32;
            }
            Err(message) => {
                if !message.is_empty() {
                    bc_error(ctx, cb, &message);
                }
                return 0;
            }
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_bc_transport(
    filename: Span,
    root: Span,
    ptr: *const BcEntryView,
    count: usize,
    ctx: *mut c_void,
    cb: BcCall,
) -> i32 {
    if filename.data.is_null() || count == 0 {
        return 0;
    }
    let name = basename(bytes(filename));
    if !name_ok(name) {
        return 0;
    }
    let folded = ascii_lower(name);
    for e in entries(ptr, count) {
        if ascii_lower(bytes(e.name)) == folded {
            let path = join_bytes(&[bytes(root), b".a2fo-dx11\\Textures\\", bytes(e.name)]);
            return bc_op(ctx, cb, 111, &path).is_some() as i32;
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_bc_match(
    width: u32,
    height: u32,
    mips: u32,
    base: Span,
    root: Span,
    ptr: *const BcEntryView,
    count: usize,
    ctx: *mut c_void,
    cb: BcCall,
) -> i32 {
    if base.data.is_null() || base.size == 0 || count == 0 {
        return 0;
    }
    let base_bytes = bytes(base);
    let supplied = hash(base_bytes, OFFSET);
    struct Selected {
        name: Vec<u8>,
        format: u32,
        normal: u32,
        alpha: u32,
        bytes: Vec<u8>,
        mips: [BcMip; 15],
    }
    let mut selected: Option<Selected> = None;
    for e in entries(ptr, count) {
        let mut w = e.width;
        let mut h = e.height;
        let mut level = 0usize;
        while level < e.mips as usize {
            if w == width && h == height {
                break;
            }
            w = (w / 2).max(1);
            h = (h / 2).max(1);
            level += 1;
        }
        if level == e.mips as usize || mips == 0 || mips > e.mips - level as u32 {
            continue;
        }
        let hashes = if e.mip_hashes.is_null() {
            continue;
        } else {
            unsafe { slice::from_raw_parts(e.mip_hashes, e.mips as usize) }
        };
        if hashes[level] != supplied {
            continue;
        }
        let image = match canonical(ctx, cb, bytes(root), e) {
            Ok(v) => v,
            Err(message) => {
                if !message.is_empty() {
                    bc_error(ctx, cb, &message);
                }
                return 0;
            }
        };
        let source = image.mips[level];
        if source.slice_pitch as usize != base.size {
            bc_error(
                ctx,
                cb,
                &join_bytes(&[b"canonical BC selected mip size mismatch: ", bytes(e.name)]),
            );
            return 0;
        }
        let payload = bytes(image.bytes);
        let start = source.offset;
        if start + base.size > payload.len() || &payload[start..start + base.size] != base_bytes {
            continue;
        }
        let end_mip = image.mips[level + mips as usize - 1];
        let end = end_mip.offset + end_mip.slice_pitch as usize;
        if end > payload.len() {
            return 0;
        }
        let chain = payload[start..end].to_vec();
        let mut chain_mips = [BcMip::default(); 15];
        for i in 0..mips as usize {
            let mut mip = image.mips[level + i];
            mip.offset -= start;
            chain_mips[i] = mip;
        }
        if let Some(previous) = &selected {
            if previous.format != e.format || previous.normal != e.normal || previous.bytes != chain
            {
                bc_error(
                    ctx,
                    cb,
                    &join_bytes(&[
                        b"ambiguous BC mip chain: ",
                        &previous.name,
                        b" / ",
                        bytes(e.name),
                    ]),
                );
                return 0;
            }
        } else {
            selected = Some(Selected {
                name: bytes(e.name).to_vec(),
                format: e.format,
                normal: e.normal,
                alpha: image.alpha,
                bytes: chain,
                mips: chain_mips,
            });
        }
    }
    let Some(s) = selected else { return 0 };
    let chain = BcChain {
        bytes: Span::of(&s.bytes),
        name: Span::of(&s.name),
        format: s.format,
        width,
        height,
        alpha: s.alpha,
        normal: s.normal,
        mip_count: mips as usize,
        mips: s.mips,
    };
    bc_op(ctx, cb, 112, pod_bytes(&chain)).is_some() as i32
}

// D3D8 state-block and surface decisions. The callback's pointers are borrowed
// only for this synchronous call; C++ keeps all COM and Binding ownership.
#[repr(C)]
#[derive(Clone, Copy)]
struct StateMask {
    render: [u8; 256],
    stage: [[u8; 33]; 8],
    transforms: [u8; 11],
    textures: [u8; 8],
    lights: [u8; 8],
    streams: [u8; 16],
    constants: [u8; 96],
    viewport: u8,
    material: u8,
    indices: u8,
    vertex_shader: u8,
    pixel_shader: u8,
    clip: u8,
}
impl Default for StateMask {
    fn default() -> Self {
        Self {
            render: [0; 256],
            stage: [[0; 33]; 8],
            transforms: [0; 11],
            textures: [0; 8],
            lights: [0; 8],
            streams: [0; 16],
            constants: [0; 96],
            viewport: 0,
            material: 0,
            indices: 0,
            vertex_shader: 0,
            pixel_shader: 0,
            clip: 0,
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct StateReply {
    hr: i32,
    src_pitch: i32,
    dst_pitch: i32,
    value: u32,
    other: u32,
    width: u32,
    height: u32,
    dst_width: u32,
    dst_height: u32,
    format: u32,
    levels: u32,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
    x: i32,
    y: i32,
    src_bits: *mut u8,
    dst_bits: *mut u8,
    mask: StateMask,
}
type StateCall = unsafe extern "C" fn(*mut c_void, u32, u32, u32, *mut StateReply) -> i32;
fn state_op(ctx: *mut c_void, cb: StateCall, id: u32, a: u32, b: u32) -> Option<StateReply> {
    let mut r = StateReply::default();
    (unsafe { cb(ctx, id, a, b, &mut r) } != 0).then_some(r)
}
fn state_all_mask(ctx: *mut c_void, cb: StateCall) -> bool {
    let mut r = StateReply::default();
    r.mask.render.fill(1);
    for stage in &mut r.mask.stage {
        stage.fill(1);
    }
    r.mask.transforms.fill(1);
    r.mask.textures.fill(1);
    r.mask.lights.fill(1);
    r.mask.streams.fill(1);
    r.mask.constants.fill(1);
    r.mask.viewport = 1;
    r.mask.material = 1;
    r.mask.indices = 1;
    r.mask.vertex_shader = 1;
    r.mask.pixel_shader = 1;
    r.mask.clip = 1;
    unsafe { cb(ctx, 107, 0, 0, &mut r) != 0 }
}
fn state_copy(ctx: *mut c_void, cb: StateCall, m: &StateMask) -> bool {
    for i in 0..256 {
        if m.render[i] != 0 && state_op(ctx, cb, 109, 1, i as u32).is_none() {
            return false;
        }
    }
    for s in 0..8 {
        for t in 0..33 {
            if m.stage[s][t] != 0 && state_op(ctx, cb, 109, 2, (s * 33 + t) as u32).is_none() {
                return false;
            }
        }
        if m.textures[s] != 0 && state_op(ctx, cb, 109, 3, s as u32).is_none() {
            return false;
        }
        if m.lights[s] != 0 && state_op(ctx, cb, 109, 4, s as u32).is_none() {
            return false;
        }
    }
    for i in 0..11 {
        if m.transforms[i] != 0 && state_op(ctx, cb, 109, 5, i as u32).is_none() {
            return false;
        }
    }
    for (flag, id) in [(m.material, 6), (m.viewport, 7)] {
        if flag != 0 && state_op(ctx, cb, 109, id, 0).is_none() {
            return false;
        }
    }
    for i in 0..16 {
        if m.streams[i] != 0 && state_op(ctx, cb, 109, 8, i as u32).is_none() {
            return false;
        }
    }
    for (flag, id) in [(m.indices, 9), (m.vertex_shader, 10), (m.pixel_shader, 11)] {
        if flag != 0 && state_op(ctx, cb, 109, id, 0).is_none() {
            return false;
        }
    }
    for i in 0..96 {
        if m.constants[i] != 0 && state_op(ctx, cb, 109, 12, i as u32).is_none() {
            return false;
        }
    }
    m.clip == 0 || state_op(ctx, cb, 109, 13, 0).is_some()
}
fn copy_rects(ctx: *mut c_void, cb: StateCall, count: u32, flags: u32, invalid: i32) -> i32 {
    let q = match state_op(ctx, cb, 117, 0, 0) {
        Some(v) => v,
        None => return invalid,
    };
    if q.value == 0 || (count != 0 && flags & 1 == 0) || (count == 0 && flags & 1 != 0) {
        return invalid;
    }
    if q.dst_pitch == 0 {
        return state_op(ctx, cb, 124, 3, 0).map_or(invalid, |v| v.hr);
    }
    if q.other & 3 == 3 {
        return state_op(ctx, cb, 118, count, 0).map_or(invalid, |v| v.hr);
    }
    if q.other & 2 != 0 {
        return state_op(ctx, cb, 124, 4, 0).map_or(invalid, |v| v.hr);
    }
    let src = match state_op(ctx, cb, 119, 0, 0) {
        Some(v) => v,
        None => return invalid,
    };
    if src.hr < 0 {
        return state_op(ctx, cb, 124, 5, src.hr as u32).map_or(invalid, |v| v.hr);
    }
    let dst = match state_op(ctx, cb, 120, 0, 0) {
        Some(v) => v,
        None => return invalid,
    };
    if dst.hr < 0 {
        let _ = state_op(ctx, cb, 121, 0, 0);
        return state_op(ctx, cb, 124, 6, dst.hr as u32).map_or(invalid, |v| v.hr);
    }
    let bc = matches!(
        q.format,
        0x31545844 | 0x32545844 | 0x33545844 | 0x34545844 | 0x35545844
    );
    let block = if bc {
        if q.format == 0x31545844 { 8 } else { 16 }
    } else {
        q.levels
    };
    let mut result = 0;
    for i in 0..if count == 0 { 1 } else { count } {
        let raw = match state_op(ctx, cb, 123, i, 0) {
            Some(v) => v,
            None => {
                result = invalid;
                break;
            }
        };
        let (l, t, r, b) = if count == 0 {
            (0, 0, q.width as i32, q.height as i32)
        } else {
            (raw.left, raw.top, raw.right, raw.bottom)
        };
        let (x, y) = if flags & 2 == 0 {
            (l, t)
        } else {
            (raw.x, raw.y)
        };
        if l < 0
            || t < 0
            || r <= l
            || b <= t
            || r > q.width as i32
            || b > q.height as i32
            || x < 0
            || y < 0
            || x.wrapping_add(r.wrapping_sub(l)) > q.dst_width as i32
            || y.wrapping_add(b.wrapping_sub(t)) > q.dst_height as i32
            || (bc && (l % 4 != 0 || t % 4 != 0 || x % 4 != 0 || y % 4 != 0))
        {
            result = invalid;
            break;
        }
        let rows = if bc { (b - t + 3) / 4 } else { b - t };
        let nbytes = if bc {
            ((r - l + 3) / 4) as u32 * block
        } else {
            (r - l) as u32 * block
        };
        let src_off = (if bc { t / 4 } else { t })
            .wrapping_mul(src.src_pitch)
            .wrapping_add((if bc { l / 4 } else { l }).wrapping_mul(block as i32));
        let dst_off = (if bc { y / 4 } else { y })
            .wrapping_mul(dst.dst_pitch)
            .wrapping_add((if bc { x / 4 } else { x }).wrapping_mul(block as i32));
        for row in 0..rows {
            let si = src_off.wrapping_add(row.wrapping_mul(src.src_pitch));
            let di = dst_off.wrapping_add(row.wrapping_mul(dst.dst_pitch));
            unsafe {
                core::ptr::copy(
                    src.src_bits.wrapping_offset(si as isize),
                    dst.dst_bits.wrapping_offset(di as isize),
                    nbytes as usize,
                );
            }
        }
    }
    let _ = state_op(ctx, cb, 121, 0, 0);
    let end = state_op(ctx, cb, 122, 0, 0).map_or(invalid, |v| v.hr);
    if result < 0 { result } else { end }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_state_run(
    method: u32,
    arg0: u32,
    arg1: u32,
    ctx: *mut c_void,
    cb: StateCall,
) -> i32 {
    let invalid = 0x8876086cu32 as i32;
    let oom = 0x8007000eu32 as i32;
    let q = if method <= 10 {
        match state_op(ctx, cb, 101, method, arg0) {
            Some(v) => v,
            None => return oom,
        }
    } else {
        StateReply::default()
    };
    match method {
        1 => {
            if q.value != 0 {
                return invalid;
            }
            state_op(ctx, cb, 102, 0, 0).map_or(oom, |v| v.hr)
        }
        2 => {
            if arg0 == 0 || q.value == 0 {
                return invalid;
            }
            for i in 0..8 {
                if q.mask.textures[i] == 0 && state_op(ctx, cb, 103, 1, i as u32).is_none() {
                    return oom;
                }
            }
            for i in 0..16 {
                if q.mask.streams[i] == 0 && state_op(ctx, cb, 103, 2, i as u32).is_none() {
                    return oom;
                }
            }
            if q.mask.indices == 0 && state_op(ctx, cb, 103, 3, 0).is_none() {
                return oom;
            }
            if q.mask.vertex_shader == 0 && state_op(ctx, cb, 103, 4, 0).is_none() {
                return oom;
            }
            let token = q.width;
            if state_op(ctx, cb, 104, token.wrapping_add(1), 0).is_none() {
                return oom;
            }
            if token == 0 {
                return oom;
            }
            if state_op(ctx, cb, 105, token, 0).is_none() {
                return oom;
            }
            0
        }
        3 | 4 => {
            if q.value != 0 || q.other == 0 {
                return invalid;
            }
            if state_copy(ctx, cb, &q.mask) {
                0
            } else {
                invalid
            }
        }
        5 => {
            if q.other == 0 {
                return invalid;
            }
            state_op(ctx, cb, 110, arg0, 0).map_or(invalid, |v| v.hr)
        }
        6 => {
            if arg1 == 0 || q.value != 0 {
                return invalid;
            }
            if arg0 != 1 {
                return state_op(ctx, cb, 124, 1, 0).map_or(invalid, |v| v.hr);
            }
            if state_op(ctx, cb, 106, 0, 0).is_none() {
                return oom;
            }
            if !state_all_mask(ctx, cb) {
                return oom;
            }
            let token = q.width;
            if state_op(ctx, cb, 104, token.wrapping_add(1), 0).is_none() {
                return oom;
            }
            if token == 0 {
                return oom;
            }
            if state_op(ctx, cb, 108, token, 0).is_none() {
                return oom;
            }
            0
        }
        7 | 8 => {
            if arg0 == 0 {
                return invalid;
            }
            state_op(ctx, cb, if method == 7 { 111 } else { 112 }, method, 0)
                .map_or(invalid, |v| v.hr)
        }
        9 | 10 => {
            if arg0 >= 6 || arg1 == 0 {
                return invalid;
            }
            state_op(ctx, cb, if method == 9 { 111 } else { 112 }, method, arg0)
                .map_or(invalid, |v| v.hr)
        }
        11 => {
            let t = match state_op(ctx, cb, 113, 0, 0) {
                Some(v) => v,
                None => return invalid,
            };
            if t.value == 0 {
                return invalid;
            }
            if t.other == 0 {
                return state_op(ctx, cb, 124, 2, 0).map_or(invalid, |v| v.hr);
            }
            for i in 0..t.levels {
                let lock = match state_op(ctx, cb, 114, i, 0) {
                    Some(v) => v,
                    None => return invalid,
                };
                if lock.value != 0 {
                    return invalid;
                }
                if state_op(ctx, cb, 115, i, 0).is_none() {
                    return invalid;
                }
                let upload = match state_op(ctx, cb, 116, i, 0) {
                    Some(v) => v,
                    None => return invalid,
                };
                if upload.hr < 0 {
                    return upload.hr;
                }
            }
            0
        }
        12 => copy_rects(ctx, cb, arg0, arg1, invalid),
        13 => state_op(ctx, cb, 124, 7, 0).map_or(invalid, |v| v.hr),
        _ => invalid,
    }
}
