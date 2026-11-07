use a2fo_native11_assets::{
    Texture, TextureMip, a2fo_native11_parse_dds, a2fo_native11_parse_texture,
};
use std::ffi::c_void;

fn word(bytes: &mut [u8], at: usize, value: u32) {
    bytes[at..at + 4].copy_from_slice(&value.to_le_bytes());
}
fn cc(value: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*value)
}

fn fixture(code: u32, dxgi: u32, width: u32, height: u32, count: u32, raw: bool) -> Vec<u8> {
    let dx10 = code == cc(b"DX10");
    let mut b = vec![0; if dx10 { 148 } else { 128 }];
    word(&mut b, 0, cc(b"DDS "));
    word(&mut b, 4, 124);
    word(&mut b, 8, 0x1007 | if count > 1 { 0x20000 } else { 0 });
    word(&mut b, 12, height);
    word(&mut b, 16, width);
    word(&mut b, 28, count);
    word(&mut b, 76, 32);
    word(&mut b, 80, if raw { 0x40 } else { 4 });
    word(&mut b, 84, code);
    if dx10 {
        word(&mut b, 128, dxgi);
        word(&mut b, 132, 3);
        word(&mut b, 140, 1);
    }
    if raw {
        word(&mut b, 88, 32);
        word(&mut b, 92, 0x00ff_0000);
        word(&mut b, 96, 0x0000_ff00);
        word(&mut b, 100, 0x0000_00ff);
        word(&mut b, 104, 0xff00_0000);
    }
    let mut w = width;
    let mut h = height;
    for i in 0..count.max(1) {
        let size = if raw || matches!(dxgi, 28 | 29 | 87 | 91) {
            w as usize * h as usize * 4
        } else {
            w.div_ceil(4) as usize
                * h.div_ceil(4) as usize
                * if matches!(dxgi, 71 | 72 | 80 | 81) || code == cc(b"DXT1") {
                    8
                } else {
                    16
                }
        };
        b.extend((0..size).map(|n| (i as usize * 37 + n) as u8));
        w = (w / 2).max(1);
        h = (h / 2).max(1);
    }
    b
}

#[derive(Default)]
struct Seen {
    calls: usize,
    accept: bool,
    format: u32,
    alpha: u32,
    payload_ptr: *const u8,
    payload: Vec<u8>,
    mips: Vec<TextureMip>,
}
unsafe extern "C" fn sink(context: *mut c_void, texture: *const Texture) -> i32 {
    let seen = unsafe { &mut *(context as *mut Seen) };
    let texture = unsafe { &*texture };
    seen.calls += 1;
    seen.format = texture.format;
    seen.alpha = texture.alpha_mode;
    seen.payload_ptr = texture.payload;
    assert_eq!(texture.reserved, 0);
    seen.payload =
        unsafe { std::slice::from_raw_parts(texture.payload, texture.payload_bytes as usize) }
            .to_vec();
    seen.mips =
        unsafe { std::slice::from_raw_parts(texture.mips, texture.mip_count as usize) }.to_vec();
    seen.accept as i32
}
fn call(bytes: &[u8], seen: &mut Seen) -> i32 {
    unsafe {
        a2fo_native11_parse_dds(
            bytes.as_ptr(),
            bytes.len() as u32,
            Some(sink),
            seen as *mut _ as *mut c_void,
        )
    }
}
fn call_generic(bytes: &[u8], seen: &mut Seen) -> i32 {
    unsafe {
        a2fo_native11_parse_texture(
            bytes.as_ptr(),
            bytes.len() as u32,
            Some(sink),
            seen as *mut _ as *mut c_void,
        )
    }
}

