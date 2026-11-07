//! Complete authored pose topology; no static preview visibility or LOD policy.
use a2fmt::sod::{ChannelKeys, NodeKind, Sod};
use alloc::vec::Vec;
use core::ffi::c_void;

use crate::{MAX_INPUT, preflight_sod_tables};

const MAX_NODES: usize = 4096;
const MAX_CHANNELS: usize = 256;
const MAX_KEY_FLOATS: usize = 1_048_576;

#[repr(C)]
pub struct PoseNode {
    pub index: u32,
    pub parent: u32,
    pub kind: u32,
    pub group_count: u32,
    pub name: *const u8,
    pub name_length: u32,
    pub local_matrix34: [f32; 12],
}

#[repr(C)]
pub struct PoseChannel {
    pub index: u32,
    pub node_index: u32,
    pub channel_type: u32,
    pub key_count: u32,
    pub period: f32,
    pub keys: *const f32,
    pub key_width: u32,
}

#[repr(C)]
pub struct PoseMetadata {
    pub node_count: u32,
    pub channel_count: u32,
    pub reference_count: u32,
    pub nodes: *const PoseNode,
    pub channels: *const PoseChannel,
}

pub type PoseSink = unsafe extern "C" fn(*mut c_void, *const PoseMetadata) -> i32;

struct Prepared {
    nodes: Vec<PoseNode>,
    channels: Vec<PoseChannel>,
    // Flattened owned payloads keep channel pointers stable through the sink.
    keys: Vec<Vec<f32>>,
    reference_count: u32,
}

fn valid_name(name: &str) -> bool {
    !name.is_empty() && name.len() <= 259 && name.bytes().all(|b| (0x20..=0x7e).contains(&b))
}

fn prepare(sod: &Sod) -> Option<Prepared> {
    if sod.nodes.len() > MAX_NODES || sod.channels.len() > MAX_CHANNELS {
        return None;
    }
    let mut nodes = Vec::with_capacity(sod.nodes.len());
    for (index, node) in sod.nodes.iter().enumerate() {
        if !valid_name(&node.name) || !node.transform.is_finite() {
            return None;
        }
        let parent = match node.parent {
            Some(parent) if parent < sod.nodes.len() && parent != index => parent as u32,
            None if node.parent_name.is_empty() => u32::MAX,
            _ => return None,
        };
        // Resolve the parser's parent index against its original name. A
        // self-parent or missing parent must not silently become a root.
        if parent != u32::MAX
            && !sod.nodes[parent as usize]
                .name
                .eq_ignore_ascii_case(&node.parent_name)
        {
            return None;
        }
        let (kind, group_count) = match &node.kind {
            NodeKind::Null => (0, 0),
            NodeKind::Mesh(mesh) => (1, mesh.groups.len() as u32),
            NodeKind::Light(_) => (2, 0),
            NodeKind::Sprite => (3, 0),
            NodeKind::Emitter { .. } => (12, 0),
        };
        nodes.push(PoseNode {
            index: index as u32,
            parent,
            kind,
            group_count,
            name: node.name.as_ptr(),
            name_length: node.name.len() as u32,
            local_matrix34: node.transform.m,
        });
    }
    // The authoritative parser checks cycles; repeat the bound before handing
    // index chains to a native consumer.
    for index in 0..sod.nodes.len() {
        let mut at = sod.nodes[index].parent;
        let mut steps = 0;
        while let Some(parent) = at {
            if parent >= sod.nodes.len() || parent == index || steps >= sod.nodes.len() {
                return None;
            }
            at = sod.nodes[parent].parent;
            steps += 1;
        }
    }
    let mut keys = Vec::with_capacity(sod.channels.len());
    let mut channels = Vec::with_capacity(sod.channels.len());
    let mut total_floats = 0usize;
    for (index, channel) in sod.channels.iter().enumerate() {
        if !channel.period.is_finite() || channel.period <= 0.0 {
            return None;
        }
        let mut target = None;
        for (node_index, node) in sod.nodes.iter().enumerate() {
            if node.name.eq_ignore_ascii_case(&channel.node) {
                if target.replace(node_index).is_some() {
                    return None; // ambiguous channel target
                }
            }
        }
        let target = target?;
        let (width, count) = match &channel.keys {
            ChannelKeys::Matrix(matrices) => (12usize, matrices.len()),
            ChannelKeys::Scale(scales) => (1usize, scales.len()),
        };
        if count == 0 || count > 65535 {
            return None;
        }
        total_floats = total_floats.checked_add(count.checked_mul(width)?)?;
        if total_floats > MAX_KEY_FLOATS {
            return None;
        }
        let values = match &channel.keys {
            ChannelKeys::Matrix(matrices) => {
                let mut values = Vec::with_capacity(matrices.len().checked_mul(12)?);
                for matrix in matrices {
                    if !matrix.is_finite() {
                        return None;
                    }
                    values.extend_from_slice(&matrix.m);
                }
                values
            }
            ChannelKeys::Scale(scales) => {
                if scales.iter().any(|value| !value.is_finite()) {
                    return None;
                }
                scales.clone()
            }
        };
        keys.push(values);
        channels.push(PoseChannel {
            index: index as u32,
            node_index: target as u32,
            channel_type: u32::from(channel.channel_type),
            key_count: count as u32,
            period: channel.period,
            keys: keys.last()?.as_ptr(),
            key_width: width as u32,
        });
    }
    Some(Prepared {
        nodes,
        channels,
        keys,
        reference_count: sod.anim_refs.len() as u32,
    })
}

