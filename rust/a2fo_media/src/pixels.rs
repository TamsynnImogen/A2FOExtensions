//! Validated, top-down BGRA composition for decoded video frames.
//!
//! Chroma siting is explicit. `Center` places chroma at the footprint center;
//! `Left` aligns it with the left luma sample and centers it vertically;
//! `TopLeft` aligns both axes. Edges extend under bilinear sampling.

#[path = "pixels_simd.rs"]
mod simd;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Layout {
    Yuv420,
    Yuv422,
    Yuv444,
    Bgra,
    Rgba,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Matrix {
    Bt601,
    Bt709,
    Bt2020,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ChromaLocation {
    Center,
    Left,
    TopLeft,
}

pub struct Plane<'a> {
    pub data: &'a [u8],
    pub stride: usize,
}

pub struct Image<'a> {
    pub width: usize,
    pub height: usize,
    pub layout: Layout,
    pub matrix: Matrix,
    pub full_range: bool,
    pub chroma_location: ChromaLocation,
    pub planes: Vec<Plane<'a>>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Rect {
    pub left: usize,
    pub top: usize,
    pub width: usize,
    pub height: usize,
}

#[derive(Clone, Copy)]
struct Weight {
    lo: usize,
    hi: usize,
    fraction: f32,
}

pub struct Composer {
    luma_x: Vec<Weight>,
    luma_y: Vec<Weight>,
    chroma_x: Vec<Weight>,
    chroma_y: Vec<Weight>,
    rows: [Vec<f32>; 3],
    cache: [RowCache; 3],
}

impl Composer {
    pub fn new() -> Self {
        Self {
            luma_x: Vec::new(),
            luma_y: Vec::new(),
            chroma_x: Vec::new(),
            chroma_y: Vec::new(),
            rows: std::array::from_fn(|_| Vec::new()),
            cache: std::array::from_fn(|_| RowCache::default()),
        }
    }

