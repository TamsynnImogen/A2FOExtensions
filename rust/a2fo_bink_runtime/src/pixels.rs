//! Checked YUV420P frame copy into the observed BinkCopyToBuffer surface formats.
//!
//! Uses integer limited-range BT.601 and nearest 2x2 chroma. The native
//! converter's rounding is not proven bit-exact by this module.
use infinitier_bik_decoder::{Plane, VideoFrame};

const ALLOWED_FLAGS: u32 = 0x8000_001f; // low format nibble, 0x10, COPYALL

pub fn bytes_per_pixel(flags: u32) -> Result<usize, String> {
    if flags & !ALLOWED_FLAGS != 0 {
        return Err(format!("unsupported copy flags 0x{flags:08x}"));
    }
    match flags & 0xf {
        1 | 2 => Ok(3),
        3 | 4 | 5 | 6 => Ok(4),
        8 | 9 | 10 => Ok(2),
        7 | 11 | 12 if cfg!(feature = "rgb16-evaluation") => Ok(2),
        format => Err(format!("unsupported copy format {format}")),
    }
}

fn validate_plane(plane: &Plane, width: usize, height: usize) -> Result<(), String> {
    if plane.width as usize != width || plane.height as usize != height || plane.stride < width {
        return Err("plane dimensions or stride do not match frame".into());
    }
    let needed = if height == 0 {
        0
    } else {
        (height - 1)
            .checked_mul(plane.stride)
            .and_then(|n| n.checked_add(width))
            .ok_or("plane span overflow")?
    };
    if plane.data.len() < needed {
        return Err("plane data shorter than visible rows".into());
    }
    Ok(())
}

fn sample(plane: &Plane, x: usize, y: usize) -> u8 {
    plane.data[y * plane.stride + x]
}

fn rgb(y: u8, u: u8, v: u8) -> (u8, u8, u8) {
    let c = (i32::from(y) - 16).max(0);
    let d = i32::from(u) - 128;
    let e = i32::from(v) - 128;
    let clamp = |n: i32| ((n + 128) >> 8).clamp(0, 255) as u8;
    (
        clamp(298 * c + 409 * e),
        clamp(298 * c - 100 * d - 208 * e),
        clamp(298 * c + 516 * d),
    )
}

fn pixel_bytes(format: u32, r: u8, g: u8, b: u8, a: u8) -> ([u8; 4], usize) {
    match format {
        1 => ([b, g, r, 0], 3),
        2 => ([r, g, b, 0], 3),
        3 => ([b, g, r, 0], 4),
        4 => ([r, g, b, 0], 4),
        5 => ([b, g, r, a], 4),
        6 => ([r, g, b, a], 4),
        7 => {
            let packed = ((u16::from(a) >> 4) << 12)
                | ((u16::from(r) >> 4) << 8)
                | ((u16::from(g) >> 4) << 4)
                | (u16::from(b) >> 4);
            let bytes = packed.to_le_bytes();
            ([bytes[0], bytes[1], 0, 0], 2)
        }
        8 | 9 => {
            let alpha = if format == 8 && a >= 128 { 0x8000 } else { 0 };
            let packed = alpha
                | ((u16::from(r) >> 3) << 10)
                | ((u16::from(g) >> 3) << 5)
                | (u16::from(b) >> 3);
            let bytes = packed.to_le_bytes();
            ([bytes[0], bytes[1], 0, 0], 2)
        }
        10 => {
            let packed =
                ((u16::from(r) >> 3) << 11) | ((u16::from(g) >> 2) << 5) | (u16::from(b) >> 3);
            let bytes = packed.to_le_bytes();
            ([bytes[0], bytes[1], 0, 0], 2)
        }
        11 => {
            let packed =
                ((u16::from(r) >> 2) << 10) | ((u16::from(g) >> 3) << 5) | (u16::from(b) >> 3);
            let bytes = packed.to_le_bytes();
            ([bytes[0], bytes[1], 0, 0], 2)
        }
        12 => {
            let packed =
                ((u16::from(r) >> 2) << 10) | ((u16::from(g) >> 2) << 4) | (u16::from(b) >> 4);
            let bytes = packed.to_le_bytes();
            ([bytes[0], bytes[1], 0, 0], 2)
        }
        _ => unreachable!("format validated by bytes_per_pixel"),
    }
}

