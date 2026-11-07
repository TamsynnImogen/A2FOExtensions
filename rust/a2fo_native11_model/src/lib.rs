//! Shared pristine static SOD policy. No filesystem, graphics, allocator or panic runtime.
#![no_std]
extern crate alloc;
#[cfg(test)]
extern crate std;
mod pose_metadata;
mod group_material;
pub use pose_metadata::{PoseChannel, PoseMetadata, PoseNode, PoseSink, inspect_sod_pose};
pub use group_material::{GroupMaterial, GroupMaterialSink, inspect_sod_group_material};
use a2fmt::sod::{NodeKind, Sod};
use alloc::collections::BTreeMap;
use alloc::vec::Vec;
use core::ffi::c_void;
pub const MAX_INPUT: usize = 64 * 1024 * 1024;

const MAX_NODES: usize = 4096;
const MAX_CORNERS: usize = 1_048_576;
const MAX_VERTICES: usize = 1_048_576;
const MAX_COORDINATE: f32 = 1_000_000.0;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Vertex {
    pub position: [f32; 3],
    pub normal: [f32; 3],
    pub uv: [f32; 2],
}

#[repr(C)]
pub struct Part {
    pub vertices: *const Vertex,
    pub vertex_count: u32,
    pub indices: *const u32,
    pub index_count: u32,
    pub local_to_model: [f32; 16],
    pub texture_name: *const u8,
    pub texture_name_length: u32,
    pub colour: [f32; 4],
    pub emissive: [f32; 3],
    pub flags: u32,
}

pub type PartSink = unsafe extern "C" fn(*mut c_void, *const Part) -> i32;
// The callback borrows owned parser positions only for this synchronous call.
// `count` is the number of tightly packed XYZ triplets, never a float count.
pub type TransformPositions = unsafe extern "C" fn(*mut c_void, *mut f32, u32) -> i32;

#[repr(C)]
pub struct PartSource {
    pub node_name: *const u8,
    pub node_name_length: u32,
    pub node_index: u32,
    pub group_index: u32,
}

pub type DetailedPartSink =
    unsafe extern "C" fn(*mut c_void, *const Part, *const PartSource) -> i32;

fn matrix_row_major(m: [f32; 12]) -> [f32; 16] {
    // a2fmt stores basis axes consecutively and translation last; its points
    // multiply as row vectors. Transpose into column-vector row-major form.
    [
        m[0], m[3], m[6], m[9], m[1], m[4], m[7], m[10], m[2], m[5], m[8], m[11], 0.0, 0.0, 0.0,
        1.0,
    ]
}

// a2fmt's bounds-checked Reader is private; this cursor only skips the fixed
// prefix tables before calling its authoritative parser.
struct PrefixReader<'a> {
    data: &'a [u8],
    pos: usize,
}
impl<'a> PrefixReader<'a> {
    fn new(data: &'a [u8], pos: usize) -> Self {
        Self { data, pos }
    }
    fn take(&mut self, length: usize) -> Result<&'a [u8], ()> {
        let end = self.pos.checked_add(length).ok_or(())?;
        let bytes = self.data.get(self.pos..end).ok_or(())?;
        self.pos = end;
        Ok(bytes)
    }
    fn u16(&mut self) -> Result<u16, ()> {
        let bytes = self.take(2)?;
        Ok(u16::from_le_bytes([bytes[0], bytes[1]]))
    }
    fn f32(&mut self) -> Result<f32, ()> {
        let bytes = self.take(4)?;
        Ok(f32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]))
    }
}

fn skip_string(r: &mut PrefixReader<'_>) -> Result<(), ()> {
    let length = usize::from(r.u16()?);
    r.take(length)?;
    Ok(())
}