pub fn inspect_sod_pose(data: &[u8], sink: PoseSink, context: *mut c_void) -> bool {
    if data.is_empty() || data.len() > MAX_INPUT || !preflight_sod_tables(data) {
        return false;
    }
    let Ok(sod) = Sod::parse(data) else {
        return false;
    };
    let Some(prepared) = prepare(&sod) else {
        return false;
    };
    let metadata = PoseMetadata {
        node_count: prepared.nodes.len() as u32,
        channel_count: prepared.channels.len() as u32,
        reference_count: prepared.reference_count,
        nodes: prepared.nodes.as_ptr(),
        channels: prepared.channels.as_ptr(),
    };
    // Keep parsed names and flattened keys alive through this one callback.
    let _keep_keys_alive = &prepared.keys;
    unsafe { sink(context, &metadata) != 0 }
}

#[cfg(test)]
mod tests {
    use super::*;
    use a2fmt::math::Matrix34;
    use a2fmt::sod::{AnimChannel, AnimRef, Container, Light, Mesh, Node};
    use alloc::string::String;
    use alloc::vec;

    const ID: [f32; 12] = [1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.];
    fn node(name: &str, parent_name: &str, parent: Option<usize>, kind: NodeKind) -> Node {
        Node {
            node_type: 0,
            name: String::from(name),
            parent_name: String::from(parent_name),
            parent,
            children: vec![],
            transform: Matrix34 { m: ID },
            kind,
        }
    }
    fn sod() -> Sod {
        let mesh = Mesh {
            texture_material: String::from("opaque"),
            flags: 0,
            textures: vec![],
            texture2: String::new(),
            field_u16: 0,
            positions: vec![],
            uvs: vec![],
            groups: vec![
                a2fmt::sod::FaceGroup {
                    material: String::new(),
                    faces: vec![],
                },
                a2fmt::sod::FaceGroup {
                    material: String::new(),
                    faces: vec![],
                },
            ],
            cull: 1,
            trailing: vec![],
        };
        Sod {
            container: Container::Storm3D,
            version: 1.93,
            texture_materials: vec![],
            lighting_materials: vec![],
            nodes: vec![
                node("mesh", "ROOT", Some(4), NodeKind::Mesh(mesh)),
                node("damage", "ROOT", Some(4), NodeKind::Null),
                node(
                    "light",
                    "ROOT",
                    Some(4),
                    NodeKind::Light(Light {
                        kind: 2,
                        colour: [1.; 3],
                        falloff_start: 0.,
                        falloff_range: 1.,
                    }),
                ),
                node("sprite", "ROOT", Some(4), NodeKind::Sprite),
                node("ROOT", "", None, NodeKind::Null),
                node(
                    "emitter",
                    "damage",
                    Some(1),
                    NodeKind::Emitter {
                        emitter: String::from("fx"),
                    },
                ),
            ],
            channels: vec![
                AnimChannel {
                    node: String::from("mesh"),
                    period: 2.,
                    channel_type: 1,
                    ignored: 7,
                    keys: ChannelKeys::Matrix(vec![Matrix34 { m: ID }]),
                },
                AnimChannel {
                    node: String::from("sprite"),
                    period: 3.,
                    channel_type: 5,
                    ignored: 0,
                    keys: ChannelKeys::Scale(vec![1., 2.]),
                },
            ],
            anim_refs: vec![AnimRef {
                ref_type: 4,
                node: String::from("mesh"),
                animation: String::from("a"),
                offset: 0.,
            }],
        }
    }

    #[test]
    fn preserves_file_order_child_before_parent_all_kinds_and_payloads() {
        let model = sod();
        let prepared = prepare(&model).unwrap();
        assert_eq!(prepared.nodes.len(), 6);
        assert_eq!(
            prepared.nodes.iter().map(|n| n.kind).collect::<Vec<_>>(),
            vec![1, 0, 2, 3, 0, 12]
        );
        assert_eq!(prepared.nodes[0].parent, 4);
        assert_eq!(prepared.nodes[4].parent, u32::MAX);
        assert_eq!(prepared.nodes[5].parent, 1);
        assert_eq!(prepared.nodes[0].group_count, 2);
        assert_eq!(prepared.nodes[5].local_matrix34, ID);
        assert_eq!(prepared.channels.len(), 2);
        assert_eq!(
            (
                prepared.channels[0].node_index,
                prepared.channels[0].channel_type,
                prepared.channels[0].key_count,
                prepared.channels[0].key_width
            ),
            (0, 1, 1, 12)
        );
        assert_eq!(
            (
                prepared.channels[1].node_index,
                prepared.channels[1].key_count,
                prepared.channels[1].key_width
            ),
            (3, 2, 1)
        );
        assert_eq!(prepared.keys[0], ID);
        assert_eq!(prepared.keys[1], vec![1., 2.]);
        assert_eq!(prepared.reference_count, 1);
    }

