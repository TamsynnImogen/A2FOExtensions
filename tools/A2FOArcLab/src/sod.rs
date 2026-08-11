use crate::odf::find_case_insensitive_relative;
use anyhow::{anyhow, Context, Result};
use bevy::prelude::*;
use bevy::render::mesh::{Indices, PrimitiveTopology};
use bevy::render::render_asset::RenderAssetUsages;
use bevy::render::texture::{CompressedImageFormats, ImageSampler, ImageType};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::{Cursor, Read};
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

const SUPPORTED_VERSIONS: &[f32] = &[1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.91, 1.92, 1.93];

#[derive(Component)]
pub struct ArcLabModelEntity;

#[derive(Component, Debug, Clone)]
pub struct SodNodeMarker {
    pub name: String,
}

#[derive(Debug, Clone, Copy)]
pub struct LoadedSod {
    pub root: Entity,
    pub center: Vec3,
    pub radius: f32,
    pub node_count: usize,
    pub hardpoint_count: usize,
}

#[derive(Debug, Clone)]
pub struct SodSummary {
    pub node_names: Vec<String>,
    pub mesh_count: usize,
    pub hardpoint_count: usize,
}

#[derive(Debug)]
struct SodFile {
    nodes: Vec<SodNode>,
}

#[derive(Debug)]
struct SodNode {
    name: String,
    parent: Option<String>,
    matrix: [f32; 12],
    mesh: Option<SodMesh>,
}

#[derive(Debug)]
struct SodMesh {
    material: String,
    texture: String,
    vertices: Vec<[f32; 3]>,
    uvs: Vec<[f32; 2]>,
    groups: Vec<SodGroup>,
}

#[derive(Debug)]
struct SodGroup {
    material: String,
    faces: Vec<SodFace>,
}

#[derive(Debug)]
struct SodFace {
    vertices: [u16; 3],
    uvs: [u16; 3],
}

pub fn inspect_sod(path: &Path) -> Result<SodSummary> {
    let bytes = fs::read(path).with_context(|| format!("Read SOD {}", path.display()))?;
    let sod = SodFile::parse(&bytes)?;
    Ok(SodSummary {
        mesh_count: sod.nodes.iter().filter(|node| node.mesh.is_some()).count(),
        hardpoint_count: sod
            .nodes
            .iter()
            .filter(|node| is_hardpoint_name(&node.name))
            .count(),
        node_names: sod.nodes.into_iter().map(|node| node.name).collect(),
    })
}

