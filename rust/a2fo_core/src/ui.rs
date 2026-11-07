//! Portable shell UI policy. The host owns files and STL storage.
use alloc::{collections::BTreeMap, vec::Vec};
use core::{ptr, slice};
#[repr(C)]
pub struct Monitor {
    name: *const u8,
    len: usize,
    adapter: i32,
    primary: u8,
}
#[repr(C)]
pub struct MonitorChoice {
    index: i32,
    configured: u8,
    present: u8,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Bounds {
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}
#[repr(C)]
pub struct EffectiveBounds {
    bounds: Bounds,
    valid: u8,
    repaired: u8,
}
#[repr(C)]
pub struct Style {
    role: u32,
    hot: u8,
    pressed: u8,
    disabled: u8,
}
#[repr(C)]
pub struct Artwork {
    width: i32,
    height: i32,
    pixels: *const u32,
    count: usize,
}
#[repr(C)]
pub struct SavedArtwork {
    key: *const u8,
    key_len: usize,
    image: Artwork,
}
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct SceneView {
    id_offset: usize,
    id_len: usize,
    label_offset: usize,
    label_len: usize,
}
#[repr(C)]
pub struct Theme {
    form: u32,
    colors: [u32; 11],
    corners: [f32; 4],
    name_offset: usize,
    name_len: usize,
    race_offset: usize,
    race_len: usize,
}
fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(p, n) }
    }
}
fn lower(b: u8) -> u8 {
    if b.is_ascii_uppercase() { b + 32 } else { b }
}
fn eq_ci(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len() && a.iter().zip(b).all(|(&x, &y)| lower(x) == lower(y))
}
fn trim(mut x: &[u8]) -> &[u8] {
    while x.first().is_some_and(|c| c.is_ascii_whitespace()) {
        x = &x[1..]
    }
    while x.last().is_some_and(|c| c.is_ascii_whitespace()) {
        x = &x[..x.len() - 1]
    }
    x
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_monitor(
    m: *const Monitor,
    n: usize,
    config: *const u8,
    cn: usize,
    fallback: i32,
) -> MonitorChoice {
    let ms = if n == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(m, n) }
    };
    let c = bytes(config, cn);
    let primary = ms
        .iter()
        .position(|x| x.primary != 0)
        .or(if n > 0 { Some(0) } else { None });
    let found = if cn > 0 {
        ms.iter().position(|x| eq_ci(bytes(x.name, x.len), c))
    } else {
        ms.iter().position(|x| x.adapter == fallback)
    };
    MonitorChoice {
        index: found.or(primary).map_or(-1, |i| i as i32),
        configured: (cn > 0) as u8,
        present: (cn > 0 && found.is_some()) as u8,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_legacy_width(w: i32) -> u8 {
    (w > 0 && w < 64) as u8
}
fn valid_bounds(b: Bounds) -> bool {
    [b.left, b.top, b.right, b.bottom]
        .iter()
        .all(|&x| (-16384..=16384).contains(&x))
        && b.right > b.left
        && b.bottom > b.top
        && b.right - b.left <= 1024
        && b.bottom - b.top <= 512
}
fn origin(l: Bounds, s: Bounds) -> bool {
    s.bottom
        .checked_add(6)
        .is_some_and(|t| l.left == s.left && l.top == t)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_load_bounds(l: Bounds, s: Bounds) -> EffectiveBounds {
    let empty = || EffectiveBounds {
        bounds: Bounds::default(),
        valid: 0,
        repaired: 0,
    };
    let lv = valid_bounds(l);
    let sv = valid_bounds(s);
    if lv && !sv {
        return EffectiveBounds {
            bounds: l,
            valid: 1,
            repaired: 0,
        };
    }
    if !sv {
        return empty();
    }
    let w = s.right - s.left;
    let h = s.bottom - s.top;
    if lv && origin(l, s) && l.right - l.left == w && l.bottom - l.top == h {
        return EffectiveBounds {
            bounds: l,
            valid: 1,
            repaired: 0,
        };
    }
    let mut r = l;
    if !origin(r, s) {
        r.left = s.left;
        let Some(t) = s.bottom.checked_add(6) else {
            return empty();
        };
        r.top = t;
    }
    let (Some(right), Some(bottom)) = (r.left.checked_add(w), r.top.checked_add(h)) else {
        return empty();
    };
    r.right = right;
    r.bottom = bottom;
    if !valid_bounds(r) {
        return empty();
    }
    EffectiveBounds {
        bounds: r,
        valid: 1,
        repaired: 1,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_contains(b: Bounds, x: i32, y: i32) -> u8 {
    (valid_bounds(b) && x >= b.left && x < b.right && y >= b.top && y < b.bottom) as u8
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_caption(p: *const u8, n: usize) -> u8 {
    let expected = b"loadsettings";
    let mut i = 0;
    for &c in bytes(p, n) {
        if matches!(c, b'&' | b' ' | b'\t' | b'_') {
            continue;
        }
        if i >= expected.len() || lower(c) != expected[i] {
            return 0;
        }
        i += 1
    }
    (i == expected.len()) as u8
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
pub unsafe extern "C" fn a2fo_rs_ui_decode_details(
    p: *const u8,
    n: usize,
    out: *mut u8,
    on: usize,
) -> usize {
    if p.is_null() || out.is_null() || on == 0 {
        return 0;
    }
    let t = bytes(p, n);
    let Some(end) = t.iter().position(|&c| matches!(c, b'\r' | b'\n' | 0)) else {
        return 0;
    };
    let mut c = 0;
    while c < end && matches!(t[c], b' ' | b'\t') {
        c += 1
    }
    if !t[c..end].starts_with(b"setupDetails") {
        return 0;
    }
    c += 12;
    while c < end && matches!(t[c], b' ' | b'\t') {
        c += 1
    }
    if c == end || t[c] != b'=' {
        return 0;
    }
    c += 1;
    while c < end && matches!(t[c], b' ' | b'\t') {
        c += 1
    }
    if on > (end - c) / 2 {
        return 0;
    }
    for i in 0..on {
        let (Some(h), Some(l)) = (hex(t[c + i * 2]), hex(t[c + i * 2 + 1])) else {
            return 0;
        };
        unsafe { *out.add(i) = h * 16 + l }
    }
    c += on * 2;
    while c < end && matches!(t[c], b' ' | b'\t') {
        c += 1
    }
    if c != end {
        return 0;
    }
    let mut next = end;
    if next < n && t[next] == b'\r' {
        next += 1
    }
    if next < n && t[next] == b'\n' {
        next += 1
    }
    next + 1
}
fn ascii_lower(v: &[u8]) -> Vec<u8> {
    v.iter().map(|&c| lower(c)).collect()
}
type SceneEntries = Vec<(Vec<u8>, Vec<u8>)>;
fn catalogue(t: &[u8]) -> Result<SceneEntries, u32> {
    if t.len() > 1048576 {
        return Err(1);
    }
    let mut views: Vec<(Vec<u8>, Vec<u8>)> = Vec::new();
    let mut current = None;
    let mut label_seen = false;
    for raw in t.split(|&c| c == b'\n') {
        let line = trim(raw.split(|&c| c == b';').next().unwrap_or_default());
        if line.is_empty() {
            continue;
        }
        if line[0] == b'[' {
            current = None;
            label_seen = false;
            if line.last() != Some(&b']') {
                return Err(2);
            }
            let section = ascii_lower(trim(&line[1..line.len() - 1]));
            if !section.starts_with(b"view.") {
                continue;
            }
            let id = &section[5..];
            if id.is_empty() || id.len() > 128 {
                return Err(3);
            }
            if id.iter().any(|&c| c < 32) {
                return Err(4);
            }
            if views.len() == 128 {
                return Err(5);
            }
            if views.iter().any(|v| v.0 == id) {
                return Err(6);
            }
            current = Some(views.len());
            views.push((id.to_vec(), id.to_vec()));
            continue;
        }
        let Some(i) = current else { continue };
        let Some(eq) = line.iter().position(|&c| c == b'=') else {
            continue;
        };
        if ascii_lower(trim(&line[..eq])) != b"label" || label_seen {
            continue;
        }
        label_seen = true;
        let mut label = trim(&line[eq + 1..]);
        while label.first() == Some(&b'"') {
            label = &label[1..]
        }
        while label.last() == Some(&b'"') {
            label = &label[..label.len() - 1]
        }
        if label.len() > 128 {
            return Err(7);
        }
        if !label.is_empty() {
            views[i].1 = label.to_vec()
        }
    }
    Ok(views)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_catalogue(
    p: *const u8,
    n: usize,
    views: *mut SceneView,
    vc: usize,
    out: *mut u8,
    oc: usize,
    used: *mut usize,
    count: *mut usize,
) -> u32 {
    if p.is_null() && n != 0
        || views.is_null()
        || out.is_null()
        || used.is_null()
        || count.is_null()
    {
        return 8;
    }
    let parsed = match catalogue(bytes(p, n)) {
        Ok(v) => v,
        Err(e) => return e,
    };
    let bytes_needed: usize = parsed
        .iter()
        .map(|(id, label)| id.len() + label.len())
        .sum();
    unsafe {
        *used = bytes_needed;
        *count = parsed.len()
    };
    if vc < parsed.len() || oc < bytes_needed {
        return 8;
    }
    let mut offset = 0;
    for (i, (id, label)) in parsed.iter().enumerate() {
        unsafe {
            ptr::copy_nonoverlapping(id.as_ptr(), out.add(offset), id.len());
        }
        let id_offset = offset;
        offset += id.len();
        unsafe {
            ptr::copy_nonoverlapping(label.as_ptr(), out.add(offset), label.len());
            *views.add(i) = SceneView {
                id_offset,
                id_len: id.len(),
                label_offset: offset,
                label_len: label.len(),
            }
        }
        offset += label.len();
    }
    0
}
fn background_name(dir: &[u8], stem: &[u8]) -> bool {
    match dir {
        b"main/" => [b"mainbkgr".as_slice()].contains(&stem),
        b"single/" => [b"singleplay".as_slice()].contains(&stem),
        b"admiralslog/" => [b"background".as_slice()].contains(&stem),
        b"missionselect/" => [b"missionselectbg".as_slice()].contains(&stem),
        b"backgrounds/" => [
            b"advoptbg".as_slice(),
            b"dlg1buttbg".as_slice(),
            b"dlg2buttbg".as_slice(),
            b"gameoptbg".as_slice(),
            b"gamespycn".as_slice(),
            b"graphicbg".as_slice(),
            b"licensebg".as_slice(),
            b"loadgamebg".as_slice(),
            b"loadsavebg".as_slice(),
            b"mapeditbg".as_slice(),
            b"modoptbg".as_slice(),
            b"optionsbg".as_slice(),
            b"savegamebg".as_slice(),
            b"soundoptionsbackgroundscreen".as_slice(),
            b"voicechat".as_slice(),
        ]
        .contains(&stem),
        b"multiconnect/" => [
            b"internetcon".as_slice(),
            b"multibackground".as_slice(),
            b"multiplayerconnectionscreen".as_slice(),
            b"crcchecking".as_slice(),
            b"crccheckin4".as_slice(),
            b"crccheckin5".as_slice(),
            b"helpscreen".as_slice(),
            b"gamespylogin_new".as_slice(),
        ]
        .contains(&stem),
        b"multisetup/" => {
            [b"instactionbkg".as_slice(), b"multiplaysetupbkg".as_slice()].contains(&stem)
        }
        b"multisetup/advanced/" => [b"advancebkg".as_slice()].contains(&stem),
        b"multisetup/mapselect/" => [b"mapselectbkg".as_slice()].contains(&stem),
        _ => false,
    }
}
fn widget_name(dir: &[u8], stem: &[u8]) -> bool {
    match dir {
        b"sharedbuttons/" => [
            b"button1".as_slice(),
            b"dlg1button".as_slice(),
            b"dlg2button".as_slice(),
            b"editfield".as_slice(),
            b"gamespy".as_slice(),
            b"largeleft".as_slice(),
            b"largecenter".as_slice(),
            b"largeright".as_slice(),
            b"xlarge".as_slice(),
            b"leftbutton".as_slice(),
            b"middlebutton".as_slice(),
            b"middlelong".as_slice(),
            b"middlemedium".as_slice(),
            b"middlemediumbut".as_slice(),
            b"middlemediumsmall".as_slice(),
            b"rightbutton".as_slice(),
            b"smallleft".as_slice(),
            b"smallright".as_slice(),
            b"scrolltop".as_slice(),
            b"scrolltab".as_slice(),
            b"scrollbot".as_slice(),
        ]
        .contains(&stem),
        b"sharedbuttons/sliders/" => [
            b"hruler".as_slice(),
            b"hthumb".as_slice(),
            b"slider".as_slice(),
            b"slidetab".as_slice(),
            b"sliderminus".as_slice(),
            b"sliderplus".as_slice(),
            b"sliderhilit".as_slice(),
        ]
        .contains(&stem),
        b"sharedbuttons/scrollbars/" => [
            b"medscrtabbott".as_slice(),
            b"medscrtabtop".as_slice(),
            b"medscrollbar".as_slice(),
            b"medscrollbarlr".as_slice(),
            b"medscrollslider".as_slice(),
            b"medscrollsliderlr".as_slice(),
            b"medscrolltableft".as_slice(),
            b"medscrolltabright".as_slice(),
            b"medscrolbott".as_slice(),
            b"longbotttab".as_slice(),
            b"longscrollbar".as_slice(),
            b"longscrollslider".as_slice(),
            b"longtoptab".as_slice(),
            b"smallbotttab".as_slice(),
            b"smallscrollbar".as_slice(),
            b"smallscrollslider".as_slice(),
            b"smalltoptab".as_slice(),
        ]
        .contains(&stem),
        b"missionselect/" => [
            b"okbutton".as_slice(),
            b"cancelbutton".as_slice(),
            b"missionfield".as_slice(),
        ]
        .contains(&stem),
        b"multiconnect/" => [
            b"chekon".as_slice(),
            b"chekov".as_slice(),
            b"changeroom".as_slice(),
            b"chatall".as_slice(),
            b"launchsmall".as_slice(),
            b"launchhilit".as_slice(),
            b"selectroom".as_slice(),
            b"textbox".as_slice(),
            b"gamename".as_slice(),
            b"chattabbotnorm2".as_slice(),
            b"scrolltab2".as_slice(),
        ]
        .contains(&stem),
        b"multisetup/buttons/" => [
            b"chatallbutt".as_slice(),
            b"chattabtop".as_slice(),
            b"chattabbot".as_slice(),
            b"mapbutts".as_slice(),
            b"optbutt".as_slice(),
            b"scrolltab".as_slice(),
            b"advancebutt".as_slice(),
            b"opttabtop".as_slice(),
            b"opttabbot".as_slice(),
            b"opttabbott".as_slice(),
            b"opttoptab".as_slice(),
            b"randombutt".as_slice(),
            b"optionsdisplaybutt".as_slice(),
        ]
        .contains(&stem),
        b"multisetup/buttons/footerbutts/" => [
            b"previbutt".as_slice(),
            b"launch".as_slice(),
            b"main".as_slice(),
        ]
        .contains(&stem),
        b"multisetup/buttons/opponentbutts/" => [
            b"placementbutt".as_slice(),
            b"armadabutt".as_slice(),
            b"oppobutt".as_slice(),
            b"racebutt".as_slice(),
            b"teambutt".as_slice(),
            b"armadback".as_slice(),
            b"oppoback".as_slice(),
            b"placementback".as_slice(),
            b"raceback".as_slice(),
            b"teamback".as_slice(),
            b"armadglow".as_slice(),
            b"oppoglow".as_slice(),
            b"placementglow".as_slice(),
            b"raceglow".as_slice(),
            b"teamglow".as_slice(),
        ]
        .contains(&stem),
        b"multisetup/advanced/" => [
            b"advtextbutt".as_slice(),
            b"advbuttcan".as_slice(),
            b"advbuttdefau".as_slice(),
            b"advbuttok".as_slice(),
            b"advscrollbar".as_slice(),
            b"advscrollslider".as_slice(),
            b"advscrollslid".as_slice(),
            b"advscrolltab".as_slice(),
            b"advscrolltabhilitmin".as_slice(),
            b"advscrolltabhilitplus".as_slice(),
        ]
        .contains(&stem),
        b"multisetup/mapselect/" => [
            b"slider".as_slice(),
            b"lmapselectbutts".as_slice(),
            b"rmapselebutts".as_slice(),
            b"rmapseleback".as_slice(),
            b"rmapseleglow".as_slice(),
            b"mapselectbottab".as_slice(),
            b"mapselecttoptab".as_slice(),
            b"mapselectscrllbar".as_slice(),
        ]
        .contains(&stem),
        _ => false,
    }
}
fn strip<'a>(s: &'a [u8], suffix: &[u8]) -> Option<&'a [u8]> {
    s.strip_suffix(suffix)
}
fn style(path: &[u8]) -> u32 {
    if path.is_empty() || matches!(path[0], b'/' | b'\\') {
        return 0;
    }
    let mut p = Vec::with_capacity(path.len());
    for &c in path {
        if c == b':' {
            return 0;
        }
        p.push(if c == b'\\' { b'/' } else { lower(c) })
    }
    if p.windows(2).any(|w| w == b"//")
        || p.last() == Some(&b'/')
        || p.split(|&c| c == b'/').any(|s| s == b"." || s == b"..")
    {
        return 0;
    }
    let p = p.strip_prefix(b"bitmaps/").unwrap_or(&p);
    let Some(slash) = p.iter().rposition(|&c| c == b'/') else {
        return 0;
    };
    let dir = &p[..slash + 1];
    let filename = &p[slash + 1..];
    let Some(dot) = filename.iter().rposition(|&c| c == b'.') else {
        return 0;
    };
    if !matches!(&filename[dot..], b".bmp" | b".png") {
        return 0;
    }
    let mut stem = &filename[..dot];
    if background_name(dir, stem) {
        return 1;
    }
    let mut flags = 0u32;
    if let Some(v) = strip(stem, b"_small") {
        stem = v
    }
    if dir == b"multiconnect/" && stem == b"launchhilitsmall" {
        return 2 | 4;
    }
    for (suffix, flag) in [
        (b"_high".as_slice(), 4),
        (b"_disabled", 16),
        (b"_dis", 16),
        (b"_down", 8),
        (b"_hoover", 4),
        (b"hilit", 4),
        (b"norm", 0),
    ] {
        if let Some(v) = strip(stem, suffix) {
            stem = v;
            flags = flag;
            break;
        }
    }
    if flags == 0
        && dir == b"missionselect/"
        && let Some(v) = strip(stem, b"disabled")
    {
        stem = v;
        flags = 16;
    }
    if flags == 0
        && dir != b"multiconnect/"
        && let Some(v) = strip(stem, b"h")
    {
        stem = v;
        flags = 4;
    }
    if let Some(v) = strip(stem, b"_small") {
        stem = v
    }
    if let Some(v) = strip(stem, b"norm") {
        stem = v
    }
    if dir == b"multisetup/advanced/" {
        if stem.starts_with(b"advtextbutt")
            && let Some(v) = strip(stem, b"r")
        {
            stem = v;
        }
        if let Some(v) = strip(stem, b"norm") {
            stem = v
        }
        if let Some(v) = strip(stem, b"hilit") {
            stem = v;
            flags |= 4
        }
    }
    if widget_name(dir, stem) { 2 | flags } else { 0 }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_style(p: *const u8, n: usize) -> u32 {
    style(bytes(p, n))
}
fn mix(x: u32, t: u32, w: u32) -> u32 {
    (x * w + t * (256 - w) + 128) / 256
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_apply(
    p: *mut u32,
    stride: usize,
    w: i32,
    h: i32,
    s: Style,
) -> u8 {
    if p.is_null()
        || s.role == 0
        || w <= 0
        || h <= 0
        || w > 8192
        || h > 8192
        || w as usize * h as usize > 8 * 1024 * 1024
        || w as usize > stride
    {
        return 0;
    }
    let Some(span) = (h as usize - 1)
        .checked_mul(stride)
        .and_then(|x| x.checked_add(w as usize))
    else {
        return 0;
    };
    if span > usize::MAX / 4 {
        return 0;
    }
    for y in 0..h as usize {
        for x in 0..w as usize {
            let cell = unsafe { p.add(y * stride + x) };
            let pixel = unsafe { *cell };
            let rgb = pixel & 0xffffff;
            if pixel >> 24 == 0 || rgb == 0xff00ff {
                continue;
            }
            let (r, g, b) = ((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
            let light = (r * 54 + g * 183 + b * 19) / 256;
            let (nr, ng, nb) = if s.role == 1 {
                (mix(r, 5, 174), mix(g, 12, 174), mix(b, 24, 174))
            } else if light >= 184 {
                (mix(r, 228, 218), mix(g, 234, 218), mix(b, 240, 218))
            } else {
                let (tr, tg, tb) = if s.disabled != 0 {
                    (40, 45, 52)
                } else if s.pressed != 0 {
                    (122, 76, 20)
                } else if s.hot != 0 {
                    (70, 79, 52)
                } else {
                    (24, 32, 43)
                };
                (mix(r, tr, 92), mix(g, tg, 92), mix(b, tb, 92))
            };
            unsafe { *cell = (pixel & 0xff000000) | (nr << 16) | (ng << 8) | nb }
        }
    }
    1
}
fn valid_art(a: &Artwork) -> bool {
    a.width > 0
        && a.height > 0
        && a.width <= 8192
        && a.height <= 8192
        && (a.width as usize * a.height as usize) <= 8_000_000
        && a.count == a.width as usize * a.height as usize
        && !a.pixels.is_null()
}
fn pix(a: &Artwork) -> &[u32] {
    unsafe { slice::from_raw_parts(a.pixels, a.count) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_valid_art(p: *const Artwork) -> u8 {
    if p.is_null() {
        0
    } else {
        valid_art(unsafe { &*p }) as u8
    }
}
fn matching(current: &Artwork, saved: &[SavedArtwork]) -> Option<usize> {
    if !valid_art(current) {
        return None;
    }
    let c = pix(current);
    let mut selected = None;
    let mut best = 0;
    for (i, s) in saved.iter().enumerate() {
        let b = &s.image;
        if !valid_art(b) || b.width != current.width || b.height != current.height {
            continue;
        }
        let bp = pix(b);
        let mut sampled = 0;
        for sy in 0..16usize {
            let y = sy * (current.height as usize - 1) / 15;
            for sx in 0..16usize {
                let x = sx * (current.width as usize - 1) / 15;
                let at = y * current.width as usize + x;
                if c[at] & 0xffffff == bp[at] & 0xffffff {
                    sampled += 1
                }
            }
        }
        if sampled * 10 < 256 * 9 {
            continue;
        }
        let equal = c
            .iter()
            .zip(bp)
            .filter(|(x, y)| *x & 0xffffff == *y & 0xffffff)
            .count();
        if equal * 10 < c.len() * 9 || (selected.is_some() && equal <= best) {
            continue;
        }
        selected = Some(i);
        best = equal;
    }
    selected
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_match(
    c: *const Artwork,
    s: *const SavedArtwork,
    n: usize,
) -> i32 {
    if c.is_null() || n > 0 && s.is_null() {
        return -1;
    }
    let saved = if n == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(s, n) }
    };
    matching(unsafe { &*c }, saved).map_or(-1, |i| i as i32)
}
fn setup_key(k: &[u8]) -> bool {
    let p: Vec<u8> = k
        .iter()
        .map(|&x| if x == b'\\' { b'/' } else { lower(x) })
        .collect();
    let p = p.strip_prefix(b"bitmaps/").unwrap_or(&p);
    matches!(
        p,
        b"multisetup/instactionbkg.bmp"
            | b"multisetup/instactionbkg.png"
            | b"multisetup/multiplaysetupbkg.bmp"
            | b"multisetup/multiplaysetupbkg.png"
    )
}
fn panel_color(p: u32) -> u32 {
    let b = 0x00060c16u32;
    let blend = |sh: u32| ((((p >> sh) & 255) * 20 + ((b >> sh) & 255) * 80) / 100) << sh;
    blend(16) | blend(8) | blend(0)
}
fn caption(p: u32) -> bool {
    let r = (p >> 16) & 255;
    let g = (p >> 8) & 255;
    let b = p & 255;
    let lo = r.min(g).min(b);
    let hi = r.max(g).max(b);
    lo > 48 && hi - lo < 32
}
fn compose(current: &Artwork, base: &Artwork, live: &Artwork, key: &[u8], result: &mut [u32]) {
    let cp = pix(current);
    let bp = pix(base);
    let lp = pix(live);
    let w = current.width as usize;
    let h = current.height as usize;
    for y in 0..h {
        let ly = y * live.height as usize / h;
        for x in 0..w {
            let i = y * w + x;
            let v = if cp[i] & 0xffffff != bp[i] & 0xffffff {
                cp[i] & 0xffffff
            } else {
                let source = lp[ly * live.width as usize + x * live.width as usize / w] & 0xffffff;
                (source & 0x00fefefe) >> 1
            };
            result[i] = v
        }
    }
    if !setup_key(key) {
        return;
    }
    let panels = [
        (14, 14, 459, 286),
        (473, 14, 314, 286),
        (367, 300, 214, 258),
        (581, 300, 206, 258),
        (14, 560, 773, 32),
    ];
    let bx = (2 * w / 800).max(1);
    let by = (2 * h / 600).max(1);
    for (px, py, pw, ph) in panels {
        let left = px * w / 800;
        let right = (px + pw) * w / 800;
        let top = py * h / 600;
        let bottom = (py + ph) * h / 600;
        for y in top..bottom {
            for x in left..right {
                let i = y * w + x;
                if cp[i] & 0xffffff != bp[i] & 0xffffff {
                    continue;
                }
                let border = x - left < bx || right - x <= bx || y - top < by || bottom - y <= by;
                result[i] = if border {
                    0x008e5a1a
                } else {
                    panel_color(result[i])
                }
            }
        }
    }
    for y in 12 * h / 600..84 * h / 600 {
        for x in 24 * w / 800..776 * w / 800 {
            let i = y * w + x;
            if caption(cp[i] & 0xffffff) {
                result[i] = cp[i] & 0xffffff
            }
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_compose(
    c: *const Artwork,
    b: *const Artwork,
    l: *const Artwork,
    k: *const u8,
    kn: usize,
    out: *mut u32,
    on: usize,
) -> u8 {
    if c.is_null() || b.is_null() || l.is_null() || out.is_null() {
        return 0;
    }
    let (c, b, l) = (unsafe { &*c }, unsafe { &*b }, unsafe { &*l });
    if !valid_art(c)
        || !valid_art(b)
        || !valid_art(l)
        || c.width != b.width
        || c.height != b.height
        || on < c.count
    {
        return 0;
    }
    let output = unsafe { slice::from_raw_parts_mut(out, on) };
    compose(c, b, l, bytes(k, kn), output);
    1
}
fn theme_trim(mut x: &[u8]) -> &[u8] {
    while x
        .first()
        .is_some_and(|c| matches!(c, b' ' | b'\t' | b'\r' | b'\n'))
    {
        x = &x[1..];
    }
    while x
        .last()
        .is_some_and(|c| matches!(c, b' ' | b'\t' | b'\r' | b'\n'))
    {
        x = &x[..x.len() - 1];
    }
    x
}
fn mapped_lower(v: &[u8], map: &[u8]) -> Vec<u8> {
    v.iter().map(|&c| map[c as usize]).collect()
}
type Section = BTreeMap<Vec<u8>, Vec<u8>>;
fn section_read(raw: &[u8], sections: &mut BTreeMap<Vec<u8>, Section>, map: &[u8]) {
    let mut current = Vec::new();
    let mut total = 0usize;
    for line in raw.split(|&c| c == b'\n') {
        if total >= 65536 {
            break;
        }
        total += line.len();
        let line = line.strip_suffix(b"\r").unwrap_or(line);
        let line = theme_trim(line);
        if line.is_empty() || matches!(line[0], b';' | b'#') {
            continue;
        }
        if line.first() == Some(&b'[') && line.last() == Some(&b']') {
            let id = mapped_lower(theme_trim(&line[1..line.len() - 1]), map);
            current = id.strip_prefix(b"theme.").unwrap_or_default().to_vec();
        } else if !current.is_empty()
            && let Some(eq) = line.iter().position(|&c| c == b'=')
        {
            let key = mapped_lower(theme_trim(&line[..eq]), map);
            if !key.is_empty() {
                sections
                    .entry(current.clone())
                    .or_default()
                    .insert(key, theme_trim(&line[eq + 1..]).to_vec());
            }
        }
    }
}
fn form(v: &[u8], map: &[u8]) -> Option<u32> {
    [
        b"shell".as_slice(),
        b"elbow",
        b"blade",
        b"wing",
        b"arch",
        b"cube",
        b"organic",
    ]
    .iter()
    .position(|&x| mapped_lower(v, map) == x)
    .map(|i| i as u32)
}
fn color(v: &[u8]) -> Option<u32> {
    if v.len() != 8 {
        return None;
    }
    v.iter()
        .try_fold(0u32, |n, &c| hex(c).map(|h| n << 4 | h as u32))
}
fn alias(s: &Section, race: &[u8], map: &[u8]) -> bool {
    s.get(b"aliases".as_slice()).is_some_and(|v| {
        v.split(|&c| c == b',')
            .any(|x| mapped_lower(theme_trim(x), map) == race)
    })
}
fn theme_parse(
    s: &Section,
    race: &[u8],
    base: &Theme,
    parse_corners: unsafe extern "C" fn(*const u8, usize, *mut f32) -> u8,
    map: &[u8],
) -> Option<(Theme, Vec<u8>, Vec<u8>)> {
    let name = s.get(b"name".as_slice())?;
    if theme_trim(name).is_empty() {
        return None;
    }
    let shape = s.get(b"form".as_slice())?;
    let mut out = Theme {
        form: form(shape, map)?,
        colors: base.colors,
        corners: base.corners,
        name_offset: 0,
        name_len: 0,
        race_offset: 0,
        race_len: 0,
    };
    for (i, k) in [
        b"panel_top".as_slice(),
        b"panel",
        b"panel_edge",
        b"panel_hi",
        b"text",
        b"text_dim",
        b"title",
        b"accent",
        b"accent2",
        b"fill",
        b"good",
    ]
    .iter()
    .enumerate()
    {
        out.colors[i] = color(s.get(*k)?)?
    }
    let corners = s.get(b"corners".as_slice())?;
    if unsafe { parse_corners(corners.as_ptr(), corners.len(), out.corners.as_mut_ptr()) } == 0 {
        return None;
    }
    Some((
        out,
        name.clone(),
        if race == b"shell" {
            Vec::new()
        } else {
            race.to_vec()
        },
    ))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_theme(
    texts: *const *const u8,
    lens: *const usize,
    n: usize,
    race: *const u8,
    rn: usize,
    lower_map: *const u8,
    parse_corners: unsafe extern "C" fn(*const u8, usize, *mut f32) -> u8,
    out: *mut Theme,
    buffer: *mut u8,
    capacity: usize,
    used: *mut usize,
) -> u32 {
    if (texts.is_null() || lens.is_null()) && n > 0
        || out.is_null()
        || buffer.is_null()
        || used.is_null()
        || lower_map.is_null()
    {
        return 1;
    }
    let map = unsafe { slice::from_raw_parts(lower_map, 256) };
    let mut sections = BTreeMap::new();
    for i in 0..n {
        let p = unsafe { *texts.add(i) };
        let len = unsafe { *lens.add(i) };
        if p.is_null() && len > 0 {
            return 1;
        }
        section_read(bytes(p, len), &mut sections, map)
    }
    let default = Theme {
        form: 0,
        colors: [
            0xe0172230, 0xe00a1018, 0xff4c5a6a, 0xffa6b6c8, 0xffe4eaf0, 0xff8e9aa8, 0xfff2b23a,
            0xfff2a93b, 0xff6f93ba, 0xfff2a93b, 0xff5fd36a,
        ],
        corners: [-12.0; 4],
        name_offset: 0,
        name_len: 0,
        race_offset: 0,
        race_len: 0,
    };
    let mut selected = (default, b"Shell".to_vec(), Vec::new());
    if let Some(s) = sections.get(b"shell".as_slice())
        && let Some(x) = theme_parse(s, b"shell", &selected.0, parse_corners, map)
    {
        selected = x;
    }
    let requested = mapped_lower(theme_trim(bytes(race, rn)), map);
    if !requested.is_empty() {
        let id = if sections.contains_key(&requested) {
            Some(requested.clone())
        } else {
            sections
                .iter()
                .find(|(k, s)| k.as_slice() != b"shell" && alias(s, &requested, map))
                .map(|(k, _)| k.clone())
        };
        if let Some(id) = id
            && id != b"shell"
            && let Some(s) = sections.get(&id)
            && let Some(x) = theme_parse(s, &id, &selected.0, parse_corners, map)
        {
            selected = x;
        }
    }
    let needed = selected.1.len() + selected.2.len();
    unsafe { *used = needed };
    if needed > capacity {
        return 2;
    }
    let mut t = selected.0;
    t.name_len = selected.1.len();
    t.race_offset = t.name_len;
    t.race_len = selected.2.len();
    unsafe {
        ptr::copy_nonoverlapping(selected.1.as_ptr(), buffer, t.name_len);
        ptr::copy_nonoverlapping(selected.2.as_ptr(), buffer.add(t.race_offset), t.race_len);
        *out = t
    };
    0
}