    pub fn compose(
        &mut self,
        source: &Image<'_>,
        width: usize,
        height: usize,
        output: &mut [u8],
    ) -> Result<Rect, String> {
        validate_dimension(source.width, source.height)?;
        validate_dimension(width, height)?;
        let needed = width
            .checked_mul(height)
            .and_then(|n| n.checked_mul(4))
            .ok_or("output dimensions overflow")?;
        if output.len() < needed {
            return Err("output buffer is too short".into());
        }
        validate_planes(source)?;

        // This is the integer truncation and centering used by FramePresenter.
        let mut fitted_width = width;
        let mut fitted_height = source.height * width / source.width;
        if fitted_height > height {
            fitted_height = height;
            fitted_width = source.width * height / source.height;
        }
        if fitted_width == 0 || fitted_height == 0 {
            return Err("source aspect is too thin for the output".into());
        }
        let rect = Rect {
            left: (width - fitted_width) / 2,
            top: (height - fitted_height) / 2,
            width: fitted_width,
            height: fitted_height,
        };

        let (sx, sy) = chroma_subsampling(source.layout);
        axis_weights(
            &mut self.luma_x,
            source.width,
            source.width,
            fitted_width,
            1,
            0.0,
        );
        axis_weights(
            &mut self.luma_y,
            source.height,
            source.height,
            fitted_height,
            1,
            0.0,
        );
        if sx != 0 {
            let x_offset = if source.chroma_location == ChromaLocation::Center {
                (sx - 1) as f32 * 0.5
            } else {
                0.0
            };
            let y_offset = if source.chroma_location == ChromaLocation::TopLeft {
                0.0
            } else {
                (sy - 1) as f32 * 0.5
            };
            axis_weights(
                &mut self.chroma_x,
                (source.width + sx - 1) / sx,
                source.width,
                fitted_width,
                sx,
                x_offset,
            );
            axis_weights(
                &mut self.chroma_y,
                (source.height + sy - 1) / sy,
                source.height,
                fitted_height,
                sy,
                y_offset,
            );
        }

        for cache in &mut self.cache {
            cache.ids = [None, None];
        }
        output[..needed].fill(0);
        for y in 0..fitted_height {
            let row = ((rect.top + y) * width + rect.left) * 4;
            if sx != 0 {
                for values in &mut self.rows {
                    values.resize(fitted_width, 0.);
                }
                self.cache[0].sample(
                    &source.planes[0],
                    &self.luma_x,
                    self.luma_y[y],
                    &mut self.rows[0],
                    source.width == fitted_width,
                );
                self.cache[1].sample(
                    &source.planes[1],
                    &self.chroma_x,
                    self.chroma_y[y],
                    &mut self.rows[1],
                    sx == 1 && source.width == fitted_width,
                );
                self.cache[2].sample(
                    &source.planes[2],
                    &self.chroma_x,
                    self.chroma_y[y],
                    &mut self.rows[2],
                    sx == 1 && source.width == fitted_width,
                );
                simd::row(
                    &self.rows[0],
                    &self.rows[1],
                    &self.rows[2],
                    &mut output[row..row + fitted_width * 4],
                    source.matrix,
                    source.full_range,
                );
                continue;
            }
            for x in 0..fitted_width {
                let pixel = &mut output[row + x * 4..row + x * 4 + 4];
                match source.layout {
                    Layout::Bgra | Layout::Rgba => {
                        let p = &source.planes[0];
                        let wx = self.luma_x[x];
                        let wy = self.luma_y[y];
                        let (b, g, r) = if source.layout == Layout::Bgra {
                            (0, 1, 2)
                        } else {
                            (2, 1, 0)
                        };
                        pixel[0] = byte(sample(p, wx, wy, 4, b));
                        pixel[1] = byte(sample(p, wx, wy, 4, g));
                        pixel[2] = byte(sample(p, wx, wy, 4, r));
                    }
                    _ => {
                        let lum = sample(&source.planes[0], self.luma_x[x], self.luma_y[y], 1, 0);
                        let cb =
                            sample(&source.planes[1], self.chroma_x[x], self.chroma_y[y], 1, 0);
                        let cr =
                            sample(&source.planes[2], self.chroma_x[x], self.chroma_y[y], 1, 0);
                        let (red, green, blue) =
                            yuv_to_rgb(lum, cb, cr, source.matrix, source.full_range);
                        pixel[0] = blue;
                        pixel[1] = green;
                        pixel[2] = red;
                    }
                }
                pixel[3] = 255;
            }
        }
        // BI_RGB 32-bit DIB ignores alpha, but an explicit opaque channel also
        // gives consumers a well-defined full-client BGRA image.
        for pixel in output[..needed].chunks_exact_mut(4) {
            pixel[3] = 255;
        }
        Ok(rect)
    }
}

impl Default for Composer {
    fn default() -> Self {
        Self::new()
    }
}

fn validate_dimension(width: usize, height: usize) -> Result<(), String> {
    if width == 0 || height == 0 || width > 16384 || height > 16384 {
        Err("dimensions must be in 1..=16384".into())
    } else {
        Ok(())
    }
}

fn chroma_subsampling(layout: Layout) -> (usize, usize) {
    match layout {
        Layout::Yuv420 => (2, 2),
        Layout::Yuv422 => (2, 1),
        Layout::Yuv444 => (1, 1),
        _ => (0, 0),
    }
}

fn validate_planes(image: &Image<'_>) -> Result<(), String> {
    let (sx, sy) = chroma_subsampling(image.layout);
    let expected = if sx == 0 { 1 } else { 3 };
    if image.planes.len() != expected {
        return Err("unexpected number of planes".into());
    }
    for (index, plane) in image.planes.iter().enumerate() {
        let (pw, ph, bpp) = if index == 0 {
            (image.width, image.height, if sx == 0 { 4 } else { 1 })
        } else {
            ((image.width + sx - 1) / sx, (image.height + sy - 1) / sy, 1)
        };
        let row_bytes = pw * bpp;
        if plane.stride < row_bytes {
            return Err(format!("plane {index} stride is too short"));
        }
        let needed = (ph - 1)
            .checked_mul(plane.stride)
            .and_then(|n| n.checked_add(row_bytes))
            .ok_or_else(|| format!("plane {index} extent overflows"))?;
        if plane.data.len() < needed {
            return Err(format!("plane {index} data is too short"));
        }
    }
    Ok(())
}