pub fn spawn_sod(
    path: &Path,
    texture_roots: &[PathBuf],
    commands: &mut Commands,
    meshes: &mut Assets<Mesh>,
    materials: &mut Assets<StandardMaterial>,
    images: &mut Assets<Image>,
) -> Result<LoadedSod> {
    let bytes = fs::read(path).with_context(|| format!("Read SOD {}", path.display()))?;
    let sod = SodFile::parse(&bytes)?;
    let root = commands
        .spawn((
            SpatialBundle::default(),
            ArcLabModelEntity,
            Name::new(
                path.file_name()
                    .and_then(|name| name.to_str())
                    .unwrap_or("Armada SOD")
                    .to_string(),
            ),
        ))
        .id();

    let world_matrices = calculate_world_matrices(&sod.nodes);
    let (center, radius) = calculate_bounds(&sod.nodes, &world_matrices);
    let hardpoint_count = sod
        .nodes
        .iter()
        .filter(|node| is_hardpoint_name(&node.name))
        .count();

    let mut node_entities = Vec::with_capacity(sod.nodes.len());
    let mut name_to_entity = HashMap::new();
    let mut texture_cache: HashMap<String, Option<Handle<Image>>> = HashMap::new();
    for node in &sod.nodes {
        let entity = commands
            .spawn((
                SpatialBundle {
                    transform: matrix_to_transform(&node.matrix),
                    ..default()
                },
                ArcLabModelEntity,
                SodNodeMarker {
                    name: node.name.clone(),
                },
                Name::new(node.name.clone()),
            ))
            .id();
        node_entities.push(entity);
        name_to_entity
            .entry(node.name.to_ascii_lowercase())
            .or_insert(entity);

        if let Some(mesh) = &node.mesh {
            let texture = if mesh.texture.trim().is_empty() {
                None
            } else if let Some(cached) = texture_cache.get(&mesh.texture.to_ascii_lowercase()) {
                cached.clone()
            } else {
                let loaded = resolve_texture(&mesh.texture, texture_roots)
                    .and_then(|texture_path| load_texture(&texture_path, images).ok());
                texture_cache.insert(mesh.texture.to_ascii_lowercase(), loaded.clone());
                loaded
            };

            for (group_index, group) in mesh.groups.iter().enumerate() {
                if group.faces.is_empty() {
                    continue;
                }
                let mesh_handle = meshes.add(build_mesh(mesh, group)?);
                let material_handle = materials.add(StandardMaterial {
                    base_color: if texture.is_some() {
                        Color::WHITE
                    } else {
                        Color::rgb(0.42, 0.46, 0.52)
                    },
                    base_color_texture: texture.clone(),
                    perceptual_roughness: 0.72,
                    metallic: 0.05,
                    cull_mode: None,
                    ..default()
                });
                commands.entity(entity).with_children(|children| {
                    children.spawn((
                        PbrBundle {
                            mesh: mesh_handle,
                            material: material_handle,
                            ..default()
                        },
                        ArcLabModelEntity,
                        Name::new(if group.material.trim().is_empty() {
                            format!("{} group {}", mesh.material, group_index + 1)
                        } else {
                            group.material.clone()
                        }),
                    ));
                });
            }
        }
    }

    for (index, node) in sod.nodes.iter().enumerate() {
        let entity = node_entities[index];
        let parent = node
            .parent
            .as_ref()
            .and_then(|name| name_to_entity.get(&name.to_ascii_lowercase()).copied())
            .unwrap_or(root);
        commands.entity(parent).add_child(entity);
    }

    Ok(LoadedSod {
        root,
        center,
        radius,
        node_count: sod.nodes.len(),
        hardpoint_count,
    })
}

pub fn is_hardpoint_name(name: &str) -> bool {
    let lower = name.to_ascii_lowercase();
    let Some(suffix) = lower.strip_prefix("hp") else {
        return false;
    };
    !suffix.is_empty() && suffix.chars().all(|ch| ch.is_ascii_digit())
}

impl SodFile {
    fn parse(bytes: &[u8]) -> Result<Self> {
        let mut reader = Cursor::new(bytes);
        let mut identifier = [0u8; 10];
        reader.read_exact(&mut identifier)?;
        let identifier = String::from_utf8_lossy(&identifier)
            .trim_matches(char::from(0))
            .to_string();
        if identifier != "Storm3D_SW" && identifier != "StarTrekDB" {
            return Err(anyhow!("Unsupported SOD header '{identifier}'"));
        }

        let version = (read_f32(&mut reader)? * 100.0).round() / 100.0;
        if !SUPPORTED_VERSIONS.contains(&version) {
            return Err(anyhow!("Unsupported SOD version {version}"));
        }
        if (1.4..=1.6).contains(&version) {
            let filler_count = read_u16(&mut reader)? as usize;
            for _ in 0..filler_count {
                let first = read_u16(&mut reader)? as usize;
                skip(&mut reader, first)?;
                let second = read_u16(&mut reader)? as usize;
                skip(&mut reader, second)?;
                skip(&mut reader, 7)?;
            }
        }

        let material_count = read_u16(&mut reader)? as usize;
        for _ in 0..material_count {
            let _name = read_identifier(&mut reader)?;
            skip(&mut reader, 9 * 4 + 4 + 1)?;
            if version >= 1.9 {
                skip(&mut reader, 1)?;
            }
        }

        let node_count = read_u16(&mut reader)? as usize;
        let mut nodes = Vec::with_capacity(node_count);
        for _ in 0..node_count {
            nodes.push(SodNode::read(&mut reader, version)?);
        }

        let animation_count = read_u16(&mut reader)? as usize;
        for _ in 0..animation_count {
            let _name = read_identifier(&mut reader)?;
            let keyframes = read_u16(&mut reader)? as usize;
            let _block_length = read_u32(&mut reader)?;
            let animation_type = read_u16(&mut reader)?;
            if animation_type == 5 {
                skip(&mut reader, keyframes * 4)?;
            } else {
                skip(&mut reader, keyframes * 12 * 4)?;
            }
        }

        if version > 1.5 {
            let reference_count = read_u16(&mut reader)? as usize;
            for _ in 0..reference_count {
                skip(&mut reader, 1)?;
                let _node = read_identifier(&mut reader)?;
                let _animation = read_identifier(&mut reader)?;
                if version >= 1.8 {
                    skip(&mut reader, 4)?;
                }
            }
        }
        Ok(Self { nodes })
    }
}

