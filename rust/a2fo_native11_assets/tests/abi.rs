use a2fo_native11_assets::{Part, a2fo_native11_decode_image, a2fo_native11_parse_sod};
use std::ffi::c_void;

fn u16le(d: &mut Vec<u8>, v: u16) {
    d.extend_from_slice(&v.to_le_bytes());
}
fn u32le(d: &mut Vec<u8>, v: u32) {
    d.extend_from_slice(&v.to_le_bytes());
}
fn f32le(d: &mut Vec<u8>, v: f32) {
    d.extend_from_slice(&v.to_le_bytes());
}
fn strle(d: &mut Vec<u8>, s: &str) {
    u16le(d, s.len() as u16);
    d.extend_from_slice(s.as_bytes());
}
fn matrix(d: &mut Vec<u8>, translation: [f32; 3]) {
    for v in [
        1.,
        0.,
        0.,
        0.,
        1.,
        0.,
        0.,
        0.,
        1.,
        translation[0],
        translation[1],
        translation[2],
    ] {
        f32le(d, v);
    }
}

fn sod() -> Vec<u8> {
    let mut d = b"Storm3D_SW".to_vec();
    f32le(&mut d, 1.93);
    u16le(&mut d, 1);
    strle(&mut d, "mat");
    for v in [0.1, 0.2, 0.3, 0.25, 0.5, 0.75, 0., 0., 0., 1.] {
        f32le(&mut d, v);
    }
    d.push(2);
    d.push(1); // model, self illuminated
    u16le(&mut d, 2);
    u16le(&mut d, 0);
    strle(&mut d, "root");
    strle(&mut d, "");
    matrix(&mut d, [3., 4., 5.]);
    u16le(&mut d, 1);
    strle(&mut d, "mesh");
    strle(&mut d, "ROOT");
    matrix(&mut d, [1., 2., 0.]);
    strle(&mut d, "additive");
    u32le(&mut d, 4);
    u32le(&mut d, 1);
    strle(&mut d, "hull.tga");
    u32le(&mut d, 0);
    strle(&mut d, "");
    u16le(&mut d, 0);
    u16le(&mut d, 3);
    u16le(&mut d, 3);
    u16le(&mut d, 1);
    for p in [[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]] {
        for v in p {
            f32le(&mut d, v);
        }
    }
    for uv in [[0., 0.], [1., 0.], [0., 1.]] {
        for v in uv {
            f32le(&mut d, v);
        }
    }
    u16le(&mut d, 1);
    strle(&mut d, "mat");
    for i in 0..3 {
        u16le(&mut d, i);
        u16le(&mut d, i);
    }
    d.push(0);
    u16le(&mut d, 0); // two sided, trailing
    u16le(&mut d, 0);
    u16le(&mut d, 0); // channels, refs
    d
}

fn lod_sod() -> Vec<u8> {
    let mut d = b"Storm3D_SW".to_vec();
    f32le(&mut d, 1.93);
    u16le(&mut d, 1);
    strle(&mut d, "mat");
    for _ in 0..10 {
        f32le(&mut d, 0.5);
    }
    d.extend_from_slice(&[2, 0]);
    u16le(&mut d, 5);
    for (name, parent) in [("geometry", ""), ("lod_far", "geometry")] {
        u16le(&mut d, 0);
        strle(&mut d, name);
        strle(&mut d, parent);
        matrix(&mut d, [0.; 3]);
    }
    append_lod_mesh(&mut d, "far", "lod_far", 1);
    u16le(&mut d, 0);
    strle(&mut d, "lod_near");
    strle(&mut d, "geometry");
    matrix(&mut d, [0.; 3]);
    append_lod_mesh(&mut d, "near", "lod_near", 2);
    u16le(&mut d, 0);
    u16le(&mut d, 0);
    d
}

fn append_lod_mesh(d: &mut Vec<u8>, name: &str, parent: &str, faces: u16) {
    u16le(d, 1);
    strle(d, name);
    strle(d, parent);
    matrix(d, [0.; 3]);
    strle(d, "opaque");
    u32le(d, 4);
    u32le(d, 1);
    strle(d, name);
    u32le(d, 0);
    strle(d, "");
    u16le(d, 0);
    u16le(d, 3);
    u16le(d, 3);
    u16le(d, 1);
    for p in [[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]] {
        for v in p {
            f32le(d, v);
        }
    }
    for uv in [[0., 0.], [1., 0.], [0., 1.]] {
        for v in uv {
            f32le(d, v);
        }
    }
    u16le(d, faces);
    strle(d, "mat");
    for _ in 0..faces {
        for i in 0..3 {
            u16le(d, i);
            u16le(d, i);
        }
    }
    d.push(1);
    u16le(d, 0);
}