fn axis_weights(
    weights: &mut Vec<Weight>,
    source_len: usize,
    luma_len: usize,
    target_len: usize,
    subsampling: usize,
    offset: f32,
) {
    weights.clear();
    weights.reserve(target_len.saturating_sub(weights.capacity()));
    for target in 0..target_len {
        let luma_position = (target as f32 + 0.5) * luma_len as f32 / target_len as f32 - 0.5;
        let position = (luma_position - offset) / subsampling as f32;
        let position = position.clamp(0.0, (source_len - 1) as f32);
        let lo = position.floor() as usize;
        weights.push(Weight {
            lo,
            hi: (lo + 1).min(source_len - 1),
            fraction: position - lo as f32,
        });
    }
}

#[derive(Default)]
struct RowCache {
    ids: [Option<usize>; 2],
    values: [Vec<f32>; 2],
}
impl RowCache {
    fn fill(
        &mut self,
        slot: usize,
        id: usize,
        plane: &Plane<'_>,
        weights: &[Weight],
        identity: bool,
    ) {
        let source = &plane.data[id * plane.stride..];
        let out = &mut self.values[slot];
        out.resize(weights.len(), 0.);
        if identity {
            for (value, byte) in out.iter_mut().zip(source) {
                *value = *byte as f32;
            }
        } else {
            let count = simd::sample(source, source, weights, 0., out);
            for (value, x) in out[count..].iter_mut().zip(&weights[count..]) {
                *value = source[x.lo] as f32 * (1. - x.fraction) + source[x.hi] as f32 * x.fraction;
            }
        }
        self.ids[slot] = Some(id);
    }
    fn sample(
        &mut self,
        plane: &Plane<'_>,
        weights: &[Weight],
        y: Weight,
        out: &mut [f32],
        identity: bool,
    ) {
        let lo = if self.ids[0] == Some(y.lo) {
            0
        } else if self.ids[1] == Some(y.lo) {
            1
        } else if self.ids[0] == Some(y.hi) {
            1
        } else {
            0
        };
        if self.ids[lo] != Some(y.lo) {
            self.fill(lo, y.lo, plane, weights, identity);
        }
        if y.fraction == 0. {
            out.copy_from_slice(&self.values[lo]);
            return;
        }
        let hi = if self.ids[0] == Some(y.hi) {
            0
        } else if self.ids[1] == Some(y.hi) {
            1
        } else {
            1 - lo
        };
        if self.ids[hi] != Some(y.hi) {
            self.fill(hi, y.hi, plane, weights, identity);
        }
        for ((value, upper), lower) in out.iter_mut().zip(&self.values[lo]).zip(&self.values[hi]) {
            *value = *upper * (1. - y.fraction) + *lower * y.fraction;
        }
    }
}

fn sample(plane: &Plane<'_>, x: Weight, y: Weight, bpp: usize, channel: usize) -> f32 {
    let at = |xx: usize, yy: usize| plane.data[yy * plane.stride + xx * bpp + channel] as f32;
    let upper = at(x.lo, y.lo) * (1.0 - x.fraction) + at(x.hi, y.lo) * x.fraction;
    let lower = at(x.lo, y.hi) * (1.0 - x.fraction) + at(x.hi, y.hi) * x.fraction;
    upper * (1.0 - y.fraction) + lower * y.fraction
}

fn byte(value: f32) -> u8 {
    if value <= 0.0 {
        0
    } else if value >= 255.0 {
        255
    } else {
        (f64::from(value) + 0.5) as u8
    }
}