impl SodNode {
    fn read<R: Read>(reader: &mut R, version: f32) -> Result<Self> {
        let node_type = read_u16(reader)?;
        let name = read_identifier(reader)?;
        let parent = read_identifier(reader)?;
        let mut matrix = [0.0; 12];
        for value in &mut matrix {
            *value = read_f32(reader)?;
        }
        let mesh = if node_type == 1 {
            Some(SodMesh::read(reader, version)?)
        } else {
            None
        };
        if node_type == 12 {
            let _emitter = read_identifier(reader)?;
        }
        Ok(Self {
            name,
            parent: (!parent.is_empty()).then_some(parent),
            matrix,
            mesh,
        })
    }
}

impl SodMesh {
    fn read<R: Read>(reader: &mut R, version: f32) -> Result<Self> {
        let material = if version >= 1.7 {
            read_identifier(reader)?
        } else {
            "default".to_string()
        };
        let mut texture_count = 1usize;
        if version >= 1.93 {
            let _flags = read_u32(reader)?;
            texture_count = read_u32(reader)? as usize;
        }
        let texture = read_identifier(reader)?;
        if (version - 1.91).abs() < f32::EPSILON {
            skip(reader, 2)?;
        } else if (version - 1.92).abs() < f32::EPSILON {
            let _extra_texture = read_identifier(reader)?;
            skip(reader, 2)?;
        } else if version >= 1.93 {
            skip(reader, 4)?;
            if texture_count == 2 {
                let _bumpmap = read_identifier(reader)?;
                skip(reader, 4)?;
            }
            let _assimilation_texture = read_identifier(reader)?;
            skip(reader, 2)?;
        }

        let vertex_count = read_u16(reader)? as usize;
        let uv_count = read_u16(reader)? as usize;
        let group_count = read_u16(reader)? as usize;
        let mut vertices = Vec::with_capacity(vertex_count);
        for _ in 0..vertex_count {
            vertices.push([read_f32(reader)?, read_f32(reader)?, read_f32(reader)?]);
        }
        let mut uvs = Vec::with_capacity(uv_count);
        for _ in 0..uv_count {
            uvs.push([read_f32(reader)?, read_f32(reader)?]);
        }
        let mut groups = Vec::with_capacity(group_count);
        for _ in 0..group_count {
            groups.push(SodGroup::read(reader)?);
        }
        skip(reader, 1)?;
        let extra_count = read_u16(reader)? as usize;
        skip(reader, extra_count * 2)?;
        Ok(Self {
            material,
            texture,
            vertices,
            uvs,
            groups,
        })
    }
}

impl SodGroup {
    fn read<R: Read>(reader: &mut R) -> Result<Self> {
        let face_count = read_u16(reader)? as usize;
        let material = read_identifier(reader)?;
        let mut faces = Vec::with_capacity(face_count);
        for _ in 0..face_count {
            let mut vertices = [0; 3];
            let mut uvs = [0; 3];
            for index in 0..3 {
                vertices[index] = read_u16(reader)?;
                uvs[index] = read_u16(reader)?;
            }
            faces.push(SodFace { vertices, uvs });
        }
        Ok(Self { material, faces })
    }
}

