//! UI draw admission, cache/buffer/resource workflows and native draw ordering.
// Native callback programs (booleans unless noted; HR operations return i32).
// 0 buffer: 0 required bytes(u32),1 capacity(u32),2 create HR,3 publish/release old,
//           4 map HR,5 memcpy,6 unmap.
// 1 constants:0 missing,1 create HR,2 map HR,3 memcpy,4 unmap.
// 2 VS:0 cached,1 build source/compile HR,2 log failure,3 log success,
//      4 create VS HR,5 create input-layout HR,6 release temporary VS,7 publish pair.
// 3 cache(one native descriptor):0 found,1 cache size(u32),2 clear,
//      3 create HR,4 emplace (release created on native exception),5 assign found.
// 4 state objects:0 prepare blend descriptor/valid factors,1 blend cache HR,
//      2 prepare depth+cache HR,3 prepare raster+cache HR,4 sampler+cache HR.
// 5 draw:0 ready/target/nonempty viewport,1 decode/admit layout HR,
// 2 primitive-count HR,3 primitive_count nonzero,4 validate vertex/data/state/depth,
// 5 capture texture_used,6 texture GetResource,7 target GetResource,8 has depth,
// 9 depth GetResource,10 compare aliases,11/12/13 release sampled/rendered/depth,
// 14 clear fan/init local index fields,15 indexed,16 index admission,
// 17 triangle fan,18 expand fan+publish locals HR,19 VS HR,20 state_objects HR,
// 21 construct/upload constants HR,22 vertex buffer HR,23 indexed(local),24 IB HR,
// 25 prepare viewport/variant/draw count,26 OM targets,27 viewport,28 input layout,
// 29 vertex buffers,30 index buffer,31 topology,32 VS,33 HS/DS/GS=null,
// 34 VS constants,35 PS,36 PS constants,37 active SRV,38 sampler,39 raster,
// 40 blend,41 depth state,42 DrawIndexed,43 Draw.
// 6 init:0 wrong owner thread,1 shutdown,2 non-null device/context,3 feature>=11,
// 4 immediate context,5 context GetDevice+cache equality,6 release owner,
// 7 cached equality,8 allocate Impl+assign device/context+AddRef both,
// 9 owner/cache_directory/log fields,10 compile PS HR,11 log failure,
// 12 log success,13 create PS HR,14 move implementation into impl_.
// 7 target:0 ready,1 target present,2 empty tuple valid,3 release/reset,
// 4 dimensions valid,5 target owner,6 depth owner,7 target desc valid,
// 8 depth present,9 depth desc valid,10 target resource/QI/release HR,
// 11 target GetDesc/release/dimensions valid,13 depth resource/QI/release HR,
// 14 depth GetDesc/release/dimensions valid,15 AddRef/release/commit/viewport.
// 8 texture:0 ready,1 same device,2 present,3 desc valid,4 retain/release/commit.
// 9 upload:0 output exists,1 zero output,2 ready,3 valid payload,
// 4 create texture HR,5 create SRV HR,6 release temporary image.
// 10 clear:0 ready/target/rect pointer,1 count(u32),2 draw viewport HR,
// 3 validate rect[index],4 clip+draw rect[index] HR. Repeated reads stay native.
use core::ffi::c_void;
const INVALID: i32 = 0x8876086cu32 as i32;
#[repr(C)]
pub struct Reply {
    value: i64,
    failed: u8,
}
type Step = unsafe extern "C" fn(*mut c_void, u32, u32, *mut Reply);
unsafe fn step(ctx: *mut c_void, s: Step, op: u32, index: u32) -> Result<i64, i32> {
    let mut r = Reply {
        value: 0,
        failed: 0,
    };
    // SAFETY: private synchronous callback writes one POD. Native exceptions are
    // held in its stack context and rethrown only once this Rust frame returns.
    unsafe { s(ctx, op, index, &mut r) };
    if r.failed != 0 {
        Err(0x80004005u32 as i32)
    } else {
        Ok(r.value)
    }
}
unsafe fn flow(program: u32, ctx: *mut c_void, s: Step) -> Result<i32, i32> {
    // SAFETY: all operations are synchronous, indices bounded below; no Rust
    // references into native containers or COM resources live across callbacks.
    unsafe {
        let c = |op| step(ctx, s, op, 0);
        let hr = |op| -> Result<i32, i32> {
            let v = c(op)? as i32;
            if v < 0 { Err(v) } else { Ok(v) }
        };
        match program {
            0 => {
                let required = c(0)? as u32;
                if required == 0 || required > 64 * 1024 * 1024 {
                    return Ok(INVALID);
                }
                if (c(1)? as u32) < required {
                    hr(2)?;
                    c(3)?;
                }
                hr(4)?;
                c(5)?;
                c(6)?;
            }
            1 => {
                if c(0)? != 0 {
                    hr(1)?;
                }
                hr(2)?;
                c(3)?;
                c(4)?;
            }
            2 => {
                if c(0)? != 0 {
                    return Ok(0);
                }
                let status = c(1)? as i32;
                if status < 0 {
                    c(2)?;
                    return Ok(status);
                }
                c(3)?;
                hr(4)?;
                let status = c(5)? as i32;
                if status < 0 {
                    c(6)?;
                    return Ok(status);
                }
                c(7)?;
            }
            3 => {
                if c(0)? == 0 {
                    if c(1)? >= 256 {
                        c(2)?;
                    }
                    hr(3)?;
                    c(4)?;
                } else {
                    c(5)?;
                }
            }
            4 => {
                if c(0)? == 0 {
                    return Ok(INVALID);
                }
                for op in 1..=4 {
                    hr(op)?;
                }
            }
            5 => {
                if c(0)? == 0 {
                    return Ok(INVALID);
                }
                hr(1)?;
                hr(2)?;
                if c(3)? == 0 {
                    return Ok(0);
                }
                if c(4)? == 0 {
                    return Ok(INVALID);
                }
                if c(5)? != 0 {
                    c(6)?;
                    c(7)?;
                    if c(8)? != 0 {
                        c(9)?;
                    }
                    let aliases = c(10)? != 0;
                    for op in 11..=13 {
                        c(op)?;
                    }
                    if aliases {
                        return Ok(INVALID);
                    }
                }
                c(14)?;
                if c(15)? != 0 && c(16)? == 0 {
                    return Ok(INVALID);
                }
                if c(17)? != 0 {
                    hr(18)?;
                }
                for op in 19..=22 {
                    hr(op)?;
                }
                if c(23)? != 0 {
                    hr(24)?;
                }
                for op in 25..=29 {
                    c(op)?;
                }
                if c(23)? != 0 {
                    c(30)?;
                }
                for op in 31..=41 {
                    c(op)?;
                }
                if c(23)? != 0 {
                    c(42)?;
                } else {
                    c(43)?;
                }
            }
            6 => {
                if c(0)? != 0 {
                    return Ok(INVALID);
                }
                c(1)?;
                for op in 2..=4 {
                    if c(op)? == 0 {
                        return Ok(INVALID);
                    }
                }
                c(5)?;
                c(6)?;
                if c(7)? == 0 {
                    return Ok(INVALID);
                }
                c(8)?;
                c(9)?;
                let status = c(10)? as i32;
                if status < 0 {
                    c(11)?;
                    return Ok(status);
                }
                c(12)?;
                hr(13)?;
                c(14)?;
            }
            7 => {
                if c(0)? == 0 {
                    return Ok(INVALID);
                }
                if c(1)? == 0 {
                    if c(2)? == 0 {
                        return Ok(INVALID);
                    }
                    c(3)?;
                    return Ok(0);
                }
                for op in 4..=7 {
                    if c(op)? == 0 {
                        return Ok(INVALID);
                    }
                }
                if c(8)? != 0 && c(9)? == 0 {
                    return Ok(INVALID);
                }
                if (c(10)? as i32) < 0 {
                    return Ok(INVALID);
                }
                if c(11)? == 0 {
                    return Ok(INVALID);
                }
                if c(8)? != 0 {
                    if (c(13)? as i32) < 0 {
                        return Ok(INVALID);
                    }
                    if c(14)? == 0 {
                        return Ok(INVALID);
                    }
                }
                c(15)?;
            }
            8 => {
                if c(0)? == 0 || c(1)? == 0 {
                    return Ok(INVALID);
                }
                if c(2)? != 0 && c(3)? == 0 {
                    return Ok(INVALID);
                }
                c(4)?;
            }
            9 => {
                if c(0)? == 0 {
                    return Ok(0x80004003u32 as i32);
                }
                c(1)?;
                if c(2)? == 0 || c(3)? == 0 {
                    return Ok(INVALID);
                }
                hr(4)?;
                let status = c(5)? as i32;
                c(6)?;
                return Ok(status);
            }
            10 => {
                if c(0)? == 0 {
                    return Ok(INVALID);
                }
                let count = c(1)? as u32;
                if count == 0 {
                    return Ok(c(2)? as i32);
                }
                for i in 0..count {
                    if step(ctx, s, 3, i)? == 0 {
                        return Ok(INVALID);
                    }
                }
                for i in 0..count {
                    let status = step(ctx, s, 4, i)? as i32;
                    if status < 0 {
                        return Ok(status);
                    }
                }
            }
            _ => return Ok(INVALID),
        }
        Ok(0)
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_run(program: u32, ctx: *mut c_void, s: Option<Step>) -> i32 {
    let Some(s) = s else {
        return INVALID;
    };
    unsafe { flow(program, ctx, s) }.unwrap_or_else(|hr| hr)
}
fn argument(v: u32) -> bool {
    v & !0x3f == 0 && v & 15 <= 2
}
fn stage(op: u32, a: u32, b: u32) -> bool {
    matches!(op, 1 | 2 | 3 | 4 | 7)
        && (op == 1
            || (!(matches!(op, 2 | 4 | 7)) || argument(a))
                && (!(matches!(op, 3 | 4 | 7)) || argument(b)))
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_used_texture(op: u32, a: u32, b: u32) -> u8 {
    u8::from(matches!(op, 2 | 4 | 7) && a & 15 == 2 || matches!(op, 3 | 4 | 7) && b & 15 == 2)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_blend(v: u32, alpha: u8) -> u32 {
    match v {
        1 => 1,
        2 => 2,
        3 => {
            if alpha != 0 {
                5
            } else {
                3
            }
        }
        4 => {
            if alpha != 0 {
                6
            } else {
                4
            }
        }
        5 => 5,
        6 => 6,
        7 => 7,
        8 => 8,
        9 => {
            if alpha != 0 {
                7
            } else {
                9
            }
        }
        10 => {
            if alpha != 0 {
                8
            } else {
                10
            }
        }
        11 => {
            if alpha != 0 {
                2
            } else {
                11
            }
        }
        _ => 0,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_state(p: *const u32, textures: u32, has: u8) -> u8 {
    if p.is_null() {
        return 0;
    }
    // SAFETY: native adapter copies 24 integer/bool fields before this call.
    let s = unsafe { core::slice::from_raw_parts(p, 24) };
    let valid = stage(s[0], s[1], s[2])
        && !(s[3] == 1 && s[0] != 1)
        && stage(s[3], s[4], s[5])
        && s[11] <= 255
        && (1..=8).contains(&s[12])
        && (1..=8).contains(&s[17])
        && (1..=3).contains(&s[18])
        && (1..=5).contains(&s[15])
        && s[16] <= 15
        && (1..=2).contains(&s[19])
        && (1..=2).contains(&s[20])
        && s[21] <= 2
        && (1..=5).contains(&s[22])
        && (1..=5).contains(&s[23])
        && a2fo_rs_ui_blend(s[13], 0) != 0
        && a2fo_rs_ui_blend(s[14], 0) != 0;
    let textured = a2fo_rs_ui_used_texture(s[0], s[1], s[2]) != 0
        || a2fo_rs_ui_used_texture(s[3], s[4], s[5]) != 0;
    u8::from(valid && (!textured || has != 0 && textures == 1))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_positions(p: *const u8, count: u32, stride: u32) -> u8 {
    for i in 0..count {
        for j in 0..4 {
            let v = unsafe {
                p.add(i as usize * stride as usize + j * 4)
                    .cast::<f32>()
                    .read_unaligned()
            };
            if !v.is_finite() || j == 3 && v <= 0. {
                return 0;
            }
        }
    }
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_upload(w: u32, h: u32, format: u32, pitch: u32, bytes: usize) -> u8 {
    if w == 0 || h == 0 || w > 16384 || h > 16384 {
        return 0;
    }
    let (rows, row) = match format {
        28 => (h as usize, w as usize * 4),
        71 => (h.div_ceil(4) as usize, w.div_ceil(4) as usize * 8),
        74 | 77 => (h.div_ceil(4) as usize, w.div_ceil(4) as usize * 16),
        _ => return 0,
    };
    u8::from(
        pitch as usize >= row
            && rows <= usize::MAX / pitch as usize
            && bytes >= rows * pitch as usize,
    )
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_indices(
    p: *const u8,
    bytes: usize,
    format: u32,
    count: u32,
    vertices: u32,
) -> u8 {
    if p.is_null() || !matches!(format, 101 | 102) {
        return 0;
    }
    let size = if format == 101 { 2 } else { 4 };
    if count > 64 * 1024 * 1024 / size || bytes < (count * size) as usize {
        return 0;
    }
    for i in 0..count {
        let p = unsafe { p.add((i * size) as usize) };
        let v = if size == 2 {
            (unsafe { p.cast::<u16>().read_unaligned() }) as u32
        } else {
            unsafe { p.cast::<u32>().read_unaligned() }
        };
        if v >= vertices {
            return 0;
        }
    }
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_capacity(required: u32) -> u32 {
    let mut n = 256;
    while n < required && n < 32 * 1024 * 1024 {
        n *= 2;
    }
    n.max(required)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_filter(min: u32, mag: u32, mip: u32) -> u32 {
    let f = if min == 1 {
        if mag == 1 { 0 } else { 4 }
    } else if mag == 1 {
        16
    } else {
        20
    };
    f | u32::from(mip == 2)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_ui_topology(p: u32) -> u32 {
    match p {
        1 => 1,
        2 => 2,
        3 => 3,
        5 => 5,
        _ => 4,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_viewport(
    p: *const u32,
    min: f32,
    max: f32,
    w: u32,
    h: u32,
) -> u8 {
    let v = unsafe { core::slice::from_raw_parts(p, 4) };
    u8::from(
        v[2] != 0
            && v[3] != 0
            && v[0] < w
            && v[1] < h
            && v[2] <= w - v[0]
            && v[3] <= h - v[1]
            && min == 0.
            && max == 1.,
    )
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_ui_clip(
    bounds: *const i32,
    rect: *const i32,
    out: *mut f32,
) -> u8 {
    let b = unsafe { core::slice::from_raw_parts(bounds, 4) };
    let r = unsafe { core::slice::from_raw_parts(rect, 4) };
    let v = [
        r[0].max(b[0]),
        r[1].max(b[1]),
        r[2].min(b[2]),
        r[3].min(b[3]),
    ];
    if v[0] >= v[2] || v[1] >= v[3] {
        return 0;
    }
    // Coordinate values are rounded to float before subtracting half a pixel.
    for (i, &value) in v.iter().enumerate() {
        unsafe { out.add(i).write(value as f32 - 0.5) }
    }
    1
}
