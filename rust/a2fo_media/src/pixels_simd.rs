//! Four-pixel SSE2 color conversion. The safe wrapper checks every extent.
use super::{Matrix, yuv_to_rgb};
#[cfg(target_arch = "x86")]
use std::arch::x86::*;
#[cfg(target_arch = "x86_64")]
use std::arch::x86_64::*;

pub(super) fn row(y: &[f32], cb: &[f32], cr: &[f32], out: &mut [u8], matrix: Matrix, full: bool) {
    assert_eq!(cb.len(), y.len());
    assert_eq!(cr.len(), y.len());
    assert_eq!(out.len(), y.len() * 4);
    let mut count = 0;
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    if std::is_x86_feature_detected!("sse2") {
        // SAFETY: SSE2 is available; checked slices contain each four-float
        // load and sixteen-byte store. Inputs are finite interpolated u8s.
        count = unsafe { vector(y, cb, cr, out, matrix, full) };
    }
    for i in count..y.len() {
        let (r, g, b) = yuv_to_rgb(y[i], cb[i], cr[i], matrix, full);
        out[i * 4..i * 4 + 4].copy_from_slice(&[b, g, r, 255]);
    }
}
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "sse2")]
unsafe fn vector(
    y: &[f32],
    cb: &[f32],
    cr: &[f32],
    out: &mut [u8],
    matrix: Matrix,
    full: bool,
) -> usize {
    // Explicit operations keep the scalar f32 order, without FMA or a biased
    // add-0.5 shortcut at values immediately below half-integers.
    unsafe {
        let (kr, kb) = match matrix {
            Matrix::Bt601 => (0.299f32, 0.114f32),
            Matrix::Bt709 => (0.2126f32, 0.0722f32),
            Matrix::Bt2020 => (0.2627f32, 0.0593f32),
        };
        let kg = 1. - kr - kb;
        let red_scale = _mm_set1_ps(2. * (1. - kr));
        let blue_scale = _mm_set1_ps(2. * (1. - kb));
        let green_blue = _mm_set1_ps(2. * kb * (1. - kb) / kg);
        let green_red = _mm_set1_ps(2. * kr * (1. - kr) / kg);
        let offset = _mm_set1_ps(128.);
        let luma_offset = _mm_set1_ps(16.);
        let luma_scale = _mm_set1_ps(255. / 219.);
        let chroma_scale = _mm_set1_ps(255. / 224.);
        let count = y.len() / 4 * 4;
        for i in (0..count).step_by(4) {
            let mut lum = _mm_loadu_ps(y.as_ptr().add(i));
            let mut bd = _mm_sub_ps(_mm_loadu_ps(cb.as_ptr().add(i)), offset);
            let mut rd = _mm_sub_ps(_mm_loadu_ps(cr.as_ptr().add(i)), offset);
            if !full {
                lum = _mm_mul_ps(_mm_sub_ps(lum, luma_offset), luma_scale);
                bd = _mm_mul_ps(bd, chroma_scale);
                rd = _mm_mul_ps(rd, chroma_scale);
            }
            let red = _mm_add_ps(lum, _mm_mul_ps(red_scale, rd));
            let blue = _mm_add_ps(lum, _mm_mul_ps(blue_scale, bd));
            let green = _mm_sub_ps(
                _mm_sub_ps(lum, _mm_mul_ps(green_blue, bd)),
                _mm_mul_ps(green_red, rd),
            );
            let r = bytes(red);
            let g = bytes(green);
            let b = bytes(blue);
            let a = _mm_set1_epi8(-1);
            let bg = _mm_unpacklo_epi8(b, g);
            let ra = _mm_unpacklo_epi8(r, a);
            _mm_storeu_si128(
                out.as_mut_ptr().add(i * 4).cast(),
                _mm_unpacklo_epi16(bg, ra),
            );
        }
        count
    }
}
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "sse2")]
unsafe fn bytes(v: __m128) -> __m128i {
    let v = _mm_min_ps(_mm_max_ps(v, _mm_setzero_ps()), _mm_set1_ps(255.));
    let base = _mm_cvttps_epi32(v);
    let fraction = _mm_sub_ps(v, _mm_cvtepi32_ps(base));
    let rounded = _mm_sub_epi32(
        base,
        _mm_castps_si128(_mm_cmpge_ps(fraction, _mm_set1_ps(0.5))),
    );
    let words = _mm_packs_epi32(rounded, rounded);
    _mm_packus_epi16(words, words)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn vector_matches_scalar_for_all_matrices_ranges_and_tails() {
        let mut seed = 0x127829u32;
        let mut y = Vec::new();
        let mut cb = Vec::new();
        let mut cr = Vec::new();
        for i in 0..4099 {
            let mut sample = || {
                seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
                (seed % 65536) as f32 / 257.
            };
            y.push(if i < 256 { i as f32 } else { sample() });
            cb.push(sample());
            cr.push(sample());
        }
        for matrix in [Matrix::Bt601, Matrix::Bt709, Matrix::Bt2020] {
            for full in [false, true] {
                let mut result = vec![0; y.len() * 4];
                row(&y, &cb, &cr, &mut result, matrix, full);
                for i in 0..y.len() {
                    let (r, g, b) = yuv_to_rgb(y[i], cb[i], cr[i], matrix, full);
                    assert_eq!(
                        &result[i * 4..i * 4 + 4],
                        &[b, g, r, 255],
                        "pixel {i} {matrix:?} full={full}"
                    );
                }
            }
        }
    }
}

