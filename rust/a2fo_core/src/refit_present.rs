//! Private presentation bridge for A2FORefitMenus. The public C++ model stays
//! on the native side; byte strings and pixel buffers are borrowed per call.

// Keep the legacy field order, bounded indices and ABI-shaped arguments auditable.
#![allow(
    clippy::too_many_arguments,
    clippy::needless_range_loop,
    clippy::type_complexity,
    clippy::approx_constant
)]
use alloc::{format, vec, vec::Vec};
#[cfg(target_arch = "x86")]
use core::arch::x86::*;
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::*;
use core::{ffi::c_void, ptr, slice};

unsafe extern "C" {
    fn roundf(value: f32) -> f32;
    fn round(value: f64) -> f64;
    fn sinf(value: f32) -> f32;
    fn cosf(value: f32) -> f32;
    fn fmod(value: f64, divisor: f64) -> f64;
    fn sin(value: f64) -> f64;
    fn cos(value: f64) -> f64;
}
fn round_f32(value: f32) -> f32 {
    unsafe { roundf(value) }
}
fn round_f64(value: f64) -> f64 {
    unsafe { round(value) }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "sse2")]
unsafe fn shade_native_sse2(
    source: &[u32],
    target: &mut [u32],
    width: usize,
    height: usize,
    factors: &[u16],
    brightnesses: &[i32],
) {
    unsafe {
        let zero = _mm_setzero_si128();
        let divisor = _mm_set1_epi16(5243);
        let rgb_mask = _mm_set1_epi32(0x00ffffff);
        for y in 0..height {
            let row = y * width;
            let mut x = 0;
            while x + 3 < width {
                let colors = _mm_loadu_si128(source.as_ptr().add(row + x) as *const __m128i);
                let low = _mm_unpacklo_epi8(colors, zero);
                let high = _mm_unpackhi_epi8(colors, zero);
                let low_factors = _mm_loadu_si128(factors.as_ptr().add(x * 4) as *const __m128i);
                let high_factors =
                    _mm_loadu_si128(factors.as_ptr().add((x + 2) * 4) as *const __m128i);
                let low_product = _mm_mullo_epi16(low, low_factors);
                let high_product = _mm_mullo_epi16(high, high_factors);
                let low_quotient = _mm_srli_epi16(_mm_mulhi_epu16(low_product, divisor), 3);
                let high_quotient = _mm_srli_epi16(_mm_mulhi_epu16(high_product, divisor), 3);
                let shaded = _mm_and_si128(_mm_packus_epi16(low_quotient, high_quotient), rgb_mask);
                _mm_storeu_si128(target.as_mut_ptr().add(row + x) as *mut __m128i, shaded);
                x += 4;
            }
            while x < width {
                let i = row + x;
                let color = source[i];
                let light = brightnesses[x] as u32;
                target[i] = if light == 100 {
                    color & 0x00ffffff
                } else {
                    ((((color >> 16) & 255) * light / 100) << 16)
                        | ((((color >> 8) & 255) * light / 100) << 8)
                        | ((color & 255) * light / 100)
                };
                x += 1;
            }
        }
    }
}

type TextSink = unsafe extern "C" fn(*mut c_void, i32, *const u8, usize) -> i32;

fn input<'a>(bytes: *const u8, length: usize) -> Option<&'a [u8]> {
    if length == 0 {
        Some(&[])
    } else if bytes.is_null() {
        None
    } else {
        Some(unsafe { slice::from_raw_parts(bytes, length) })
    }
}

fn trimmed(mut bytes: &[u8]) -> &[u8] {
    while bytes.first().is_some_and(u8::is_ascii_whitespace) {
        bytes = &bytes[1..];
    }
    while bytes.last().is_some_and(u8::is_ascii_whitespace) {
        bytes = &bytes[..bytes.len() - 1];
    }
    bytes
}

fn append(to: &mut Vec<u8>, bytes: &[u8]) {
    if !to.is_empty() {
        to.push(b' ');
    }
    to.extend_from_slice(bytes);
}

fn flush(para: &mut Vec<u8>, out: &mut Vec<Vec<u8>>) {
    if !para.is_empty() {
        out.push(core::mem::take(para));
    }
}

fn hint_prefix(line: &[u8]) -> bool {
    line.len() >= 4 && line[..4].eq_ignore_ascii_case(b"hint")
}

struct Objectives {
    title: Vec<u8>,
    briefing: Vec<Vec<u8>>,
    objectives: Vec<Vec<u8>>,
    hint: Vec<u8>,
}