fn build_mesh(source: &SodMesh, group: &SodGroup) -> Result<Mesh> {
    let mut positions = Vec::<[f32; 3]>::new();
    let mut texture_coordinates = Vec::<[f32; 2]>::new();
    let mut normals = Vec::<Vec3>::new();
    let mut indices = Vec::<u32>::new();
    let mut vertex_map = HashMap::<(u16, u16), u32>::new();

    for face in &group.faces {
        let mut triangle = [0u32; 3];
        for (corner, output_index) in triangle.iter_mut().enumerate() {
            let key = (face.vertices[corner], face.uvs[corner]);
            *output_index = *vertex_map.entry(key).or_insert_with(|| {
                positions.push(
                    source
                        .vertices
                        .get(key.0 as usize)
                        .copied()
                        .unwrap_or([0.0; 3]),
                );
                texture_coordinates
                    .push(source.uvs.get(key.1 as usize).copied().unwrap_or([0.0; 2]));
                normals.push(Vec3::ZERO);
                (positions.len() - 1) as u32
            });
        }
        indices.extend_from_slice(&[triangle[0], triangle[2], triangle[1]]);
        let a = Vec3::from_array(positions[triangle[0] as usize]);
        let b = Vec3::from_array(positions[triangle[1] as usize]);
        let c = Vec3::from_array(positions[triangle[2] as usize]);
        let normal = (c - a).cross(b - a);
        for index in triangle {
            normals[index as usize] += normal;
        }
    }

    let normals = normals
        .into_iter()
        .map(|normal| normal.normalize_or_zero().to_array())
        .collect::<Vec<_>>();
    let mut mesh = Mesh::new(PrimitiveTopology::TriangleList, RenderAssetUsages::all());
    mesh.insert_attribute(Mesh::ATTRIBUTE_POSITION, positions);
    mesh.insert_attribute(Mesh::ATTRIBUTE_NORMAL, normals);
    mesh.insert_attribute(Mesh::ATTRIBUTE_UV_0, texture_coordinates);
    mesh.insert_indices(Indices::U32(indices));
    Ok(mesh)
}

fn resolve_texture(name: &str, roots: &[PathBuf]) -> Option<PathBuf> {
    let normalized = name.replace('\\', "/");
    let path = PathBuf::from(&normalized);
    let mut candidates = vec![path.clone()];
    if path.extension().is_none() {
        for extension in ["tga", "dds", "png", "jpg", "jpeg"] {
            candidates.push(path.with_extension(extension));
        }
    }
    for root in roots {
        for candidate in &candidates {
            if let Some(hit) = find_case_insensitive_relative(root, candidate) {
                if hit.is_file() {
                    return Some(hit);
                }
            }
        }
    }
    let wanted_names = candidates
        .iter()
        .filter_map(|candidate| candidate.file_name())
        .map(|name| name.to_string_lossy().to_ascii_lowercase())
        .collect::<HashSet<_>>();
    for root in roots {
        for entry in WalkDir::new(root)
            .max_depth(3)
            .into_iter()
            .filter_map(|entry| entry.ok())
        {
            if entry.file_type().is_file()
                && wanted_names.contains(&entry.file_name().to_string_lossy().to_ascii_lowercase())
            {
                return Some(entry.into_path());
            }
        }
    }
    None
}

fn load_texture(path: &Path, images: &mut Assets<Image>) -> Result<Handle<Image>> {
    let bytes = fs::read(path)?;
    let extension = path
        .extension()
        .and_then(|extension| extension.to_str())
        .unwrap_or("tga");
    #[cfg(debug_assertions)]
    let image = Image::from_buffer(
        path.display().to_string(),
        &bytes,
        ImageType::Extension(extension),
        CompressedImageFormats::BC,
        true,
        ImageSampler::default(),
        RenderAssetUsages::all(),
    )?;
    #[cfg(not(debug_assertions))]
    let image = Image::from_buffer(
        &bytes,
        ImageType::Extension(extension),
        CompressedImageFormats::BC,
        true,
        ImageSampler::default(),
        RenderAssetUsages::all(),
    )?;
    Ok(images.add(image))
}