// Allocation-free prefix scan following a2fmt::sod::parse's table gates.
// It stops before node payloads; the full dependency parser remains authoritative.
fn preflight_sod_tables(data: &[u8]) -> bool {
    if data.len() < 14 || !matches!(&data[..10], b"Storm3D_SW" | b"StarTrekDB") {
        return false;
    }
    let mut r = PrefixReader::new(data, 10);
    let Ok(version) = r.f32() else {
        return false;
    };
    if !(version >= 1.4 && version <= 1.93) {
        return false;
    }
    if version < 1.7 {
        let Ok(count) = r.u16() else {
            return false;
        };
        if usize::from(count) > MAX_NODES {
            return false;
        }
        for _ in 0..count {
            if skip_string(&mut r).is_err() || skip_string(&mut r).is_err() || r.take(7).is_err() {
                return false;
            }
        }
    }
    let Ok(count) = r.u16() else {
        return false;
    };
    if usize::from(count) > MAX_NODES {
        return false;
    }
    for _ in 0..count {
        if skip_string(&mut r).is_err()
            || r.take(40).is_err()
            || r.take(1).is_err()
            || (version >= 1.9 && r.take(1).is_err())
        {
            return false;
        }
    }
    r.u16().is_ok_and(|count| usize::from(count) <= MAX_NODES)
}

fn valid_model(sod: &Sod) -> bool {
    if sod.nodes.len() > MAX_NODES {
        return false;
    }
    let mut total_corners = 0usize;
    for node in &sod.nodes {
        if !node.transform.is_finite()
            || node.transform.m.iter().any(|v| v.abs() > MAX_COORDINATE)
            || (!node.parent_name.is_empty() && node.parent.is_none())
        {
            return false;
        }
        let NodeKind::Mesh(mesh) = &node.kind else {
            continue;
        };
        if mesh.cull > 1
            || mesh
                .positions
                .iter()
                .flatten()
                .any(|v| !v.is_finite() || v.abs() > MAX_COORDINATE)
            || mesh.uvs.iter().flatten().any(|v| !v.is_finite())
        {
            return false;
        }
        if !mesh
            .groups
            .iter()
            .flat_map(|g| &g.faces)
            .flat_map(|f| f.iter())
            .all(|c| {
                usize::from(c.position) < mesh.positions.len() && usize::from(c.uv) < mesh.uvs.len()
            })
        {
            return false;
        }
        if mesh
            .groups
            .iter()
            .any(|g| sod.lighting_material(&g.material).is_none())
        {
            return false;
        }
        for group in &mesh.groups {
            let Some(corners) = group.faces.len().checked_mul(3) else {
                return false;
            };
            let Some(next) = total_corners.checked_add(corners) else {
                return false;
            };
            total_corners = next;
            if total_corners > MAX_CORNERS {
                return false;
            }
        }
        if sod.version < 1.7 && sod.texture_for_ident(&mesh.texture_material).is_none() {
            return false;
        }
        // Pre-1.7 material modes other than 2/2 have no proven mapping into
        // native11 DrawFlags, so reject them instead of inventing a blend mode.
        if sod.version < 1.7
            && !sod.texture_materials.iter().any(|t| {
                t.ident.eq_ignore_ascii_case(&mesh.texture_material) && t.modes[..2] == [2, 2]
            })
        {
            return false;
        }
        let mode = mesh.texture_material.to_ascii_lowercase();
        if sod.version >= 1.7
            && !matches!(
                mode.as_str(),
                "opaque" | "default" | "additive" | "wormhole" | "alphathreshold" | "alpha"
            )
        {
            return false;
        }
    }
    // The parser checks cycles too. Repeat this bound before any traversal
    // helper, so future parser changes cannot expose a cyclic hierarchy here.
    for i in 0..sod.nodes.len() {
        let mut at = sod.nodes[i].parent;
        let mut steps = 0;
        while let Some(parent) = at {
            if parent >= sod.nodes.len() || parent == i || steps >= sod.nodes.len() {
                return false;
            }
            at = sod.nodes[parent].parent;
            steps += 1;
        }
    }
    sod.lighting_materials.iter().all(|mat| {
        mat.diffuse
            .iter()
            .chain(mat.ambient.iter())
            .chain(mat.specular.iter())
            .all(|v| v.is_finite() && *v >= 0.0)
            && mat.power.is_finite()
    })
}

pub fn parse_sod(data: &[u8], sink: PartSink, context: *mut c_void) -> bool {
    parse_sod_impl(
        data,
        false,
        None,
        |part, _source| unsafe { sink(context, part) } != 0,
    )
}

pub fn parse_sod_detailed(data: &[u8], sink: DetailedPartSink, context: *mut c_void) -> bool {
    parse_sod_impl(
        data,
        false,
        None,
        |part, source| unsafe { sink(context, part, source) } != 0,
    )
}