fn yuv_to_rgb(y: f32, cb: f32, cr: f32, matrix: Matrix, full_range: bool) -> (u8, u8, u8) {
    let (kr, kb) = match matrix {
        Matrix::Bt601 => (0.299_f32, 0.114_f32),
        Matrix::Bt709 => (0.2126_f32, 0.0722_f32),
        Matrix::Bt2020 => (0.2627_f32, 0.0593_f32),
    };
    let kg = 1.0 - kr - kb;
    let (luma, blue_difference, red_difference) = if full_range {
        (y, cb - 128.0, cr - 128.0)
    } else {
        (
            (y - 16.0) * (255.0 / 219.0),
            (cb - 128.0) * (255.0 / 224.0),
            (cr - 128.0) * (255.0 / 224.0),
        )
    };
    let red = luma + 2.0 * (1.0 - kr) * red_difference;
    let blue = luma + 2.0 * (1.0 - kb) * blue_difference;
    let green = luma
        - 2.0 * kb * (1.0 - kb) / kg * blue_difference
        - 2.0 * kr * (1.0 - kr) / kg * red_difference;
    (byte(red), byte(green), byte(blue))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cached_rows_are_invalidated_between_frames() {
        let mut composer = Composer::new();
        let u = [77u8; 4];
        let v = [150u8; 4];
        let first = [16u8; 16];
        let second = [220u8; 16];
        // Separate source buffers keep the same geometry while changing data.
        let image1 = Image {
            width: 4,
            height: 4,
            layout: Layout::Yuv420,
            matrix: Matrix::Bt601,
            full_range: false,
            chroma_location: ChromaLocation::Left,
            planes: vec![
                Plane {
                    data: &first,
                    stride: 4,
                },
                Plane {
                    data: &u,
                    stride: 2,
                },
                Plane {
                    data: &v,
                    stride: 2,
                },
            ],
        };
        let image2 = Image {
            width: 4,
            height: 4,
            layout: Layout::Yuv420,
            matrix: Matrix::Bt601,
            full_range: false,
            chroma_location: ChromaLocation::Left,
            planes: vec![
                Plane {
                    data: &second,
                    stride: 4,
                },
                Plane {
                    data: &u,
                    stride: 2,
                },
                Plane {
                    data: &v,
                    stride: 2,
                },
            ],
        };
        let (mut a, mut b, mut fresh) =
            (vec![0; 9 * 7 * 4], vec![0; 9 * 7 * 4], vec![0; 9 * 7 * 4]);
        composer.compose(&image1, 9, 7, &mut a).unwrap();
        composer.compose(&image2, 9, 7, &mut b).unwrap();
        Composer::new().compose(&image2, 9, 7, &mut fresh).unwrap();
        assert_ne!(a, b);
        assert_eq!(b, fresh);
    }
    #[test]
    fn row_composition_matches_original_scalar_sampling() {
        let mut composer = Composer::new();
        for layout in [Layout::Yuv420, Layout::Yuv422, Layout::Yuv444] {
            let (sx, sy) = chroma_subsampling(layout);
            let (w, h) = (17usize, 11usize);
            let cw = w.div_ceil(sx);
            let ch = h.div_ceil(sy);
            let luma = (0..w * h).map(|i| (i * 17) as u8).collect::<Vec<_>>();
            let cb = (0..cw * ch).map(|i| (i * 31) as u8).collect::<Vec<_>>();
            let cr = (0..cw * ch).map(|i| (i * 43) as u8).collect::<Vec<_>>();
            for matrix in [Matrix::Bt601, Matrix::Bt709, Matrix::Bt2020] {
                for full_range in [false, true] {
                    for chroma_location in [
                        ChromaLocation::Left,
                        ChromaLocation::Center,
                        ChromaLocation::TopLeft,
                    ] {
                        let source = Image {
                            width: w,
                            height: h,
                            layout,
                            matrix,
                            full_range,
                            chroma_location,
                            planes: vec![
                                Plane {
                                    data: &luma,
                                    stride: w,
                                },
                                Plane {
                                    data: &cb,
                                    stride: cw,
                                },
                                Plane {
                                    data: &cr,
                                    stride: cw,
                                },
                            ],
                        };
                        for (ow, oh) in [(17, 11), (31, 23), (8, 7)] {
                            let mut out = vec![0; ow * oh * 4];
                            let rect = composer.compose(&source, ow, oh, &mut out).unwrap();
                            for yy in 0..rect.height {
                                for xx in 0..rect.width {
                                    let l = sample(
                                        &source.planes[0],
                                        composer.luma_x[xx],
                                        composer.luma_y[yy],
                                        1,
                                        0,
                                    );
                                    let u = sample(
                                        &source.planes[1],
                                        composer.chroma_x[xx],
                                        composer.chroma_y[yy],
                                        1,
                                        0,
                                    );
                                    let v = sample(
                                        &source.planes[2],
                                        composer.chroma_x[xx],
                                        composer.chroma_y[yy],
                                        1,
                                        0,
                                    );
                                    let (r, g, b) = yuv_to_rgb(l, u, v, matrix, full_range);
                                    let i = ((rect.top + yy) * ow + rect.left + xx) * 4;
                                    assert_eq!(&out[i..i + 4], &[b, g, r, 255]);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    #[test]
    fn bounded_byte_matches_round_clamp() {
        let original = |value: f32| value.round().clamp(0.0, 255.0) as u8;
        let check =
            |value: f32| assert_eq!(byte(value), original(value), "bits {:08x}", value.to_bits());
        for value in [
            f32::NAN,
            f32::INFINITY,
            f32::NEG_INFINITY,
            -0.0,
            0.0,
            -0.5,
            255.0,
            255.5,
            f32::MAX,
            f32::MIN,
        ] {
            check(value);
        }
        for whole in -2..=257 {
            let half = whole as f32 + 0.5;
            for bits in [
                half.to_bits().saturating_sub(1),
                half.to_bits(),
                half.to_bits().saturating_add(1),
            ] {
                check(f32::from_bits(bits));
            }
        }
        let mut bits = 0x9e37_79b9_u32;
        for _ in 0..1_000_000 {
            bits = bits.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
            check(f32::from_bits(bits));
        }
    }

    fn image<'a>(width: usize, height: usize, layout: Layout, planes: Vec<Plane<'a>>) -> Image<'a> {
        Image {
            width,
            height,
            layout,
            matrix: Matrix::Bt601,
            full_range: false,
            chroma_location: ChromaLocation::Center,
            planes,
        }
    }

    #[test]
    fn aspect_bars_and_opaque_bgra() {
        let rgba = [255, 0, 0, 3, 0, 255, 0, 7];
        let source = image(
            2,
            1,
            Layout::Rgba,
            vec![Plane {
                data: &rgba,
                stride: 8,
            }],
        );
        let mut output = [66; 4 * 4 * 4];
        let rect = Composer::new().compose(&source, 4, 4, &mut output).unwrap();
        assert_eq!(
            rect,
            Rect {
                left: 0,
                top: 1,
                width: 4,
                height: 2
            }
        );
        assert_eq!(&output[0..4], &[0, 0, 0, 255]);
        assert_eq!(&output[16..20], &[0, 0, 255, 255]);
        assert_eq!(&output[28..32], &[0, 255, 0, 255]);
        assert_eq!(&output[48..52], &[0, 0, 0, 255]);
    }

    #[test]
    fn packed_bilinear_checker_gradient_and_stride() {
        // Padded rows: red, blue in the top row; blue, red in the bottom.
        let bgra = [
            0, 0, 255, 0, 255, 0, 0, 0, 99, 99, 255, 0, 0, 0, 0, 0, 255, 0,
        ];
        let source = image(
            2,
            2,
            Layout::Bgra,
            vec![Plane {
                data: &bgra,
                stride: 10,
            }],
        );
        let mut output = [0; 3 * 3 * 4];
        Composer::new().compose(&source, 3, 3, &mut output).unwrap();
        assert_eq!(&output[..4], &[0, 0, 255, 255]);
        assert_eq!(&output[16..20], &[128, 0, 128, 255]);
        assert_eq!(&output[32..36], &[0, 0, 255, 255]);
    }

    #[test]
    fn known_limited_and_full_range_neutrals() {
        for (y, range, expected) in [
            (16, false, 0),
            (235, false, 255),
            (0, true, 0),
            (255, true, 255),
        ] {
            let yy = [y];
            let chroma = [128];
            let mut source = image(
                1,
                1,
                Layout::Yuv444,
                vec![
                    Plane {
                        data: &yy,
                        stride: 1,
                    },
                    Plane {
                        data: &chroma,
                        stride: 1,
                    },
                    Plane {
                        data: &chroma,
                        stride: 1,
                    },
                ],
            );
            source.full_range = range;
            let mut output = [0; 4];
            Composer::new().compose(&source, 1, 1, &mut output).unwrap();
            assert_eq!(output, [expected, expected, expected, 255]);
        }
    }

    #[test]
    fn color_matrix_vectors() {
        // Conventional limited-range digital red, green and blue code values.
        for (matrix, y, cb, cr, expected) in [
            (Matrix::Bt601, 81, 90, 240, [0, 0, 254, 255]),
            (Matrix::Bt601, 145, 54, 34, [1, 255, 0, 255]),
            (Matrix::Bt709, 63, 102, 240, [1, 0, 255, 255]),
            (Matrix::Bt2020, 74, 98, 240, [1, 0, 255, 255]),
        ] {
            let yy = [y];
            let uu = [cb];
            let vv = [cr];
            let mut source = image(
                1,
                1,
                Layout::Yuv444,
                vec![
                    Plane {
                        data: &yy,
                        stride: 1,
                    },
                    Plane {
                        data: &uu,
                        stride: 1,
                    },
                    Plane {
                        data: &vv,
                        stride: 1,
                    },
                ],
            );
            source.matrix = matrix;
            let mut output = [0; 4];
            Composer::new().compose(&source, 1, 1, &mut output).unwrap();
            for channel in 0..3 {
                assert!(
                    output[channel].abs_diff(expected[channel]) <= 2,
                    "{matrix:?} {output:?} vs {expected:?}"
                );
            }
        }
    }

    #[test]
    fn odd_chroma_ceil_and_siting() {
        let y = [128; 15];
        let u = [128; 6];
        let v = [128; 6];
        let mut source = image(
            5,
            3,
            Layout::Yuv420,
            vec![
                Plane {
                    data: &y,
                    stride: 5,
                },
                Plane {
                    data: &u,
                    stride: 3,
                },
                Plane {
                    data: &v,
                    stride: 3,
                },
            ],
        );
        let mut output = [0; 5 * 3 * 4];
        for siting in [
            ChromaLocation::Center,
            ChromaLocation::Left,
            ChromaLocation::TopLeft,
        ] {
            source.chroma_location = siting;
            assert_eq!(
                Composer::new()
                    .compose(&source, 5, 3, &mut output)
                    .unwrap()
                    .width,
                5
            );
        }
        source.planes[2].data = &v[..5];
        assert!(Composer::new().compose(&source, 5, 3, &mut output).is_err());
    }

    #[test]
    fn chroma_location_changes_reconstructed_color() {
        let y = [128; 16];
        let u = [0, 255, 0, 255];
        let v = [128; 4];
        let mut source = image(
            4,
            4,
            Layout::Yuv420,
            vec![
                Plane {
                    data: &y,
                    stride: 4,
                },
                Plane {
                    data: &u,
                    stride: 2,
                },
                Plane {
                    data: &v,
                    stride: 2,
                },
            ],
        );
        let mut center = [0; 64];
        let mut left = [0; 64];
        let mut top_left = [0; 64];
        Composer::new().compose(&source, 4, 4, &mut center).unwrap();
        source.chroma_location = ChromaLocation::Left;
        Composer::new().compose(&source, 4, 4, &mut left).unwrap();
        source.chroma_location = ChromaLocation::TopLeft;
        Composer::new()
            .compose(&source, 4, 4, &mut top_left)
            .unwrap();
        assert!(center[4] < left[4]); // The second pixel is nearer blue chroma when left-sited.
        assert_eq!(left, top_left); // Rows have identical chroma, so vertical siting is immaterial.
    }

    #[test]
    fn malformed_input_does_not_touch_output() {
        let data = [0; 7];
        let source = image(
            2,
            1,
            Layout::Bgra,
            vec![Plane {
                data: &data,
                stride: 7,
            }],
        );
        let mut output = [19; 8];
        assert!(Composer::new().compose(&source, 2, 1, &mut output).is_err());
        assert_eq!(output, [19; 8]);
        assert!(Composer::new().compose(&source, 0, 1, &mut output).is_err());
        assert!(
            Composer::new()
                .compose(&source, 16385, 1, &mut output)
                .is_err()
        );
    }
}
