//! Safe pixel-format classification matching the pinned native Bink helper.

/// Pixel-format fields returned by the native surface descriptor.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct PixelFormat {
    pub size: u32,
    pub flags: u32,
    pub fourcc: u32,
    pub bits: u32,
    pub red: u32,
    pub green: u32,
    pub blue: u32,
    pub alpha: u32,
}

/// Classifies a pixel format using the result codes from the pinned native helper.
///
/// FOURCC values take precedence over flags and all other fields. For RGB
/// formats, the helper selects the 8-, 24-, and 32-bit cases before its mask
/// fallback; that fallback intentionally applies to unusual bit counts too.
pub fn classify(p: &PixelFormat) -> u32 {
    match p.fourcc {
        0x5956_5955 => return 14, // UYVY
        0x3231_5659 => return 15, // YV12
        0x3259_5559 => return 13, // YUY2
        _ => {}
    }

    match p.bits {
        8 => return 0,
        24 => return if p.red == 0x00ff_0000 { 1 } else { 2 },
        32 => {
            return match (p.alpha == 0, p.red == 0x00ff_0000) {
                (true, true) => 3,
                (true, false) => 4,
                (false, true) => 5,
                (false, false) => 6,
            };
        }
        _ => {}
    }

    if p.red == 0xf800 && p.green == 0x07e0 && p.blue == 0x001f {
        return 10;
    }

    if p.alpha == 0x8000 && p.red == 0x7c00 && p.green == 0x03e0 && p.blue == 0x001f {
        return 8;
    }
    if p.red == 0x7c00 && p.green == 0x03e0 && p.blue == 0x001f {
        return 9;
    }

    if p.red == 0xfc00 && p.green == 0x03e0 && p.blue == 0x001f {
        return 11;
    }
    if p.red == 0xfc00 && p.green == 0x03f0 && p.blue == 0x000f {
        return 12;
    }

    if p.alpha == 0xf000 && p.red == 0x0f00 && p.green == 0x00f0 && p.blue == 0x000f {
        return 7;
    }

    u32::MAX
}

/// Classifies a pixel format using the codes from the legacy DDraw helper.
///
/// This is intentionally a separate entry point: legacy and current Bink
/// builds assign different codes to the same formats. HRESULT handling is
/// external to this pixel-format classifier.
pub fn classify_legacy(p: &PixelFormat) -> u32 {
    match p.fourcc {
        0x5956_5955 => return 8, // UYVY
        0x3231_5659 => return 9, // YV12
        0x3259_5559 => return 7, // YUY2
        _ => {}
    }

    match p.bits {
        8 => return 6,
        24 => return 0,
        32 => return 1,
        _ => {}
    }

    if p.red == 0xf800 && p.green == 0x07e0 && p.blue == 0x001f {
        return 3;
    }
    if p.red == 0x7c00 && p.green == 0x03e0 && p.blue == 0x001f {
        return 2;
    }
    if p.red == 0xfc00 && p.green == 0x03e0 && p.blue == 0x001f {
        return 4;
    }
    if p.red == 0xfc00 && p.green == 0x03f0 && p.blue == 0x000f {
        return 5;
    }

    u32::MAX
}

#[cfg(test)]
mod tests {
    use super::{PixelFormat, classify, classify_legacy};

    fn format(bits: u32, red: u32, green: u32, blue: u32, alpha: u32) -> PixelFormat {
        PixelFormat {
            size: 32,
            bits,
            red,
            green,
            blue,
            alpha,
            ..PixelFormat::default()
        }
    }

    #[test]
    fn c_layout_is_eight_u32_fields() {
        assert_eq!(core::mem::size_of::<PixelFormat>(), 32);
        assert_eq!(core::mem::align_of::<PixelFormat>(), 4);
    }

    #[test]
    fn fourcc_classification_precedes_flags_and_bit_fields() {
        let cases = [(0x5956_5955, 14), (0x3231_5659, 15), (0x3259_5559, 13)];
        for (fourcc, expected) in cases {
            let p = PixelFormat {
                flags: u32::MAX,
                fourcc,
                bits: 32,
                red: 1,
                green: 2,
                blue: 3,
                alpha: 4,
                ..PixelFormat::default()
            };
            assert_eq!(classify(&p), expected);
        }
    }