// Authored full mesh set for fresh-pose selection by an owned consumer. Unlike
// the static preview, this keeps damage subtrees and every LOD level.
pub fn parse_sod_all(data: &[u8], sink: PartSink, context: *mut c_void) -> bool {
    parse_sod_impl(
        data,
        true,
        None,
        |part, _source| unsafe { sink(context, part) } != 0,
    )
}

// The transform runs once per mesh carrying positions and groups, before any
// normal reconstruction or part emission. It may edit only its borrowed XYZ slice.
pub fn parse_sod_all_with_positions(
    data: &[u8],
    transform: TransformPositions,
    transform_context: *mut c_void,
    sink: PartSink,
    context: *mut c_void,
) -> bool {
    parse_sod_impl(
        data,
        true,
        Some((transform, transform_context)),
        |part, _source| unsafe { sink(context, part) } != 0,
    )
}

pub fn parse_sod_detailed_all(data: &[u8], sink: DetailedPartSink, context: *mut c_void) -> bool {
    parse_sod_impl(
        data,
        true,
        None,
        |part, source| unsafe { sink(context, part, source) } != 0,
    )
}

fn parse_sod_impl(
    data: &[u8],
    all_meshes: bool,
    transform: Option<(TransformPositions, *mut c_void)>,
    mut emit: impl FnMut(&Part, &PartSource) -> bool,
) -> bool {
    if !preflight_sod_tables(data) {
        return false;
    }
    let Ok(mut sod) = Sod::parse(data) else {
        return false;
    };
    if !valid_model(&sod) {
        return false;
    }
    if let Some((callback, callback_context)) = transform {
        for node in &mut sod.nodes {
            let NodeKind::Mesh(mesh) = &mut node.kind else {
                continue;
            };
            // a2fmt keeps zero-position/zero-group mesh nodes. They emit no
            // geometry, and the native position policy rejects a zero count.
            if mesh.groups.is_empty() || mesh.positions.is_empty() {
                continue;
            }
            let Ok(count) = u32::try_from(mesh.positions.len()) else {
                return false;
            };
            if unsafe { callback(callback_context, mesh.positions.as_mut_ptr().cast(), count) } == 0
                || mesh
                    .positions
                    .iter()
                    .flatten()
                    .any(|value| !value.is_finite() || value.abs() > MAX_COORDINATE)
            {
                return false;
            }
        }
        if !valid_model(&sod) {
            return false;
        }
    }
    // Build and validate every emitted part before invoking the first callback.
    // A malformed later mesh cannot leave the caller with a partial model.
    let mut prepared = Vec::new();
    let mut selected_vertices = 0usize;
    for (node_index, node) in sod.nodes.iter().enumerate() {
        let NodeKind::Mesh(mesh) = &node.kind else {
            continue;
        };
        // Damage overlays are state-selected by the game. This API's explicit
        // pristine base pose must not turn assimilation boxes on by default.
        if !all_meshes {
            let mut ancestor = Some(node_index);
            let mut damaged = false;
            while let Some(index) = ancestor {
                let node = &sod.nodes[index];
                if matches!(node.kind, NodeKind::Null) && node.name.eq_ignore_ascii_case("damage") {
                    damaged = true;
                    break;
                }
                ancestor = node.parent;
            }
            if damaged {
                continue;
            }
            if let Some((geometry, level)) = sod.lod_group(node_index) {
                if sod.lod_levels(geometry).last().copied() != Some(level) {
                    continue;
                }
            }
        }
        if mesh.groups.is_empty() {
            continue;
        }
        let matrix = matrix_row_major(sod.model_transform(node_index).m);
        if matrix
            .iter()
            .any(|v| !v.is_finite() || v.abs() > MAX_COORDINATE)
        {
            return false;
        }
        let selected_corners: usize = mesh.groups.iter().map(|g| g.faces.len() * 3).sum();
        if selected_corners > MAX_CORNERS
            || selected_vertices
                .checked_add(selected_corners)
                .is_none_or(|v| v > MAX_VERTICES)
        {
            return false;
        }
        let normals = mesh.smooth_normals();
        if normals.iter().flatten().any(|v| !v.is_finite()) {
            return false;
        }
        // First-seen (position, UV) expansion preserves source face/group order
        // without Mesh::unindexed's quadratic sorted-Vec insertion.
        let mut lookup: BTreeMap<(u16, u16), u32> = BTreeMap::new();
        let mut vertices = Vec::new();
        let mut groups = Vec::with_capacity(mesh.groups.len());
        for group in &mesh.groups {
            let mut indices = Vec::with_capacity(group.faces.len() * 3);
            for face in &group.faces {
                for corner in face {
                    let key = (corner.position, corner.uv);
                    let index = if let Some(&index) = lookup.get(&key) {
                        index
                    } else {
                        let Ok(index) = u32::try_from(vertices.len()) else {
                            return false;
                        };
                        vertices.push(Vertex {
                            position: mesh.positions[usize::from(corner.position)],
                            // SOD fronts are CCW on screen; the native scene uses CW.
                            // Reconstruct outward normals alongside the index conversion.
                            normal: normals[usize::from(corner.position)].map(|v| -v),
                            uv: mesh.uvs[usize::from(corner.uv)],
                        });
                        lookup.insert(key, index);
                        index
                    };
                    indices.push(index);
                }
            }
            // Normalize SOD winding at the asset boundary, preserving vertex/UV
            // pairing and group order. Mirrored instances are handled by the renderer.
            for triangle in indices.chunks_exact_mut(3) {
                triangle.swap(1, 2);
            }
            groups.push(indices);
        }
        let Some(next) = selected_vertices.checked_add(vertices.len()) else {
            return false;
        };
        selected_vertices = next;
        if selected_vertices > MAX_VERTICES {
            return false;
        }
        let Ok(vertex_count) = u32::try_from(vertices.len()) else {
            return false;
        };
        let texture = if sod.version < 1.7 {
            sod.texture_for_ident(&mesh.texture_material).unwrap_or("")
        } else {
            mesh.diffuse_texture().unwrap_or("")
        };
        let Ok(texture_name_length) = u32::try_from(texture.len()) else {
            return false;
        };
        if groups
            .iter()
            .any(|indices| u32::try_from(indices.len()).is_err())
        {
            return false;
        }
        prepared.push((
            node_index,
            vertices,
            groups,
            matrix,
            vertex_count,
            texture_name_length,
        ));
    }
    for (node_index, vertices, groups, matrix, vertex_count, texture_name_length) in &prepared {
        let NodeKind::Mesh(mesh) = &sod.nodes[*node_index].kind else {
            unreachable!()
        };
        let texture = if sod.version < 1.7 {
            sod.texture_for_ident(&mesh.texture_material).unwrap_or("")
        } else {
            mesh.diffuse_texture().unwrap_or("")
        };
        let mode = mesh.texture_material.to_ascii_lowercase();
        for (group_index, (group, indices)) in mesh.groups.iter().zip(groups).enumerate() {
            if indices.is_empty() {
                continue;
            }
            let Some(material) = sod.lighting_material(&group.material) else {
                return false;
            };
            let index_count = indices.len() as u32;
            let mut flags = if mesh.cull == 0 { 1 } else { 0 };
            flags |= match mode.as_str() {
                "alphathreshold" => 2,
                "alpha" => 4,
                "additive" | "wormhole" => 8,
                _ => 0,
            };
            let part = Part {
                vertices: vertices.as_ptr(),
                vertex_count: *vertex_count,
                indices: indices.as_ptr(),
                index_count,
                local_to_model: *matrix,
                texture_name: texture.as_ptr(),
                texture_name_length: *texture_name_length,
                // Native self-illumination is textured and independent of
                // source lighting, rather than diffuse plus a solid RGB wash.
                colour: if material.self_illuminating {
                    [0.0, 0.0, 0.0, 1.0]
                } else {
                    [
                        material.diffuse[0],
                        material.diffuse[1],
                        material.diffuse[2],
                        1.0,
                    ]
                },
                emissive: if material.self_illuminating {
                    material.diffuse
                } else {
                    [0.0; 3]
                },
                flags,
            };
            let Ok(node_name_length) = u32::try_from(sod.nodes[*node_index].name.len()) else {
                return false;
            };
            let source = PartSource {
                node_name: sod.nodes[*node_index].name.as_ptr(),
                node_name_length,
                node_index: *node_index as u32,
                group_index: group_index as u32,
            };
            if !emit(&part, &source) {
                return false;
            }
        }
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::vec;

    fn u16le(d: &mut Vec<u8>, v: u16) {
        d.extend_from_slice(&v.to_le_bytes());
    }
    fn u32le(d: &mut Vec<u8>, v: u32) {
        d.extend_from_slice(&v.to_le_bytes());
    }
    fn f32le(d: &mut Vec<u8>, v: f32) {
        d.extend_from_slice(&v.to_le_bytes());
    }
    fn string(d: &mut Vec<u8>, s: &str) {
        u16le(d, s.len() as u16);
        d.extend_from_slice(s.as_bytes());
    }

    // Version 1.93 fixture with one mesh, an empty middle group and two drawn groups.
    fn grouped_sod() -> Vec<u8> {
        let mut d = b"Storm3D_SW".to_vec();
        f32le(&mut d, 1.93);
        u16le(&mut d, 1);
        string(&mut d, "mat");
        for v in [0.1, 0.2, 0.3, 0.25, 0.5, 0.75, 0., 0., 0., 1.] {
            f32le(&mut d, v);
        }
        d.extend_from_slice(&[2, 1]);
        u16le(&mut d, 2);
        u16le(&mut d, 0);
        string(&mut d, "root");
        string(&mut d, "");
        for v in [1., 0., 0., 0., 1., 0., 0., 0., 1., 3., 4., 5.] {
            f32le(&mut d, v);
        }
        u16le(&mut d, 1);
        string(&mut d, "mesh");
        string(&mut d, "ROOT");
        for v in [1., 0., 0., 0., 1., 0., 0., 0., 1., 1., 2., 0.] {
            f32le(&mut d, v);
        }
        string(&mut d, "additive");
        u32le(&mut d, 4);
        u32le(&mut d, 1);
        string(&mut d, "hull.tga");
        u32le(&mut d, 0);
        string(&mut d, "");
        u16le(&mut d, 0);
        u16le(&mut d, 3);
        u16le(&mut d, 3);
        u16le(&mut d, 3);
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
        for (faces, order) in [(1, [0, 1, 2]), (0, [0, 1, 2]), (1, [2, 1, 0])] {
            u16le(&mut d, faces);
            string(&mut d, "mat");
            for _ in 0..faces {
                for i in order {
                    u16le(&mut d, i);
                    u16le(&mut d, i);
                }
            }
        }
        d.push(0);
        u16le(&mut d, 0);
        u16le(&mut d, 0);
        u16le(&mut d, 0);
        d
    }

    fn selection_sod(bad_late_mesh: bool) -> Vec<u8> {
        selection_sod_with_empty(bad_late_mesh, false)
    }

    fn selection_sod_with_empty(bad_late_mesh: bool, with_empty: bool) -> Vec<u8> {
        let mut d = b"Storm3D_SW".to_vec();
        f32le(&mut d, 1.93);
        u16le(&mut d, 1);
        string(&mut d, "mat");
        for v in [0., 0., 0., 1., 1., 1., 0., 0., 0., 1.] {
            f32le(&mut d, v);
        }
        d.extend_from_slice(&[2, 0]);
        // Both LODs contain one triangle. a2fmt retains file order on ties,
        // making lod1 the static preview's highest-detail selected level.
        let mut nodes = vec![
            ("root", "", false),
            ("geometry", "root", false),
            ("lod0", "geometry", false),
            ("lod1", "geometry", false),
            ("low", "lod0", true),
            ("high", "lod1", true),
            ("damage", "root", false),
            ("overlay", "damage", true),
        ];
        if with_empty {
            nodes.push(("empty", "root", true));
        }
        u16le(&mut d, nodes.len() as u16);
        for (name, parent, mesh) in nodes {
            u16le(&mut d, if mesh { 1 } else { 0 });
            string(&mut d, name);
            string(&mut d, parent);
            for value in [1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.] {
                f32le(&mut d, value);
            }
            if !mesh {
                continue;
            }
            string(&mut d, "opaque");
            u32le(&mut d, 4);
            u32le(&mut d, 1);
            string(&mut d, "hull.tga");
            u32le(&mut d, 0);
            string(&mut d, "");
            u16le(&mut d, 0);
            if name == "empty" {
                u16le(&mut d, 0); // a2fmt's empty Mesh ends at zero vertex count
                continue;
            }
            u16le(&mut d, 3);
            u16le(&mut d, 3);
            u16le(&mut d, 1);
            for value in [
                if bad_late_mesh && name == "overlay" { f32::NAN } else { 0. },
                0., 0., 1., 0., 0., 0., 1., 0.,
            ] {
                f32le(&mut d, value);
            }
            for value in [0., 0., 1., 0., 0., 1.] {
                f32le(&mut d, value);
            }
            u16le(&mut d, 1);
            string(&mut d, "mat");
            for i in 0..3 {
                u16le(&mut d, i);
                u16le(&mut d, i);
            }
            d.push(1);
            u16le(&mut d, 0);
        }
        u16le(&mut d, 0); // channels
        u16le(&mut d, 0); // references
        d
    }

    #[derive(Debug, PartialEq)]
    struct CapturedPart {
        vertices: Vec<([f32; 3], [f32; 3], [f32; 2])>,
        indices: Vec<u32>,
        matrix: [f32; 16],
        texture: Vec<u8>,
        colour: [f32; 4],
        emissive: [f32; 3],
        flags: u32,
    }
    unsafe fn capture(part: *const Part) -> CapturedPart {
        let p = unsafe { &*part };
        CapturedPart {
            vertices: unsafe { core::slice::from_raw_parts(p.vertices, p.vertex_count as usize) }
                .iter()
                .map(|v| (v.position, v.normal, v.uv))
                .collect(),
            indices: unsafe { core::slice::from_raw_parts(p.indices, p.index_count as usize) }
                .to_vec(),
            matrix: p.local_to_model,
            texture: unsafe {
                core::slice::from_raw_parts(p.texture_name, p.texture_name_length as usize)
            }
            .to_vec(),
            colour: p.colour,
            emissive: p.emissive,
            flags: p.flags,
        }
    }
    unsafe extern "C" fn ordinary(ctx: *mut c_void, part: *const Part) -> i32 {
        unsafe { &mut *(ctx as *mut Vec<CapturedPart>) }.push(unsafe { capture(part) });
        1
    }
    struct TransformState {
        calls: usize,
        fail_on: usize,
        invalid_on: usize,
        overflow_on: usize,
        swap_yz: bool,
    }
    unsafe extern "C" fn transform_positions(
        ctx: *mut c_void,
        positions: *mut f32,
        count: u32,
    ) -> i32 {
        let state = unsafe { &mut *(ctx as *mut TransformState) };
        state.calls += 1;
        assert_eq!(count, 3); // fixture meshes contain exactly three XYZ triplets
        if state.calls == state.fail_on {
            return 0;
        }
        let values = unsafe { core::slice::from_raw_parts_mut(positions, count as usize * 3) };
        for triplet in values.chunks_exact_mut(3) {
            if state.swap_yz {
                triplet.swap(1, 2);
            }
            triplet[0] += 2.0;
        }
        if state.calls == state.invalid_on {
            values[0] = f32::NAN;
        }
        if state.calls == state.overflow_on {
            values[0] = MAX_COORDINATE + 1.0;
        }
        1
    }
    fn transform_state() -> TransformState {
        TransformState {
            calls: 0,
            fail_on: 0,
            invalid_on: 0,
            overflow_on: 0,
            swap_yz: false,
        }
    }
    type DetailedCapture = Vec<(CapturedPart, Vec<u8>, u32, u32)>;
    unsafe extern "C" fn detailed(
        ctx: *mut c_void,
        part: *const Part,
        source: *const PartSource,
    ) -> i32 {
        let s = unsafe { &*source };
        let name = unsafe { core::slice::from_raw_parts(s.node_name, s.node_name_length as usize) }
            .to_vec();
        unsafe { &mut *(ctx as *mut DetailedCapture) }.push((
            unsafe { capture(part) },
            name,
            s.node_index,
            s.group_index,
        ));
        1
    }

    #[test]
    fn detailed_parts_match_original_and_report_source_group_ordinals() {
        let data = grouped_sod();
        let mut original: Vec<CapturedPart> = vec![];
        let mut inspected: DetailedCapture = vec![];
        assert!(parse_sod(
            &data,
            ordinary,
            &mut original as *mut _ as *mut c_void
        ));
        assert!(parse_sod_detailed(
            &data,
            detailed,
            &mut inspected as *mut _ as *mut c_void
        ));
        assert_eq!(original.len(), 2);
        assert_eq!(
            inspected
                .iter()
                .map(|(_, name, node, group)| (name.as_slice(), *node, *group))
                .collect::<Vec<_>>(),
            vec![(b"mesh".as_slice(), 1, 0), (b"mesh".as_slice(), 1, 2)]
        );
        assert_eq!(
            original,
            inspected
                .into_iter()
                .map(|(part, _, _, _)| part)
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn full_mesh_entrypoints_preserve_damage_and_both_lods() {
        let data = selection_sod(false);
        let mut pristine: DetailedCapture = vec![];
        let mut all: DetailedCapture = vec![];
        let mut plain: Vec<CapturedPart> = vec![];
        assert!(parse_sod_detailed(
            &data, detailed, &mut pristine as *mut _ as *mut c_void
        ));
        assert!(parse_sod_detailed_all(
            &data, detailed, &mut all as *mut _ as *mut c_void
        ));
        assert!(parse_sod_all(
            &data, ordinary, &mut plain as *mut _ as *mut c_void
        ));
        assert_eq!(pristine.len(), 1);
        assert_eq!((pristine[0].2, pristine[0].1.as_slice()), (5, b"high".as_slice()));
        assert_eq!(
            all.iter().map(|(_, name, index, group)| (name.as_slice(), *index, *group)).collect::<Vec<_>>(),
            vec![(b"low".as_slice(), 4, 0), (b"high".as_slice(), 5, 0),
                 (b"overlay".as_slice(), 7, 0)]
        );
        assert_eq!(plain, all.into_iter().map(|(part, _, _, _)| part).collect::<Vec<_>>());
    }

    #[test]
    fn malformed_later_full_mesh_never_calls_sink() {
        let data = selection_sod(true);
        let mut collected: DetailedCapture = vec![];
        assert!(!parse_sod_detailed_all(
            &data, detailed, &mut collected as *mut _ as *mut c_void
        ));
        assert!(collected.is_empty());
    }

    #[test]
    fn native_geometry_transforms_owned_positions_before_normal_reconstruction() {
        let data = selection_sod(false);
        let original = data.clone();
        let mut authored: Vec<CapturedPart> = vec![];
        assert!(parse_sod_all(
            &data,
            ordinary,
            &mut authored as *mut _ as *mut c_void
        ));
        let mut state = transform_state();
        state.swap_yz = true;
        let mut parts: Vec<CapturedPart> = vec![];
        assert!(parse_sod_all_with_positions(
            &data, transform_positions, &mut state as *mut _ as *mut c_void,
            ordinary, &mut parts as *mut _ as *mut c_void,
        ));
        assert_eq!(state.calls, 3); // low, high, and damage overlay meshes
        assert_eq!(parts.len(), 3);
        assert_eq!(parts[0].vertices[0].0, [2.0, 0.0, 0.0]);
        assert_eq!(parts[0].vertices[1].0, [3.0, 0.0, 0.0]);
        assert_eq!(parts[0].vertices[2].0, [2.0, 0.0, 1.0]);
        assert_eq!(parts[0].vertices[0].1, [0.0, 1.0, 0.0]);
        assert!(parts
            .iter()
            .zip(&authored)
            .all(|(native, source)| native.matrix == source.matrix));
        // Position callback does not alter authored node matrices or translations.
        assert_eq!(data, original); // callback never receives source file bytes
    }

    #[test]
    fn identity_transform_matches_full_legacy_parse_and_runs_once_per_mesh() {
        unsafe extern "C" fn identity(ctx: *mut c_void, _: *mut f32, count: u32) -> i32 {
            assert_eq!(count, 3);
            unsafe { *(ctx as *mut usize) += 1 };
            1
        }
        let data = grouped_sod();
        let mut old: Vec<CapturedPart> = vec![];
        let mut native: Vec<CapturedPart> = vec![];
        let mut calls = 0usize;
        assert!(parse_sod_all(&data, ordinary, &mut old as *mut _ as *mut c_void));
        assert!(parse_sod_all_with_positions(
            &data, identity, &mut calls as *mut _ as *mut c_void,
            ordinary, &mut native as *mut _ as *mut c_void,
        ));
        assert_eq!(calls, 1); // three groups, one source mesh
        assert_eq!(native, old);
    }

    #[test]
    fn empty_mesh_does_not_call_native_position_policy() {
        unsafe extern "C" fn identity_reject_zero(
            ctx: *mut c_void,
            _: *mut f32,
            count: u32,
        ) -> i32 {
            if count == 0 {
                return 0;
            }
            unsafe { *(ctx as *mut usize) += 1 };
            1
        }
        let data = selection_sod_with_empty(false, true);
        let mut legacy: Vec<CapturedPart> = vec![];
        let mut native: Vec<CapturedPart> = vec![];
        let mut calls = 0usize;
        assert!(parse_sod_all(&data, ordinary, &mut legacy as *mut _ as *mut c_void));
        assert!(parse_sod_all_with_positions(
            &data,
            identity_reject_zero,
            &mut calls as *mut _ as *mut c_void,
            ordinary,
            &mut native as *mut _ as *mut c_void,
        ));
        assert_eq!(calls, 3); // three rendered meshes, one empty Mesh skipped
        assert_eq!(native, legacy);
    }

    #[test]
    fn transform_failures_and_invalid_later_geometry_emit_no_parts() {
        let data = selection_sod(false);
        for variant in 0..3 {
            let mut state = transform_state();
            match variant {
                0 => state.fail_on = 2,
                1 => state.invalid_on = 3,
                _ => state.overflow_on = 3,
            }
            let mut parts: Vec<CapturedPart> = vec![];
            assert!(!parse_sod_all_with_positions(
                &data, transform_positions, &mut state as *mut _ as *mut c_void,
                ordinary, &mut parts as *mut _ as *mut c_void,
            ));
            assert_eq!(state.calls, if variant == 0 { 2 } else { 3 });
            assert!(parts.is_empty());
        }
        let mut state = transform_state();
        let mut parts: Vec<CapturedPart> = vec![];
        assert!(!parse_sod_all_with_positions(
            &selection_sod(true), transform_positions, &mut state as *mut _ as *mut c_void,
            ordinary, &mut parts as *mut _ as *mut c_void,
        ));
        assert_eq!(state.calls, 0); // raw validation precedes transformation
        assert!(parts.is_empty());
    }

    fn header(version: f32, texture_count: u16, lighting_count: u16, node_count: u16) -> Vec<u8> {
        let mut data = b"Storm3D_SW".to_vec();
        data.extend_from_slice(&version.to_le_bytes());
        if version < 1.7 {
            data.extend_from_slice(&texture_count.to_le_bytes());
            for _ in 0..texture_count {
                data.extend_from_slice(&0u16.to_le_bytes()); // ident
                data.extend_from_slice(&0u16.to_le_bytes()); // texture
                data.extend_from_slice(&[0; 7]); // parameter and modes
            }
        }
        data.extend_from_slice(&lighting_count.to_le_bytes());
        for _ in 0..lighting_count {
            data.extend_from_slice(&0u16.to_le_bytes()); // material name
            data.extend_from_slice(&[0; 40]); // ambient, diffuse, specular, power
            data.push(0); // model
            if version >= 1.9 {
                data.push(0);
            } // self illuminating
        }
        data.extend_from_slice(&node_count.to_le_bytes());
        data
    }

    #[test]
    fn prefix_preflight_checks_all_version_gates_and_counts() {
        for version in [1.4, 1.6, 1.7, 1.8, 1.9, 1.91, 1.92, 1.93] {
            let data = header(version, 1, 1, 0);
            assert!(preflight_sod_tables(&data), "version {version}");
            assert!(!preflight_sod_tables(&data[..data.len() - 1]));
        }
        assert!(!preflight_sod_tables(&header(1.2, 0, 0, 0)));
        assert!(!preflight_sod_tables(&header(1.93, 0, 0, 4097)));
        assert!(!preflight_sod_tables(&header(1.4, 4097, 0, 0)));
        assert!(!preflight_sod_tables(&header(1.93, 0, 4097, 0)));
    }
}
