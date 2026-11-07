//! Synchronous borrowed lighting-material query for one authored mesh group.
use a2fmt::sod::{NodeKind, Sod};
use core::ffi::c_void;

use crate::{preflight_sod_tables, valid_model};

#[repr(C)]
pub struct GroupMaterial {
    pub node_index: u32,
    pub group_index: u32,
    pub material_name: *const u8,
    pub material_name_length: u32,
    pub model: u32,
    pub self_illuminating: u32,
    pub diffuse: [f32; 3],
    pub ambient: [f32; 3],
    pub specular: [f32; 3],
    pub power: f32,
}

pub type GroupMaterialSink = unsafe extern "C" fn(*mut c_void, *const GroupMaterial) -> i32;

fn valid_name(name: &str) -> bool {
    !name.is_empty() && name.len() <= 255 && name.bytes().all(|b| (0x20..=0x7e).contains(&b))
}

/// Parses and validates the entire source before borrowing one group's material
/// through the synchronous sink. The sink must copy fields it keeps.
pub fn inspect_sod_group_material(
    data: &[u8],
    node_index: u32,
    group_index: u32,
    sink: GroupMaterialSink,
    context: *mut c_void,
) -> bool {
    if !preflight_sod_tables(data) {
        return false;
    }
    let Ok(sod) = Sod::parse(data) else {
        return false;
    };
    if !valid_model(&sod) {
        return false;
    }
    let Some(node) = sod.nodes.get(node_index as usize) else {
        return false;
    };
    let NodeKind::Mesh(mesh) = &node.kind else {
        return false;
    };
    let Some(group) = mesh.groups.get(group_index as usize) else {
        return false;
    };
    if !valid_name(&group.material) {
        return false;
    }
    let Some(material) = sod.lighting_material(&group.material) else {
        return false;
    };
    if !valid_name(&material.name) {
        return false;
    }
    let output = GroupMaterial {
        node_index,
        group_index,
        material_name: material.name.as_ptr(),
        material_name_length: material.name.len() as u32,
        model: u32::from(material.model),
        self_illuminating: u32::from(material.self_illuminating),
        diffuse: material.diffuse,
        ambient: material.ambient,
        specular: material.specular,
        power: material.power,
    };
    unsafe { sink(context, &output) != 0 }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;

    fn h(d: &mut Vec<u8>, n: u16) {
        d.extend_from_slice(&n.to_le_bytes());
    }
    fn f(d: &mut Vec<u8>, n: f32) {
        d.extend_from_slice(&n.to_le_bytes());
    }
    fn s(d: &mut Vec<u8>, n: &str) {
        h(d, n.len() as u16);
        d.extend_from_slice(n.as_bytes());
    }
    fn fixture(material: &str, group_material: &str) -> Vec<u8> {
        let mut d = b"Storm3D_SW".to_vec();
        f(&mut d, 1.93);
        h(&mut d, 1);
        s(&mut d, material);
        for v in [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 8.0] {
            f(&mut d, v);
        }
        d.extend_from_slice(&[2, 1]);
        h(&mut d, 2);
        h(&mut d, 0);
        s(&mut d, "root");
        s(&mut d, "");
        for v in [1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.] {
            f(&mut d, v);
        }
        h(&mut d, 1);
        s(&mut d, "hull");
        s(&mut d, "ROOT");
        for v in [1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.] {
            f(&mut d, v);
        }
        s(&mut d, "opaque");
        d.extend_from_slice(&4u32.to_le_bytes());
        d.extend_from_slice(&1u32.to_le_bytes());
        s(&mut d, "hull.tga");
        d.extend_from_slice(&0u32.to_le_bytes());
        s(&mut d, "");
        h(&mut d, 0);
        h(&mut d, 3);
        h(&mut d, 3);
        h(&mut d, 1);
        for v in [0., 0., 0., 1., 0., 0., 0., 1., 0.] {
            f(&mut d, v);
        }
        for v in [0., 0., 1., 0., 0., 1.] {
            f(&mut d, v);
        }
        h(&mut d, 1);
        s(&mut d, group_material);
        for i in 0..3 {
            h(&mut d, i);
            h(&mut d, i);
        }
        d.push(1);
        h(&mut d, 0);
        h(&mut d, 0);
        h(&mut d, 0);
        d
    }
    #[derive(Default)]
    struct Capture {
        calls: usize,
        name: Vec<u8>,
        value: Option<(u32, u32, u32, u32, [f32; 3], [f32; 3], [f32; 3], f32)>,
    }
    unsafe extern "C" fn copy(ctx: *mut c_void, raw: *const GroupMaterial) -> i32 {
        let state = unsafe { &mut *ctx.cast::<Capture>() };
        let p = unsafe { &*raw };
        state.calls += 1;
        state.name = unsafe {
            core::slice::from_raw_parts(p.material_name, p.material_name_length as usize)
        }
        .to_vec();
        state.value = Some((
            p.node_index,
            p.group_index,
            p.model,
            p.self_illuminating,
            p.diffuse,
            p.ambient,
            p.specular,
            p.power,
        ));
        1
    }
    unsafe extern "C" fn reject(ctx: *mut c_void, _: *const GroupMaterial) -> i32 {
        unsafe { &mut *ctx.cast::<Capture>() }.calls += 1;
        0
    }
    #[test]
    fn complete_borrowed_material_and_failures() {
        let mut source = fixture("mat", "MAT");
        let mut state = Capture::default();
        let ctx = (&mut state as *mut Capture).cast();
        assert!(inspect_sod_group_material(&source, 1, 0, copy, ctx));
        assert_eq!(state.calls, 1);
        assert_eq!(state.name, b"mat");
        assert_eq!(
            state.value,
            Some((
                1,
                0,
                2,
                1,
                [0.4, 0.5, 0.6],
                [0.1, 0.2, 0.3],
                [0.7, 0.8, 0.9],
                8.0
            ))
        );
        let original = source.clone();
        source.fill(0);
        assert_eq!(state.name, b"mat"); // sink-owned copy survives source mutation
        source = original;
        assert!(!inspect_sod_group_material(&source, 0, 0, copy, ctx));
        assert!(!inspect_sod_group_material(&source, 2, 0, copy, ctx));
        assert!(!inspect_sod_group_material(&source, u32::MAX, 0, copy, ctx));
        assert!(!inspect_sod_group_material(&source, 1, 1, copy, ctx));
        assert!(!inspect_sod_group_material(
            &source[..source.len() - 1],
            1,
            0,
            copy,
            ctx
        ));
        assert!(!inspect_sod_group_material(
            &fixture("mat", "missing"),
            1,
            0,
            copy,
            ctx
        ));
        assert!(!inspect_sod_group_material(
            &fixture("", ""),
            1,
            0,
            copy,
            ctx
        ));
        assert!(!inspect_sod_group_material(
            &fixture("mat\n", "mat\n"),
            1,
            0,
            copy,
            ctx
        ));
        assert!(!inspect_sod_group_material(
            &fixture("mat", "mat\n"),
            1,
            0,
            copy,
            ctx
        ));
        let long_name = "m".repeat(256);
        assert!(!inspect_sod_group_material(
            &fixture(&long_name, &long_name),
            1,
            0,
            copy,
            ctx
        ));
        let mut bad_float = source.clone();
        bad_float[21..25].copy_from_slice(&f32::NAN.to_le_bytes());
        assert!(!inspect_sod_group_material(&bad_float, 1, 0, copy, ctx));
        assert_eq!(state.calls, 1); // complete validation before callback
        assert!(!inspect_sod_group_material(&source, 1, 0, reject, ctx));
        assert_eq!(state.calls, 2);
        let mut bad = source.clone();
        bad[0] = 0;
        assert!(!inspect_sod_group_material(&bad, 1, 0, copy, ctx));
        assert_eq!(state.calls, 2);
    }
}