fn group_count_offset(d: &[u8]) -> usize {
    d.windows(6).rposition(|w| w == [3, 0, 3, 0, 1, 0]).unwrap() + 4
}

fn append_group(d: &mut Vec<u8>, faces: u16, reverse: bool) {
    u16le(d, faces);
    strle(d, "mat");
    for _ in 0..faces {
        let order = if reverse { [2, 1, 0] } else { [0, 1, 2] };
        for i in order {
            u16le(d, i);
            u16le(d, i);
        }
    }
}

#[derive(Default)]
struct Seen {
    count: usize,
    reject: bool,
}
unsafe extern "C" fn part_sink(ctx: *mut c_void, part: *const Part) -> i32 {
    let seen = unsafe { &mut *(ctx as *mut Seen) };
    let p = unsafe { &*part };
    seen.count += 1;
    assert_eq!(p.vertex_count, 3);
    assert_eq!(p.index_count, 3);
    assert_eq!(
        unsafe { std::slice::from_raw_parts(p.indices, 3) },
        [0, 2, 1]
    );
    assert_eq!(
        unsafe { std::slice::from_raw_parts(p.texture_name, p.texture_name_length as usize) },
        b"hull.tga"
    );
    assert_eq!(
        p.local_to_model,
        [
            1., 0., 0., 4., 0., 1., 0., 6., 0., 0., 1., 5., 0., 0., 0., 1.
        ]
    );
    assert_eq!(p.colour, [0.0, 0.0, 0.0, 1.]);
    assert_eq!(p.emissive, [0.25, 0.5, 0.75]);
    assert_eq!(p.flags, 9);
    let verts = unsafe { std::slice::from_raw_parts(p.vertices, 3) };
    assert_eq!(verts[0].normal, [0., 0., -1.]);
    if seen.reject { 0 } else { 1 }
}

#[test]
fn emits_static_part_and_honours_rejection() {
    let d = sod();
    let mut seen = Seen::default();
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                d.as_ptr(),
                d.len() as u32,
                Some(part_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        1
    );
    assert_eq!(seen.count, 1);
    seen.reject = true;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                d.as_ptr(),
                d.len() as u32,
                Some(part_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(seen.count, 2);
}

#[test]
fn rejects_truncation_bad_values_and_unsupported_semantics() {
    let d = sod();
    let mut seen = Seen::default();
    for n in 0..d.len() {
        assert_eq!(
            unsafe {
                a2fo_native11_parse_sod(
                    d.as_ptr(),
                    n as u32,
                    Some(part_sink),
                    &mut seen as *mut _ as *mut c_void,
                )
            },
            0
        );
    }
    assert_eq!(seen.count, 0);
    let mut bad = d.clone();
    let pos = bad
        .windows(4)
        .position(|w| w == 1.0f32.to_le_bytes())
        .unwrap();
    bad[pos..pos + 4].copy_from_slice(&f32::NAN.to_le_bytes());
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                bad.as_ptr(),
                bad.len() as u32,
                Some(part_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        0
    );
    let mut missing_parent = d.clone();
    let at = missing_parent
        .windows(4)
        .position(|w| w == b"ROOT")
        .unwrap();
    missing_parent[at..at + 4].copy_from_slice(b"LOST");
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                missing_parent.as_ptr(),
                missing_parent.len() as u32,
                Some(part_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        0
    );
}

unsafe extern "C" fn count_sink(ctx: *mut c_void, _part: *const Part) -> i32 {
    let count = unsafe { &mut *(ctx as *mut usize) };
    *count += 1;
    1
}

unsafe extern "C" fn near_sink(ctx: *mut c_void, part: *const Part) -> i32 {
    let count = unsafe { &mut *(ctx as *mut usize) };
    *count += 1;
    let p = unsafe { &*part };
    assert_eq!(p.index_count, 6);
    assert_eq!(
        unsafe { std::slice::from_raw_parts(p.texture_name, p.texture_name_length as usize) },
        b"near"
    );
    1
}

#[test]
fn selects_highest_detail_lod_by_face_count() {
    let d = lod_sod();
    let mut count = 0usize;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                d.as_ptr(),
                d.len() as u32,
                Some(near_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        1
    );
    assert_eq!(count, 1);
}

#[test]
fn pristine_preview_hides_damage_overlay_without_hiding_base_mesh() {
    fn replace(mut data: Vec<u8>, from: &str, to: &str) -> Vec<u8> {
        let mut old = Vec::new();
        strle(&mut old, from);
        let mut new = Vec::new();
        strle(&mut new, to);
        while let Some(at) = data.windows(old.len()).position(|bytes| bytes == old) {
            data.splice(at..at + old.len(), new.iter().copied());
        }
        data
    }
    // Disable the LOD switch: both independent child meshes now exist in the
    // base scene. Turning one child into the standard damage group hides it.
    let base = replace(lod_sod(), "geometry", "root");
    let mut count = 0usize;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                base.as_ptr(),
                base.len() as u32,
                Some(count_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        1
    );
    assert_eq!(count, 2);
    let pristine = replace(base, "lod_far", "damage");
    count = 0;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                pristine.as_ptr(),
                pristine.len() as u32,
                Some(near_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        1
    );
    assert_eq!(count, 1);
}

unsafe extern "C" fn groups_sink(ctx: *mut c_void, part: *const Part) -> i32 {
    let groups = unsafe { &mut *(ctx as *mut Vec<Vec<u32>>) };
    let p = unsafe { &*part };
    groups.push(unsafe { std::slice::from_raw_parts(p.indices, p.index_count as usize) }.to_vec());
    1
}

unsafe extern "C" fn opaque_sink(ctx: *mut c_void, part: *const Part) -> i32 {
    let count = unsafe { &mut *(ctx as *mut usize) };
    *count += 1;
    assert_eq!(unsafe { &*part }.flags, 1); // two-sided, no inferred cutout
    1
}

#[test]
fn default_material_does_not_infer_cutout() {
    let mut d = sod();
    let at = d.windows(8).position(|w| w == b"additive").unwrap();
    d[at - 2..at].copy_from_slice(&7u16.to_le_bytes());
    d.splice(at..at + 8, b"default".iter().copied());
    let mut count = 0usize;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                d.as_ptr(),
                d.len() as u32,
                Some(opaque_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        1
    );
    assert_eq!(count, 1);
}

#[test]
fn preserves_face_group_order_and_first_seen_vertices() {
    let mut d = sod();
    let group_count = group_count_offset(&d);
    d[group_count] = 2;
    let at = d.len() - 7;
    let mut extra = Vec::new();
    append_group(&mut extra, 1, true);
    d.splice(at..at, extra);
    let mut groups = Vec::<Vec<u32>>::new();
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                d.as_ptr(),
                d.len() as u32,
                Some(groups_sink),
                &mut groups as *mut _ as *mut c_void,
            )
        },
        1
    );
    // Preserve groups and vertex/UV identities while normalizing each triangle.
    assert_eq!(groups, [vec![0, 2, 1], vec![2, 0, 1]]);
}