/// Copy the visible portion of a decoded frame at destination pixel `(x, y)`.
///
/// `pitch` is bytes per destination row; `dest_height` is the declared row
/// count. Rows/columns outside the destination are clipped. Any invalid
/// request is rejected before a destination byte is modified.
pub fn copy_frame(
    frame: &VideoFrame,
    destination: &mut [u8],
    pitch: usize,
    dest_height: usize,
    x: usize,
    y: usize,
    flags: u32,
) -> Result<(), String> {
    let bpp = bytes_per_pixel(flags)?;
    let format = flags & 0xf;
    let width = frame.y.width as usize;
    let height = frame.y.height as usize;
    if width == 0 || height == 0 {
        return Err("zero-size source frame".into());
    }
    let chroma_width = width.div_ceil(2);
    let chroma_height = height.div_ceil(2);
    validate_plane(&frame.y, width, height)?;
    validate_plane(&frame.u, chroma_width, chroma_height)?;
    validate_plane(&frame.v, chroma_width, chroma_height)?;
    if let Some(alpha) = &frame.alpha {
        validate_plane(alpha, width, height)?;
    }
    let dest_len = pitch
        .checked_mul(dest_height)
        .ok_or("destination span overflow")?;
    if destination.len() < dest_len {
        return Err("destination shorter than pitch times height".into());
    }
    let dest_width = pitch / bpp;
    if x >= dest_width || y >= dest_height {
        return Ok(());
    }
    let copy_width = width.min(dest_width - x);
    let copy_height = height.min(dest_height - y);
    // This ensures all offsets below are representable and every pixel stays
    // inside a declared destination row, including rows with padding bytes.
    let first_col = x.checked_mul(bpp).ok_or("destination column overflow")?;
    let copied_bytes = copy_width.checked_mul(bpp).ok_or("copy width overflow")?;
    if first_col
        .checked_add(copied_bytes)
        .is_none_or(|n| n > pitch)
    {
        return Err("copy extends past destination pitch".into());
    }
    for row in 0..copy_height {
        let dest_row = (y + row) * pitch + first_col;
        for col in 0..copy_width {
            let yy = sample(&frame.y, col, row);
            let uu = sample(&frame.u, col / 2, row / 2);
            let vv = sample(&frame.v, col / 2, row / 2);
            let alpha = frame.alpha.as_ref().map_or(0, |p| sample(p, col, row));
            let (r, g, b) = rgb(yy, uu, vv);
            let (bytes, size) = pixel_bytes(format, r, g, b, alpha);
            let at = dest_row + col * bpp;
            destination[at..at + size].copy_from_slice(&bytes[..size]);
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn solid(width: u32, height: u32, y: u8, u: u8, v: u8, alpha: Option<u8>) -> VideoFrame {
        let mut f = VideoFrame::new(width, height, alpha.is_some());
        f.y.data.fill(y);
        f.u.data.fill(u);
        f.v.data.fill(v);
        if let (Some(plane), Some(value)) = (&mut f.alpha, alpha) {
            plane.data.fill(value);
        }
        f
    }

    #[test]
    fn black_white_and_red_bt601() {
        let mut out = [99u8; 12];
        copy_frame(&solid(1, 1, 16, 128, 128, None), &mut out, 12, 1, 0, 0, 3).unwrap();
        assert_eq!(&out[..4], &[0, 0, 0, 0]);
        copy_frame(&solid(1, 1, 235, 128, 128, None), &mut out, 12, 1, 1, 0, 4).unwrap();
        assert_eq!(&out[4..8], &[255, 255, 255, 0]);
        copy_frame(&solid(1, 1, 81, 90, 240, None), &mut out, 12, 1, 2, 0, 5).unwrap();
        assert_eq!(&out[8..12], &[0, 0, 255, 0]);
    }

    #[test]
    fn every_supported_format_and_alpha() {
        let frame = solid(1, 1, 81, 90, 240, Some(200));
        let cases: &[(u32, &[u8])] = &[
            (1, &[0, 0, 255]),
            (2, &[255, 0, 0]),
            (3, &[0, 0, 255, 0]),
            (4, &[255, 0, 0, 0]),
            (5, &[0, 0, 255, 200]),
            (6, &[255, 0, 0, 200]),
            (8, &[0, 0xfc]),
            (9, &[0, 0x7c]),
            (10, &[0, 0xf8]),
        ];
        for &(format, expected) in cases {
            let mut out = [0x55u8; 4];
            copy_frame(&frame, &mut out, expected.len(), 1, 0, 0, format).unwrap();
            assert_eq!(&out[..expected.len()], expected, "format {format}");
        }
        let mut out = [0u8; 2];
        copy_frame(&solid(1, 1, 81, 90, 240, Some(42)), &mut out, 2, 1, 0, 0, 8).unwrap();
        assert_eq!(out, [0, 0x7c]); // alpha bit clear below midpoint
    }

    #[cfg(feature = "rgb16-evaluation")]
    #[test]
    fn rgb16_masks_quantize_each_channel_and_argb4444_alpha() {
        // Exercise the packed channels directly so YUV conversion rounding
        // cannot obscure their mask and shift behavior.
        assert_eq!(
            pixel_bytes(7, 0xab, 0xcd, 0xef, 0x76),
            ([0xce, 0x7a, 0, 0], 2)
        );
        assert_eq!(
            pixel_bytes(7, 0xff, 0xff, 0xff, 0xff),
            ([0xff, 0xff, 0, 0], 2)
        );
        assert_eq!(
            pixel_bytes(11, 0xff, 0xff, 0xff, 0),
            ([0xff, 0xff, 0, 0], 2)
        );
        assert_eq!(pixel_bytes(11, 0x03, 0x07, 0x07, 0), ([0, 0, 0, 0], 2));
        assert_eq!(
            pixel_bytes(12, 0xff, 0xff, 0xff, 0),
            ([0xff, 0xff, 0, 0], 2)
        );
        assert_eq!(pixel_bytes(12, 0x03, 0x03, 0x0f, 0), ([0, 0, 0, 0], 2));
    }

    #[cfg(feature = "rgb16-evaluation")]
    #[test]
    fn rgb16_destination_clip_padding_and_invalid_format_guard() {
        for format in [7, 11, 12] {
            assert_eq!(bytes_per_pixel(format), Ok(2));
        }
        let frame = solid(2, 1, 235, 128, 128, Some(255));
        let mut out = [0x5a; 7]; // three pixels plus one byte of row padding
        copy_frame(&frame, &mut out, 7, 1, 2, 0, 7).unwrap();
        assert_eq!(&out[..4], &[0x5a; 4]);
        assert_eq!(&out[4..6], &[0xff, 0xff]);
        assert_eq!(out[6], 0x5a);

        let untouched = [0xa5; 4];
        let mut guarded = untouched;
        for format in [0, 13, 14, 15, 0x20 | 11, 0x4000_000b] {
            assert!(copy_frame(&frame, &mut guarded, 4, 1, 0, 0, format).is_err());
        }
        assert_eq!(guarded, untouched);
    }

    #[cfg(not(feature = "rgb16-evaluation"))]
    #[test]
    fn rgb16_evaluation_formats_are_rejected_before_destination_mutation() {
        let frame = solid(1, 1, 235, 128, 128, Some(255));
        let original = [0xa5; 8];
        let mut destination = original;
        for format in [7, 11, 12] {
            assert!(bytes_per_pixel(format).is_err(), "format {format}");
            assert!(copy_frame(&frame, &mut destination, 8, 1, 0, 0, format).is_err());
            assert_eq!(destination, original, "format {format}");
        }
    }

    #[test]
    fn absent_alpha_is_zero_in_native_alpha_formats() {
        let frame = solid(1, 1, 81, 90, 240, None);
        for format in [5, 6] {
            let mut out = [0xffu8; 4];
            copy_frame(&frame, &mut out, 4, 1, 0, 0, format).unwrap();
            assert_eq!(out[3], 0, "format {format}");
        }
        let mut packed = [0xffu8; 2];
        copy_frame(&frame, &mut packed, 2, 1, 0, 0, 8).unwrap();
        assert_eq!(u16::from_le_bytes(packed) & 0x8000, 0);
        copy_frame(&frame, &mut packed, 2, 1, 0, 0, 9).unwrap();
        assert_eq!(u16::from_le_bytes(packed) & 0x8000, 0);
    }

    #[test]
    fn odd_one_by_one_and_chroma_nearest() {
        let mut frame = solid(3, 1, 81, 90, 240, None);
        // Chroma width is ceil(3/2) = 2; the third luma pixel uses U/V[1].
        frame.u.data[1] = 128;
        frame.v.data[1] = 128;
        let mut out = [0u8; 9];
        copy_frame(&frame, &mut out, 9, 1, 0, 0, 2).unwrap();
        assert_eq!(&out[..6], &[255, 0, 0, 255, 0, 0]);
        assert_eq!(&out[6..9], &[76, 76, 76]);
        let mut single = [0u8; 3];
        copy_frame(&solid(1, 1, 16, 128, 128, None), &mut single, 3, 1, 0, 0, 2).unwrap();
        assert_eq!(single, [0, 0, 0]);
    }

    #[test]
    fn placement_clip_and_padding() {
        let frame = solid(2, 2, 235, 128, 128, None);
        let mut out = [0x55u8; 26]; // two 13-byte rows, three 4-byte pixels each
        copy_frame(&frame, &mut out, 13, 2, 2, 1, 0x8000_0013).unwrap();
        assert_eq!(&out[21..25], &[255, 255, 255, 0]);
        assert!(out[..21].iter().all(|&x| x == 0x55));
        assert_eq!(out[25], 0x55); // row padding stays untouched
    }

    #[test]
    fn invalid_requests_never_modify_destination() {
        let frame = solid(1, 1, 81, 90, 240, None);
        let mut out = [0x5au8; 8];
        for flags in [0, 13, 0x20, 0x4000_0003] {
            assert!(copy_frame(&frame, &mut out, 4, 2, 0, 0, flags).is_err());
        }
        assert!(copy_frame(&frame, &mut out, usize::MAX, 2, 0, 0, 3).is_err());
        assert!(copy_frame(&frame, &mut out, 4, 3, 0, 0, 3).is_err());
        let mut bad = frame.clone();
        bad.u.data.clear();
        assert!(copy_frame(&bad, &mut out, 4, 2, 0, 0, 3).is_err());
        assert_eq!(out, [0x5a; 8]);
    }
}