#[test]
fn preserved_npot_chain_and_metadata() {
    let mut b = fixture(cc(b"DX10"), 99, 7, 3, 3, false);
    word(&mut b, 144, 2);
    b.push(0xee);
    let mut seen = Seen {
        accept: true,
        ..Default::default()
    };
    assert_eq!(call(&b, &mut seen), 1);
    assert_eq!((seen.calls, seen.format, seen.alpha), (1, 16, 2));
    assert_eq!(
        seen.mips
            .iter()
            .map(|m| (
                m.width,
                m.height,
                m.row_pitch,
                m.slice_pitch,
                m.offset,
                m.byte_count
            ))
            .collect::<Vec<_>>(),
        vec![
            (7, 3, 32, 32, 0, 32),
            (3, 1, 16, 16, 32, 16),
            (1, 1, 16, 16, 48, 16)
        ]
    );
    assert_eq!(seen.payload, b[148..b.len() - 1]);
    assert_eq!(seen.payload_ptr, unsafe { b.as_ptr().add(148) });
    assert_eq!(seen.payload[32], 37);
}

#[test]
fn c_layout() {
    assert_eq!(std::mem::size_of::<TextureMip>(), 32);
    assert_eq!(std::mem::offset_of!(TextureMip, offset), 16);
    assert_eq!(std::mem::offset_of!(TextureMip, byte_count), 24);
    assert_eq!(std::mem::offset_of!(Texture, payload_bytes), 24);
    #[cfg(target_pointer_width = "64")]
    {
        assert_eq!(std::mem::size_of::<Texture>(), 48);
        assert_eq!(std::mem::offset_of!(Texture, payload), 32);
        assert_eq!(std::mem::offset_of!(Texture, mips), 40);
    }
    #[cfg(target_pointer_width = "32")]
    {
        assert_eq!(std::mem::size_of::<Texture>(), 40);
        assert_eq!(std::mem::offset_of!(Texture, payload), 32);
        assert_eq!(std::mem::offset_of!(Texture, mips), 36);
    }
}

#[test]
fn formats_and_raw_masks() {
    for (dxgi, format) in [
        (28, 2),
        (29, 4),
        (87, 1),
        (91, 3),
        (71, 5),
        (72, 6),
        (74, 7),
        (75, 8),
        (77, 9),
        (78, 10),
        (80, 11),
        (81, 12),
        (83, 13),
        (84, 14),
        (98, 15),
        (99, 16),
    ] {
        let b = fixture(cc(b"DX10"), dxgi, 4, 4, 1, false);
        let mut seen = Seen {
            accept: true,
            ..Default::default()
        };
        assert_eq!(call(&b, &mut seen), 1, "DXGI {dxgi}");
        assert_eq!(seen.format, format);
    }
    for (code, format, alpha) in [
        (b"DXT1", 5, 0),
        (b"DXT2", 7, 2),
        (b"DXT3", 7, 0),
        (b"DXT4", 9, 2),
        (b"DXT5", 9, 0),
        (b"ATI1", 11, 0),
        (b"BC4U", 11, 0),
        (b"BC4S", 12, 0),
        (b"ATI2", 13, 0),
        (b"BC5U", 13, 0),
        (b"BC5S", 14, 0),
    ] {
        let b = fixture(cc(code), 77, 4, 4, 1, false);
        let mut seen = Seen {
            accept: true,
            ..Default::default()
        };
        assert_eq!(call(&b, &mut seen), 1);
        assert_eq!((seen.format, seen.alpha), (format, alpha));
    }
    for (code, expected_format) in [(b"DXT1", 5), (b"DXT5", 9)] {
        let mut b = fixture(cc(code), 77, 5, 3, 2, false);
        word(&mut b, 80, 0x5); // FOURCC plus ALPHAPIXELS is accepted by the core parser.
        let mut seen = Seen {
            accept: true,
            ..Default::default()
        };
        assert_eq!(call(&b, &mut seen), 1);
        assert_eq!(
            (seen.format, seen.alpha, seen.mips.len()),
            (expected_format, 0, 2)
        );
        assert_eq!(seen.payload, b[128..]);
    }
    let mut b = fixture(0, 0, 3, 2, 2, true);
    let mut seen = Seen {
        accept: true,
        ..Default::default()
    };
    assert_eq!(call(&b, &mut seen), 1);
    assert_eq!((seen.format, seen.alpha), (1, 0));
    assert_eq!(
        seen.mips
            .iter()
            .map(|m| (m.row_pitch, m.slice_pitch, m.offset))
            .collect::<Vec<_>>(),
        vec![(12, 24, 0), (4, 4, 24)]
    );
    word(&mut b, 92, 0xff);
    word(&mut b, 100, 0xff0000);
    word(&mut b, 104, 0);
    assert_eq!(call(&b, &mut seen), 1);
    assert_eq!((seen.format, seen.alpha), (2, 3));
    word(&mut b, 8, 0x100f); // DDSD_PITCH with canonical top-level row width.
    word(&mut b, 20, 12);
    assert_eq!(call(&b, &mut seen), 1);
    assert_eq!(seen.mips.len(), 2);
    word(&mut b, 8, 0x1007); // Without DDSD_PITCH the stale hint is ignored.
    word(&mut b, 20, 99);
    assert_eq!(call(&b, &mut seen), 1);
}