fn parse_objectives(mut text: &[u8]) -> Option<Objectives> {
    if text.len() > 64 * 1024 {
        return None;
    }
    let mut parsed = Objectives {
        title: Vec::new(),
        briefing: Vec::new(),
        objectives: Vec::new(),
        hint: Vec::new(),
    };
    let mut para = Vec::new();
    let mut in_hint = false;
    while !text.is_empty() {
        let end = text.iter().position(|&b| b == b'\n');
        let line = trimmed(&text[..end.unwrap_or(text.len())]);
        text = end.map_or(&[][..], |n| &text[n + 1..]);
        if parsed.title.is_empty() {
            if !line.is_empty() {
                parsed.title.extend_from_slice(line);
            }
            continue;
        }
        if line.first() == Some(&b'*') {
            flush(&mut para, &mut parsed.briefing);
            parsed.objectives.push(trimmed(&line[1..]).to_vec());
            continue;
        }
        if line.is_empty() {
            if in_hint {
                in_hint = false;
            } else {
                flush(&mut para, &mut parsed.briefing);
            }
            continue;
        }
        if hint_prefix(line) {
            flush(&mut para, &mut parsed.briefing);
            let n = line
                .iter()
                .take_while(|&&b| b.is_ascii_alphabetic() || b == b':')
                .count();
            parsed.hint = trimmed(&line[n..]).to_vec();
            in_hint = true;
            continue;
        }
        if in_hint {
            parsed.hint.push(b' ');
            parsed.hint.extend_from_slice(line);
            continue;
        }
        if !parsed.objectives.is_empty() {
            append(&mut parsed.hint, line);
            continue;
        }
        append(&mut para, line);
    }
    flush(&mut para, &mut parsed.briefing);
    if parsed.title.is_empty() {
        None
    } else {
        Some(parsed)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_parse_objectives(
    bytes: *const u8,
    length: usize,
    sink: Option<TextSink>,
    context: *mut c_void,
) -> i32 {
    {
        let Some(text) = input(bytes, length) else {
            return 0;
        };
        let Some(sink) = sink else {
            return 0;
        };
        let Some(parsed) = parse_objectives(text) else {
            return 0;
        };
        let emit =
            |field, value: &[u8]| unsafe { sink(context, field, value.as_ptr(), value.len()) != 0 };
        if !emit(0, &parsed.title) {
            return 0;
        }
        for part in &parsed.briefing {
            if !emit(1, part) {
                return 0;
            }
        }
        for objective in &parsed.objectives {
            if !emit(2, objective) {
                return 0;
            }
        }
        i32::from(emit(3, &parsed.hint))
    }
}

fn sanitize_map(input: &[u8]) -> Option<&[u8]> {
    if input.is_empty() || input.len() > 260 {
        return None;
    }
    if input
        .split(|&b| b == b'/' || b == b'\\')
        .any(|part| part == b"..")
    {
        return None;
    }
    let mut stem = input.rsplit(|&b| b == b'/' || b == b'\\').next()?;
    if stem.len() >= 4
        && stem[stem.len() - 4] == b'.'
        && stem[stem.len() - 3..].eq_ignore_ascii_case(b"bzn")
    {
        stem = &stem[..stem.len() - 4];
    }
    if stem.is_empty()
        || stem.len() > 128
        || !stem
            .iter()
            .all(|b| b.is_ascii_alphanumeric() || matches!(*b, b'_' | b'-' | b' '))
    {
        return None;
    }
    Some(stem)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_sanitize_map(
    bytes: *const u8,
    length: usize,
    output: *mut u8,
    capacity: usize,
) -> usize {
    {
        let Some(input) = input(bytes, length) else {
            return 0;
        };
        let Some(stem) = sanitize_map(input) else {
            return 0;
        };
        if output.is_null() || capacity < stem.len() {
            return 0;
        }
        unsafe {
            ptr::copy_nonoverlapping(stem.as_ptr(), output, stem.len());
        }
        stem.len()
    }
}

fn u16_at(bytes: &[u8], at: usize) -> u16 {
    u16::from_le_bytes([bytes[at], bytes[at + 1]])
}
fn u32_at(bytes: &[u8], at: usize) -> u32 {
    u32::from_le_bytes(bytes[at..at + 4].try_into().unwrap())
}

fn decode_bmp(bytes: &[u8]) -> Option<(i32, i32, Vec<u32>)> {
    if bytes.len() < 54 || bytes.len() > 64 * 1024 * 1024 || &bytes[..2] != b"BM" {
        return None;
    }
    let offset = u32_at(bytes, 10) as u64;
    let dib_size = u32_at(bytes, 14) as u64;
    let width = u32_at(bytes, 18) as i32;
    let signed_height = u32_at(bytes, 22) as i32;
    let height = i64::from(signed_height).abs();
    let depth = u16_at(bytes, 28);
    if dib_size < 40
        || u16_at(bytes, 26) != 1
        || !matches!(depth, 8 | 24 | 32)
        || u32_at(bytes, 30) != 0
        || !(1..=3840).contains(&width)
        || !(1..=2160).contains(&height)
        || width as u64 * height as u64 > 8_000_000
        || offset < 14 + dib_size
    {
        return None;
    }
    let palette_start = 14 + dib_size;
    let palette_size = if depth == 8 {
        let n = u32_at(bytes, 46);
        if n == 0 { 256 } else { n }
    } else {
        0
    };
    if palette_size > 256 || (depth == 8 && palette_start + palette_size as u64 * 4 > offset) {
        return None;
    }
    let stride = (width as u64 * depth as u64).div_ceil(32) * 4;
    if offset + stride * height as u64 > bytes.len() as u64 {
        return None;
    }
    let mut pixels = vec![0; width as usize * height as usize];
    for y in 0..height as usize {
        let row = offset as usize + y * stride as usize;
        let dest_y = if signed_height > 0 {
            height as usize - y - 1
        } else {
            y
        };
        for x in 0..width as usize {
            let p = if depth == 8 {
                let index = bytes[row + x] as u32;
                if index >= palette_size {
                    return None;
                }
                palette_start as usize + index as usize * 4
            } else {
                row + x * (depth as usize / 8)
            };
            pixels[dest_y * width as usize + x] =
                ((bytes[p + 2] as u32) << 16) | ((bytes[p + 1] as u32) << 8) | bytes[p] as u32;
        }
    }
    Some((width, height as i32, pixels))
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_decode_bmp(
    bytes: *const u8,
    length: usize,
    pixels: *mut u32,
    capacity: usize,
    width: *mut i32,
    height: *mut i32,
) -> i32 {
    {
        let Some(input) = input(bytes, length) else {
            return 0;
        };
        let Some((w, h, decoded)) = decode_bmp(input) else {
            return 0;
        };
        if width.is_null() || height.is_null() {
            return 0;
        }
        unsafe {
            *width = w;
            *height = h;
        }
        if !pixels.is_null() {
            if capacity < decoded.len() {
                return 0;
            }
            unsafe {
                ptr::copy_nonoverlapping(decoded.as_ptr(), pixels, decoded.len());
            }
        }
        1
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    data: *const u8,
    length: usize,
}
type Read = unsafe extern "C" fn(*mut c_void, i32, *const u8, usize) -> i32;

fn full_path(root: &[u8], relative: &[u8]) -> Vec<u8> {
    let mut result = Vec::with_capacity(root.len() + relative.len() + 1);
    result.extend_from_slice(root);
    if !matches!(root.last(), Some(b'/' | b'\\')) {
        result.push(b'/');
    }
    result.extend_from_slice(relative);
    result
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_load_content(
    roots: *const Bytes,
    root_count: usize,
    map: *const u8,
    map_length: usize,
    windows_images: i32,
    read: Option<Read>,
    context: *mut c_void,
    show_preview: *mut i32,
) -> i32 {
    {
        let Some(map) = input(map, map_length) else {
            return 0;
        };
        let Some(read) = read else {
            return 0;
        };
        if show_preview.is_null() || (root_count != 0 && roots.is_null()) {
            return 0;
        }
        unsafe {
            *show_preview = 0;
        }
        let roots = if root_count == 0 {
            &[][..]
        } else {
            unsafe { slice::from_raw_parts(roots, root_count) }
        };
        let mut loaded = false;
        let extensions: &[&[u8]] = if windows_images != 0 {
            &[b".png", b".bmp", b".jpg", b".jpeg"]
        } else {
            &[b".bmp"]
        };
        let attempt = |root: &[u8], relative: &[u8], kind: i32| -> bool {
            if root.is_empty() {
                return false;
            }
            let path = full_path(root, relative);
            unsafe { read(context, kind, path.as_ptr(), path.len()) != 0 }
        };
        if let Some(stem) = sanitize_map(map) {
            let mut text_loaded = false;
            for root in roots.iter().rev() {
                if text_loaded {
                    break;
                }
                let Some(root) = input(root.data, root.length) else {
                    return 0;
                };
                for part in [b"a".as_slice(), b"main", b"primary", b"main_objectives"] {
                    let mut rel = b"objectives/".to_vec();
                    rel.extend_from_slice(stem);
                    rel.push(b'_');
                    rel.extend_from_slice(part);
                    rel.extend_from_slice(b".txt");
                    if attempt(root, &rel, 0) {
                        loaded = true;
                        text_loaded = true;
                        break;
                    }
                }
            }
            for root in roots.iter().rev() {
                let Some(root) = input(root.data, root.length) else {
                    return 0;
                };
                let mut found = false;
                for extension in extensions {
                    let mut rel = b"bzn/".to_vec();
                    rel.extend_from_slice(stem);
                    rel.extend_from_slice(extension);
                    if attempt(root, &rel, 1) {
                        loaded = true;
                        found = true;
                        unsafe {
                            *show_preview = 1;
                        }
                        break;
                    }
                }
                if found {
                    break;
                }
            }
        }
        for root in roots.iter().rev() {
            let Some(root) = input(root.data, root.length) else {
                return 0;
            };
            let mut found = false;
            for extension in extensions {
                let mut rel = b"RefitScene/Loading/background".to_vec();
                rel.extend_from_slice(extension);
                if attempt(root, &rel, 2) {
                    loaded = true;
                    found = true;
                    break;
                }
            }
            if found {
                break;
            }
        }
        i32::from(loaded)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Rect {
    x: i32,
    y: i32,
    width: i32,
    height: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct MenuEntry {
    caption: Bytes,
    visible: u8,
    enabled: u8,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MenuLayout {
    title: Rect,
    rows: [Rect; 7],
    footer: Rect,
    scale: f32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Artwork {
    width: i32,
    height: i32,
    pixels: *const u32,
    pixel_count: usize,
}
type MenuDraw = unsafe extern "C" fn(
    *mut c_void,
    i32,
    Rect,
    u32,
    i32,
    i32,
    *const u8,
    usize,
    *const u32,
    usize,
) -> i32;

fn paint_menu(
    width: i32,
    height: i32,
    entries: &[MenuEntry; 7],
    version: &[u8],
    hot: i32,
    pressed: i32,
    artwork: Artwork,
    has_sse2: bool,
    draw: MenuDraw,
    context: *mut c_void,
) -> bool {
    let geometry = menu_layout(width, height, entries);
    if geometry.scale <= 0.0 {
        return false;
    }
    let call = |op: i32,
                rect: Rect,
                color: u32,
                size: i32,
                flags: i32,
                text: &[u8],
                pixels: &[u32]|
     -> bool {
        unsafe {
            draw(
                context,
                op,
                rect,
                color,
                size,
                flags,
                text.as_ptr(),
                text.len(),
                pixels.as_ptr(),
                pixels.len(),
            ) != 0
        }
    };
    let empty = Rect::default();
    let fill = |r, color| call(0, r, color, 0, 0, &[], &[]);
    let label = |r, text: &[u8], size, color, flags| call(1, r, color, size, flags, text, &[]);
    let pixels = |r, data: &[u32], alpha| call(if alpha { 3 } else { 2 }, r, 0, 0, 0, &[], data);
    let fade = |r: Rect, accent: u32, opacity: i32| -> bool {
        if r.width <= 0 || r.height <= 0 {
            return true;
        }
        let Some(count) = (r.width as usize).checked_mul(r.height as usize) else {
            return false;
        };
        let mut data = Vec::new();
        if data.try_reserve_exact(count).is_err() {
            return false;
        }
        data.resize(count, 0);
        for x in 0..r.width as usize {
            let alpha = opacity as u32 * (r.width as usize - x) as u32 / r.width as u32;
            let color = (alpha << 24)
                | ((((accent >> 16) & 255) * alpha / 255) << 16)
                | ((((accent >> 8) & 255) * alpha / 255) << 8)
                | ((accent & 255) * alpha / 255);
            for y in 0..r.height as usize {
                data[y * r.width as usize + x] = color;
            }
        }
        pixels(r, &data, true)
    };
    let valid_art = artwork.width > 0
        && artwork.height > 0
        && !artwork.pixels.is_null()
        && (artwork.width as u64 * artwork.height as u64) == artwork.pixel_count as u64;
    if valid_art {
        let source = unsafe { slice::from_raw_parts(artwork.pixels, artwork.pixel_count) };
        let Some(count) = (width as usize).checked_mul(height as usize) else {
            return false;
        };
        let mut data = Vec::new();
        if data.try_reserve_exact(count).is_err() {
            return false;
        }
        data.resize(count, 0u32);
        let scale =
            (width as f64 / artwork.width as f64).max(height as f64 / artwork.height as f64);
        let crop_width = width as f64 / scale;
        let crop_height = height as f64 / scale;
        let origin_x = (artwork.width as f64 - crop_width) * 0.5;
        let origin_y = (artwork.height as f64 - crop_height) * 0.5;
        let fade_start = geometry.title.x;
        let fade_end =
            geometry.title.x + geometry.title.width + (48).max((80.0 * geometry.scale) as i32);
        let native_size = artwork.width == width && artwork.height == height;
        let x_step = crop_width / width as f64;
        let y_step = crop_height / height as f64;
        let fade_width = (1).max(fade_end - fade_start);
        let mut x_samples = Vec::new();
        let mut brightnesses = Vec::new();
        if x_samples.try_reserve_exact(width as usize).is_err()
            || brightnesses.try_reserve_exact(width as usize).is_err()
        {
            return false;
        }
        for x in 0..width {
            if !native_size {
                let at = (origin_x + (x as f64 + 0.5) * x_step - 0.5)
                    .clamp(0.0, (artwork.width - 1) as f64);
                let first = at as i32;
                x_samples.push((
                    first,
                    (first + 1).min(artwork.width - 1),
                    round_f64((at - first as f64) * 256.0) as i32,
                ));
            }
            let blend = ((x - fade_start) as f64 / fade_width as f64).clamp(0.0, 1.0);
            brightnesses.push(28 + (72.0 * blend) as i32);
        }
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        let accelerated = if native_size && has_sse2 {
            let mut factors = Vec::new();
            let Some(factor_count) = (width as usize).checked_mul(4) else {
                return false;
            };
            if factors.try_reserve_exact(factor_count).is_err() {
                return false;
            }
            for &light in &brightnesses {
                factors.extend_from_slice(&[light as u16; 4]);
            }
            unsafe {
                shade_native_sse2(
                    source,
                    &mut data,
                    width as usize,
                    height as usize,
                    &factors,
                    &brightnesses,
                );
            }
            true
        } else {
            false
        };
        #[cfg(not(any(target_arch = "x86", target_arch = "x86_64")))]
        let accelerated = false;
        if !accelerated {
            for y in 0..height as usize {
                if native_size {
                    for x in 0..width as usize {
                        let i = y * width as usize + x;
                        let color = source[i];
                        let light = brightnesses[x] as u32;
                        data[i] = if light == 100 {
                            color & 0x00ff_ffff
                        } else {
                            ((((color >> 16) & 255) * light / 100) << 16)
                                | ((((color >> 8) & 255) * light / 100) << 8)
                                | ((color & 255) * light / 100)
                        };
                    }
                    continue;
                }
                let at = (origin_y + (y as f64 + 0.5) * y_step - 0.5)
                    .clamp(0.0, (artwork.height - 1) as f64);
                let fy0 = at as usize;
                let fy1 = (fy0 + 1).min(artwork.height as usize - 1);
                let fy = round_f64((at - fy0 as f64) * 256.0) as i32;
                let first_row = fy0 * artwork.width as usize;
                let second_row = fy1 * artwork.width as usize;
                for x in 0..width as usize {
                    let (first, second, fraction) = x_samples[x];
                    let a = source[first_row + first as usize];
                    let b = source[first_row + second as usize];
                    let c = source[second_row + first as usize];
                    let d = source[second_row + second as usize];
                    let brightness = brightnesses[x];
                    let channel = |shift| -> u32 {
                        let left = ((a >> shift) & 255u32) as i32 * (256 - fy)
                            + ((c >> shift) & 255u32) as i32 * fy;
                        let right = ((b >> shift) & 255u32) as i32 * (256 - fy)
                            + ((d >> shift) & 255u32) as i32 * fy;
                        let mixed = (left * (256 - fraction) + right * fraction + 32768) >> 16;
                        (mixed * brightness / 100) as u32
                    };
                    data[y * width as usize + x] =
                        (channel(16) << 16) | (channel(8) << 8) | channel(0);
                }
            }
        }
        if !pixels(
            Rect {
                x: 0,
                y: 0,
                width,
                height,
            },
            &data,
            false,
        ) {
            return false;
        }
    } else {
        let bands = height.min(128);
        for band in 0..bands {
            let y0 = (band as i64 * height as i64 / bands as i64) as i32;
            let y1 = ((band + 1) as i64 * height as i64 / bands as i64) as i32;
            let r = 3 + 7 * band / (bands - 1).max(1);
            let g = 6 + 14 * band / (bands - 1).max(1);
            let b = 12 + 22 * band / (bands - 1).max(1);
            if !fill(
                Rect {
                    x: 0,
                    y: y0,
                    width,
                    height: y1 - y0,
                },
                ((r as u32) << 16) | ((g as u32) << 8) | b as u32,
            ) {
                return false;
            }
        }
    }
    let s = geometry.scale;
    let title = geometry.title;
    let star_rise = round_i32(40.0 * s);
    if !label(
        Rect {
            y: title.y - star_rise,
            height: star_rise,
            ..title
        },
        b"STAR TREK",
        round_i32(28.0 * s),
        0xe4eaf0,
        0,
    ) {
        return false;
    }
    let armada_left = title.x - round_i32(3.0 * s);
    if !label(
        Rect {
            x: armada_left,
            y: title.y - round_i32(10.0 * s),
            width: round_i32(570.0 * s),
            height: round_i32(80.0 * s),
        },
        b"ARMADA II",
        round_i32(68.0 * s),
        0xf2b23a,
        0,
    ) {
        return false;
    }
    if !fade(
        Rect {
            x: title.x,
            y: title.y + round_i32(74.0 * s),
            width: round_i32(260.0 * s).max(1),
            height: round_i32(2.0 * s).max(1),
        },
        0xf2a93b,
        255,
    ) {
        return false;
    }
    for (index, entry) in entries.iter().enumerate() {
        if entry.visible == 0 {
            continue;
        }
        let row = geometry.rows[index];
        let hovered = entry.enabled != 0 && hot == index as i32;
        let held = hovered && pressed == index as i32;
        if hovered {
            if !fade(row, 0xf2a93b, if held { 100 } else { 71 }) {
                return false;
            }
            let inset = round_i32(8.0 * s);
            if !fill(
                Rect {
                    x: row.x,
                    y: row.y + inset,
                    width: round_i32(4.0 * s).max(2),
                    height: row.height - 2 * inset,
                },
                0xf2a93b,
            ) {
                return false;
            }
        }
        let label_rect = Rect {
            x: row.x + round_i32((if hovered { 28.0 } else { 18.0 }) * s),
            y: row.y,
            width: row.width
                - round_i32((if hovered { 28.0 } else { 18.0 }) * s)
                - round_i32(24.0 * s),
            height: row.height,
        };
        let Some(caption) = input(entry.caption.data, entry.caption.length) else {
            return false;
        };
        if !label(
            label_rect,
            caption,
            round_i32(26.0 * s),
            if entry.enabled == 0 {
                0x525e6c
            } else if held {
                0xf2a93b
            } else if hovered {
                0xffffff
            } else {
                0xe4eaf0
            },
            4,
        ) {
            return false;
        }
    }
    let footer = geometry.footer;
    let brand = Rect {
        width: footer.width / 3,
        ..footer
    };
    if !label(brand, b"Fleet Operations", round_i32(13.0 * s), 0x8e9aa8, 2) {
        return false;
    }
    let foot_left = footer.x + footer.width;
    let version_left = foot_left.min(brand.x + brand.width + 4);
    if !version.is_empty()
        && version_left < foot_left
        && !label(
            Rect {
                x: version_left,
                y: footer.y,
                width: foot_left - version_left,
                height: footer.height,
            },
            version,
            round_i32(13.0 * s),
            0x8e9aa8,
            3,
        )
    {
        return false;
    }
    let _ = empty;
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_paint_menu(
    width: i32,
    height: i32,
    entries: *const MenuEntry,
    version: Bytes,
    hot: i32,
    pressed: i32,
    artwork: Artwork,
    has_sse2: i32,
    draw: Option<MenuDraw>,
    context: *mut c_void,
) -> i32 {
    {
        if width <= 0 || height <= 0 || entries.is_null() {
            return 0;
        }
        let Some(version) = input(version.data, version.length) else {
            return 0;
        };
        let Some(draw) = draw else {
            return 0;
        };
        let entries = unsafe { &*(entries as *const [MenuEntry; 7]) };
        i32::from(paint_menu(
            width,
            height,
            entries,
            version,
            hot,
            pressed,
            artwork,
            has_sse2 != 0,
            draw,
            context,
        ))
    }
}

fn contains(r: Rect, x: i32, y: i32) -> bool {
    r.width > 0
        && r.height > 0
        && x >= r.x
        && y >= r.y
        && (x as i64) < r.x as i64 + r.width as i64
        && (y as i64) < r.y as i64 + r.height as i64
}
fn round_i32(value: f32) -> i32 {
    round_f32(value) as i32
}
fn menu_layout(width: i32, height: i32, entries: &[MenuEntry; 7]) -> MenuLayout {
    let mut out = MenuLayout::default();
    if width < 80 || height < 100 {
        return out;
    }
    let scale = 1.0f32
        .min((width - 16) as f32 / 460.0)
        .min((height - 48) as f32 / 580.0);
    if scale <= 0.0 {
        return out;
    }
    out.scale = scale;
    let row_width = 1.max(round_i32(380.0 * scale));
    let row_height = 1.max(round_i32(54.0 * scale));
    let gap = 1.max(round_i32(12.0 * scale));
    let title_height = 1.max(round_i32(90.0 * scale));
    let left = round_i32(width as f32 * 0.08).max(round_i32(40.0 * scale));
    let visible = entries.iter().filter(|entry| entry.visible != 0).count() as i32;
    let total = title_height + visible * row_height + 0.max(visible - 1) * gap;
    let mut top = 0.max((height - total) / 2);
    out.title = Rect {
        x: left,
        y: top,
        width: row_width,
        height: title_height,
    };
    top += title_height;
    for (i, entry) in entries.iter().enumerate() {
        if entry.visible == 0 {
            continue;
        }
        out.rows[i] = Rect {
            x: left,
            y: top,
            width: row_width,
            height: row_height,
        };
        top += row_height + gap;
    }
    let footer_height = 1.max(round_i32(20.0 * scale));
    let footer_width = (width - 16).min(1.max(round_i32(400.0 * scale)));
    out.footer = Rect {
        x: width - 8 - footer_width,
        y: height - 8 - footer_height,
        width: footer_width,
        height: footer_height,
    };
    out
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_menu_layout(
    width: i32,
    height: i32,
    entries: *const MenuEntry,
    output: *mut MenuLayout,
) {
    if entries.is_null() || output.is_null() {
        return;
    }
    let entries = unsafe { &*(entries as *const [MenuEntry; 7]) };
    unsafe {
        *output = menu_layout(width, height, entries);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_menu_hit(
    layout: *const MenuLayout,
    entries: *const MenuEntry,
    x: i32,
    y: i32,
) -> i32 {
    if layout.is_null() || entries.is_null() {
        return -1;
    }
    let layout = unsafe { &*layout };
    let entries = unsafe { &*(entries as *const [MenuEntry; 7]) };
    entries
        .iter()
        .enumerate()
        .find(|(i, entry)| {
            entry.visible != 0 && entry.enabled != 0 && contains(layout.rows[*i], x, y)
        })
        .map_or(-1, |(i, _)| i as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_menu_next(
    entries: *const MenuEntry,
    current: i32,
    direction: i32,
) -> i32 {
    if entries.is_null() {
        return -1;
    }
    let entries = unsafe { &*(entries as *const [MenuEntry; 7]) };
    if direction == 0 {
        return if (0..7).contains(&current)
            && entries[current as usize].visible != 0
            && entries[current as usize].enabled != 0
        {
            current
        } else {
            -1
        };
    }
    let step = if direction > 0 { 1 } else { -1 };
    let mut index = if (0..7).contains(&current) {
        current
    } else if step > 0 {
        -1
    } else {
        0
    };
    for _ in 0..7 {
        index = (index + step + 7) % 7;
        if entries[index as usize].visible != 0 && entries[index as usize].enabled != 0 {
            return index;
        }
    }
    -1
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LoadingInput {
    kind: i32,
    show_preview: i32,
    player_count: usize,
    protected_regions: *const Rect,
    protected_count: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LoadingLayout {
    valid: i32,
    scale: f32,
    kicker: Rect,
    title: Rect,
    briefing: Rect,
    objectives: Rect,
    hint: Rect,
    preview: Rect,
    players: Rect,
    status: Rect,
    percentage: Rect,
}

fn px(scale: f32, value: i32) -> i32 {
    round_i32(scale * value as f32).max(1)
}
fn rect(x: i32, y: i32, width: i32, height: i32) -> Rect {
    Rect {
        x,
        y,
        width: width.max(0),
        height: height.max(0),
    }
}
fn overlaps(a: Rect, b: Rect) -> bool {
    a.width > 0
        && a.height > 0
        && b.width > 0
        && b.height > 0
        && a.x < b.x + b.width
        && b.x < a.x + a.width
        && a.y < b.y + b.height
        && b.y < a.y + a.height
}
fn free_band(x: i32, width: i32, top: i32, bottom: i32, obstacles: &[Rect]) -> (i32, i32) {
    let mut cuts = vec![top, bottom];
    for r in obstacles {
        if r.width <= 0 || r.height <= 0 || r.x >= x + width || r.x + r.width <= x {
            continue;
        }
        cuts.push(r.y.clamp(top, bottom));
        cuts.push((r.y + r.height).clamp(top, bottom));
    }
    cuts.sort_unstable();
    cuts.dedup();
    let mut best = (top, top);
    for pair in cuts.windows(2) {
        let candidate = Rect {
            x,
            y: pair[0],
            width,
            height: pair[1] - pair[0],
        };
        if candidate.height > best.1 - best.0 && obstacles.iter().all(|&r| !overlaps(candidate, r))
        {
            best = (candidate.y, candidate.y + candidate.height);
        }
    }
    best
}
fn loading_layout(
    width: i32,
    height: i32,
    input: LoadingInput,
    obstacles: &[Rect],
) -> LoadingLayout {
    let mut out = LoadingLayout::default();
    if width < 800 || height < 600 || width > 3840 || height > 2160 {
        return out;
    }
    let s = (width as f32 / 1920.0).min(height as f32 / 1080.0);
    let margin = px(s, 96).min(width * 6 / 100);
    let gap = px(s, 48);
    let line_h = px(s, 48);
    let mut status_y = height - margin - line_h - px(s, 18);
    loop {
        let mut moved = false;
        for &obstacle in obstacles {
            if obstacle.width < width / 2
                || obstacle.y < height / 2
                || !overlaps(
                    Rect {
                        x: margin,
                        y: status_y,
                        width: width - 2 * margin,
                        height: line_h,
                    },
                    obstacle,
                )
            {
                continue;
            }
            status_y = obstacle.y - line_h - px(s, 12);
            moved = true;
        }
        if !moved {
            break;
        }
    }
    let bottom = status_y - px(s, 24);
    let right = input.kind != 2 && (input.show_preview != 0 || input.player_count != 0);
    let top = margin * 9 / 10;
    if bottom <= top + 100 {
        return out;
    }
    let mut left_end = width - margin;
    let mut right_begin = margin;
    for &r in obstacles {
        if r.width <= 0 || r.height < (bottom - top) / 2 || r.y >= bottom || r.y + r.height <= top {
            continue;
        }
        if r.x > margin && r.x < left_end {
            left_end = r.x;
        }
        let edge = r.x + r.width;
        if edge < width - margin && edge > right_begin {
            right_begin = edge;
        }
    }
    let left_w = px(s, 820).min(left_end - margin - gap);
    let right_w = if right {
        px(s, 440)
            .min(width * 30 / 100)
            .min(width - margin - right_begin - gap)
    } else {
        0
    };
    if left_w < px(s, 250) || (right && right_w < px(s, 180)) {
        return out;
    }
    let (left_top, left_bottom) = free_band(margin, left_w, top, bottom, obstacles);
    let right_x = width - margin - right_w;
    let (right_top, right_bottom) = if right {
        free_band(right_x, right_w, top, bottom, obstacles)
    } else {
        (top, bottom)
    };
    if left_bottom - left_top < px(s, 210) {
        return out;
    }
    out.scale = s;
    out.kicker = rect(margin, left_top, left_w, px(s, 28));
    out.title = rect(
        margin,
        out.kicker.y + out.kicker.height + px(s, 10),
        left_w,
        px(s, 100),
    );
    let text_y = out.title.y + out.title.height + px(s, 30);
    if input.kind == 0 || input.kind == 2 {
        out.briefing = rect(margin, text_y, left_w, 0.max(left_bottom - text_y));
    }
    out.objectives = out.briefing;
    out.hint = out.briefing;
    if right {
        let players_count = input.player_count as i32;
        let header = px(s, 52);
        let preview_gap = px(s, 16);
        let minimum_preview = if input.show_preview != 0 {
            px(s, 48)
        } else {
            0
        };
        let row_h = if players_count != 0 {
            px(s, 84).min(
                (right_bottom
                    - right_top
                    - header
                    - minimum_preview
                    - if input.show_preview != 0 {
                        preview_gap
                    } else {
                        0
                    })
                    / players_count,
            )
        } else {
            0
        };
        if players_count != 0 && row_h < px(s, 72) {
            return LoadingLayout::default();
        }
        let rows = row_h * players_count;
        let side = if input.show_preview != 0 {
            right_w
                .min(px(s, 240))
                .min((right_bottom - right_top - header - rows - preview_gap).max(0))
        } else {
            0
        };
        if input.show_preview != 0 && side < px(s, 48) {
            return LoadingLayout::default();
        }
        if input.show_preview != 0 {
            out.preview = rect(right_x + right_w - side, right_top, side, side);
        }
        let players_y = if input.show_preview != 0 {
            out.preview.y + side + preview_gap
        } else {
            right_top
        };
        out.players = rect(
            right_x,
            players_y,
            right_w,
            (right_bottom - players_y).max(0),
        );
        if players_count != 0 && out.players.height < header + rows {
            return LoadingLayout::default();
        }
    }
    let mut status_end = width - margin;
    for &r in obstacles {
        if r.x > margin
            && overlaps(
                Rect {
                    x: margin,
                    y: status_y,
                    width: status_end - margin,
                    height: line_h,
                },
                r,
            )
        {
            status_end = status_end.min(r.x - gap);
        }
    }
    let percentage_w = px(s, 100);
    if status_end - margin < px(s, 220) + percentage_w {
        return LoadingLayout::default();
    }
    out.status = rect(
        margin,
        status_y,
        status_end - margin - percentage_w - px(s, 12),
        line_h,
    );
    out.percentage = rect(status_end - percentage_w, status_y, percentage_w, line_h);
    for &r in obstacles {
        for placed in [
            out.kicker,
            out.title,
            out.briefing,
            out.preview,
            out.players,
            out.status,
            out.percentage,
        ] {
            if overlaps(r, placed) {
                return LoadingLayout::default();
            }
        }
    }
    out.valid = 1;
    out
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_loading_layout(
    width: i32,
    height: i32,
    input: LoadingInput,
    output: *mut LoadingLayout,
) {
    if output.is_null() || (input.protected_count != 0 && input.protected_regions.is_null()) {
        return;
    }
    let obstacles = if input.protected_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(input.protected_regions, input.protected_count) }
    };
    unsafe {
        *output = loading_layout(width, height, input, obstacles);
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SetupControl {
    key: usize,
    kind: i32,
    source: Rect,
    text: Bytes,
    message: i32,
    visible: i32,
    enabled: i32,
    mode: i32,
    open: i32,
    down: i32,
    item_height: i32,
    pull: Rect,
    item_count: usize,
    items: *const Bytes,
    selected: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Placement {
    key: usize,
    source: Rect,
    target: Rect,
    kind: i32,
    item: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SetupLayout {
    valid: i32,
    scale: f32,
    title: Rect,
    players: Rect,
    map: Rect,
    preview: Rect,
    placement_count: usize,
}
fn scaled(s: f32, n: i32) -> i32 {
    round_i32(s * n as f32).max(1)
}
fn box_rect(x: i32, y: i32, w: i32, h: i32) -> Rect {
    Rect {
        x,
        y,
        width: w.max(1),
        height: h.max(1),
    }
}
fn good(r: Rect) -> bool {
    r.width > 0 && r.height > 0
}
fn title_label(c: &SetupControl) -> bool {
    c.kind == 2 && c.message == 0 && c.source.y < 30 && c.text.length != 0
}
fn heading_column(c: &SetupControl) -> i32 {
    if c.kind != 2
        || c.message != 0
        || c.source.x < 41
        || c.source.x >= 450
        || c.source.y <= 40
        || c.source.y >= 80
    {
        -1
    } else if c.source.x < 200 {
        0
    } else if c.source.x < 360 {
        1
    } else {
        2
    }
}
fn place(out: &mut Vec<Placement>, c: &SetupControl, target: Rect, item: i32) {
    out.push(Placement {
        key: c.key,
        source: c.source,
        target,
        kind: c.kind,
        item,
    });
}
fn setup_arrange(
    width: i32,
    height: i32,
    controls: &[SetupControl],
    popup_scroll: i32,
) -> (SetupLayout, Vec<Placement>) {
    let mut out = SetupLayout::default();
    let mut placed = Vec::new();
    if width < 480 || height < 360 {
        return (out, placed);
    }
    let margin = 8.max((width / 60).min(height / 34));
    let ah = height - 2 * margin;
    let aw = (width - 2 * margin).min(ah * 16 / 9 + 2 * margin);
    if aw < 440 || ah < 300 {
        return (out, placed);
    }
    let s = (aw as f32 / 1856.0).min(ah as f32 / 1016.0);
    let ox = (width - aw) / 2;
    let oy = margin;
    let title_h = scaled(s, 44);
    let header_gap = scaled(s, 8);
    let body_y = oy + title_h + header_gap;
    let nav_h = scaled(s, 40);
    let nav_gap = scaled(s, 16);
    let body_h = oy + ah - nav_h - nav_gap - body_y;
    let gap = scaled(s, 18);
    let left_w = (aw - gap) * 56 / 100;
    out.scale = s;
    out.title = box_rect(ox, oy, aw, title_h);
    out.players = box_rect(ox, body_y, left_w, body_h.min(scaled(s, 464)));
    out.map = box_rect(ox + left_w + gap, body_y, aw - left_w - gap, body_h);
    let preview_side = ((out.map.width - 2 * scaled(s, 22)) / 2).min(scaled(s, 395));
    out.preview = box_rect(
        out.map.x + scaled(s, 22),
        out.map.y + scaled(s, 118),
        preview_side,
        preview_side,
    );
    let (
        mut player,
        mut swatches,
        mut palette,
        mut random,
        mut settings,
        mut nav,
        mut labels,
        mut previews,
    ): (
        Vec<usize>,
        Vec<usize>,
        Vec<usize>,
        Vec<usize>,
        Vec<usize>,
        Vec<usize>,
        Vec<usize>,
        Vec<usize>,
    ) = (
        Vec::new(),
        Vec::new(),
        Vec::new(),
        Vec::new(),
        Vec::new(),
        Vec::new(),
        Vec::new(),
        Vec::new(),
    );
    let mut keys = Vec::new();
    for (i, c) in controls.iter().enumerate() {
        if c.visible == 0 {
            continue;
        }
        if c.key == 0 || !good(c.source) || keys.contains(&c.key) {
            return (out, placed);
        }
        keys.push(c.key);
        if c.message != 0 && c.kind != 2 {
            return (out, placed);
        }
        if c.kind == 1 && c.source.x < 450 && c.source.y < 260 {
            player.push(i);
        } else if c.kind == 3 && c.source.x < 80 && c.source.y < 260 {
            swatches.push(i);
        } else if c.kind == 3 && c.source.x < 450 && (260..320).contains(&c.source.y) {
            palette.push(i);
        } else if c.kind == 0 && c.source.x < 450 && (260..320).contains(&c.source.y) {
            random.push(i);
        } else if c.kind == 3 && c.source.x >= 600 && (125..310).contains(&c.source.y) {
            previews.push(i);
        } else if c.kind == 0 && c.source.y >= 560 {
            nav.push(i);
        } else if (c.kind == 0 || c.kind == 1)
            && c.source.x >= 380
            && (120..560).contains(&c.source.y)
        {
            settings.push(i);
        } else if c.kind == 2 || c.kind == 3 {
            labels.push(i);
        } else {
            return (out, placed);
        }
    }
    if player.len() != 24 || nav.len() < 2 {
        return (out, placed);
    }
    player.sort_by_key(|&i| (controls[i].source.y, controls[i].source.x));
    for row in 0..8 {
        let y0 = controls[player[row * 3]].source.y;
        for col in 1..3 {
            if (controls[player[row * 3 + col]].source.y - y0).abs() > 8 {
                return (out, placed);
            }
        }
        if row > 0 && y0 <= controls[player[(row - 1) * 3]].source.y + 8 {
            return (out, placed);
        }
    }
    let inset = scaled(s, 16);
    let chip_strip = scaled(s, 64);
    let row_h = scaled(s, 34);
    let row_gap = scaled(s, 4);
    let rows_top = out.players.y + scaled(s, 82);
    let content_x = out.players.x + inset + chip_strip;
    let content_w = out.players.width - 2 * inset - chip_strip;
    let col_gap = scaled(s, 8);
    let widths = [
        (content_w - 2 * col_gap) * 46 / 100,
        (content_w - 2 * col_gap) * 32 / 100,
        0,
    ];
    let colx = [
        content_x,
        content_x + widths[0] + col_gap,
        content_x + widths[0] + col_gap + widths[1] + col_gap,
    ];
    let last_w = content_x + content_w - colx[2];
    if keys.contains(&usize::MAX) {
        return (out, placed);
    }
    if previews.is_empty() {
        placed.push(Placement {
            key: usize::MAX,
            source: Rect {
                x: 627,
                y: 129,
                width: 155,
                height: 155,
            },
            target: out.preview,
            kind: 3,
            item: -1,
        });
    } else {
        for &i in &previews {
            place(&mut placed, &controls[i], out.preview, -1);
        }
    }
    for row in 0..8 {
        for col in 0..3 {
            let c = &controls[player[row * 3 + col]];
            place(
                &mut placed,
                c,
                box_rect(
                    colx[col],
                    rows_top + row as i32 * (row_h + row_gap),
                    if col == 2 { last_w } else { widths[col] },
                    row_h,
                ),
                -1,
            );
        }
    }
    let mut native_color = [false; 8];
    for &i in &swatches {
        let c = &controls[i];
        let mut row = 0;
        let mut dist = 100000;
        for k in 0..8 {
            let d = (c.source.y - controls[player[k * 3]].source.y).abs();
            if d < dist {
                dist = d;
                row = k;
            }
        }
        if dist > 12 {
            return (out, placed);
        }
        let dim = scaled(s, 16);
        let ready = c.source.x < 25;
        if !ready {
            native_color[row] = true;
        }
        let x = out.players.x + inset + scaled(s, if ready { 4 } else { 32 });
        place(
            &mut placed,
            c,
            box_rect(
                x,
                rows_top + row as i32 * (row_h + row_gap) + (row_h - dim) / 2,
                dim,
                dim,
            ),
            -1,
        );
    }
    for row in 0..8 {
        if !native_color[row] {
            let dim = scaled(s, 16);
            placed.push(Placement {
                key: usize::MAX,
                source: Rect {
                    x: 30,
                    y: controls[player[row * 3]].source.y,
                    width: 16,
                    height: 16,
                },
                target: box_rect(
                    out.players.x + inset + scaled(s, 32),
                    rows_top + row as i32 * (row_h + row_gap) + (row_h - dim) / 2,
                    dim,
                    dim,
                ),
                kind: 3,
                item: -2,
            });
        }
    }
    random.sort_by_key(|&i| (controls[i].source.y, controls[i].source.x));
    let palette_y = out.players.y + scaled(s, 414);
    for (i, &index) in random.iter().enumerate() {
        let x = out.players.x + inset + i as i32 * scaled(s, 186);
        if x + scaled(s, 180) > out.players.x + out.players.width - inset {
            return (out, placed);
        }
        place(
            &mut placed,
            &controls[index],
            box_rect(x, palette_y, scaled(s, 180), scaled(s, 34)),
            -1,
        );
    }
    palette.sort_by_key(|&i| (controls[i].source.y, controls[i].source.x));
    for (i, &index) in palette.iter().enumerate() {
        let x =
            out.players.x + inset + random.len() as i32 * scaled(s, 186) + i as i32 * scaled(s, 22);
        let d = scaled(s, 16);
        if x + d > out.players.x + out.players.width - inset {
            return (out, placed);
        }
        place(
            &mut placed,
            &controls[index],
            box_rect(x, palette_y + scaled(s, 9), d, d),
            -1,
        );
    }
    nav.sort_by_key(|&i| (controls[i].source.x, controls[i].source.y));
    let nav_y = oy + ah - nav_h;
    for (i, &index) in nav.iter().enumerate() {
        let target = if i == 0 {
            box_rect(ox, nav_y, scaled(s, 260), nav_h)
        } else {
            let right_count = nav.len() as i32 - 1;
            let bw = scaled(s, 260);
            let x = ox + aw - right_count * bw - (right_count - 1) * scaled(s, 12)
                + (i as i32 - 1) * (bw + scaled(s, 12));
            box_rect(x, nav_y, bw, nav_h)
        };
        if target.x < ox
            || target.x + target.width > ox + aw
            || (i > 0 && target.x < ox + scaled(s, 260) + scaled(s, 12))
        {
            return (out, placed);
        }
        place(&mut placed, &controls[index], target, -1);
    }
    settings.sort_by_key(|&i| {
        (
            controls[i].source.x / 100,
            controls[i].source.y,
            controls[i].source.x,
        )
    });
    let side_x = out.preview.x + out.preview.width + scaled(s, 16);
    let mut side = 0;
    let mut below = 0;
    for &index in &settings {
        let c = &controls[index];
        let target = if (c.source.x >= 480 || c.kind == 1)
            && side_x + scaled(s, 96) < out.map.x + out.map.width - scaled(s, 14)
            && side < 7
        {
            let t = box_rect(
                side_x,
                out.preview.y + side * scaled(s, 46),
                out.map.x + out.map.width - scaled(s, 18) - side_x,
                scaled(s, 38),
            );
            side += 1;
            t
        } else {
            let bw = (out.map.width - 3 * scaled(s, 18)) / 2;
            let t = box_rect(
                out.map.x + scaled(s, 18) + (below % 2) * (bw + scaled(s, 18)),
                out.preview.y + out.preview.height + scaled(s, 22) + (below / 2) * scaled(s, 46),
                bw,
                scaled(s, 38),
            );
            below += 1;
            t
        };
        if target.y + target.height > out.map.y + out.map.height - scaled(s, 12) {
            return (out, placed);
        }
        place(&mut placed, c, target, -1);
    }
    labels.sort_by_key(|&i| (controls[i].source.y, controls[i].source.x));
    let is_message = |c: &SetupControl| {
        c.kind == 2 && (c.message != 0 || (c.source.x < 450 && c.source.y >= 300))
    };
    let message_count = labels.iter().filter(|&&i| is_message(&controls[i])).count() as i32;
    let message_gap = scaled(s, 8);
    let message_y = out.players.y + out.players.height + scaled(s, 16);
    let message_height = out.map.y + out.map.height - message_y;
    let message_slot = if message_count != 0 {
        (message_height - (message_count - 1) * message_gap) / message_count
    } else {
        0
    };
    if message_count != 0 && message_slot < scaled(s, 48) {
        return (out, placed);
    }
    let mut map_label = 0;
    let mut message_index = 0;
    for &index in &labels {
        let c = &controls[index];
        let target = if is_message(c) {
            let t = box_rect(
                out.players.x,
                message_y + message_index * (message_slot + message_gap),
                out.players.width,
                message_slot,
            );
            message_index += 1;
            t
        } else if title_label(c) {
            out.title
        } else if c.source.x >= 480 && c.source.y < 125 {
            let y = out.map.y
                + if map_label == 0 {
                    scaled(s, 6)
                } else {
                    scaled(s, 55) + (map_label - 1) * scaled(s, 25)
                };
            let t = box_rect(
                out.map.x + scaled(s, 22),
                y,
                out.map.width - 2 * scaled(s, 22),
                scaled(s, if map_label == 0 { 40 } else { 24 }),
            );
            map_label += 1;
            t
        } else if heading_column(c) >= 0 {
            let col = heading_column(c) as usize;
            box_rect(
                colx[col],
                out.players.y + scaled(s, 43),
                if col == 2 { last_w } else { widths[col] },
                scaled(s, 25),
            )
        } else if c.source.x < 450 && (260..320).contains(&c.source.y) {
            box_rect(
                out.players.x + inset,
                palette_y - scaled(s, 24),
                out.players.width - 2 * inset,
                scaled(s, 22),
            )
        } else if c.source.x < 450 && c.source.y < 260 {
            let mut row = 0;
            let mut dist = 100000;
            for k in 0..8 {
                let d = (c.source.y - controls[player[k * 3]].source.y).abs();
                if d < dist {
                    dist = d;
                    row = k;
                }
            }
            box_rect(
                colx[0],
                rows_top + row as i32 * (row_h + row_gap),
                widths[0],
                row_h,
            )
        } else {
            box_rect(
                out.map.x + scaled(s, 22),
                out.map.y + scaled(s, 56),
                out.map.width - 2 * scaled(s, 22),
                scaled(s, 24),
            )
        };
        place(&mut placed, c, target, -1);
    }
    let mut popups = player;
    for &index in &settings {
        if controls[index].kind == 1 {
            popups.push(index);
        }
    }
    for index in popups {
        let c = &controls[index];
        if c.open == 0 {
            continue;
        }
        if c.item_height <= 0 || !good(c.pull) {
            return (out, placed);
        }
        let Some(base) = placed.iter().find(|p| p.key == c.key && p.item < 0) else {
            return (out, placed);
        };
        let base = *base;
        let shown = (c.item_count as i32).min(8);
        let start = popup_scroll.clamp(0, (c.item_count as i32 - shown).max(0));
        let ih = base.target.height;
        let below = base.target.y + base.target.height + shown * ih
            <= if c.source.x < 450 {
                out.players.y + out.players.height
            } else {
                height - scaled(s, 8)
            };
        let ty = if below {
            base.target.y + base.target.height
        } else {
            base.target.y - shown * ih
        };
        if ty < 0 {
            return (out, placed);
        }
        for slot in 0..shown {
            let item = start + slot;
            let source = Rect {
                x: c.pull.x,
                y: c.pull.y + (if c.down != 0 { item + 1 } else { item }) * c.item_height,
                width: c.pull.width,
                height: c.item_height,
            };
            placed.push(Placement {
                key: c.key,
                source,
                target: box_rect(base.target.x, ty + slot * ih, base.target.width, ih),
                kind: 1,
                item,
            });
        }
    }
    out.valid = 1;
    (out, placed)
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_setup_arrange(
    width: i32,
    height: i32,
    controls: *const SetupControl,
    count: usize,
    popup_scroll: i32,
    output: *mut SetupLayout,
    placements: *mut Placement,
    capacity: usize,
) -> i32 {
    if output.is_null() || (count != 0 && controls.is_null()) {
        return 0;
    }
    let controls = if count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(controls, count) }
    };
    let (mut layout, items) = setup_arrange(width, height, controls, popup_scroll);
    layout.placement_count = items.len();
    unsafe {
        *output = layout;
    }
    if placements.is_null() {
        return 1;
    }
    if capacity < items.len() {
        return 0;
    }
    unsafe {
        ptr::copy_nonoverlapping(items.as_ptr(), placements, items.len());
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_setup_map_point(
    layout: *const SetupLayout,
    placements: *const Placement,
    x: i32,
    y: i32,
    sx: *mut i32,
    sy: *mut i32,
) -> i32 {
    if layout.is_null() || sx.is_null() || sy.is_null() {
        return 0;
    }
    let layout = unsafe { &*layout };
    if layout.valid == 0 || (layout.placement_count != 0 && placements.is_null()) {
        return 0;
    }
    let items = if layout.placement_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(placements, layout.placement_count) }
    };
    for p in items.iter().rev() {
        if p.kind == 2 || !contains(p.target, x, y) || !good(p.source) {
            continue;
        }
        unsafe {
            *sx = p.source.x
                + (((x - p.target.x) as i64 * p.source.width as i64) / p.target.width as i64)
                    as i32;
            *sy = p.source.y
                + (((y - p.target.y) as i64 * p.source.height as i64) / p.target.height as i64)
                    as i32;
        }
        return 1;
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_setup_launch_at(
    controls: *const SetupControl,
    count: usize,
    layout: *const SetupLayout,
    placements: *const Placement,
    x: i32,
    y: i32,
) -> usize {
    if layout.is_null() || (count != 0 && controls.is_null()) {
        return 0;
    }
    let layout = unsafe { &*layout };
    if layout.valid == 0 || (layout.placement_count != 0 && placements.is_null()) {
        return 0;
    }
    let controls = if count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(controls, count) }
    };
    let placements = if layout.placement_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(placements, layout.placement_count) }
    };
    let mut right = None;
    let mut count = 0;
    for c in controls {
        if c.visible != 0 && c.kind == 1 && c.open != 0 {
            return 0;
        }
        if c.visible != 0 && c.kind == 0 && c.source.y >= 560 {
            count += 1;
            if right.is_none_or(|r: &SetupControl| c.source.x > r.source.x) {
                right = Some(c);
            }
        }
    }
    let Some(right) = right else {
        return 0;
    };
    if count != 2 || right.enabled == 0 {
        return 0;
    }
    for p in placements {
        if p.key == right.key && p.kind == 0 && p.item == -1 && contains(p.target, x, y) {
            return right.key;
        }
    }
    0
}

fn normalize_controls(native_width: i32, controls: &mut [SetupControl]) {
    let scale = |n: i32| round_f64(n as f64 * 800.0 / native_width as f64) as i32;
    let rectangle = |r: &mut Rect| {
        let right = scale(r.x + r.width);
        let bottom = scale(r.y + r.height);
        r.x = scale(r.x);
        r.y = scale(r.y);
        r.width = right - r.x;
        r.height = bottom - r.y;
    };
    for c in controls {
        rectangle(&mut c.source);
        rectangle(&mut c.pull);
        c.item_height = scale(c.item_height);
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_setup_normalize(
    native_width: i32,
    controls: *mut SetupControl,
    count: usize,
) -> i32 {
    if native_width != 800 && native_width != 1200 {
        return 0;
    }
    if count != 0 && controls.is_null() {
        return 0;
    }
    if count != 0 {
        normalize_controls(native_width, unsafe {
            slice::from_raw_parts_mut(controls, count)
        });
    }
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_setup_native_width(
    width: i32,
    height: i32,
    canvas_width: i32,
    canvas_height: i32,
    controls: *const SetupControl,
    count: usize,
    popup_scroll: i32,
) -> i32 {
    if !((canvas_width == 800 && canvas_height == 600)
        || (canvas_width == 1200 && canvas_height == 900))
        || (count != 0 && controls.is_null())
    {
        return 0;
    }
    let raw = if count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(controls, count) }
    };
    let mut admitted = 0;
    let mut chosen = 0;
    for native_width in [800, 1200] {
        let native_height = native_width * 3 / 4;
        if native_width > canvas_width || native_height > canvas_height {
            continue;
        }
        let mut normalized = raw.to_vec();
        normalize_controls(native_width, &mut normalized);
        let (geometry, _) = setup_arrange(width, height, &normalized, popup_scroll);
        if geometry.valid != 0 {
            admitted += 1;
            chosen = native_width;
        }
    }
    if admitted == 1 { chosen } else { 0 }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_setup_preview(
    artwork: Artwork,
    layout: *const SetupLayout,
    placements: *const Placement,
    output: *mut u32,
    capacity: usize,
    width: *mut i32,
    height: *mut i32,
) -> i32 {
    if layout.is_null() || width.is_null() || height.is_null() {
        return 0;
    }
    let layout = unsafe { &*layout };
    if layout.valid == 0
        || artwork.width <= 0
        || artwork.height <= 0
        || artwork.pixels.is_null()
        || artwork.width as u64 * artwork.height as u64 != artwork.pixel_count as u64
        || (layout.placement_count != 0 && placements.is_null())
    {
        return 0;
    }
    let placements = if layout.placement_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(placements, layout.placement_count) }
    };
    let pixels = unsafe { slice::from_raw_parts(artwork.pixels, artwork.pixel_count) };
    for p in placements {
        if p.kind != 3 || p.item != -1 {
            continue;
        }
        let source = p.source;
        if !good(source)
            || source.x < 0
            || source.y < 0
            || source.x > 800 - source.width
            || source.y > 600 - source.height
        {
            return 0;
        }
        let x = (source.x as i64 * artwork.width as i64 / 800) as i32;
        let y = (source.y as i64 * artwork.height as i64 / 600) as i32;
        let w = (artwork.width - x)
            .min(1.max((source.width as i64 * artwork.width as i64 / 800) as i32));
        let h = (artwork.height - y)
            .min(1.max((source.height as i64 * artwork.height as i64 / 600) as i32));
        if x < 0 || y < 0 || w <= 0 || h <= 0 {
            return 0;
        }
        unsafe {
            *width = w;
            *height = h;
        }
        if !output.is_null() {
            if capacity < w as usize * h as usize {
                return 0;
            }
            for row in 0..h as usize {
                let src = (y as usize + row) * artwork.width as usize + x as usize;
                unsafe {
                    ptr::copy_nonoverlapping(
                        pixels.as_ptr().add(src),
                        output.add(row * w as usize),
                        w as usize,
                    );
                }
            }
        }
        return 1;
    }
    0
}

type SetupDraw =
    unsafe extern "C" fn(*mut c_void, i32, Rect, Rect, u32, u32, i32, i32, Bytes, Artwork) -> i32;
fn setup_paint(
    width: i32,
    height: i32,
    controls: &[SetupControl],
    layout: SetupLayout,
    placements: &[Placement],
    background: Artwork,
    native_canvas: Artwork,
    background_present: bool,
    native_present: bool,
    hot_x: i32,
    hot_y: i32,
    draw: SetupDraw,
    context: *mut c_void,
) -> bool {
    if width <= 0 || height <= 0 || layout.valid == 0 {
        return false;
    }
    let valid_art = |a: Artwork| {
        a.width > 0
            && a.height > 0
            && !a.pixels.is_null()
            && a.width as u64 * a.height as u64 == a.pixel_count as u64
    };
    if (background_present && !valid_art(background))
        || (native_present && !valid_art(native_canvas))
    {
        return false;
    }
    let empty = Bytes {
        data: ptr::null(),
        length: 0,
    };
    let no_art = Artwork {
        width: 0,
        height: 0,
        pixels: ptr::null(),
        pixel_count: 0,
    };
    let call = |op, r, src, color, edge, size, flags, text, art| unsafe {
        draw(context, op, r, src, color, edge, size, flags, text, art) != 0
    };
    let fill = |r, c| call(0, r, Rect::default(), c, 0, 0, 0, empty, no_art);
    let text = |r, bytes: Bytes, size, color, heavy, center| {
        call(
            3,
            r,
            Rect::default(),
            color,
            0,
            size,
            (if heavy { 1 } else { 0 }) | (if center { 2 } else { 0 }),
            bytes,
            no_art,
        )
    };
    let panel = |r: Rect, color, edge, cut, part| {
        call(1, r, Rect::default(), 0, 0, cut, part, empty, no_art)
            || call(2, r, Rect::default(), color, edge, cut, 0, empty, no_art)
    };
    let image = |art: Artwork, dst: Rect, source: Rect, dim| {
        call(4, dst, source, 0, 0, 0, if dim { 1 } else { 0 }, empty, art)
    };
    let crop = |p: &Placement| {
        if !valid_art(native_canvas) {
            return true;
        }
        let a = native_canvas;
        let mut source = Rect {
            x: p.source.x * a.width / 800,
            y: p.source.y * a.height / 600,
            width: (p.source.width * a.width / 800).max(1),
            height: (p.source.height * a.height / 600).max(1),
        };
        source.x = source.x.clamp(0, a.width - 1);
        source.y = source.y.clamp(0, a.height - 1);
        source.width = source.width.min(a.width - source.x);
        source.height = source.height.min(a.height - source.y);
        image(a, p.target, source, false)
    };
    if background_present {
        if !image(
            background,
            Rect {
                x: 0,
                y: 0,
                width,
                height,
            },
            Rect {
                x: 0,
                y: 0,
                width: background.width,
                height: background.height,
            },
            true,
        ) {
            return false;
        }
    } else if !fill(
        Rect {
            x: 0,
            y: 0,
            width,
            height,
        },
        0x071525,
    ) {
        return false;
    }
    let s = layout.scale;
    let cut = scaled(s, 7);
    let pad = scaled(s, 16);
    if !panel(layout.players, 0x101820, 0x8c9aa4, cut, 0)
        || !panel(layout.map, 0x101820, 0x8c9aa4, cut, 0)
    {
        return false;
    }
    let player_header = Rect {
        x: layout.players.x + 1,
        y: layout.players.y + 1,
        width: layout.players.width - 2,
        height: scaled(s, 39),
    };
    let map_header = Rect {
        x: layout.map.x + 1,
        y: layout.map.y + 1,
        width: layout.map.width - 2,
        height: scaled(s, 39),
    };
    let header = |r| call(1, r, Rect::default(), 0, 0, cut, 1, empty, no_art) || fill(r, 0x27343f);
    if !header(player_header) || !header(map_header) {
        return false;
    }
    if !fill(
        Rect {
            x: layout.players.x + 1,
            y: player_header.y + player_header.height,
            width: layout.players.width - 2,
            height: scaled(s, 2),
        },
        0xb38a4d,
    ) || !fill(
        Rect {
            x: layout.map.x + 1,
            y: map_header.y + map_header.height,
            width: layout.map.width - 2,
            height: scaled(s, 2),
        },
        0xb38a4d,
    ) {
        return false;
    }
    let native_title = controls.iter().any(|c| c.visible != 0 && title_label(c));
    let lit = |bytes: &'static [u8]| Bytes {
        data: bytes.as_ptr(),
        length: bytes.len(),
    };
    if !native_title
        && !text(
            Rect {
                x: layout.title.x + pad,
                y: layout.title.y,
                width: layout.title.width - 2 * pad,
                height: layout.title.height,
            },
            lit(b"GAME SETUP"),
            scaled(s, 44),
            0xe3b76b,
            true,
            false,
        )
    {
        return false;
    }
    if !text(
        Rect {
            x: layout.players.x + pad,
            y: layout.players.y,
            width: layout.players.width - 2 * pad,
            height: player_header.height,
        },
        lit(b"PLAYERS"),
        scaled(s, 26),
        0xd9e0e5,
        true,
        false,
    ) {
        return false;
    }
    let mut player_headings = [false; 3];
    let mut map_heading = false;
    let mut first_map_y = i32::MAX;
    for c in controls {
        if c.visible != 0 && c.kind == 2 && c.message == 0 && c.source.x >= 480 && c.source.y < 125
        {
            first_map_y = first_map_y.min(c.source.y);
        }
    }
    for c in controls {
        if c.visible != 0 && c.kind == 2 {
            let col = heading_column(c);
            if col >= 0 {
                player_headings[col as usize] = true;
            }
            if c.message == 0 && c.source.x >= 480 && c.source.y == first_map_y {
                map_heading = true;
            }
        }
    }
    if !map_heading
        && !text(
            Rect {
                x: layout.map.x + pad,
                y: layout.map.y,
                width: layout.map.width - 2 * pad,
                height: map_header.height,
            },
            lit(b"MAP"),
            scaled(s, 26),
            0xd9e0e5,
            true,
            false,
        )
    {
        return false;
    }
    let head_x = layout.players.x + scaled(s, 80);
    let head_w = layout.players.width - scaled(s, 96);
    let head_gap = scaled(s, 8);
    let hw0 = (head_w - 2 * head_gap) * 46 / 100;
    let hw1 = (head_w - 2 * head_gap) * 32 / 100;
    let hx = [
        head_x,
        head_x + hw0 + head_gap,
        head_x + hw0 + head_gap + hw1 + head_gap,
    ];
    let names = [lit(b"PLAYER NAME"), lit(b"SIDE"), lit(b"TEAMS")];
    for col in 0..3 {
        if !player_headings[col]
            && !text(
                Rect {
                    x: hx[col],
                    y: layout.players.y + scaled(s, 44),
                    width: if col == 0 {
                        hw0
                    } else if col == 1 {
                        hw1
                    } else {
                        head_x + head_w - hx[col]
                    },
                    height: scaled(s, 27),
                },
                names[col],
                scaled(s, 18),
                0xc1cad0,
                true,
                false,
            )
        {
            return false;
        }
    }
    let lookup = |key| controls.iter().rev().find(|c| c.key == key);
    for p in placements {
        if p.kind == 2 {
            let Some(c) = lookup(p.key) else {
                return false;
            };
            if c.text.length != 0
                && (c.message != 0 || (p.source.x < 450 && p.source.y >= 300))
                && !panel(p.target, 0x101820, 0x8c9aa4, scaled(s, 7), 0)
            {
                return false;
            }
        }
    }
    for p in placements {
        if p.key == usize::MAX {
            if p.kind == 3 && !crop(p) {
                return false;
            }
            continue;
        }
        let Some(c) = lookup(p.key) else {
            return false;
        };
        if p.kind == 3 {
            if !crop(p) {
                return false;
            }
            continue;
        }
        if p.kind == 2 {
            if c.message != 0 || (c.source.x < 450 && c.source.y >= 300) {
                if c.text.length == 0 {
                    continue;
                }
                let r = Rect {
                    x: p.target.x + pad,
                    y: p.target.y + scaled(s, 12),
                    width: p.target.width - 2 * pad,
                    height: p.target.height - 2 * scaled(s, 12),
                };
                if !call(
                    5,
                    r,
                    Rect::default(),
                    0xd6e2e8,
                    0,
                    scaled(s, 17),
                    0,
                    c.text,
                    no_art,
                ) {
                    return false;
                }
            } else {
                let size = scaled(
                    s,
                    if title_label(c) {
                        44
                    } else if c.message == 0 && c.source.x >= 480 && c.source.y == first_map_y {
                        26
                    } else {
                        18
                    },
                );
                if !text(
                    p.target,
                    c.text,
                    size,
                    if title_label(c) { 0xe3b76b } else { 0xd6e2e8 },
                    (c.message == 0 && c.source.x >= 480 && c.source.y < 125) || title_label(c),
                    false,
                ) {
                    return false;
                }
            }
            continue;
        }
        let hot = contains(p.target, hot_x, hot_y);
        let selected = p.item >= 0 && p.item == c.selected;
        let fill_color = if c.enabled == 0 {
            0x202933
        } else if c.mode > 0 {
            0x7a5630
        } else if hot {
            0x365064
        } else {
            0x263744
        };
        let border = if c.enabled == 0 {
            0x53616a
        } else if hot || selected {
            0xe9be73
        } else {
            0x8999a3
        };
        let part = if c.enabled == 0 {
            5
        } else if p.item >= 0 {
            if selected { 7 } else { 6 }
        } else if c.mode > 0 {
            4
        } else if hot {
            3
        } else {
            2
        };
        if !panel(p.target, fill_color, border, scaled(s, 6), part) {
            return false;
        }
        let caption = if p.item >= 0 {
            if c.items.is_null() || p.item as usize >= c.item_count {
                return false;
            }
            unsafe { *c.items.add(p.item as usize) }
        } else {
            c.text
        };
        let text_inset = scaled(s, 11);
        let label_box = Rect {
            x: p.target.x + text_inset,
            y: p.target.y,
            width: p.target.width - text_inset - scaled(s, 15),
            height: p.target.height,
        };
        if !text(
            label_box,
            caption,
            scaled(s, 20),
            if c.enabled != 0 { 0xe6e9e8 } else { 0x82909b },
            c.kind == 0,
            c.kind == 0,
        ) {
            return false;
        }
        if p.kind == 1 && p.item < 0 {
            let arrow = Rect {
                x: p.target.x + p.target.width - scaled(s, 27),
                y: p.target.y,
                width: scaled(s, 20),
                height: p.target.height,
            };
            if !text(
                arrow,
                lit(b"v"),
                scaled(s, 16),
                if c.enabled != 0 { 0xe4b970 } else { 0x82909b },
                true,
                true,
            ) {
                return false;
            }
        }
    }
    true
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_paint_setup(
    width: i32,
    height: i32,
    controls: *const SetupControl,
    control_count: usize,
    layout: *const SetupLayout,
    placements: *const Placement,
    background: Artwork,
    native_canvas: Artwork,
    background_present: i32,
    native_present: i32,
    hot_x: i32,
    hot_y: i32,
    draw: Option<SetupDraw>,
    context: *mut c_void,
) -> i32 {
    if layout.is_null() || (control_count != 0 && controls.is_null()) {
        return 0;
    }
    let layout = unsafe { *layout };
    if layout.placement_count != 0 && placements.is_null() {
        return 0;
    }
    let Some(draw) = draw else {
        return 0;
    };
    let controls = if control_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(controls, control_count) }
    };
    let placements = if layout.placement_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(placements, layout.placement_count) }
    };
    i32::from(setup_paint(
        width,
        height,
        controls,
        layout,
        placements,
        background,
        native_canvas,
        background_present != 0,
        native_present != 0,
        hot_x,
        hot_y,
        draw,
        context,
    ))
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LoadingTheme {
    form: i32,
    panel_top: u32,
    panel: u32,
    panel_edge: u32,
    panel_hi: u32,
    text: u32,
    text_dim: u32,
    title: u32,
    accent: u32,
    accent2: u32,
    fill: u32,
    good: u32,
    corners: [f32; 4],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Player {
    name: Bytes,
    detail: Bytes,
    team: i32,
    local: i32,
    fraction: f32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LoadingPaintInput {
    kind: i32,
    backdrop_present: i32,
    preview_present: i32,
    title: Bytes,
    mode: Bytes,
    status: Bytes,
    hint: Bytes,
    briefing: *const Bytes,
    briefing_count: usize,
    objectives: *const Bytes,
    objective_count: usize,
    players: *const Player,
    player_count: usize,
    progress: f32,
    time_seconds: f64,
    departure_shown: i32,
    bar_track: Rect,
    protected_regions: *const Rect,
    protected_count: usize,
    theme: LoadingTheme,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Point {
    x: i32,
    y: i32,
}
type LoadingDraw = unsafe extern "C" fn(
    *mut c_void,
    i32,
    Rect,
    u32,
    i32,
    i32,
    Bytes,
    *const u32,
    usize,
    *const Point,
    usize,
    *mut i32,
) -> i32;

fn blend_color(a: u32, b: u32, t: f32) -> u32 {
    let channel = |shift| {
        round_i32(((a >> shift) & 255u32) as f32 * (1.0 - t) + ((b >> shift) & 255u32) as f32 * t)
            as u32
    };
    (channel(16) << 16) | (channel(8) << 8) | channel(0)
}
fn outline(r: Rect, theme: LoadingTheme, scale: f32) -> Vec<Point> {
    let mut points = Vec::new();
    let max_corner = r.width.min(r.height) as f32 * 0.45;
    let c = |i: usize| (theme.corners[i] * scale).clamp(-max_corner, max_corner);
    let left = r.x as f32;
    let top = r.y as f32;
    let right = (r.x + r.width - 1) as f32;
    let bottom = (r.y + r.height - 1) as f32;
    let mut point = |x: f32, y: f32| {
        points.push(Point {
            x: round_i32(x),
            y: round_i32(y),
        })
    };
    let corner = |cx: f32, cy: f32, radius: f32, quadrant: i32, push: &mut dyn FnMut(f32, f32)| {
        if radius <= 0.0 {
            push(cx, cy);
            return;
        }
        let steps = 5;
        for j in 0..=steps {
            let angle = ((quadrant * 90) as f64 + j as f64 * 90.0 / steps as f64)
                * 3.141_592_653_589_793
                / 180.0;
            let a = angle as f32;
            push(
                cx + unsafe { cosf(a) } * radius.abs(),
                cy + unsafe { sinf(a) } * radius.abs(),
            );
        }
    };
    let tl = c(0);
    let tr = c(1);
    let br = c(2);
    let bl = c(3);
    point(left + tl.abs(), top);
    point(right - tr.abs(), top);
    if tr > 0.0 {
        corner(right - tr, top + tr, tr, 3, &mut point)
    } else {
        point(right, top + tr.abs())
    }
    point(right, bottom - br.abs());
    if br > 0.0 {
        corner(right - br, bottom - br, br, 0, &mut point)
    } else {
        point(right - br.abs(), bottom)
    }
    point(left + bl.abs(), bottom);
    if bl > 0.0 {
        corner(left + bl, bottom - bl, bl, 1, &mut point)
    } else {
        point(left, bottom - bl.abs())
    }
    point(left, top + tl.abs());
    if tl > 0.0 {
        corner(left + tl, top + tl, tl, 2, &mut point)
    }
    points
}

fn loading_image_pixels(
    art: Artwork,
    dest: Rect,
    shaded: bool,
    departure: bool,
) -> Option<Vec<u32>> {
    if dest.width <= 0
        || dest.height <= 0
        || art.width <= 0
        || art.height <= 0
        || art.pixels.is_null()
        || art.width as u64 * art.height as u64 != art.pixel_count as u64
    {
        return None;
    }
    let source = unsafe { slice::from_raw_parts(art.pixels, art.pixel_count) };
    let scale = (dest.width as f64 / art.width as f64).max(dest.height as f64 / art.height as f64);
    let crop_w = dest.width as f64 / scale;
    let crop_h = dest.height as f64 / scale;
    let ox = (art.width as f64 - crop_w) * 0.5;
    let oy = (art.height as f64 - crop_h) * 0.5;
    let count = (dest.width as usize).checked_mul(dest.height as usize)?;
    let mut pixels = Vec::new();
    pixels.try_reserve_exact(count).ok()?;
    pixels.resize(count, 0);
    let exact = dest.width == art.width && dest.height == art.height;
    let mut source_x = Vec::new();
    source_x.try_reserve_exact(dest.width as usize).ok()?;
    let mut light_x = Vec::new();
    let mut departure_left = Vec::new();
    if shaded && departure {
        departure_left.try_reserve_exact(dest.width as usize).ok()?;
    } else if shaded {
        light_x.try_reserve_exact(dest.width as usize).ok()?;
    }
    for x in 0..dest.width {
        source_x.push(
            if exact {
                x
            } else {
                (ox + (x as f64 + 0.5) * crop_w / dest.width as f64) as i32
            }
            .clamp(0, art.width - 1),
        );
        if shaded && departure {
            departure_left.push((1.0f32 - x as f32 / (dest.width as f32 * 0.55)).clamp(0.0, 1.0));
        } else if shaded {
            light_x.push(23 + 27 * x / (dest.width - 1).max(1));
        }
    }
    for y in 0..dest.height as usize {
        let sy = if exact {
            y as i32
        } else {
            (oy + (y as f64 + 0.5) * crop_h / dest.height as f64) as i32
        }
        .clamp(0, art.height - 1) as usize;
        let source_row = sy * art.width as usize;
        let foot = if shaded && departure {
            ((y as f32 / dest.height as f32 - 0.7) / 0.3).clamp(0.0, 1.0)
        } else {
            0.0
        };
        let bottom = if shaded && !departure {
            100 - 35 * y as i32 / (dest.height - 1).max(1)
        } else {
            0
        };
        for x in 0..dest.width as usize {
            let color = source[source_row + source_x[x] as usize];
            pixels[y * dest.width as usize + x] = if !shaded {
                0xff000000 | (color & 0xffffff)
            } else {
                let k = if departure {
                    (100.0f32 - (departure_left[x] * 56.0).max(foot * 63.0)) as i32
                } else {
                    light_x[x] * bottom / 100
                };
                0xff000000
                    | ((((color >> 16) & 255) * k as u32 / 100) << 16)
                    | ((((color >> 8) & 255) * k as u32 / 100) << 8)
                    | ((color & 255) * k as u32 / 100)
            };
        }
    }
    Some(pixels)
}

fn loading_paint(
    width: i32,
    height: i32,
    model: LoadingPaintInput,
    backdrop: Artwork,
    preview: Artwork,
    transparent: bool,
    draw: LoadingDraw,
    context: *mut c_void,
) -> bool {
    let protected = if model.protected_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(model.protected_regions, model.protected_count) }
    };
    let briefing = if model.briefing_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(model.briefing, model.briefing_count) }
    };
    let objectives = if model.objective_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(model.objectives, model.objective_count) }
    };
    let players = if model.player_count == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(model.players, model.player_count) }
    };
    let layout = loading_layout(
        width,
        height,
        LoadingInput {
            kind: model.kind,
            show_preview: model.preview_present,
            player_count: model.player_count,
            protected_regions: model.protected_regions,
            protected_count: model.protected_count,
        },
        protected,
    );
    if !layout.valid.eq(&1) {
        return false;
    }
    let empty = Bytes {
        data: ptr::null(),
        length: 0,
    };
    let lit = |v: &'static [u8]| Bytes {
        data: v.as_ptr(),
        length: v.len(),
    };
    let byte_vec = |v: &Vec<u8>| Bytes {
        data: v.as_ptr(),
        length: v.len(),
    };
    let call = |op,
                r,
                color,
                size,
                flags,
                text: Bytes,
                pixels: &[u32],
                points: &[Point],
                used: &mut i32| unsafe {
        draw(
            context,
            op,
            r,
            color,
            size,
            flags,
            text,
            pixels.as_ptr(),
            pixels.len(),
            points.as_ptr(),
            points.len(),
            used,
        )
    };
    let fill = |r, color| call(0, r, color, 0, 0, empty, &[], &[], &mut 0) != 0;
    let text = |r, value, size, color, bold, flags| {
        let mut used = 0;
        let okay = call(
            1,
            r,
            color,
            size,
            flags | (if bold { 1 } else { 0 }),
            value,
            &[],
            &[],
            &mut used,
        ) != 0;
        (okay, used)
    };
    let pixels = |r, data: &[u32]| call(2, r, 0, 0, 0, empty, data, &[], &mut 0) != 0;
    let simple = |op, r, size| call(op, r, 0, size, 0, empty, &[], &[], &mut 0);
    let image = |art: Artwork, r, shaded, departure| {
        let Some(data) = loading_image_pixels(art, r, shaded, departure) else {
            return false;
        };
        pixels(r, &data)
    };
    if transparent {
        if simple(
            3,
            Rect {
                x: 0,
                y: 0,
                width,
                height,
            },
            0,
        ) == 0
        {
            return false;
        }
    } else if model.backdrop_present != 0 {
        if !image(
            backdrop,
            Rect {
                x: 0,
                y: 0,
                width,
                height,
            },
            true,
            model.departure_shown != 0,
        ) {
            return false;
        }
    } else {
        let count = width as usize * height as usize;
        let mut data = Vec::new();
        if data.try_reserve_exact(count).is_err() {
            return false;
        }
        data.resize(count, 0);
        for y in 0..height {
            for x in 0..width {
                let glow = 12 * x / width + 9 * (height - y) / height;
                data[y as usize * width as usize + x as usize] = 0xff000000
                    | (((11 + glow / 3) as u32) << 16)
                    | (((20 + glow / 2) as u32) << 8)
                    | (34 + glow) as u32;
            }
        }
        if !pixels(
            Rect {
                x: 0,
                y: 0,
                width,
                height,
            },
            &data,
        ) {
            return false;
        }
    }
    let track = model.bar_track;
    let theme = model.theme;
    if track.width > 0
        && track.height > 0
        && (!fill(
            Rect {
                x: track.x - 1,
                y: track.y - 1,
                width: track.width + 2,
                height: track.height + 2,
            },
            theme.panel_edge,
        ) || !fill(track, theme.panel))
    {
        return false;
    }
    let saved = simple(4, Rect::default(), 0);
    if saved == 0 {
        return false;
    }
    let mut ok = true;
    for r in protected {
        if r.width <= 0 || r.height <= 0 {
            continue;
        }
        let left = (r.x as i64).clamp(0, width as i64) as i32;
        let top = (r.y as i64).clamp(0, height as i64) as i32;
        let right = (r.x as i64 + r.width as i64).clamp(0, width as i64) as i32;
        let bottom = (r.y as i64 + r.height as i64).clamp(0, height as i64) as i32;
        if left < right
            && top < bottom
            && simple(
                6,
                Rect {
                    x: left,
                    y: top,
                    width: right - left,
                    height: bottom - top,
                },
                0,
            ) == 0
        {
            ok = false;
        }
    }
    let s = layout.scale;
    let margin = layout.kicker.x;
    let small = px(s, 24);
    let body = px(s, 30);
    let mode = if model.mode.length == 0 {
        lit(b"LOADING")
    } else {
        model.mode
    };
    ok = text(layout.kicker, mode, small, theme.accent, true, 2 | 4).0 && ok;
    let title = if model.title.length == 0 {
        lit(b"Loading")
    } else {
        model.title
    };
    ok = text(layout.title, title, px(s, 60), theme.title, true, 8 | 16).0 && ok;
    ok = fill(
        Rect {
            x: margin,
            y: layout.title.y + layout.title.height + px(s, 8),
            width: px(s, 120),
            height: px(s, 3),
        },
        theme.accent,
    ) && ok;
    if model.kind == 0 || model.kind == 2 {
        let column = simple(4, Rect::default(), 0);
        if column == 0 {
            ok = false;
        } else {
            if simple(7, layout.briefing, 0) == 0 {
                ok = false;
            }
            let mut y = layout.briefing.y;
            let paragraph_gap = px(s, 12);
            for &paragraph in briefing {
                let (painted, used) = text(
                    Rect {
                        x: margin,
                        y,
                        width: layout.briefing.width,
                        height: (layout.briefing.y + layout.briefing.height - y).max(0),
                    },
                    paragraph,
                    body,
                    theme.text,
                    false,
                    8 | 16,
                );
                ok = painted && ok;
                y += used + paragraph_gap;
                if y >= layout.briefing.y + layout.briefing.height {
                    break;
                }
            }
            if !objectives.is_empty() && y < layout.briefing.y + layout.briefing.height {
                y += px(s, 10);
                ok = text(
                    Rect {
                        x: margin,
                        y,
                        width: layout.briefing.width,
                        height: px(s, 35),
                    },
                    lit(b"OBJECTIVES"),
                    small,
                    theme.accent,
                    true,
                    2,
                )
                .0 && ok;
                y += px(s, 42);
                for &objective in objectives {
                    let remaining = layout.briefing.y + layout.briefing.height - y;
                    if remaining <= 0 {
                        break;
                    }
                    ok = fill(
                        Rect {
                            x: margin + px(s, 2),
                            y: y + px(s, 9),
                            width: px(s, 7),
                            height: px(s, 7),
                        },
                        theme.accent,
                    ) && ok;
                    let (painted, used) = text(
                        Rect {
                            x: margin + px(s, 22),
                            y,
                            width: layout.briefing.width - px(s, 22),
                            height: remaining,
                        },
                        objective,
                        small,
                        theme.text,
                        false,
                        8 | 16,
                    );
                    ok = painted && ok;
                    y += used + px(s, 6);
                }
            }
            if model.hint.length != 0 && y < layout.briefing.y + layout.briefing.height {
                y += px(s, 14);
                let mut hint = b"Hint: ".to_vec();
                if let Some(value) = input(model.hint.data, model.hint.length) {
                    hint.extend_from_slice(value);
                } else {
                    ok = false;
                }
                ok = text(
                    Rect {
                        x: margin,
                        y,
                        width: layout.briefing.width,
                        height: (layout.briefing.y + layout.briefing.height - y).max(0),
                    },
                    byte_vec(&hint),
                    small,
                    theme.text_dim,
                    false,
                    8 | 16,
                )
                .0 && ok;
            }
            simple(5, Rect::default(), column);
        }
    }
    let panel = |r: Rect| {
        if r.width <= 0 || r.height <= 0 {
            return true;
        }
        let points = outline(r, theme, s);
        let count = r.width as usize * r.height as usize;
        let mut data = Vec::new();
        if data.try_reserve_exact(count).is_err() {
            return false;
        }
        data.resize(count, 0);
        for y in 0..r.height {
            let t = y as f32 / (r.height - 1).max(1) as f32;
            let color = blend_color(theme.panel_top, theme.panel, t);
            for x in 0..r.width {
                data[y as usize * r.width as usize + x as usize] = 0xff000000 | (color & 0xffffff);
            }
        }
        let mut good = call(
            8,
            r,
            theme.panel_edge,
            px(s, 2),
            0,
            empty,
            &data,
            &points,
            &mut 0,
        ) != 0;
        let inset = px(s, 5);
        good = fill(
            Rect {
                x: r.x + inset,
                y: r.y + inset,
                width: r.width - inset * 2,
                height: 1,
            },
            blend_color(theme.panel_edge, theme.panel_hi, 0.45),
        ) && good;
        match theme.form {
            4 => {
                good = fill(
                    Rect {
                        x: r.x + inset,
                        y: r.y + inset,
                        width: r.width - inset * 2,
                        height: px(s, 2),
                    },
                    theme.accent,
                ) && good
            }
            5 => {
                good = fill(
                    Rect {
                        x: r.x + inset,
                        y: r.y + inset,
                        width: r.width - inset * 2,
                        height: 1,
                    },
                    theme.panel_hi,
                ) && good;
                good = fill(
                    Rect {
                        x: r.x + inset,
                        y: r.y + r.height - inset,
                        width: r.width - inset * 2,
                        height: 1,
                    },
                    theme.panel_hi,
                ) && good;
            }
            2 | 3 => {
                good = fill(
                    Rect {
                        x: r.x + inset,
                        y: r.y + inset,
                        width: px(s, 36),
                        height: px(s, 2),
                    },
                    theme.accent,
                ) && good
            }
            6 => {
                good = fill(
                    Rect {
                        x: r.x + inset,
                        y: r.y + inset,
                        width: r.width - inset * 2,
                        height: 1,
                    },
                    blend_color(theme.panel_hi, theme.accent, 0.5),
                ) && good
            }
            1 => {
                good = fill(
                    Rect {
                        x: r.x + inset,
                        y: r.y + inset,
                        width: px(s, 12),
                        height: r.height - inset * 2,
                    },
                    theme.accent,
                ) && good
            }
            _ => {}
        }
        good
    };
    if model.kind != 2 && (model.preview_present != 0 || !players.is_empty()) {
        if model.preview_present != 0 && layout.preview.height > 0 {
            ok = fill(
                Rect {
                    x: layout.preview.x - px(s, 3),
                    y: layout.preview.y - px(s, 3),
                    width: layout.preview.width + px(s, 6),
                    height: layout.preview.height + px(s, 6),
                },
                theme.panel_edge,
            ) && ok;
            ok = image(preview, layout.preview, false, false) && ok;
        }
        let mut y = layout.players.y;
        if !players.is_empty() {
            let fractions = players.iter().any(|p| p.fraction >= 0.0);
            let row_h = px(s, 84).min((layout.players.height - px(s, 52)) / players.len() as i32);
            let panel_h = layout
                .players
                .height
                .min(px(s, 52) + row_h * players.len() as i32);
            ok = panel(Rect {
                x: layout.players.x,
                y,
                width: layout.players.width,
                height: panel_h,
            }) && ok;
            let inset = px(s, 12);
            ok = text(
                Rect {
                    x: layout.players.x + inset,
                    y: y + px(s, 8),
                    width: layout.players.width - 2 * inset,
                    height: px(s, 35),
                },
                if fractions {
                    lit(b"PLAYERS  -  LOADING")
                } else {
                    lit(b"PLAYERS")
                },
                small,
                theme.accent,
                true,
                2,
            )
            .0 && ok;
            y += px(s, 42);
            let player_clip = simple(4, Rect::default(), 0);
            if player_clip == 0 {
                ok = false;
            } else {
                if simple(7, layout.players, 0) == 0 {
                    ok = false;
                }
                for p in players {
                    if y >= layout.players.y + layout.players.height {
                        break;
                    }
                    let badge_x = layout.players.x + inset;
                    ok = fill(
                        Rect {
                            x: badge_x,
                            y: y + px(s, 5),
                            width: px(s, 28),
                            height: px(s, 28),
                        },
                        theme.accent2,
                    ) && ok;
                    let team = format!("{}", p.team.max(0) + 1).into_bytes();
                    ok = text(
                        Rect {
                            x: badge_x,
                            y: y + px(s, 5),
                            width: px(s, 28),
                            height: px(s, 28),
                        },
                        byte_vec(&team),
                        small,
                        theme.panel,
                        true,
                        2 | 32 | 64,
                    )
                    .0 && ok;
                    let name_x = badge_x + px(s, 38);
                    ok = text(
                        Rect {
                            x: name_x,
                            y,
                            width: layout.players.x + layout.players.width - inset - name_x,
                            height: px(s, 34),
                        },
                        p.name,
                        body,
                        if p.local != 0 {
                            theme.title
                        } else {
                            theme.text
                        },
                        p.local != 0,
                        2 | 4,
                    )
                    .0 && ok;
                    ok = text(
                        Rect {
                            x: name_x,
                            y: y + px(s, 32),
                            width: layout.players.x + layout.players.width - inset - name_x,
                            height: row_h - px(s, 42),
                        },
                        if p.detail.length == 0 {
                            lit(b"Random")
                        } else {
                            p.detail
                        },
                        px(s, 21),
                        theme.text_dim,
                        false,
                        8 | 16,
                    )
                    .0 && ok;
                    if p.fraction.is_finite() && p.fraction >= 0.0 {
                        let bar_x = name_x;
                        let bar_y = y + row_h - px(s, 7);
                        let bar_w = layout.players.x + layout.players.width - inset - bar_x;
                        ok = fill(
                            Rect {
                                x: bar_x,
                                y: bar_y,
                                width: bar_w,
                                height: px(s, 5),
                            },
                            theme.panel_edge,
                        ) && ok;
                        ok = fill(
                            Rect {
                                x: bar_x,
                                y: bar_y,
                                width: round_i32(bar_w as f32 * p.fraction.clamp(0.0, 1.0)),
                                height: px(s, 5),
                            },
                            if p.fraction >= 1.0 {
                                theme.good
                            } else {
                                theme.fill
                            },
                        ) && ok;
                    }
                    y += row_h;
                }
                simple(5, Rect::default(), player_clip);
            }
        }
    }
    let progress = if model.progress.is_finite() {
        model.progress.clamp(0.0, 1.0)
    } else {
        0.0
    };
    let percentage = round_i32(progress * 100.0);
    let spinner_size = px(s, 28);
    let spinner_x = layout.status.x + spinner_size / 2;
    let spinner_y = layout.status.y + layout.status.height / 2;
    let seconds = if model.time_seconds.is_finite() {
        model.time_seconds
    } else {
        0.0
    };
    let lead = unsafe { fmod(seconds.max(0.0) * 14.0, 10.0) };
    for i in 0..10 {
        let a = i as f64 * 6.283185307179586 / 10.0 - 1.5707963267948966;
        let x = spinner_x + round_f64(unsafe { cos(a) } * spinner_size as f64 * 0.38) as i32;
        let y = spinner_y + round_f64(unsafe { sin(a) } * spinner_size as f64 * 0.38) as i32;
        let behind = (unsafe { fmod(lead - i as f64 + 10.0, 10.0) } / 10.0) as f32;
        ok = fill(
            Rect {
                x,
                y,
                width: px(s, 4),
                height: px(s, 4),
            },
            blend_color(theme.accent, theme.panel, behind * 0.85),
        ) && ok;
    }
    ok = text(
        Rect {
            x: layout.status.x + spinner_size + px(s, 14),
            y: layout.status.y,
            width: layout.status.width - spinner_size - px(s, 14),
            height: layout.status.height,
        },
        if model.status.length == 0 {
            lit(b"Loading")
        } else {
            model.status
        },
        body,
        theme.text,
        false,
        2 | 4,
    )
    .0 && ok;
    let percent = format!("{}%", percentage).into_bytes();
    ok = text(
        layout.percentage,
        byte_vec(&percent),
        px(s, 42),
        theme.title,
        true,
        2 | 128,
    )
    .0 && ok;
    simple(5, Rect::default(), saved);
    ok
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_refit_paint_loading(
    width: i32,
    height: i32,
    input: LoadingPaintInput,
    backdrop: Artwork,
    preview: Artwork,
    transparent_overlay: i32,
    draw: Option<LoadingDraw>,
    context: *mut c_void,
) -> i32 {
    if input.protected_count != 0 && input.protected_regions.is_null()
        || input.briefing_count != 0 && input.briefing.is_null()
        || input.objective_count != 0 && input.objectives.is_null()
        || input.player_count != 0 && input.players.is_null()
    {
        return 0;
    }
    let Some(draw) = draw else {
        return 0;
    };
    i32::from(loading_paint(
        width,
        height,
        input,
        backdrop,
        preview,
        transparent_overlay != 0,
        draw,
        context,
    ))
}