pub(super) fn sample(
    top: &[u8],
    bottom: &[u8],
    xs: &[super::Weight],
    fraction: f32,
    out: &mut [f32],
) -> usize {
    assert_eq!(out.len(), xs.len());
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    if std::is_x86_feature_detected!("sse2") {
        // SAFETY: feature checked; all gathers below use checked slice reads.
        // Each store is four floats inside out's checked extent.
        return unsafe { sample_vector(top, bottom, xs, fraction, out) };
    }
    0
}
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "sse2")]
unsafe fn sample_vector(
    top: &[u8],
    bottom: &[u8],
    xs: &[super::Weight],
    fraction: f32,
    out: &mut [f32],
) -> usize {
    unsafe {
        let count = xs.len() / 4 * 4;
        let one = _mm_set1_ps(1.);
        let vertical = _mm_set1_ps(fraction);
        let inverse_y = _mm_set1_ps(1. - fraction);
        for i in (0..count).step_by(4) {
            let [a, b, c, d] = [xs[i], xs[i + 1], xs[i + 2], xs[i + 3]];
            let x = _mm_set_ps(d.fraction, c.fraction, b.fraction, a.fraction);
            let inverse_x = _mm_sub_ps(one, x);
            let upper_lo = _mm_cvtepi32_ps(_mm_set_epi32(
                top[d.lo] as i32,
                top[c.lo] as i32,
                top[b.lo] as i32,
                top[a.lo] as i32,
            ));
            let upper_hi = _mm_cvtepi32_ps(_mm_set_epi32(
                top[d.hi] as i32,
                top[c.hi] as i32,
                top[b.hi] as i32,
                top[a.hi] as i32,
            ));
            let upper = _mm_add_ps(_mm_mul_ps(upper_lo, inverse_x), _mm_mul_ps(upper_hi, x));
            let result = if fraction == 0. {
                upper
            } else {
                let lower_lo = _mm_cvtepi32_ps(_mm_set_epi32(
                    bottom[d.lo] as i32,
                    bottom[c.lo] as i32,
                    bottom[b.lo] as i32,
                    bottom[a.lo] as i32,
                ));
                let lower_hi = _mm_cvtepi32_ps(_mm_set_epi32(
                    bottom[d.hi] as i32,
                    bottom[c.hi] as i32,
                    bottom[b.hi] as i32,
                    bottom[a.hi] as i32,
                ));
                let lower = _mm_add_ps(_mm_mul_ps(lower_lo, inverse_x), _mm_mul_ps(lower_hi, x));
                _mm_add_ps(_mm_mul_ps(upper, inverse_y), _mm_mul_ps(lower, vertical))
            };
            _mm_storeu_ps(out.as_mut_ptr().add(i), result);
        }
        count
    }
}