fn calculate_world_matrices(nodes: &[SodNode]) -> Vec<Mat4> {
    let mut name_to_index = HashMap::new();
    for (index, node) in nodes.iter().enumerate() {
        name_to_index
            .entry(node.name.to_ascii_lowercase())
            .or_insert(index);
    }
    let mut output = vec![None; nodes.len()];
    let mut visiting = HashSet::new();
    for index in 0..nodes.len() {
        calculate_world_matrix(index, nodes, &name_to_index, &mut output, &mut visiting);
    }
    output
        .into_iter()
        .map(|value| value.unwrap_or(Mat4::IDENTITY))
        .collect()
}

fn calculate_world_matrix(
    index: usize,
    nodes: &[SodNode],
    name_to_index: &HashMap<String, usize>,
    output: &mut [Option<Mat4>],
    visiting: &mut HashSet<usize>,
) -> Mat4 {
    if let Some(value) = output[index] {
        return value;
    }
    let local = matrix_to_mat4(&nodes[index].matrix);
    if !visiting.insert(index) {
        output[index] = Some(local);
        return local;
    }
    let world = nodes[index]
        .parent
        .as_ref()
        .and_then(|name| name_to_index.get(&name.to_ascii_lowercase()).copied())
        .filter(|parent| *parent != index)
        .map(|parent| {
            calculate_world_matrix(parent, nodes, name_to_index, output, visiting) * local
        })
        .unwrap_or(local);
    visiting.remove(&index);
    output[index] = Some(world);
    world
}

fn calculate_bounds(nodes: &[SodNode], world: &[Mat4]) -> (Vec3, f32) {
    let mut minimum = Vec3::splat(f32::INFINITY);
    let mut maximum = Vec3::splat(f32::NEG_INFINITY);
    let mut found = false;
    for (index, node) in nodes.iter().enumerate() {
        if let Some(mesh) = &node.mesh {
            for vertex in &mesh.vertices {
                let point = world[index].transform_point3(Vec3::from_array(*vertex));
                if point.is_finite() {
                    minimum = minimum.min(point);
                    maximum = maximum.max(point);
                    found = true;
                }
            }
        }
    }
    if !found {
        return (Vec3::ZERO, 5.0);
    }
    let center = (minimum + maximum) * 0.5;
    let radius = maximum.distance(minimum).max(1.0) * 0.5;
    (center, radius)
}

fn matrix_to_mat4(matrix: &[f32; 12]) -> Mat4 {
    Mat4::from_cols_array(&[
        matrix[0], matrix[1], matrix[2], 0.0, matrix[3], matrix[4], matrix[5], 0.0, matrix[6],
        matrix[7], matrix[8], 0.0, matrix[9], matrix[10], matrix[11], 1.0,
    ])
}

fn matrix_to_transform(matrix: &[f32; 12]) -> Transform {
    Transform::from_matrix(matrix_to_mat4(matrix))
}

fn read_u16<R: Read>(reader: &mut R) -> Result<u16> {
    let mut bytes = [0; 2];
    reader.read_exact(&mut bytes)?;
    Ok(u16::from_le_bytes(bytes))
}

fn read_u32<R: Read>(reader: &mut R) -> Result<u32> {
    let mut bytes = [0; 4];
    reader.read_exact(&mut bytes)?;
    Ok(u32::from_le_bytes(bytes))
}

fn read_f32<R: Read>(reader: &mut R) -> Result<f32> {
    let mut bytes = [0; 4];
    reader.read_exact(&mut bytes)?;
    Ok(f32::from_le_bytes(bytes))
}

fn read_identifier<R: Read>(reader: &mut R) -> Result<String> {
    let length = read_u16(reader)? as usize;
    let mut bytes = vec![0; length];
    reader.read_exact(&mut bytes)?;
    Ok(String::from_utf8_lossy(&bytes)
        .trim_matches(char::from(0))
        .to_string())
}

fn skip<R: Read>(reader: &mut R, count: usize) -> Result<()> {
    let mut bytes = vec![0; count];
    reader.read_exact(&mut bytes)?;
    Ok(())
}