#[test]
fn reject_invalid_before_callback() {
    let base = fixture(cc(b"DX10"), 98, 5, 9, 4, false);
    let mut bad = Vec::new();
    bad.push(base[..base.len() - 1].to_vec());
    for (at, value) in [
        (4, 123),
        (8, 0),
        (12, 0),
        (16, 16385),
        (24, 2),
        (28, 5),
        (80, 0x44),
        (112, 0x200),
        (128, 95),
        (132, 4),
        (136, 4),
        (140, 2),
        (144, 5),
    ] {
        let mut b = base.clone();
        word(&mut b, at, value);
        bad.push(b);
    }
    let mut b = base.clone();
    word(&mut b, 28, 0xffff_ffff);
    bad.push(b);
    let mut b = fixture(0, 0, 4, 4, 1, true);
    word(&mut b, 88, 24);
    bad.push(b);
    let mut b = fixture(0, 0, 4, 4, 1, true);
    word(&mut b, 92, 0xf800);
    bad.push(b);
    let mut b = fixture(0, 0, 3, 2, 2, true);
    word(&mut b, 8, 0x100f); // DDSD_PITCH and padded row claim.
    word(&mut b, 20, 16);
    bad.push(b);
    for b in bad {
        let mut seen = Seen {
            accept: true,
            ..Default::default()
        };
        assert_eq!(call(&b, &mut seen), 0);
        assert_eq!(seen.calls, 0);
    }
}