#[test]
fn rejects_cycles_and_malformed_later_mesh_without_callbacks() {
    let mut cycle = lod_sod();
    let at = cycle.windows(8).position(|w| w == b"geometry").unwrap() + 8;
    cycle[at..at + 2].copy_from_slice(&4u16.to_le_bytes());
    cycle.splice(at + 2..at + 2, b"near".iter().copied());
    let mut count = 0usize;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                cycle.as_ptr(),
                cycle.len() as u32,
                Some(count_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(count, 0);
    let mut later = lod_sod();
    let root = later.windows(8).position(|w| w == b"geometry").unwrap();
    later[root..root + 8].copy_from_slice(b"root____"); // both LOD branches now selected
    let n = later.len();
    later[n - 9..n - 7].copy_from_slice(&9u16.to_le_bytes());
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                later.as_ptr(),
                later.len() as u32,
                Some(count_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(count, 0);
}

#[test]
fn rejects_aggregate_corner_budget_before_callbacks() {
    let mut d = sod();
    let group_count = group_count_offset(&d);
    d[group_count] = 7;
    let at = d.len() - 7;
    let mut extra = Vec::new();
    for _ in 0..6 {
        append_group(&mut extra, u16::MAX, false);
    }
    d.splice(at..at, extra);
    let mut count = 0usize;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                d.as_ptr(),
                d.len() as u32,
                Some(count_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(count, 0);
}

#[test]
fn rejects_oversize_node_table_header_without_callback() {
    let mut d = b"Storm3D_SW".to_vec();
    f32le(&mut d, 1.93);
    u16le(&mut d, 0);
    u16le(&mut d, 4097);
    let mut count = 0usize;
    assert_eq!(
        unsafe {
            a2fo_native11_parse_sod(
                d.as_ptr(),
                d.len() as u32,
                Some(count_sink),
                &mut count as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(count, 0);
}

#[test]
fn opt_in_real_static_preview_inventory() {
    if std::env::var_os("A2FO_NATIVE11_REAL_ASSETS").is_none() {
        return;
    }
    let mut successes = 0;
    for path in [
        "/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/Fleet Ops 4.0/sod/fed_venture.sod",
        "/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/Fleet Ops 4.0/sod/fed_mediterranean.SOD",
    ] {
        let data = std::fs::read(path).expect("read real fixture");
        let sod = a2fmt::sod::Sod::parse(&data).expect("parse real fixture");
        let selected_meshes: Vec<_> = sod
            .nodes
            .iter()
            .enumerate()
            .filter_map(|(i, n)| {
                let a2fmt::sod::NodeKind::Mesh(m) = &n.kind else {
                    return None;
                };
                let mut ancestor = Some(i);
                while let Some(index) = ancestor {
                    let node = &sod.nodes[index];
                    if matches!(node.kind, a2fmt::sod::NodeKind::Null)
                        && node.name.eq_ignore_ascii_case("damage")
                    {
                        return None;
                    }
                    ancestor = node.parent;
                }
                if let Some((geometry, level)) = sod.lod_group(i) {
                    if sod.lod_levels(geometry).last().copied() != Some(level) {
                        return None;
                    }
                }
                Some(m)
            })
            .collect();
        let omitted_bump = selected_meshes
            .iter()
            .filter(|m| m.height_map().is_some())
            .count();
        let mut parts = 0usize;
        let ok = unsafe {
            a2fo_native11_parse_sod(
                data.as_ptr(),
                data.len() as u32,
                Some(count_sink),
                &mut parts as *mut _ as *mut c_void,
            )
        };
        eprintln!(
            "{}: result={}, parts={}, selected_meshes={}, omitted_bump={}, nodes={}, channels={}, anim_refs={}, sprites={}, emitters={}, lights={}, lod_geometry={}",
            path,
            ok,
            parts,
            selected_meshes.len(),
            omitted_bump,
            sod.nodes.len(),
            sod.channels.len(),
            sod.anim_refs.len(),
            sod.nodes
                .iter()
                .filter(|n| matches!(n.kind, a2fmt::sod::NodeKind::Sprite))
                .count(),
            sod.nodes
                .iter()
                .filter(|n| matches!(n.kind, a2fmt::sod::NodeKind::Emitter { .. }))
                .count(),
            sod.nodes
                .iter()
                .filter(|n| matches!(n.kind, a2fmt::sod::NodeKind::Light(_)))
                .count(),
            sod.nodes
                .iter()
                .filter(|n| matches!(n.role(), a2fmt::sod::Role::Geometry))
                .count()
        );
        if ok == 1 && parts > 0 {
            successes += 1;
        }
    }
    assert_eq!(successes, 2, "one or more static preview fixtures failed");
}

#[derive(Default)]
struct Pixels {
    count: usize,
    reject: bool,
}
unsafe extern "C" fn image_sink(
    ctx: *mut c_void,
    p: *const u8,
    w: u32,
    h: u32,
    pitch: u32,
    len: u64,
) -> i32 {
    let seen = unsafe { &mut *(ctx as *mut Pixels) };
    seen.count += 1;
    assert_eq!((w, h, pitch, len), (2, 2, 8, 16));
    assert_eq!(
        unsafe { std::slice::from_raw_parts(p, 16) },
        &[
            0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255
        ]
    );
    if seen.reject { 0 } else { 1 }
}

#[test]
fn tga_bottom_up_becomes_top_down_bgra_and_is_bounded() {
    let mut d = vec![0u8; 18];
    d[2] = 2;
    d[12] = 2;
    d[14] = 2;
    d[16] = 24;
    // File rows bottom first: blue, white; then red, green.
    d.extend_from_slice(&[255, 0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0]);
    let mut seen = Pixels::default();
    assert_eq!(
        unsafe {
            a2fo_native11_decode_image(
                d.as_ptr(),
                d.len() as u32,
                Some(image_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        1
    );
    seen.reject = true;
    assert_eq!(
        unsafe {
            a2fo_native11_decode_image(
                d.as_ptr(),
                d.len() as u32,
                Some(image_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        0
    );
    assert_eq!(seen.count, 2);
    d[12] = 255;
    d[13] = 255;
    d[14] = 255;
    d[15] = 255;
    assert_eq!(
        unsafe {
            a2fo_native11_decode_image(
                d.as_ptr(),
                d.len() as u32,
                Some(image_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        0
    );
}

#[test]
fn dds_magic_dispatches_to_bgra_decoder() {
    let mut d = vec![0u8; 128];
    d[..4].copy_from_slice(b"DDS ");
    for (offset, value) in [
        (4usize, 124u32),
        (8, 0x1007),
        (12, 2),
        (16, 2),
        (76, 32),
        (80, 0x41),
        (88, 32),
        (92, 0x00ff_0000),
        (96, 0x0000_ff00),
        (100, 0x0000_00ff),
        (104, 0xff00_0000),
    ] {
        d[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
    }
    d.extend_from_slice(&[
        0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255,
    ]);
    let mut seen = Pixels::default();
    assert_eq!(
        unsafe {
            a2fo_native11_decode_image(
                d.as_ptr(),
                d.len() as u32,
                Some(image_sink),
                &mut seen as *mut _ as *mut c_void,
            )
        },
        1
    );
    assert_eq!(seen.count, 1);
}