    #[test]
    fn indexed_and_truecolor_bit_counts_use_native_priority() {
        assert_eq!(classify(&format(8, 0, 0, 0, 0)), 0);
        assert_eq!(classify(&format(24, 0x00ff_0000, 0, 0, 0)), 1);
        assert_eq!(classify(&format(24, 0x00ff_0001, 0, 0, 0)), 2);
        assert_eq!(classify(&format(32, 0x00ff_0000, 0, 0, 0)), 3);
        assert_eq!(classify(&format(32, 1, 0, 0, 0)), 4);
        assert_eq!(classify(&format(32, 0x00ff_0000, 0, 0, 1)), 5);
        assert_eq!(classify(&format(32, 1, 0, 0, 1)), 6);
    }

    #[test]
    fn masks_match_native_codes_and_rgb565_ignores_alpha() {
        assert_eq!(classify(&format(16, 0xf800, 0x07e0, 0x001f, 0xffff)), 10);
        assert_eq!(classify(&format(16, 0x7c00, 0x03e0, 0x001f, 0x8000)), 8);
        assert_eq!(classify(&format(16, 0x7c00, 0x03e0, 0x001f, 0)), 9);
        assert_eq!(classify(&format(16, 0xfc00, 0x03e0, 0x001f, 0)), 11);
        assert_eq!(classify(&format(16, 0xfc00, 0x03f0, 0x000f, 0)), 12);
        assert_eq!(classify(&format(16, 0x0f00, 0x00f0, 0x000f, 0xf000)), 7);
    }

    #[test]
    fn unusual_bit_counts_use_mask_fallback_but_bad_masks_are_rejected() {
        assert_eq!(classify(&format(12, 0xf800, 0x07e0, 0x001f, 0)), 10);
        assert_eq!(classify(&format(16, 0xf800, 0x07e1, 0x001f, 0)), u32::MAX);
        assert_eq!(
            classify(&format(16, 0x7c00, 0x03e0, 0x001e, 0x8000)),
            u32::MAX
        );
        assert_eq!(classify(&format(16, 0xfc00, 0x03e1, 0x001f, 0)), u32::MAX);
        assert_eq!(classify(&format(16, 0x0f00, 0x00f0, 0x000f, 0)), u32::MAX);
    }

    #[test]
    fn legacy_classifier_uses_its_own_fourcc_and_bit_depth_codes() {
        let cases = [(0x5956_5955, 8), (0x3231_5659, 9), (0x3259_5559, 7)];
        for (fourcc, expected) in cases {
            let p = PixelFormat {
                flags: u32::MAX,
                fourcc,
                bits: 32,
                ..PixelFormat::default()
            };
            assert_eq!(classify_legacy(&p), expected);
        }
        assert_eq!(classify_legacy(&format(8, 0, 0, 0, 0)), 6);
        assert_eq!(classify_legacy(&format(24, 1, 2, 3, 4)), 0);
        assert_eq!(classify_legacy(&format(32, 1, 2, 3, 4)), 1);
    }

    #[test]
    fn legacy_mask_codes_ignore_alpha_and_do_not_support_4444() {
        assert_eq!(
            classify_legacy(&format(16, 0xf800, 0x07e0, 0x001f, 0xffff)),
            3
        );
        assert_eq!(classify_legacy(&format(16, 0x7c00, 0x03e0, 0x001f, 0)), 2);
        assert_eq!(
            classify_legacy(&format(16, 0x7c00, 0x03e0, 0x001f, 0x8000)),
            2
        );
        assert_eq!(classify_legacy(&format(16, 0xfc00, 0x03e0, 0x001f, 0)), 4);
        assert_eq!(classify_legacy(&format(16, 0xfc00, 0x03f0, 0x000f, 0)), 5);
        assert_eq!(
            classify_legacy(&format(16, 0x0f00, 0x00f0, 0x000f, 0xf000)),
            u32::MAX
        );
    }

    #[test]
    fn legacy_and_current_results_are_not_interchangeable() {
        let fourcc = PixelFormat {
            fourcc: 0x3259_5559,
            ..PixelFormat::default()
        };
        assert_eq!(classify_legacy(&fourcc), 7);
        assert_eq!(classify(&fourcc), 13);

        let indexed = format(8, 0, 0, 0, 0);
        assert_eq!(classify_legacy(&indexed), 6);
        assert_eq!(classify(&indexed), 0);

        let truecolor = format(32, 0, 0, 0, 0);
        assert_eq!(classify_legacy(&truecolor), 1);
        assert_eq!(classify(&truecolor), 4);
    }
}