    #[test]
    fn rejects_bad_targets_hierarchy_values_and_counts_before_callback() {
        let mut model = sod();
        model.channels[0].node = String::from("missing");
        assert!(prepare(&model).is_none());
        let mut model = sod();
        model.nodes.push(node("MESH", "", None, NodeKind::Null));
        assert!(prepare(&model).is_none());
        let mut model = sod();
        model.nodes[0].parent = None;
        assert!(prepare(&model).is_none());
        let mut model = sod();
        model.nodes[0].transform.m[0] = f32::NAN;
        assert!(prepare(&model).is_none());
        let mut model = sod();
        model.channels[0].period = 0.;
        assert!(prepare(&model).is_none());
        let mut model = sod();
        model.channels[1].keys = ChannelKeys::Scale(vec![f32::INFINITY]);
        assert!(prepare(&model).is_none());
        let mut model = sod();
        model.channels[0].keys = ChannelKeys::Matrix(vec![]);
        assert!(prepare(&model).is_none());
        let mut model = sod();
        let channel = model.channels[0].clone();
        model.channels.resize(MAX_CHANNELS + 1, channel);
        assert!(prepare(&model).is_none());
        let mut model = sod();
        model.channels[0].keys = ChannelKeys::Matrix(vec![Matrix34 { m: ID }; 65535]);
        model.channels[1].keys = ChannelKeys::Matrix(vec![Matrix34 { m: ID }; 65535]);
        assert!(prepare(&model).is_none()); // combined float bound
    }

    fn f32le(d: &mut Vec<u8>, value: f32) {
        d.extend_from_slice(&value.to_le_bytes());
    }
    fn u16le(d: &mut Vec<u8>, value: u16) {
        d.extend_from_slice(&value.to_le_bytes());
    }
    fn string(d: &mut Vec<u8>, value: &str) {
        u16le(d, value.len() as u16);
        d.extend_from_slice(value.as_bytes());
    }
    fn minimal_bytes() -> Vec<u8> {
        let mut d = b"Storm3D_SW".to_vec();
        f32le(&mut d, 1.93);
        u16le(&mut d, 0); // lighting materials
        u16le(&mut d, 2); // nodes, child before parent
        for (name, parent) in [("child", "root"), ("root", "")] {
            u16le(&mut d, 0);
            string(&mut d, name);
            string(&mut d, parent);
            for value in ID {
                f32le(&mut d, value);
            }
        }
        u16le(&mut d, 1); // channels
        string(&mut d, "child");
        u16le(&mut d, 1);
        f32le(&mut d, 1.);
        d.extend_from_slice(&[5, 0]);
        f32le(&mut d, 2.);
        u16le(&mut d, 1); // complete reference count
        d.push(4);
        string(&mut d, "child");
        string(&mut d, "anim");
        f32le(&mut d, 0.);
        d
    }
    unsafe extern "C" fn capture(ctx: *mut c_void, ptr: *const PoseMetadata) -> i32 {
        let count = unsafe { &mut *(ctx as *mut usize) };
        *count += 1;
        let metadata = unsafe { &*ptr };
        assert_eq!(
            (
                metadata.node_count,
                metadata.channel_count,
                metadata.reference_count
            ),
            (2, 1, 1)
        );
        let nodes = unsafe { core::slice::from_raw_parts(metadata.nodes, 2) };
        assert_eq!(nodes[0].parent, 1);
        0 // callback refusal propagates
    }
    unsafe extern "C" fn accept(ctx: *mut c_void, ptr: *const PoseMetadata) -> i32 {
        let count = unsafe { &mut *(ctx as *mut usize) };
        *count += 1;
        let metadata = unsafe { &*ptr };
        let channels = unsafe { core::slice::from_raw_parts(metadata.channels, 1) };
        assert_eq!((channels[0].node_index, channels[0].key_width), (0, 1));
        assert_eq!(unsafe { *channels[0].keys }, 2.);
        1
    }
    #[test]
    fn parses_authoritative_bytes_once_and_propagates_refusal() {
        let mut calls = 0usize;
        assert!(inspect_sod_pose(
            &minimal_bytes(),
            accept,
            &mut calls as *mut _ as *mut c_void
        ));
        assert_eq!(calls, 1);
        assert!(!inspect_sod_pose(
            &minimal_bytes(),
            capture,
            &mut calls as *mut _ as *mut c_void
        ));
        assert_eq!(calls, 2);
        let mut bad = minimal_bytes();
        bad.pop();
        assert!(!inspect_sod_pose(
            &bad,
            capture,
            &mut calls as *mut _ as *mut c_void
        ));
        assert_eq!(calls, 2);
    }
}