#[test]
fn pointers_and_rejection() {
    let b = fixture(cc(b"DXT1"), 71, 4, 4, 1, false);
    let mut seen = Seen::default();
    assert_eq!(call(&b, &mut seen), 0);
    assert_eq!(seen.calls, 1);
    assert_eq!(
        unsafe {
            a2fo_native11_parse_dds(
                std::ptr::null(),
                8,
                Some(sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(
        unsafe { a2fo_native11_parse_dds(b.as_ptr(), b.len() as u32, None, std::ptr::null_mut()) },
        0
    );
    assert_eq!(seen.calls, 1);
}

#[test]
fn generic_direct_dds_preserves_authored_mips() {
    for b in [
        fixture(cc(b"DX10"), 99, 7, 3, 3, false),
        fixture(0, 0, 3, 2, 2, true),
    ] {
        let mut seen = Seen {
            accept: true,
            ..Default::default()
        };
        assert_eq!(call_generic(&b, &mut seen), 1);
        assert_eq!(seen.calls, 1);
        assert_eq!(seen.mips.len(), if b[84] == b'D' { 3 } else { 2 });
        let header = if b[84] == b'D' { 148 } else { 128 };
        assert_eq!(seen.payload, b[header..]);
        assert_eq!(seen.payload_ptr, unsafe { b.as_ptr().add(header) });
    }
}

#[test]
fn generic_legacy_image_bridge() {
    let mut tga = vec![0u8; 18];
    tga[2] = 2;
    tga[12] = 2;
    tga[14] = 1;
    tga[16] = 24;
    tga.extend_from_slice(&[0, 0, 255, 0, 255, 0]); // Red, green BGR pixels.
    let mut raw16 = fixture(0, 0, 2, 1, 1, true);
    raw16.truncate(128);
    word(&mut raw16, 88, 16);
    word(&mut raw16, 92, 0xf800);
    word(&mut raw16, 96, 0x07e0);
    word(&mut raw16, 100, 0x001f);
    word(&mut raw16, 104, 0);
    raw16.extend_from_slice(&[0x00, 0xf8, 0xe0, 0x07]); // RGB565 red, green.
    let mut raw24 = fixture(0, 0, 2, 1, 1, true);
    raw24.truncate(128);
    word(&mut raw24, 88, 24);
    word(&mut raw24, 104, 0);
    raw24.extend_from_slice(&[0, 0, 255, 0, 255, 0]);
    let mut padded = fixture(0, 0, 2, 2, 1, true);
    padded.truncate(128);
    word(&mut padded, 8, 0x100f);
    word(&mut padded, 20, 12);
    padded.extend_from_slice(&[0, 0, 255, 255, 0, 255, 0, 255, 99, 99, 99, 99]);
    padded.extend_from_slice(&[255, 0, 0, 255, 255, 255, 255, 255, 88, 88, 88, 88]);
    for (b, width, height, expected) in [
        (tga, 2, 1, vec![0, 0, 255, 255, 0, 255, 0, 255]),
        (raw16, 2, 1, vec![0, 0, 255, 255, 0, 255, 0, 255]),
        (raw24, 2, 1, vec![0, 0, 255, 255, 0, 255, 0, 255]),
        (
            padded,
            2,
            2,
            vec![
                0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255,
            ],
        ),
    ] {
        let mut seen = Seen {
            accept: true,
            ..Default::default()
        };
        assert_eq!(call_generic(&b, &mut seen), 1);
        assert_eq!((seen.calls, seen.format, seen.alpha), (1, 1, 0));
        assert_eq!(seen.mips.len(), 1);
        assert_eq!(
            (
                seen.mips[0].width,
                seen.mips[0].height,
                seen.mips[0].row_pitch,
                seen.mips[0].slice_pitch,
                seen.mips[0].offset
            ),
            (width, height, width * 4, width * height * 4, 0)
        );
        assert_eq!(seen.payload, expected);
    }
}

#[test]
fn generic_invalid_and_callback_rejection_do_not_retry() {
    let mut compressed = fixture(cc(b"DXT1"), 71, 4, 4, 1, false);
    compressed.pop();
    let mut raw16_multi = fixture(0, 0, 2, 2, 2, true);
    word(&mut raw16_multi, 88, 16);
    let mut padded_multi = fixture(0, 0, 2, 2, 2, true);
    word(&mut padded_multi, 8, 0x100f);
    word(&mut padded_multi, 20, 12);
    let mut disguised_fourcc = fixture(0, 0, 2, 1, 1, true);
    word(&mut disguised_fourcc, 84, cc(b"DXT1"));
    let mut cube = fixture(cc(b"DX10"), 99, 4, 4, 1, false);
    word(&mut cube, 112, 0x200);
    let mut array = fixture(cc(b"DX10"), 99, 4, 4, 1, false);
    word(&mut array, 140, 2);
    for b in [
        compressed,
        raw16_multi,
        padded_multi,
        disguised_fourcc,
        cube,
        array,
    ] {
        let mut seen = Seen {
            accept: true,
            ..Default::default()
        };
        assert_eq!(call_generic(&b, &mut seen), 0);
        assert_eq!(seen.calls, 0);
    }
    let mut tga = vec![0u8; 18];
    tga[2] = 2;
    tga[12] = 1;
    tga[14] = 1;
    tga[16] = 24;
    tga.extend_from_slice(&[0, 0, 255]);
    for b in [fixture(0, 0, 2, 2, 2, true), tga] {
        let mut seen = Seen::default();
        assert_eq!(call_generic(&b, &mut seen), 0);
        assert_eq!(seen.calls, 1);
    }
    let mut seen = Seen::default();
    assert_eq!(
        unsafe {
            a2fo_native11_parse_texture(
                std::ptr::null(),
                1,
                Some(sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(
        unsafe { a2fo_native11_parse_texture(b"x".as_ptr(), 1, None, std::ptr::null_mut()) },
        0
    );
    assert_eq!(seen.calls, 0);
}
