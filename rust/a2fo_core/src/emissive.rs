//! ODF emissive classification and image algorithms behind borrowed C spans.
use crate::legacy_float;
use core::slice;
unsafe extern "C" {
    fn isspace(value: i32) -> i32;
    fn tolower(value: i32) -> i32;
}
#[repr(C)]
pub struct BloomPlan {
    valid: u8,
    process: u8,
    radius: usize,
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_emissive_subsystem(
    operational: u8,
    forced: u8,
    maximum: i32,
    current: f64,
    disable: f32,
) -> i32 {
    if operational != 0 {
        return 0;
    }
    if !current.is_finite() || !disable.is_finite() || maximum <= 0 {
        return 0;
    }
    if (forced == 0 && disable <= 0. && legacy_float::below_maximum(current, maximum))
        || current <= 0.
    {
        2
    } else {
        1
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_emissive_motion(
    warp_state: u32,
    velocity_squared: f32,
    impulse_speed: f32,
) -> u8 {
    let moving = velocity_squared.is_finite() && velocity_squared > 0.01;
    if warp_state <= 3 {
        if warp_state == 2 {
            return 3;
        }
        if warp_state == 0 && moving {
            return 2;
        }
        return 1;
    }
    if !moving {
        return 0;
    }
    if impulse_speed.is_finite() && impulse_speed > 0. {
        let threshold = impulse_speed * 1.05;
        if threshold.is_finite() && velocity_squared > threshold * threshold {
            return 3;
        }
    }
    2
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_emissive_intensity(state: u8, output: *mut u32) {
    let result = unsafe { slice::from_raw_parts_mut(output, 6) };
    result.fill(100);
    match state {
        1 => result[0] = 125,
        2 => {
            result[0] = 125;
            result[1] = 150;
        }
        3 => result[0] = 200,
        _ => {}
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_emissive_key(
    input: *const u8,
    length: usize,
    output: *mut u8,
    capacity: usize,
) -> usize {
    let value = if length == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(input, length) }
    };
    let mut first = 0;
    while first < length && unsafe { isspace(value[first] as i32) } != 0 {
        first += 1;
    }
    let mut last = length;
    while last > first && unsafe { isspace(value[last - 1] as i32) } != 0 {
        last -= 1;
    }
    // std::string::find_last_of with position (last - 1) treats underflow as npos.
    let search_end = last.wrapping_sub(1).min(length.saturating_sub(1));
    if let Some(slash) = (0..=search_end)
        .rev()
        .find(|&i| i < length && (value[i] == b'\\' || value[i] == b'/'))
        && slash >= first
    {
        first = slash + 1;
    }
    if let Some(dot) = (0..=search_end)
        .rev()
        .find(|&i| i < length && value[i] == b'.')
        && dot > first
    {
        last = dot;
    }
    let count = last.saturating_sub(first);
    if output.is_null() {
        return count;
    }
    if count > capacity {
        return 0;
    }
    for (index, &character) in value[first..last].iter().enumerate() {
        unsafe { *output.add(index) = tolower(character as i32) as u8 };
    }
    count
}
fn scaled(channel: u32, percent: u32) -> u32 {
    ((channel as u64 * percent as u64 + 50) / 100).min(255) as u32
}
fn merge(combined: u32, source: u32, percent: u32) -> u32 {
    let red = ((combined >> 16) & 255).max(scaled((source >> 16) & 255, percent));
    let green = ((combined >> 8) & 255).max(scaled((source >> 8) & 255, percent));
    let blue = (combined & 255).max(scaled(source & 255, percent));
    0xff000000 | (red << 16) | (green << 8) | blue
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_emissive_merge(combined: u32, source: u32, percent: u32) -> u32 {
    merge(combined, source, percent)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_emissive_combine(
    pixels: *const u32,
    mask: u8,
    percent: *const u32,
) -> u32 {
    let pixels = unsafe { slice::from_raw_parts(pixels, 6) };
    let intensity = if percent.is_null() {
        None
    } else {
        Some(unsafe { slice::from_raw_parts(percent, 6) })
    };
    let mut combined = 0xff000000;
    for index in 0..6 {
        if mask & (1 << index) == 0 {
            continue;
        }
        combined = merge(combined, pixels[index], intensity.map_or(100, |p| p[index]));
    }
    combined
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_emissive_bloom_plan(
    length: usize,
    width: usize,
    height: usize,
    radius: usize,
    strength: u32,
) -> BloomPlan {
    if width == 0 || height == 0 || width > length / height || width * height != length {
        return BloomPlan {
            valid: 0,
            process: 0,
            radius: 0,
        };
    }
    if radius == 0 || strength == 0 {
        return BloomPlan {
            valid: 1,
            process: 0,
            radius,
        };
    }
    BloomPlan {
        valid: 1,
        process: 1,
        radius: radius.min(width.max(height) - 1),
    }
}
fn add_pixel(pixel: u32, red: &mut u64, green: &mut u64, blue: &mut u64) {
    *red = red.wrapping_add(((pixel >> 16) & 255) as u64);
    *green = green.wrapping_add(((pixel >> 8) & 255) as u64);
    *blue = blue.wrapping_add((pixel & 255) as u64);
}
fn remove_pixel(pixel: u32, red: &mut u64, green: &mut u64, blue: &mut u64) {
    *red = red.wrapping_sub(((pixel >> 16) & 255) as u64);
    *green = green.wrapping_sub(((pixel >> 8) & 255) as u64);
    *blue = blue.wrapping_sub((pixel & 255) as u64);
}
fn average(red: u64, green: u64, blue: u64, count: usize) -> u32 {
    if count == 0 {
        return 0xff000000;
    }
    0xff000000
        | (((red / count as u64) as u32) << 16)
        | (((green / count as u64) as u32) << 8)
        | ((blue / count as u64) as u32)
}
fn horizontal(source: &[u32], destination: &mut [u32], width: usize, height: usize, radius: usize) {
    for y in 0..height {
        let row = y * width;
        let initial = radius.min(width - 1);
        let (mut red, mut green, mut blue) = (0, 0, 0);
        let mut count = initial + 1;
        for x in 0..=initial {
            add_pixel(source[row + x], &mut red, &mut green, &mut blue);
        }
        for x in 0..width {
            destination[row + x] = average(red, green, blue, count);
            let next = x + 1;
            if next >= width {
                break;
            }
            if next > radius {
                remove_pixel(
                    source[row + next - radius - 1],
                    &mut red,
                    &mut green,
                    &mut blue,
                );
                count -= 1;
            }
            if next + radius < width {
                add_pixel(source[row + next + radius], &mut red, &mut green, &mut blue);
                count += 1;
            }
        }
    }
}
fn vertical(source: &[u32], destination: &mut [u32], width: usize, height: usize, radius: usize) {
    for x in 0..width {
        let initial = radius.min(height - 1);
        let (mut red, mut green, mut blue) = (0, 0, 0);
        let mut count = initial + 1;
        for y in 0..=initial {
            add_pixel(source[y * width + x], &mut red, &mut green, &mut blue);
        }
        for y in 0..height {
            destination[y * width + x] = average(red, green, blue, count);
            let next = y + 1;
            if next >= height {
                break;
            }
            if next > radius {
                remove_pixel(
                    source[(next - radius - 1) * width + x],
                    &mut red,
                    &mut green,
                    &mut blue,
                );
                count -= 1;
            }
            if next + radius < height {
                add_pixel(
                    source[(next + radius) * width + x],
                    &mut red,
                    &mut green,
                    &mut blue,
                );
                count += 1;
            }
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_emissive_bloom_process(
    pixels: *mut u32,
    original: *const u32,
    scratch: *mut u32,
    blurred: *mut u32,
    length: usize,
    width: usize,
    height: usize,
    radius: usize,
    strength: u32,
) {
    let pixels = unsafe { slice::from_raw_parts_mut(pixels, length) };
    let original = unsafe { slice::from_raw_parts(original, length) };
    let scratch = unsafe { slice::from_raw_parts_mut(scratch, length) };
    let blurred = unsafe { slice::from_raw_parts_mut(blurred, length) };
    horizontal(original, scratch, width, height, radius);
    vertical(scratch, blurred, width, height, radius);
    horizontal(blurred, scratch, width, height, radius);
    vertical(scratch, blurred, width, height, radius);
    for index in 0..length {
        let source = original[index];
        let glow = blurred[index];
        let channel = |base: u32, bloom: u32| -> u32 {
            (base as u64 + (bloom as u64 * strength as u64 / 100)).min(255) as u32
        };
        let red = channel((source >> 16) & 255, (glow >> 16) & 255);
        let green = channel((source >> 8) & 255, (glow >> 8) & 255);
        let blue = channel(source & 255, glow & 255);
        pixels[index] = 0xff000000 | (red << 16) | (green << 8) | blue;
    }
}
